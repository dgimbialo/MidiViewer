#include "pch.h"
#include "MidiParser.h"

#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>

// ---------------------------------------------------------------------------
// Big-endian helpers
// ---------------------------------------------------------------------------
uint32_t MidiParser::ReadBE32(const uint8_t* p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
}

uint16_t MidiParser::ReadBE16(const uint8_t* p)
{
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

// Variable-Length Quantity decoder
uint32_t MidiParser::ReadVLQ(const uint8_t* data, size_t size, size_t& pos)
{
    uint32_t value = 0;
    for (int i = 0; i < 4 && pos < size; ++i)
    {
        uint8_t b = data[pos++];
        value = (value << 7) | (b & 0x7F);
        if ((b & 0x80) == 0) break;
    }
    return value;
}

// ---------------------------------------------------------------------------
// Parse a single MTrk chunk
// ---------------------------------------------------------------------------
bool MidiParser::ParseTrack(const uint8_t*              data,
                             size_t                      size,
                             MidiTrack&                  track,
                             std::vector<TempoChange>&   tempos,
                             std::vector<TimeSigChange>& timeSigs,
                             long&                       outMaxTick,
                             bool                        isTempoTrack)
{
    // pending[channel][pitch] = startTick  (-1 = not active)
    std::array<std::array<long, 128>, 16> pending;
    for (auto& ch : pending)
        ch.fill(-1);

    size_t pos       = 0;
    long   absTick   = 0;
    uint8_t runStatus = 0;

    while (pos < size)
    {
        // Delta time
        uint32_t delta = ReadVLQ(data, size, pos);
        absTick += (long)delta;

        if (pos >= size) break;

        uint8_t statusByte = data[pos];

        // Meta event
        if (statusByte == 0xFF)
        {
            ++pos;
            if (pos + 1 >= size) break;
            uint8_t  metaType = data[pos++];
            uint32_t metaLen  = ReadVLQ(data, size, pos);
            if (pos + metaLen > size) break;

            if (metaType == 0x51 && metaLen >= 3)
            {
                // Tempo
                long usPerQN = (long(data[pos]) << 16) |
                               (long(data[pos+1]) << 8) |
                                long(data[pos+2]);
                TempoChange tc{ absTick, usPerQN };
                tempos.push_back(tc);
            }
            else if (metaType == 0x58 && metaLen >= 4)
            {
                // Time signature
                int nn = data[pos];
                int dd = data[pos + 1];
                TimeSigChange ts{ absTick, nn, 1 << dd };
                timeSigs.push_back(ts);
            }
            else if (metaType == 0x03 && metaLen > 0)
            {
                // Track name (ASCII)
                track.name.assign(reinterpret_cast<const char*>(data + pos), metaLen);
            }
            else if (metaType == 0x2F)
            {
                // End of track
                pos += metaLen;
                break;
            }

            pos += metaLen;
            runStatus = 0;
            continue;
        }

        // SysEx — skip
        if (statusByte == 0xF0 || statusByte == 0xF7)
        {
            ++pos;
            uint32_t sysexLen = ReadVLQ(data, size, pos);
            pos += sysexLen;
            runStatus = 0;
            continue;
        }

        // MIDI channel message
        uint8_t eventByte;
        if (statusByte & 0x80)
        {
            eventByte = statusByte;
            runStatus = statusByte;
            ++pos;
        }
        else
        {
            // Running status
            eventByte = runStatus;
        }

        uint8_t msgType = eventByte & 0xF0;
        uint8_t channel = eventByte & 0x0F;

        auto readByte = [&]() -> uint8_t {
            return (pos < size) ? data[pos++] : 0;
        };

        switch (msgType)
        {
        case 0x80: // Note Off
        {
            uint8_t pitch = readByte();
            uint8_t vel   = readByte();
            (void)vel;
            if (pitch < 128 && pending[channel][pitch] >= 0)
            {
                MidiNote note;
                note.pitch     = pitch;
                note.startTick = pending[channel][pitch];
                note.endTick   = absTick;
                note.channel   = channel;
                note.velocity  = 64; // already consumed
                track.notes.push_back(note);
                pending[channel][pitch] = -1;
            }
            break;
        }
        case 0x90: // Note On
        {
            uint8_t pitch = readByte();
            uint8_t vel   = readByte();
            if (pitch < 128)
            {
                if (vel > 0)
                {
                    // Open note (overwrite if re-triggered without NoteOff)
                    pending[channel][pitch] = absTick;
                }
                else
                {
                    // velocity=0 → treat as NoteOff
                    if (pending[channel][pitch] >= 0)
                    {
                        MidiNote note;
                        note.pitch     = pitch;
                        note.startTick = pending[channel][pitch];
                        note.endTick   = absTick;
                        note.channel   = channel;
                        note.velocity  = 64;
                        track.notes.push_back(note);
                        pending[channel][pitch] = -1;
                    }
                }
            }
            break;
        }
        case 0xA0: readByte(); readByte(); break; // Aftertouch
        case 0xB0: readByte(); readByte(); break; // Control Change
        case 0xC0: readByte();             break; // Program Change
        case 0xD0: readByte();             break; // Channel Pressure
        case 0xE0: readByte(); readByte(); break; // Pitch Bend
        default:
            // Unknown — stop parsing this track
            pos = size;
            break;
        }

        if (absTick > outMaxTick)
            outMaxTick = absTick;
    }

    // Close any unclosed notes at end of track
    for (int ch = 0; ch < 16; ++ch)
        for (int p = 0; p < 128; ++p)
            if (pending[ch][p] >= 0)
            {
                MidiNote note;
                note.pitch     = p;
                note.startTick = pending[ch][p];
                note.endTick   = absTick;
                note.channel   = ch;
                note.velocity  = 64;
                track.notes.push_back(note);
            }

    return true;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
bool MidiParser::Parse(const std::wstring& path,
                       MidiDocument&       outDoc,
                       std::wstring&       outError)
{
    // Read entire file into memory
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open())
    {
        outError = L"Cannot open file.";
        return false;
    }
    std::streamsize fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
    if (!f.read(reinterpret_cast<char*>(buf.data()), fileSize))
    {
        outError = L"File read error.";
        return false;
    }

    const uint8_t* data = buf.data();
    size_t size = buf.size();
    size_t pos  = 0;

    // Parse MThd header
    if (size < 14 || std::memcmp(data, "MThd", 4) != 0)
    {
        outError = L"Not a valid MIDI file (missing MThd).";
        return false;
    }
    pos = 4;
    uint32_t headerLen = ReadBE32(data + pos); pos += 4;
    if (headerLen < 6 || pos + headerLen > size)
    {
        outError = L"Corrupt MIDI header.";
        return false;
    }

    outDoc.format    = ReadBE16(data + pos);     pos += 2;
    uint16_t numTrk  = ReadBE16(data + pos);     pos += 2;
    uint16_t timeDef = ReadBE16(data + pos);     pos += 2;
    pos = 8 + headerLen; // skip any extra header bytes

    if (timeDef & 0x8000)
    {
        outError = L"SMPTE time code not supported.";
        return false;
    }
    outDoc.tpqn = timeDef;

    // Default tempo + time sig (in case file has none)
    outDoc.tempos.push_back({ 0L, 500000L });
    outDoc.timeSigs.push_back({ 0L, 4, 4 });

    long maxTick = 0;

    // Parse each MTrk chunk
    for (int t = 0; t < numTrk; ++t)
    {
        if (pos + 8 > size) break;
        if (std::memcmp(data + pos, "MTrk", 4) != 0)
        {
            outError = L"Expected MTrk chunk.";
            return false;
        }
        pos += 4;
        uint32_t trackLen = ReadBE32(data + pos); pos += 4;
        if (pos + trackLen > size)
        {
            outError = L"Corrupt MTrk chunk.";
            return false;
        }

        MidiTrack track;
        track.name = "Track " + std::to_string(t + 1);

        // For Format 1, track 0 is the tempo map track
        bool isTempoTrack = (outDoc.format == 1 && t == 0);

        // Temporary copies to detect if this track adds changes
        size_t tempoBefore   = outDoc.tempos.size();
        size_t timeSigBefore = outDoc.timeSigs.size();

        ParseTrack(data + pos, trackLen, track,
                   outDoc.tempos, outDoc.timeSigs,
                   maxTick, isTempoTrack);

        pos += trackLen;

        // For format 1 tempo track: don't add it as a note track if empty
        if (isTempoTrack && track.notes.empty())
            continue;

        outDoc.tracks.push_back(std::move(track));
    }

    outDoc.totalTicks = maxTick;

    // Sort tempo/timesig by tick (should already be in order, but be safe)
    std::sort(outDoc.tempos.begin(), outDoc.tempos.end(),
              [](const TempoChange& a, const TempoChange& b){ return a.tick < b.tick; });
    std::sort(outDoc.timeSigs.begin(), outDoc.timeSigs.end(),
              [](const TimeSigChange& a, const TimeSigChange& b){ return a.tick < b.tick; });

    // Remove duplicated defaults if file provided its own at tick 0
    if (outDoc.tempos.size() > 1 && outDoc.tempos[0].tick == 0 && outDoc.tempos[1].tick == 0)
        outDoc.tempos.erase(outDoc.tempos.begin());
    if (outDoc.timeSigs.size() > 1 && outDoc.timeSigs[0].tick == 0 && outDoc.timeSigs[1].tick == 0)
        outDoc.timeSigs.erase(outDoc.timeSigs.begin());

    return true;
}

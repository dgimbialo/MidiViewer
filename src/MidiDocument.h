#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>   // MultiByteToWideChar

// ---------------------------------------------------------------------------
// A single sounding MIDI note (NoteOn matched with NoteOff)
// ---------------------------------------------------------------------------
struct MidiNote
{
    int  pitch;       // MIDI pitch 0-127
    long startTick;   // absolute tick of NoteOn
    long endTick;     // absolute tick of NoteOff
    int  channel;     // MIDI channel 0-15
    int  velocity;    // NoteOn velocity 1-127
};

// ---------------------------------------------------------------------------
// Tempo change meta event (0xFF 0x51)
// ---------------------------------------------------------------------------
struct TempoChange
{
    long tick;
    long microsecondsPerQN;  // default 500000 = 120 BPM
};

// ---------------------------------------------------------------------------
// Time-signature meta event (0xFF 0x58)
// ---------------------------------------------------------------------------
struct TimeSigChange
{
    long tick;
    int  numerator;    // nn  (beats per bar)
    int  denominator;  // 2^dd  (e.g. 4 for quarter-note beat)
};

// ---------------------------------------------------------------------------
// One MIDI track
// ---------------------------------------------------------------------------
struct MidiTrack
{
    std::string           name;
    std::vector<MidiNote> notes;
};

// ---------------------------------------------------------------------------
// The full parsed MIDI document
// ---------------------------------------------------------------------------
struct MidiDocument
{
    int  format;     // SMF format: 0, 1, or 2
    long tpqn;       // ticks per quarter note

    std::vector<MidiTrack>      tracks;
    std::vector<TempoChange>    tempos;
    std::vector<TimeSigChange>  timeSigs;

    long totalTicks; // max end tick across all tracks

    // Helpers -----------------------------------------------------------

    // Ticks → pixel X  (pixelsPerBeat = zoom factor)
    static int TickToPixel(long tick, int pixelsPerBeat, long tpqn)
    {
        if (tpqn <= 0) return 0;
        return static_cast<int>((static_cast<long long>(tick) * pixelsPerBeat) / tpqn);
    }

    // Bar length in ticks for a given time signature
    static long BarLengthTicks(int numerator, int denominator, long tpqn)
    {
        // denominator is already the actual value (4, 8, etc.)
        // One quarter note = tpqn ticks
        // One beat (1/denominator) = tpqn * 4 / denominator ticks
        if (denominator <= 0) denominator = 4;
        return (long)numerator * tpqn * 4 / denominator;
    }

    // Return the active time signature at a given tick
    void GetTimeSigAt(long tick, int& outNumerator, int& outDenominator) const
    {
        outNumerator   = 4;
        outDenominator = 4;
        for (const auto& ts : timeSigs)
        {
            if (ts.tick <= tick)
            {
                outNumerator   = ts.numerator;
                outDenominator = ts.denominator;
            }
            else break;
        }
    }

    // ---------------------------------------------------------------------------
    // Tick → milliseconds (accounts for all tempo changes)
    // ---------------------------------------------------------------------------
    long TickToMs(long tick) const
    {
        if (tpqn <= 0) return 0;
        long ms       = 0;
        long prevTick = 0;
        long curTempo = 500000; // default 120 BPM
        for (const auto& tc : tempos)
        {
            if (tc.tick >= tick) break;
            long segTicks = tc.tick - prevTick;
            if (segTicks > 0)
                ms += static_cast<long>((long long)segTicks * curTempo / (tpqn * 1000LL));
            prevTick = tc.tick;
            curTempo = tc.microsecondsPerQN;
        }
        long segTicks = tick - prevTick;
        if (segTicks > 0)
            ms += static_cast<long>((long long)segTicks * curTempo / (tpqn * 1000LL));
        return ms;
    }

    // ---------------------------------------------------------------------------
    // Tick → 0-based bar number (accounts for time signature changes)
    // ---------------------------------------------------------------------------
    int TickToBar(long tick) const
    {
        if (tpqn <= 0) return 0;
        long   curTick = 0;
        int    barNum  = 0;
        size_t tsIdx   = 1;
        int curNum = 4, curDen = 4;
        if (!timeSigs.empty())
        {
            curNum = timeSigs[0].numerator;
            curDen = timeSigs[0].denominator;
        }
        while (true)
        {
            while (tsIdx < timeSigs.size() && timeSigs[tsIdx].tick <= curTick)
            {
                curNum = timeSigs[tsIdx].numerator;
                curDen = timeSigs[tsIdx].denominator;
                ++tsIdx;
            }
            long barLen = BarLengthTicks(curNum, curDen, tpqn);
            if (barLen <= 0) return barNum;
            if (curTick + barLen > tick) return barNum;
            curTick += barLen;
            ++barNum;
        }
    }

    // ---------------------------------------------------------------------------
    // MIDI pitch number (0-127) → note name string, e.g. 60 → "C4", 61 → "C#4"
    // ---------------------------------------------------------------------------
    static std::wstring MidiPitchName(int pitch)
    {
        static const wchar_t* names[] = {
            L"C", L"C#", L"D", L"D#", L"E", L"F",
            L"F#", L"G", L"G#", L"A", L"A#", L"B"
        };
        int octave = pitch / 12 - 1;
        int note   = pitch % 12;
        return std::wstring(names[note]) + std::to_wstring(octave);
    }

    // ---------------------------------------------------------------------------
    // Convert std::string (UTF-8 or Latin-1) to std::wstring for display
    // ---------------------------------------------------------------------------
    static std::wstring ToWide(const std::string& s)
    {
        if (s.empty()) return {};
        // Try UTF-8 first
        int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    s.c_str(), (int)s.size(), nullptr, 0);
        if (n > 0)
        {
            std::wstring ws(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), n);
            return ws;
        }
        // Fallback: the user's current ANSI code page (e.g. 1251 for Cyrillic
        // Windows, 1252 for Western). MIDI text has no standard encoding, so
        // matching the local code page gives the best result for the user.
        n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
        if (n > 0)
        {
            std::wstring ws(n, L'\0');
            MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), ws.data(), n);
            return ws;
        }
        return std::wstring(s.begin(), s.end());
    }
};

// ---------------------------------------------------------------------------
// Identifies a single note by its track and note index in MidiDocument::tracks
// ---------------------------------------------------------------------------
struct NoteRef
{
    int trackIdx = -1;
    int noteIdx  = -1;

    bool IsValid()                   const { return trackIdx >= 0 && noteIdx >= 0; }
    bool operator==(const NoteRef& o) const { return trackIdx == o.trackIdx && noteIdx == o.noteIdx; }
    bool operator!=(const NoteRef& o) const { return !(*this == o); }
};

#pragma once
#include "MidiDocument.h"
#include <string>

// ---------------------------------------------------------------------------
// MidiParser — reads a Standard MIDI File (.mid/.midi) from disk and fills
// a MidiDocument.  No external libraries; hand-written binary reader.
// ---------------------------------------------------------------------------
class MidiParser
{
public:
    // Parse the file at 'path'.  Returns true on success.
    // On failure, outError contains a human-readable message.
    static bool Parse(const std::wstring& path,
                      MidiDocument&       outDoc,
                      std::wstring&       outError);

private:
    // Internal helpers
    static uint32_t ReadBE32(const uint8_t* p);
    static uint16_t ReadBE16(const uint8_t* p);
    static uint32_t ReadVLQ(const uint8_t* data, size_t size,
                            size_t& pos);
    static bool ParseTrack(const uint8_t* data, size_t size,
                           MidiTrack& track,
                           std::vector<TempoChange>&   tempos,
                           std::vector<TimeSigChange>& timeSigs,
                           long& outMaxTick,
                           bool  isTempoTrack);
};

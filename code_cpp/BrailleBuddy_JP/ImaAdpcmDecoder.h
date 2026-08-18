#pragma once

#include <stdint.h>

// Decoder for Microsoft IMA ADPCM stored in a WAV file (format code 0x0011).
// Each mono block starts with a 16-bit predictor, an 8-bit step index and one
// reserved byte. The remaining bytes contain a low nibble followed by a high
// nibble. Every nibble expands to one signed 16-bit PCM sample.
class ImaAdpcmDecoder {
 public:
  bool beginBlock(int16_t predictor, uint8_t stepIndex) {
    if (stepIndex > 88U) {
      return false;
    }
    predictor_ = predictor;
    stepIndex_ = stepIndex;
    return true;
  }

  int16_t predictor() const {
    return predictor_;
  }

  uint8_t stepIndex() const {
    return stepIndex_;
  }

  int16_t decodeNibble(uint8_t nibble) {
    nibble &= 0x0FU;

    const int32_t step = stepTable()[stepIndex_];
    // FFmpeg's IMA WAV codec calculates this in one expression. Keeping the
    // multiplication before the shift avoids the cumulative rounding error
    // produced by shifting each term separately at small step sizes.
    const int32_t magnitude = nibble & 0x07U;
    const int32_t difference = ((2 * magnitude + 1) * step) >> 3;

    int32_t nextPredictor = predictor_;
    if ((nibble & 0x08U) != 0U) {
      nextPredictor -= difference;
    } else {
      nextPredictor += difference;
    }

    if (nextPredictor > 32767) {
      nextPredictor = 32767;
    } else if (nextPredictor < -32768) {
      nextPredictor = -32768;
    }
    predictor_ = static_cast<int16_t>(nextPredictor);

    int16_t nextIndex =
        static_cast<int16_t>(stepIndex_) + indexTable()[nibble];
    if (nextIndex < 0) {
      nextIndex = 0;
    } else if (nextIndex > 88) {
      nextIndex = 88;
    }
    stepIndex_ = static_cast<uint8_t>(nextIndex);
    return predictor_;
  }

 private:
  static const int8_t* indexTable() {
    static const int8_t table[16] = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8,
    };
    return table;
  }

  static const int16_t* stepTable() {
    static const int16_t table[89] = {
        7,     8,     9,     10,    11,    12,    13,    14,    16,
        17,    19,    21,    23,    25,    28,    31,    34,    37,
        41,    45,    50,    55,    60,    66,    73,    80,    88,
        97,    107,   118,   130,   143,   157,   173,   190,   209,
        230,   253,   279,   307,   337,   371,   408,   449,   494,
        544,   598,   658,   724,   796,   876,   963,   1060,  1166,
        1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,
        3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,
        7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
    };
    return table;
  }

  int16_t predictor_ = 0;
  uint8_t stepIndex_ = 0;
};

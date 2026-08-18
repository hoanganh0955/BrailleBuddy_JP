#pragma once

#include <Arduino.h>

namespace Pins {
// Braille matrix:
//   D1 GPIO4    D4 GPIO7
//   D2 GPIO5    D5 GPIO15
//   D3 GPIO6    D6 GPIO16
constexpr uint8_t BRAILLE[6] = {4, 5, 6, 7, 15, 16};

constexpr uint8_t PRACTICE = 8;
constexpr uint8_t STUDY = 18;
constexpr uint8_t SEND = 17;

// MAX98357A
constexpr int8_t I2S_DIN = 11;
constexpr int8_t I2S_BCLK = 12;
constexpr int8_t I2S_LRC = 13;
}  // namespace Pins

namespace Timing {
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t STUDY_LONG_PRESS_MS = 2000;
// At the end of each lesson the learner has this long to start holding Study.
// If Study is not held, the next lesson starts automatically.
constexpr uint32_t LESSON_DECISION_AUTO_CONTINUE_MS = 3500;
constexpr uint8_t DOT_SAMPLE_COUNT = 7;
constexpr uint8_t DOT_SAMPLE_INTERVAL_MS = 2;
}  // namespace Timing

namespace AudioConfig {
constexpr uint32_t SAMPLE_RATE = 12000;
constexpr uint16_t OUTPUT_BITS_PER_SAMPLE = 16;
constexpr uint16_t ADPCM_BITS_PER_SAMPLE = 4;
constexpr uint16_t IMA_ADPCM_FORMAT_CODE = 0x0011;
constexpr size_t MONO_FRAMES_PER_CHUNK = 256;
constexpr size_t MAX_ADPCM_BLOCK_ALIGN = 2048;
// ADPCM expands back to full-range PCM. Limiting it here prevents loud voice
// peaks from clipping the MAX98357A and the small speaker.
constexpr uint8_t WAV_VOLUME_PERCENT = 55;
// Short ramps and silence remove clicks between consecutive prompt files.
constexpr uint16_t FADE_MS = 8;
constexpr size_t FADE_FRAMES =
    (SAMPLE_RATE * static_cast<size_t>(FADE_MS)) / 1000U;
constexpr size_t INTER_FILE_SILENCE_FRAMES = 64;
constexpr int16_t FALLBACK_AMPLITUDE = 6500;
constexpr uint16_t FALLBACK_TONE_MS = 500;
constexpr float CORRECT_TONE_HZ = 880.0f;
constexpr float WRONG_TONE_HZ = 330.0f;
constexpr float AUDIO_TWO_PI = 6.2831853071795864769f;
static_assert(WAV_VOLUME_PERCENT > 0U && WAV_VOLUME_PERCENT <= 100U,
              "WAV volume must be between 1 and 100 percent.");
}  // namespace AudioConfig

namespace AudioPath {
constexpr const char* POWER_ON = "/audio/system/power_on_intro.wav";
constexpr const char* WELCOME_BACK = "/audio/system/welcome_back.wav";
constexpr const char* UNKNOWN_PATTERN = "/audio/system/unknown_pattern.wav";
constexpr const char* GOODBYE = "/audio/system/goodbye.wav";
constexpr const char* NO_LEARNED_ITEMS = "/audio/system/no_learned_items.wav";

constexpr const char* STUDY_INTRO = "/audio/study/study_intro.wav";
constexpr const char* STUDY_NEXT_OR_REPEAT =
    "/audio/study/study_next_or_repeat.wav";
constexpr const char* CONTINUE_NEXT = "/audio/study/continue_next.wav";
constexpr const char* REPEAT_LESSON = "/audio/study/repeat_lesson.wav";

constexpr const char* PRACTICE_INTRO = "/audio/practice/practice_intro.wav";
}  // namespace AudioPath

#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>
#include <LittleFS.h>
#include <math.h>

#include "BrailleConfig.h"
#include "ImaAdpcmDecoder.h"

class AudioPlayer {
 public:
  bool begin() {
    filesystemReady_ = LittleFS.begin(false);

    // ESP_I2S pin order: BCLK, WS/LRC, DOUT, DIN, MCLK.
    i2s_.setPins(Pins::I2S_BCLK, Pins::I2S_LRC, Pins::I2S_DIN, -1, -1);
    i2sReady_ =
        i2s_.begin(I2S_MODE_STD,
                   AudioConfig::SAMPLE_RATE,
                   I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_STEREO);
    return filesystemReady_ && i2sReady_;
  }

  bool ready() const {
    return filesystemReady_ && i2sReady_;
  }

  bool busy() const {
    return busy_;
  }

  bool play(const char* path) {
    if (!ready()) {
      Serial.println(F("音声システムを使用できません。"));
      return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
      Serial.println(F("音声ファイルが見つかりません。"));
      return false;
    }

    WavInfo info;
    if (!parseWav(file, info) || !isSupported(info)) {
      file.close();
      Serial.println(F("音声ファイルは12kHz・mono・4bit IMA ADPCM WAVにしてください。"));
      return false;
    }

    busy_ = true;
    decodedFrameCount_ = 0;
    playbackSampleIndex_ = 0;
    playbackSampleCount_ = info.factSampleCount;

    const bool startedCleanly =
        writeSilence(AudioConfig::INTER_FILE_SILENCE_FRAMES);
    const bool streamed = startedCleanly && streamImaAdpcm(file, info);
    const bool endedCleanly =
        writeSilence(AudioConfig::INTER_FILE_SILENCE_FRAMES);
    const bool played = streamed && endedCleanly;
    busy_ = false;
    file.close();

    if (!played) {
      Serial.println(F("音声の再生に失敗しました。"));
    }
    return played;
  }

  void playCorrectFallback() {
    playTone(AudioConfig::CORRECT_TONE_HZ, AudioConfig::FALLBACK_TONE_MS);
  }

  void playWrongFallback() {
    playTone(AudioConfig::WRONG_TONE_HZ, AudioConfig::FALLBACK_TONE_MS);
  }

 private:
  struct WavInfo {
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t averageBytesPerSecond = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
    uint16_t formatExtraSize = 0;
    uint16_t samplesPerBlock = 0;
    uint32_t factSampleCount = 0;
    uint32_t dataOffset = 0;
    uint32_t dataSize = 0;
  };

  static uint16_t readU16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8U);
  }

  static int16_t readS16(const uint8_t* bytes) {
    return static_cast<int16_t>(readU16(bytes));
  }

  static uint32_t readU32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8U) |
           (static_cast<uint32_t>(bytes[2]) << 16U) |
           (static_cast<uint32_t>(bytes[3]) << 24U);
  }

  static bool readExact(File& file, uint8_t* destination, size_t count) {
    size_t offset = 0;
    while (offset < count) {
      const size_t received = file.read(destination + offset, count - offset);
      if (received == 0) {
        return false;
      }
      offset += received;
    }
    return true;
  }

  static bool chunkIdEquals(const uint8_t* chunkId, const char* expected) {
    return chunkId[0] == static_cast<uint8_t>(expected[0]) &&
           chunkId[1] == static_cast<uint8_t>(expected[1]) &&
           chunkId[2] == static_cast<uint8_t>(expected[2]) &&
           chunkId[3] == static_cast<uint8_t>(expected[3]);
  }

  static bool isSupported(const WavInfo& info) {
    if (info.audioFormat != AudioConfig::IMA_ADPCM_FORMAT_CODE ||
        info.channels != 1U ||
        info.sampleRate != AudioConfig::SAMPLE_RATE ||
        info.bitsPerSample != AudioConfig::ADPCM_BITS_PER_SAMPLE ||
        info.blockAlign < 5U ||
        info.blockAlign > AudioConfig::MAX_ADPCM_BLOCK_ALIGN ||
        info.samplesPerBlock < 2U) {
      return false;
    }

    const uint32_t calculatedSamples =
        1U + (static_cast<uint32_t>(info.blockAlign) - 4U) * 2U;
    return calculatedSamples == info.samplesPerBlock;
  }

  bool parseFormatChunk(File& file,
                        uint32_t chunkDataOffset,
                        uint32_t chunkSize,
                        WavInfo& info) {
    if (chunkSize < 16U) {
      return false;
    }

    uint8_t baseFormat[16];
    if (!readExact(file, baseFormat, sizeof(baseFormat))) {
      return false;
    }

    info.audioFormat = readU16(baseFormat);
    info.channels = readU16(baseFormat + 2);
    info.sampleRate = readU32(baseFormat + 4);
    info.averageBytesPerSecond = readU32(baseFormat + 8);
    info.blockAlign = readU16(baseFormat + 12);
    info.bitsPerSample = readU16(baseFormat + 14);

    if (chunkSize >= 18U) {
      uint8_t extraSizeBytes[2];
      if (!readExact(file, extraSizeBytes, sizeof(extraSizeBytes))) {
        return false;
      }
      info.formatExtraSize = readU16(extraSizeBytes);
    }

    if (info.audioFormat == AudioConfig::IMA_ADPCM_FORMAT_CODE) {
      if (chunkSize < 20U || info.formatExtraSize < 2U) {
        return false;
      }
      uint8_t samplesPerBlockBytes[2];
      if (!readExact(file,
                     samplesPerBlockBytes,
                     sizeof(samplesPerBlockBytes))) {
        return false;
      }
      info.samplesPerBlock = readU16(samplesPerBlockBytes);
    }

    const uint32_t nextChunk = chunkDataOffset + chunkSize + (chunkSize & 1U);
    return file.seek(nextChunk, SeekSet);
  }

  bool parseWav(File& file, WavInfo& info) {
    uint8_t riffHeader[12];
    if (!readExact(file, riffHeader, sizeof(riffHeader)) ||
        !chunkIdEquals(riffHeader, "RIFF") ||
        !chunkIdEquals(riffHeader + 8, "WAVE")) {
      return false;
    }

    bool foundFormat = false;
    bool foundData = false;
    const uint32_t fileSize = file.size();

    while (file.position() + 8U <= fileSize) {
      uint8_t chunkHeader[8];
      if (!readExact(file, chunkHeader, sizeof(chunkHeader))) {
        return false;
      }

      const uint32_t chunkSize = readU32(chunkHeader + 4);
      const uint32_t chunkDataOffset = file.position();
      const uint32_t paddedSize = chunkSize + (chunkSize & 1U);
      if (chunkDataOffset > fileSize || paddedSize > fileSize - chunkDataOffset) {
        return false;
      }

      if (chunkIdEquals(chunkHeader, "fmt ")) {
        if (!parseFormatChunk(file, chunkDataOffset, chunkSize, info)) {
          return false;
        }
        foundFormat = true;
        continue;
      }

      if (chunkIdEquals(chunkHeader, "fact") && chunkSize >= 4U) {
        uint8_t sampleCountBytes[4];
        if (!readExact(file, sampleCountBytes, sizeof(sampleCountBytes))) {
          return false;
        }
        info.factSampleCount = readU32(sampleCountBytes);
      } else if (chunkIdEquals(chunkHeader, "data")) {
        info.dataOffset = chunkDataOffset;
        info.dataSize = chunkSize;
        foundData = true;
      }

      if (!file.seek(chunkDataOffset + paddedSize, SeekSet)) {
        return false;
      }
    }

    return foundFormat && foundData && file.seek(info.dataOffset, SeekSet);
  }

  bool writeAll(uint8_t* data, size_t count) {
    size_t offset = 0;
    while (offset < count) {
      const size_t written = i2s_.write(data + offset, count - offset);
      if (written == 0) {
        return false;
      }
      offset += written;
    }
    return true;
  }

  bool writeSilence(size_t frameCount) {
    int16_t silence[64 * 2] = {0};
    while (frameCount > 0U) {
      const size_t frames = frameCount < 64U ? frameCount : 64U;
      if (!writeAll(reinterpret_cast<uint8_t*>(silence),
                    frames * 2U * sizeof(int16_t))) {
        return false;
      }
      frameCount -= frames;
    }
    return true;
  }

  bool flushDecodedFrames() {
    if (decodedFrameCount_ == 0U) {
      return true;
    }
    const size_t bytes = decodedFrameCount_ * 2U * sizeof(int16_t);
    if (!writeAll(reinterpret_cast<uint8_t*>(decodedStereo_), bytes)) {
      return false;
    }
    decodedFrameCount_ = 0;
    return true;
  }

  bool queueDecodedSample(int16_t sample) {
    uint32_t fadePermille = 1000U;
    if (AudioConfig::FADE_FRAMES > 0U &&
        playbackSampleIndex_ < AudioConfig::FADE_FRAMES) {
      fadePermille = static_cast<uint32_t>(
          ((playbackSampleIndex_ + 1U) * 1000U) /
          AudioConfig::FADE_FRAMES);
    }

    if (AudioConfig::FADE_FRAMES > 0U && playbackSampleCount_ > 0U &&
        playbackSampleIndex_ < playbackSampleCount_) {
      const uint32_t remaining =
          playbackSampleCount_ - playbackSampleIndex_;
      if (remaining <= AudioConfig::FADE_FRAMES) {
        const uint32_t fadeOutPermille = static_cast<uint32_t>(
            (remaining * 1000U) / AudioConfig::FADE_FRAMES);
        if (fadeOutPermille < fadePermille) {
          fadePermille = fadeOutPermille;
        }
      }
    }

    const int32_t outputSample = static_cast<int32_t>(
        (static_cast<int64_t>(sample) *
         AudioConfig::WAV_VOLUME_PERCENT * fadePermille) /
        100000LL);
    const int16_t scaledSample = static_cast<int16_t>(outputSample);

    decodedStereo_[decodedFrameCount_ * 2U] = scaledSample;
    decodedStereo_[decodedFrameCount_ * 2U + 1U] = scaledSample;
    ++playbackSampleIndex_;
    ++decodedFrameCount_;
    if (decodedFrameCount_ == AudioConfig::MONO_FRAMES_PER_CHUNK) {
      return flushDecodedFrames();
    }
    return true;
  }

  bool decodeBlock(const uint8_t* block,
                   size_t blockBytes,
                   uint32_t& samplesRemaining) {
    if (blockBytes < 4U ||
        !decoder_.beginBlock(readS16(block), block[2])) {
      return false;
    }

    if (samplesRemaining == 0U) {
      return true;
    }
    if (!queueDecodedSample(decoder_.predictor())) {
      return false;
    }
    if (samplesRemaining != UINT32_MAX) {
      --samplesRemaining;
    }

    for (size_t byteIndex = 4U;
         byteIndex < blockBytes && samplesRemaining != 0U;
         ++byteIndex) {
      const uint8_t packed = block[byteIndex];
      if (!queueDecodedSample(decoder_.decodeNibble(packed & 0x0FU))) {
        return false;
      }
      if (samplesRemaining != UINT32_MAX) {
        --samplesRemaining;
      }

      if (samplesRemaining == 0U) {
        break;
      }
      if (!queueDecodedSample(decoder_.decodeNibble(packed >> 4U))) {
        return false;
      }
      if (samplesRemaining != UINT32_MAX) {
        --samplesRemaining;
      }
    }
    return true;
  }

  bool streamImaAdpcm(File& file, const WavInfo& info) {
    uint32_t encodedRemaining = info.dataSize;
    uint32_t samplesRemaining =
        info.factSampleCount == 0U ? UINT32_MAX : info.factSampleCount;

    while (encodedRemaining > 0U && samplesRemaining != 0U) {
      const size_t blockBytes =
          encodedRemaining < info.blockAlign ? encodedRemaining : info.blockAlign;
      if (blockBytes < 4U ||
          !readExact(file, adpcmBlock_, blockBytes) ||
          !decodeBlock(adpcmBlock_, blockBytes, samplesRemaining)) {
        return false;
      }
      encodedRemaining -= blockBytes;
      yield();
    }

    return flushDecodedFrames();
  }

  void playTone(float frequencyHz, uint16_t durationMs) {
    if (!i2sReady_) {
      return;
    }

    busy_ = true;
    writeSilence(AudioConfig::INTER_FILE_SILENCE_FRAMES);
    const uint32_t totalFrames =
        (AudioConfig::SAMPLE_RATE * static_cast<uint32_t>(durationMs)) / 1000U;
    const float phaseIncrement =
        AudioConfig::AUDIO_TWO_PI * frequencyHz /
        static_cast<float>(AudioConfig::SAMPLE_RATE);
    int16_t samples[128 * 2];
    uint32_t produced = 0;
    float phase = 0.0f;

    while (produced < totalFrames) {
      const size_t frames =
          (totalFrames - produced) < 128U ? totalFrames - produced : 128U;
      for (size_t frame = 0; frame < frames; ++frame) {
        const uint32_t absoluteFrame = produced + frame;
        uint32_t fadePermille = 1000U;
        if (AudioConfig::FADE_FRAMES > 0U &&
            absoluteFrame < AudioConfig::FADE_FRAMES) {
          fadePermille = static_cast<uint32_t>(
              ((absoluteFrame + 1U) * 1000U) /
              AudioConfig::FADE_FRAMES);
        }
        const uint32_t remaining = totalFrames - absoluteFrame;
        if (AudioConfig::FADE_FRAMES > 0U &&
            remaining <= AudioConfig::FADE_FRAMES) {
          const uint32_t fadeOutPermille = static_cast<uint32_t>(
              (remaining * 1000U) / AudioConfig::FADE_FRAMES);
          if (fadeOutPermille < fadePermille) {
            fadePermille = fadeOutPermille;
          }
        }
        const int16_t sample = static_cast<int16_t>(
            sinf(phase) * static_cast<float>(AudioConfig::FALLBACK_AMPLITUDE) *
            static_cast<float>(fadePermille) / 1000.0f);
        samples[frame * 2U] = sample;
        samples[frame * 2U + 1U] = sample;
        phase += phaseIncrement;
        if (phase >= AudioConfig::AUDIO_TWO_PI) {
          phase -= AudioConfig::AUDIO_TWO_PI;
        }
      }
      if (!writeAll(reinterpret_cast<uint8_t*>(samples),
                    frames * 2U * sizeof(int16_t))) {
        break;
      }
      produced += frames;
    }

    writeSilence(AudioConfig::INTER_FILE_SILENCE_FRAMES);
    busy_ = false;
  }

  I2SClass i2s_;
  ImaAdpcmDecoder decoder_;
  uint8_t adpcmBlock_[AudioConfig::MAX_ADPCM_BLOCK_ALIGN];
  int16_t decodedStereo_[AudioConfig::MONO_FRAMES_PER_CHUNK * 2U];
  size_t decodedFrameCount_ = 0;
  uint32_t playbackSampleIndex_ = 0;
  uint32_t playbackSampleCount_ = 0;
  bool filesystemReady_ = false;
  bool i2sReady_ = false;
  bool busy_ = false;
};

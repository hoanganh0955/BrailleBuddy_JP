#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../code_cpp/BrailleBuddy_JP/ImaAdpcmDecoder.h"

static uint16_t readU16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8U);
}

static uint32_t readU32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8U) |
         (static_cast<uint32_t>(bytes[2]) << 16U) |
         (static_cast<uint32_t>(bytes[3]) << 24U);
}

static bool idEquals(const uint8_t* id, const char* expected) {
  return id[0] == expected[0] && id[1] == expected[1] &&
         id[2] == expected[2] && id[3] == expected[3];
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: decode_adpcm_file input.wav output.pcm\n";
    return 2;
  }

  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "cannot open input\n";
    return 2;
  }
  std::vector<uint8_t> wav((std::istreambuf_iterator<char>(input)), {});
  if (wav.size() < 12 || !idEquals(wav.data(), "RIFF") ||
      !idEquals(wav.data() + 8, "WAVE")) {
    std::cerr << "not a WAV file\n";
    return 2;
  }

  uint16_t format = 0;
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t blockAlign = 0;
  uint16_t bitsPerSample = 0;
  uint16_t samplesPerBlock = 0;
  uint32_t factSamples = 0;
  const uint8_t* data = nullptr;
  uint32_t dataSize = 0;

  size_t position = 12;
  while (position + 8 <= wav.size()) {
    const uint8_t* header = wav.data() + position;
    const uint32_t size = readU32(header + 4);
    const size_t body = position + 8;
    const size_t padded = size + (size & 1U);
    if (body + padded > wav.size()) {
      std::cerr << "invalid chunk\n";
      return 2;
    }
    if (idEquals(header, "fmt ") && size >= 20) {
      format = readU16(wav.data() + body);
      channels = readU16(wav.data() + body + 2);
      sampleRate = readU32(wav.data() + body + 4);
      blockAlign = readU16(wav.data() + body + 12);
      bitsPerSample = readU16(wav.data() + body + 14);
      samplesPerBlock = readU16(wav.data() + body + 18);
    } else if (idEquals(header, "fact") && size >= 4) {
      factSamples = readU32(wav.data() + body);
    } else if (idEquals(header, "data")) {
      data = wav.data() + body;
      dataSize = size;
    }
    position = body + padded;
  }

  if (format != 0x0011 || channels != 1 || sampleRate != 12000 ||
      bitsPerSample != 4 || blockAlign < 5 || samplesPerBlock < 2 || !data) {
    std::cerr << "unsupported format\n";
    return 2;
  }

  std::ofstream output(argv[2], std::ios::binary);
  ImaAdpcmDecoder decoder;
  uint32_t encodedRemaining = dataSize;
  uint32_t samplesRemaining = factSamples == 0 ? UINT32_MAX : factSamples;
  size_t offset = 0;
  uint32_t writtenSamples = 0;

  auto writeSample = [&](int16_t sample) {
    const uint8_t bytes[2] = {
        static_cast<uint8_t>(sample & 0xFF),
        static_cast<uint8_t>((static_cast<uint16_t>(sample) >> 8U) & 0xFF),
    };
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
    ++writtenSamples;
    if (samplesRemaining != UINT32_MAX) {
      --samplesRemaining;
    }
  };

  while (encodedRemaining > 0 && samplesRemaining != 0) {
    const size_t blockBytes =
        encodedRemaining < blockAlign ? encodedRemaining : blockAlign;
    const uint8_t* block = data + offset;
    if (blockBytes < 4 ||
        !decoder.beginBlock(static_cast<int16_t>(readU16(block)), block[2])) {
      std::cerr << "invalid block\n";
      return 2;
    }

    writeSample(decoder.predictor());
    for (size_t index = 4;
         index < blockBytes && samplesRemaining != 0;
         ++index) {
      const uint8_t packed = block[index];
      writeSample(decoder.decodeNibble(packed & 0x0F));
      if (samplesRemaining == 0) {
        break;
      }
      writeSample(decoder.decodeNibble(packed >> 4U));
    }
    offset += blockBytes;
    encodedRemaining -= blockBytes;
  }

  std::cout << "decoded_samples=" << writtenSamples << '\n';
  return output ? 0 : 2;
}

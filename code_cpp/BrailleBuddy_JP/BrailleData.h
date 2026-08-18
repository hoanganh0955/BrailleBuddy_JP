#pragma once

#include <Arduino.h>

struct BrailleItem {
  const char* id;
  const char* kana;
  uint8_t mask;
  uint8_t lesson;
};

// Mask bit 0 = dot 1, ..., bit 5 = dot 6.
constexpr BrailleItem ITEMS[] = {
    {"a", "あ", 1, 1},
    {"i", "い", 3, 1},
    {"u", "う", 9, 1},
    {"e", "え", 11, 1},
    {"o", "お", 10, 1},

    {"ka", "か", 33, 2},
    {"ki", "き", 35, 2},
    {"ku", "く", 41, 2},
    {"ke", "け", 43, 2},
    {"ko", "こ", 42, 2},

    {"sa", "さ", 49, 3},
    {"shi", "し", 51, 3},
    {"su", "す", 57, 3},
    {"se", "せ", 59, 3},
    {"so", "そ", 58, 3},

    {"ta", "た", 21, 4},
    {"chi", "ち", 23, 4},
    {"tsu", "つ", 29, 4},
    {"te", "て", 31, 4},
    {"to", "と", 30, 4},

    {"na", "な", 5, 5},
    {"ni", "に", 7, 5},
    {"nu", "ぬ", 13, 5},
    {"ne", "ね", 15, 5},
    {"no", "の", 14, 5},

    {"ha", "は", 37, 6},
    {"hi", "ひ", 39, 6},
    {"fu", "ふ", 45, 6},
    {"he", "へ", 47, 6},
    {"ho", "ほ", 46, 6},

    {"ma", "ま", 53, 7},
    {"mi", "み", 55, 7},
    {"mu", "む", 61, 7},
    {"me", "め", 63, 7},
    {"mo", "も", 62, 7},

    {"ya", "や", 12, 8},
    {"yu", "ゆ", 44, 8},
    {"yo", "よ", 28, 8},

    {"ra", "ら", 17, 9},
    {"ri", "り", 19, 9},
    {"ru", "る", 25, 9},
    {"re", "れ", 27, 9},
    {"ro", "ろ", 26, 9},

    {"wa", "わ", 4, 10},
    {"wo", "を", 20, 10},
    {"n", "ん", 52, 10},
    {"small_tsu", "っ", 2, 10},
    {"long_vowel", "ー", 18, 10},
};

constexpr uint8_t ITEM_COUNT =
    static_cast<uint8_t>(sizeof(ITEMS) / sizeof(ITEMS[0]));
constexpr uint8_t LESSON_COUNT = 10;
static_assert(ITEM_COUNT == 48, "The catalog must contain 48 characters.");

constexpr const char* QUIZ_INTROS[] = {
    "/audio/practice/quiz_intro_01.wav",
    "/audio/practice/quiz_intro_02.wav",
    "/audio/practice/quiz_intro_03.wav",
    "/audio/practice/quiz_intro_04.wav",
    "/audio/practice/quiz_intro_05.wav",
};

// These names intentionally follow Practice JP-VN.docx exactly.
constexpr const char* CORRECT_RESPONSES[] = {
    "/audio/practice/correct_01.wav",
    "/audio/practice/correct_02.wav",
    "/audio/practice/correct_03.wav",
    "/audio/practice/correct_04.wav",
    "/audio/practice/correct_05.wav",
    "/audio/practice/correct_06.wav",
    "/audio/practice/correct_07.wav",
    "/audio/practice/correct_08.wav",
    "/audio/practice/correct_09.wav",
    "/audio/practice/correct_010.wav",
    "/audio/practice/correct_011.wav",
    "/audio/practice/correct_012.wav",
    "/audio/practice/correct_013.wav",
    "/audio/practice/correct_014.wav",
    "/audio/practice/correct_015.wav",
};

constexpr const char* WRONG_1_RESPONSES[] = {
    "/audio/practice/wrong_01_01.wav",
    "/audio/practice/wrong_01_02.wav",
    "/audio/practice/wrong_01_03.wav",
    "/audio/practice/wrong_01_04.wav",
    "/audio/practice/wrong_01_05.wav",
    "/audio/practice/wrong_01_06.wav",
    "/audio/practice/wrong_01_07.wav",
    "/audio/practice/wrong_01_08.wav",
    "/audio/practice/wrong_01_09.wav",
    "/audio/practice/wrong_01_010.wav",
};

constexpr const char* WRONG_2_RESPONSES[] = {
    "/audio/practice/wrong_02_01.wav",
    "/audio/practice/wrong_02_02.wav",
    "/audio/practice/wrong_02_03.wav",
    "/audio/practice/wrong_02_04.wav",
    "/audio/practice/wrong_02_05.wav",
    "/audio/practice/wrong_02_06.wav",
    "/audio/practice/wrong_02_07.wav",
    "/audio/practice/wrong_02_08.wav",
    "/audio/practice/wrong_02_09.wav",
    "/audio/practice/wrong_02_010.wav",
};

constexpr const char* WRONG_3_RESPONSES[] = {
    "/audio/practice/wrong_03_01.wav",
    "/audio/practice/wrong_03_02.wav",
    "/audio/practice/wrong_03_03.wav",
    "/audio/practice/wrong_03_04.wav",
    "/audio/practice/wrong_03_05.wav",
    "/audio/practice/wrong_03_06.wav",
    "/audio/practice/wrong_03_07.wav",
    "/audio/practice/wrong_03_08.wav",
    "/audio/practice/wrong_03_09.wav",
    "/audio/practice/wrong_03_010.wav",
};

template <size_t N>
constexpr size_t arrayCount(const char* const (&)[N]) {
  return N;
}

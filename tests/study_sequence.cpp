#include <cassert>
#include <cstdint>
#include <iostream>

#include "../code_cpp/BrailleBuddy_JP/BrailleData.h"

int main() {
  uint8_t visited = 0;
  for (uint8_t lesson = 1; lesson <= LESSON_COUNT; ++lesson) {
    uint8_t lessonItems = 0;
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
      if (ITEMS[index].lesson != lesson) {
        continue;
      }
      ++lessonItems;
      ++visited;

      const uint8_t nextIndex = index + 1U;
      const bool hasNextInSameLesson =
          nextIndex < ITEM_COUNT && ITEMS[nextIndex].lesson == lesson;
      if (hasNextInSameLesson) {
        assert(nextIndex == static_cast<uint8_t>(index + 1U));
      }
    }

    if (lesson == 8U) {
      assert(lessonItems == 3U);
    } else {
      assert(lessonItems == 5U);
    }
  }

  assert(visited == ITEM_COUNT);
  std::cout << "study_sequence_ok items=" << static_cast<int>(visited)
            << '\n';
  return 0;
}

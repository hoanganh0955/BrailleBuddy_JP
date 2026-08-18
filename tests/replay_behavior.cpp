#include <cassert>
#include <cstdint>

#include "host_stubs/Arduino.h"
#include "host_stubs/ESP_I2S.h"
#include "host_stubs/LittleFS.h"
#include "host_stubs/Preferences.h"
#include "host_stubs/esp_system.h"

#define private public
#include "../code_cpp/BrailleBuddy_JP/BrailleApp.h"
#undef private

int main() {
  BrailleApp app;

  app.nextStudyIndex_ = 0;
  app.beginLessonSelection();
  assert(app.state_ == AppState::LESSON_SELECT);
  assert(app.selectedLesson_ == 1);

  app.handleStudyButton(ButtonEvent::SHORT_PRESS);
  assert(app.selectedLesson_ == 2);

  for (uint8_t lesson = 2; lesson <= LESSON_COUNT; ++lesson) {
    app.handleStudyButton(ButtonEvent::SHORT_PRESS);
  }
  assert(app.selectedLesson_ == 1);

  app.handlePracticeButton(ButtonEvent::SHORT_PRESS);
  assert(app.selectedLesson_ == LESSON_COUNT);
  app.handleStudyButton(ButtonEvent::SHORT_PRESS);
  assert(app.selectedLesson_ == 1);

  app.selectedLesson_ = 2;
  app.handleSubmission(0);
  assert(app.state_ == AppState::STUDY_WAIT_INPUT);
  assert(app.currentItemIndex_ == app.lessonStart(2));

  app.state_ = AppState::STUDY_WAIT_INPUT;
  app.currentItemIndex_ = 6;
  app.learnedCount_ = 6;
  app.nextStudyIndex_ = 6;
  app.handleStudyButton(ButtonEvent::SHORT_PRESS);

  assert(app.state_ == AppState::STUDY_WAIT_INPUT);
  assert(app.currentItemIndex_ == 6);
  assert(app.learnedCount_ == 6);
  assert(app.nextStudyIndex_ == 6);

  app.state_ = AppState::PRACTICE_WAIT_INPUT;
  app.currentItemIndex_ = 4;
  app.previousPracticeIndex_ = 4;
  app.wrongAttempts_ = 2;
  app.remediationRequired_ = true;
  app.handlePracticeButton(ButtonEvent::SHORT_PRESS);

  assert(app.state_ == AppState::PRACTICE_WAIT_INPUT);
  assert(app.currentItemIndex_ == 4);
  assert(app.previousPracticeIndex_ == 4);
  assert(app.wrongAttempts_ == 2);
  assert(app.remediationRequired_);

  return 0;
}

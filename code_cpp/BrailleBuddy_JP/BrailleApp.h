#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>

#include "AudioPlayer.h"
#include "BrailleConfig.h"
#include "BrailleData.h"

enum class ButtonEvent : uint8_t {
  NONE,
  SHORT_PRESS,
  LONG_PRESS,
};

class DebouncedButton {
 public:
  DebouncedButton(uint8_t pin, uint32_t longPressMs)
      : pin_(pin), longPressMs_(longPressMs) {}

  void begin() {
    pinMode(pin_, INPUT_PULLUP);
    rawState_ = digitalRead(pin_);
    stableState_ = rawState_;
    rawChangedAt_ = millis();
  }

  ButtonEvent update() {
    const bool currentRaw = digitalRead(pin_);
    const uint32_t now = millis();

    if (currentRaw != rawState_) {
      rawState_ = currentRaw;
      rawChangedAt_ = now;
    }

    if (stableState_ != rawState_ &&
        (now - rawChangedAt_) >= Timing::DEBOUNCE_MS) {
      stableState_ = rawState_;
      if (stableState_ == LOW) {
        pressedAt_ = now;
        longPressReported_ = false;
      } else if (!longPressReported_) {
        return ButtonEvent::SHORT_PRESS;
      }
    }

    if (stableState_ == LOW && !longPressReported_ &&
        (now - pressedAt_) >= longPressMs_) {
      longPressReported_ = true;
      return ButtonEvent::LONG_PRESS;
    }

    return ButtonEvent::NONE;
  }

  bool isPressed() const {
    return stableState_ == LOW;
  }

 private:
  uint8_t pin_;
  uint32_t longPressMs_;
  bool rawState_ = HIGH;
  bool stableState_ = HIGH;
  bool longPressReported_ = false;
  uint32_t rawChangedAt_ = 0;
  uint32_t pressedAt_ = 0;
};

enum class AppState : uint8_t {
  HOME,
  RETURNING_USER_CHOICE,
  LESSON_SELECT,
  STUDY_WAIT_INPUT,
  LESSON_DECISION,
  PRACTICE_WAIT_INPUT,
};

class BrailleApp {
 public:
  BrailleApp()
      : practiceButton_(Pins::PRACTICE, Timing::STUDY_LONG_PRESS_MS),
        studyButton_(Pins::STUDY, Timing::STUDY_LONG_PRESS_MS),
        sendButton_(Pins::SEND, Timing::STUDY_LONG_PRESS_MS) {}

  void begin() {
    Serial.begin(115200);
    serialCommand_.reserve(80);

    for (uint8_t dot = 0; dot < 6; ++dot) {
      pinMode(Pins::BRAILLE[dot], INPUT_PULLUP);
    }
    practiceButton_.begin();
    studyButton_.begin();
    sendButton_.begin();

    preferencesReady_ = preferences_.begin("braille-jp-v1", false);
    bool launchedBefore = false;
    if (preferencesReady_) {
      launchedBefore = preferences_.getBool("launched", false);
      learnedCount_ = preferences_.getUChar("learned", 0);
      nextStudyIndex_ = preferences_.getUChar("next", 0);

      if (learnedCount_ > ITEM_COUNT) {
        learnedCount_ = 0;
      }
      if (nextStudyIndex_ > ITEM_COUNT) {
        nextStudyIndex_ = learnedCount_;
      }
      preferences_.putBool("launched", true);
    }

    randomSeed(esp_random());
    audio_.begin();

    if (launchedBefore) {
      state_ = AppState::RETURNING_USER_CHOICE;
      audio_.play(AudioPath::WELCOME_BACK);
      printReturningMenu();
    } else {
      state_ = AppState::HOME;
      audio_.play(AudioPath::POWER_ON);
      printHomeMenu();
    }
  }

  void update() {
    updateSerial();
    if (!audio_.busy()) {
      updateButtons();
      updateLessonDecisionTimeout();
    }
  }

 private:
  static const char* stateName(AppState state) {
    switch (state) {
      case AppState::HOME:
        return "待機中";
      case AppState::RETURNING_USER_CHOICE:
        return "再開方法を選択中";
      case AppState::LESSON_SELECT:
        return "レッスンを選択中";
      case AppState::STUDY_WAIT_INPUT:
        return "学習中";
      case AppState::LESSON_DECISION:
        return "次のレッスンを選択中";
      case AppState::PRACTICE_WAIT_INPUT:
        return "練習中";
    }
    return "不明";
  }

  void printHomeMenu() const {
    Serial.println(F("準備完了"));
    Serial.println(F("学習ボタン：学習"));
    Serial.println(F("練習ボタン：練習"));
  }

  void printReturningMenu() const {
    Serial.println(F("おかえりなさい"));
    Serial.println(F("学習ボタン：続きから学ぶ"));
    Serial.println(F("練習ボタン：前のレッスンを学び直す"));
  }

  uint8_t readBrailleMask() const {
    uint8_t pressedCount[6] = {0, 0, 0, 0, 0, 0};

    for (uint8_t sample = 0; sample < Timing::DOT_SAMPLE_COUNT; ++sample) {
      for (uint8_t dot = 0; dot < 6; ++dot) {
        if (digitalRead(Pins::BRAILLE[dot]) == LOW) {
          ++pressedCount[dot];
        }
      }
      if (sample + 1U < Timing::DOT_SAMPLE_COUNT) {
        delay(Timing::DOT_SAMPLE_INTERVAL_MS);
      }
    }

    const uint8_t majority = (Timing::DOT_SAMPLE_COUNT / 2U) + 1U;
    uint8_t mask = 0;
    for (uint8_t dot = 0; dot < 6; ++dot) {
      if (pressedCount[dot] >= majority) {
        mask |= static_cast<uint8_t>(1U << dot);
      }
    }
    return mask;
  }

  int16_t findItemByMask(uint8_t mask) const {
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
      if (ITEMS[index].mask == mask) {
        return index;
      }
    }
    return -1;
  }

  uint8_t lessonStart(uint8_t lesson) const {
    for (uint8_t index = 0; index < ITEM_COUNT; ++index) {
      if (ITEMS[index].lesson == lesson) {
        return index;
      }
    }
    return 0;
  }

  uint8_t lessonEnd(uint8_t lesson) const {
    uint8_t result = lessonStart(lesson);
    for (uint8_t index = result; index < ITEM_COUNT; ++index) {
      if (ITEMS[index].lesson != lesson) {
        break;
      }
      result = index;
    }
    return result;
  }

  void makeLessonTitlePath(uint8_t lesson, char* path, size_t pathSize) const {
    snprintf(path, pathSize, "/audio/lessons/lesson%02u_title.wav", lesson);
  }

  void playSelectedLessonTitle() {
    char path[80];
    makeLessonTitlePath(selectedLesson_, path, sizeof(path));
    audio_.play(path);
    Serial.print(F("レッスン "));
    Serial.println(selectedLesson_);
  }

  void beginLessonSelection() {
    returningChoiceResolved_ = true;
    selectedLesson_ = nextStudyIndex_ < ITEM_COUNT
                          ? ITEMS[nextStudyIndex_].lesson
                          : 1U;
    state_ = AppState::LESSON_SELECT;
    audio_.play(AudioPath::STUDY_INTRO);
    playSelectedLessonTitle();
  }

  void selectNextLesson() {
    if (state_ != AppState::LESSON_SELECT) {
      return;
    }
    selectedLesson_ =
        static_cast<uint8_t>((selectedLesson_ % LESSON_COUNT) + 1U);
    playSelectedLessonTitle();
  }

  void selectPreviousLesson() {
    if (state_ != AppState::LESSON_SELECT) {
      return;
    }
    selectedLesson_ = selectedLesson_ <= 1U
                          ? LESSON_COUNT
                          : static_cast<uint8_t>(selectedLesson_ - 1U);
    playSelectedLessonTitle();
  }

  void confirmSelectedLesson() {
    if (state_ != AppState::LESSON_SELECT) {
      return;
    }
    startStudyAt(lessonStart(selectedLesson_), false);
  }

  void makeLessonStoryPath(uint8_t lesson, char* path, size_t pathSize) const {
    snprintf(path, pathSize, "/audio/lessons/lesson%02u_story.wav", lesson);
  }

  void makeLessonItemPath(uint8_t itemIndex,
                          char* path,
                          size_t pathSize) const {
    const BrailleItem& item = ITEMS[itemIndex];
    snprintf(path,
             pathSize,
             "/audio/lessons/lesson%02u_%s.wav",
             item.lesson,
             item.id);
  }

  void makeCharacterPath(uint8_t itemIndex, char* path, size_t pathSize) const {
    snprintf(path,
             pathSize,
             "/audio/chars/char_%s.wav",
             ITEMS[itemIndex].id);
  }

  void makeHintPath(uint8_t itemIndex, char* path, size_t pathSize) const {
    snprintf(path,
             pathSize,
             "/audio/hints/hint_%s.wav",
             ITEMS[itemIndex].id);
  }

  template <size_t N>
  bool playRandom(const char* const (&paths)[N]) {
    return audio_.play(paths[static_cast<size_t>(random(N))]);
  }

  void playCorrectResponse() {
    if (!playRandom(CORRECT_RESPONSES)) {
      audio_.playCorrectFallback();
    }
  }

  void playWrongResponse(const char* const* paths, size_t pathCount) {
    const char* selected = paths[static_cast<size_t>(random(pathCount))];
    if (!audio_.play(selected)) {
      audio_.playWrongFallback();
    }
  }

  void saveProgress() {
    if (!preferencesReady_) {
      return;
    }
    preferences_.putUChar("learned", learnedCount_);
    preferences_.putUChar("next", nextStudyIndex_);
  }

  void markLearned(uint8_t itemIndex) {
    if (itemIndex == learnedCount_ && learnedCount_ < ITEM_COUNT) {
      ++learnedCount_;
    }
    if (itemIndex + 1U > nextStudyIndex_) {
      nextStudyIndex_ = itemIndex + 1U;
    }
    if (nextStudyIndex_ > ITEM_COUNT) {
      nextStudyIndex_ = ITEM_COUNT;
    }
    saveProgress();
  }

  void presentStudyItem(bool announceLessonTitle) {
    const BrailleItem& item = ITEMS[currentItemIndex_];
    char path[80];

    if (announceLessonTitle) {
      makeLessonTitlePath(item.lesson, path, sizeof(path));
      audio_.play(path);
    }

    makeLessonItemPath(currentItemIndex_, path, sizeof(path));
    audio_.play(path);

    // Normal Study/Practice output is intentionally only the Japanese symbol.
    Serial.println(item.kana);
    state_ = AppState::STUDY_WAIT_INPUT;
  }

  void replayCurrentStudyItem() {
    if (state_ != AppState::STUDY_WAIT_INPUT) {
      return;
    }

    char path[80];
    makeLessonItemPath(currentItemIndex_, path, sizeof(path));
    audio_.play(path);
    Serial.println(ITEMS[currentItemIndex_].kana);
  }

  void startStudyAt(uint8_t itemIndex, bool playIntro) {
    if (itemIndex >= ITEM_COUNT) {
      state_ = AppState::HOME;
      Serial.println(F("ひらがなの学習が終わりました。"));
      printHomeMenu();
      return;
    }
    currentItemIndex_ = itemIndex;
    returningChoiceResolved_ = true;

    if (playIntro) {
      audio_.play(AudioPath::STUDY_INTRO);
    }
    presentStudyItem(currentItemIndex_ == lessonStart(ITEMS[currentItemIndex_].lesson));
  }

  void resumeStudy() {
    if (nextStudyIndex_ >= ITEM_COUNT) {
      state_ = AppState::HOME;
      Serial.println(F("ひらがなの学習が終わりました。"));
      printHomeMenu();
      return;
    }
    if (nextStudyIndex_ > 0) {
      audio_.play(AudioPath::CONTINUE_NEXT);
    }
    startStudyAt(nextStudyIndex_, true);
  }

  void repeatLesson(uint8_t lesson) {
    returningChoiceResolved_ = true;
    audio_.play(AudioPath::REPEAT_LESSON);
    startStudyAt(lessonStart(lesson), false);
  }

  void reviewPreviousLesson() {
    const uint8_t lesson =
        learnedCount_ == 0 ? 1 : ITEMS[learnedCount_ - 1U].lesson;
    repeatLesson(lesson);
  }

  void finishLesson(uint8_t lesson) {
    completedLesson_ = lesson;
    char path[80];
    makeLessonStoryPath(lesson, path, sizeof(path));
    audio_.play(path);
    audio_.play(AudioPath::STUDY_NEXT_OR_REPEAT);

    Serial.println(F("学習ボタンを短く押す：次へ"));
    Serial.println(F("学習ボタンを二秒押す：もう一度"));
    state_ = AppState::LESSON_DECISION;
    lessonDecisionStartedAt_ = millis();
  }

  void continueAfterLesson() {
    const uint8_t nextIndex = lessonEnd(completedLesson_) + 1U;
    audio_.play(AudioPath::CONTINUE_NEXT);

    if (nextIndex >= ITEM_COUNT) {
      nextStudyIndex_ = ITEM_COUNT;
      saveProgress();
      Serial.println(F("ひらがなの学習が終わりました。"));
      state_ = AppState::HOME;
      printHomeMenu();
      return;
    }

    startStudyAt(nextIndex, false);
  }

  void handleStudyCorrect() {
    const uint8_t answeredItemIndex = currentItemIndex_;
    const uint8_t lesson = ITEMS[answeredItemIndex].lesson;
    playCorrectResponse();
    Serial.println(F("正解"));
    markLearned(answeredItemIndex);

    const uint8_t nextItemIndex = answeredItemIndex + 1U;
    if (nextItemIndex < ITEM_COUNT &&
        ITEMS[nextItemIndex].lesson == lesson) {
      currentItemIndex_ = nextItemIndex;
      presentStudyItem(false);
      return;
    }

    finishLesson(lesson);
  }

  uint8_t choosePracticeItem() {
    if (learnedCount_ <= 1U) {
      return 0;
    }

    uint8_t choice = previousPracticeIndex_;
    for (uint8_t attempt = 0;
         attempt < 12U && choice == previousPracticeIndex_;
         ++attempt) {
      choice = static_cast<uint8_t>(random(learnedCount_));
    }
    if (choice == previousPracticeIndex_) {
      choice = static_cast<uint8_t>(
          (previousPracticeIndex_ + 1U) % learnedCount_);
    }
    return choice;
  }

  void presentPracticeQuestion() {
    currentItemIndex_ = choosePracticeItem();
    previousPracticeIndex_ = currentItemIndex_;
    wrongAttempts_ = 0;
    remediationRequired_ = false;

    playRandom(QUIZ_INTROS);
    char path[80];
    makeCharacterPath(currentItemIndex_, path, sizeof(path));
    audio_.play(path);

    Serial.println(ITEMS[currentItemIndex_].kana);
    state_ = AppState::PRACTICE_WAIT_INPUT;
  }

  void replayCurrentPracticeQuestion() {
    if (state_ != AppState::PRACTICE_WAIT_INPUT) {
      return;
    }

    // Replay only the current character prompt. Do not choose a new random
    // item and do not reset wrongAttempts_ or remediationRequired_.
    char path[80];
    makeCharacterPath(currentItemIndex_, path, sizeof(path));
    audio_.play(path);
    Serial.println(ITEMS[currentItemIndex_].kana);
  }

  void startPractice() {
    returningChoiceResolved_ = true;
    if (learnedCount_ == 0) {
      audio_.play(AudioPath::NO_LEARNED_ITEMS);
      Serial.println(F("まだ学習した文字がありません。"));
      state_ = AppState::HOME;
      return;
    }

    audio_.play(AudioPath::PRACTICE_INTRO);
    presentPracticeQuestion();
  }

  void handlePracticeWrong() {
    ++wrongAttempts_;
    Serial.println(F("もう一度"));

    if (remediationRequired_) {
      playWrongResponse(WRONG_1_RESPONSES, arrayCount(WRONG_1_RESPONSES));
      return;
    }

    if (wrongAttempts_ == 1U) {
      playWrongResponse(WRONG_1_RESPONSES, arrayCount(WRONG_1_RESPONSES));
      return;
    }

    if (wrongAttempts_ == 2U) {
      playWrongResponse(WRONG_2_RESPONSES, arrayCount(WRONG_2_RESPONSES));
      char hintPath[80];
      makeHintPath(currentItemIndex_, hintPath, sizeof(hintPath));
      audio_.play(hintPath);
      return;
    }

    playWrongResponse(WRONG_3_RESPONSES, arrayCount(WRONG_3_RESPONSES));
    char lessonPath[80];
    makeLessonItemPath(currentItemIndex_, lessonPath, sizeof(lessonPath));
    audio_.play(lessonPath);
    remediationRequired_ = true;
  }

  void handleSubmission(uint8_t mask) {
    if (state_ == AppState::LESSON_SELECT) {
      confirmSelectedLesson();
      return;
    }

    if (state_ != AppState::STUDY_WAIT_INPUT &&
        state_ != AppState::PRACTICE_WAIT_INPUT) {
      return;
    }

    if (findItemByMask(mask) < 0) {
      audio_.play(AudioPath::UNKNOWN_PATTERN);
      Serial.println(F("登録されていません。"));
      return;
    }

    const bool correct = mask == ITEMS[currentItemIndex_].mask;
    if (state_ == AppState::STUDY_WAIT_INPUT) {
      if (correct) {
        handleStudyCorrect();
      } else {
        Serial.println(F("もう一度"));
        playWrongResponse(WRONG_1_RESPONSES, arrayCount(WRONG_1_RESPONSES));
      }
      return;
    }

    if (!correct) {
      handlePracticeWrong();
      return;
    }

    playCorrectResponse();
    Serial.println(F("正解"));
    presentPracticeQuestion();
  }

  void handleStudyButton(ButtonEvent event) {
    if (event == ButtonEvent::NONE) {
      return;
    }

    if (state_ == AppState::RETURNING_USER_CHOICE) {
      beginLessonSelection();
      return;
    }

    if (state_ == AppState::LESSON_DECISION) {
      if (event == ButtonEvent::LONG_PRESS) {
        repeatLesson(completedLesson_);
      } else {
        continueAfterLesson();
      }
      return;
    }

    if (state_ == AppState::STUDY_WAIT_INPUT) {
      if (event == ButtonEvent::LONG_PRESS) {
        repeatLesson(ITEMS[currentItemIndex_].lesson);
      } else {
        replayCurrentStudyItem();
      }
      return;
    }

    if (state_ == AppState::LESSON_SELECT) {
      selectNextLesson();
      return;
    }

    if (event == ButtonEvent::LONG_PRESS) {
      const uint8_t lesson = nextStudyIndex_ >= ITEM_COUNT
                                 ? LESSON_COUNT
                                 : ITEMS[nextStudyIndex_].lesson;
      repeatLesson(lesson);
    } else {
      beginLessonSelection();
    }
  }

  void handlePracticeButton(ButtonEvent event) {
    if (event == ButtonEvent::NONE) {
      return;
    }

    if (state_ == AppState::LESSON_SELECT) {
      selectPreviousLesson();
      return;
    }

    if (state_ == AppState::PRACTICE_WAIT_INPUT) {
      replayCurrentPracticeQuestion();
      return;
    }

    if (state_ == AppState::RETURNING_USER_CHOICE &&
        !returningChoiceResolved_) {
      reviewPreviousLesson();
      return;
    }

    startPractice();
  }

  void handleReplayCommand(ButtonEvent event) {
    if (event == ButtonEvent::NONE) {
      return;
    }

    if (state_ == AppState::STUDY_WAIT_INPUT) {
      replayCurrentStudyItem();
    } else if (state_ == AppState::PRACTICE_WAIT_INPUT) {
      replayCurrentPracticeQuestion();
    }
  }

  void updateButtons() {
    const ButtonEvent practiceEvent = practiceButton_.update();
    const ButtonEvent studyEvent = studyButton_.update();
    const ButtonEvent sendEvent = sendButton_.update();

    // Capture the chord while Send is held. The old code sampled only after
    // Send was released, so releasing the dot buttons at nearly the same time
    // could turn a correct answer into mask 0 and repeat the same character.
    if (sendSubmittedWhileHeld_) {
      if (!sendButton_.isPressed()) {
        sendSubmittedWhileHeld_ = false;
      }
    } else if (sendButton_.isPressed()) {
      if (!sendCaptureActive_) {
        sendCaptureActive_ = true;
        capturedBrailleMask_ = 0;
      }
      const uint8_t sampledMask = readBrailleMask();
      if (sampledMask != 0U) {
        capturedBrailleMask_ = sampledMask;
      }
    }

    if (studyEvent != ButtonEvent::NONE) {
      handleStudyButton(studyEvent);
      return;
    }

    if (practiceEvent != ButtonEvent::NONE) {
      handlePracticeButton(practiceEvent);
      return;
    }

    if (sendEvent != ButtonEvent::NONE) {
      const uint8_t submittedMask =
          sendCaptureActive_ ? capturedBrailleMask_ : readBrailleMask();
      sendCaptureActive_ = false;
      capturedBrailleMask_ = 0;
      if (sendEvent == ButtonEvent::LONG_PRESS) {
        sendSubmittedWhileHeld_ = true;
      }
      handleSubmission(submittedMask);
    }
  }

  void updateLessonDecisionTimeout() {
    if (state_ != AppState::LESSON_DECISION) {
      return;
    }

    // A hold that has already started must be allowed to reach two seconds.
    if (digitalRead(Pins::STUDY) == LOW) {
      return;
    }

    if ((millis() - lessonDecisionStartedAt_) >=
        Timing::LESSON_DECISION_AUTO_CONTINUE_MS) {
      continueAfterLesson();
    }
  }

  void printStatus() const {
    Serial.print(F("状態："));
    Serial.println(stateName(state_));
    Serial.print(F("学習済み："));
    Serial.print(learnedCount_);
    Serial.print('/');
    Serial.println(ITEM_COUNT);
    Serial.print(F("次の位置："));
    if (nextStudyIndex_ >= ITEM_COUNT) {
      Serial.println(F("完了"));
    } else {
      Serial.println(nextStudyIndex_ + 1U);
    }
    Serial.print(F("音声："));
    Serial.println(audio_.ready() ? F("準備完了") : F("使用不可"));
    if (state_ == AppState::STUDY_WAIT_INPUT ||
        state_ == AppState::PRACTICE_WAIT_INPUT) {
      Serial.print(F("現在の文字："));
      Serial.println(ITEMS[currentItemIndex_].kana);
    } else if (state_ == AppState::LESSON_SELECT) {
      Serial.print(F("選択中のレッスン："));
      Serial.println(selectedLesson_);
    }
  }

  void printHelp() const {
    Serial.println(F("study：学習ボタンを短く押す"));
    Serial.println(F("hold_study：学習ボタンを二秒押す"));
    Serial.println(F("practice：練習ボタンを押す"));
    Serial.println(F("replay：現在の音声をもう一度再生する"));
    Serial.println(F("submit：現在の点字ボタンを送信する"));
    Serial.println(F("mask 数字：0から63の入力を送信する"));
    Serial.println(F("status：状態を表示する"));
    Serial.println(F("reset_progress：進み具合を消去する"));
  }

  void resetProgress() {
    learnedCount_ = 0;
    nextStudyIndex_ = 0;
    currentItemIndex_ = 0;
    selectedLesson_ = 1;
    previousPracticeIndex_ = 0xFF;
    wrongAttempts_ = 0;
    remediationRequired_ = false;
    sendCaptureActive_ = false;
    sendSubmittedWhileHeld_ = false;
    capturedBrailleMask_ = 0;
    returningChoiceResolved_ = true;
    state_ = AppState::HOME;
    if (preferencesReady_) {
      preferences_.clear();
    }
    Serial.println(F("進み具合を消去しました。"));
    printHomeMenu();
  }

  void handleSerialCommand(String command) {
    command.trim();
    command.toLowerCase();
    if (command.length() == 0) {
      return;
    }

    if (command == "study") {
      handleStudyButton(ButtonEvent::SHORT_PRESS);
    } else if (command == "hold_study") {
      handleStudyButton(ButtonEvent::LONG_PRESS);
    } else if (command == "practice") {
      handlePracticeButton(ButtonEvent::SHORT_PRESS);
    } else if (command == "replay") {
      handleReplayCommand(ButtonEvent::SHORT_PRESS);
    } else if (command == "submit") {
      handleSubmission(readBrailleMask());
    } else if (command == "status") {
      printStatus();
    } else if (command == "help") {
      printHelp();
    } else if (command == "tone_ok") {
      audio_.playCorrectFallback();
    } else if (command == "tone_fail") {
      audio_.playWrongFallback();
    } else if (command == "reset_progress") {
      resetProgress();
    } else if (command.startsWith("mask ")) {
      const String valueText = command.substring(5);
      char* endPointer = nullptr;
      const long value = strtol(valueText.c_str(), &endPointer, 10);
      if (endPointer == valueText.c_str() || *endPointer != '\0' ||
          value < 0 || value > 63) {
        Serial.println(F("0から63までの数字を入力してください。"));
      } else {
        handleSubmission(static_cast<uint8_t>(value));
      }
    } else {
      Serial.println(F("コマンドが分かりません。"));
      printHelp();
    }
  }

  void updateSerial() {
    while (Serial.available() > 0) {
      const char input = static_cast<char>(Serial.read());
      if (input == '\r') {
        continue;
      }
      if (input == '\n') {
        handleSerialCommand(serialCommand_);
        serialCommand_ = "";
      } else if (serialCommand_.length() < 80) {
        serialCommand_ += input;
      } else {
        serialCommand_ = "";
        Serial.println(F("コマンドが長すぎます。"));
      }
    }
  }

  AudioPlayer audio_;
  Preferences preferences_;
  DebouncedButton practiceButton_;
  DebouncedButton studyButton_;
  DebouncedButton sendButton_;

  AppState state_ = AppState::HOME;
  uint8_t learnedCount_ = 0;
  uint8_t nextStudyIndex_ = 0;
  uint8_t currentItemIndex_ = 0;
  uint8_t selectedLesson_ = 1;
  uint8_t previousPracticeIndex_ = 0xFF;
  uint8_t completedLesson_ = 1;
  uint8_t wrongAttempts_ = 0;
  bool remediationRequired_ = false;
  bool preferencesReady_ = false;
  bool returningChoiceResolved_ = false;
  bool sendCaptureActive_ = false;
  bool sendSubmittedWhileHeld_ = false;
  uint8_t capturedBrailleMask_ = 0;
  uint32_t lessonDecisionStartedAt_ = 0;
  String serialCommand_;
};

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h> 
#include <AS5600.h>

// ==================== 핀 설정 ====================
#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1
#define PHOTO_PIN 5       // [추가] 포토 인터럽터 핀 (기존 4번은 AS5600 충돌 가능성으로 5번 권장)

#define swing 10          // 측정할 왕복 횟수

// ==================== 객체 생성 ====================
LiquidCrystal_I2C lcd(0x27, 16, 2);
AS5600 as5600(&Wire); 

// ==================== 전역 변수 ====================
int mode = 0;
int measureSourceMode = 5; // [추가] 측정 모드가 어디였는지 기억 (3=Photo, 5=Hall)

// 모드 0에서 입력할 초기 스윙 시작 각도
float SetAngle = 0.0;

// 관성모멘트 계산 물리량
float mass_kg = 0.0;     
float distance_m = 0.0;  
float time_s = 0.0;      // 측정된 주기(T)

// ==================== 클래스 정의 ====================
class DebouncedButton 
{
    private:
        uint8_t _pin;
        unsigned long _debounceDelay;
        unsigned long _lastDebounceTime;
        int _buttonState;
        int _lastButtonState;

        uint8_t _buzzerPin;
        int _buzzerTone;
        int _buzzerDuration;

    public:
        DebouncedButton(uint8_t pin) 
        {
            _pin = pin;
            _debounceDelay = 50;
            _lastDebounceTime = 0;
            _buttonState = HIGH;
            _lastButtonState = HIGH;
            _buzzerPin = 0;
            _buzzerTone = 0;
            _buzzerDuration = 0;
        }

    void begin() 
    {
        pinMode(_pin, INPUT_PULLUP);
    }

    void attachBuzzer(uint8_t pin, int tone, int duration) 
    {
        _buzzerPin = pin;
        _buzzerTone = tone;
        _buzzerDuration = duration;
        pinMode(_buzzerPin, OUTPUT);
    }

    bool checkPressed() 
    {
        bool pressedEvent = false;
        int reading = digitalRead(_pin);

        if (reading != _lastButtonState)
        {
            _lastDebounceTime = millis();
        }

        if ((millis() - _lastDebounceTime) > _debounceDelay) 
        {
            if (reading != _buttonState) 
            {
                _buttonState = reading;
                
                if (_buttonState == LOW) 
                {
                    pressedEvent = true; 
                    if (_buzzerPin != 0) 
                    {
                        tone(_buzzerPin, _buzzerTone, _buzzerDuration);
                    }
                }
            }
        }
        _lastButtonState = reading;
        return pressedEvent;
    }
};

DebouncedButton* buttonA;
DebouncedButton* buttonB;

// ==================== 함수 정의 ====================

// 모드 변경 시 LCD 초기화 함수
void updateLcdDisplay() 
{
  lcd.clear(); 
  lcd.setCursor(0, 0); 

  switch (mode) 
  {
    case 0: lcd.print("== Set angle ==");   break;
    case 1: lcd.print("== which mode? =="); break;
    case 2: lcd.print("== Hall Cal. ==");   break;
    case 3: lcd.print("== Photo Mode ==");  break; // [변경] TBD -> Photo Mode
    case 4: lcd.print("== Set M & D ==");   break;
    case 5: lcd.print("== Hall Mode ==");   break; 
    case 6: lcd.print("== Inertia Cal =="); break;
  }
}

// Mode 4용 화면 업데이트 헬퍼
void mode4_updateLcd(const char* title, int digits[6], int pos, bool isDone) 
{
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(title);

  char displayString[10];
  sprintf(displayString, "%d%d%d%d.%d%d", 
          digits[5], digits[4], digits[3], digits[2], digits[1], digits[0]);
  lcd.setCursor(0, 1);
  lcd.print(displayString);
  lcd.print("        "); 
}

float mode4_getFinalValue(int digits[6]) {
  float value = 0.0;
  value += (float)digits[0] * 0.01;
  value += (float)digits[1] * 0.1;
  value += (float)digits[2] * 1.0;
  value += (float)digits[3] * 10.0;
  value += (float)digits[4] * 100.0;
  value += (float)digits[5] * 1000.0;
  return value;
}

void mode4_resetInput(int digits[6], int& pos, int& lastDigit, bool& isDone) {
  for (int i = 0; i < 6; i++) {
    digits[i] = 0;
  }
  pos = 0; 
  lastDigit = -1;
  isDone = false;
}

// ==================== SETUP ====================
void setup() 
{
  // 핀 설정
  pinMode(PHOTO_PIN, INPUT_PULLUP); // [추가] 포토 인터럽터

  buttonA = new DebouncedButton(BUTTON_A_PIN);
  buttonB = new DebouncedButton(BUTTON_B_PIN);

  buttonA->attachBuzzer(BUZZER_PIN, 1500, 100);
  buttonB->attachBuzzer(BUZZER_PIN, 800, 100);

  buttonA->begin();
  buttonB->begin();

  Wire.begin();
  lcd.init();
  lcd.backlight();

  Serial.begin(9600);
  Serial.println("===== Serial initialization =====");

  Serial.println("Checking for AS5600...");
  as5600.begin(4); // AS5600 direction pin
  if (as5600.isConnected() == false) { 
      Serial.println("AS5600 not detected! Check wiring.");
      lcd.clear();
      lcd.print("AS5600 ERROR");
      while (1) delay(10); 
  }
  Serial.println("AS5600 found!");

  updateLcdDisplay();
}

// ==================== LOOP ====================
void loop() 
{
  bool A_pressed = buttonA->checkPressed();
  bool B_pressed = buttonB->checkPressed();

  // ----- Mode 1 (Selection) 변수 -----
  static int mode1_selection = 0; // 0: Hall, 1: Photo

  // ----- Mode 2 (Hall Calib) 변수 -----
  static int mode2_step = 0; 
  static float angleOffset = 0.0; 
  static unsigned long stableStartTime = 0; 
  static float lastStableValue = -1.0;
  
  // ----- Mode 3 (Photo Measure) 변수 [신규] -----
  static int mode3_step = 0;
  static unsigned long mode3_stableStartTime = 0;
  static unsigned long mode3_timerStart = 0;
  static unsigned long mode3_prevTime = 0;
  static int mode3_countdown = 3;
  
  static int mode3_hitCount = 0;
  static int mode3_lastPhotoState = HIGH;
  static unsigned long mode3_lastHitMs = 0;

  // ----- Mode 4 (Input M & D) 변수 -----
  static int mode4_editingStep = 0; 
  static int digits[6] = {0, 0, 0, 0, 0, 0}; 
  static int currentDigitPosition = 0; 
  static int lastMappedDigit = -1;     
  static bool isInputDone = false;      

  // ----- Mode 5 (Hall Measure) 변수 -----
  static int mode5_step = 0; 
  static unsigned long mode5_stableStartTime = 0; 
  static unsigned long mode5_timerStart = 0;      
  static unsigned long mode5_prevTime = 0;        
  static int mode5_countdown = 3;                 
  
  static int mode5_swingCount = 0;         
  static int mode5_prevIntAngle = 0;  
  static bool mode5_readyForPeak = false; 

  switch (mode) 
  {
    // ======================================================
    // Mode 0: Set Target Angle (초기 각도 설정)
    // ======================================================
    case 0: 
    {
      float potVal = analogRead(POT_PIN);
      float angle = map(potVal, 0, 1020, 0, 300) / 10.0;

      lcd.setCursor(1, 1);
      lcd.print(" Angle: ");
      lcd.print(angle, 1);

      if (A_pressed) 
      {
        mode++; // -> Mode 1
        SetAngle = angle;
        updateLcdDisplay();
      }
      break;
    }

    // ======================================================
    // Mode 1: Select Sensor (Hall vs Photo)
    // ======================================================
    case 1:
    {
      lcd.setCursor(0, 1); 
      if (mode1_selection == 0) lcd.print("[Hall] |  Photo ");
      else                      lcd.print(" Hall  | [Photo] ");

      // B버튼: 선택 변경
      if (B_pressed) 
      {
         mode1_selection = !mode1_selection;
      }

      // A버튼: 확정
      if (A_pressed) 
      {
        if (mode1_selection == 0) // Hall 선택
        {
            mode = 2; // Hall Calibration으로 이동
            measureSourceMode = 5; // 나중을 위해 기록
            mode2_step = 0;
        } 
        else // Photo 선택
        {
            mode = 2; // Photo도 각도 확인 위해 Hall Calib 먼저 수행
            measureSourceMode = 3; // 나중을 위해 기록
            mode2_step = 0; 
        }
        updateLcdDisplay();
      }
      break;
    }
    
    // ======================================================
    // Mode 2: Hall Sensor Calibration (0점 잡기)
    // ======================================================
    case 2: 
    {
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;

      if (mode2_step == 0) // 대기
      {
        lcd.setCursor(0, 1);
        lcd.print("Raw: "); lcd.print(currentAngle, 2); lcd.print("   ");

        if (A_pressed) {
          mode2_step = 1; 
          stableStartTime = millis(); 
          lastStableValue = currentAngle; 
          lcd.setCursor(0, 1); lcd.print("Waiting static...");
        }
        if (B_pressed) {
          mode = 1; updateLcdDisplay();
        }
      }
      else if (mode2_step == 1) // 안정화 감지
      {
        int currentIntAngle = (int)currentAngle;
        int lastIntAngle = (int)lastStableValue;

        if (currentIntAngle != lastIntAngle) {
          stableStartTime = millis(); 
          lastStableValue = currentAngle; 
        }

        if (millis() - stableStartTime > 2000) {
          angleOffset = currentAngle; 
          mode2_step = 2; 
          lcd.setCursor(0, 1); lcd.print("Calibrated!     ");
          delay(1000); 
        }
      }
      else if (mode2_step == 2) // 확인
      {
        float calibratedAngle = currentAngle - angleOffset;
        if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
        else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
        
        lcd.setCursor(0, 1);
        lcd.print("Angle: ");
        if (calibratedAngle > 0) lcd.print("+"); 
        lcd.print(calibratedAngle, 2); lcd.print(" deg   ");

        if (A_pressed) {
          // 측정 모드에 따라 분기
          if (measureSourceMode == 5) mode = 5; // Hall Measure
          else                        mode = 3; // Photo Measure
          updateLcdDisplay();
        }
        if (B_pressed) {
          mode = 1; updateLcdDisplay();
        }
      }
      break; 
    }

    // ======================================================
    // Mode 3: Photo Interrupter Measure (신규 구현)
    // ======================================================
    case 3: 
    {       
      // --- 각도 계산 (AS5600 사용 - 초기 위치 잡기용) ---
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;
      float calibratedAngle = currentAngle - angleOffset;
      if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
      else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
      float absAngle = fabs(calibratedAngle);

      // --- Step 0: 각도 맞추기 (Mode 5와 동일 로직) ---
      if (mode3_step == 0)
      {
         float diff = fabs(SetAngle - absAngle);
         
         lcd.setCursor(0, 1);
         lcd.print("Go to: "); lcd.print(SetAngle, 1);
         lcd.print(" ("); lcd.print(absAngle, 1); lcd.print(") ");

         if (diff < 3.0) 
         {
            if (mode3_stableStartTime == 0) mode3_stableStartTime = millis();
            if (millis() - mode3_stableStartTime > 1500) 
            {
               mode3_step = 1; // 카운트다운 진입
               mode3_countdown = 3;
               mode3_prevTime = millis();
               tone(BUZZER_PIN, 1500, 100); 
               lcd.clear();
            }
         }
         else { mode3_stableStartTime = 0; }
      }

      // --- Step 1: 카운트다운 ---
      else if (mode3_step == 1)
      {
         lcd.setCursor(0, 0); lcd.print("== Ready? ==");
         lcd.setCursor(0, 1); lcd.print("Start in "); lcd.print(mode3_countdown); lcd.print("...    ");

         if (millis() - mode3_prevTime >= 1000) 
         {
            mode3_countdown--;
            mode3_prevTime = millis();
            if (mode3_countdown > 0) {
               tone(BUZZER_PIN, 800, 100); 
            } else {
               tone(BUZZER_PIN, 2500, 600); 
               // 측정 시작 초기화
               mode3_step = 2; 
               mode3_hitCount = 0;
               mode3_timerStart = 0; 
               mode3_lastPhotoState = digitalRead(PHOTO_PIN); // 초기 상태 읽기
               
               lcd.clear();
               lcd.setCursor(0, 0); lcd.print("Release!");
               lcd.setCursor(0, 1); lcd.print("Waiting sensor..");
            }
         }
      }

      // --- Step 2: 측정 (포토 인터럽터) ---
      else if (mode3_step == 2)
      {
          int photoState = digitalRead(PHOTO_PIN);
          unsigned long now = millis();

          // 엣지 감지: 막힘 (Beam Broken, 보통 LOW)
          // PHOTO_PIN이 평소 HIGH(Pullup)이고 막히면 LOW라고 가정 (일반적 BUP-50S 등)
          // photo_final.cpp 로직 참조: HIGH -> LOW 일 때 blockActive
          if (mode3_lastPhotoState == HIGH && photoState == LOW) 
          {
             // 디바운싱: 너무 빠른 연속 감지 방지 (예: 50ms)
             if (now - mode3_lastHitMs > 50) 
             {
                 mode3_hitCount++;
                 mode3_lastHitMs = now;
                 tone(BUZZER_PIN, 1200, 50); // 짧은 삑

                 // === 로직 설명 ===
                 // Hit 1: 첫 번째 통과 (최저점). 타이머 시작.
                 // Hit 2: 반대편 갔다가 돌아옴 (1/2 주기) -> 진자 1회 통과
                 // Hit 3: 다시 원래 방향 (1 주기 완료) -> 진자 2회 통과
                 // ...
                 // 우리는 'swing'번의 왕복을 측정하고 싶음.
                 // 1회 왕복 = 2번의 통과 (왔다 갔다)
                 // 따라서 swing * 2 번의 추가 통과가 필요함.
                 // 시작점(Hit 1)을 0초로 잡으면, Hit (1 + swing*2) 에서 멈춰야 함.
                 
                 if (mode3_hitCount == 1) 
                 {
                     mode3_timerStart = now;
                     lcd.clear();
                     lcd.setCursor(0, 0);
                     lcd.print("Measuring...");
                 }
                 else 
                 {
                     // 진행 상황 표시 (왕복 횟수)
                     int currentRoundTrip = (mode3_hitCount - 1) / 2;
                     lcd.setCursor(0, 1);
                     lcd.print("Count: "); lcd.print(currentRoundTrip); 
                     lcd.print("/"); lcd.print(swing);

                     // 종료 조건: 목표 왕복 횟수 채움
                     if (mode3_hitCount >= (1 + swing * 2)) 
                     {
                         mode3_step = 3;
                         tone(BUZZER_PIN, 2000, 800);
                     }
                 }
             }
          }
          mode3_lastPhotoState = photoState;
      }

      // --- Step 3: 결과 표시 ---
      else if (mode3_step == 3)
      {
          // 계산
          if (mode3_timerStart > 0) {
              unsigned long endTime = mode3_lastHitMs; // 마지막 히트 시각
              float totalTimeSec = (endTime - mode3_timerStart) / 1000.0;
              time_s = totalTimeSec / (float)swing; // 평균 주기

              lcd.clear();
              lcd.setCursor(0, 0); lcd.print("T_avg: "); lcd.print(time_s, 3); lcd.print("s");
              lcd.setCursor(0, 1); lcd.print("Tot: "); lcd.print(totalTimeSec, 2); lcd.print("s");

              mode3_timerStart = 0; // 플래그 리셋하여 계산 1회만 수행
          }

          // A버튼: 다음(입력 모드)
          if (A_pressed) {
              mode = 4; // 입력 모드로
              mode4_editingStep = 0;
              updateLcdDisplay();
          }
          // B버튼: 재측정
          if (B_pressed) {
              mode3_step = 0;
              mode3_stableStartTime = 0;
              updateLcdDisplay();
          }
      }

      // 측정 도중(Step 0~2) B버튼 누르면 설정 취소
      if (mode3_step < 3 && B_pressed) {
          mode = 2; // 다시 Calib 화면이나 모드 선택으로
          updateLcdDisplay();
      }
      break;
    }

    // ======================================================
    // Mode 4: Input Variables (Mass & Distance)
    // ======================================================
    case 4:
    {
      const char* title;
      if (mode4_editingStep == 0) title = "Set Mass (kg)";
      else                        title = "Set Dist. (m)";

      if (isInputDone) 
      {
        if (mode4_editingStep == 0) { // Mass 완료
          mass_kg = mode4_getFinalValue(digits); 
          mode4_editingStep = 1; 
          mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone); 
          mode4_updateLcd("Set Dist. (m)", digits, currentDigitPosition, isInputDone); 
        } 
        else { // Distance 완료 -> 결과 계산 모드(6)로
          distance_m = mode4_getFinalValue(digits); 
          
          mode4_editingStep = 0; 
          isInputDone = false;     
          mode = 6; 
          updateLcdDisplay(); 
        }
      }
      else 
      {
        int potVal = analogRead(POT_PIN);
        int newDigit = map(potVal, 0, 1023, 0, 10);
        newDigit = constrain(newDigit, 0, 9);

        if (newDigit != lastMappedDigit) {
            digits[currentDigitPosition] = newDigit;
            lastMappedDigit = newDigit;
            mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); 
        }

        if (A_pressed) {
            currentDigitPosition++;
            lastMappedDigit = -1; 
            if (currentDigitPosition >= 6) isInputDone = true; 
            else mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); 
        }

        if (B_pressed) {
            if (currentDigitPosition > 0) {
                currentDigitPosition--;
                lastMappedDigit = -1; 
                mode4_updateLcd(title, digits, currentDigitPosition, isInputDone);
            }
            else {
                // [탈출 로직 수정] 이전 단계나 측정 모드로 복귀
                if (mode4_editingStep == 1) { // Distance -> Mass
                    mode4_editingStep = 0; 
                    mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
                    mode4_updateLcd("Set Mass (kg)", digits, currentDigitPosition, isInputDone);
                }
                else { // Mass -> 측정 모드(Photo or Hall)로 복귀
                    mode = measureSourceMode; // 3 or 5
                    
                    // 각 모드 초기화
                    if(mode == 5) { mode5_step = 3; } // 결과 화면으로? 아니면 0으로
                    else          { mode3_step = 3; } 
                    
                    // 바로 재측정 대기 상태로 보내고 싶다면:
                    if(mode == 5) { mode5_step = 0; mode5_stableStartTime=0; }
                    else          { mode3_step = 0; mode3_stableStartTime=0; }

                    mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
                    updateLcdDisplay();
                }
            }
        }
      }
      break;
    }
   
    // ======================================================
    // Mode 5: Hall Sensor Measure (기존 유지)
    // ======================================================
    case 5: 
    {
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;
      float calibratedAngle = currentAngle - angleOffset;
      if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
      else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
      float absAngle = fabs(calibratedAngle); 

      static float filteredAbsAngle = 0.0;
      filteredAbsAngle = (filteredAbsAngle * 0.2) + (absAngle * 0.8);
      int currentIntAngle = (int)filteredAbsAngle;

      // --- Step 0 ---
      if (mode5_step == 0)
      {
         if (mode5_stableStartTime == 0) { filteredAbsAngle = absAngle; }
         float diff = fabs(SetAngle - filteredAbsAngle);
         lcd.setCursor(0, 1);
         lcd.print("Go to: "); lcd.print(SetAngle, 1);
         lcd.print(" ("); lcd.print(filteredAbsAngle, 1); lcd.print(")  "); 

         if (diff < 3.0) {
            if (mode5_stableStartTime == 0) mode5_stableStartTime = millis();
            if (millis() - mode5_stableStartTime > 1500) {
               mode5_step = 1; 
               mode5_countdown = 3;
               mode5_prevTime = millis();
               tone(BUZZER_PIN, 1500, 100); 
               lcd.clear();
            }
         }
         else { mode5_stableStartTime = 0; }
      }
      // --- Step 1 ---
      else if (mode5_step == 1)
      {
         lcd.setCursor(0, 0); lcd.print("== Ready? ==");
         lcd.setCursor(0, 1); lcd.print("Start in "); lcd.print(mode5_countdown); lcd.print("...    ");
         if (millis() - mode5_prevTime >= 1000) {
            mode5_countdown--;
            mode5_prevTime = millis();
            if (mode5_countdown > 0) tone(BUZZER_PIN, 800, 100); 
            else {
               tone(BUZZER_PIN, 2500, 600); 
               mode5_step = 2; 
               mode5_swingCount = 0; 
               mode5_prevIntAngle = currentIntAngle; 
               mode5_readyForPeak = false; 
               mode5_timerStart = 0; 
               lcd.clear(); lcd.setCursor(0, 0); lcd.print("Warm-up..."); 
            }
         }
      }
      // --- Step 2 ---
      else if (mode5_step == 2)
      {
         unsigned long currentMillis = millis();
         if (currentIntAngle < 6) mode5_readyForPeak = true;
         
         if (mode5_readyForPeak && (mode5_prevIntAngle > currentIntAngle) && (mode5_prevIntAngle > 12))
         {
             mode5_swingCount++; 
             mode5_readyForPeak = false; 
             tone(BUZZER_PIN, 1000, 50); 

             if (mode5_swingCount == 2) {
                 mode5_timerStart = millis(); 
                 lcd.clear(); lcd.setCursor(0, 0); lcd.print("Start! 0/"); lcd.print(swing);
                 tone(BUZZER_PIN, 1500, 200); 
             }
             else if (mode5_swingCount > 2) {
                 int validPeaks = mode5_swingCount - 2;
                 if (validPeaks % 2 == 0) {
                     int validRoundTrip = validPeaks / 2;
                     lcd.setCursor(0, 0);
                     lcd.print("Count: "); lcd.print(validRoundTrip); lcd.print("/"); lcd.print(swing);
                     if (validRoundTrip >= swing) {
                         mode5_step = 3; 
                         tone(BUZZER_PIN, 2000, 1000); 
                         lcd.clear();
                     }
                 }
             }
         }
         mode5_prevIntAngle = currentIntAngle;

         if (mode5_timerStart > 0) {
             float totalElapsed = (currentMillis - mode5_timerStart) / 1000.0;
             lcd.setCursor(0, 1); lcd.print("Time: "); lcd.print(totalElapsed, 2); lcd.print(" s   ");
         }
      }
      // --- Step 3 ---
      else if (mode5_step == 3)
      {
          unsigned long endTime = millis(); 
          if (mode5_timerStart > 0) { 
              float totalTimeSec = (endTime - mode5_timerStart) / 1000.0;
              time_s = totalTimeSec / swing; 

              lcd.setCursor(0, 0); lcd.print("Avg T: "); lcd.print(time_s, 3); lcd.print(" s");
              lcd.setCursor(0, 1); lcd.print("Tot: "); lcd.print(totalTimeSec, 2); lcd.print("s");
              mode5_timerStart = 0; 
          }
          if (A_pressed) {
              mode = 4; // 입력 모드로
              mode4_editingStep = 0;
              updateLcdDisplay();
          }
          if (B_pressed) {
            mode5_step = 0; mode5_stableStartTime = 0; updateLcdDisplay(); 
          }
      }
      // Step 0~2에서 B 누르면
      if (mode5_step < 3 && B_pressed) {
          mode = 2; updateLcdDisplay();
      }
      break;
    }

    // ======================================================
    // Mode 6: Calculation Result
    // ======================================================
    case 6:
    {
      float T = time_s;
      float M = mass_kg;
      float D = distance_m;
      float g = 9.80665;
      float PI_VAL = 3.14159265;
      float I_value = 0.0;
      
      if (M > 0 && D > 0) I_value = (T * T * M * g * D) / (4 * PI_VAL * PI_VAL);

      lcd.setCursor(0, 0);
      lcd.print("I="); lcd.print(I_value, 5); lcd.print(" kgm^2 ");

      lcd.setCursor(0, 1);
      lcd.print("A:Reset B:Back");

      if (A_pressed) {
        mode = 0; // 완전 초기화
        updateLcdDisplay();
      }
      if (B_pressed) {
        // [수정] 측정했던 모드로 돌아가기
        mode = measureSourceMode; 
        
        // 결과 화면 상태로 복귀
        if(mode == 5) mode5_step = 3;
        else          mode3_step = 3;
        
        updateLcdDisplay();
      }
      break;
    }
  }
  
  // 측정 중(Mode 3 step 2)일 때는 루프 지연을 최소화하여 센서 미스를 방지
  if (mode == 3 && mode3_step == 2) {
      // No delay
  } else {
      delay(10);
  }
}
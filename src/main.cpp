#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h> 
#include <AS5600.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1

LiquidCrystal_I2C lcd(0x27, 16, 2);
AS5600 as5600(&Wire); // [수정] Wire 객체 전달 (RobTillaart 라이브러리 호환)

int mode = 0;
float SetAngle = 0.0;

// 관성모멘트 계산 물리량
float mass_kg = 0.0;     // 예시 질량 값 (kg)
float distance_m = 0.0;  // 예시 거리 값 (m)

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

void updateLcdDisplay() 
{
  lcd.clear(); 
  lcd.setCursor(0, 0); 

  switch (mode) 
  {
    case 0: lcd.print("== Set angle ==");   break;
    case 1: lcd.print("== which mode? =="); break;
    case 2: lcd.print("== Hall mode ==");   break;
    case 3: lcd.print("== Measure ==");     break;
    case 4: lcd.print("== Set M & D ==");   break;
    case 5: lcd.print("== Measure Hall =="); break; // 이름 변경
  }
  
}

void mode4_updateLcd(const char* title, int digits[6], int pos, bool isDone) 
{
  // 1행 제목 표시
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(title);

  // 2행 "XXXX.XX" 형식으로 숫자 문자열 생성
  char displayString[10];
  sprintf(displayString, "%d%d%d%d.%d%d", 
          digits[5], digits[4], digits[3], digits[2], digits[1], digits[0]);
  lcd.setCursor(0, 1);
  lcd.print(displayString);
  lcd.print("        "); // 잔상 제거
  
  if (!isDone) {
      int cursorMap[6] = {6, 5, 3, 2, 1, 0}; 
      // 숫자 위치에 맞게 커서 조정 (소수점 고려)
      // digits[0]=0.01(pos6), digits[1]=0.1(pos5), dot(pos4), digits[2]=1(pos3)...
      // 위 sprintf 포맷: %d%d%d%d.%d%d -> [5][4][3][2].[1][0]
      // 인덱스: 0123 4 56
      // 커서맵 수정: 
      int realCursorMap[6] = {6, 5, 3, 2, 1, 0}; 
      
      lcd.setCursor(realCursorMap[pos], 1);
      // lcd.print("^"); // (선택) 커서 대신 깜빡임 효과를 원하면 이 부분 수정
  }
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
  pos = 0; // 0.01 자리부터
  lastDigit = -1;
  isDone = false;
}

void setup() 
{
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

  // 시리얼 모니터에서 as5600 연결 확인 (디버깅)
  Serial.println("Checking for AS5600...");
  
  // [수정] RobTillaart 라이브러리 연결 확인 로직
  as5600.begin(4); // 4 = Wire 모드
  if (as5600.isConnected() == false) { 
      Serial.println("AS5600 not detected! Check wiring.");
      lcd.clear();
      lcd.print("AS5600 ERROR");
      while (1) delay(10); // 오류 시 여기서 정지
  }
  Serial.println("AS5600 found!");

  updateLcdDisplay();
  Serial.print("Current mode: ");
  Serial.println(mode);
}

void loop() 
{
  bool A_pressed = buttonA->checkPressed();
  bool B_pressed = buttonB->checkPressed();

  static int mode1_selection = 0;

  static int mode2_step = 0; 
  static float angleOffset = 0.0; 
  static unsigned long stableStartTime = 0; 
  static float lastStableValue = -1.0;
  
  static int mode4_editingStep = 0; // 0=Mass 입력 중, 1=Distance 입력 중
  static int digits[6] = {0, 0, 0, 0, 0, 0}; // [0.01 ... 1000]
  static int currentDigitPosition = 0; // 현재 수정 중인 자릿수
  static int lastMappedDigit = -1;     // 가변저항 값 변경 감지용
  static bool isInputDone = false;      // 6자리 입력 완료 플래그

  // --- [수정] Mode 5 스윙 측정용 변수 ---
  static int mode5_step = 0; // 0=각도대기, 1=카운트다운, 2=타이머작동
  static unsigned long mode5_stableStartTime = 0; // 안정화 타이머
  static unsigned long mode5_timerStart = 0;      // 스톱워치 시작 시간
  static unsigned long mode5_prevTime = 0;        // 카운트다운 1초 체크용
  static int mode5_countdown = 3;                 // 3초 카운트다운
  
  // [알고리즘 변수]
  static int mode5_swingCount = 0;         // 스윙 횟수
  static float mode5_prevAbsAngle = 0.0;   // 이전 루프의 절대 각도
  static bool mode5_readyForPeak = false;  // 0점 통과 여부 (디바운싱용)
  static unsigned long mode5_lastPeakTime = 0; // 이전 정점 시간 (주기 계산용)
  static float mode5_lastPeriod = 0.0;     // 측정된 주기(초)
  // ------------------------------------
  
  switch (mode) 
  {
    case 0: // setting angle mode
    {
      float potVal = analogRead(POT_PIN);
      float angle = map(potVal, 0, 1020, 0, 300) / 10.0;

      lcd.setCursor(1, 1);
      lcd.print(" Angle: ");
      
      lcd.setCursor(9, 1);
      lcd.print(angle, 1);

      if (A_pressed) 
      {
        mode++;
        Serial.print("Button A clicked, currne mode: ");
        Serial.println(mode);
        SetAngle = angle;
        Serial.print("setting angle: ");
        Serial.println(SetAngle);
        updateLcdDisplay();
        mode2_step = 0; 

      }
      break;
    }

    case 1:
    {
      lcd.setCursor(0, 1); // 2번째 줄로 이동
      
      if (mode1_selection == 0) // 'Hall'이 선택된 경우
      {
        lcd.print("[Hall] |  Photo ");
      } 
      else // 'Photo'가 선택된 경우
      {
        lcd.print(" Hall  | [Photo] ");
      }

      // --- 2. 버튼 A 로직 (Hall 선택/확인) ---
      if (B_pressed) 
      {
        if (mode1_selection == 0) // 'Hall'이 이미 선택된 상태에서 A를 누름 (확인)
        {
            mode = 2; // 'Hall' 모드(mode 2)로 이동
            Serial.print("Button A confirmed, current mode: ");
            Serial.println(mode);
            updateLcdDisplay();
            mode2_step = 0; // 새로 추가: Mode 2의 상태를 초기화
        } 
        else // 'Photo'가 선택된 상태에서 A를 누름 (선택 변경)
        {
            mode1_selection = 0; // 'Hall'을 선택
        }
      }

      // --- 3. 버튼 B 로직 (Photo 선택/확인) ---
      if (A_pressed) 
      {
        if (mode1_selection == 1) // 'Photo'가 이미 선택된 상태에서 B를 누름 (확인)
        {
            mode = 3; // 'Photo' 모드(mode 3)로 이동
            Serial.print("Button B confirmed, current mode: ");
            Serial.println(mode);
            updateLcdDisplay();
        }
        else // 'Hall'이 선택된 상태에서 B를 누름 (선택 변경)
        {
            mode1_selection = 1; // 'Photo'를 선택
        }
      }
      break;
    }
    
    case 2: // hall sensor calibration mode
    {
      // 1. AS5600에서 원시 각도 읽기 (항상)
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;

      // --- Step 0: (진입) 캘리브레이션 시작 대기 ---
      if (mode2_step == 0)
      {
        lcd.setCursor(0, 1);
        lcd.print("Raw: ");
        lcd.print(currentAngle, 2);
        lcd.print(" deg  ");

        // (1. 워크플로우) A 버튼 클릭 시 캘리브레이션 대기 시작
        if (A_pressed) 
        {
          mode2_step = 1; // 캘리브레이션 대기 단계로 이동
          stableStartTime = millis(); // 타이머 리셋
          lastStableValue = currentAngle; // 현재 값으로 안정화 기준 설정
          lcd.setCursor(0, 1);
          lcd.print("Waiting static...");
          Serial.println("Calibration step 1: Waiting for static...");
        }

        if (B_pressed) 
        {
          mode--; // mode 1 (선택 화면)으로 이동
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
        }
      }
      
      // --- Step 1: (대기) 2초간 정수 각도 정지 감지 ---
      else if (mode2_step == 1)
      {
        // '정수' 각도만 비교
        int currentIntAngle = (int)currentAngle;
        int lastIntAngle = (int)lastStableValue;

        // 정수 각도가 변경되면 타이머 리셋
        if (currentIntAngle != lastIntAngle) 
        {
          stableStartTime = millis(); // 타이머 리셋
          lastStableValue = currentAngle; // 기준 값 갱신
        }

        // 2초간 정지했는지 확인
        if (millis() - stableStartTime > 2000) 
        {
          angleOffset = currentAngle; // [영점 설정]
          mode2_step = 2; // (3) 캘리브레이션 완료 단계로 이동
          Serial.print("Zero point set at: "); Serial.println(angleOffset);
          lcd.setCursor(0, 1);
          lcd.print("Calibrated!     ");
          delay(1000); 
        }
      }

      // --- Step 2: (완료) 보정된 각도(+/-) 출력 ---
      else if (mode2_step == 2)
      {
        float calibratedAngle = currentAngle - angleOffset;

        // Wrap-around 처리
        if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
        else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
        
        lcd.setCursor(0, 1);
        lcd.print("Angle: ");
        if (calibratedAngle > 0) lcd.print("+"); 
        lcd.print(calibratedAngle, 2);
        lcd.print(" deg  ");

        if (A_pressed) 
        {
          mode=4; // [수정] 바로 Measure(3)이 아니라 설정(4)로 가도록 흐름 변경
          Serial.print("Button A clicked, currne mode: ");
          Serial.println(mode);
          updateLcdDisplay();
        }

        if (B_pressed) 
        {
          mode--; 
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
        }
      }
      
      break; 
    }

    case 3: // (구) Measure Mode - Photo? 여기는 포토모드로 남겨둠? 
    {       // 사용자가 Mode 5를 원했으므로 Mode 3은 일단 비워두거나 기존 로직 유지
            // 흐름상 Mode 1 -> Hall -> Mode 2 -> Mode 4 -> Mode 5
            // Mode 1 -> Photo -> Mode 3 
      lcd.setCursor(0, 1);
      lcd.print("Photo Mode (TBD)");
      
      if (A_pressed || B_pressed) {
          mode = 1; // 뒤로가기
          updateLcdDisplay();
      }
      break;
    }

    case 4:
    {
      const char* title;
      if (mode4_editingStep == 0) {
        title = "Set Mass (kg)";
      } else {
        title = "Set Dist. (m)";
      }

      if (isInputDone) {
        if (mode4_editingStep == 0) { // Mass 완료
          mass_kg = mode4_getFinalValue(digits); 
          Serial.print("Mass set to: "); Serial.println(mass_kg);
          
          mode4_editingStep = 1; // Distance로 이동
          mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone); 
          mode4_updateLcd("Set Dist. (m)", digits, currentDigitPosition, isInputDone); 
        } 
        else { // Distance 완료
          distance_m = mode4_getFinalValue(digits); 
          Serial.print("Distance set to: "); Serial.println(distance_m);
          
          mode4_editingStep = 0; 
          isInputDone = false;     
          mode = 5; // [수정] 설정 완료 후 Mode 5(측정)로 이동
          updateLcdDisplay(); 
          
          // Mode 5 진입 초기화
          mode5_step = 0; 
        }
      }
      else {
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
            if (currentDigitPosition >= 6) { 
                isInputDone = true; 
            } else {
                mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); 
            }
        }

        if (B_pressed) {
            mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
            if (mode4_editingStep == 0) { 
                mode = 2; // [수정] 뒤로가기 시 캘리브레이션 완료 화면으로
                mode2_step = 2; 
                updateLcdDisplay();
            } else { 
                mode4_editingStep = 0; 
                mode4_updateLcd("Set Mass (kg)", digits, currentDigitPosition, isInputDone);
            }
        }
      }
      break;
    } 
   
    case 5: // [수정] 홀센서 스윙 측정 및 타이머
    {
      // 1. 현재 각도 및 보정
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;
      float calibratedAngle = currentAngle - angleOffset;
      
      if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
      else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
      
      float absAngle = fabs(calibratedAngle); // 절댓값 각도

      // --- Step 0: 목표 각도 맞춤 및 안정화 ---
      if (mode5_step == 0)
      {
         float diff = fabs(SetAngle - absAngle);

         lcd.setCursor(0, 1);
         lcd.print("Go to: ");
         lcd.print(SetAngle, 1);
         lcd.print(" (");
         lcd.print(absAngle, 1); 
         lcd.print(")  "); 

         // 3도 이내 오차로 진입
         if (diff < 3.0) 
         {
            if (mode5_stableStartTime == 0) mode5_stableStartTime = millis();
            
            // 1.5초 유지 시 카운트다운 시작
            if (millis() - mode5_stableStartTime > 1500) 
            {
               mode5_step = 1; 
               mode5_countdown = 3;
               mode5_prevTime = millis();
               tone(BUZZER_PIN, 1500, 100); 
               lcd.clear();
            }
         }
         else 
         {
            mode5_stableStartTime = 0; 
         }
      }

      // --- Step 1: 카운트다운 ---
      else if (mode5_step == 1)
      {
         lcd.setCursor(0, 0);
         lcd.print("== Ready? ==");
         lcd.setCursor(0, 1);
         lcd.print("Start in ");
         lcd.print(mode5_countdown);
         lcd.print("...    ");

         if (millis() - mode5_prevTime >= 1000) 
         {
            mode5_countdown--;
            mode5_prevTime = millis();
            
            if (mode5_countdown > 0) {
               tone(BUZZER_PIN, 800, 100); 
            } else {
               tone(BUZZER_PIN, 2500, 600); // 시작음
               
               // [측정 시작 초기화]
               mode5_step = 2; 
               mode5_timerStart = millis(); 
               mode5_swingCount = 0;
               mode5_prevAbsAngle = absAngle;
               mode5_readyForPeak = false; 
               mode5_lastPeakTime = millis(); 
               mode5_lastPeriod = 0.0;

               lcd.clear();
               lcd.setCursor(0, 0);
               lcd.print("Run: 0     0.00s");
            }
         }
      }

      // --- Step 2: 스윙 측정 (알고리즘 적용) ---
      else if (mode5_step == 2)
      {
         unsigned long currentMillis = millis();
         
         // [알고리즘 1] 0점 통과 확인 (Rearm)
         // 각도가 1도 미만으로 떨어져야만 다음 정점 감지 허용 (디바운싱)
         if (absAngle < 2.0) {
            mode5_readyForPeak = true;
         }

         // [알고리즘 2] 정점(Peak) 감지
         // 조건: (0점 통과함) AND (각도 꺾임: 이전 > 현재) AND (노이즈 아님: > 10도)
         if (mode5_readyForPeak && (mode5_prevAbsAngle > absAngle) && (mode5_prevAbsAngle > 5.0))
         {
             // 스윙 감지!
             mode5_swingCount++;
             
             // 주기 측정
             unsigned long interval = currentMillis - mode5_lastPeakTime;
             mode5_lastPeriod = interval / 1000.0; 
             mode5_lastPeakTime = currentMillis;   
             
             mode5_readyForPeak = false; // [잠금] 다시 0점으로 갈 때까지 대기
             
             tone(BUZZER_PIN, 1000, 50); // 피드백 비프
         }
         
         // 현재 각도 저장
         mode5_prevAbsAngle = absAngle;

         // [LCD 출력]
         lcd.setCursor(0, 0);
         lcd.print("Cnt:");
         lcd.print(mode5_swingCount);
         lcd.print("  T:");
         lcd.print(mode5_lastPeriod, 2);
         lcd.print("s ");

         float totalElapsed = (currentMillis - mode5_timerStart) / 1000.0;
         lcd.setCursor(0, 1);
         lcd.print("Total: ");
         lcd.print(totalElapsed, 2);
         lcd.print(" s   ");
      }
      
      // B버튼: 재설정(Mode 4)으로 복귀
      if (B_pressed) 
      {
        mode = 4;
        mode4_editingStep = 0;
        updateLcdDisplay();
      }
      break;
    }
  }
  
  delay(10); 
}
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h> 
#include <AS5600.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1
#define swing 5

LiquidCrystal_I2C lcd(0x27, 16, 2);
AS5600 as5600(&Wire); 

// 모드 지정 전역변수
int mode = 0;

// 모드 0에서 입력할 초기 스윙 시작 각도
float SetAngle = 0.0;

// 관성모멘트 계산 물리량
float mass_kg = 0.0;     
float distance_m = 0.0;  
float time_s = 0.0;

// 디바운싱 및 부저 알림음 발생 객체 클래스
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

// 모드별 상단 문자 lcd 출력 함수, 모드 변경시 매번 실행
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
    case 5: lcd.print("== Measure Hall =="); break; 
    case 6: lcd.print("== Inertia Cal =="); break;
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

  Serial.println("Checking for AS5600...");
  
  as5600.begin(4); 
  if (as5600.isConnected() == false) { 
      Serial.println("AS5600 not detected! Check wiring.");
      lcd.clear();
      lcd.print("AS5600 ERROR");
      while (1) delay(10); 
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
  
  static int mode4_editingStep = 0; 
  static int digits[6] = {0, 0, 0, 0, 0, 0}; 
  static int currentDigitPosition = 0; 
  static int lastMappedDigit = -1;     
  static bool isInputDone = false;      

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
      lcd.setCursor(0, 1); 
      
      if (mode1_selection == 0) 
      {
        lcd.print("[Hall] |  Photo ");
      } 
      else 
      {
        lcd.print(" Hall  | [Photo] ");
      }

      if (B_pressed) 
      {
        if (mode1_selection == 0) 
        {
            mode = 2; 
            Serial.print("Button A confirmed, current mode: ");
            Serial.println(mode);
            updateLcdDisplay();
            mode2_step = 0; 
        } 
        else 
        {
            mode1_selection = 0; 
        }
      }

      if (A_pressed) 
      {
        if (mode1_selection == 1) 
        {
            mode = 3; 
            Serial.print("Button B confirmed, current mode: ");
            Serial.println(mode);
            updateLcdDisplay();
        }
        else 
        {
            mode1_selection = 1; 
        }
      }
      break;
    }
    
    case 2: // hall sensor calibration mode
    {
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;

      if (mode2_step == 0)
      {
        lcd.setCursor(0, 1);
        lcd.print("Raw: ");
        lcd.print(currentAngle, 2);
        lcd.print(" deg  ");

        if (A_pressed) 
        {
          mode2_step = 1; 
          stableStartTime = millis(); 
          lastStableValue = currentAngle; 
          lcd.setCursor(0, 1);
          lcd.print("Waiting static...");
          Serial.println("Calibration step 1: Waiting for static...");
        }

        if (B_pressed) 
        {
          mode--; 
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
        }
      }
      
      else if (mode2_step == 1)
      {
        int currentIntAngle = (int)currentAngle;
        int lastIntAngle = (int)lastStableValue;

        if (currentIntAngle != lastIntAngle) 
        {
          stableStartTime = millis(); 
          lastStableValue = currentAngle; 
        }

        if (millis() - stableStartTime > 2000) 
        {
          angleOffset = currentAngle; 
          mode2_step = 2; 
          Serial.print("Zero point set at: "); Serial.println(angleOffset);
          lcd.setCursor(0, 1);
          lcd.print("Calibrated!     ");
          delay(1000); 
        }
      }

      else if (mode2_step == 2)
      {
        float calibratedAngle = currentAngle - angleOffset;

        if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
        else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
        
        lcd.setCursor(0, 1);
        lcd.print("Angle: ");
        if (calibratedAngle > 0) lcd.print("+"); 
        lcd.print(calibratedAngle, 2);
        lcd.print(" deg  ");

        if (A_pressed) 
        {
          mode=5; 
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

    case 3: 
    {       
      lcd.setCursor(0, 1);
      lcd.print("Photo Mode (TBD)");
      
      if (A_pressed || B_pressed) {
          mode = 1; 
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
          mode = 6; // [흐름 수정] 설정 완료 후 Mode 6(최종 계산)로 이동
          mode5_step = 0; // Mode 5 초기화
          mode5_stableStartTime = 0;
          updateLcdDisplay(); 
        }
      }
      else {
        int potVal = analogRead(POT_PIN);
        int newDigit = map(potVal, 0, 1023, 0, 10);
        newDigit = constrain(newDigit, 0, 9);

        // 값 변경 감지 및 LCD 업데이트
        if (newDigit != lastMappedDigit) {
            digits[currentDigitPosition] = newDigit;
            lastMappedDigit = newDigit;
            mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); 
        }

        // A버튼: 다음 자릿수 이동 (Next / Confirm)
        if (A_pressed) {
            currentDigitPosition++;
            lastMappedDigit = -1; // 다음 자릿수에서 팟 값 강제 리프레시

            if (currentDigitPosition >= 6) { 
                isInputDone = true; // 6자리 모두 입력하면 완료 플래그
            } else {
                mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); // 커서 이동
            }
        }

        // B버튼: 이전 자릿수 이동 (Backspace) 또는 모드 탈출
        if (B_pressed) {
            
            if (currentDigitPosition > 0) 
            {
                // [수정] 자릿수가 남아있으면 뒤로 가기 (Backspace)
                currentDigitPosition--;
                lastMappedDigit = -1; // 이전 자릿수 값 다시 읽기 위해 리셋
                mode4_updateLcd(title, digits, currentDigitPosition, isInputDone);
            }
            else 
            {
                // [수정] 가장 낮은 자릿수(0)에서 B를 누르면 -> 이전 단계/모드로 탈출
                
                // 현재 Distance 입력 중이었다면 -> Mass 입력으로 돌아가기
                if (mode4_editingStep == 1) {
                    mode4_editingStep = 0; 
                    // Mass 값은 유지되거나 리셋 (여기선 리셋)
                    mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
                    mode4_updateLcd("Set Mass (kg)", digits, currentDigitPosition, isInputDone);
                }
                // 현재 Mass 입력 중이었다면 -> Mode 5(스윙측정)로 탈출
                else { 
                    mode = 5; 
                    mode2_step = 2; // 캘리브레이션 완료 상태
                    // 입력기 초기화
                    mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
                    updateLcdDisplay();
                }
            }
        }
      }
      break;
    }
   
    case 5: 
    {
      int rawAngle = as5600.readAngle();
      float currentAngle = (float)rawAngle * 360.0 / 4096.0;
      float calibratedAngle = currentAngle - angleOffset;
      
      if (calibratedAngle > 180.0) calibratedAngle -= 360.0;
      else if (calibratedAngle < -180.0) calibratedAngle += 360.0;
      
      float absAngle = fabs(calibratedAngle); 

      // 노이즈 필터링 (이동 평균)
      static float filteredAbsAngle = 0.0;
      filteredAbsAngle = (filteredAbsAngle * 0.2) + (absAngle * 0.8);
      
      // 정수형 변환
      int currentIntAngle = (int)filteredAbsAngle;

      // --- Step 0: 각도 대기 ---
      if (mode5_step == 0)
      {
         if (mode5_stableStartTime == 0) { filteredAbsAngle = absAngle; }

         float diff = fabs(SetAngle - filteredAbsAngle);

         lcd.setCursor(0, 1);
         lcd.print("Go to: ");
         lcd.print(SetAngle, 1);
         lcd.print(" (");
         lcd.print(filteredAbsAngle, 1); 
         lcd.print(")  "); 

         if (diff < 3.0) 
         {
            if (mode5_stableStartTime == 0) mode5_stableStartTime = millis();
            if (millis() - mode5_stableStartTime > 1500) 
            {
               mode5_step = 1; 
               mode5_countdown = 3;
               mode5_prevTime = millis();
               tone(BUZZER_PIN, 1500, 100); 
               lcd.clear();
            }
         }
         else { mode5_stableStartTime = 0; }
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
               tone(BUZZER_PIN, 2500, 600); 
               
               // [측정 시작 초기화]
               mode5_step = 2; 
               mode5_swingCount = 0; 
               mode5_prevIntAngle = currentIntAngle; 
               mode5_readyForPeak = false; 
               
               // 타이머 시작은 아직 하지 않음 (첫 왕복 무시)
               mode5_timerStart = 0; 
               
               lcd.clear();
               lcd.setCursor(0, 0);
               lcd.print("Warm-up..."); // 첫 스윙은 준비운동
            }
         }
      }

      // --- Step 2: 스윙 측정 (첫 1회 무시 후 10회 측정) ---
      else if (mode5_step == 2)
      {
         unsigned long currentMillis = millis();
         
         // 0점 통과 인식 (6도 미만)
         if (currentIntAngle < 6) {
            mode5_readyForPeak = true;
         }

         // 정점(Peak) 감지 (12도 초과)
         if (mode5_readyForPeak && (mode5_prevIntAngle > currentIntAngle) && (mode5_prevIntAngle > 12))
         {
             mode5_swingCount++; // 피크(편도) 카운트 증가
             mode5_readyForPeak = false; // 잠금
             tone(BUZZER_PIN, 1000, 50); 

             // [수정] 유효 왕복 횟수 계산 로직
             // Swing 1, 2 (첫 왕복) -> 무시
             // Swing 2가 되는 순간이 '진짜 시작점' (0회 완료, 타이머 시작)
             // Swing 4 (두 번째 왕복) -> 유효 1회 완료
             
             if (mode5_swingCount == 2) 
             {
                 // 첫 번째 왕복(준비운동) 끝! 진짜 측정 시작
                 mode5_timerStart = millis(); 
                 lcd.clear();
                 lcd.setCursor(0, 0);
                 lcd.print("Start! 0/");
                 lcd.print(swing);
                 tone(BUZZER_PIN, 1500, 200); // 시작 알림
             }
             else if (mode5_swingCount > 2)
             {
                 // 여기서부터 유효 데이터
                 // (현재 카운트 - 2) / 2 = 유효 왕복 횟수
                 // 예: 카운트 4 -> (4-2)/2 = 1회 완료
                 // 예: 카운트 22 -> (22-2)/2 = 10회 완료
                 
                 int validPeaks = mode5_swingCount - 2;
                 
                 // 짝수 번째 피크일 때만(왕복 완료 시점) 갱신
                 if (validPeaks % 2 == 0) 
                 {
                     int validRoundTrip = validPeaks / 2;
                     
                     lcd.setCursor(0, 0);
                     lcd.print("Count: ");
                     lcd.print(validRoundTrip);
                     lcd.print("/");
                     lcd.print(swing);

                     // swing회 유효 왕복 완료 시 종료
                     if (validRoundTrip >= swing) 
                     {
                         mode5_step = 3; 
                         tone(BUZZER_PIN, 2000, 1000); 
                         lcd.clear();
                     }
                 }
             }
         }
         
         mode5_prevIntAngle = currentIntAngle;

         // 타이머 표시 (시작된 경우에만)
         if (mode5_timerStart > 0) {
             float totalElapsed = (currentMillis - mode5_timerStart) / 1000.0;
             lcd.setCursor(0, 1);
             lcd.print("Time: ");
             lcd.print(totalElapsed, 2);
             lcd.print(" s   ");
         }
      }

      // --- Step 3: 측정 완료 및 결과 표시 ---
      else if (mode5_step == 3)
      {
          unsigned long endTime = millis(); 
          if (mode5_timerStart > 0) { // 한 번만 계산
              float totalTimeSec = (endTime - mode5_timerStart) / 1000.0;
              float avg = swing;
              float avgPeriod = totalTimeSec / avg; // 10회 평균

              time_s = avgPeriod;
              Serial.print("측정된 주기: ");
              Serial.print(time_s);
              Serial.println("");
              
              lcd.setCursor(0, 0);
              lcd.print("Avg T: ");
              lcd.print(avgPeriod, 3); 
              lcd.print(" s");

              lcd.setCursor(0, 1);
              lcd.print("Tot: ");
              lcd.print(totalTimeSec, 2);
              lcd.print("s");
              
              mode5_timerStart = 0; // 플래그 리셋
          }
      }

      // --- 버튼 로직 ---
      
      // B버튼 (리셋 / 설정이동)
      if (B_pressed) 
      {
        if (mode5_step == 3) { // 결과 화면에서 -> 재측정
            mode5_step = 0;
            mode5_stableStartTime = 0;
            updateLcdDisplay(); 
        } 
        else { // 측정 중 -> 설정(Mode 4)으로
            mode = 2;
            mode4_editingStep = 0; 
            mode5_step = 0;
            mode5_stableStartTime = 0;
            updateLcdDisplay();
        }
      }

      // A버튼 (다음 모드 (mode 4)로 이동)
      if (A_pressed) 
      {
        mode = 4;
        mode5_step = 0;
        mode5_stableStartTime = 0;
        updateLcdDisplay();
      }
      
      break;
    }

    case 6:
    {
      // 1. 관성모멘트 계산
      float T = time_s;
      float M = mass_kg;
      float D = distance_m;
      float g = 9.80665;
      float PI_VAL = 3.14159265;

      float I_value = 0.0;
      if (M > 0 && D > 0) { // 0으로 나누기 방지 및 유효성 체크
          I_value = (T * T * M * g * D) / (4 * PI_VAL * PI_VAL);
      }

      // 2. LCD 출력
      lcd.setCursor(0, 0);
      lcd.print("I=");
      lcd.print(I_value, 5); // 공간 확보를 위해 소수점 5자리
      lcd.print(" kgm^2 ");

      lcd.setCursor(0, 1);
      lcd.print("A:Reset B:Back");

      // 3. 버튼 로직
      if (A_pressed) 
      {
        mode = 0; // 처음부터 다시 시작
        Serial.println("Reset to Mode 0");
        updateLcdDisplay();
      }

      if (B_pressed) 
      {
        mode = 5; // 재측정 하러 가기
        // (주의: mode 5로 가면 step 3 상태일 수 있음. A/B 눌러서 리셋 필요)
        updateLcdDisplay();
      }
      break;
    }
  }
  
  delay(10); 
}
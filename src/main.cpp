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
AS5600 as5600;

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
    case 0:
      lcd.print("== Set angle ==");   
      break;
    case 1:
      lcd.print("== which mode? ==");
      break;
    case 2:
      lcd.print("== Hall mode ==");
      break;
    case 3:
      lcd.print("== mode 3 ==");
      break;
    case 4:
      lcd.print("== Set M & D ==");
      break;

  }
  
}

void mode4_updateLcd(const char* title, int digits[6], int pos, bool isDone) {
  // 1행 제목 표시
  lcd.clear(); // [참고] 버튼 클릭 시 깜박임. 나중에 clear() 대신 setCursor()와 공백으로 최적화 가능
  lcd.setCursor(0, 0); 
  lcd.print(title);

  // 2행 "XXXX.XX" 형식으로 숫자 문자열 생성
  char displayString[10];
  sprintf(displayString, "%d%d%d%d.%d%d", 
          digits[5], digits[4], digits[3], digits[2], digits[1], digits[0]);
  lcd.setCursor(0, 1);
  lcd.print(displayString);
  lcd.print("        "); // 잔상 제거

  // 3. 커서(^) 위치 계산 (입력 중에만)
  if (!isDone) {
    int cursorMap[6] = {6, 5, 3, 2, 1, 0}; // XXXX.XX 문자열에서의 인덱스
    char cursorString[17] = "                "; 
    int lcd_pos = cursorMap[pos];
    cursorString[lcd_pos] = '^';
    lcd.setCursor(0, 1);
    lcd.print(cursorString); // 덮어쓰기
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

  Serial.begin(115200);
  Serial.println("===== Serial initialization =====");

  // as5600 연결 확인
  Serial.println("Checking for AS5600...");
  if (as5600.readAngle() == -1) { // Seeed 라이브러리는 I2C 오류 시 -1 반환
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
  
  switch (mode) 
  {
    case 0: // setting angle mode
    // angle 자료형 추후 변경 논의 필요 (float -> int) 
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
        mode2_step = 0; // 새로 추가: Mode 2의 상태를 초기화

      }
      break;
    }

    case 1:
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
    
    case 2: // hall sensor calibration mode
    {
      // --- [수정] 캘리브레이션 워크플로우 구현 ---

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

        // '뒤로 가기' 버튼 (이 단계에선 B만 활성화)
        if (B_pressed) 
        {
          mode--; // mode 1 (선택 화면)으로 이동
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
          // mode2_step = 0; // (어차피 0임)
        }
      }
      
      // --- Step 1: (대기) 2초간 정수 각도 정지 감지 ---
      else if (mode2_step == 1)
      {
        // (2. 워크플로우) 이 단계에서는 A/B 버튼 입력을 의도적으로 무시
        
        // '정수' 각도만 비교
        int currentIntAngle = (int)currentAngle;
        int lastIntAngle = (int)lastStableValue;

        // 정수 각도가 변경되면 타이머 리셋
        if (currentIntAngle != lastIntAngle) 
        {
          stableStartTime = millis(); // 타이머 리셋
          lastStableValue = currentAngle; // 기준 값 갱신
          // lcd.print("Waiting static..."); // 메시지 유지
        }

        // 2초간 정지했는지 확인
        if (millis() - stableStartTime > 2000) 
        {
          angleOffset = currentAngle; // [영점 설정] 현재 각도를 0점(offset)으로 저장
          mode2_step = 2; // (3) 캘리브레이션 완료 단계로 이동
          Serial.print("Zero point set at: "); Serial.println(angleOffset);
          lcd.setCursor(0, 1);
          lcd.print("Calibrated!     ");
          delay(1000); // 사용자가 메시지를 볼 수 있도록 잠시 대기
        }
      }

      // --- Step 2: (완료) 보정된 각도(+/-) 출력 ---
      else if (mode2_step == 2)
      {
        // 1. 보정된 각도 계산 (currentAngle - angleOffset)
        float calibratedAngle = currentAngle - angleOffset;

        // 2. Wrap-around 처리 (각도를 -180 ~ +180 범위로)
        if (calibratedAngle > 180.0) {
          calibratedAngle -= 360.0; // 예: (350 - 0) = 350  -> -10
        } else if (calibratedAngle < -180.0) {
          calibratedAngle += 360.0; // 예: (10 - 350) = -340 -> +20
        }
        
        // (센서의 시계방향(+) 설정은 하드웨어 방향에 따라 결정됩니다)
        
        // 3. LCD에 튜닝된 각도 출력
        lcd.setCursor(0, 1);
        lcd.print("Angle: ");
        if (calibratedAngle > 0) lcd.print("+"); // + 부호 표시
        lcd.print(calibratedAngle, 2);
        lcd.print(" deg  ");

        // (3. 워크플로우) 캘리브레이션 완료 후 버튼 활성화
        if (A_pressed) 
        {
          mode++;
          Serial.print("Button A clicked, currne mode: ");
          Serial.println(mode);
          updateLcdDisplay();
          // mode2_step = 0; // (어차피 case 0에서 리셋됨)
        }

        if (B_pressed) 
        {
          mode--; // mode 1 (선택 화면)으로 이동
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
          // mode2_step = 0; // (어차피 case 1에서 리셋됨)
        }
      }
      
      break; // case 2의 break
    }
      
    case 3:
      lcd.setCursor(0, 1);
      lcd.print("from case - 3");
      if (A_pressed) 
      {
        mode++;
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
      break;
    
    case 4:
    {
      // 0. 현재 편집 중인 변수의 제목 결정
      const char* title;
      if (mode4_editingStep == 0) {
        title = "Set Mass (kg)";
      } else {
        title = "Set Dist. (m)";
      }

      // 1. 입력 완료 상태 (isInputDone == true)
      if (isInputDone) {
        // (Mass 또는 Dist 입력이 방금 완료됨)
        if (mode4_editingStep == 0) { // Mass 입력이 끝났다면
          mass_kg = mode4_getFinalValue(digits); // 1. 값 저장
          Serial.print("Mass set to: "); Serial.println(mass_kg);
          
          mode4_editingStep = 1; // 2. Distance 입력으로 이동
          // 3. 입력기 리셋 (digits, pos, lastDigit, isDone 변수 리셋)
          mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone); 
          // 4. 새 화면 표시 (헬퍼 함수 호출)
          mode4_updateLcd("Set Dist. (m)", digits, currentDigitPosition, isInputDone); 
        } 
        else { // Distance 입력이 끝났다면
          distance_m = mode4_getFinalValue(digits); // 1. 값 저장
          Serial.print("Distance set to: "); Serial.println(distance_m);
          
          mode4_editingStep = 0; // 2. (다음 진입을 위해) Mass 입력으로 리셋
          isInputDone = false;     // 3. (다음 진입을 위해) 플래그 리셋
          mode = 0; // 4. 완료! mode 0으로 이동
          updateLcdDisplay(); // 5. mode 0 화면 표시
        }
      }
      
      // 2. 입력 진행 상태 (isInputDone == false)
      else {
        // 2a. 가변저항 값 읽기 (POT_PIN은 A1으로 정의되어 있음)
        int potVal = analogRead(POT_PIN);
        int newDigit = map(potVal, 0, 1023, 0, 10);
        newDigit = constrain(newDigit, 0, 9);

        // 2b. '변화가 있을 때만' 값 업데이트
        if (newDigit != lastMappedDigit) {
            digits[currentDigitPosition] = newDigit;
            lastMappedDigit = newDigit;
            mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); // LCD 새로고침
        }

        // 2c. 버튼 A (다음 자릿수)
        if (A_pressed) {
            currentDigitPosition++;
            lastMappedDigit = -1; // 다음 자릿수에서 팟 값 강제 리프레시

            if (currentDigitPosition >= 6) { // 6자리 입력 완료
                isInputDone = true; 
                // (loop가 다음 턴에 if(isInputDone) 블록을 실행할 것임)
            } else {
                mode4_updateLcd(title, digits, currentDigitPosition, isInputDone); // 커서 이동
            }
        }

        // 2d. 버튼 B (취소/뒤로가기)
        if (B_pressed) {
            mode4_resetInput(digits, currentDigitPosition, lastMappedDigit, isInputDone);
            
            if (mode4_editingStep == 0) { // Mass 입력 중 B를 누르면
                mode = 3; // mode 3으로 '뒤로가기'
                updateLcdDisplay();
            } else { // Distance 입력 중 B를 누르면
                mode4_editingStep = 0; // Mass 입력으로 '뒤로가기'
                mode4_updateLcd("Set Mass (kg)", digits, currentDigitPosition, isInputDone);
            }
        }
      }
      break;
    } 
  }
  
  delay(10); 
}
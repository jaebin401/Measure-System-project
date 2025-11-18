#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h> 

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1
#define HALL_SIM_PIN A0 

LiquidCrystal_I2C lcd(0x27, 16, 2);

int mode = 0;
float SetAngle = 0.0;

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

  }
  
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
  updateLcdDisplay();

  Serial.begin(9600);
  Serial.println("===== Serial initialization =====");
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
  static unsigned long lastBuzzerTime = 0; 
  static unsigned long countdownStartTime = 0; 
  static int countdown = 3; 

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
        Serial.print("setting angle:");
        Serial.print(SetAngle);
        updateLcdDisplay();
        mode2_step = 0; // 새로 추가: Mode 2의 상태를 초기화

      }
      break;
    }

    case 1:
      // --- 1. LCD 디스플레이 로직 (선택 반전 효과) ---
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
        // --- (1) Step 0: 캘리브레이션 (영점 잡기) ---
        if (mode2_step == 0)
        {
          // 1. 센서 값 읽기 (0~360도)
          int potVal = analogRead(HALL_SIM_PIN);
          float currentAngle = (float)map(potVal, 0, 1023, 0, 36000) / 100.0;

          // 2. 센서가 움직였는지 확인 (소수점 1자리 기준)
          if (fabs(currentAngle - lastStableValue) > 0.1) 
          {
            stableStartTime = millis(); // 타이머 리셋
            lastStableValue = currentAngle;
            lcd.setCursor(0, 1);
            lcd.print("Waiting static...");
          }

          // 3. 2초간 정지했는지 확인
          if (millis() - stableStartTime > 2000) 
          {
            lcd.setCursor(0, 1);
            lcd.print("Set Zero? (A=OK)");
            
            if (A_pressed) 
            {
              angleOffset = currentAngle; // 현재 각도를 0점(offset)으로 저장
              mode2_step = 1; // (2) 각도 세팅 단계로 이동
              lastBuzzerTime = millis(); // 부저 타이머 초기화
              Serial.print("Zero point set at: "); Serial.println(angleOffset);
              lcd.setCursor(0, 1);
              lcd.print("                "); // 이전 글씨 지우기
            }
          }
        }
        // --- (2) Step 1: 목표 각도 세팅 (부저 피드백) ---
        else if (mode2_step == 1)
        {
          // 1. 보정된 각도 읽기
          int potVal = analogRead(HALL_SIM_PIN);
          float currentAngle = (float)map(potVal, 0, 1023, 0, 36000) / 100.0;
          float calibratedAngle = currentAngle - angleOffset;
          
          // 2. 목표 각도(SetAngle)와의 차이 계산
          float gap = fabs(SetAngle - calibratedAngle);
          const float READY_THRESHOLD = 1.0; // 1.0도 허용 오차

          if (gap < READY_THRESHOLD) 
          {
            // 3a. 목표 도달
            lcd.setCursor(0, 1);
            lcd.print("Ready! (A=Start)");
            noTone(BUZZER_PIN); // 부저 정지

            if (A_pressed) {
                mode2_step = 2; // (3) 카운트다운 단계로 이동
                countdown = 3; // 카운트다운 3초로 리셋
                countdownStartTime = millis();
                tone(BUZZER_PIN, 1500, 100); // 시작음
            }
          } 
          else 
          {
            // 3b. 목표 근접 중 (후진 경고등)
            lcd.setCursor(0, 1);
            lcd.print("Set Angle...");
            lcd.print(calibratedAngle, 1);
            
            // 갭(30도~1도)을 부저 간격(1초~0.1초)으로 매핑
            int beep_interval = (int)map(gap, 1, 30, 100, 1000);
            beep_interval = constrain(beep_interval, 100, 1000); // 100~1000ms 범위 제한

            if (millis() - lastBuzzerTime > beep_interval) {
                tone(BUZZER_PIN, 1000, 50); // 짧은 삑 소리
                lastBuzzerTime = millis();
            }
          }
        }
        // --- (3) Step 2: 3초 카운트다운 ---
        else if (mode2_step == 2)
        {
          if (millis() - countdownStartTime >= 1000) {
              countdown--;
              countdownStartTime = millis(); // 타이머 리셋
              
              if (countdown > 0) {
                  tone(BUZZER_PIN, 800, 100); // 카운트다운 '삑'
              } else {
                  tone(BUZZER_PIN, 2000, 500); // '시작!' 긴 삑
              }
          }

          if (countdown == 0) {
              lcd.clear(); // 화면 완전히 지우기
              lcd.setCursor(0, 0);
              lcd.print("== Start! ==");
              mode2_step = 3; // (4) 측정 시작 단계로 이동
          } else {
              lcd.setCursor(0, 1);
              lcd.print("Starting in... ");
              lcd.print(countdown);
              lcd.print(" ");
          }
        }
        // --- (4) Step 3: 측정 시작 (추후 구현) ---
        else if (mode2_step == 3)
        {
          // (이곳에 나중에 스윙 카운트 로직이 들어옵니다)
          lcd.setCursor(0, 1);
          lcd.print("Swinging...");
        }

        // '뒤로 가기' 버튼 (어느 단계에서든 동작)
        if (B_pressed && mode2_step != 2) // (단, 카운트다운 중에는 제외)
        {
          mode--; // mode 1 (선택 화면)으로 이동
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
          mode2_step = 0; // 상태 리셋
        }
        break;
    } // --- case 2 구현 완료 ---
      
    case 3:
      lcd.setCursor(0, 1);
      lcd.print("from case - 3");
      if (A_pressed) 
      {
        mode = 0;
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

  }
  
  delay(10); 
}
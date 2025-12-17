#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <math.h> 
#include <AS5600.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1
//#define HALL_SIM_PIN A0 

// --- 수정: 핀 번호를 4로 확정 (만약 D2에 연결했다면 2로 변경하세요) ---
const int   PHOTO_PIN      = 4;         // BUP-50S 검정선 → D4 (또는 D2?)
const float FLAG_WIDTH_M   = 0.005f;    // 플래그 폭 [m]
const float M_total_kg     = 1.50f;
const float D_m            = 0.40f;
const float G_ACCEL        = 9.80665f;
const float PI_F           = 3.14159265f;

// --- 수정: 혼란을 주는 중복된 전역 변수 삭제 ---
// float lastT_valid = -1.0f; // (삭제)
// float lastI_valid = -1.0f; // (삭제)


LiquidCrystal_I2C lcd(0x27, 16, 2);
AS5600 as5600(&Wire); // 수정: robtillaart 라이브러리는 Wire 객체 전달 필요

int mode = 0;
float SetAngle = 0.0;

class DebouncedButton 
{
    // ... (DebouncedButton 클래스는 변경 없음) ...
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
        DebouncedButton(uint8_t pin) : _pin(pin), _debounceDelay(50), _lastDebounceTime(0), _buttonState(HIGH), _lastButtonState(HIGH), _buzzerPin(0), _buzzerTone(0), _buzzerDuration(0) {}
    void begin() { pinMode(_pin, INPUT_PULLUP); }
    void attachBuzzer(uint8_t pin, int tone, int duration) {
        _buzzerPin = pin; _buzzerTone = tone; _buzzerDuration = duration;
        pinMode(_buzzerPin, OUTPUT);
    }
    bool checkPressed() {
        bool pressedEvent = false;
        int reading = digitalRead(_pin);
        if (reading != _lastDebounceTime) { _lastDebounceTime = millis(); }
        if ((millis() - _lastDebounceTime) > _debounceDelay) {
            if (reading != _buttonState) {
                _buttonState = reading;
                if (_buttonState == LOW) {
                    pressedEvent = true; 
                    if (_buzzerPin != 0) { tone(_buzzerPin, _buzzerTone, _buzzerDuration); }
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
  // ... (updateLcdDisplay 함수는 변경 없음) ...
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  switch (mode) {
    case 0: lcd.print("== Set angle =="); break;
    case 1: lcd.print("== which mode? =="); break;
    case 2: lcd.print("== Hall mode =="); break;
    case 3: lcd.print("== Photo Mode =="); break; 
  }
}

void setup() 
{
  // ... (버튼 설정 동일) ...
  buttonA = new DebouncedButton(BUTTON_A_PIN);
  buttonB = new DebouncedButton(BUTTON_B_PIN);
  buttonA->attachBuzzer(BUZZER_PIN, 1500, 100);
  buttonB->attachBuzzer(BUZZER_PIN, 800, 100);
  buttonA->begin();
  buttonB->begin();

  pinMode(PHOTO_PIN, INPUT_PULLUP); 

  Wire.begin();
  lcd.init();
  lcd.backlight();
  
  Serial.begin(115200);
  Serial.println("===== Serial initialization =====");

  // --- 수정: as5600 연결 확인 로직 변경 (robtillaart 버전) ---
  Serial.println("Checking for AS5600...");
  as5600.begin(4); // 4핀 모드(SDA, SCL, VCC, GND)로 시작
  if (as5600.isConnected() == false) { // isConnected() 사용
      Serial.println("AS5600 not detected! Check wiring.");
      lcd.clear();
      lcd.print("AS5600 ERROR");
      while (1) delay(10); 
  }
  Serial.println("AS5600 found!");
  // --- 수정 완료 ---

  updateLcdDisplay();
  Serial.print("Current mode: ");
  Serial.println(mode);
}

void loop() 
{
  bool A_pressed = buttonA->checkPressed();
  bool B_pressed = buttonB->checkPressed();

  static int mode1_selection = 0;

  // --- Mode 2 변수 ---
  static int mode2_step = 0; 
  static float angleOffset = 0.0; 
  static unsigned long stableStartTime = 0; 
  static float lastStableValue = -1.0; 
  
  // --- Mode 3 변수 ---
  static int   mode3_lastState    = HIGH;
  static bool  mode3_blockActive  = false;
  static unsigned long mode3_blockStartUs = 0;
  static unsigned long mode3_prevHitUs    = 0;
  static unsigned long mode3_lastHitUs    = 0;
  static unsigned long mode3_hitCount     = 0;
  static float mode3_lastV        = 0.0f;
  static float mode3_prevV        = 0.0f;
  static float mode3_lastDelta    = 0.0f;
  static float mode3_lastZeta     = 0.0f;
  static bool  mode3_havePrevV    = false;
  static float mode3_lastT_valid  = -1.0f;
  static float mode3_lastI_valid  = -1.0f;

  switch (mode) 
  {
    case 0: // setting angle mode
    {
      // ... (case 0는 변경 없음) ...
      
      delay(10); // 수정: delay를 case 3이 아닌 곳으로 이동
      break;
    }

    case 1:
    { 
      // ... (case 1은 변경 없음) ...

      delay(10); // 수정: delay를 case 3이 아닌 곳으로 이동
      break;
    }
    
    case 2: // hall sensor calibration mode
    {
      // ... (case 2는 변경 없음) ...

      delay(10); // 수정: delay를 case 3이 아닌 곳으로 이동
      break; 
    }
      
    case 3: // Photo Mode
    {
      // --- [수정] case 3은 delay(1)도 없이 최대 속도로 실행 ---
      
      // --- 1. 222.ino의 loop() 로직 시작 ---
      int state = digitalRead(PHOTO_PIN);
      unsigned long nowUs = micros();

      if (state != mode3_lastState) {
        // ... (HIGH -> LOW 로직 동일) ...
        if (mode3_lastState == HIGH && state == LOW) {
          mode3_blockActive  = true;
          mode3_blockStartUs = nowUs;
        }
        // ... (LOW -> HIGH 로직 동일) ...
        else if (mode3_lastState == LOW && state == HIGH) {
          if (mode3_blockActive) {
            unsigned long tauUs = nowUs - mode3_blockStartUs;
            mode3_blockActive = false;

            // ... (히트 카운트, 주기 T, tau/v 계산 동일) ...
            mode3_prevHitUs = mode3_lastHitUs;
            mode3_lastHitUs = nowUs;
            mode3_hitCount++;
            
            float T_inst = -1.0f;
            if (mode3_prevHitUs != 0) {
              unsigned long halfUs = mode3_lastHitUs - mode3_prevHitUs;
              float halfT = halfUs / 1e6f;
              T_inst = 2.0f * halfT;
              mode3_lastT_valid = T_inst; 
            }
            float T_use = (mode3_lastT_valid > 0.0f) ? mode3_lastT_valid : -1.0f;
            
            float tau_ms = tauUs / 1000.0f;
            float tau_s  = tauUs / 1e6f;
            float v      = FLAG_WIDTH_M / tau_s;

            // ... (감쇠량/감쇠비 계산 동일) ...
            bool  delta_valid = false;
            float delta = 0.0f;
            float zeta  = 0.0f;
            mode3_prevV = mode3_lastV;
            mode3_lastV = v;
            if (mode3_havePrevV && mode3_prevV > 0.0f && mode3_lastV > 0.0f) {
              delta = log(mode3_prevV / mode3_lastV);
              zeta  = delta / sqrt(4.0f * PI_F * PI_F + delta * delta);
              mode3_lastDelta = delta;
              mode3_lastZeta  = zeta;
              delta_valid = true;
            } else {
              mode3_havePrevV = true;
            }

            // ... (관성모멘트 계산 동일) ...
            float I_total = -1.0f;
            if (T_use > 0.0f && M_total_kg > 0.0f && D_m > 0.0f) {
              I_total = (T_use * T_use) * M_total_kg * G_ACCEL * D_m
                        / (4.0f * PI_F * PI_F);
              mode3_lastI_valid = I_total;
            }
            
            // --- [수정] 버그 수정: lastI_valid -> mode3_lastI_valid ---
            float I_print = (mode3_lastI_valid > 0.0f) ? mode3_lastI_valid : -1.0f;

            // ===== 5) 시리얼 출력 (222.ino 원본) =====
            Serial.print('#');
            Serial.print(mode3_hitCount);
            Serial.print('\t');
            if (T_use > 0) Serial.print(T_use, 6);
            else           Serial.print(F("NA"));
            Serial.print('\t');
            Serial.print(tau_ms, 3);
            Serial.print('\t');
            Serial.print(v, 4);
            Serial.print('\t');
            if (delta_valid) Serial.print(delta, 6);
            else             Serial.print(F("NA"));
            Serial.print('\t');
            if (delta_valid) Serial.print(zeta, 6);
            else             Serial.print(F("NA"));
            Serial.print('\t');
            if (I_print > 0) Serial.print(I_print, 8);
            else             Serial.print(F("NA"));
            Serial.println();
          }
        }
        mode3_lastState = state;
      }
      // --- 222.ino의 loop() 로직 종료 ---

      // ... (LCD 디스플레이 및 버튼 로직 동일) ...
      lcd.setCursor(0, 1);
      lcd.print("Hit:");
      lcd.print(mode3_hitCount);
      lcd.print(" T:");
      if (mode3_lastT_valid > 0) lcd.print(mode3_lastT_valid, 3);
      else lcd.print("NA");
      lcd.print("   "); 

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
    } // --- case 3 수정 완료 ---

  } // --- switch (mode) 종료 ---
  
  // delay(1); // [수정] 삭제: 이 지연이 case 3의 감지를 방해함
}
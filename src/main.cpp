#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9
#define POT_PIN A1

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
      lcd.print("== mode 2 ==");
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
      lcd.setCursor(0, 1);
      lcd.print("from case - 2");
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
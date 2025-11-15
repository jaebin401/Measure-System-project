#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9

LiquidCrystal_I2C lcd(0x27, 16, 2);

int mode = 0;
const int TOTAL_MODES = 3;

class DebouncedButton 
{
    private:
        uint8_t _pin;
        long _debounceDelay;
        long _lastDebounceTime;
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
      lcd.print("== Main Menu ==");
      break;
    case 1:
      lcd.print("Angle: "); 
      break;
    case 2:
      break;
  }
  lcd.setCursor(0, 1); 
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

  switch (mode) 
  {
    case 0:
      lcd.print("from loop - 0");
      break;
    case 1:
      lcd.print("from loop - 1");
      break;
    case 2:
      lcd.print("from loop - 2");
      break;
  }
  
  delay(10); 
}
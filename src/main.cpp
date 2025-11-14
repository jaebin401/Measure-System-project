#include <Arduino.h>

#define BUTTON_A_PIN 2
#define BUTTON_B_PIN 3
#define BUZZER_PIN 9

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

void setup() {
  Serial.begin(9600);
  
  buttonA = new DebouncedButton(BUTTON_A_PIN);
  buttonB = new DebouncedButton(BUTTON_B_PIN);

  buttonA->attachBuzzer(BUZZER_PIN, 1500, 100);
  buttonB->attachBuzzer(BUZZER_PIN, 800, 100);

  buttonA->begin();
  buttonB->begin();

  Serial.println("==== Serial initialization =====");
  Serial.print("Current mode: ");
  Serial.println(mode);
}

void loop() {
  bool A_pressed = buttonA->checkPressed();
  bool B_pressed = buttonB->checkPressed();

  if (A_pressed) {
    mode++;
    Serial.print("Button A clicked, currne mode: ");
    Serial.println(mode);
  }

  if (B_pressed) {
    mode--;
    if (mode < 0) {
      mode = TOTAL_MODES - 1;
    }
    Serial.print("Button B clicked, current mode: ");
    Serial.println(mode);
  }

  switch (mode) {
    case 0:
      break;
    case 1:
      break;
    case 2:
      break;
    default:
      break;
  }
  
  delay(10); 
}
#include <Arduino.h>

const int ButtonA = 2;
const int ButtonB = 4;
const int buzzerPin = 8;

const unsigned long debounceDelay = 50; 
const unsigned long buzzerDuration = 100; 

int lastButtonState = HIGH;   
int lastReading = HIGH;       
unsigned long lastDebounceTime = 0; 

bool isBuzzerOn = false;           
unsigned long buzzerStartTime = 0; 

void setup() {
  Serial.begin(9600); 
  pinMode(ButtonA, INPUT_PULLUP);
  pinMode(ButtonB, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); 
}

void handleButton() {
  unsigned long currentMillis = millis();
  int A_reading = digitalRead(ButtonA);

  if (A_reading != lastReading) {
    lastDebounceTime = currentMillis;
  }

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (A_reading != lastButtonState) {
      lastButtonState = A_reading;
      if (lastButtonState == LOW) {
        Serial.println("버튼 눌림!");
        digitalWrite(buzzerPin, HIGH);
        isBuzzerOn = true;
        buzzerStartTime = currentMillis;
      }
    }
  }
  
  lastReading = A_reading;
}

void handleBuzzer() {
  if (!isBuzzerOn) { return; }

  unsigned long currentMillis = millis();
  if (currentMillis - buzzerStartTime >= buzzerDuration) {
    digitalWrite(buzzerPin, LOW);
    isBuzzerOn = false; 
  }
}

void isClick()
{
    handleButton();
    handleBuzzer();
}

void loop() {
    isClick();
}
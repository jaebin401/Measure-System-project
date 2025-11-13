#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); 

// button setting
const int buttonPin = 2;
const int buzzerPin = 8;
int lastButtonState = HIGH;
int currentButtonState = digitalRead(buttonPin);

bool isClick()
{
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    Serial.println("버튼 눌림!"); 

    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
  }
  
  lastButtonState = currentButtonState;
  return currentButtonState;
}

void setup()
{
  // lcd setting
  lcd.init();				
  lcd.clear();         		
  lcd.backlight(); 		
  
  lcd.setCursor(2,0); 		
  lcd.print("hello world"); 

  // button
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Serial Monitor Initialization
  Serial.begin(9600);
}

void loop()
{

}


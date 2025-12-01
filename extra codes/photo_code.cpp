#include <Arduino.h>
const int PHOTO_PIN = 2;

volatile unsigned long hitCount = 0;      // 감지 카운트
volatile unsigned long lastTriggerUs = 0; // 마지막 트리거 시간(us)

const unsigned long DEBOUNCE_US = 5000;   // 최소 인터럽트 간격: 5 ms

void photoISR() {
  unsigned long now = micros();

  // 직전 트리거 이후 DEBOUNCE_US 이상 지났을 때만 카운트
  if (now - lastTriggerUs > DEBOUNCE_US) {
    hitCount++;
    lastTriggerUs = now;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PHOTO_PIN, INPUT_PULLUP);

  // FALLING: HIGH → LOW로 떨어질 때만 인터럽트 발생
  attachInterrupt(digitalPinToInterrupt(PHOTO_PIN), photoISR, FALLING);

  Serial.println("PHOTO SENSOR COUNT TEST (DEBOUNCE)");
}

void loop() {
  static unsigned long lastPrinted = 0;

  noInterrupts();
  unsigned long current = hitCount;
  interrupts();

  if (current != lastPrinted) {
    Serial.print("COUNT = ");
    Serial.println(current);
    lastPrinted = current;
  }
}
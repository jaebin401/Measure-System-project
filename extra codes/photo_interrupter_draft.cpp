#include <Arduino.h>

const int PHOTO_PIN = 2;

volatile unsigned long hitCount = 0;      // 감지 카운트
// ★ 추가: 마지막 트리거 시간 저장용
volatile unsigned long lastTriggerUs = 0; 

// ★ 추가: 인터럽트 간 최소 간격 (us 단위) → 5ms 정도
const unsigned long DEBOUNCE_US = 5000;   

void photoISR() {
  unsigned long now = micros();

  // ★ 변경: 직전 트리거 이후 DEBOUNCE_US 이상 지났을 때만 카운트
  if (now - lastTriggerUs > DEBOUNCE_US) {
    hitCount++;               
    lastTriggerUs = now;      // ★ 추가: 마지막 트리거 시각 갱신
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PHOTO_PIN, INPUT_PULLUP);

  // FALLING: HIGH→LOW로 떨어질 때만 인터럽트
  attachInterrupt(digitalPinToInterrupt(PHOTO_PIN), photoISR, FALLING);

  Serial.println("PHOTO SENSOR COUNT TEST (DEBOUNCE)");
}

void loop() {
  static unsigned long lastPrinted = 0;

  noInterrupts();
  unsigned long current = hitCount;
  interrupts();

  if (current != lastPrinted) {   // ★ 변경 없음: 값이 바뀔 때만 출력
    Serial.print("COUNT = ");
    Serial.println(current);
    lastPrinted = current;
  }

  // 굳이 딜레이 없어도 됨. 있으면 5~10ms 정도만.
  // delay(10);
}
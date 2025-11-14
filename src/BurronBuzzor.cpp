#include "ButtonBuzzer.h"

// 생성자: 전달받은 값들을 멤버 변수(m_... )에 저장
ButtonBuzzer::ButtonBuzzer(int buttonPin, int buzzerPin, unsigned long duration)
    : m_buttonPin(buttonPin), m_buzzerPin(buzzerPin), m_buzzerDuration(duration) 
{

}

void ButtonBuzzer::init() {
    pinMode(m_buttonPin, INPUT_PULLUP);
    pinMode(m_buzzerPin, OUTPUT);
    digitalWrite(m_buzzerPin, LOW);
}

void ButtonBuzzer::update() {
    unsigned long currentMillis = millis();

    // --- 1. 버튼 핸들러 로직 ---
    int currentReading = digitalRead(m_buttonPin);

    if (currentReading != m_lastReading) {
        m_lastDebounceTime = currentMillis;
    }

    if ((currentMillis - m_lastDebounceTime) > m_debounceDelay) {
        if (currentReading != m_lastButtonState) {
            m_lastButtonState = currentReading;
            if (m_lastButtonState == LOW) {
                // 버튼 눌림 감지!
                Serial.print(m_buttonPin); // 몇 번 핀이 눌렸는지 출력
                Serial.println("번 버튼 눌림!");
                
                // 부저 울리기 시작
                //digitalWrite(m_buzzerPin, HIGH);
                m_isBuzzerOn = true;
                m_buzzerStartTime = currentMillis;
            }
        }
    }
    m_lastReading = currentReading;

    // --- 2. 부저 핸들러 로직 ---
    if (!m_isBuzzerOn) {
        return; // 부저가 안 울리고 있으면 바로 종료
    }

    if (currentMillis - m_buzzerStartTime >= m_buzzerDuration) {
        digitalWrite(m_buzzerPin, LOW);
        m_isBuzzerOn = false;
    }
}
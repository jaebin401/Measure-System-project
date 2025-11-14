#ifndef BUTTON_BUZZER_H
#define BUTTON_BUZZER_H

#include <Arduino.h>

class ButtonBuzzer {
public:
    ButtonBuzzer(int buttonPin, int buzzerPin, unsigned long duration = 100);
    void init();
    void update();

private:

    int m_buttonPin;
    int m_buzzerPin;

    const unsigned long m_buzzerDuration;
    const unsigned long m_debounceDelay = 100;

    int m_lastButtonState = HIGH;
    int m_lastReading = HIGH;
    unsigned long m_lastDebounceTime = 0;
    bool m_isBuzzerOn = false;
    unsigned long m_buzzerStartTime = 0;
};

#endif
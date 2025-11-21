// 홀센서 캘리브레이션 코드 백업

case 2: // hall sensor calibration mode
      {
        // --- (1) Step 0: 캘리브레이션 (영점 잡기) ---
        if (mode2_step == 0)
        {
          // 1. 센서 값 읽기 (0~360도)
          int potVal = analogRead(HALL_SIM_PIN);
          float currentAngle = (float)map(potVal, 0, 1023, 0, 36000) / 100.0;

          // 2. 센서가 움직였는지 확인 (소수점 1자리 기준)
          if (fabs(currentAngle - lastStableValue) > 0.1) 
          {
            stableStartTime = millis(); // 타이머 리셋
            lastStableValue = currentAngle;
            lcd.setCursor(0, 1);
            lcd.print("Waiting static...");
          }

          // 3. 2초간 정지했는지 확인
          if (millis() - stableStartTime > 2000) 
          {
            lcd.setCursor(0, 1);
            lcd.print("Set Zero? (A=OK)");
            
            if (A_pressed) 
            {
              angleOffset = currentAngle; // 현재 각도를 0점(offset)으로 저장
              mode2_step = 1; // (2) 각도 세팅 단계로 이동
              lastBuzzerTime = millis(); // 부저 타이머 초기화
              Serial.print("Zero point set at: "); Serial.println(angleOffset);
              lcd.setCursor(0, 1);
              lcd.print("                "); // 이전 글씨 지우기
            }
          }
        }
        // --- (2) Step 1: 목표 각도 세팅 (부저 피드백) ---
        else if (mode2_step == 1)
        {
          // 1. 보정된 각도 읽기
          int potVal = analogRead(HALL_SIM_PIN);
          float currentAngle = (float)map(potVal, 0, 1023, 0, 36000) / 100.0;
          float calibratedAngle = currentAngle - angleOffset;
          
          // 2. 목표 각도(SetAngle)와의 차이 계산
          float gap = fabs(SetAngle - calibratedAngle);
          const float READY_THRESHOLD = 1.0; // 1.0도 허용 오차

          if (gap < READY_THRESHOLD) 
          {
            // 3a. 목표 도달
            lcd.setCursor(0, 1);
            lcd.print("Ready! (A=Start)");
            noTone(BUZZER_PIN); // 부저 정지

            if (A_pressed) {
                mode2_step = 2; // (3) 카운트다운 단계로 이동
                countdown = 3; // 카운트다운 3초로 리셋
                countdownStartTime = millis();
                tone(BUZZER_PIN, 1500, 100); // 시작음
            }
          } 
          else 
          {
            // 3b. 목표 근접 중 (후진 경고등)
            lcd.setCursor(0, 1);
            lcd.print("Set Angle...");
            lcd.print(calibratedAngle, 1);
            
            // 갭(30도~1도)을 부저 간격(1초~0.1초)으로 매핑
            int beep_interval = (int)map(gap, 1, 30, 100, 1000);
            beep_interval = constrain(beep_interval, 100, 1000); // 100~1000ms 범위 제한

            if (millis() - lastBuzzerTime > beep_interval) {
                tone(BUZZER_PIN, 1000, 50); // 짧은 삑 소리
                lastBuzzerTime = millis();
            }
          }
        }
        // --- (3) Step 2: 3초 카운트다운 ---
        else if (mode2_step == 2)
        {
          if (millis() - countdownStartTime >= 1000) {
              countdown--;
              countdownStartTime = millis(); // 타이머 리셋
              
              if (countdown > 0) {
                  tone(BUZZER_PIN, 800, 100); // 카운트다운 '삑'
              } else {
                  tone(BUZZER_PIN, 2000, 500); // '시작!' 긴 삑
              }
          }

          if (countdown == 0) {
              lcd.clear(); // 화면 완전히 지우기
              lcd.setCursor(0, 0);
              lcd.print("== Start! ==");
              mode2_step = 3; // (4) 측정 시작 단계로 이동
          } else {
              lcd.setCursor(0, 1);
              lcd.print("Starting in... ");
              lcd.print(countdown);
              lcd.print(" ");
          }
        }
        // --- (4) Step 3: 측정 시작 (추후 구현) ---
        else if (mode2_step == 3)
        {
          // (이곳에 나중에 스윙 카운트 로직이 들어옵니다)
          lcd.setCursor(0, 1);
          lcd.print("Swinging...");
        }

        // '뒤로 가기' 버튼 (어느 단계에서든 동작)
        if (B_pressed && mode2_step != 2) // (단, 카운트다운 중에는 제외)
        {
          mode--; // mode 1 (선택 화면)으로 이동
          Serial.print("Button B clicked, current mode: ");
          Serial.println(mode);
          updateLcdDisplay();
          mode2_step = 0; // 상태 리셋
        }
        break;
    } // --- case 2 구현 완료 ---
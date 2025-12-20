#include <Arduino.h>
#include <math.h>

// ==================== 사용자 설정 ==========================
const int   PHOTO_PIN      = 4;          // BUP-50S 검정선 → D2

// 관성모멘트: I = (T^2 * M * g * D) / (4 π^2)
// ★ M_total_kg, D_m 는 실험 셋업에 맞게 나중에 바꿔도 됨
const float M_total_kg     = 0.470f;
const float D_m            = 0.38f;
const float G_ACCEL        = 9.80665f;
const float PI_F           = 3.14159265f;
// ==========================================================

// 상태 저장용
int           lastState        = HIGH;
bool          blockActive      = false;
unsigned long blockStartUs     = 0;

unsigned long prevHitUs        = 0;
unsigned long lastHitUs        = 0;   // 마지막 히트 시각(us)
unsigned long hitCount         = 0;

// 마지막 유효 주기 / 관성모멘트 기억
float lastT_valid = -1.0f;
float lastI_valid = -1.0f;

// ==================== 요약용 추가 변수 =====================
// ★ 유효한 주기/관성모멘트 저장 배열
const int   MAX_SAMPLES   = 100;
float       Ts_valid[MAX_SAMPLES];
float       Is_valid[MAX_SAMPLES];
int         nValid        = 0;

// ★ 유효 T 범위 (정상 주기만 통과시키는 필터)
const float T_MIN_VALID   = 0.8f;   // [s] 이보다 작으면 비정상으로 간주
const float T_MAX_VALID   = 2.0f;   // [s] 이보다 크면 2회분이 합쳐진 걸로 보고 버림

// ★ 정지 판단용 (5초 동안 히트 없음이면 ‘멈췄다’고 판단)
const unsigned long STOP_MS      = 5000;
bool        summaryPrinted       = false;
// ==========================================================

void setup() {
  Serial.begin(115200);     // ★ 시리얼 모니터도 115200으로 맞춰야 함
  pinMode(PHOTO_PIN, INPUT_PULLUP);

  lastState = digitalRead(PHOTO_PIN);

  Serial.println(F("=== PHOTO MODE (polling, T & tau & I only, with SUMMARY) ==="));
}

void printSummaryIfStopped() {
  // 이미 한 번 요약 출력했으면 패스
  if (summaryPrinted) return;
  // 유효 데이터 너무 적으면 패스
  if (nValid < 3)     return;

  unsigned long nowMs     = millis();
  unsigned long lastHitMs = lastHitUs / 1000UL;

  // 마지막 히트 이후 STOP_MS(예: 5000ms) 이상 지났는지 확인
  if (nowMs - lastHitMs < STOP_MS) return;

  // ===== 여기서부터 요약 계산 =====
  float sumT = 0.0f;
  float sumI = 0.0f;
  float minT =  1e9;
  float maxT = -1e9;

  for (int i = 0; i < nValid; i++) {
    sumT += Ts_valid[i];
    sumI += Is_valid[i];
    if (Ts_valid[i] < minT) minT = Ts_valid[i];
    if (Ts_valid[i] > maxT) maxT = Ts_valid[i];
  }

  float avgT = sumT / nValid;
  float avgI = sumI / nValid;

  Serial.println();
  Serial.println(F("=== SUMMARY (after pendulum stopped) ==="));
  Serial.print(F("Valid samples     : ")); Serial.println(nValid);
  Serial.print(F("T range  [s]      : ")); Serial.print(minT, 6);
  Serial.print(F("  ~  "));               Serial.println(maxT, 6);
  Serial.print(F("T_avg   [s]       : ")); Serial.println(avgT, 6);
  Serial.print(F("I_avg [kg*m^2]    : ")); Serial.println(avgI, 8);

  summaryPrinted = true;
}

void loop() {
  int state = digitalRead(PHOTO_PIN);
  unsigned long nowUs = micros();

  // --- 엣지 감지 ---
  if (state != lastState) {
    // HIGH → LOW : 막힘 시작
    if (lastState == HIGH && state == LOW) {
      blockActive  = true;
      blockStartUs = nowUs;
    }
    // LOW → HIGH : 막힘 끝 = 플래그 통과 완료
    else if (lastState == LOW && state == HIGH) {
      if (blockActive) {
        unsigned long tauUs = nowUs - blockStartUs;
        blockActive = false;

        // ----- 히트 시간 업데이트 -----
        prevHitUs = lastHitUs;
        lastHitUs = nowUs;
        hitCount++;

        // ===== 1) 주기 T 계산 =====
        // 센서가 최저점 근처라면, 연속 두 히트 사이 시간 = 반주기 T/2
        // → 실제 T = 2 * (lastHitUs - prevHitUs)
        float T_inst = -1.0f;
        if (prevHitUs != 0) {
          unsigned long halfUs = lastHitUs - prevHitUs; // 반주기 [us]
          float halfT = halfUs / 1e6f;                  // 반주기 [s]
          T_inst = 2.0f * halfT;                        // 전체 주기 [s]
          lastT_valid = T_inst;
        }
        float T_use = (lastT_valid > 0.0f) ? lastT_valid : -1.0f;

        // ===== 2) tau =====
        float tau_ms = tauUs / 1000.0f;   // [ms]

        // ===== 3) 관성모멘트 =====
        // I = (T^2 * M * g * D) / (4 π^2)
        float I_total = -1.0f;
        if (T_use > 0.0f && M_total_kg > 0.0f && D_m > 0.0f) {
          I_total = (T_use * T_use) * M_total_kg * G_ACCEL * D_m
                    / (4.0f * PI_F * PI_F);
          lastI_valid = I_total;
        }
        float I_print = (lastI_valid > 0.0f) ? lastI_valid : -1.0f;

        // ===== 3-1) 요약용 유효 데이터 저장 =====
        if (T_use > T_MIN_VALID && T_use < T_MAX_VALID && I_total > 0.0f) {
          if (nValid < MAX_SAMPLES) {
            Ts_valid[nValid] = T_use;
            Is_valid[nValid] = I_total;
            nValid++;
          }
        }

        // ===== 4) 한 줄 출력 =====
        //Serial.print('#');
        //Serial.print(hitCount);
        //Serial.print('\t');

        if (T_use > 0) Serial.print(T_use, 6);
        else           Serial.print(F("NA"));
        Serial.print('\t');

        //Serial.print(tau_ms, 3);
        //Serial.print('\t');

        //if (I_print > 0) Serial.print(I_print, 8);
        //else             Serial.print(F("NA"));
        Serial.println();
      }
    }

    lastState = state;
  }

  printSummaryIfStopped();
}
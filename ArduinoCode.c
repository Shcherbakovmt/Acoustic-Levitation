// Сдвиг фазы: пин 2 (+5°/с), пин 3 (-5°/с), пин 4 (RESET)

const uint32_t F_CPU_HZ = 16000000UL;
const uint16_t F_OUT = 40000;       // 40 кГц
const uint16_t TOP = (F_CPU_HZ / (2UL * F_OUT)) - 1; // 199
const float TICS_PER_DEGREE = (float)(TOP + 1) / 360.0;

// Пины управления
const int PIN_PHASE_UP = 2;   // +5°/с при замыкании на GND
const int PIN_PHASE_DOWN = 3; // -5°/с при замыкании на GND
const int PIN_RESET = 4;      // Сброс фазы в 0° при замыкании на GND
const int PIN_PWM_A = 9;      // OC1A (таймер 1)
const int PIN_PWM_B = 10;     // OC1B (таймер 1)

volatile float current_phase = 0.0; // Сдвиг фазы в градусах
unsigned long last_shift_time = 0;
bool reset_pressed = false;         // Флаг состояния кнопки RESET

// Обновление фазы для обоих каналов
void updatePhase() {
  float phase_norm = fmod(current_phase, 360.0);
  if (phase_norm < 0) phase_norm += 360.0;

  uint16_t offset_ticks = (uint16_t)(phase_norm * TICS_PER_DEGREE) % (TOP + 1);
  
  noInterrupts();
  // Устанавливаем сдвиг для обоих каналов
  TCNT1 = offset_ticks; // Базовый сдвиг для таймера
  OCR1A = TOP;          // Канал A: без дополнительного сдвига
  OCR1B = (offset_ticks + (TOP + 1) / 2) % (TOP + 1); // Канал B: +180°
  interrupts();
}

void setup() {
  // Настраиваем PWM пины
  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  digitalWrite(PIN_PWM_A, LOW);
  digitalWrite(PIN_PWM_B, LOW);

  // Настраиваем пины управления
  pinMode(PIN_PHASE_UP, INPUT_PULLUP);
  pinMode(PIN_PHASE_DOWN, INPUT_PULLUP);
  pinMode(PIN_RESET, INPUT_PULLUP); // Кнопка RESET с подтяжкой

  // -------- Таймер 1 для пинов 9 и 10 --------
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // Режим CTC (WGM12=1)
  TCCR1B |= _BV(WGM12);
  OCR1A = TOP; // Частота для канала A
  OCR1B = TOP; // Частота для канала B (временно)

  // Toggle OC1A и OC1B при совпадении (50% скважность)
  TCCR1A |= _BV(COM1A0) | _BV(COM1B0);

  // Предделитель = 1 (CS10=1)
  TCCR1B |= _BV(CS10);
  interrupts();
}

void loop() {
  unsigned long now = millis();
  float delta_t = (now - last_shift_time) / 1000.0;
  last_shift_time = now;
  
  bool up_pressed = (digitalRead(PIN_PHASE_UP) == LOW);
  bool down_pressed = (digitalRead(PIN_PHASE_DOWN) == LOW);
  bool current_reset = (digitalRead(PIN_RESET) == LOW);

  // Антидребезг для кнопки RESET (10 мс)
  if (current_reset != reset_pressed) {
    delay(10);
    if (digitalRead(PIN_RESET) == LOW) {
      reset_pressed = true;
      current_phase = 0.0; // Мгновенный сброс фазы
    } else {
      reset_pressed = false;
    }
  }

  // Изменение фазы ТОЛЬКО если RESET не нажат
  if (!reset_pressed) {
    if (up_pressed && !down_pressed) {
      current_phase += 5.0 * delta_t;
    } else if (down_pressed && !up_pressed) {
      current_phase -= 5.0 * delta_t;
    }
  }
  
  updatePhase();
  delay(1);
}

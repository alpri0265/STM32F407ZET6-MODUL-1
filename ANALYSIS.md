# Аналіз програми STM32F407ZET6-MODUL-1

## 1. Загальна структура

- **Платформа:** STM32F407ZET6, HAL.
- **Режими збірки:** `BRINGUP_MODE=1` (Inc/bringup_config.h) — тестовий режим з меню на кнопках та джозі; інакше — робочий режим (planner, safety, stepgen).

### Потік виконання

```
main()
  → HAL_Init(), SystemClock_Config()
  → MX_GPIO_Init(), MX_ADC1_Init(), MX_TIM1_Init(), MX_I2C2_Init()
  → TIM6: 1 ms (для enc_if_poll_1ms та jog_process у BRINGUP_MODE)
  → app_init()
  → while(1) app_loop()
```

**app_loop():**  
`events_process()` → `system_state_process()` → (якщо !BRINGUP: safety, planner) → (якщо BRINGUP: `jog_process()`) → `screens_process()`.

---

## 2. Критичні модулі в BRINGUP_MODE

### 2.1 Джог (jog.c)

- **Призначення:** рух осей X/Z по джойстику (або кнопках меню) у режимі Jog.
- **Піни:** X: PA8=STEP, PA9=DIR, PA10=ENA; Z: PB6=STEP, PB7=DIR, PB8=ENA (з main.h).
- **Ініціалізація (jog_init):** викликається з app_init(). Переводить PA8 і PB6 з AF (TIM1/TIM4) у звичайний GPIO output, встановлює ENA залежно від `ENA_ACTIVE_HIGH`.
- **Кроки:** генеруються в `jog_process()`:
  - Викликається **з TIM6 (кожні 1 ms)** — це забезпечує стабільну частоту кроків (~500/с), незалежно від повільного головного циклу (I2C дисплей).
  - Додатково викликається з **app_loop()** — зайві виклики, трохи навантаження на CPU, логіку не ламають.
- **Умова виконання:** `menu_current_screen() == SCREEN_JOG`; інакше вихід без кроків.
- **Обмеження частоти:** один крок не частіше ніж раз на 2 ms (`JOG_STEP_PERIOD_MS`).
- **Імпульс STEP:** за замовчуванням активний по LOW (idle HIGH, потім короткий LOW ~48 µs), потім знову HIGH.

### 2.2 Меню (menu.c, encoder_menu.c, screens.c)

- **menu.c:** стек екранів, список пунктів (Main → Jog / Settings / Diagnostics / Tool angle / Info), перехід за Enter/Back.
- **encoder_menu.c:** опитування кнопок PB12 (Up), PB13 (Down), PB14 (Enter) з дебаунсом; формує дії CCW/CW/Enter для screens.
- **screens.c:** відмальовування поточного екрану (в т.ч. Jog з U/D/L/R та StX/StZ), обробка дій (вхід/вихід, особливі екрани — Jog, Tool angle, Limits тощо).

На екрані Jog тільки дія «Up» (CCW) викликає `menu_back()`; Enter і CW передаються в jog (перемикання осі/рух при режимі кнопок).

### 2.3 Таймери та переривання

- **TIM6 (1 ms):** у `TIM6_DAC_IRQHandler` викликаються `enc_if_poll_1ms()` та (у BRINGUP_MODE) `jog_process()`. Джог зав’язаний на цей період для стабільної частоти кроків.
- **TIM1:** ініціалізується (MX_TIM1_Init), PA8 з HAL налаштовується як TIM1_CH1 у stm32f4xx_hal_msp.c. У BRINGUP_MODE TIM1 **не запускається** (tim_if_start/stepgen не викликаються), тому обробник TIM1_UP_TIM10 не викликається і на PA8 ніхто крім jog не пише. Якщо в майбутньому у іншому режимі буде використовуватись stepgen (tim_if_start), обробник TIM1 перемикає X_STEP — тоді конфлікт з джогом не виникає, бо джог у цьому режимі не використовується.

---

## 3. Потенційні проблеми та рекомендації

### 3.1 Розходження board.h і main.h (крокові виходи)

- **board.h:** X_STEP=PA0, X_DIR=PA1, Z_STEP=PA2, Z_DIR=PA3 (енкодери/ADC на цих пінах у проєкті).
- **main.h (фактично використовується):** X_STEP=PA8, X_DIR=PA9, X_EN=PA10, Z_STEP=PB6, Z_DIR=PB7, Z_EN=PB8.

Усі модулі кроків (jog, main.c GPIO, HAL MSP) орієнтуються на main.h. Якщо хтось підключить код до board.h для STEP/DIR — отримає неправильні піни. **Рекомендація:** у board.h вирівняти визначення STEP/DIR/EN під main.h (PA8/PA9/PA10, PB6/PB7/PB8) або чітко задокументувати, що крокові виходи беруться лише з main.h.

### 3.2 Подвійний виклик jog_process()

- `jog_process()` викликається з TIM6 (кожні 1 ms) і з app_loop(). Це не помилка: обмеження 2 ms у самому jog_process не дає подвоєння кроків. Можна прибрати виклик з app_loop() для зменшення навантаження, залишивши тільки виклик з TIM6.

### 3.3 Шпиндель і TIM1

- **spindle.c** використовує TIM1, канал 1 (PA8): `HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1)`. У BRINGUP_MODE, якщо ніде не викликати spindle_set_rpm, TIM1 не стартує і конфлікту з джогом по PA8 немає. Якщо в тестовому режимі додати керування шпинделем з меню — треба пам’ятати, що PA8 тоді буде зайнятий PWM і джог по X (STEP на PA8) і шпиндель разом не працюватимуть коректно без зміни пін-мапи.

### 3.4 Рух двигунів відсутній при зростанні StX/StZ

- Якщо на екрані Jog лічильники StX/StZ змінюються, а двигуни не крутяться — програмна частина генерує кроки коректно. Проблема зводиться до:
  - проводки (PA8/PB6 — до PUL− драйвера, GND спільний);
  - живлення та логіки драйвера (5 V на PUL+/DIR+/ENA+, правильна полярність ENA);
  - відповідності полярності/ширини імпульсу даташиту драйвера (у коді вже є STEP_PULSE_ACTIVE_LOW та ENA_ACTIVE_HIGH для підбору).

---

## 4. Залежності (орієнтовно)

- **main.c:** app, enc_if.
- **app.c:** system_state, events, screens, system_config, tool_angle; у BRINGUP_MODE — encoder_menu, menu, jog.
- **jog.c:** main.h (піни), bringup_config, menu (SCREEN_JOG, menu_current_screen).
- **stm32f4xx_it.c:** enc_if; у BRINGUP_MODE — bringup_config, jog (jog_process з TIM6).
- **screens.c:** menu, encoder_menu, lcd, axis_feedback, system_config, fault, temperature, tool_angle, adc_if, jog (jog_get_joy_state, jog_get_step_counts).

---

## 5. Висновок

- У BRINGUP_MODE джог коректно переводить PA8/PB6 у GPIO, генерує кроки з TIM6 при натисканні джойстика на екрані Jog і не конфліктує з TIM1, оскільки TIM1 не запускається.
- Розходження в board.h щодо пінів крокових виходів варто усунути або явно задокументувати.
- Відсутність руху двигунів при зміні StX/StZ слід далі шукати на стороні підключення та драйвера (перевірка осцилографом по PINOUT.md).

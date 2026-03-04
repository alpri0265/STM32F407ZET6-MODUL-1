# План дій в STM32CubeMX: меню на кнопках замість енкодера

Нижче — лише кроки в CubeMX. Код (btn_menu, перемикання в events/app) робиться окремо.

**Джойстик (PC2–PC5) залишається для руху по осях — для меню не використовується.**

---

## 1. Кнопки керування меню (PB12, PB13, PB14)

Ті самі піни, що були під енкодер: тепер три окремі кнопки.

| Пін   | Сигнал   | Дія в меню           | Режим в CubeMX      |
|-------|----------|----------------------|----------------------|
| **PB12** | MENU_UP   | Курсор вгору (prev)  | GPIO_Input, Pull-up  |
| **PB13** | MENU_DOWN | Курсор вниз (next)   | GPIO_Input, Pull-up  |
| **PB14** | MENU_ENTER| Enter (вхід/вибір)   | GPIO_Input, Pull-up  |

Підключення: одна нога кнопки — на пін, друга — на **GND**. Натиснуто = низький рівень.

**У CubeMX:**

1. **Pinout & Configuration** → **System Core** → **GPIO** → **GPIOB**.
2. **PB12**:
   - **GPIO mode**: **Input mode** (не External Interrupt).
   - **GPIO Pull-up/Pull-down**: **Pull-up**.
   - **User Label**: MENU_UP (або залишити MPG_A, якщо так зручніше).
3. **PB13**:
   - **GPIO mode**: Input mode.
   - **Pull-up**: Pull-up.
   - **User Label**: MENU_DOWN (або MPG_B).
4. **PB14**:
   - **GPIO mode**: Input mode.
   - **Pull-up**: Pull-up.
   - **User Label**: MENU_ENTER (або MPG_BTN).

Важливо: **PB12 без EXTI** — лише Input + Pull-up, щоб не потрібен був обробник переривань.

---

## 2. NVIC — вимкнути EXTI для PB12

Якщо раніше PB12 був у режимі External Interrupt:

- **System Core** → **NVIC**.
- **EXTI line [15:10] interrupts** — **вимкнути** (Disabled).

Після цього переривання по PB12 викликатись не буде.

---

## 3. TIM6 — вимкнути

TIM6 використовувався для опитування енкодера. Для меню на кнопках він не потрібен.

- **Timers** → **TIM6**.
- **Mode**: **Disabled**.

Після генерації коду TIM6 не буде ініціалізований.

---

## 4. I2C2 (LCD)

Без змін: I2C2 увімкнений, PB10 (SCL), PB11 (SDA).

---

## 5. Генерація коду

1. **Project** → **Generate Code** (або Ctrl+Shift+G).
2. У згенерованому `main.c` перевірити **MX_GPIO_Init()**:
   - PB12, PB13, PB14 — **GPIO_MODE_INPUT**, **GPIO_PULLUP** (без EXTI).
3. Переконатись, що викликів ініціалізації TIM6 у вашому коді після генерації немає або вони не виконуються.

---

## 6. Підсумок

| Крок | Дія в CubeMX |
|------|------------------------------|
| 1    | **GPIOB**: PB12, PB13, PB14 — **Input**, **Pull-up**; PB12 **не** в режимі EXTI. |
| 2    | **NVIC**: вимкнути **EXTI line [15:10]**. |
| 3    | **Timers** → **TIM6** → Mode: **Disabled**. |
| 4    | **Generate Code**. |

Після цього залишається додати модуль кнопок меню (btn_menu) і в events/app використовувати його замість енкодера.

# Перелік файлів, що відповідають за роботу дисплея (LCD 2004 I2C)

## 1. Драйвер та логіка екранів (основні)

| Файл | Роль |
|------|------|
| **`Src/lcd.c`** | Реалізація драйвера LCD: `lcd_init()`, `lcd_print()`. Зараз — заглушка; тут має бути робота з I2C та HD44780. |
| **`Inc/lcd.h`** | Оголошення інтерфейсу драйвера: `lcd_init()`, `lcd_print(const char*)`. |
| **`Src/screens.c`** | Логіка екранів: що показувати (INIT/READY/HOLD/ERROR), виклики `lcd_init()` та `lcd_print()`. |
| **`Inc/screens.h`** | Оголошення: `screens_init()`, `screens_process()`. |

---

## 2. Конфігурація та інтеграція

| Файл | Роль |
|------|------|
| **`Inc/board.h`** | Конфігурація дисплея: `LCD_I2C` (I2C2), піни PB10 (SCL), PB11 (SDA) — макроси для драйвера. |
| **`Inc/main.h`** | Визначення пінів I2C2: `I2C2_SCL_Pin`, `I2C2_SDA_Pin`, порти GPIO. |
| **`Src/main.c`** | Налаштування пінів PB10/PB11 під I2C2 у `MX_GPIO_Init()`; тут має бути виклик `MX_I2C2_Init()` (зараз відсутній). |
| **`Inc/stm32f4xx_hal_conf.h`** | Увімкнення модуля I2C у HAL: `HAL_I2C_MODULE_ENABLED` (зараз вимкнено). |

---

## 3. Виклик з рівня застосунку

| Файл | Роль |
|------|------|
| **`Src/app.c`** | Викликає `screens_init()` при старті та `screens_process()` у циклі — запускає роботу дисплея. |

---

## 4. Документація (довідково)

| Файл | Роль |
|------|------|
| **`DISPLAY_ANALYSIS.md`** | Аналіз архітектури дисплея та інтерфейсу. |
| **`LCD_TROUBLESHOOTING.md`** | Розбір причин, чому дисплей не працює, та кроки виправлення. |
| **`PINOUT.md`** | Розпіновка, у т.ч. PB10 (SCL), PB11 (SDA) для LCD 2004. |

---

## 5. Схема залежностей

```
app.c          → screens_init(), screens_process()
screens.c      → lcd.h, system_state.h  →  lcd_init(), lcd_print()
lcd.c          → lcd.h, (потрібно: board.h, HAL I2C)
board.h        → LCD_I2C, LCD_I2C_SCL_*, LCD_I2C_SDA_*
main.c         → MX_GPIO_Init() (піни I2C2), потрібно: MX_I2C2_Init()
main.h         → I2C2_SCL_Pin, I2C2_SDA_Pin
hal_conf.h     → HAL_I2C_MODULE_ENABLED
```

---

## 6. Стислий перелік тільки файлів коду

- `Src/lcd.c`
- `Inc/lcd.h`
- `Src/screens.c`
- `Inc/screens.h`
- `Inc/board.h`
- `Inc/main.h`
- `Src/main.c`
- `Inc/stm32f4xx_hal_conf.h`
- `Src/app.c`

Ці файли безпосередньо відповідають за роботу дисплея в проєкті.

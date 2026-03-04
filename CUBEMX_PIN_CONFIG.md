# CubeMX pin configuration for bring-up mode (encoder + LCD)

## 1. I2C2 (LCD 2004)

- **Connectivity → I2C2**: Enable.
- **Mode**: I2C.
- **Configuration → Parameter Settings**: Default (100 kHz).
- **Pin assignment** (auto):
  - PB10 → I2C2_SCL
  - PB11 → I2C2_SDA
- **GPIO**: Open-Drain, Pull-up (if not auto), Alternate AF4.

## 2. Encoder (menu navigation)

Use existing MPG pins: **PB12 (A)**, **PB13 (B)**, **PB14 (Button)**.

### PB12 – Channel A (EXTI)

- **System Core → GPIO → PB12**:
  - **GPIO mode**: External Interrupt Mode with Rising edge trigger.
  - **GPIO Pull-up/Pull-down**: Pull-up.
  - **User Label**: MPG_A (or keep existing).

### PB13 – Channel B (polled)

- **System Core → GPIO → PB13**:
  - **GPIO mode**: Input mode.
  - **GPIO Pull-up/Pull-down**: Pull-up.
  - **User Label**: MPG_B.

### PB14 – Button (polled)

- **System Core → GPIO → PB14**:
  - **GPIO mode**: Input mode.
  - **GPIO Pull-up/Pull-down**: Pull-up.
  - **User Label**: MPG_BTN.

## 3. NVIC

- **System Core → NVIC**:
  - **EXTI line[15:10] interrupts**: Enabled.
  - **Priority**: e.g. 1 (or as in code: 1, 0).

## 4. Generate code

- **Project → Generate Code**.
- Ensure `MX_GPIO_Init()` configures:
  - PB12: EXTI, rising, pull-up.
  - PB13, PB14: input, pull-up (no EXTI).
- Ensure `MX_I2C2_Init()` is generated and called from `main()`.

## 5. Add bring-up source files to project

- **C source**: `bsp/enc_if.c`, `Src/encoder_menu.c`, `Src/menu.c`.
- **Include path**: ensure `bsp` (or project root) is in Include path so `enc_if.h` is found.
- **Header**: `Inc/bringup_config.h` (already in Inc).

## 6. Manual check after generation

- In `main.c`, `MX_GPIO_Init()` must **not** set PB13/PB14 as `GPIO_MODE_IT_*`; only PB12 as EXTI.
- If CubeMX sets all three as EXTI, manually change the init so that only PB12 is EXTI; PB13 and PB14 are `GPIO_MODE_INPUT` with `GPIO_PULLUP`.
- Confirm `EXTI15_10_IRQHandler` is present in `stm32f4xx_it.c` and that `HAL_GPIO_EXTI_Callback` in `main.c` calls `enc_if_a_irq_trigger()` when `GPIO_Pin == MPG_A_Pin`.
- Build: add `bsp/enc_if.c` to the project if not auto-included.

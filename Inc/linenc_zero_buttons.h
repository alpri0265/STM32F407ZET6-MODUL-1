#ifndef LINENC_ZERO_BUTTONS_H
#define LINENC_ZERO_BUTTONS_H

/* Optional feature: separate GPIO buttons to zero linear encoder readout.
 * To enable, define pins in CubeMX (or in main.h USER CODE section):
 *   LIN_ZERO_X_Pin / LIN_ZERO_X_GPIO_Port
 *   LIN_ZERO_Z_Pin / LIN_ZERO_Z_GPIO_Port
 *
 * Buttons are assumed active-low with internal pull-up.
 */

void linenc_zero_buttons_init(void);
void linenc_zero_buttons_process(void);

#endif /* LINENC_ZERO_BUTTONS_H */

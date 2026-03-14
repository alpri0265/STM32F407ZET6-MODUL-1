#ifndef SL_LIMITS_H
#define SL_LIMITS_H

#include <stdbool.h>
#include "system_config.h"

/* Програмні ліміти: навчання кнопками SL_*_BIT, індикація LED. */

void sl_limits_init(void);

/* Читання кнопок: 1 = натиснуто. Очікується активний LOW (натиснуто = GND). */
bool sl_limits_btn_x_neg(void);
bool sl_limits_btn_x_pos(void);
bool sl_limits_btn_z_neg(void);
bool sl_limits_btn_z_pos(void);

/* Навчання: зберегти поточну позицію (pos_mm) як ліміт для осі. */
void sl_limits_teach_x_neg(float pos_mm);
void sl_limits_teach_x_pos(float pos_mm);
void sl_limits_teach_z_neg(float pos_mm);
void sl_limits_teach_z_pos(float pos_mm);

/* Отримати збережені ліміти (мм). */
float sl_limits_get_x_min(void);
float sl_limits_get_x_max(void);
float sl_limits_get_z_min(void);
float sl_limits_get_z_max(void);

/* Чи обидва ліміти осі навчені */
bool sl_limits_x_taught(void);
bool sl_limits_z_taught(void);

/* Перевірка: чи pos в межах. Якщо не навчено - завжди true. */
bool sl_limits_in_range_x(float pos_mm);
bool sl_limits_in_range_z(float pos_mm);

/* Оновити LED за позицією. */
void sl_limits_update_leds(float x_mm, float z_mm);

void sl_limits_reset(void);

/* Обробка кнопок (навчання) та оновлення LED. Викликати з app з x_mm, z_mm. */
void sl_limits_process(float x_mm, float z_mm);

#endif

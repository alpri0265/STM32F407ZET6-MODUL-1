#ifndef TOOL_ANGLE_H
#define TOOL_ANGLE_H
/* Kut instrumentu z absoljutnogo enkodera (ADC, 0..360 deg). */
/* Zavantazhyty zberezheni ref z Flash; vyklykaty pislya init peryferii. */
void tool_angle_init(void);
float tool_angle_get_deg(void);
/* Obnulennja: potochna pozycija vvazhatymetjsja 0 deg. */
void tool_angle_zero(void);
/* Vstanovyty vidobrazhuvanyj kut vruchnu: potochna pozycija pokazuvatymetjsja jak deg (0..360). */
void tool_angle_set_displayed_deg(float deg);
#endif

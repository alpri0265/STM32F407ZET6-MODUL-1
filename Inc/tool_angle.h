#ifndef TOOL_ANGLE_H
#define TOOL_ANGLE_H
/* Kut instrumentu z absoljutnogo enkodera (ADC, 0..360 deg). */
float tool_angle_get_deg(void);
/* Obnulennja: potochna pozycija vvazhatymetjsja 0 deg. */
void tool_angle_zero(void);
/* Vstanovyty vidobrazhuvanyj kut vruchnu: potochna pozycija pokazuvatymetjsja jak deg (0..360). */
void tool_angle_set_displayed_deg(float deg);
#endif

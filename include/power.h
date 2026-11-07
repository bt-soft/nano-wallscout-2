/**
 * @file power.h
 * @brief Mélyalvás (Power-down) és ébredés gombnyomásra.
 */
#pragma once
#include <Arduino.h>

/**
 * @brief Mélyalvásba megy, és bármelyik gomb megnyomásáig (pin-change megszakítás) alszik.
 *        Visszatérés = ébredés. Az ADC leáll, a BOD kikapcsolva; a hívó felelős a TX,
 *        a buzzer és a kijelző előzetes lekapcsolásáért, valamint az ébredés utáni újrainicializálásért.
 */
void powerDeepSleep();

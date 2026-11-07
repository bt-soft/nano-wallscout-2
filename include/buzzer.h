/**
 * @file buzzer.h
 * @brief Passzív buzzer hangkeltése a Timer2 hardveres PWM-jével (OC2B = PD3).
 */
#pragma once
#include <Arduino.h>

/** @brief A buzzer láb inicializálása (kimenet, alacsony). */
void buzzerInit();

/**
 * @brief Folyamatos hang indítása / frekvenciájának módosítása.
 * @param hz     Frekvencia Hz-ben (kb. 250..3000 tartományban értelmes).
 * @param volume Hangerő 0..3 (0 = néma). A hangerőt a kitöltési tényező szabályozza.
 */
void buzzerTone(uint16_t hz, uint8_t volume);

/** @brief A hang leállítása, a Timer2 kikapcsolása, a láb alacsonyra állítása. */
void buzzerOff();

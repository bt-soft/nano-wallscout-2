/**
 * @file measure.h
 * @brief Az impulzus-indukciós (PI) mérés: TX impulzus, késleltetett ADC mintavétel, átlagolás.
 *
 * A mérés közben a megszakítások tiltottak (determinisztikus időzítés),
 * az időalapot a Timer1 (1 tick = 62,5 ns) adja, az ADC-t közvetlen regiszterekkel hajtjuk.
 */
#pragma once
#include <Arduino.h>

/** Egy átlagolt mérés eredménye. */
struct MeasureResult {
  float s1;        ///< Korai minta átlaga (nyers ADC érték, 0..1023, nyugalmi érték ~512)
  float s2;        ///< Késői minta átlaga (nyers ADC érték)
  bool saturated;  ///< Igaz, ha az impulzusok jelentős részénél az ADC telített
};

/**
 * @brief Hardver inicializálása: TX láb, ADC, Timer1. Alvásból ébredés után is hívható.
 */
void measureInit();

/**
 * @brief Az időzítések kiszámítása a beállításokból (tick-ekben).
 * @param pulseUs TX impulzus hossza (us).
 * @param freqHz  Ismétlési frekvencia (Hz).
 * @param s1Us    Korai minta késleltetése a TX lekapcsolásától (us).
 * @param s2Us    Késői minta késleltetése a TX lekapcsolásától (us).
 */
void measureConfigure(uint8_t pulseUs, uint16_t freqHz, uint16_t s1Us, uint16_t s2Us);

/** @brief Az átlagoló akkumulátor törlése. */
void measureReset();

/**
 * @brief Adott számú impulzus lefuttatása és az eredmények hozzáadása az akkumulátorhoz.
 * @param pulses Az impulzusok száma.
 */
void measureRun(uint8_t pulses);

/**
 * @brief Folyamatos TX impulzussor (szkópos beállításhoz); gombnyomásra tér vissza.
 */
void measureTxStream();

/**
 * @brief Az akkumulált impulzusok átlagának lekérése.
 * @param r Ide kerül az eredmény.
 * @return Igaz, ha volt mérés (az akkumulátor nem üres).
 */
bool measureResult(MeasureResult &r);

/**
 * @brief A tápfeszültség mérése az A1 csatornán (az osztó figyelembevételével).
 * @return Tápfeszültség mV-ban.
 */
uint16_t readVbattMv();

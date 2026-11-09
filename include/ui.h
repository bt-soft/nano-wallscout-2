/**
 * @file ui.h
 * @brief Az OLED (SSD1306 128x64) képernyői. Az adatokat a main.cpp állítja össze (UiData).
 */
#pragma once
#include <Arduino.h>
#include "config.h"
#include "settings.h"

/** Mélység-állapot jelzők a UiData::depthMm mezőhöz. */
#define DEPTH_NONE (-1)  ///< Nincs találat
#define DEPTH_OVER (-2)  ///< A mélység a mérési tartomány felett van

/** A képernyők kirajzolásához szükséges adatok. */
struct UiData {
  float strength;       ///< Előjeles jelerősség (nullázott S1, ADC egység), nyers érték a naplóhoz
  float shown;          ///< Kijelzett jelerősség: találat nélkül 0 (hiszterézis), találatnál = strength
  float peak;           ///< Csúcs-megtartott |jel|
  uint16_t fullScale;   ///< Az érzékenységhez tartozó teljes skála
  bool graphAbs;        ///< Grafikon módja: igaz = abszolút érték (ABS), hamis = előjeles (SIG)
  float dd;             ///< Simított impulzus alatti eltérés a DD képernyőhöz
  float ddRaw;          ///< Simítatlan impulzus alatti eltérés
  float ddBaseline;     ///< TX előtti ADC átlag
  float ddPulse;        ///< TX alatti ADC átlag
  bool ddSaturated;     ///< Bármely DD minta ADC telítésben volt
  float s1Raw;          ///< Nyers S1 átlag
  float s2Raw;          ///< Nyers S2 átlag
  int16_t depthMm;      ///< Becsült mélység (mm) vagy DEPTH_NONE / DEPTH_OVER
  bool depthCal;        ///< Igaz, ha a mélységbecslés kalibrált
  uint8_t metal;        ///< 0 = nincs, 1 = vas jellegű, 2 = színesfém jellegű
  bool saturated;       ///< Az ADC telítésben van
  bool zeroing;         ///< Nullázás folyamatban (a keresőkép alján "NULLAZAS..." látszik)
  uint16_t vbattMv;     ///< Tápfeszültség (mV)
  const int8_t *hist;   ///< Grafikon előzmények (gyűrűpuffer)
  uint8_t histHead;     ///< A gyűrűpuffer legrégebbi elemének indexe
};

/** @brief A kijelző inicializálása (I2C 400 kHz). */
void uiInit();

/**
 * @brief Függvény beállítása, amit a kijelzőfrissítés közben minden lap (page) után meghívunk,
 *        így a gombok közben is pollozhatók.
 * @param hook A hívandó függvény (nullptr = nincs).
 */
void uiSetPollHook(void (*hook)());

/** @brief A kijelző kikapcsolása alvás előtt. */
void uiSleep();

/** @brief A kijelző visszakapcsolása ébredés után. */
void uiWake();

/**
 * @brief Egysoros üzenet a kijelző közepén.
 * @param fmt Flash-ben lévő (PSTR) szöveg, tartalmazhat egy %d-t.
 * @param v   A %d helyére kerülő érték.
 */
void uiMessage(PGM_P fmt, int v);

/** @brief Keresés képernyő kirajzolása. @param d A megjelenítendő adatok. */
void uiDrawSearch(const UiData &d);

/** @brief Grafikon képernyő kirajzolása. @param d A megjelenítendő adatok. */
void uiDrawGraph(const UiData &d);

/** @brief DD beállítás (nullpont-keresés) képernyő kirajzolása. @param d A megjelenítendő adatok. */
void uiDrawDD(const UiData &d);

/**
 * @brief Beállítások képernyő kirajzolása.
 * @param s     Az aktuális beállítások.
 * @param sel   A kijelölt elem indexe.
 * @param vbatt Tápfeszültség (mV).
 * @param txTest Igaz, ha a folyamatos TX teszt be van kapcsolva.
 */
void uiDrawSettings(const Settings &s, uint8_t sel, uint16_t vbatt, bool txTest);

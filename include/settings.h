/**
 * @file settings.h
 * @brief Állítható paraméterek és tárolásuk az EEPROM-ban.
 */
#pragma once
#include <Arduino.h>

/** A Beállítások képernyő elemeinek sorszámai. */
enum SettingsItem : uint8_t {
  SET_PULSE = 0,   ///< TX impulzus hossza (us)
  SET_FREQ,        ///< TX ismétlési frekvencia (Hz)
  SET_S1,          ///< Korai minta késleltetése a lekapcsolás után (us)
  SET_S2,          ///< Késői minta késleltetése a lekapcsolás után (us)
  SET_SENS,        ///< Érzékenység (kijelzés- és hangskála)
  SET_VOLUME,      ///< Buzzer hangerő (0 = néma)
  SET_CAL_DEPTH,   ///< A mélység-kalibrációhoz használt referencia-mélység (mm)
  SET_GRAPH_MODE,  ///< Grafikon kijelzési módja: abszolút érték (ABS) vagy előjeles (SIG)
  SET_TX_TEST,     ///< Folyamatos TX impulzussor szkópos csillapítás-beállításhoz (nem tárolódik)
  SET_DEFAULTS,     ///< Alapértelmezések visszaállítása (művelet)
  SET_COUNT        ///< Elemek száma
};

/** A tartósan tárolt beállítások. */
struct Settings {
  uint16_t magic;       ///< Érvényességi jelző
  uint8_t version;      ///< Szerkezet-verzió
  uint8_t pulseUs;      ///< TX impulzushossz (us)
  uint16_t freqHz;      ///< TX ismétlési frekvencia (Hz)
  uint16_t s1Us;        ///< Korai minta késleltetése (us)
  uint16_t s2Us;        ///< Késői minta késleltetése (us)
  uint8_t sensIdx;      ///< Érzékenység-fokozat indexe
  uint8_t volume;       ///< Hangerő 0..3
  uint8_t calDepthMm;   ///< Referencia-mélység a kalibrációhoz (mm)
  uint8_t calValid;     ///< 1, ha a calAmp valódi kalibrációból származik
  float calAmp;         ///< Referencia amplitúdó (ADC egység) a calDepthMm mélységhez
  uint8_t graphAbs;     ///< 1 = a grafikon és a felső szám abszolút értéket mutat (ABS), 0 = előjeles (SIG)
  uint8_t crc;          ///< Ellenőrző összeg
};

/**
 * @brief Alapértelmezett értékek betöltése.
 * @param s A kitöltendő beállítás-struktúra.
 */
void settingsDefaults(Settings &s);

/**
 * @brief Beállítások betöltése az EEPROM-ból; érvénytelen tartalom esetén alapértékek.
 * @param s A kitöltendő struktúra.
 */
void settingsLoad(Settings &s);

/**
 * @brief Beállítások mentése az EEPROM-ba (csak a megváltozott bájtokat írja).
 * @param s A mentendő struktúra.
 */
void settingsSave(Settings &s);

/**
 * @brief Az adott elem értékének léptetése (túlcsorduláskor visszaugrik a minimumra).
 * @param s    A módosítandó beállítások.
 * @param item Az elem sorszáma (SettingsItem).
 * @note A SET_DEFAULTS elemet ez nem kezeli, azt a hívónak kell végrehajtania.
 */
void settingsAdjust(Settings &s, uint8_t item);

/**
 * @brief Értékek korlátok közé szorítása és az S1/S2 minimális távolságának kikényszerítése.
 * @param s A javítandó beállítások.
 */
void settingsSanitize(Settings &s);

/**
 * @brief Az érzékenység-fokozathoz tartozó teljes skála (ADC egység).
 * @param idx Fokozat indexe (0..SENS_COUNT-1).
 * @return A kijelzés/hang teljes skálája.
 */
uint16_t settingsFullScale(uint8_t idx);

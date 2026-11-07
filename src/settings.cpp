/**
 * @file settings.cpp
 * @brief Beállítások kezelése és EEPROM tárolás.
 */
#include "settings.h"
#include "config.h"
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

/** Érvényességi jelző és szerkezet-verzió; verzióváltáskor az EEPROM tartalom eldobódik. */
static const uint16_t SETTINGS_MAGIC = 0x1975; // tetszőleges érték, hogy az EEPROM üres vagy érvénytelen értéke felismerhető legyen.
static const uint8_t SETTINGS_VERSION = 4;     // EEPROM struktúra verziója

/** Az érzékenység-fokozatok teljes skálái (ADC egység); nagyobb index = érzékenyebb. */
static const uint16_t kFullScale[SENS_COUNT] PROGMEM = {500, 250, 120, 60, 30, 15};

/**
 * @brief Egyszerű 8 bites ellenőrző összeg a struktúra utolsó (crc) bájtja nélkül.
 * @param s A struktúra.
 * @return Az ellenőrző összeg.
 */
static uint8_t calcCrc(const Settings &s) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&s);
    uint8_t crc = 0x5A;
    for (uint8_t i = 0; i < sizeof(Settings) - 1; i++) {
        crc = (uint8_t)((crc << 1) | (crc >> 7)); // forgatás balra
        crc ^= p[i];
    }
    return crc;
}

/**
 * Alapértelmezett értékek beállítása.
 *
 * @param s A beállítások struktúrája.
 */
void settingsDefaults(Settings &s) {
    s.magic = SETTINGS_MAGIC;
    s.version = SETTINGS_VERSION;
    s.pulseUs = PULSE_DEF_US;
    s.freqHz = FREQ_DEF_HZ;
    s.s1Us = S1_DEF_US;
    s.s2Us = S2_DEF_US;
    s.sensIdx = SENS_DEF_IDX;
    s.volume = 2;
    s.calDepthMm = CAL_DEPTH_DEF_MM;
    s.calValid = 0;
    s.calAmp = CAL_AMP_DEF;
    s.graphAbs = 1;
    s.crc = 0;
}

/**
 * @brief Az aktuális impulzushosszal és S2 késleltetéssel megengedett legnagyobb frekvencia.
 *
 * Egy periódusba bele kell férnie az impulzusnak, az S2 késleltetésnek, az S2 konverziónak
 * és egy biztonsági tartaléknak, különben a tényleges ismétlési frekvencia lecsökkenne.
 *
 * @param s A beállítások struktúrája.
 * @return A legnagyobb megengedett frekvencia Hz-ben (legfeljebb FREQ_MAX_HZ).
 */
static uint16_t freqLimitHz(const Settings &s) {
    const uint32_t minPeriodUs = (uint32_t)s.pulseUs + s.s2Us + ADC_CONV_US + PERIOD_MARGIN_US;
    const uint32_t f = 1000000UL / minPeriodUs;
    return f > FREQ_MAX_HZ ? FREQ_MAX_HZ : (uint16_t)f;
}

/**
 * @brief A beállítások érvényes tartományra korlátozása.
 *
 * @param s A beállítások struktúrája.
 */
void settingsSanitize(Settings &s) {
    s.pulseUs = constrain(s.pulseUs, PULSE_MIN_US, PULSE_MAX_US);
    s.s1Us = constrain(s.s1Us, S1_MIN_US, S1_MAX_US);
    s.s2Us = constrain(s.s2Us, S2_MIN_US, S2_MAX_US);
    // Az S2 mintának a S1 konverziója után kell jönnie.
    if (s.s2Us < s.s1Us + MIN_S_GAP_US)
        s.s2Us = s.s1Us + MIN_S_GAP_US;
    // A frekvencia az impulzus és az S2 késleltetés függvényében korlátozott (a minimum mindig érvényes).
    uint16_t fMax = freqLimitHz(s);
    if (fMax < FREQ_MIN_HZ)
        fMax = FREQ_MIN_HZ;
    s.freqHz = constrain(s.freqHz, FREQ_MIN_HZ, fMax);
    if (s.sensIdx >= SENS_COUNT)
        s.sensIdx = SENS_DEF_IDX;
    if (s.volume > 3)
        s.volume = 3;
    s.graphAbs = s.graphAbs ? 1 : 0;
    s.calDepthMm = constrain(s.calDepthMm, CAL_DEPTH_MIN_MM, CAL_DEPTH_MAX_MM);
    if (!(s.calAmp > 0.0f)) { // NaN és nem pozitív érték kiszűrése
        s.calAmp = CAL_AMP_DEF;
        s.calValid = 0;
    }
}
/**
 * @brief A beállítások betöltése az EEPROM-ból.
 *
 * @param s A beállítások struktúrája.
 */
void settingsLoad(Settings &s) {
    eeprom_read_block(&s, (void *)0, sizeof(Settings));
    if (s.magic != SETTINGS_MAGIC || s.version != SETTINGS_VERSION || s.crc != calcCrc(s)) {
        settingsDefaults(s);
    }
    settingsSanitize(s);
}

/**
 * @brief A beállítások mentése az EEPROM-ba.
 *
 * @param s A beállítások struktúrája.
 */
void settingsSave(Settings &s) {
    settingsSanitize(s);
    s.crc = calcCrc(s);
    eeprom_update_block(&s, (void *)0, sizeof(Settings));
}

/**
 * @brief A beállítások egy elemének növelése.
 *
 * @param s    A beállítások struktúrája.
 * @param item Az elem sorszáma.
 */
void settingsAdjust(Settings &s, uint8_t item) {
    switch (item) {
        case SET_PULSE:
            s.pulseUs += 10;
            if (s.pulseUs > PULSE_MAX_US)
                s.pulseUs = PULSE_MIN_US;
            break;
        case SET_FREQ:
            s.freqHz += 50;
            if (s.freqHz > freqLimitHz(s))
                s.freqHz = FREQ_MIN_HZ;
            break;
        case SET_S1:
            s.s1Us += 5;
            if (s.s1Us > S1_MAX_US)
                s.s1Us = S1_MIN_US;
            break;
        case SET_S2:
            // A kis lépések finomhangoláshoz, a nagyobbak a hosszú tartomány gyors bejárásához kellenek.
            s.s2Us += (s.s2Us < 100) ? 5 : (s.s2Us < 300) ? 10 : 20;
            if (s.s2Us > S2_MAX_US)
                s.s2Us = S2_MIN_US;
            break;
        case SET_SENS:
            s.sensIdx = (s.sensIdx + 1) % SENS_COUNT;
            break;
        case SET_VOLUME:
            s.volume = (s.volume + 1) & 3;
            break;
        case SET_GRAPH_MODE:
            s.graphAbs = !s.graphAbs;
            break;
        case SET_CAL_DEPTH:
            s.calDepthMm += 5;
            if (s.calDepthMm > CAL_DEPTH_MAX_MM)
                s.calDepthMm = CAL_DEPTH_MIN_MM;
            s.calValid = 0; // a referencia-mélység változása érvényteleníti a kalibrációt
            s.calAmp = CAL_AMP_DEF;
            break;
        default:
            break;
    }
    settingsSanitize(s);
}

/**
 * @brief A szenzor teljes skálájának lekérdezése.
 * @param idx A szenzor indexe.
 * @return A teljes skála értéke.
 */
uint16_t settingsFullScale(uint8_t idx) {
    if (idx >= SENS_COUNT) {
        idx = SENS_DEF_IDX;
    }
    return pgm_read_word(&kFullScale[idx]);
}

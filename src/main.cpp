/**
 * @file main.cpp
 * @brief Wallscout firmware: PI (impulzus-indukciós) betonvas-kereső, DD tekercs, ATmega328P.
 *
 * Felépítés:
 *  - measure.*  : PI mérés (TX impulzus, késleltetett ADC mintavétel, átlagolás)
 *  - settings.* : beállítások és EEPROM
 *  - buzzer.*   : hangjelzés (Timer2)
 *  - buttons.*  : gombkezelés
 *  - power.*    : mélyalvás
 *  - ui.*       : OLED képernyők
 *  - main.cpp   : állapotgép, jelfeldolgozás, mélységbecslés, soros napló
 *
 * Képernyők:
 *  Kereső csoport : KERESES <-> GRAFIKON (SW1 rövid)
 *  Beállító csoport: DD BEALLITAS <-> BEALLITASOK (SW1 rövid)
 *  SW2 hosszú nyomás: váltás a két csoport között.
 */
#include "buttons.h"
#include "buzzer.h"
#include "config.h"
#include "measure.h"
#include "power.h"
#include "settings.h"
#include "ui.h"
#include <Arduino.h>
#include <math.h>

/** A képernyők azonosítói. */
enum Screen : uint8_t {
    SCR_SEARCH = 0, ///< Keresés (jelerősség, mélység)
    SCR_GRAPH,      ///< Grafikon (jelerősség az idő függvényében)
    SCR_DD,         ///< DD beállítás (nullpont-keresés)
    SCR_SETTINGS    ///< Beállítások
};

/* ------------------------------------------------------------------ */
/* Globális állapot                                                   */
/* ------------------------------------------------------------------ */

static Settings g_set;          ///< Aktuális beállítások
static bool g_setDirty = false; ///< Mentetlen változás van a beállításokban
static Screen g_screen = SCR_SEARCH;
static uint8_t g_selItem = 0; ///< Kijelölt elem a Beállítások képernyőn
static bool g_txTest = false; ///< Folyamatos TX teszt (szkópos beállítás); nem tárolódik
static bool g_redraw = true;  ///< A Beállítások képernyőt újra kell rajzolni

static float g_base1 = 512.0f;        ///< Nullázott alapszint (S1), nyers ADC egység
static float g_base2 = 512.0f;        ///< Nullázott alapszint (S2)
static float g_ddDisplay = 0.0f;      ///< DD képernyőre simított impulzus alatti eltérés
static bool g_ddDisplayReady = false; ///< Első DD minta után lesz érvényes a kijelzési szűrő

static UiData g_ui;             ///< A képernyőknek átadott adatok
static int8_t g_hist[HIST_LEN]; ///< Grafikon gyűrűpuffer (pixel-egységben, előjeles)
static uint8_t g_histHead = 0;  ///< A legrégebbi elem indexe (= a következő írási hely)

static float g_peak = 0.0f;           ///< Csúcs-megtartott |jel|
static uint32_t g_peakMs = 0;         ///< A csúcs időbélyege
static uint16_t g_vbattMv = 0;        ///< Utolsó tápfeszültség mérés
static uint32_t g_lastBattMs = 0;     ///< Utolsó tápfeszültség mérés ideje
static uint32_t g_lastActivityMs = 0; ///< Utolsó aktivitás ideje (alvási időzítő)
static float g_activityRef = 0.0f;    ///< Az aktivitás-detektálás referenciaértéke
static bool g_detected = false;       ///< Találat-állapot (hiszterézissel)
static uint32_t g_lastDrawMs = 0;     ///< A Beállítások képernyő utolsó rajzolása

/* ------------------------------------------------------------------ */
/* Segédfüggvények                                                    */
/* ------------------------------------------------------------------ */

/** @brief Gombok pollozása a kijelzőfrissítés közben (a ui hívja laponként). */
static void pollButtons() { buttonsUpdate(); }

/** @brief A mérés időzítésének újraszámolása az aktuális beállításokból. */
static void applySettings() { measureConfigure(g_set.pulseUs, g_set.freqHz, g_set.s1Us, g_set.s2Us); }

/** @brief Az inaktivitási időzítő nullázása. */
static void markActivity() { g_lastActivityMs = millis(); }

/** @brief Soros napló fejléc. */
static void logHeader() {
#ifdef LOG_ENABLED
    Serial.println(F("# ms,scr,S1raw,S2raw,base1,base2,s1,s2,dd,peak,depth_mm,vbatt_mV,sat"));
#endif
}

/**
 * @brief Egy mérés sorának kiírása a soros naplóba (CSV).
 *        Előjelek: s1 = S1 - base1, s2 = S2 - base2, dd = S1 - S2 (nyers).
 */
static void logMeasurement() {
#ifdef LOG_ENABLED
    Serial.print(millis());
    Serial.print(',');
    Serial.print((uint8_t)g_screen);
    Serial.print(',');
    Serial.print(g_ui.s1Raw, 2);
    Serial.print(',');
    Serial.print(g_ui.s2Raw, 2);
    Serial.print(',');
    Serial.print(g_base1, 2);
    Serial.print(',');
    Serial.print(g_base2, 2);
    Serial.print(',');
    Serial.print(g_ui.strength, 2);
    Serial.print(',');
    Serial.print(g_ui.s1Raw - g_ui.s2Raw - (g_base1 - g_base2), 2);
    Serial.print(',');
    Serial.print(g_ui.s1Raw - g_ui.s2Raw, 2);
    Serial.print(',');
    Serial.print(g_ui.peak, 2);
    Serial.print(',');
    Serial.print(g_ui.depthMm);
    Serial.print(',');
    Serial.print(g_vbattMv);
    Serial.print(',');
    Serial.println(g_ui.saturated ? 1 : 0);
#endif
}

/**
 * @brief Mélységbecslés a jel amplitúdójából: d = dRef * (aRef / a)^(1/n).
 *
 * @param amp A jel amplitúdója (ADC egység, pozitív).
 * @return Mélység mm-ben, vagy DEPTH_NONE / DEPTH_OVER.
 */
static int16_t estimateDepth(float amp) {
    if (!g_detected || amp <= 0.0f) {
        return DEPTH_NONE;
    }

    float d = g_set.calDepthMm * powf(g_set.calAmp / amp, 1.0f / DEPTH_EXPONENT);

    // Ha a becsült mélység nagyobb, mint a maximális, DEPTH_OVER-t adunk vissza.
    if (d > DEPTH_MAX_MM) {
        return DEPTH_OVER;
    }

    // Ha a becsült mélység kisebb, mint a minimális, 5 mm-re állítjuk.
    if (d < 5.0f) {
        d = 5.0f;
    }

    return (int16_t)lroundf(d);
}

/**
 * @brief Impulzus-sorozat futtatása csomagokban, közben a gombok pollozásával.
 *
 * @param total Az összes impulzus száma (PULSE_CHUNK többszöröse).
 * @param r     Az átlagolt eredmény.
 * @return Igaz, ha az eredmény érvényes.
 */
static bool runBurst(uint16_t total, MeasureResult &r) {

    measureReset();

    //  végigmegyünk az összes impulzuson PULSE_CHUNK-okra bontva.
    for (uint16_t done = 0; done < total; done += PULSE_CHUNK) {
        measureRun(PULSE_CHUNK);
        buttonsUpdate();
    }

    return measureResult(r);
}

/**
 * @brief Kirajzolja az aktuális mérő képernyőt (KERESES vagy GRAFIKON).
 */
static void uiDrawMeasureScreen() {
    if (g_screen == SCR_GRAPH) {
        uiDrawGraph(g_ui);
    } else {
        uiDrawSearch(g_ui);
    }
}

/**
 * @brief Nullázás: az aktuális (fémmentes) jelszint eltárolása alapszintként.
 *        A nullázás alatt az éppen aktív mérő képernyő (KERESES vagy GRAFIKON) frissül,
 *        a hang elnémul, az előzmények törlődnek.
 */
static void zeroNow() {

    // A hang elnémítása a nullázás kezdetén.
    buzzerOff();

    // A régi grafikon-adatok azonnal törlődnek, hogy a nullázás alatt ne látszódjanak.
    memset(g_hist, 0, sizeof(g_hist));
    g_histHead = 0;

    g_ui.strength = 0.0f;
    g_ui.shown = 0.0f;
    g_ui.peak = 0.0f;
    g_ui.fullScale = settingsFullScale(g_set.sensIdx);
    g_ui.depthMm = DEPTH_NONE;
    g_ui.metal = 0;
    g_ui.saturated = false;
    g_ui.vbattMv = g_vbattMv;
    g_ui.hist = g_hist;
    g_ui.histHead = g_histHead;
    g_ui.graphAbs = g_set.graphAbs;

    // A jelzés után a bázist a jelzés nélküli képernyő mellett mérjük.
    g_ui.zeroing = true;
    uiDrawMeasureScreen();
    delay(300);
    g_ui.zeroing = false;

    // Addig mérünk normál ciklusokban, amíg a jel meg nem áll:
    // a legutóbbi ZERO_WINDOW ciklus S1 értékeinek szórása (max-min) ZERO_STABLE_COUNTS alá kerül.
    // A nullpont ennek az ablaknak az átlaga. Ha a jel nem áll meg, ZERO_MAX_CYCLES után is ezt használjuk.
    float w1[ZERO_WINDOW], w2[ZERO_WINDOW];
    uint8_t n = 0; // az ablakban lévő értékek száma
    for (uint8_t i = 0; i < ZERO_MAX_CYCLES; i++) {
        uiDrawMeasureScreen();
        MeasureResult r;

        // Ha a burst sikertelen volt, újra próbálkozunk.
        if (!runBurst(PULSES_PER_CYCLE, r)) {
            continue;
        }

        // Ha a burst sikeres volt, az értékeket az ablakba tesszük.
        if (n == ZERO_WINDOW) { // az ablak léptetése: a legrégebbi érték kiesik
            for (uint8_t k = 1; k < ZERO_WINDOW; k++) {
                w1[k - 1] = w1[k];
                w2[k - 1] = w2[k];
            }
            n--;
        }
        w1[n] = r.s1;
        w2[n] = r.s2;
        n++;
        if (n == ZERO_WINDOW) {
            float lo = w1[0], hi = w1[0];
            for (uint8_t k = 1; k < ZERO_WINDOW; k++) {
                lo = min(lo, w1[k]);
                hi = max(hi, w1[k]);
            }

            // Ha a szórás a kívánt határ alatt van, a jel stabilnak tekinthető.
            if (hi - lo < ZERO_STABLE_COUNTS) {
                break;
            }
        }
    }

    // A nullázás eredménye: az ablakban lévő értékek átlaga lesz az új bázis.
    if (n) {
        float sum1 = 0.0f, sum2 = 0.0f;
        for (uint8_t k = 0; k < n; k++) {
            sum1 += w1[k];
            sum2 += w2[k];
        }
        g_base1 = sum1 / n;
        g_base2 = sum2 / n;
        g_ddDisplayReady = false;
    }
    g_ui.zeroing = false;
    g_peak = 0.0f;
    g_peakMs = millis();
    g_activityRef = 0.0f;
    g_detected = false;
    memset(g_hist, 0, sizeof(g_hist));
    g_histHead = 0;
    markActivity();

#ifdef LOG_ENABLED
    Serial.print(F("# ZERO base1="));
    Serial.print(g_base1, 2);
    Serial.print(F(" base2="));
    Serial.println(g_base2, 2);
#endif
}

/**
 * @brief Mélység-kalibráció: a jelenlegi jelerősséget a referencia-mélységhez rendeli.
 *        Ismert mélységű vas fölött kell végrehajtani.
 */
static void calibrateDepth() {
    const float amp = fabsf(g_ui.strength);
    if (amp < DETECT_MIN_COUNTS * 3.0f) {
        uiMessage(PSTR("Nincs eleg jel"), 0);
        delay(1000);
        return;
    }
    g_set.calAmp = amp;
    g_set.calValid = 1;
    settingsSave(g_set);
    uiMessage(PSTR("KALIBRALVA %dmm"), g_set.calDepthMm);

#ifdef LOG_ENABLED
    Serial.print(F("# CAL amp="));
    Serial.print(amp, 2);
    Serial.print(F(" depth_mm="));
    Serial.println(g_set.calDepthMm);
#endif
    delay(1000);
}

/**
 * @brief Egy mérési ciklus: impulzus-sorozat, jelfeldolgozás, hang, napló, aktivitás-figyelés.
 */
static void measureCycle() {
    MeasureResult r;
    if (!runBurst(PULSES_PER_CYCLE, r)) {
        return;
    }
    const uint32_t now = millis();

    // Nullázott S1 jel.
    const float s1 = r.s1 - g_base1;
    const float amp = fabsf(s1);
    const uint16_t fs = settingsFullScale(g_set.sensIdx);

    // Csúcs-megtartás: PEAK_HOLD_MS-ig tartja, utána exponenciálisan lecseng az aktuális érték felé.
    static uint32_t lastCycleMs = 0;
    const uint32_t dt = now - lastCycleMs;
    lastCycleMs = now;
    if (amp >= g_peak) {
        g_peak = amp;
        g_peakMs = now;
    } else if ((now - g_peakMs) > PEAK_HOLD_MS) {
        float k = (float)dt / (float)PEAK_DECAY_MS;
        if (k > 1.0f)
            k = 1.0f;
        g_peak += (amp - g_peak) * k;
    }

    // Találat-állapot hiszterézissel: be DETECT_ON_COUNTS fölött, ki DETECT_OFF_COUNTS alatt.
    if (g_detected) {
        if (amp < DETECT_OFF_COUNTS) {
            g_detected = false;
        }
    } else if (amp >= DETECT_ON_COUNTS) {
        g_detected = true;
    }

    // Fém-jelleg becslése az s1 előjeléből (heurisztika).
    uint8_t metal = 0;
    if (g_detected) {
        metal = ((s1 < 0.0f) == FERROUS_S1_NEGATIVE) ? 1 : 2;
    }

    g_ui.strength = s1;

    // Kijelzett érték: a találat-hiszterézis alatt (nyugalomban) 0, így a zaj nem billegteti a számot.
    g_ui.shown = g_detected ? s1 : 0.0f;
    g_ui.peak = g_peak;
    g_ui.fullScale = fs;
    g_ui.s1Raw = r.s1;
    g_ui.s2Raw = r.s2;
    g_ui.depthMm = estimateDepth(amp);
    g_ui.depthCal = g_set.calValid;
    g_ui.metal = metal;
    g_ui.saturated = r.saturated;
    g_ui.vbattMv = g_vbattMv;

    // Grafikon-előzmény: a teljes skála +-24 pixelre képezve.
    // Grafikon-előzmény pixelben: ABS módban 0..GRAPH_ABS_RANGE_PX, SIG módban +-GRAPH_HALF_RANGE_PX.
    float px;
    if (g_set.graphAbs) {
        px = constrain(amp / fs * GRAPH_ABS_RANGE_PX, 0.0f, (float)GRAPH_ABS_RANGE_PX);
    } else {
        px = constrain(s1 / fs * GRAPH_HALF_RANGE_PX, -(float)GRAPH_HALF_RANGE_PX, (float)GRAPH_HALF_RANGE_PX);
    }
    g_hist[g_histHead] = (int8_t)lroundf(px);
    g_histHead = (g_histHead + 1) % HIST_LEN;

    if (g_detected) {
        const float k = constrain(amp / fs, 0.0f, 1.0f);
        buzzerTone((uint16_t)(500.0f + 2500.0f * k), g_set.volume);
    } else {
        buzzerOff();
    }

    if (fabsf(s1 - g_activityRef) > ACTIVITY_DELTA) {
        g_activityRef = s1;
        markActivity();
    }

    logMeasurement();
}

static void measureDdCycle() {
    DdMeasureResult r;
    if (!measureDd(PULSES_PER_CYCLE, r)) {
        return;
    }
    const float ddRaw = r.pulse - r.baseline;
    g_ui.ddRaw = ddRaw;
    g_ui.ddBaseline = r.baseline;
    g_ui.ddPulse = r.pulse;
    g_ui.ddSaturated = r.saturated;
    if (r.saturated) {
        g_ddDisplayReady = false;
        g_ddDisplay = ddRaw;
    } else if (!g_ddDisplayReady) {
        g_ddDisplay = ddRaw;
        g_ddDisplayReady = true;
    } else {
        g_ddDisplay += (ddRaw - g_ddDisplay) * 0.20f;
    }
    g_ui.dd = g_ddDisplay;

    const float soundVal = fabsf(ddRaw);
    if (r.saturated || soundVal >= DD_OK_COUNTS) {
        const float k = r.saturated ? 1.0f : constrain(soundVal / 100.0f, 0.0f, 1.0f);
        buzzerTone((uint16_t)(500.0f + 2500.0f * k), g_set.volume);
    } else {
        buzzerOff();
    }
    if (fabsf(ddRaw - g_activityRef) > ACTIVITY_DELTA) {
        g_activityRef = ddRaw;
        markActivity();
    }
#ifdef LOG_ENABLED
    Serial.print(F("# DD baseline="));
    Serial.print(r.baseline, 2);
    Serial.print(F(" pulse="));
    Serial.print(r.pulse, 2);
    Serial.print(F(" dd="));
    Serial.print(ddRaw, 2);
    Serial.print(F(" sat="));
    Serial.println(r.saturated ? 1 : 0);
#endif
}

/**
 * @brief Az aktuális mérőképernyő kirajzolása.
 */
static void drawMeasureScreen() {
    g_ui.hist = g_hist;
    g_ui.graphAbs = g_set.graphAbs;
    g_ui.histHead = g_histHead;
    switch (g_screen) {
        case SCR_SEARCH:
            uiDrawSearch(g_ui);
            break;
        case SCR_GRAPH:
            uiDrawGraph(g_ui);
            break;
        case SCR_DD:
            uiDrawDD(g_ui);
            break;
        default:
            break;
    }
}

/**
 * @brief Mélyalvás előkészítése, alvás, majd ébredés utáni újrainicializálás és nullázás.
 */
static void goToSleep() {
#ifdef LOG_ENABLED
    Serial.println(F("# SLEEP"));
    Serial.flush();
#endif
    if (g_setDirty) {
        settingsSave(g_set);
        g_setDirty = false;
    }
    buzzerOff();
    uiSleep();
    powerDeepSleep();

    // Ébredés: minden gomb ébreszt; a nyomás nem okoz műveletet, hanem nullázás és a kereső képernyő jön.
    uiWake();
    measureInit();
    applySettings();
    buttonsWaitRelease();
    g_vbattMv = readVbattMv();
    g_lastBattMs = millis();
    g_screen = SCR_SEARCH;
    zeroNow();

#ifdef LOG_ENABLED
    Serial.println(F("# WAKE"));
#endif
}

/**
 * @brief Kilépés a beállító csoportból: mentés (ha kell), új időzítés, nullázás, kereső képernyő.
 */
static void leaveSetupGroup() {
    if (g_setDirty) {
        settingsSave(g_set);
        g_setDirty = false;
    }
    applySettings();
    g_screen = SCR_SEARCH;
    zeroNow();
}

/**
 * @brief A gombok eseményeinek feldolgozása a képernyő-állapotgép szerint.
 *
 * - SW2 hosszú: váltás a kereső és a beállító csoport között.
 * - SW1 rövid: képernyőváltás a csoporton belül.
 * - SW2 rövid: kereső csoportban újranullázás, Beállításokban a következő elem.
 * - SW1 hosszú: kereső csoportban mélység-kalibráció, Beállításokban az érték léptetése.
 */
static void handleButtons() {
    const ButtonEvent e1 = buttonEvent(BTN_SW1);
    const ButtonEvent e2 = buttonEvent(BTN_SW2);

    if (e1 == EV_NONE && e2 == EV_NONE) {
        return;
    }

    markActivity();

    // SW2 hosszú: váltás a kereső és a beállító csoport között.
    if (e2 == EV_LONG) {
        if (g_screen == SCR_DD || g_screen == SCR_SETTINGS) {
            leaveSetupGroup();
        } else {
            buzzerOff();
            g_screen = SCR_DD;
            g_ddDisplayReady = false;
            g_redraw = true;
        }
        return;
    }

    // SW1 rövid: képernyőváltás a csoporton belül.
    if (e1 == EV_SHORT) {
        switch (g_screen) {
            case SCR_SEARCH:
                g_screen = SCR_GRAPH;
                zeroNow(); // a grafikon más kijelzőterhelése miatt ott újra kell nullázni
                break;
            case SCR_GRAPH:
                g_screen = SCR_SEARCH;
                zeroNow();
                break;
            case SCR_DD:
                buzzerOff();
                g_screen = SCR_SETTINGS;
                break;
            case SCR_SETTINGS:
                g_screen = SCR_DD;
                g_ddDisplayReady = false;
                break;
        }
        g_redraw = true;
        return;
    }

    // SW2 rövid: kereső csoportban újranullázás, Beállításokban a következő elem.
    if (e2 == EV_SHORT) {
        if (g_screen == SCR_SETTINGS) {
            g_selItem = (g_selItem + 1) % SET_COUNT;
            g_redraw = true;
        } else if (g_screen == SCR_SEARCH || g_screen == SCR_GRAPH) {
            zeroNow();
        }
        return;
    }

    // SW1 hosszú: kereső csoportban mélység-kalibráció, Beállításokban az érték léptetése.
    if (e1 == EV_LONG || e1 == EV_REPEAT) {

        // SW1 hosszú vagy ismételt: kereső csoportban mélység-kalibráció, Beállításokban az érték léptetése.
        if (g_screen == SCR_SETTINGS) {
            // Beállítások csoportban az aktuális elem módosítása.
            if (g_selItem == SET_TX_TEST) {
                // Csak az első hosszú nyomás vált (ismétlés nem), hogy ne villogjon ki-be.
                if (e1 == EV_LONG) {
                    g_txTest = !g_txTest;
                    g_redraw = true;
                }
            } else if (g_selItem == SET_DEFAULTS) {
                if (e1 == EV_LONG) {
                    settingsDefaults(g_set);
                    settingsSave(g_set);
                    g_setDirty = false;
                    applySettings();
                    buzzerOff();
                    uiMessage(PSTR("GYARI ERTEKEK"), 0);
                    delay(1200);
                    g_screen = SCR_SEARCH;
                    zeroNow();
                }
            } else {
                settingsAdjust(g_set, g_selItem);
                if (g_selItem == SET_GRAPH_MODE || g_selItem == SET_SENS) {
                    // A régi előzmények más pixel-skálán készültek, ezért törlődnek.
                    memset(g_hist, 0, sizeof(g_hist));
                    g_histHead = 0;
                }
                g_setDirty = true;
                applySettings();
                g_redraw = true;
            }
#ifdef LOG_ENABLED
            Serial.print(F("# SET pulse="));
            Serial.print(g_set.pulseUs);
            Serial.print(F(" freq="));
            Serial.print(g_set.freqHz);
            Serial.print(F(" s1="));
            Serial.print(g_set.s1Us);
            Serial.print(F(" s2="));
            Serial.print(g_set.s2Us);
            Serial.print(F(" sens="));
            Serial.print(g_set.sensIdx);
            Serial.print(F(" vol="));
            Serial.println(g_set.volume);
#endif
        } else if ((g_screen == SCR_SEARCH || g_screen == SCR_GRAPH) && e1 == EV_LONG) {
            calibrateDepth();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Arduino belépési pontok                                            */
/* ------------------------------------------------------------------ */

/** @brief Inicializálás bekapcsoláskor: hardver, beállítások, kijelző, nullázás. */
void setup() {
    // Használaton kívüli lábak felhúzása; PB5 (D13 / LED_BUILTIN) kivétel, hogy a Nano beépített LED ne világítson.
    PORTB |= 0x1F;                // PB0-PB4 felhúzása
    PORTC |= _BV(PC2) | _BV(PC3); // PC2-PC3 felhúzása
    PORTD |= _BV(PD6) | _BV(PD7); // PD6-PD7 felhúzása

#ifdef LOG_ENABLED
    Serial.begin(LOG_BAUD);
    Serial.println(F("# Wallscout PI fémkereső indul"));
#endif

    measureInit();              // a TX lábat azonnal alacsonyra állítja
    buzzerInit();               // Buzzer inicializálása
    buttonsInit();              // Gombok inicializálása
    settingsLoad(g_set);        // Beállítások betöltése
    applySettings();            // Beállítások alkalmazása
    uiSetPollHook(pollButtons); // UI poll hook beállítása a gombokhoz
    uiInit();                   // UI inicializálása

    g_vbattMv = readVbattMv(); // Akkumulátor feszültség olvasása
    g_lastBattMs = millis();
    uiMessage(PSTR("WALLSCOUT v%d"), 1);
    delay(800);
    buttonsWaitRelease(); // Várakozás, amíg minden gomb felengedésre kerül

    logHeader(); // Napló fejléc kiírása
    g_ui.vbattMv = g_vbattMv;
    g_screen = SCR_SEARCH;
    zeroNow();
}

/**
 * @brief Főciklus: gombok, tápfeszültség, mérés, kijelző, alvás-időzítő.
 */
void loop() {

    buttonsUpdate(); // Gombok állapotának frissítése
    handleButtons(); // Gombok eseményeinek kezelése

    // A folyamatos TX teszt csak a Beállításokban él (menüelemek közt lépkedve is); onnan kilépve leáll.
    if (g_screen != SCR_SETTINGS) {
        g_txTest = false;
    }
    const uint32_t now = millis();
    if (now - g_lastBattMs >= VBATT_PERIOD_MS) {
        g_vbattMv = readVbattMv(); // Akkumulátor feszültség olvasása
        g_lastBattMs = now;
    }
    g_ui.vbattMv = g_vbattMv; // UI frissítése az aktuális feszültséggel

    // UI frissítése az aktuális állapotnak megfelelően
    if (g_screen == SCR_SETTINGS) {
        // A Beállításokban nincs mérés (a TX ki van kapcsolva); csak változáskor / villogáshoz rajzolunk.
        // Folyamatos TX tesztnél a rajzolás szünetet okozna az impulzusokban, ezért csak változáskor frissítünk.
        if (g_redraw || (!g_txTest && (now - g_lastDrawMs) > 500)) {
            uiDrawSettings(g_set, g_selItem, g_vbattMv, g_txTest);
            g_lastDrawMs = millis();
            g_redraw = false;
        }
        if (g_txTest) {
            measureTxStream(); // gombnyomásig megszakítás nélküli impulzussor
        } else {
            delay(5);
        }
    } else {
        if (g_screen == SCR_DD) {
            measureDdCycle();
        } else {
            measureCycle();
        }
        drawMeasureScreen(); // Mérési képernyő kirajzolása
        g_redraw = false;
    }

    // Alvás-időzítő ellenőrzése
    if (millis() - g_lastActivityMs >= SLEEP_TIMEOUT_MS) {
        goToSleep();
    }
}

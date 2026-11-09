/**
 * @file config.h
 * @brief A firmware központi konfigurációja: lábkiosztás, időzítések, küszöbök.
 *
 * Itt található minden hardverhez kötött és hangolható konstans, hogy a többi
 * modulban ne legyenek "mágikus számok".
 */
#pragma once
#include <Arduino.h>

/* ------------------------------------------------------------------ */
/* Általános                                                          */
/* ------------------------------------------------------------------ */

/** Soros napló engedélyezése */
// #define LOG_ENABLED

/** Soros port sebessége (baud). */
#define LOG_BAUD 115200

/* ------------------------------------------------------------------ */
/* Lábkiosztás (a kapcsolási rajz alapján)                            */
/* ------------------------------------------------------------------ */

/** Adó (TX) MOSFET kapuja: PD2 (D2/Tx). Aktív magas (R16 = 100R sorosan, R17 = 10k lehúzó). */
#define TX_PORT PORTD // TX kimenet portja
#define TX_DDR DDRD   // TX kimenet irányítása
#define TX_BIT PD2    // TX kimenet bitje

/** Passzív buzzer meghajtó tranzisztor (Q1) bázisa: PD3 = OC2B (Timer2 PWM kimenet). */
#define BUZZER_DDR DDRD   // Buzzer kimenet irányítása
#define BUZZER_PORT PORTD // Buzzer kimenet portja
#define BUZZER_BIT PD3    // Buzzer kimenet bitje

/** Gombok (aktív alacsony, belső felhúzással): SW1 "Kalibrálás" = PD5, SW2 "Nullázás" = PD4. */
#define BTN_PIN PIND    // Gombok bemeneti portja
#define BTN_PORT PORTD  // Gombok kimeneti portja (felhúzás)
#define BTN_DDR DDRD    // Gombok irányítása
#define BTN_SW1_BIT PD5 // SW1 bitje
#define BTN_SW2_BIT PD4 // SW2 bitje

/** ADC csatornák: A0 = vevőjel (Rx, a Vmid = Vcc/2 körül), A1 = tápfeszültség osztó. */
#define ADC_CH_RX 0    // ADC csatorna a vevőjelhez (A0)
#define ADC_CH_VBATT 1 // ADC csatorna a tápfeszültség osztóhoz (A1)

/* ------------------------------------------------------------------ */
/* ADC és időzítés                                                    */
/* ------------------------------------------------------------------ */

/**
 * ADC előosztó. 16 -> 1 MHz ADC órajel, egy konverzió 13 us.
 * (A névleges 50..200 kHz-nél gyorsabb, a pontosság kicsit romlik, ezt az átlagolás ellensúlyozza.)
 * Érvényes értékek: 2, 4, 8, 16, 32, 64, 128. A hozzá tartozó ADPS biteket az ADC_PS_BITS adja.
 */
#define ADC_PRESCALER 16       // ADC előosztó
#define ADC_PS_BITS _BV(ADPS2) // ADPS bitek az előosztóhoz

/** Timer1 tick-ek száma 1 us alatt (Timer1 előosztó = 1, így 1 tick = 1 CPU ciklus = 62,5 ns). */
#define TICKS_PER_US (F_CPU / 1000000UL)
/** A konverzió indítása és a tényleges mintavétel (S&H) közti idő: 1,5 ADC órajel (CPU ciklusban). */
#define ADC_SAMPLE_TICKS ((3 * ADC_PRESCALER) / 2)
/** Egy ADC konverzió ideje us-ban (13 ADC órajel). */
#define ADC_CONV_US ((13 * ADC_PRESCALER) / 16)
/** Az S1 és S2 minta közti minimális távolság (us): konverzióidő + biztonsági tartalék. */
#define MIN_S_GAP_US (ADC_CONV_US + 3)

/** Egy mérési ciklusban átlagolt impulzusok száma. */
#define PULSES_PER_CYCLE 32
/** Nullázáskor a stabilitás-vizsgálat ablaka (ciklusok száma); a nullpont ennek az átlaga. */
#define ZERO_WINDOW 6
/** Az ablakban az S1 értékek max-min különbsége ennél kisebb (ADC egység) → a jel megállt. */
#define ZERO_STABLE_COUNTS 1.6f
/** A nullázás legfeljebb ennyi mérési ciklusig tart (ha a jel nem áll meg). */
#define ZERO_MAX_CYCLES 40
/** Egyszerre lefuttatott impulzusok száma (két csomag között a gombokat is pollozzuk). */
#define PULSE_CHUNK 8

/** ADC telítés-detektálás: ennél kisebb / nagyobb nyers érték telítésnek számít. */
#define ADC_SAT_LOW 4
#define ADC_SAT_HIGH 1019

/* ------------------------------------------------------------------ */
/* Állítható paraméterek határai és alapértékei                       */
/* ------------------------------------------------------------------ */

#define PULSE_MIN_US 20  // Minimális impulzusidő us-ban
#define PULSE_MAX_US 150 // Maximális impulzusidő us-ban
#define PULSE_DEF_US 60  // Alapértelmezett impulzusidő us-ban

#define FREQ_MIN_HZ 250  // Minimális frekvencia Hz-ben
#define FREQ_MAX_HZ 2500 // Maximális frekvencia Hz-ben
#define FREQ_DEF_HZ 500  // Alapértelmezett frekvencia Hz-ben

#define S1_MIN_US 40  // Minimális S1 késleltetés us-ban (a tekercsáram lecsengése után; lásd README 2.1)
#define S1_MAX_US 120 // Maximális S1 késleltetés us-ban
#define S1_DEF_US 40  // Alapértelmezett S1 késleltetés us-ban

#define S2_MIN_US 60  // Minimális S2 késleltetés us-ban (legalább S1_MIN_US + MIN_S_GAP_US)
#define S2_MAX_US 800 // Maximális S2 késleltetés us-ban
#define S2_DEF_US 60  // Alapértelmezett S2 késleltetés us-ban

/** Biztonsági tartalék (us) a periódus végén: a frekvencia felső határa az impulzus + S2 + konverzió + ennyi alapján adódik. */
#define PERIOD_MARGIN_US 10

/** Az érzékenység-fokozatok száma (a fullscale értékek a settings.cpp-ben). */
#define SENS_COUNT 6   // Az érzékenység-fokozatok száma
#define SENS_DEF_IDX 2 // Alapértelmezett érzékenység-fokozat indexe

#define CAL_DEPTH_MIN_MM 10  // Minimális kalibrációs mélység mm-ben
#define CAL_DEPTH_MAX_MM 100 // Maximális kalibrációs mélység mm-ben
#define CAL_DEPTH_DEF_MM 30  // Alapértelmezett kalibrációs mélység mm-ben
#define CAL_AMP_DEF 60.0f    // Kalibráció nélküli alapértelmezett referencia-amplitúdó (ADC egység) a CAL_DEPTH_DEF_MM mélységhez

/* ------------------------------------------------------------------ */
/* Jelfeldolgozás                                                     */
/* ------------------------------------------------------------------ */

/** Ennél kisebb |jel| (ADC egység, nullázás után) zajnak számít; a kalibráció minimális jelszintjének alapja. */
#define DETECT_MIN_COUNTS 3.0f
/** Találat bekapcsolási küszöbe (ADC egység). */
#define DETECT_ON_COUNTS 4.0f
/** Találat kikapcsolási küszöbe (ADC egység); a két küszöb közti sáv a hiszterézis, ami megakadályozza a billegést. */
#define DETECT_OFF_COUNTS 2.5f
/** A vas jelének előjele: true = negatív S1, false = pozitív S1; az ellenkező előjel színesfém. */
#define FERROUS_S1_NEGATIVE true
/** A mélységbecslés modelljének kitevője: amplitúdó ~ 1/mélység^n. */
#define DEPTH_EXPONENT 3.0f
/** A becsült mélység felső határa (mm); efölött ">max" jelenik meg. */
#define DEPTH_MAX_MM 150
/** A csúcs-megtartás ideje (ms), utána a csúcs exponenciálisan lecseng az aktuális érték felé. */
#define PEAK_HOLD_MS 500UL
/** A csúcs lecsengésének időállandója (ms): ennyi idő alatt csökken a különbség ~63 %-ára. */
#define PEAK_DECAY_MS 1000UL
/** DD képernyőn ennyi eltérés (ADC egység) alatt "OK" a nullpont. */
#define DD_OK_COUNTS 2.0f

/* ------------------------------------------------------------------ */
/* Tápfeszültség mérés                                                */
/* ------------------------------------------------------------------ */

/** Az osztó: R11 = 100k (felső), R12 = 33k (alsó) -> Vin = Vadc * 133 / 33. */
#define VBATT_DIV_NUM 133UL
#define VBATT_DIV_DEN 33UL
/** Az ADC referencia (AVcc, a 7805 kimenete) mV-ban. Pontosítható multiméteres méréssel. */
#define VREF_MV 5000UL
/** Utólagos korrekció mV-ban (pl. a bemeneti dióda esése, ha a kijelzett érték eltér). */
#define VBATT_OFFSET_MV 0
/** Ennél kisebb tápfeszültségnél (mV) alacsony elem figyelmeztetés. */
#define VBATT_LOW_MV 7000
/** A tápfeszültség mérésének periódusa (ms). */
#define VBATT_PERIOD_MS 500UL

/* ------------------------------------------------------------------ */
/* Gombok és alvás                                                    */
/* ------------------------------------------------------------------ */

#define BTN_DEBOUNCE_MS 25UL
/** Ennyi ideig tartó nyomás hosszú nyomásnak számít. */
#define BTN_LONG_MS 700UL
/** Hosszú nyomás alatt ennyi ms-onként ismétlődik az esemény. (ennyi időnként változik a menüben a menüelem értéke) */
#define BTN_REPEAT_MS 500UL

/** Inaktivitás ideje (ms) mélyalvásig: 5 perc. */
#define SLEEP_TIMEOUT_MS 300000UL
/** Ekkora jelváltozás (ADC egység) aktivitásnak számít. */
#define ACTIVITY_DELTA 8.0f

/** A grafikon előzményeinek hossza (pixel / minta). */
#define HIST_LEN 128
/** A grafikon függőleges kitérése a nullvonaltól (pixel): a teljes skála ennyi pixelnek felel meg. */
#define GRAPH_HALF_RANGE_PX 24
/** Abszolút értékes (ABS) módban a nullvonal ezen a soron van (a kijelző legalsó sora). */
#define GRAPH_ABS_ZERO_Y 63
/** ABS módban a teljes skála ennyi pixelnek felel meg (a felső sor alatti 14. sorig). */
#define GRAPH_ABS_RANGE_PX 49

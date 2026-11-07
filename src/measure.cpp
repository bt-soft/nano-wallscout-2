/**
 * @file measure.cpp
 * @brief PI mérés megvalósítása közvetlen regiszterprogramozással.
 *
 * Egy impulzus időrendje (a Timer1 a TX bekapcsolásakor nullázódik):
 *
 *   t = 0            TX BE (MOSFET vezet, a tekercs árama nő)
 *   t = tp           TX KI (a tekercs árama megszakad, a lecsengés indul)
 *   t = tp + tS1      S1 mintavétel (korai minta)
 *   t = tp + tS2      S2 mintavétel (késői minta)
 *   t = period       következő impulzus
 *
 * Az ADC a konverzió indítása után 1,5 ADC órajellel mintavételez, ezért a konverziót
 * ennyivel korábban indítjuk (ADC_SAMPLE_TICKS), hogy a tényleges mintavétel pontosan
 * a kívánt késleltetésnél történjen.
 */
#include "measure.h"
#include "config.h"
#include "buttons.h"

/** Az impulzus hossza tick-ben. */
static uint16_t g_tpTicks;
/** Az S1 konverzió indításának ideje a Timer1 nullázásától (tick). */
static uint16_t g_t1Ticks;
/** Az S2 konverzió indításának ideje (tick). */
static uint16_t g_t2Ticks;
/** Az impulzus-periódus (tick). */
static uint16_t g_periodTicks;

/** Akkumulátorok az átlagoláshoz. */
static uint32_t g_sum1;
static uint32_t g_sum2;
static uint16_t g_count;
static uint16_t g_satCount;

/**
 * @brief Inicializálja a mérést: beállítja a TX lábat kimenetként, az ADC-t és a Timer1-et.
 */
void measureInit() {
    // TX láb kimenet, alaphelyzetben ALACSONY (a MOSFET zárva).
    TX_PORT &= ~_BV(TX_BIT);
    TX_DDR |= _BV(TX_BIT);

    // Az analóg bemenetek digitális bemeneti puffere felesleges (zaj, fogyasztás).
    DIDR0 |= _BV(ADC0D) | _BV(ADC1D);

    // ADC: referencia AVcc, A0 csatorna, engedélyezve, gyorsított előosztó.
    ADMUX = _BV(REFS0) | ADC_CH_RX;
    ADCSRA = _BV(ADEN) | ADC_PS_BITS;

    // Timer1: normál mód, előosztó 1 (az Arduino init() PWM módját felülírjuk). Nincs megszakítás.
    TCCR1A = 0;
    TCCR1B = _BV(CS10);
    TIMSK1 = 0;

    // Az első konverzió hosszabb (25 ADC órajel), ezt "eldobjuk".
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
}

/**
 * @brief Konfigurálja az impulzus hosszát, frekvenciáját és a mintavételek időzítését.
 * @param pulseUs Az impulzus hossza mikrosekundumban.
 * @param freqHz Az impulzus frekvenciája hertzben.
 * @param s1Us Az S1 mintavétel késleltetése mikrosekundumban a TX kikapcsolása után.
 * @param s2Us Az S2 mintavétel késleltetése mikrosekundumban a TX kikapcsolása után.
 */
void measureConfigure(uint8_t pulseUs, uint16_t freqHz, uint16_t s1Us, uint16_t s2Us) {
    g_tpTicks = (uint16_t)pulseUs * TICKS_PER_US;
    g_t1Ticks = g_tpTicks + (uint16_t)s1Us * TICKS_PER_US - ADC_SAMPLE_TICKS;
    g_t2Ticks = g_tpTicks + (uint16_t)s2Us * TICKS_PER_US - ADC_SAMPLE_TICKS;
    g_periodTicks = (uint16_t)(F_CPU / freqHz); // freqHz >= 250 -> legfeljebb 64000 tick
}

/**
 * @brief Visszaállítja az akkumulátorok átlagolásához használt változókat.
 */
void measureReset() {
    g_sum1 = 0;
    g_sum2 = 0;
    g_count = 0;
    g_satCount = 0;
}

/**
 * @brief Egyetlen impulzus és a két mintavétel.
 * A hívás alatt a megszakítások tiltottak.
 *
 * @param s1 Ide kerül a korai minta (0..1023).
 * @param s2 Ide kerül a késői minta (0..1023).
 */
static inline void pulseOnce(uint16_t &s1, uint16_t &s2) {
    const uint8_t sreg = SREG;
    cli(); // a Timer0 (millis) megszakítás ne zavarja az időzítést

    // Timer1 számláló nullázása előtt várakozás a periódus végéig, ha szükséges
    TCNT1 = 0;
    // Timer1 számláló nullázása
    TX_PORT |= _BV(TX_BIT); // TX be
    // Várakozás az impulzus hosszáig
    while (TCNT1 < g_tpTicks) {
    }
    // Várakozás az S1 mintavétel idejéig
    TX_PORT &= ~_BV(TX_BIT); // TX ki

    // Várakozás az S1 mintavétel idejéig a TX kikapcsolása után
    while (TCNT1 < g_t1Ticks) {
    }

    ADCSRA |= _BV(ADSC); // S1 konverzió indítása
    // Várakozás az S1 konverzió befejezéséig
    while (ADCSRA & _BV(ADSC)) {
    }
    s1 = ADC;

    // Várakozás az S2 mintavétel idejéig a TX kikapcsolása után
    while (TCNT1 < g_t2Ticks) {
    }

    ADCSRA |= _BV(ADSC); // S2 konverzió indítása
    // Várakozás az S2 konverzió befejezéséig
    while (ADCSRA & _BV(ADSC)) {
    }
    s2 = ADC;

    SREG = sreg;
}

/**
 * @brief Egy sorozat impulzus és mintavétel végrehajtása.
 *
 * @param pulses Az impulzusok száma.
 */
void measureRun(uint8_t pulses) {

    // Végrehajtjuk a sorozat impulzusait és mintavételeit.
    for (uint8_t i = 0; i < pulses; i++) {
        uint16_t a, b;
        pulseOnce(a, b);
        g_sum1 += a;
        g_sum2 += b;
        g_count++;
        if (a <= ADC_SAT_LOW || a >= ADC_SAT_HIGH || b <= ADC_SAT_LOW || b >= ADC_SAT_HIGH) {
            g_satCount++;
        }

        // Várakozás a periódus végéig (itt a megszakítások engedélyezettek, a Timer0 futhat).
        while (TCNT1 < g_periodTicks) {
        }
    }
}

/**
 * @brief Folyamatos (csomagolás nélküli) TX impulzussor a tekercs csillapításának szkópos beállításához.
 *
 * A beállított impulzushosszal és frekvenciával szakadatlanul adja az impulzusokat (nincs mintavétel),
 * így a szkóp stabilan ráhangolható. A függvény addig fut, amíg valamelyik gombnak új eseménye nem
 * keletkezik (rövid/hosszú/ismétlés); ekkor visszatér, hogy a főciklus feldolgozza, majd újra hívható.
 * Az impulzus ideje alatt a megszakítások tiltottak (pontos szélesség), a periódus hátralévő
 * részében engedélyezettek (ekkor fut a gombkezelés).
 */
void measureTxStream() {
    while (true) {
        const uint8_t sreg = SREG;
        cli();
        TCNT1 = 0;
        TX_PORT |= _BV(TX_BIT); // TX be
        while (TCNT1 < g_tpTicks) {
        }
        TX_PORT &= ~_BV(TX_BIT); // TX ki
        SREG = sreg;

        while (TCNT1 < g_periodTicks) {
        }
        buttonsUpdate();
        if (buttonsEventPending()) {
            return;
        }
    }
}

/**
 * @brief A mérés eredményének lekérése.
 *
 * @param r A mérés eredménye.
 * @return true, ha van érvényes mérési eredmény.
 * @return false, ha nincs érvényes mérési eredmény.
 */
bool measureResult(MeasureResult &r) {
    if (g_count == 0) {
        return false;
    }

    // Átlagok számítása és a telítettség ellenőrzése.
    r.s1 = (float)g_sum1 / g_count;
    r.s2 = (float)g_sum2 / g_count;
    r.saturated = g_satCount > (g_count / 4);

    return true;
}

/**
 * @brief Az akkumulátor feszültségének mérése millivoltban.
 *
 * @return uint16_t Az akkumulátor feszültsége millivoltban.
 */
uint16_t readVbattMv() {
    const uint8_t oldAdmux = ADMUX;
    const uint8_t oldAdcsra = ADCSRA;
    // Az ADC beállításainak mentése a VBATT méréshez.
    // Az osztó forrásimpedanciája nagy (~25k), ezért lassú ADC órajel (előosztó 128) kell.
    ADMUX = _BV(REFS0) | ADC_CH_VBATT;
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
    delayMicroseconds(100); // a csatornaváltás utáni beállás

    ADCSRA |= _BV(ADSC); // eldobott konverzió
    // Várakozás az eldobott konverzió befejezéséig
    while (ADCSRA & _BV(ADSC)) {
    }

    // A tényleges mérés
    uint16_t sum = 0;
    // 8-szoros átlagolás a zaj csökkentésére.
    for (uint8_t i = 0; i < 8; i++) {
        ADCSRA |= _BV(ADSC);
        while (ADCSRA & _BV(ADSC)) {
        }
        sum += ADC;
    }

    ADMUX = oldAdmux; // vissza az Rx csatornára és a gyors órajelre
    ADCSRA = oldAdcsra;
    ADCSRA |= _BV(ADSC); // csatornaváltás utáni eldobott konverzió
    // Várakozás az eldobott konverzió befejezéséig
    while (ADCSRA & _BV(ADSC)) {
    }

    // Az átlagolt ADC érték millivoltba konvertálása a feszültségosztó figyelembevételével.
    const uint32_t avg = sum / 8;
    const int32_t mv = (int32_t)((avg * VREF_MV * VBATT_DIV_NUM) / (1024UL * VBATT_DIV_DEN)) + VBATT_OFFSET_MV;
    return (uint16_t)(mv < 0 ? 0 : mv);
}

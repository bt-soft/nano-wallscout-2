/**
 * @file power.cpp
 * @brief Power-down alvás megvalósítása, ébresztés a gombokhoz tartozó PCINT20/21 megszakítással.
 */
#include "power.h"
#include "config.h"
#include <avr/interrupt.h>
#include <avr/sleep.h>

/**
 * Pin-change megszakítás kezelő. Üres: csak az alvásból való ébresztés a feladata.
 * A gombok (PD4, PD5) a PCINT2 csoportba (PCINT16..23) tartoznak.
 */
ISR(PCINT2_vect) {}

/**
 * @brief Mélyalvásba helyezi a rendszert, ébresztés a gombok lenyomására.
 *
 * Ha valamelyik gomb már lenyomott, nem lép alvásba.
 */
void powerDeepSleep() {

    // Ha valamelyik gomb már lenyomott, nincs mi ébresszen: ne aludjunk el.
    if ((BTN_PIN & (_BV(BTN_SW1_BIT) | _BV(BTN_SW2_BIT))) != (_BV(BTN_SW1_BIT) | _BV(BTN_SW2_BIT))) {
        return;
    }

    ADCSRA &= ~_BV(ADEN);    // ADC ki (fogyasztás)
    TX_PORT &= ~_BV(TX_BIT); // TX kikapcsolása (fogyasztás)

    PCMSK2 |= _BV(PCINT20) | _BV(PCINT21); // csak a két gomb ébreszthet
    PCIFR |= _BV(PCIF2);                   // függő jelző törlése
    PCICR |= _BV(PCIE2);                   // PCINT2 csoport engedélyezése

    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // mélyalvás mód beállítása
    cli();                               // globális megszakítások tiltása
    sleep_enable();                      // alvás engedélyezése
    sleep_bod_disable();                 // a Brown-out detektor kikapcsolása az alvás idejére
    sei();                               // az sei() után közvetlenül még lefut a sleep_cpu() -> nincs versenyhelyzet
    sleep_cpu();                         // alvásba lépés
    sleep_disable();                     // alvás tiltása ébredés után

    PCICR &= ~_BV(PCIE2); // ébredés után a PCINT2 csoport megszakítás már nem kell, a gombokat pollozzuk
    delay(5);             // az oszcillátor stabilizálódása
}

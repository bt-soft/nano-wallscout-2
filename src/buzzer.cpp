/**
 * @file buzzer.cpp
 * @brief Timer2 Fast PWM (7. mód, TOP = OCR2A) alapú hangkeltés.
 *
 * Előosztó 256 -> a Timer2 órajele 62,5 kHz. A hang frekvenciája: f = 62500 / (OCR2A + 1).
 * A kitöltési tényezőt (OCR2B / (OCR2A + 1)) a hangerő szabja meg: a passzív buzzer
 * kisebb kitöltésnél halkabban szól.
 */
#include "buzzer.h"
#include "config.h"

/**
 * A hangerő-fokozatokhoz tartozó kitöltési tényező százalékban (0 = néma).
 */
static const uint8_t kDutyPercent[4] = {0, 6, 20, 50};

/**
 * @brief Buzzer inicializálása.
 *
 * Beállítja a buzzer lábát kimenetként, és biztosítja, hogy kezdetben ne legyen áram a tranzisztoron keresztül.
 */
void buzzerInit() {
    BUZZER_DDR |= _BV(BUZZER_BIT);
    BUZZER_PORT &= ~_BV(BUZZER_BIT);
}

/**
 * @brief Buzzer kikapcsolása.
 *
 * Leválasztja a Timer2 OC2B kimenetét, leállítja a Timert, és biztosítja, hogy a tranzisztoron keresztül ne folyjon áram.
 */
void buzzerOff() {
    TCCR2A = 0;                      // OC2B leválasztva a lábról
    TCCR2B = 0;                      // Timer2 leáll
    BUZZER_PORT &= ~_BV(BUZZER_BIT); // a tranzisztor zárva, nincs áram
}

/**
 * @brief Buzzer bekapcsolása adott frekvenciával és hangerővel.
 *
 * @param hz A hang frekvenciája hertzben.
 * @param volume A hangerő fokozata (0 = néma, 1-3 = fokozatok).
 */
void buzzerTone(uint16_t hz, uint8_t volume) {
    if (volume == 0 || hz == 0) {
        buzzerOff();
        return;
    }

    // Ha a hangerő nagyobb, mint a maximális fokozat, állítsuk a legnagyobbra.
    if (volume > 3) {
        volume = 3;
    }

    // TOP érték: 62500/hz - 1, 8 bitre korlátozva (kb. 245 Hz alatt nem szólaltatható meg).
    uint16_t top = (uint16_t)(62500UL / hz);
    if (top < 2) {
        top = 2;
    }

    top -= 1;
    if (top > 255) {
        top = 255;
    }

    uint8_t duty = (uint8_t)(((uint16_t)(top + 1) * kDutyPercent[volume]) / 100);

    if (duty < 1) {
        duty = 1;
    }

    OCR2A = (uint8_t)top; // beállítjuk a TOP értéket
    OCR2B = duty;         // beállítjuk a kitöltési tényezőt

    // Fast PWM, TOP = OCR2A (WGM22:0 = 111), OC2B nem invertált kimenet, előosztó 256.
    TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS22) | _BV(CS21);
}

/**
 * @file buttons.cpp
 *
 * @brief Gombkezelés pollozással (nem használ megszakítást).
 */
#include "buttons.h"
#include "config.h"

/**
 * Egy gomb belső állapota.
 */
struct BtnState {
    uint8_t mask;          ///< A gomb bitmaszkja a BTN_PIN regiszterben
    bool raw;              ///< Utolsó nyers állapot (true = nyomva)
    bool stable;           ///< Pergésmentesített állapot
    bool longFired;        ///< A hosszú nyomás eseménye már elsült
    uint32_t rawChangedMs; ///< A nyers állapot utolsó változásának ideje
    uint32_t pressedMs;    ///< A lenyomás ideje
    uint32_t lastRepeatMs; ///< Az utolsó hosszú/ismétlés esemény ideje
    ButtonEvent pending;   ///< Még le nem kért esemény
};

/**
 * @brief Gombok belső állapotának tárolása.
 */
static BtnState g_btn[2];

/**
 * @brief Nyers állapot beolvasása.
 *
 * @param b A gomb állapot-struktúrája.
 * @return Igaz, ha a gomb nyomva van (a láb alacsony).
 */
static inline bool readRaw(const BtnState &b) { return (BTN_PIN & b.mask) == 0; }

/**
 * @brief Állapotgépek nullázása az aktuális nyers állapotból.
 *
 * Ez a függvény beállítja a gombok nyers és stabil állapotát, valamint
 * az összes időzítőt az aktuális időre. Használat után a gombok
 * állapota "frissen" kezdődik, mintha éppen most lettek volna inicializálva.
 */
static void resetStates() {
    const uint32_t now = millis();
    g_btn[BTN_SW1].mask = _BV(BTN_SW1_BIT);
    g_btn[BTN_SW2].mask = _BV(BTN_SW2_BIT);
    for (uint8_t i = 0; i < 2; i++) {
        g_btn[i].raw = readRaw(g_btn[i]);
        g_btn[i].stable = g_btn[i].raw;
        g_btn[i].longFired = g_btn[i].raw; // ha már nyomva, ne adjon eseményt
        g_btn[i].rawChangedMs = now;
        g_btn[i].pressedMs = now;
        g_btn[i].lastRepeatMs = now;
        g_btn[i].pending = EV_NONE;
    }
}

/**
 * @brief Gombok inicializálása.
 *
 * Beállítja a gombok lábait bemenetként, engedélyezi a belső felhúzást, és inicializálja a gombok állapotát.
 */
void buttonsInit() {
    BTN_DDR &= ~(_BV(BTN_SW1_BIT) | _BV(BTN_SW2_BIT));
    BTN_PORT |= _BV(BTN_SW1_BIT) | _BV(BTN_SW2_BIT); // belső felhúzás (a rajzon nincs külső)
    resetStates();
}

/**
 * @brief Gombok állapotának frissítése.
 *
 * Ezt a függvényt rendszeresen kell hívni a gombok állapotának nyomon követéséhez.
 * Pergésmentesítést, hosszú nyomás és ismétlődés kezelését végzi.
 */
void buttonsUpdate() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < 2; i++) {
        BtnState &b = g_btn[i];
        const bool raw = readRaw(b);

        // Pergésmentesítés: az új állapotnak BTN_DEBOUNCE_MS-ig stabilnak kell lennie.
        if (raw != b.raw) {
            b.raw = raw;
            b.rawChangedMs = now;
        }
        if (raw != b.stable && (now - b.rawChangedMs) >= BTN_DEBOUNCE_MS) {
            b.stable = raw;
            if (raw) { // lenyomás
                b.pressedMs = now;
                b.longFired = false;
            } else if (!b.longFired) { // felengedés hosszú nyomás nélkül = rövid nyomás
                b.pending = EV_SHORT;
            }
        }

        // Hosszú nyomás és ismétlődés nyomva tartás alatt.
        if (b.stable) {
            if (!b.longFired && (now - b.pressedMs) >= BTN_LONG_MS) {
                b.longFired = true;
                b.lastRepeatMs = now;
                b.pending = EV_LONG;
            } else if (b.longFired && (now - b.lastRepeatMs) >= BTN_REPEAT_MS) {
                b.lastRepeatMs = now;
                b.pending = EV_REPEAT;
            }
        }
    }
}

/**
 * @brief Igaz, ha valamelyik gombnak függőben lévő eseménye van.
 */
bool buttonsEventPending() { return g_btn[0].pending != EV_NONE || g_btn[1].pending != EV_NONE; }

/**
 * @brief Gomb esemény lekérdezése.
 *
 * Visszaadja a megadott gombhoz tartozó eseményt, majd törli azt.
 * Ha nincs esemény, EV_NONE-t ad vissza.
 */
ButtonEvent buttonEvent(ButtonId id) {
    const ButtonEvent e = g_btn[id].pending;
    g_btn[id].pending = EV_NONE;
    return e;
}

/**
 * @brief Ellenőrzi, hogy bármelyik gomb le van-e nyomva.
 *
 * @return true, ha legalább egy gomb le van nyomva, különben false.
 */
bool buttonsAnyDown() { return readRaw(g_btn[BTN_SW1]) || readRaw(g_btn[BTN_SW2]); }

/**
 * @brief Várakozás a gombok felengedésére.
 *
 * Ez a függvény blokkolja a programot, amíg az összes gomb fel nem lesz engedve,
 * vagy amíg el nem telik 3 másodperc. A végén vár a pergésmentesítés idejére
 * és nullázza a gombok állapotát.
 */
void buttonsWaitRelease() {
    const uint32_t start = millis();
    while (buttonsAnyDown() && (millis() - start) < 3000UL) {
        delay(10);
    }
    delay(BTN_DEBOUNCE_MS);
    resetStates();
}

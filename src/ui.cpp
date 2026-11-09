/**
 * @file ui.cpp
 * @brief Képernyők megvalósítása az U8g2 könyvtárral, "page buffer" módban (1 lap = 128 bájt RAM).
 *
 * A kirajzoló függvények nyolcszor futnak le képkockánként (laponként egyszer),
 * ezért a rajzoló kódban nincs tartós állapot és nincs nehéz számítás.
 * A feliratok ékezet nélküliek, mert a használt betűkészletek nem tartalmazzák az ő/ű karaktereket.
 */
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include <math.h>

#include "ui.h"

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/** Lapok közti hívandó függvény (gombpollozás). */
static void (*g_hook)() = nullptr;
/** Nullpont-jelzésének hiszterézise megakadályozza a küszöb körüli villogást. */
static bool g_ddNullOk = false;

/** A DD sáv skálaértékei (ADC egység), a legkisebb alkalmas kerül kiválasztásra. */
static const uint16_t kDdScales[] PROGMEM = {50, 100, 200, 500, 1000};

/**
 * @brief Új lap kérése a kijelzőtől, majd a poll hook hívása.
 *
 * @return Igaz, ha van még lap.
 */
static bool nextPageAndPoll() {
    const bool more = u8g2.nextPage();
    if (g_hook) {
        g_hook();
    }
    return more;
}

/**
 * @brief Beállítja a poll hook függvényt.
 *
 * @param hook A poll hook függvény.
 */
void uiSetPollHook(void (*hook)()) { g_hook = hook; }

/**
 * @brief A kijelzőt alvó módba helyezi.
 */
void uiSleep() { u8g2.setPowerSave(1); }

/**
 * @brief A kijelzőt ébreszti fel az alvó módból.
 */
void uiWake() { u8g2.setPowerSave(0); }

/**
 * @brief Flash-beli szöveg kirajzolása.
 *
 * @param x Bal szél.
 * @param y Alapvonal.
 * @param p PSTR szöveg (max. 23 karakter).
 */
static void drawP(uint8_t x, uint8_t y, PGM_P p) {
    char b[24];
    strncpy_P(b, p, sizeof(b) - 1);
    b[sizeof(b) - 1] = 0;
    u8g2.drawStr(x, y, b);
}

/**
 * @brief A felső sor: bal oldalt cím, jobb oldalt a tápfeszültség (alacsony elemnél villogó kerettel).
 *
 * @param title Cím (PSTR).
 * @param mv    Tápfeszültség mV-ban.
 */
static void drawTopBar(PGM_P title, uint16_t mv) {
    char b[10];
    u8g2.setFont(u8g2_font_6x10_tf);
    drawP(0, 9, title);

    snprintf_P(b, sizeof(b), PSTR("%u.%uV"), mv / 1000, (mv % 1000) / 100);
    const uint8_t w = u8g2.getStrWidth(b);
    const uint8_t x = 128 - w;

    // Alacsony elem esetén villogó keret
    if (mv < VBATT_LOW_MV && ((millis() / 500) & 1)) {
        // Villogó, inverz kijelzés: alacsony elem
        u8g2.drawBox(x - 2, 0, w + 2, 11);
        u8g2.setDrawColor(0);
        u8g2.drawStr(x - 1, 9, b);
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawStr(x, 9, b);
    }
}

/**
 * @brief Nagy, előjeles szám kirajzolása (negatív értéknél kis "-" jel, a szám nagy számjegyekkel).
 *        A "+" jel nincs kirajzolva, de a helye üresen marad, így a szám pozíciója nem ugrál.
 *        A kerekített 0 mindig előjel nélkül jelenik meg.
 *
 * @param x      Bal szél, vagy a középpont, ha center igaz.
 * @param y      Alapvonal.
 * @param v      Az érték.
 * @param center Igaz: a szöveget a megadott x köré középre igazítja.
 */
static void drawSignedBig(uint8_t x, uint8_t y, int16_t v, bool center) {
    char b[8];
    itoa(v < 0 ? -v : v, b, 10);
    u8g2.setFont(u8g2_font_logisoso18_tn);
    const uint8_t w = u8g2.getStrWidth(b);
    uint8_t x0 = x;

    if (center) {
        x0 = (x > (w + 8) / 2) ? x - (w + 8) / 2 : 0;
    }

    u8g2.drawStr(x0 + 8, y, b);
    u8g2.setFont(u8g2_font_6x10_tf);
    if (v < 0) {
        u8g2.drawStr(x0, y - 4, "-");
    }
}

/**
 * @brief Keretes „NULLAZAS...” felirat a kijelző közepén (a KERESES és a GRAFIKON képernyő közös kódja).
 *        A keret belseje törlődik, hogy a mögötte lévő rajz ne zavarja az olvasást.
 *        A hívás a firstPage()/nextPage() ciklusban, a többi rajzolás után történjen.
 */
static void drawZeroingBox() {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char *msg = "NULLAZAS...";
    const uint8_t w = u8g2.getStrWidth(msg);
    const uint8_t x0 = (128 - w) / 2;
    const uint8_t padX = 12; // vízszintes hézag a szöveg és a keret között
    const uint8_t boxY = 21;
    const uint8_t boxH = 25;
    u8g2.setDrawColor(0);
    u8g2.drawBox(x0 - padX, boxY, w + 2 * padX, boxH);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(x0 - padX, boxY, w + 2 * padX, boxH);
    u8g2.drawStr(x0, 37, msg);
}

/**
 * @brief Inicializálja a felhasználói felületet.
 */
void uiInit() {
    u8g2.begin();
    u8g2.setBusClock(400000); // I2C busz sebesség 400 kHz
    u8g2.setContrast(220);    // Kijelző kontraszt beállítása
}

/**
 * @brief Üzenet megjelenítése a kijelzőn.
 *
 * @param fmt PSTR formátum string.
 * @param v   Érték a formátumhoz.
 */
void uiMessage(PGM_P fmt, int v) {
    char b[24];
    snprintf_P(b, sizeof(b), fmt, v);
    u8g2.setFont(u8g2_font_6x10_tf);
    const uint8_t w = u8g2.getStrWidth(b);
    u8g2.firstPage();
    do {
        u8g2.drawFrame(4, 20, 120, 24);
        u8g2.drawStr((128 - w) / 2, 36, b);
    } while (u8g2.nextPage());
}

/**
 * @brief Kirajzolja a kereső képernyőt.
 *
 * @param d A felhasználói felület adatai.
 */
void uiDrawSearch(const UiData &d) {
    char b[16];
    const float fs = d.fullScale;
    const float amp = fabsf(d.shown);
    uint8_t barW = (uint8_t)(constrain(amp / fs, 0.0f, 1.0f) * 124.0f);
    uint8_t peakX = (uint8_t)(constrain(d.peak / fs, 0.0f, 1.0f) * 124.0f);

    u8g2.firstPage();
    do {
        drawTopBar(PSTR("KERESES"), d.vbattMv);

        drawSignedBig(0, 36, (int16_t)lroundf(d.shown), false);

        // Mélység és fém típusa a jobb oldalon
        u8g2.setFont(u8g2_font_6x10_tf);
        if (d.depthMm == DEPTH_NONE) {
            drawP(72, 26, PSTR("--mm"));
        } else if (d.depthMm == DEPTH_OVER) {
            snprintf_P(b, sizeof(b), PSTR(">%dmm"), DEPTH_MAX_MM);
            u8g2.drawStr(72, 26, b);
        } else {
            snprintf_P(b, sizeof(b), d.depthCal ? PSTR("%dmm") : PSTR("~%dmm?"), d.depthMm);
            u8g2.drawStr(72, 26, b);
        }

        // Fém típusa (vas / nem vas) a jel erősség sáv alatt
        if (d.metal == 1) {
            drawP(58, 38, PSTR("Fe (vas)"));

        } else if (d.metal == 2) {
            drawP(58, 38, PSTR("nFe (szines)"));
        }

        // Jelerősség-sáv
        u8g2.drawFrame(0, 42, 128, 10);
        if (barW) {
            u8g2.drawBox(2, 44, barW, 6);
        }

        // Csúcs-jelző
        if (peakX) {
            u8g2.drawVLine(2 + peakX, 40, 14);
        }

        // Alsó sor: állapot balra, a skála mindig jobbra igazítva (nem ugrál)
        u8g2.setFont(u8g2_font_5x7_tf);
        if (d.saturated) {
            drawP(0, 63, PSTR("TELITES!"));
        } else if (d.metal == 0) {
            drawP(0, 63, PSTR("Nincs jel"));
        }

        snprintf_P(b, sizeof(b), PSTR("Skala: %u"), d.fullScale);
        u8g2.drawStr(128 - u8g2.getStrWidth(b), 63, b);

        // Zeroing box
        if (d.zeroing) {
            drawZeroingBox();
        }

    } while (nextPageAndPoll());
}

/**
 * @brief Kirajzolja a grafikon képernyőt.
 *
 * @param d A felhasználói felület adatai.
 */
void uiDrawGraph(const UiData &d) {
    char b[12];
    // ABS módban a nullvonal az alsó sor, így a teljes magasság felhasználható.
    const uint8_t zeroY = d.graphAbs ? GRAPH_ABS_ZERO_Y : 38;

    u8g2.firstPage();
    do {
        drawTopBar(PSTR("GRAFIKON"), d.vbattMv);
        u8g2.setFont(u8g2_font_6x10_tf);

        // Mód jelzése a cím és a mért érték között
        drawP(55, 9, d.graphAbs ? PSTR("ABS") : PSTR("SIG"));

        // A mért érték jobbra igazítva, a tápfeszültség előtt
        const int v = (int)lroundf(d.shown);
        snprintf_P(b, sizeof(b), PSTR("%d"), d.graphAbs ? abs(v) : v);
        u8g2.drawStr(94 - u8g2.getStrWidth(b), 9, b);

        // Függőleges elválasztók a fejlécben (táblázatszerű megjelenés)
        u8g2.drawVLine(52, 0, 11);
        u8g2.drawVLine(97, 0, 11);

        // Pontozott nullvonal
        for (uint8_t x = 0; x < 128; x += 4) {
            u8g2.drawPixel(x, zeroY);
        }
        // A teljes skála határai: ritkább pöttyözés, hogy ne zavarja a görbét
        const uint8_t range = d.graphAbs ? GRAPH_ABS_RANGE_PX : GRAPH_HALF_RANGE_PX;
        for (uint8_t x = 0; x < 128; x += 8) {
            u8g2.drawPixel(x, zeroY - range);
            if (!d.graphAbs)
                u8g2.drawPixel(x, zeroY + range);
        }

        // A jelalak: a gyűrűpuffer legrégebbi elemétől a legújabbig, balról jobbra
        int8_t prev = d.hist[d.histHead];
        for (uint8_t x = 1; x < HIST_LEN; x++) {
            const int8_t cur = d.hist[(d.histHead + x) % HIST_LEN];
            u8g2.drawLine(x - 1, zeroY - prev, x, zeroY - cur);
            prev = cur;
        }

        if (d.zeroing) {
            drawZeroingBox();
        }

    } while (nextPageAndPoll());
}

/**
 * @brief Kirajzolja a DD beállítás képernyőt.
 *
 * @param d A felhasználói felület adatai.
 */
void uiDrawDD(const UiData &d) {
    char b[20];
    const float a = fabsf(d.dd);

    // Nullázás ellenőrzése
    if (d.ddSaturated) {
        g_ddNullOk = false;
    } else if (a <= DD_OK_COUNTS && fabsf(d.ddRaw) <= DD_OK_COUNTS) {
        g_ddNullOk = true;
    } else if (a > DD_OK_COUNTS + 1.0f || fabsf(d.ddRaw) > DD_OK_COUNTS + 1.0f) {
        g_ddNullOk = false;
    }

    // A legkisebb alkalmas skála kiválasztása
    uint16_t scale = pgm_read_word(&kDdScales[0]);
    for (uint8_t i = 0; i < sizeof(kDdScales) / sizeof(kDdScales[0]); i++) {
        scale = pgm_read_word(&kDdScales[i]);
        if (a <= scale) {
            break;
        }
    }

    int16_t len = (int16_t)(d.dd / scale * 58.0f);
    len = constrain(len, -58, 58);

    u8g2.firstPage();
    do {
        drawTopBar(PSTR("DD BEALLITAS"), d.vbattMv);

        drawSignedBig(0, 34, (int16_t)lroundf(d.dd), false);
        if (d.ddSaturated) {
            u8g2.setFont(u8g2_font_5x7_tf);
            drawP(83, 32, PSTR("TELITES!"));
        } else if (g_ddNullOk) {
            u8g2.setFont(u8g2_font_5x7_tf);
            drawP(88, 32, PSTR("NULL OK"));
        }

        // Előjeles sáv, a közepe a nullpont
        u8g2.drawFrame(3, 40, 122, 12);
        if (len > 0) {
            u8g2.drawBox(65, 42, len, 8);
        } else if (len < 0) {
            u8g2.drawBox(64 + len, 42, -len, 8);
        }
        u8g2.drawVLine(64, 37, 18);

        u8g2.setFont(u8g2_font_5x7_tf);

        snprintf_P(b, sizeof(b), PSTR("B:%d P:%d"), (int)lroundf(d.ddBaseline), (int)lroundf(d.ddPulse));
        u8g2.drawStr(0, 63, b);
        snprintf_P(b, sizeof(b), PSTR("+-%u"), scale);
        u8g2.drawStr(128 - u8g2.getStrWidth(b), 63, b);

    } while (nextPageAndPoll());
}

/**
 * @brief Beállítás-elem neve.
 *
 * @param item Az elem sorszáma.
 * @return PSTR név. */
static PGM_P itemName(uint8_t item) {

    switch (item) {
        case SET_PULSE:
            return PSTR("Impulzus");
        case SET_FREQ:
            return PSTR("Frekvencia");
        case SET_S1:
            return PSTR("S1 keslelt.");
        case SET_S2:
            return PSTR("S2 keslelt.");
        case SET_SENS:
            return PSTR("Erzekenyseg");
        case SET_VOLUME:
            return PSTR("Hang");
        case SET_CAL_DEPTH:
            return PSTR("Kal. melyseg");
        case SET_GRAPH_MODE:
            return PSTR("Grafikon");
        case SET_TX_TEST:
            return PSTR("TX folyamatos");
        default:
            return PSTR("Reset");
    }
}

/**
 * @brief Beállítás-elem értékének szövege.
 *
 * @param s    Beállítások.
 * @param item Elem sorszáma.
 * @param b    Cél puffer.
 * @param n    A puffer mérete.
 * @param txTest A folyamatos TX teszt állapota.
 */
static void itemValue(const Settings &s, uint8_t item, char *b, uint8_t n, bool txTest) {

    switch (item) {
        case SET_PULSE:
            snprintf_P(b, n, PSTR("%uus"), s.pulseUs);
            break;
        case SET_FREQ:
            snprintf_P(b, n, PSTR("%uHz"), s.freqHz);
            break;
        case SET_S1:
            snprintf_P(b, n, PSTR("%uus"), s.s1Us);
            break;
        case SET_S2:
            snprintf_P(b, n, PSTR("%uus"), s.s2Us);
            break;
        case SET_SENS:
            snprintf_P(b, n, PSTR("%u/%u"), s.sensIdx + 1, SENS_COUNT);
            break;
        case SET_VOLUME:
            if (s.volume == 0)
                strncpy_P(b, PSTR("KI"), n);
            else
                snprintf_P(b, n, PSTR("%u/3"), s.volume);
            break;
        case SET_CAL_DEPTH:
            snprintf_P(b, n, PSTR("%umm"), s.calDepthMm);
            break;
        case SET_GRAPH_MODE:
            strncpy_P(b, s.graphAbs ? PSTR("ABS") : PSTR("SIG"), n);
            break;
        case SET_TX_TEST:
            strncpy_P(b, txTest ? PSTR("BE") : PSTR("KI"), n);
            break;
        default:
            strncpy_P(b, PSTR("SW1 hossz"), n);
            break;
    }

    b[n - 1] = 0;
}

/**
 * @brief Kirajzolja a beállítások képernyőt.
 *
 * @param s    Beállítások.
 * @param sel  Kijelölt elem sorszáma.
 * @param vbatt Akkumulátor feszültség.
 * @param txTest Igaz, ha a folyamatos TX teszt be van kapcsolva.
 */
void uiDrawSettings(const Settings &s, uint8_t sel, uint16_t vbatt, bool txTest) {
    char b[14];
    // 5 sor látható; a kijelölt elem körül görgetünk.
    uint8_t first = sel > 2 ? sel - 2 : 0;
    if (first + 5 > SET_COUNT) {
        first = SET_COUNT - 5;
    }

    u8g2.firstPage();
    do {
        drawTopBar(PSTR("BEALLITASOK"), vbatt);
        u8g2.setFont(u8g2_font_6x10_tf);
        for (uint8_t r = 0; r < 5; r++) {
            const uint8_t item = first + r;
            const uint8_t y = 22 + r * 10;
            const bool selected = (item == sel);
            if (selected) {
                u8g2.drawBox(0, y - 9, 128, 10);
                u8g2.setDrawColor(0);
            }
            drawP(2, y, itemName(item));
            itemValue(s, item, b, sizeof(b), txTest);
            u8g2.drawStr(126 - u8g2.getStrWidth(b), y, b);
            u8g2.setDrawColor(1);
        }

    } while (nextPageAndPoll());
}

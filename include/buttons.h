/**
 * @file buttons.h
 * @brief Két gomb kezelése: pergésmentesítés, rövid / hosszú / ismétlődő esemény.
 */
#pragma once
#include <Arduino.h>

/** A gombok azonosítói. */
enum ButtonId : uint8_t {
  BTN_SW1 = 0,  ///< SW1 "Kalibrálás" (PD5)
  BTN_SW2 = 1   ///< SW2 "Nullázás" (PD4)
};

/** A gombok által kiváltott események. */
enum ButtonEvent : uint8_t {
  EV_NONE = 0,  ///< Nincs esemény
  EV_SHORT,     ///< Rövid nyomás (felengedéskor)
  EV_LONG,      ///< Hosszú nyomás elérése (nyomva tartás közben, egyszer)
  EV_REPEAT     ///< Hosszú nyomás utáni ismétlődés (nyomva tartás alatt)
};

/** @brief A gomb lábak beállítása (bemenet, belső felhúzás). */
void buttonsInit();

/** @brief A gombok állapotának frissítése; gyakran (legalább ~10 ms-onként) hívandó. */
void buttonsUpdate();

/**
 * @brief Az adott gomb függőben lévő eseményének lekérése (és törlése).
 * @param id A gomb azonosítója.
 * @return Az esemény, vagy EV_NONE.
 */
ButtonEvent buttonEvent(ButtonId id);

/**
 * @brief Megvárja, míg minden gomb felengedésre kerül (max. 3 s), majd törli az eseményeket.
 *        Ébredés után használjuk, hogy az ébresztő nyomás ne okozzon műveletet.
 */
void buttonsWaitRelease();

/**
 * @brief Igaz, ha bármelyik gomb éppen nyomva van (nyers láb-állapot).
 * @return Igaz, ha van lenyomott gomb.
 */
bool buttonsAnyDown();

/**
 * @brief Igaz, ha valamelyik gombnak függőben lévő (még le nem kérdezett) eseménye van.
 * @return Igaz, ha van függő esemény.
 */
bool buttonsEventPending();

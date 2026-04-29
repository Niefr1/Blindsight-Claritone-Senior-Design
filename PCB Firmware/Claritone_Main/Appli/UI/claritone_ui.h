/**
 * @file    claritone_ui.h
 * @brief   Claritone user-interface module
 *
 * Handles all physical controls on the Claritone headset:
 *
 *  BTN_SENS  (PC6) — pushbutton, active-low, pull-up
 *                    Cycles sensitivity level 0 → 1 → 2 → 3 → 4 → 0 …
 *                    Sensitivity controls the maximum detection range:
 *                      0=1.5m  1=2.0m  2=2.5m  3=3.0m  4=4.0m
 *
 *  BTN_TONE  (PC4) — pushbutton, active-low, pull-up
 *                    Cycles tone preset  0 → 1 → 2 → 0 …
 *                      0=A pentatonic  1=C major  2=D minor
 *
 *  SCROLL_1  (PD8) — rotary encoder CLK/A, active-low pull-up
 *  SCROLL_2  (PA9) — rotary encoder DT/B,  active-low pull-up
 *                    Quadrature decode: on each falling edge of SCROLL_1,
 *                    sample SCROLL_2:
 *                      SCROLL_2 HIGH → clockwise    → volume++
 *                      SCROLL_2 LOW  → counter-CW   → volume--
 *                    Volume range: 0 (mute) … 10 (max).
 *
 *  SCROLL_NO (PD9) — encoder push-switch, active-low pull-up (reserved).
 *
 * Usage:
 *   1. Call claritone_ui_init() once from main context.
 *   2. Call claritone_ui_poll() every ~10 ms from the main loop.
 *   3. Read claritone_ui_get_state() to get current levels.
 *   4. The UI module does NOT directly call audio functions; the caller
 *      should propagate state changes to claritone_audio_set_volume() etc.
 */

#ifndef CLARITONE_UI_H
#define CLARITONE_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * UI state snapshot
 * ---------------------------------------------------------------------- */
typedef struct {
    uint8_t volume_level;    /**< 0 (mute) – 10 (max)                      */
    uint8_t sensitivity;     /**< 0–4; maps to max detection range          */
    uint8_t tone_preset;     /**< 0=pentatonic, 1=C major, 2=D minor        */
} ClaritoneUIState_t;

/** Maximum detection range (metres) for each sensitivity level. */
extern const float CLARITONE_SENS_DIST[5];

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * @brief  Initialise the UI module.  Call once before the main loop.
 *         Assumes GPIO clock enables and pin modes are already configured
 *         by MX_GPIO_Init().
 */
void claritone_ui_init(void);

/**
 * @brief  Poll all controls.  Call every 10–50 ms from the main loop.
 *         Updates internal state; returns 1 if anything changed, 0 otherwise.
 */
uint8_t claritone_ui_poll(void);

/**
 * @brief  Return a pointer to the current (immutable from caller) UI state.
 */
const ClaritoneUIState_t *claritone_ui_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* CLARITONE_UI_H */

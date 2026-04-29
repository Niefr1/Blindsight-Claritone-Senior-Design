/**
 * @file    claritone_ui.c
 * @brief   Claritone user-interface module — button debounce + scroll wheel
 *
 * All pin reads use HAL_GPIO_ReadPin (no EXTI interrupts required).
 * Debounce is purely time-based via HAL_GetTick().
 *
 * Scroll wheel quadrature decode:
 *   On each falling edge of SCROLL_1 (PD8), SCROLL_2 (PA9) is sampled.
 *   B=HIGH at the A falling edge means CW (volume up).
 *   B=LOW  at the A falling edge means CCW (volume down).
 *   This is standard Gray-code quadrature — one detent per A falling edge.
 */

#include "claritone_ui.h"
#include "main.h"      /* pin macros: BTN_SENS_*, BTN_TONE_*, SCROLL_*     */
#include "stm32n6xx_hal.h"

/* =========================================================================
 * Configuration
 * ====================================================================== */
#define BTN_DEBOUNCE_MS   50u   /**< Minimum ms between button state changes */
#define SCROLL_DEBOUNCE_MS 5u  /**< Minimum ms between scroll events         */

/* Default starting values */
#define DEFAULT_VOLUME      8u
#define DEFAULT_SENSITIVITY 2u
#define DEFAULT_PRESET      0u

#define NUM_SENSITIVITY_LEVELS 5u
#define NUM_TONE_PRESETS       3u
#define MAX_VOLUME            10u

/* =========================================================================
 * Public constant — max detection range per sensitivity level
 * ====================================================================== */
const float CLARITONE_SENS_DIST[NUM_SENSITIVITY_LEVELS] = {
    1.5f, 2.0f, 2.5f, 3.0f, 4.0f
};

/* =========================================================================
 * Internal state
 * ====================================================================== */
static ClaritoneUIState_t s_state;

/* --- BTN_SENS ---------------------------------------------------------- */
static uint8_t  s_sens_pin_prev;        /* last sampled pin state          */
static uint32_t s_sens_debounce_tick;   /* tick of last state change       */

/* --- BTN_TONE ---------------------------------------------------------- */
static uint8_t  s_tone_pin_prev;
static uint32_t s_tone_debounce_tick;

/* --- Scroll wheel ------------------------------------------------------ */
static uint8_t  s_scroll_A_prev;        /* previous SCROLL_1 pin state     */
static uint32_t s_scroll_debounce_tick;

/* =========================================================================
 * Helper: read a GPIO pin and return 0 or 1
 * ====================================================================== */
static inline uint8_t read_pin(GPIO_TypeDef *port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1u : 0u;
}

/* =========================================================================
 * Public API
 * ====================================================================== */

void claritone_ui_init(void)
{
    s_state.volume_level = DEFAULT_VOLUME;
    s_state.sensitivity  = DEFAULT_SENSITIVITY;
    s_state.tone_preset  = DEFAULT_PRESET;

    uint32_t now = HAL_GetTick();

    /* Capture initial pin states so the first poll doesn't see a false edge */
    s_sens_pin_prev  = read_pin(BTN_SENS_GPIO_Port,  BTN_SENS_Pin);
    s_tone_pin_prev  = read_pin(BTN_TONE_GPIO_Port,  BTN_TONE_Pin);
    s_scroll_A_prev  = read_pin(SCROLL_1_GPIO_Port,  SCROLL_1_Pin);

    s_sens_debounce_tick   = now;
    s_tone_debounce_tick   = now;
    s_scroll_debounce_tick = now;
}

uint8_t claritone_ui_poll(void)
{
    uint32_t now     = HAL_GetTick();
    uint8_t  changed = 0u;

    /* ------------------------------------------------------------------ */
    /* BTN_SENS (PC6) — active-low                                        */
    /* ------------------------------------------------------------------ */
    {
        uint8_t pin_now = read_pin(BTN_SENS_GPIO_Port, BTN_SENS_Pin);

        /* Detect falling edge (HIGH→LOW = button pressed) after debounce */
        if (pin_now != s_sens_pin_prev &&
            (now - s_sens_debounce_tick) >= BTN_DEBOUNCE_MS)
        {
            s_sens_pin_prev       = pin_now;
            s_sens_debounce_tick  = now;

            if (pin_now == 0u) {
                /* Button pressed: cycle sensitivity */
                s_state.sensitivity = (uint8_t)
                    ((s_state.sensitivity + 1u) % NUM_SENSITIVITY_LEVELS);
                changed = 1u;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* BTN_TONE (PC4) — active-low                                        */
    /* ------------------------------------------------------------------ */
    {
        uint8_t pin_now = read_pin(BTN_TONE_GPIO_Port, BTN_TONE_Pin);

        if (pin_now != s_tone_pin_prev &&
            (now - s_tone_debounce_tick) >= BTN_DEBOUNCE_MS)
        {
            s_tone_pin_prev      = pin_now;
            s_tone_debounce_tick = now;

            if (pin_now == 0u) {
                /* Button pressed: cycle tone preset */
                s_state.tone_preset = (uint8_t)
                    ((s_state.tone_preset + 1u) % NUM_TONE_PRESETS);
                changed = 1u;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* Scroll wheel — quadrature decode on SCROLL_1 (PD8) falling edge   */
    /* ------------------------------------------------------------------ */
    {
        uint8_t A_now = read_pin(SCROLL_1_GPIO_Port, SCROLL_1_Pin);

        if ((now - s_scroll_debounce_tick) >= SCROLL_DEBOUNCE_MS) {
            /* Detect falling edge of A (HIGH → LOW) */
            if (s_scroll_A_prev == 1u && A_now == 0u) {
                s_scroll_debounce_tick = now;

                /* Sample B (SCROLL_2 / PA9) to determine direction */
                uint8_t B = read_pin(SCROLL_2_GPIO_Port, SCROLL_2_Pin);

                if (B == 1u) {
                    /* CW → volume up */
                    if (s_state.volume_level < MAX_VOLUME) {
                        s_state.volume_level++;
                        changed = 1u;
                    }
                } else {
                    /* CCW → volume down */
                    if (s_state.volume_level > 0u) {
                        s_state.volume_level--;
                        changed = 1u;
                    }
                }
            }
            s_scroll_A_prev = A_now;
        }
    }

    return changed;
}

const ClaritoneUIState_t *claritone_ui_get_state(void)
{
    return &s_state;
}

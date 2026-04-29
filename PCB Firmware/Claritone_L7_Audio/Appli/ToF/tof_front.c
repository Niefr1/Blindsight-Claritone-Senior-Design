#include "tof_front.h"
#include "platform.h"
#include "vl53l7cx_api.h"
#include "i2c_bb.h"
#include "main.h"
#include <string.h>

/* ===========================================================
 * Per-sensor configuration
 * =========================================================== */
typedef struct {
    GPIO_TypeDef *lpn_port;
    uint16_t      lpn_pin;
    uint8_t       i2c_addr_8bit;   /* 0x52 default; reassigned for non-front */
    const char   *name;
} sensor_pin_cfg_t;

static const sensor_pin_cfg_t sensor_cfg[TOF_COUNT] = {
    [TOF_IDX_FRONT] = { TOF_LPN_GPIO_Port, TOF_LPN_Pin, 0x52, "FRONT" },
    [TOF_IDX_LEFT]  = { GPIOF, GPIO_PIN_9,             0x54, "LEFT"  },
    [TOF_IDX_RIGHT] = { GPIOF, GPIO_PIN_1,             0x56, "RIGHT" },
    [TOF_IDX_REAR]  = { GPIOF, GPIO_PIN_2,             0x58, "REAR"  },
};

static VL53L7CX_Configuration s_dev[TOF_COUNT];
static uint8_t                s_online[TOF_COUNT] = { 0, 0, 0, 0 };

static inline uint8_t zone_valid(uint8_t status) {
    return (status == 5U) || (status == 6U) || (status == 9U);
}

/* ===========================================================
 * GPIO helpers
 * =========================================================== */

/* Configure GPIOF pins as outputs (for SATEL LPn lines). */
static void init_gpiof_pins(void)
{
    __HAL_RCC_GPIOF_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Pin   = GPIO_PIN_9 | GPIO_PIN_1 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOF, &g);
}

/* All sensor LPns LOW = bus silent. */
static void all_lpn_low(void)
{
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_2, GPIO_PIN_RESET);
}

static void sensor_reset_pulse(void)
{
    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(TOF_I2C_RST_GPIO_Port, TOF_I2C_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
}

/* ===========================================================
 * Bring up a single sensor at default address 0x52,
 * then optionally reassign its address.
 * Returns 1 on success, 0 on failure (sensor not present).
 * Pre: this sensor's LPn is HIGH, all others LOW.
 * =========================================================== */
static uint8_t bring_up_sensor(tof_idx_t idx)
{
    const sensor_pin_cfg_t *cfg = &sensor_cfg[idx];
    uint8_t alive = 0;

    s_dev[idx].platform.address  = 0x52;
    s_dev[idx].platform.lpn_port = cfg->lpn_port;
    s_dev[idx].platform.lpn_pin  = cfg->lpn_pin;
    s_dev[idx].platform.rst_port = TOF_I2C_RST_GPIO_Port;
    s_dev[idx].platform.rst_pin  = TOF_I2C_RST_Pin;
    s_dev[idx].platform.int_port = TOF_INT_GPIO_Port;
    s_dev[idx].platform.int_pin  = TOF_INT_Pin;

    if (vl53l7cx_is_alive(&s_dev[idx], &alive) != 0 || !alive) return 0;

    /* Reassign address if non-default. After this point, this sensor
     * responds at the new address even if LPn is later toggled. */
    if (cfg->i2c_addr_8bit != 0x52) {
        if (vl53l7cx_set_i2c_address(&s_dev[idx], cfg->i2c_addr_8bit) != 0) return 0;
    }

    if (vl53l7cx_init(&s_dev[idx]) != 0) return 0;
    if (vl53l7cx_set_resolution(&s_dev[idx], VL53L7CX_RESOLUTION_4X4) != 0) return 0;
    if (vl53l7cx_set_ranging_frequency_hz(&s_dev[idx], 15) != 0) return 0;
    if (vl53l7cx_start_ranging(&s_dev[idx]) != 0) return 0;

    return 1;
}

/* ===========================================================
 * Multi-sensor API
 * =========================================================== */

uint8_t ToF_Array_Init(void)
{
    i2c_bb_init();
    init_gpiof_pins();

    /* All LPns LOW initially -- sensors in standby */
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_2, GPIO_PIN_RESET);

    /* Mark sensors we're not using as offline upfront */
    s_online[TOF_IDX_LEFT] = 0;
    s_online[TOF_IDX_REAR] = 0;

    /* I2C reset pulse */
    sensor_reset_pulse();

    /* === RIGHT first: bring up at 0x52, reassign to 0x56 ===
     * FRONT stays in standby so the bus belongs to RIGHT only. */
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_Delay(10);
    s_online[TOF_IDX_RIGHT] = bring_up_sensor(TOF_IDX_RIGHT);
    if (!s_online[TOF_IDX_RIGHT]) {
        /* RIGHT failed -- pull its LPn back low and continue with FRONT only */
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
    }

    /* === FRONT next: bring up at 0x52 (RIGHT is now at 0x56, no conflict) === */
    HAL_GPIO_WritePin(TOF_LPN_GPIO_Port, TOF_LPN_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    s_online[TOF_IDX_FRONT] = bring_up_sensor(TOF_IDX_FRONT);

    return s_online[TOF_IDX_FRONT] + s_online[TOF_IDX_RIGHT];
}

uint8_t ToF_Array_Poll(tof_array_state_t *out)
{
    if (!out) return 0;
    uint8_t updated = 0;

    for (int i = 0; i < TOF_COUNT; i++) {
        out->online[i] = s_online[i];
        if (!s_online[i]) {
            out->valid[i] = 0;
            out->distance_mm[i] = 0;
            continue;
        }

        uint8_t ready = 0;
        if (vl53l7cx_check_data_ready(&s_dev[i], &ready) != 0) continue;
        if (!ready) continue;

        VL53L7CX_ResultsData r;
        if (vl53l7cx_get_ranging_data(&s_dev[i], &r) != 0) continue;

        uint16_t best = 0xFFFFu;
        uint8_t  any_valid = 0;
        for (int z = 0; z < 16; z++) {
            if (!zone_valid(r.target_status[z])) continue;
            int16_t d = r.distance_mm[z];
            if (d <= 0) continue;
            if ((uint16_t)d < best) { best = (uint16_t)d; any_valid = 1; }
        }

        if (any_valid) {
            out->distance_mm[i] = best;
            out->valid[i] = 1;
        } else {
            out->distance_mm[i] = 0;
            out->valid[i] = 0;
        }
        updated |= (uint8_t)(1u << i);
    }

    return updated;
}

/* ===========================================================
 * Legacy single-sensor API (back-compat for current main.c)
 * =========================================================== */

uint8_t ToF_Front_Init(void)
{
    uint8_t up = ToF_Array_Init();
    /* Return 0 = success (at least front came up), nonzero = failure */
    return s_online[TOF_IDX_FRONT] ? 0 : 1;
}

uint8_t ToF_Front_Poll(tof_front_state_t *out)
{
    if (!out) return 0;
    tof_array_state_t arr;
    memset(&arr, 0, sizeof(arr));
    if (ToF_Array_Poll(&arr) == 0) return 0;

    if (arr.valid[TOF_IDX_FRONT]) {
        out->distance_mm = arr.distance_mm[TOF_IDX_FRONT];
        out->azimuth_q15 = 0;  /* not computed in this version */
        out->valid = 1;
    } else {
        out->distance_mm = 0;
        out->azimuth_q15 = 0;
        out->valid = 0;
    }
    return 1;
}

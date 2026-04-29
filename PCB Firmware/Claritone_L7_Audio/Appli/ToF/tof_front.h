#ifndef TOF_FRONT_H
#define TOF_FRONT_H

#include <stdint.h>

/* ===========================================================
 * Multi-sensor API (use when multi-sensor mode is active)
 * =========================================================== */
typedef enum {
    TOF_IDX_FRONT = 0,
    TOF_IDX_LEFT  = 1,
    TOF_IDX_RIGHT = 2,
    TOF_IDX_REAR  = 3,
    TOF_COUNT     = 4
} tof_idx_t;

typedef struct {
    uint16_t distance_mm[TOF_COUNT];
    uint8_t  valid[TOF_COUNT];
    uint8_t  online[TOF_COUNT];   /* 1 = sensor came online at init */
} tof_array_state_t;

/* Init all 4 sensors. Skips ones that don't respond. Returns count online. */
uint8_t ToF_Array_Init(void);

/* Poll all online sensors. Returns bitmask of sensors with new data. */
uint8_t ToF_Array_Poll(tof_array_state_t *out);

/* ===========================================================
 * Legacy single-sensor API (front sensor only, back-compat)
 * =========================================================== */
typedef struct {
    uint16_t distance_mm;
    int16_t  azimuth_q15;
    uint8_t  valid;
} tof_front_state_t;

uint8_t ToF_Front_Init(void);
uint8_t ToF_Front_Poll(tof_front_state_t *out);

#endif

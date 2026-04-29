#include "spatial_audio.h"
#include <math.h>
#include <string.h>

/* ============================================================
 * Audio buffer (DMA-visible)
 * ============================================================*/
__attribute__((aligned(32)))
int32_t g_audio_buffer[SA_TOTAL_WORDS];

/* ============================================================
 * Sine LUT — 256 samples, 32-bit signed, ~90% of full scale
 * ============================================================*/
#define SINE_LUT_SIZE  256u
static int32_t s_sine_lut[SINE_LUT_SIZE];

/* Two oscillators: high (front) and low (rear).
 * Each has its own Q16.16 phase + phase_inc. */
static uint32_t s_phase_hi      = 0;
static uint32_t s_phase_lo      = 0;
static uint32_t s_phase_inc_hi  = 0;   /* set in init for 880 Hz */
static uint32_t s_phase_inc_lo  = 0;   /* set in init for 440 Hz */

/* ============================================================
 * Spatial state — written by main loop, read by ISR.
 * Values are pre-computed Q15 for ISR speed.
 * ============================================================*/
typedef struct {
    int16_t  pan_q15;          /* -32768 left .. +32767 right */
    int16_t  hi_gain_q15;      /* high (front) tone amplitude  0..32767 */
    int16_t  lo_gain_q15;      /* low (rear) tone amplitude    0..32767 */
    uint8_t  itd_samples_left; /* delay applied to L if pan>0 */
    uint8_t  itd_samples_right;/* delay applied to R if pan<0 */
} spatial_state_t;

static volatile spatial_state_t s_state = { 0, 0, 0, 0, 0 };

/* ============================================================
 * ITD delay lines — short FIFOs, max 64 samples (~1.3 ms @ 48k)
 * ============================================================*/
#define DELAY_LEN  64u
static int32_t s_delay_l[DELAY_LEN] = {0};
static int32_t s_delay_r[DELAY_LEN] = {0};
static uint32_t s_delay_widx = 0;

/* ============================================================
 * Equal-power pan + head shadow LUTs
 * gL_lut[i], gR_lut[i] for pan index i where pan = (i/256.0 - 1.0).
 * Both are 16-bit Q15 unsigned.
 * ============================================================*/
#define PAN_LUT_SIZE  512u
static uint16_t s_gL_lut[PAN_LUT_SIZE];
static uint16_t s_gR_lut[PAN_LUT_SIZE];

/* ============================================================
 * Init
 * ============================================================*/
void SpatialAudio_Init(void)
{
    const float twoPi = 2.0f * 3.1415926535f;

    /* Sine LUT (90% of full scale) */
    for (uint32_t i = 0; i < SINE_LUT_SIZE; ++i) {
        float v = sinf(twoPi * (float)i / (float)SINE_LUT_SIZE);
        s_sine_lut[i] = (int32_t)(0x7FFFFFFF * 0.9f * v);
    }

    /* Phase increments: high=880 Hz, low=440 Hz */
    s_phase_inc_hi = (uint32_t)(((uint64_t)880 * SINE_LUT_SIZE * 65536u) / SA_SAMPLE_RATE_HZ);
    s_phase_inc_lo = (uint32_t)(((uint64_t)440 * SINE_LUT_SIZE * 65536u) / SA_SAMPLE_RATE_HZ);

    /* Pan + head-shadow LUT.
     *   index 0..511 → pan -1.0..+1.0
     *   gL = sqrt(0.5 * (1 - pan))   [equal-power]
     *   gR = sqrt(0.5 * (1 + pan))
     * Head shadow: reduce far-ear gain by (1 - 0.25 * |pan|). */
    for (uint32_t i = 0; i < PAN_LUT_SIZE; ++i) {
        float pan = ((float)i / (float)(PAN_LUT_SIZE / 2)) - 1.0f;
        float gL = sqrtf(0.5f * (1.0f - pan));
        float gR = sqrtf(0.5f * (1.0f + pan));
        float shadow = 1.0f - 0.25f * fabsf(pan);
        if (pan > 0.0f) gL *= shadow;
        else            gR *= shadow;
        s_gL_lut[i] = (uint16_t)(gL * 32767.0f);
        s_gR_lut[i] = (uint16_t)(gR * 32767.0f);
    }

    /* Clear state and delay lines */
    memset(s_delay_l, 0, sizeof(s_delay_l));
    memset(s_delay_r, 0, sizeof(s_delay_r));
    memset(g_audio_buffer, 0, sizeof(g_audio_buffer));
    s_phase_hi = s_phase_lo = 0;
    s_delay_widx = 0;
}

/* ============================================================
 * Update from ToF — runs in main loop
 * ============================================================*/

/* Clamp distance to demo-friendly range [50mm, 1500mm].
 * Returns gain_q15 in [0, 32767]. Closer = louder. */
static int16_t distance_to_gain_q15(uint16_t mm)
{
    if (mm == 0)       return 0;       /* invalid */
    if (mm < 50)       return 32767;   /* point-blank, max */
    if (mm > 1500)     return 0;       /* out of range */
    /* Linear ramp: 50mm→32767, 1500mm→0 */
    int32_t v = (int32_t)32767L * (1500 - (int32_t)mm) / 1450;
    if (v < 0)     v = 0;
    if (v > 32767) v = 32767;
    return (int16_t)v;
}

void SpatialAudio_UpdateFromToF(const tof_array_state_t *tof)
{
    if (!tof) return;

    /* Each sensor's contribution is a vector pointing from user to obstacle.
     *
     *   FRONT: (x= 0, z=+1)
     *   LEFT:  (x=-1, z= 0)
     *   RIGHT: (x=+1, z= 0)
     *   REAR:  (x= 0, z=-1)
     *
     * Sum weighted by individual gains. Resulting vector tells us:
     *   atan2(x, z) → pan direction
     *   |z| polarity → front (positive) vs rear (negative) bias for pitch
     */
    static const float dir_x[TOF_COUNT] = {  0.0f, -1.0f, +1.0f,  0.0f };
    static const float dir_z[TOF_COUNT] = { +1.0f,  0.0f,  0.0f, -1.0f };

    float sum_x = 0.0f, sum_z = 0.0f;
    float front_weight = 0.0f, rear_weight = 0.0f;

    for (int i = 0; i < TOF_COUNT; i++) {
        if (!tof->online[i] || !tof->valid[i]) continue;
        int16_t g = distance_to_gain_q15(tof->distance_mm[i]);
        float gf = (float)g / 32767.0f;
        sum_x += gf * dir_x[i];
        sum_z += gf * dir_z[i];
        if (dir_z[i] > 0.0f) front_weight += gf;
        else if (dir_z[i] < 0.0f) rear_weight += gf;
        /* Side sensors (z=0) contribute to neither front nor rear. */
    }

    /* Magnitude = overall gain. Cap at 1.0. */
    float mag = sqrtf(sum_x * sum_x + sum_z * sum_z);
    if (mag > 1.0f) mag = 1.0f;

    /* Pan from azimuth */
    float pan = 0.0f;
    if (mag > 0.001f) {
        float az = atan2f(sum_x, sum_z);   /* -PI..+PI */
        pan = sinf(az);                     /* -1..+1 */
        if (pan < -1.0f) pan = -1.0f;
        if (pan >  1.0f) pan =  1.0f;
    }

    /* Pitch split: front_weight drives high tone, rear_weight drives low tone.
     * Side sensors split equally between the two so they're audible. */
    float side_gain = mag - front_weight - rear_weight;
    if (side_gain < 0.0f) side_gain = 0.0f;
    float hi_gain = front_weight + 0.5f * side_gain;
    float lo_gain = rear_weight  + 0.5f * side_gain;

    /* ITD: max 0.7 ms = ~33 samples at 48 kHz */
    int32_t itd_samples = (int32_t)(fabsf(pan) * 33.0f);
    if (itd_samples >= (int32_t)DELAY_LEN) itd_samples = DELAY_LEN - 1;

    spatial_state_t ns;
    ns.pan_q15        = (int16_t)(pan * 32767.0f);
    ns.hi_gain_q15    = (int16_t)(hi_gain * 32767.0f);
    ns.lo_gain_q15    = (int16_t)(lo_gain * 32767.0f);
    ns.itd_samples_left  = (pan > 0.0f) ? (uint8_t)itd_samples : 0;
    ns.itd_samples_right = (pan < 0.0f) ? (uint8_t)itd_samples : 0;

    s_state = ns;   /* atomic-ish: 8-byte struct, single store */
}

/* ============================================================
 * Fill half-buffer (called from ISR)
 * ============================================================*/
void SpatialAudio_FillHalf(uint32_t word_offset)
{
    /* Snapshot state once per half-buffer to prevent torn reads */
    spatial_state_t st = s_state;

    /* Pan LUT index */
    uint32_t pan_idx = ((uint32_t)((int32_t)st.pan_q15 + 32768)) >> 7;
    if (pan_idx >= PAN_LUT_SIZE) pan_idx = PAN_LUT_SIZE - 1;
    uint16_t gL = s_gL_lut[pan_idx];
    uint16_t gR = s_gR_lut[pan_idx];

    uint32_t phi = s_phase_hi;
    uint32_t plo = s_phase_lo;
    const uint32_t inc_hi = s_phase_inc_hi;
    const uint32_t inc_lo = s_phase_inc_lo;
    const uint32_t mask = (SINE_LUT_SIZE - 1u) << 16;

    int32_t hi_amp = st.hi_gain_q15;
    int32_t lo_amp = st.lo_gain_q15;
    uint32_t widx = s_delay_widx;
    uint32_t itd_l = st.itd_samples_left;
    uint32_t itd_r = st.itd_samples_right;

    for (uint32_t i = 0; i < SA_HALF_FRAMES; ++i) {
        /* Two oscillators */
        int32_t s_hi = s_sine_lut[(phi & mask) >> 16];
        int32_t s_lo = s_sine_lut[(plo & mask) >> 16];

        /* Mix high+low by individual gains */
        int32_t mono = (int32_t)((((int64_t)s_hi * hi_amp) + ((int64_t)s_lo * lo_amp)) >> 15);

        /* Apply pan + head shadow */
        int32_t sL = (int32_t)(((int64_t)mono * gL) >> 15);
        int32_t sR = (int32_t)(((int64_t)mono * gR) >> 15);

        /* Push to delay line */
        s_delay_l[widx] = sL;
        s_delay_r[widx] = sR;

        /* Read with ITD offset (read older sample for delayed ear) */
        uint32_t rl = (widx + DELAY_LEN - itd_l) % DELAY_LEN;
        uint32_t rr = (widx + DELAY_LEN - itd_r) % DELAY_LEN;
        int32_t outL = s_delay_l[rl];
        int32_t outR = s_delay_r[rr];

        g_audio_buffer[word_offset + 2u*i + 0u] = outL;
        g_audio_buffer[word_offset + 2u*i + 1u] = outR;

        widx = (widx + 1u) % DELAY_LEN;
        phi += inc_hi;
        plo += inc_lo;
    }

    s_phase_hi = phi;
    s_phase_lo = plo;
    s_delay_widx = widx;
}

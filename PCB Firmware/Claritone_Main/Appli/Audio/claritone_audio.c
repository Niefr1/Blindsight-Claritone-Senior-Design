/**
 * @file    claritone_audio.c
 * @brief   Claritone spatial audio engine — C port of Sola's playback.cpp
 *
 * Algorithm (ported from playback.cpp, miniaudio removed):
 *  1. Pick the closest valid obstacle from up to 4 sensors.
 *  2. Map its distance to a pitch using the active preset frequency table.
 *  3. Compute azimuth pan from direction vector.
 *  4. Equal-power panning + head-shadow ILD (Sola's formulas, unchanged).
 *  5. ITD: delay the lagging ear channel by up to 0.7 ms (33 samples @48k).
 *  6. Distance attenuation: gain = (nearDist / dist)^rolloff.
 *  7. Pre-multiply all gains into two Q0.15 integers for the ISR.
 *
 * ISR path (claritone_audio_fill_half):
 *  - Reads precomputed volatile params (atomic on Cortex-M for ≤32-bit).
 *  - Runs LUT oscillator (integer phase accumulator, no float).
 *  - Applies ITD delay ring buffer.
 *  - Multiplies by Q0.15 channel gains.
 *  - Writes int32_t stereo samples to DMA buffer.
 *  No float, no libc math calls in the ISR path.
 *
 * Demo-sweep mode:
 *  Activates when no valid obstacle has been reported for DEMO_TIMEOUT_MS.
 *  A virtual obstacle at DEMO_DIST_M sweeps around the head in
 *  DEMO_PERIOD_MS, exercising the full left-right spatial effect on the bench.
 */

#include "claritone_audio.h"
#include "stm32n6xx_hal.h"   /* HAL_GetTick(), for demo timeout */

#include <string.h>
#include <math.h>            /* sinf, cosf, sqrtf, atan2f, powf, fabsf — main context ONLY */
#include <stddef.h>

/* =========================================================================
 * Constants
 * ====================================================================== */
#define SINE_LUT_SIZE      256u       /* must be power-of-2                 */
#define SINE_LUT_MASK      (SINE_LUT_SIZE - 1u)
#define ITD_BUF_SIZE       64u        /* 64 samples @ 48 kHz ≈ 1.33 ms     */

#define BASE_AMPLITUDE     0.20f      /* 20 % of full DAC scale             */
#define NEAR_DIST_M        0.30f      /* distance clamp lower bound (m)     */
#define ROLLOFF            1.0f       /* inverse-distance exponent          */
#define MAX_ITD_SEC        0.0007f    /* 0.7 ms max inter-aural delay       */
#define PAN_STRENGTH       1.0f       /* 0..1; 1.0 = full spatial width     */
#define HEAD_SHADOW        0.25f      /* far-ear attenuation coefficient    */

/* Silence above this distance regardless of preset */
#define SILENCE_DIST_M     3.5f

/* Demo mode */
#define DEMO_TIMEOUT_MS    5000u      /* ms of no-data before demo kicks in */
#define DEMO_DIST_M        2.0f       /* virtual obstacle distance (m)      */
#define DEMO_PERIOD_MS     4000u      /* full 360° sweep period (ms)        */

/* =========================================================================
 * Frequency / distance tables
 *
 * Six distance bins (descending).  When the closest obstacle is <= bin[i]
 * the engine uses freq_table[preset][i].  Beyond bin[0] → silence.
 * ====================================================================== */
#define N_BINS  6u

static const float DIST_BINS[N_BINS] = { 3.0f, 2.5f, 2.0f, 1.5f, 1.0f, 0.5f };

/** Preset 0: A pentatonic (A2–A5) — natural, easy on the ears */
/** Preset 1: C major root-position (C3–G5) */
/** Preset 2: D minor scale (D3–D6) */
static const float FREQ_TABLE[3][N_BINS] = {
    { 110.0f, 220.0f, 330.0f, 440.0f, 660.0f, 880.0f  },  /* preset 0 */
    { 130.8f, 196.0f, 261.6f, 392.0f, 523.3f, 783.9f  },  /* preset 1 */
    { 146.8f, 220.0f, 293.7f, 440.0f, 587.3f, 1174.7f },  /* preset 2 */
};

/* Sensor direction vectors — matches SENSOR_IDX_* in the header */
static const float SENSOR_DIR_X[SENSOR_COUNT] = {  0.0f, -1.0f,  1.0f,  0.0f };
static const float SENSOR_DIR_Z[SENSOR_COUNT] = {  1.0f,  0.0f,  0.0f, -1.0f };

/* =========================================================================
 * DMA buffer (extern-declared in header)
 * ====================================================================== */
int32_t g_audio_dma_buf[CLARITONE_DMA_WORDS];

/* =========================================================================
 * Sine LUT  (built once in claritone_audio_init, read from ISR)
 * Full-scale int32 amplitudes: lut[i] = (int32_t)(INT32_MAX * sin(...))
 * ====================================================================== */
static int32_t s_sine_lut[SINE_LUT_SIZE];

/* =========================================================================
 * ISR-shared parameters (written by main, read by ISR)
 * All ≤32-bit values are inherently atomic on Cortex-M (single LDRD/STR).
 * We use volatile to prevent the compiler reordering/caching them.
 * ====================================================================== */
static volatile uint32_t s_phase_inc_q;   /**< Q16.16 phase increment per sample */
static volatile uint32_t s_phase_q;       /**< Q16.16 phase accumulator          */
static volatile int16_t  s_gain_L_q15;    /**< Combined left  gain  Q0.15        */
static volatile int16_t  s_gain_R_q15;    /**< Combined right gain  Q0.15        */
static volatile int16_t  s_itd_delay;     /**< ITD delay in samples (0..63)      */
static volatile int8_t   s_itd_pan_sign;  /**< +1 = pan right, -1 = pan left, 0  */

/* ITD delay ring buffers (written and read in ISR only) */
static int32_t s_delay_buf[ITD_BUF_SIZE]; /**< common pre-gain sample ring      */
static uint8_t s_delay_write_idx;

/* =========================================================================
 * Main-context state
 * ====================================================================== */
static uint8_t  s_volume_level;           /* 0–10                               */
static uint8_t  s_tone_preset;            /* 0–2                                */
static uint32_t s_last_valid_tick;        /* HAL tick of last valid obstacle     */
static float    s_demo_angle;             /* current demo sweep angle (radians)  */
static uint32_t s_demo_last_tick;         /* tick of last demo update            */

/* =========================================================================
 * Internal helpers (main context only)
 * ====================================================================== */

/** Convert float frequency (Hz) to Q16.16 phase increment for the LUT. */
static uint32_t freq_to_phase_inc(float freq_hz)
{
    /* inc = (freq / fs) * LUT_SIZE * 65536 */
    return (uint32_t)(((double)freq_hz * (double)SINE_LUT_SIZE * 65536.0)
                      / (double)CLARITONE_SAMPLE_RATE);
}

/** Clamp a float to [lo, hi]. */
static inline float clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

/**
 * @brief  Compute and commit spatial params for a given obstacle direction
 *         and distance.  Called from main context only.
 *
 * @param  dir_x       Obstacle x-direction (-1=left, +1=right).
 * @param  dir_z       Obstacle z-direction (+1=front, -1=rear).
 * @param  distance_m  Distance in metres.
 * @param  freq_hz     Oscillator frequency in Hz (0 = silence).
 */
static void apply_spatial_params(float dir_x, float dir_z,
                                 float distance_m, float freq_hz)
{
    /* --- Silence path ------------------------------------------------- */
    if (freq_hz <= 0.0f || distance_m > SILENCE_DIST_M) {
        s_gain_L_q15   = 0;
        s_gain_R_q15   = 0;
        s_itd_delay    = 0;
        s_itd_pan_sign = 0;
        s_phase_inc_q  = 0;
        return;
    }

    /* --- Azimuth → pan  (Sola's formula) ------------------------------ */
    /* Normalise direction (guard against zero vector) */
    float len = sqrtf(dir_x * dir_x + dir_z * dir_z);
    if (len < 1e-6f) { dir_x = 0.0f; dir_z = 1.0f; }
    else             { dir_x /= len;  dir_z /= len; }

    float az  = atan2f(dir_x, dir_z);               /* azimuth in radians */
    float pan = clampf(sinf(az) * PAN_STRENGTH, -1.0f, 1.0f);

    /* --- Equal-power panning (Sola's formulas, unchanged) ------------- */
    float gL = sqrtf(0.5f * (1.0f - pan));
    float gR = sqrtf(0.5f * (1.0f + pan));

    /* Head shadow: attenuate the far ear */
    float shadow = 1.0f - HEAD_SHADOW * fabsf(pan);
    if (pan > 0.0f) gL *= shadow;
    else            gR *= shadow;

    /* --- Distance attenuation ----------------------------------------- */
    float dist  = clampf(distance_m, NEAR_DIST_M, SILENCE_DIST_M);
    float dGain = powf(NEAR_DIST_M / dist, ROLLOFF);
    dGain = clampf(dGain, 0.0f, 1.0f);

    /* --- Volume scale -------------------------------------------------- */
    float vol = (float)s_volume_level / 10.0f;    /* 0.0 – 1.0 */

    /* --- Combined gain ------------------------------------------------- */
    float combL = BASE_AMPLITUDE * dGain * gL * vol;
    float combR = BASE_AMPLITUDE * dGain * gR * vol;

    /* Convert to Q0.15 (max 32767) */
    s_gain_L_q15 = (int16_t)clampf(combL * 32767.0f, 0.0f, 32767.0f);
    s_gain_R_q15 = (int16_t)clampf(combR * 32767.0f, 0.0f, 32767.0f);

    /* --- ITD delay ----------------------------------------------------- */
    /* Delay the lagging ear: pan > 0 → right leads → delay left */
    float itd_sec     = MAX_ITD_SEC * fabsf(pan);
    int   itd_samples = (int)(itd_sec * (float)CLARITONE_SAMPLE_RATE + 0.5f);
    if (itd_samples >= (int)ITD_BUF_SIZE) itd_samples = (int)ITD_BUF_SIZE - 1;

    s_itd_delay    = (int16_t)itd_samples;
    s_itd_pan_sign = (pan > 0.0f) ? 1 : (pan < 0.0f) ? -1 : 0;

    /* --- Phase increment ----------------------------------------------- */
    s_phase_inc_q = freq_to_phase_inc(freq_hz);
}

/**
 * @brief  Map a distance (m) to the oscillator frequency for the active preset.
 * @return Frequency in Hz, or 0.0f if the obstacle is too far.
 */
static float distance_to_freq(float distance_m)
{
    if (distance_m > DIST_BINS[0]) return 0.0f;  /* too far → silence */

    /* Walk bins from farthest to closest; return first match */
    for (uint8_t i = 0u; i < N_BINS; ++i) {
        if (distance_m <= DIST_BINS[i]) {
            /* Keep going until we find the tightest (closest) bin */
            if (i == N_BINS - 1u || distance_m > DIST_BINS[i + 1u]) {
                return FREQ_TABLE[s_tone_preset][i];
            }
        }
    }
    /* Closer than the tightest bin → highest pitch */
    return FREQ_TABLE[s_tone_preset][N_BINS - 1u];
}

/* =========================================================================
 * Public API — main context
 * ====================================================================== */

void claritone_audio_init(void)
{
    /* Build full-scale sine LUT */
    const float twoPi = 2.0f * 3.14159265358979f;
    for (uint32_t i = 0u; i < SINE_LUT_SIZE; ++i) {
        float s = sinf(twoPi * (float)i / (float)SINE_LUT_SIZE);
        s_sine_lut[i] = (int32_t)((float)0x7FFFFFFF * s);
    }

    /* Zero DMA buffer and ISR params */
    memset(g_audio_dma_buf, 0, sizeof(g_audio_dma_buf));
    memset(s_delay_buf,     0, sizeof(s_delay_buf));

    s_phase_q      = 0u;
    s_phase_inc_q  = 0u;
    s_gain_L_q15   = 0;
    s_gain_R_q15   = 0;
    s_itd_delay    = 0;
    s_itd_pan_sign = 0;
    s_delay_write_idx = 0u;

    s_volume_level   = 8u;    /* default: 80% */
    s_tone_preset    = 0u;    /* default: A pentatonic */
    s_last_valid_tick = 0u;
    s_demo_angle      = 0.0f;
    s_demo_last_tick  = 0u;
}

void claritone_audio_set_volume(uint8_t level)
{
    if (level > 10u) level = 10u;
    s_volume_level = level;
}

void claritone_audio_set_tone_preset(uint8_t preset)
{
    if (preset > 2u) preset = 2u;
    s_tone_preset = preset;
}

void claritone_audio_update(const ClaritoneObstacle_t *obstacles)
{
    uint32_t now = HAL_GetTick();

    /* ------------------------------------------------------------------ */
    /* 1. Find the closest valid obstacle                                  */
    /* ------------------------------------------------------------------ */
    int8_t  closest_idx  = -1;
    float   closest_dist = SILENCE_DIST_M + 1.0f;

    if (obstacles != NULL) {
        for (uint8_t i = 0u; i < SENSOR_COUNT; ++i) {
            if (obstacles[i].valid && obstacles[i].distance_m < closest_dist) {
                closest_dist = obstacles[i].distance_m;
                closest_idx  = (int8_t)i;
            }
        }
    }

    if (closest_idx >= 0) {
        /* Valid obstacle — update timestamp */
        s_last_valid_tick = now;

        float dir_x   = SENSOR_DIR_X[(uint8_t)closest_idx];
        float dir_z   = SENSOR_DIR_Z[(uint8_t)closest_idx];
        float freq_hz = distance_to_freq(closest_dist);

        apply_spatial_params(dir_x, dir_z, closest_dist, freq_hz);
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 2. No valid obstacle — check demo timeout                          */
    /* ------------------------------------------------------------------ */
    if ((now - s_last_valid_tick) < DEMO_TIMEOUT_MS) {
        /* Within grace period: output silence */
        s_gain_L_q15  = 0;
        s_gain_R_q15  = 0;
        s_phase_inc_q = 0;
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 3. Demo-sweep mode                                                  */
    /* Virtual obstacle at DEMO_DIST_M, sweeping 360° over DEMO_PERIOD_MS */
    /* ------------------------------------------------------------------ */
    uint32_t elapsed = now - s_demo_last_tick;
    s_demo_last_tick = now;

    /* Advance sweep angle */
    float angle_inc = (2.0f * 3.14159265f)
                      * ((float)elapsed / (float)DEMO_PERIOD_MS);
    s_demo_angle += angle_inc;
    if (s_demo_angle >= 2.0f * 3.14159265f) {
        s_demo_angle -= 2.0f * 3.14159265f;
    }

    float dir_x   = sinf(s_demo_angle);   /* sweeps left ↔ right */
    float dir_z   = cosf(s_demo_angle);   /* and front ↔ rear     */
    float freq_hz = distance_to_freq(DEMO_DIST_M);

    apply_spatial_params(dir_x, dir_z, DEMO_DIST_M, freq_hz);
}

/* =========================================================================
 * ISR path — integer only, no float, no libc math
 * ====================================================================== */

/**
 * @brief  Fill CLARITONE_HALF_FRAMES stereo frames starting at word_offset.
 *         Must be called from SAI DMA half/full complete callbacks only.
 */
void claritone_audio_fill_half(uint32_t word_offset)
{
    /* Snapshot volatile params (single loads — atomic on Cortex-M) */
    const uint32_t phase_inc = s_phase_inc_q;
    const int16_t  gL        = s_gain_L_q15;
    const int16_t  gR        = s_gain_R_q15;
    const int16_t  delay     = s_itd_delay;
    const int8_t   pan_sign  = s_itd_pan_sign;

    uint32_t phase = s_phase_q;
    uint8_t  widx  = s_delay_write_idx;

    /* Fast path: silence (no multiply needed) */
    if (gL == 0 && gR == 0) {
        /* memset the half we own */
        int32_t *dst = &g_audio_dma_buf[word_offset];
        for (uint32_t i = 0u; i < CLARITONE_HALF_FRAMES * 2u; ++i) {
            dst[i] = 0;
        }
        /* Still advance phase so a tone restart is seamless */
        phase += phase_inc * CLARITONE_HALF_FRAMES;
        s_phase_q = phase;
        s_delay_write_idx = widx;
        return;
    }

    for (uint32_t i = 0u; i < CLARITONE_HALF_FRAMES; ++i) {
        /* LUT oscillator */
        uint32_t idx = (phase >> 16u) & SINE_LUT_MASK;
        int32_t  raw = s_sine_lut[idx];
        phase += phase_inc;

        /* Write raw sample into the delay ring (both channels share source) */
        s_delay_buf[widx] = raw;
        widx = (uint8_t)((widx + 1u) & (ITD_BUF_SIZE - 1u));

        /* Determine which channel gets the live sample vs the delayed one */
        int32_t sL = raw;
        int32_t sR = raw;

        if (delay > 0) {
            /* Read delayed sample from ring */
            uint8_t ridx = (uint8_t)((widx - 1u - (uint8_t)delay)
                                     & (ITD_BUF_SIZE - 1u));
            int32_t delayed = s_delay_buf[ridx];

            if (pan_sign > 0) {
                /* obstacle on the right → left ear is the lagging ear */
                sL = delayed;
            } else if (pan_sign < 0) {
                /* obstacle on the left → right ear is the lagging ear */
                sR = delayed;
            }
        }

        /* Apply channel gains (Q0.15 multiply → arithmetic right-shift 15) */
        /* int64 intermediate prevents overflow for large raw values          */
        int32_t outL = (int32_t)(((int64_t)sL * gL) >> 15);
        int32_t outR = (int32_t)(((int64_t)sR * gR) >> 15);

        /* Write stereo frame (L then R) */
        g_audio_dma_buf[word_offset + 2u * i + 0u] = outL;
        g_audio_dma_buf[word_offset + 2u * i + 1u] = outR;
    }

    /* Commit updated accumulators back to globals */
    s_phase_q         = phase;
    s_delay_write_idx = widx;
}

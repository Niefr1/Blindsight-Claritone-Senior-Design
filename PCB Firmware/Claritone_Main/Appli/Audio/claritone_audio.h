/**
 * @file    claritone_audio.h
 * @brief   Claritone spatial audio engine
 *
 * Architecture
 * ------------
 *  • Main context  — calls claritone_audio_update() every main-loop tick.
 *                    Runs float math (pan, gain, pitch) and writes
 *                    precomputed Q0.15 integer params to volatile globals.
 *  • ISR context   — HAL_SAI_TxHalfCpltCallback / HAL_SAI_TxCpltCallback
 *                    call claritone_audio_fill_half().  Integer-only:
 *                    LUT oscillator + multiply + ITD delay ring buffer.
 *
 * Obstacle input
 * --------------
 *  Feed up to 4 ClaritoneObstacle_t structs (one per ToF sensor):
 *    SENSOR_IDX_FRONT = 0  dir = (x=0,  z=+1)  forward
 *    SENSOR_IDX_LEFT  = 1  dir = (x=-1, z=0)   left
 *    SENSOR_IDX_RIGHT = 2  dir = (x=+1, z=0)   right
 *    SENSOR_IDX_REAR  = 3  dir = (x=0,  z=-1)  behind
 *
 *  The engine picks the closest valid obstacle, maps its distance to a
 *  pitch from the active tone-preset table, and spatialises it with the
 *  direction vector.  When no valid obstacles are present the engine enters
 *  a slow demo-sweep so the audio chain can be tested on the bench.
 *
 * Volume / preset control
 * -----------------------
 *  Call claritone_audio_set_volume(0..10) and
 *       claritone_audio_set_tone_preset(0..2)
 *  from the UI module; both are safe to call from main context at any time.
 */

#ifndef CLARITONE_AUDIO_H
#define CLARITONE_AUDIO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */
#define CLARITONE_HALF_FRAMES    256u          /**< Frames per DMA half-buffer  */
#define CLARITONE_TOTAL_FRAMES   (2u * CLARITONE_HALF_FRAMES)
/** Words in the DMA buffer (stereo int32, ping-pong) */
#define CLARITONE_DMA_WORDS      (2u * CLARITONE_TOTAL_FRAMES)

#define CLARITONE_SAMPLE_RATE    48000u        /**< Hz */

#define SENSOR_IDX_FRONT  0u
#define SENSOR_IDX_LEFT   1u
#define SENSOR_IDX_RIGHT  2u
#define SENSOR_IDX_REAR   3u
#define SENSOR_COUNT      4u

/* -------------------------------------------------------------------------
 * Obstacle descriptor (filled by main loop from ToF readings)
 * ---------------------------------------------------------------------- */
typedef struct {
    uint8_t valid;        /**< 1 = sensor has a fresh, trusted reading      */
    float   dir_x;        /**< Normalised lateral: -1 = left, +1 = right    */
    float   dir_z;        /**< Normalised fore/aft: +1 = front, -1 = rear   */
    float   distance_m;   /**< Distance in metres (>= 0)                    */
} ClaritoneObstacle_t;

/* -------------------------------------------------------------------------
 * DMA buffer — declared here so main.c can pass it to HAL_SAI_Transmit_DMA
 * ---------------------------------------------------------------------- */
extern int32_t g_audio_dma_buf[CLARITONE_DMA_WORDS];

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * @brief  Initialise the audio engine.
 *         Call once from main context before starting SAI DMA.
 *         Builds the sine LUT, zeros the DMA buffer, clears all state.
 */
void claritone_audio_init(void);

/**
 * @brief  Update spatial parameters from fresh obstacle data.
 *         Call from main loop (NOT from ISR).
 *
 * @param  obstacles  Array of SENSOR_COUNT obstacle descriptors.
 *                    Pass NULL or set all valid=0 to run demo-sweep mode.
 */
void claritone_audio_update(const ClaritoneObstacle_t *obstacles);

/**
 * @brief  Set master volume.
 * @param  level  0 = mute, 10 = full scale (20 % of DAC full-scale).
 */
void claritone_audio_set_volume(uint8_t level);

/**
 * @brief  Set tone-preset (pitch table selection).
 * @param  preset  0 = A pentatonic, 1 = C major, 2 = D minor.
 */
void claritone_audio_set_tone_preset(uint8_t preset);

/**
 * @brief  Fill one half of the DMA ping-pong buffer.
 *         Called from HAL_SAI_TxHalfCpltCallback (word_offset = 0) and
 *         HAL_SAI_TxCpltCallback (word_offset = CLARITONE_HALF_FRAMES * 2).
 *         INTEGER-ONLY — safe to call from ISR context.
 *
 * @param  word_offset  Starting word index in g_audio_dma_buf.
 */
void claritone_audio_fill_half(uint32_t word_offset);

#ifdef __cplusplus
}
#endif

#endif /* CLARITONE_AUDIO_H */

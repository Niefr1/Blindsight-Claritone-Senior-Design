#ifndef SPATIAL_AUDIO_H
#define SPATIAL_AUDIO_H

#include <stdint.h>
#include "tof_front.h"   /* for tof_array_state_t */

/* Audio format: 48 kHz, int32 stereo interleaved. */
#define SA_SAMPLE_RATE_HZ      48000u
#define SA_HALF_FRAMES         128u
#define SA_TOTAL_FRAMES        (2u * SA_HALF_FRAMES)
#define SA_TOTAL_WORDS         (2u * SA_TOTAL_FRAMES)   /* 512 int32 words */

extern int32_t g_audio_buffer[SA_TOTAL_WORDS];

/**
 * @brief Initialize LUTs, oscillator state, delay lines.
 *        Call once at boot before HAL_SAI_Transmit_DMA.
 */
void SpatialAudio_Init(void);

/**
 * @brief Update spatial parameters from latest ToF data.
 *        Call from main loop after each ToF poll.
 *        Computes pan, pitch, gain, ITD from sensor distances.
 */
void SpatialAudio_UpdateFromToF(const tof_array_state_t *tof);

/**
 * @brief Fill one half of the audio buffer. Called from SAI HT/TC callbacks.
 *        word_offset = 0 for first half, SA_HALF_FRAMES*2 for second half.
 */
void SpatialAudio_FillHalf(uint32_t word_offset);

#endif

/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @defgroup swappyGL_extra Swappy for OpenGL extras
 * Extra utility functions to use Swappy with OpenGL.
 * @{
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <jni.h>
#include <stdint.h>

#include "swappy_common.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief If an app wishes to use the Android choreographer to provide ticks to
 * Swappy, it can call this function.
 *
 * @warning This function *must* be called before the first `Swappy_swap()`
 * call. Afterwards, call this function every choreographer tick.
 */
SWAPPY_EXPORT void SwappyGL_onChoreographer(int64_t frameTimeNanos);

/** @brief Pass callbacks to be called each frame to trace execution. */
SWAPPY_EXPORT void SwappyGL_injectTracer(const SwappyTracer *t);

/**
 * @brief Toggle auto-swap interval detection on/off
 *
 * By default, Swappy will adjust the swap interval based on actual frame
 * rendering time. If an app wants to override the swap interval calculated by
 * Swappy, it can call `SwappyGL_setSwapIntervalNS`. This will temporarily
 * override Swappy's frame timings but, unless
 * `SwappyGL_setAutoSwapInterval(false)` is called, the timings will continue to
 * be be updated dynamically, so the swap interval may change.
 */
SWAPPY_EXPORT void SwappyGL_setAutoSwapInterval(bool enabled);

/**
 * @brief Sets the maximal duration for auto-swap interval in milliseconds.
 *
 * If Swappy is operating in auto-swap interval and the frame duration is longer
 * than `max_swap_ns`, Swappy will not do any pacing and just submit the frame
 * as soon as possible.
 */
SWAPPY_EXPORT void SwappyGL_setMaxAutoSwapIntervalNS(uint64_t max_swap_ns);

/**
 * @brief Toggle auto-pipeline mode on/off
 *
 * By default, if auto-swap interval is on, auto-pipelining is on and Swappy
 * will try to reduce latency by scheduling cpu and gpu work in the same
 * pipeline stage, if it fits.
 */
SWAPPY_EXPORT void SwappyGL_setAutoPipelineMode(bool enabled);

/**
 * @brief Toggle statistics collection on/off
 *
 * By default, stats collection is off and there is no overhead related to
 * stats. An app can turn on stats collection by calling
 * `SwappyGL_enableStats(true)`. Then, the app is expected to call
 * ::SwappyGL_recordFrameStart for each frame before starting to do any CPU
 * related work. Stats will be logged to logcat with a 'FrameStatistics' tag. An
 * app can get the stats by calling ::SwappyGL_getStats.
 */
SWAPPY_EXPORT void SwappyGL_enableStats(bool enabled);

/**
 * @brief Should be called if stats have been enabled with SwappyGL_enableStats.
 *
 * When stats collection is enabled with SwappyGL_enableStats, the app is
 * expected to call this function for each frame before starting to do any CPU
 * related work.
 *
 * @see SwappyGL_enableStats.
 */
SWAPPY_EXPORT void SwappyGL_recordFrameStart(EGLDisplay display, EGLSurface surface);

/**
 * @brief Returns the stats collected, if statistics collection was toggled on.
 *
 * @param swappyStats Pointer to a SwappyStats that will be populated with
 * collected stats.
 * @see SwappyStats
 * @see SwappyGL_enableStats
 */
SWAPPY_EXPORT void SwappyGL_getStats(SwappyStats *swappyStats);

/** @brief Remove callbacks that were previously added using
 * SwappyGL_injectTracer. */
SWAPPY_EXPORT void SwappyGL_uninjectTracer(const SwappyTracer *t);

#ifdef __cplusplus
};
#endif

/** @} */

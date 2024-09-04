// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <cassert>
#include <cstdint>

#include "epic_rtc_string_view.h"

#pragma pack(push, 8)

// Forward declare
struct EpicRtcVideoEncodingConfig;
class EpicRtcAudioEncoderInitializerInterface;
class EpicRtcAudioDecoderInitializerInterface;
class EpicRtcVideoEncoderInitializerInterface;
class EpicRtcVideoDecoderInitializerInterface;
enum class EpicRtcPixelFormat : uint8_t;
struct EpicRtcVideoResolutionBitrateLimits;
struct EpicRtcIceServer;

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcStringViewSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcStringView* _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcStringViewSpan) == 16);  // Ensure EpicRtcStringViewSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcVideoEncodingConfigSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcVideoEncodingConfig* _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcVideoEncodingConfigSpan) == 16);  // Ensure EpicRtcVideoEncodingConfigSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcAudioEncoderInitializerSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcAudioEncoderInitializerInterface** _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcAudioEncoderInitializerSpan) == 16);  // Ensure EpicRtcVideoEncodingConfigSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcAudioDecoderInitializerSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcAudioDecoderInitializerInterface** _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcAudioDecoderInitializerSpan) == 16);  // Ensure EpicRtcVideoEncodingConfigSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcVideoEncoderInitializerInterfaceSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcVideoEncoderInitializerInterface** _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcVideoEncoderInitializerInterfaceSpan) == 16);  // Ensure EpicRtcVideoEncoderInitializerInterfaceSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcVideoDecoderInitializerInterfaceSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcVideoDecoderInitializerInterface** _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcVideoDecoderInitializerInterfaceSpan) == 16);  // Ensure EpicRtcVideoDecoderInitializerInterfaceSpan is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcUint32Span
{
    /**
     * Raw pointer to the data.
     */
    const uint32_t* _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcUint32Span) == 16);  // Ensure EpicRtcUint32Span is expected size on all platforms

/**
 * Represents a view into a contiguous sequence of memory. The struct is non owning.
 */
struct EpicRtcIceServerSpan
{
    /**
     * Raw pointer to the data.
     */
    const EpicRtcIceServer* _ptr;

    /**
     * Number of elements in the Span.
     */
    uint64_t _size;
};

static_assert(sizeof(EpicRtcIceServerSpan) == 16);  // Ensure EpicRtcIceServerSpan is expected size on all platforms

#pragma pack(pop)

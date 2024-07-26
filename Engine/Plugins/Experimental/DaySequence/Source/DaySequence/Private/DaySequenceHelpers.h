// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DaySequenceTime.h"
#include "Misc/OptionalFwd.h"

struct FTimecode;

struct FFrameRate;

class DaySequenceHelpers
{
public:
	/**
	 * Convert Timecode to hours. Optionally provide a frame rate to convert
	 * frames portion of Timecode. If a frame rate is not provided, the
	 * frames portion of Timecode will be ignored.
	 *
	 * @param InTimecode Timecode to convert
	 * @param InRate Optional frame rate to convert frames portion of Timecode.
	 * @return result in hours.
	 */
	static float TimecodeToHours(const FDaySequenceTime& InTimecode, const TOptional<FFrameRate>& InRate);

	/**
	 * Convert Timecode to seconds. Optionally provide a frame rate to convert
	 * frames portion of Timecode. If a frame rate is not provided, the
	 * frames portion of Timecode will be ignored.
	 *
	 * @param InTimecode Timecode to convert
	 * @param InRate Optional frame rate to convert frames portion of Timecode.
	 * @return result in seconds.
	 */
	static float TimecodeToSeconds(const FDaySequenceTime& InTimecode, const TOptional<FFrameRate>& InRate);

	/**
	 * Convert hours to Timecode. Optionally provide a frame rate to convert
	 * fractional seconds into Timecode frames. If a frame rate is not provided,
	 * the fractional seconds will be ignored.
	 *
	 * @param InHours hours to convert
	 * @param InRate Optional frame rate to convert fractional seconds to frames.
	 * @return result in timecode.
	 */
	static FDaySequenceTime HoursToTimecode(float InHours, const TOptional<FFrameRate>& InRate);

	/**
	 * Convert seconds to Timecode. Optionally provide a frame rate to convert
	 * fractional seconds into Timecode frames. If a frame rate is not provided,
	 * the fractional seconds will be ignored.
	 *
	 * @param InSeconds seconds to convert
	 * @param InRate Optional frame rate to convert fractional seconds to frames.
	 * @return result in timecode.
	 */
	static FDaySequenceTime SecondsToTimecode(float InSeconds, const TOptional<FFrameRate>& InRate);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variants/MovieSceneTimeWarpVariantPayloads.h"
#include "MovieSceneTimeHelpers.h"
#include "MovieSceneTransformTypes.h"
#include <type_traits>

static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpFixedFrame>, "FMovieSceneTimeWarpFixedFrame must be trivially copyable");
static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpFrameRate>, "FMovieSceneTimeWarpFrameRate must be trivially copyable");
static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpLoop>, "FMovieSceneTimeWarpLoop must be trivially copyable");
static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpClamp>, "FMovieSceneTimeWarpClamp must be trivially copyable");
static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpLoopFloat>, "FMovieSceneTimeWarpLoopFloat must be trivially copyable");
static_assert(std::is_trivially_copyable_v<FMovieSceneTimeWarpClampFloat>, "FMovieSceneTimeWarpClampFloat must be trivially copyable");


FFrameTime FMovieSceneTimeWarpLoop::LoopTime(FFrameTime InTime) const
{
	// Maintain subframe
	InTime.FrameNumber = (InTime.FrameNumber % Duration);
	return InTime;
}

FFrameTime FMovieSceneTimeWarpLoop::LoopTime(FFrameTime InTime, int32& OutLoop) const
{
	OutLoop = InTime.FrameNumber.Value / Duration.Value;

	// Maintain subframe
	InTime.FrameNumber = InTime.FrameNumber % Duration;
	return InTime;
}

TRange<FFrameTime> FMovieSceneTimeWarpLoop::ComputeTraversedHull(const TRange<FFrameTime>& Range) const
{
	int32 StartLoop = 0;
	int32 EndLoop   = 0;

	FFrameTime LoopStart = 0;
	FFrameTime LoopEnd = Duration;

	if (Range.GetLowerBound().IsOpen() || Range.GetUpperBound().IsOpen())
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	const FFrameTime WarpedStart = LoopTime(Range.GetLowerBoundValue(), StartLoop);
	const FFrameTime WarpedEnd   = LoopTime(Range.GetUpperBoundValue(), EndLoop);

	if (StartLoop == EndLoop)
	{
		TRange<FFrameTime> Result = Range;
		Result.SetLowerBoundValue(WarpedStart);
		Result.SetUpperBoundValue(WarpedEnd);
		return Result;
	}

	const int32 NumCompleteLoops = EndLoop - StartLoop - 1;
	if (NumCompleteLoops >= 1)
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	// If the range crosses a loop boundary and the end time is > the start time, we have traversed a full loop
	if (WarpedEnd > WarpedStart)
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	// Technically there are 2 disjointed ranges that were traversed, but this api can only return 1 so we just return the most recent one
	return TRange<FFrameTime>(LoopStart, WarpedEnd);
}

TOptional<FFrameTime> FMovieSceneTimeWarpLoop::InverseRemapTimeCycled(FFrameTime InValue, FFrameTime InTimeHint, const UE::MovieScene::FInverseTransformTimeParams& Params) const
{
	if ((InValue.FrameNumber >= 0 && InValue.FrameNumber < Duration) ||
		 EnumHasAnyFlags(Params.Flags, UE::MovieScene::EInverseEvaluateFlags::Cycle))
	{
		int32 HintCycle = 0;
		FFrameTime LoopedHint = LoopTime(InTimeHint, HintCycle);
		//FFrameTime Result = LoopTime(InValue);

		FFrameTime Difference(InValue - LoopedHint);
		int32 DifferenceCycle = 0;
		FFrameTime LoopedDiff = LoopTime(Difference, DifferenceCycle);

		const int32 Length = Duration.Value;
		// Get the result within the correct loop according to the hint
		return Length*HintCycle + Length*DifferenceCycle + LoopedHint + LoopedDiff;
	}
	return TOptional<FFrameTime>();
}

bool FMovieSceneTimeWarpLoop::InverseRemapTimeWithinRange(FFrameTime InTime, FFrameTime RangeStart, FFrameTime RangeEnd, const TFunctionRef<bool(FFrameTime)>& VisitorCallback) const
{
	ensure(RangeStart < RangeEnd);

	int32 Length = Duration.Value;

	int32 InputLoop = 0;
	int32 StartLoop = 0;
	int32 EndLoop   = 0;

	FFrameTime LoopedInput = LoopTime(InTime,     InputLoop);
	FFrameTime StartTime   = LoopTime(RangeStart, StartLoop);
	FFrameTime EndTime     = LoopTime(RangeEnd,   EndLoop);

	int32 LoopIndex = InputLoop;
	FFrameTime Result  = LoopedInput + FFrameTime(Length*LoopIndex);

	// Handle with the start loop
	if (InputLoop != StartLoop || LoopedInput >= StartTime)
	{
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}

	++LoopIndex;
	for ( ; LoopIndex < EndLoop; ++LoopIndex)
	{
		Result += FFrameTime(Length);
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}

	// Handle trailing loop
	if (EndLoop != StartLoop && LoopedInput < EndTime)
	{
		Result += FFrameTime(Length);
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}

	return true;
}

bool FMovieSceneTimeWarpLoop::ExtractBoundariesWithinRange(FFrameTime RangeStart, FFrameTime RangeEnd, const TFunctionRef<bool(FFrameTime)>& InVisitor) const
{
	ensure(!(RangeStart == MIN_int32 && RangeEnd == MAX_int32));

	const int32 Length = Duration.Value;

	if (RangeStart == MIN_int32)
	{
		// Start at the end and go back
		int32 LoopIndex = 0;
		int32 EndLoop = 0;

		LoopTime(RangeStart, EndLoop);
		LoopTime(RangeEnd, LoopIndex);

		for (; LoopIndex >= EndLoop; --LoopIndex)
		{
			FFrameTime Result(Length * LoopIndex);

			if (!InVisitor(Result))
			{
				return false;
			}
		}
	}
	else
	{
		ensure(RangeStart < RangeEnd);

		int32 LoopIndex = 0;
		int32 EndLoop   = 0;

		LoopTime(RangeStart, LoopIndex);
		LoopTime(RangeEnd,   EndLoop);

		for ( ; LoopIndex < EndLoop + 1; ++LoopIndex)
		{
			FFrameTime StartResult = FFrameTime(Length*LoopIndex);

			if (StartResult >= RangeStart)
			{
				if (!InVisitor(StartResult))
				{
					return false;
				}
			}
		}
	}

	return true;
}

FFrameTime FMovieSceneTimeWarpClamp::Clamp(FFrameTime InTime) const
{
	if (InTime < 0)
	{
		return FFrameTime(0);
	}
	if (InTime > Max)
	{
		return Max;
	}
	return InTime;
}

TRange<FFrameTime> FMovieSceneTimeWarpClamp::ComputeTraversedHull(const TRange<FFrameTime>& Range) const
{
	TRange<FFrameTime> Result = Range;
	if (!Range.GetLowerBound().IsOpen())
	{
		Result.SetLowerBoundValue(Clamp(Range.GetLowerBoundValue()));
	}
	if (!Range.GetUpperBound().IsOpen())
	{
		Result.SetUpperBoundValue(Clamp(Range.GetUpperBoundValue()));
	}
	return Result;
}


FFrameTime FMovieSceneTimeWarpLoopFloat::LoopTime(FFrameTime InTime) const
{
	// Maintain subframe
	InTime = FFrameTime::FromDecimal(FMath::Fmod(InTime.AsDecimal(), Duration));
	return InTime;
}

FFrameTime FMovieSceneTimeWarpLoopFloat::LoopTime(FFrameTime InTime, int32& OutLoop) const
{
	OutLoop = FMath::FloorToInt(InTime.AsDecimal() / Duration);
	InTime = FFrameTime::FromDecimal(FMath::Fmod(InTime.AsDecimal(), Duration));
	return InTime;
}

TRange<FFrameTime> FMovieSceneTimeWarpLoopFloat::ComputeTraversedHull(const TRange<FFrameTime>& Range) const
{
	int32 StartLoop = 0;
	int32 EndLoop   = 0;

	FFrameTime LoopStart = 0;
	FFrameTime LoopEnd = FFrameTime::FromDecimal(Duration);

	if (Range.GetLowerBound().IsOpen() || Range.GetUpperBound().IsOpen())
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	const FFrameTime WarpedStart = LoopTime(Range.GetLowerBoundValue(), StartLoop);
	const FFrameTime WarpedEnd   = LoopTime(Range.GetUpperBoundValue(), EndLoop);

	if (StartLoop == EndLoop)
	{
		TRange<FFrameTime> Result = Range;
		Result.SetLowerBoundValue(WarpedStart);
		Result.SetUpperBoundValue(WarpedEnd);
		return Result;
	}

	const int32 NumCompleteLoops = EndLoop - StartLoop - 1;
	if (NumCompleteLoops >= 1)
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	// If the range crosses a loop boundary and the end time is > the start time, we have traversed a full loop
	if (WarpedEnd > WarpedStart)
	{
		return TRange<FFrameTime>(LoopStart, LoopEnd);
	}

	// Technically there are 2 disjointed ranges that were traversed, but this api can only return 1 so we just return the most recent one
	return TRange<FFrameTime>(LoopStart, WarpedEnd);
}

TOptional<FFrameTime> FMovieSceneTimeWarpLoopFloat::InverseRemapTimeCycled(FFrameTime InValue, FFrameTime InTimeHint, const UE::MovieScene::FInverseTransformTimeParams& Params) const
{
	if (InValue.FrameNumber >= 0 && InValue <= FFrameTime::FromDecimal(Duration))
	{
		int32 HintCycle = 0;
		LoopTime(InTimeHint, HintCycle);

		// Get the result within the correct loop according to the hint
		double Result = FMath::Fmod(InValue.AsDecimal(), Duration) + Duration*HintCycle;
		return FFrameTime::FromDecimal(Result);
	}
	return TOptional<FFrameTime>();
}

bool FMovieSceneTimeWarpLoopFloat::InverseRemapTimeWithinRange(FFrameTime InTime, FFrameTime RangeStart, FFrameTime RangeEnd, const TFunctionRef<bool(FFrameTime)>& VisitorCallback) const
{
	ensure(RangeStart < RangeEnd);

	FFrameTime Length = FFrameTime::FromDecimal(Duration);

	int32 InputLoop = 0;
	int32 StartLoop = 0;
	int32 EndLoop   = 0;

	FFrameTime LoopedInput = LoopTime(InTime,     InputLoop);
	FFrameTime StartTime   = LoopTime(RangeStart, StartLoop);
	FFrameTime EndTime     = LoopTime(RangeEnd,   EndLoop);

	int32 LoopIndex = InputLoop;
	FFrameTime Result  = LoopedInput + Length*LoopIndex;

	// Handle with the start loop
	if (InputLoop != StartLoop || LoopedInput >= StartTime)
	{
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}

	++LoopIndex;
	for ( ; LoopIndex < EndLoop; ++LoopIndex)
	{
		Result += Length;
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}

	// Handle trailing loop
	if (EndLoop != StartLoop && LoopedInput < EndTime)
	{
		Result += Length;
		if (!VisitorCallback(Result))
		{
			return false;
		}
	}
	
	return true;
}

bool FMovieSceneTimeWarpLoopFloat::ExtractBoundariesWithinRange(FFrameTime RangeStart, FFrameTime RangeEnd, const TFunctionRef<bool(FFrameTime)>& InVisitor) const
{
	ensure(!(RangeStart == MIN_int32 && RangeEnd == MAX_int32));

	FFrameTime Length = FFrameTime::FromDecimal(Duration);

	if (RangeStart == MIN_int32)
	{
		// Start at the end and go back
		int32 LoopIndex = 0;
		int32 EndLoop = 0;

		LoopTime(RangeStart, EndLoop);
		LoopTime(RangeEnd, LoopIndex);

		for (; LoopIndex >= EndLoop; --LoopIndex)
		{
			FFrameTime Result(Length * LoopIndex);

			if (!InVisitor(Result))
			{
				return false;
			}
		}
	}
	else
	{
		ensure(RangeStart < RangeEnd);

		int32 LoopIndex = 0;
		int32 EndLoop   = 0;

		LoopTime(RangeStart, LoopIndex);
		LoopTime(RangeEnd,   EndLoop);

		for ( ; LoopIndex < EndLoop + 1; ++LoopIndex)
		{
			FFrameTime StartResult = FFrameTime(Length*LoopIndex);

			if (StartResult >= RangeStart)
			{
				if (!InVisitor(StartResult))
				{
					return false;
				}
			}
		}
	}

	return true;
}

FFrameTime FMovieSceneTimeWarpClampFloat::Clamp(FFrameTime InTime) const
{
	if (InTime < 0)
	{
		return FFrameTime(0);
	}
	if (InTime.AsDecimal() > Max)
	{
		return FFrameTime::FromDecimal(Max);
	}
	return InTime;
}

TRange<FFrameTime> FMovieSceneTimeWarpClampFloat::ComputeTraversedHull(const TRange<FFrameTime>& Range) const
{
	TRange<FFrameTime> Result = Range;
	if (!Range.GetLowerBound().IsOpen())
	{
		Result.SetLowerBoundValue(Clamp(Range.GetLowerBoundValue()));
	}
	if (!Range.GetUpperBound().IsOpen())
	{
		Result.SetUpperBoundValue(Clamp(Range.GetUpperBoundValue()));
	}
	return Result;
}

FMovieSceneTimeWarpFrameRate::FMovieSceneTimeWarpFrameRate()
	: FMovieSceneTimeWarpFrameRate(FFrameRate())
{}

FMovieSceneTimeWarpFrameRate::FMovieSceneTimeWarpFrameRate(FFrameRate InRate)
{
	constexpr int32 SignBit32   = 0x80000000;
	constexpr int32 SignBit24   = 0x00800000;
	constexpr int32 InvalidBits = 0x7F800000;

	int32 Numerator   = InRate.Numerator;
	int32 Denominator = InRate.Denominator;

	// Do not allow 8 most significant bits, offset by the sign bit (our sign bit becomes bit index 23)
	check( (Numerator   & InvalidBits) == 0 );
	check( (Denominator & InvalidBits) == 0 );

	// Move the sign bit
	Numerator   |= ( (Numerator   & SignBit32) >> 8 );
	Denominator |= ( (Denominator & SignBit32) >> 8 );

	// Copy LSBs from 32 bits to 24 bits
	FMemory::Memcpy(FrameRateNumerator,   &Numerator,   sizeof(FrameRateNumerator));
	FMemory::Memcpy(FrameRateDenominator, &Denominator, sizeof(FrameRateDenominator));
}

FFrameRate FMovieSceneTimeWarpFrameRate::GetFrameRate() const
{
	constexpr int32 SignBit24 = 0x00800000;

	int32 Numerator   = 0;
	int32 Denominator = 0;

	// Copy LSBs from 24 bits to 32 bits
	FMemory::Memcpy(&Numerator,   FrameRateNumerator,   sizeof(FrameRateNumerator));
	FMemory::Memcpy(&Denominator, FrameRateDenominator, sizeof(FrameRateDenominator));

	// Move the sign bit
	Numerator   = ((Numerator  & SignBit24) << 8) | (Numerator  & ~SignBit24);
	Denominator = ((Denominator & SignBit24) << 8) | (Denominator & ~SignBit24);

	return FFrameRate(Numerator, Denominator);
}

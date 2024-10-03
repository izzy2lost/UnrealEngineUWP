// Copyright Epic Games, Inc.All Rights Reserved.

#include "ImageSequenceTimecodeUtils.h"

#include "ParseTakeUtils.h"

void UImageSequenceTimecodeUtils::SetTimecodeInfo(const FTimecode& InTimecode, const FFrameRate& InFrameRate, UImgMediaSource* InImageSequence)
{
	InImageSequence->StartTimecode = InTimecode;
	InImageSequence->FrameRateOverride = InFrameRate;
}

void UImageSequenceTimecodeUtils::SetTimecodeInfoString(const FString& InTimecode, const FString& InFrameRate, UImgMediaSource* InImageSequence)
{
	InImageSequence->StartTimecode = ParseTimecode(InTimecode);

	double TimecodeRate = FCString::Atod(*InFrameRate);
	InImageSequence->FrameRateOverride = ConvertFrameRate(TimecodeRate);
}

FTimecode UImageSequenceTimecodeUtils::GetTimecode(UImgMediaSource* InImageSequence)
{
	return InImageSequence->StartTimecode;
}

FFrameRate UImageSequenceTimecodeUtils::GetFrameRate(UImgMediaSource* InImageSequence)
{
	return InImageSequence->FrameRateOverride;
}

FString UImageSequenceTimecodeUtils::GetTimecodeString(UImgMediaSource* InImageSequence)
{
	return InImageSequence->StartTimecode.ToString();
}

FString UImageSequenceTimecodeUtils::GetFrameRateString(UImgMediaSource* InImageSequence)
{
	return FString::SanitizeFloat(InImageSequence->FrameRateOverride.AsDecimal());
}

bool UImageSequenceTimecodeUtils::IsValidTimecodeInfo(const FTimecode& InTimecode, const FFrameRate& InTimecodeRate)
{
	return IsValidTimecode(InTimecode) && IsValidFrameRate(InTimecodeRate);
}

bool UImageSequenceTimecodeUtils::IsValidTimecode(const FTimecode& InTimecode)
{
	return InTimecode.IsValid() && InTimecode != FTimecode();
}

bool UImageSequenceTimecodeUtils::IsValidFrameRate(const FFrameRate& InTimecodeRate)
{
	return InTimecodeRate.IsValid() && InTimecodeRate != FFrameRate();
}

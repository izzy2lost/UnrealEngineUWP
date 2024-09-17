// Copyright Epic Games, Inc.All Rights Reserved.

#include "ImageSequenceTimecodeUtils.h"

#include "ParseTakeUtils.h"

#if WITH_EDITOR
#include "Subsystems/EditorAssetSubsystem.h"
#include "Editor.h"
#endif

const FName UImageSequenceTimecodeUtils::TimecodeTagName = "Timecode";
const FName UImageSequenceTimecodeUtils::TimecodeRateTagName = "TimecodeRate";

void UImageSequenceTimecodeUtils::SetTimecodeInfo(const FTimecode& InTimecode, const FFrameRate& InFrameRate, UImgMediaSource* InImageSequence)
{
	FString ImageTimecodeString = InTimecode.ToString();
	FString ImageTimecodeRateString = FString::SanitizeFloat(InFrameRate.AsDecimal());

	SetTimecodeInfoString(ImageTimecodeString, ImageTimecodeRateString, InImageSequence);
}

void UImageSequenceTimecodeUtils::SetTimecodeInfoString(const FString& InTimecode, const FString& InFrameRate, UImgMediaSource* InImageSequence)
{
#if WITH_EDITOR
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;

	if (!EditorAssetSubsystem)
	{
		return;
	}

	EditorAssetSubsystem->SetMetadataTag(InImageSequence, UImageSequenceTimecodeUtils::TimecodeTagName, InTimecode);
	EditorAssetSubsystem->SetMetadataTag(InImageSequence, UImageSequenceTimecodeUtils::TimecodeRateTagName, InFrameRate);
#endif
}

FTimecode UImageSequenceTimecodeUtils::GetTimecode(UImgMediaSource* InImageSequence)
{
	TOptional<FTimecode> TimecodeOpt = TryGetTimecode(InImageSequence);

	if (TimecodeOpt.IsSet())
	{
		return TimecodeOpt.GetValue();
	}

	return FTimecode();
}

FFrameRate UImageSequenceTimecodeUtils::GetFrameRate(UImgMediaSource* InImageSequence)
{
	TOptional<FFrameRate> FrameRateOpt = TryGetFrameRate(InImageSequence);

	if (FrameRateOpt.IsSet())
	{
		return FrameRateOpt.GetValue();
	}

	return FFrameRate();
}

FString UImageSequenceTimecodeUtils::GetTimecodeString(UImgMediaSource* InImageSequence)
{
	TOptional<FString> TimecodeOpt = TryGetTimecodeString(InImageSequence);

	if (TimecodeOpt.IsSet())
	{
		return TimecodeOpt.GetValue();
	}

	return FString();
}

FString UImageSequenceTimecodeUtils::GetFrameRateString(UImgMediaSource* InImageSequence)
{
	TOptional<FString> FrameRateOpt = TryGetFrameRateString(InImageSequence);

	if (FrameRateOpt.IsSet())
	{
		return FrameRateOpt.GetValue();
	}

	return FString();
}

TOptional<FTimecode> UImageSequenceTimecodeUtils::TryGetTimecode(UImgMediaSource* InImageSequence)
{
	TOptional<FString> TimecodeOpt = TryGetTimecodeString(InImageSequence);

	if (!TimecodeOpt.IsSet())
	{
		return {};
	}

	return ParseTimecode(TimecodeOpt.GetValue());
}

TOptional<FFrameRate> UImageSequenceTimecodeUtils::TryGetFrameRate(UImgMediaSource* InImageSequence)
{
	TOptional<FString> TimecodeRateOpt = TryGetFrameRateString(InImageSequence);

	if (!TimecodeRateOpt.IsSet())
	{
		return {};
	}

	double TimecodeRate = FCString::Atod(*TimecodeRateOpt.GetValue());

	return ConvertFrameRate(TimecodeRate);
}

TOptional<FString> UImageSequenceTimecodeUtils::TryGetTimecodeString(UImgMediaSource* InImageSequence)
{
#if WITH_EDITOR
	if (!InImageSequence)
	{
		return {};
	}

	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;

	if (!EditorAssetSubsystem)
	{
		return {};
	}

	FString Timecode = EditorAssetSubsystem->GetMetadataTag(InImageSequence, UImageSequenceTimecodeUtils::TimecodeTagName);

	if (Timecode.IsEmpty())
	{
		return {};
	}

	return Timecode;
#else
	return {};
#endif
}

TOptional<FString> UImageSequenceTimecodeUtils::TryGetFrameRateString(UImgMediaSource* InImageSequence)
{
#if WITH_EDITOR
	if (!InImageSequence)
	{
		return {};
	}

	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;

	if (!EditorAssetSubsystem)
	{
		return {};
	}

	FString TimecodeRateMetadata = EditorAssetSubsystem->GetMetadataTag(InImageSequence, UImageSequenceTimecodeUtils::TimecodeRateTagName);

	if (!TimecodeRateMetadata.IsNumeric())
	{
		return {};
	}
	
	return TimecodeRateMetadata;
#else
	return {};
#endif
}

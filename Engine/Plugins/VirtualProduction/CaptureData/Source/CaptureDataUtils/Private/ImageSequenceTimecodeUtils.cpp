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

TOptional<FTimecode> UImageSequenceTimecodeUtils::GetTimecode(UImgMediaSource* InImageSequence)
{
	TOptional<FString> TimecodeOpt = GetTimecodeString(InImageSequence);

	if (!TimecodeOpt.IsSet())
	{
		return {};
	}

	return ParseTimecode(TimecodeOpt.GetValue());
}

TOptional<FFrameRate> UImageSequenceTimecodeUtils::GetFrameRate(UImgMediaSource* InImageSequence)
{
	TOptional<FString> TimecodeRateOpt = GetFrameRateString(InImageSequence);

	if (!TimecodeRateOpt.IsSet())
	{
		return {};
	}

	double TimecodeRate = FCString::Atod(*TimecodeRateOpt.GetValue());

	return ConvertFrameRate(TimecodeRate);
}

TOptional<FString> UImageSequenceTimecodeUtils::GetTimecodeString(UImgMediaSource* InImageSequence)
{
#if WITH_EDITOR
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;

	if (!EditorAssetSubsystem)
	{
		return {};
	}

	FString Timecode = EditorAssetSubsystem->GetMetadataTag(InImageSequence, UImageSequenceTimecodeUtils::TimecodeTagName);

	return Timecode;
#else
	return {};
#endif
}

TOptional<FString> UImageSequenceTimecodeUtils::GetFrameRateString(UImgMediaSource* InImageSequence)
{
#if WITH_EDITOR
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
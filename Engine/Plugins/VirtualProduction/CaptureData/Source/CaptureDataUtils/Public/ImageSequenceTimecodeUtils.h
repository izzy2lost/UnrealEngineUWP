// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timecode.h"
#include "ImgMediaSource.h"

#include "ImageSequenceTimecodeUtils.generated.h"

UCLASS(BlueprintType, Blueprintable)
class CAPTUREDATAUTILS_API UImageSequenceTimecodeUtils
	: public UObject
{
	GENERATED_BODY()

public:

	static const FName TimecodeTagName;
	static const FName TimecodeRateTagName;

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static void SetTimecodeInfo(const FTimecode& InTimecode, const FFrameRate& InFrameRate, UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static void SetTimecodeInfoString(const FString& InTimecode, const FString& InFrameRate, UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static TOptional<FTimecode> GetTimecode(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static TOptional<FFrameRate> GetFrameRate(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static TOptional<FString> GetTimecodeString(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static TOptional<FString> GetFrameRateString(UImgMediaSource* InImageSequence);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timecode.h"
#include "ImgMediaSource.h"

#include "ImageSequenceTimecodeUtils.generated.h"

UCLASS(BlueprintType, Blueprintable)
class UImageSequenceTimecodeInfo
	: public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category = "ImageSequenceInfo")
	FTimecode Timecode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ExposeOnSpawn), Category = "ImageSequenceInfo")
	FFrameRate FrameRate;
};

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
	static FTimecode GetTimecode(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static FFrameRate GetFrameRate(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static FString GetTimecodeString(UImgMediaSource* InImageSequence);

	UFUNCTION(BlueprintCallable, Category = "ImageSequence")
	static FString GetFrameRateString(UImgMediaSource* InImageSequence);

	static TOptional<FTimecode> TryGetTimecode(UImgMediaSource* InImageSequence);
	static TOptional<FFrameRate> TryGetFrameRate(UImgMediaSource* InImageSequence);
	static TOptional<FString> TryGetTimecodeString(UImgMediaSource* InImageSequence);
	static TOptional<FString> TryGetFrameRateString(UImgMediaSource* InImageSequence);
};

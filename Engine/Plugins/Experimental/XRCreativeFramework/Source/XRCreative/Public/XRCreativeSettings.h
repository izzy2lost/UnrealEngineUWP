// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "XRCreativeSettings.generated.h"

UENUM(BlueprintType)
enum class EXRCreativeHandedness : uint8
{
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right"),
};

UCLASS(Config=XRCreativeSettings, DisplayName="XR Creative")
class XRCREATIVE_API UXRCreativeSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="XR Creative", meta=(DisplayName="Show Measurements in Imperial Units"))
	bool bUseImperial = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="XR Creative", meta=(DisplayName="TestArray"))
	TArray<float> FloatArray = {1,3,2,5};

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="XR Creative")
	TSoftClassPtr<class UXRCreativeSubsystemHelper> SubsystemHelperClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="XR Creative", meta=(DisplayName="Handedness"))
	EXRCreativeHandedness Handedness = EXRCreativeHandedness::Right;

	UFUNCTION(BlueprintPure, Category="XR Creative")
	static UXRCreativeSettings* GetXRCreativeSettings();
};
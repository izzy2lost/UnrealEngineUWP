// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Engine/DeveloperSettings.h"
#include "Misc/EnumRange.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "SlateRendererSettings.generated.h"

/**
 * Bitfield used to mark if a slate post RT is used or not
 */
UENUM(BlueprintType)
enum class ESlatePostRT : uint8
{
	None = 0 << 0,
	ESlatePostRT_0 = 1 << 0,
	ESlatePostRT_1 = 1 << 1,
	ESlatePostRT_2 = 1 << 2,
	ESlatePostRT_3 = 1 << 3,
	ESlatePostRT_4 = 1 << 4,
	Num = 5
};

ENUM_CLASS_FLAGS(ESlatePostRT);

ENUM_RANGE_BY_VALUES(ESlatePostRT, ESlatePostRT::ESlatePostRT_0, ESlatePostRT::ESlatePostRT_1, ESlatePostRT::ESlatePostRT_2, ESlatePostRT::ESlatePostRT_3, ESlatePostRT::ESlatePostRT_4);

/**
 * Settings for a particular Slate Post RT.
 * Notably if enabled & blur by default. To be updated with additional effects & to be expandable in game code / settings.
 */
USTRUCT(BlueprintType, meta=(HiddenByDefault, DisableSplitPin))
struct SLATECORE_API FSlatePostSettings
{
	GENERATED_BODY()

public:

	FSlatePostSettings();

	friend class USlateRendererSettings;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault))
	uint8 bEnabled:1;

public:

	const FString& GetPathToSlatePostRT() const { return PathToSlatePostRT; }

private:

	/** Path to Slate Post RT Asset */
	FString PathToSlatePostRT;

	/** Cached load of Slate Post RT Asset */
	UObject* CachedSlatePostRT;
};

/**
 * Settings used to control slate rendering
 */
UCLASS(config = Game, defaultconfig)
class SLATECORE_API USlateRendererSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
		
	static const USlateRendererSettings* Get() 
	{ 
		return GetDefault<USlateRendererSettings>();
	}

	static USlateRendererSettings* GetMutable()
	{
		return GetMutableDefault<USlateRendererSettings>();
	}

public:

	USlateRendererSettings();
	~USlateRendererSettings();

	/** Try to get post RT asset, returns nullptr if not already loaded */
	UObject* TryGetPostBufferRT(ESlatePostRT InPostBufferBit) const;

	/** Get post RT asset, loading if not already loaded */
	UObject* LoadGetPostBufferRT(ESlatePostRT InPostBufferBit);

public:

	/** 
	 * Map of all slate post RT's and their settings 
	 * Note that each post RT used in a frame will result in 1 full framebuffer copy for slate to sample from.
	 * If a post RT is not used, no copy will occur & that post RT will be resized to 1x1 after 2 frames of non-use.
	 * 
	 * By default only SlatePostRT_0 is enabled. The rest must manually be enabled in settings below.
	 */

	// @TODO: I did a map, kinda hoping small map optimizations would be fine, anyone suggest different data structure? Discuss in review.
	// Map is nice since needs no editor customization. After initial run there should be no more than 5 lookups each frame.
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "PostProcessing")
	TMap<ESlatePostRT, FSlatePostSettings> SlatePostSettings;
};

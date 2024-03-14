// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangeAnimationDefinitions.h"
#include "InterchangeAnimationTrackSetNode.h"


#include "Engine/DeveloperSettings.h"

#include "InterchangeFbxSettings.generated.h"

USTRUCT(BlueprintType)
struct FInterchangePropertyTracksSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, config, Category = "FBX | Property Tracks")
	EInterchangePropertyTracks Property = EInterchangePropertyTracks::None;

	UPROPERTY(EditAnywhere, config, Category = "FBX | Property Tracks")
	EInterchangeAnimationPayLoadType Type = EInterchangeAnimationPayLoadType::NONE;

	/** Return the property name of the enum as a string, see InterchangeAnimationDefinitions.h */
	FName GetPropertyName();
};

UCLASS(config = Interchange, meta = (DisplayName = "Interchange FBX"))
class INTERCHANGEFBXPARSER_API UInterchangeFbxSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UInterchangeFbxSettings();

	/** Search for a predefined property track, if the property has been found it returns it, otherwise we search for a custom property track*/
	FInterchangePropertyTracksSettings GetPropertyTrack(const FString& PropertyName) const;

	UPROPERTY(EditAnywhere, config, Category = "FBX | Property Tracks")
	TMap<FString, FInterchangePropertyTracksSettings> CustomPropertyTracks;

private:
	TMap<FString, FInterchangePropertyTracksSettings> PredefinedPropertyTracks;
};
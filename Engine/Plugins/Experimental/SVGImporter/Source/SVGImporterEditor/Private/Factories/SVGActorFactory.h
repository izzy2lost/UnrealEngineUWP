// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "SVGActorFactory.generated.h"

UCLASS()
class SVGIMPORTEREDITOR_API USVGActorFactory : public UActorFactory
{
	GENERATED_BODY()
	
	USVGActorFactory(const FObjectInitializer& ObjectInitializer);

protected:
	//~ Begin UActorFactory
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual AActor* GetDefaultActor(const FAssetData& AssetData) override;
	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams) override;
	//~ End UActorFactory
};

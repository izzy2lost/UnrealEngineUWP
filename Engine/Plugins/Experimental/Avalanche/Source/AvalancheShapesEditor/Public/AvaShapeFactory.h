// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "AvaShapeFactory.generated.h"

class UAvaShapeDynamicMeshBase;

UCLASS()
class AVALANCHESHAPESEDITOR_API UAvaShapeFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UAvaShapeFactory();

	void SetMeshClass(TSubclassOf<UAvaShapeDynamicMeshBase> InMeshClass);

protected:
	TSubclassOf<UAvaShapeDynamicMeshBase> MeshClass;

	//~ Begin UActorFactory
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;

	virtual AActor* GetDefaultActor(const FAssetData& AssetData) override;

	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams) override;
	//~ End UActorFactory
};

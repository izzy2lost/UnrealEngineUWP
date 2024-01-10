// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorPartition/PartitionActor.h"
#include "InstancedActorsManager.generated.h"


UCLASS(Config=Game)
class INSTANCEDACTORS_API AInstancedActorsManager : public APartitionActor
{
	GENERATED_BODY()

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
#endif
};

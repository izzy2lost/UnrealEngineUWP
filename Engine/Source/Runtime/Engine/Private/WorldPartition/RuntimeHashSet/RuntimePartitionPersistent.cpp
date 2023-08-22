// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartitionPersistent.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"

#if WITH_EDITOR
bool URuntimePartitionPersistent::GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult)
{
	UWorldPartition* WorldPartition = GetTypedOuter<UWorldPartition>();
	UWorld* World = WorldPartition->GetWorld();
	UWorld* OuterWorld = GetTypedOuter<UWorld>();
	const bool bIsMainWorldPartition = (World == OuterWorld);

	TArray<IStreamingGenerationContext::FActorInstance> CellActorInstances;
	if (PopulateCellActorInstances(*InParams.ActorSetInstances, bIsMainWorldPartition, true, CellActorInstances))
	{
		const FString CellName(TEXT("Persistent"));
		OutResult.RuntimeCellDescs.Emplace(CreateCellDesc(CellName, false, CellActorInstances[0].ActorSetInstance->ContentBundleID, 0, CellActorInstances));
	}

	return true;
}
#endif
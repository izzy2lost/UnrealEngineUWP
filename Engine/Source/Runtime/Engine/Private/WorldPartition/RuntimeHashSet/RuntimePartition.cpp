// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"

#if WITH_EDITOR
void URuntimePartition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(URuntimePartition, LoadingRange))
	{
		LoadingRange = FMath::Max(LoadingRange, 0);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool URuntimePartition::PopulateCellActorInstances(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, bool bIsMainWorldPartition, bool bIsCellAlwaysLoaded, TArray<IStreamingGenerationContext::FActorInstance>& OutCellActorInstances)
{
	UWorldPartitionRuntimeHash* RuntimeHash = GetTypedOuter<UWorldPartitionRuntimeHash>();
	return RuntimeHash->PopulateCellActorInstances(ActorSetInstances, bIsMainWorldPartition, bIsCellAlwaysLoaded, OutCellActorInstances);
}
#endif
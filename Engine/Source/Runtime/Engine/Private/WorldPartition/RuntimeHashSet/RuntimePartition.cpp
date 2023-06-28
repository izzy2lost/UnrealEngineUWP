// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"

#if WITH_EDITOR
void URuntimePartition::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(URuntimePartition, LoadingRange))
	{
		LoadingRange = FMath::Max(LoadingRange, 0);
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

bool URuntimePartition::PopulateCellActorInstances(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& InActorSetInstances, bool bInIsMainWorldPartition, bool bInIsCellAlwaysLoaded, TArray<IStreamingGenerationContext::FActorInstance>& OutCellActorInstances)
{
	UWorldPartitionRuntimeHash* RuntimeHash = GetTypedOuter<UWorldPartitionRuntimeHash>();
	return RuntimeHash->PopulateCellActorInstances(InActorSetInstances, bInIsMainWorldPartition, bInIsCellAlwaysLoaded, OutCellActorInstances);
}

URuntimePartition::FCellDesc URuntimePartition::CreateCellDesc(const FString& InName, bool bInIsSpatiallyLoaded, const FGuid& InContentBundleID, int32 InLevel, const TArray<IStreamingGenerationContext::FActorInstance>& InActorInstances)
{
	FCellDesc CellDesc;

	// Construct a unique name like this: PartitionName_CellName
	TStringBuilder<512> StringBuilder;
	StringBuilder += Name.ToString();
	StringBuilder += TEXT("_");
	StringBuilder += InName;
	CellDesc.Name = *StringBuilder;

	// Copy values coming from this partition
	CellDesc.bBlockOnSlowStreaming = bBlockOnSlowStreaming;
	CellDesc.bClientOnlyVisible = bClientOnlyVisible;
	CellDesc.Priority = Priority;

	// Set provided input values
	CellDesc.bIsSpatiallyLoaded = bInIsSpatiallyLoaded;
	CellDesc.ContentBundleID = InContentBundleID;
	CellDesc.Level = InLevel;

	// Add actor instances and update bounds
	CellDesc.ActorInstances = InActorInstances;
	for (const IStreamingGenerationContext::FActorInstance& ActorInstance : CellDesc.ActorInstances)
	{
		const FWorldPartitionActorDescView& ActorDescView = ActorInstance.GetActorDescView();
		const FBox RuntimeBounds = ActorDescView.GetRuntimeBounds();

		if (RuntimeBounds.IsValid)
		{
			CellDesc.Bounds += RuntimeBounds.TransformBy(ActorInstance.GetTransform());
		}
	}

	return CellDesc;
}
#endif
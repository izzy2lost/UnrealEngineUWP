// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "WorldPartition/RuntimeHashSet/WorldPartitionRuntimeHashSet.h"

#if WITH_EDITOR
void URuntimePartition::SetDefaultValues()
{
	Name = GetClass()->GetFName();
	bBlockOnSlowStreaming = false;
	bClientOnlyVisible = false;
	Priority = 0;
	LoadingRange = 25600;
	bIsHLODSetup = false;
}

void URuntimePartition::InitHLODRuntimePartitionFrom(const URuntimePartition* RuntimePartition, int32 HLODIndex)
{
	Name = *FString::Printf(TEXT("%s_HLOD%d"), *RuntimePartition->Name.ToString(), HLODIndex);
	LoadingRange = RuntimePartition->LoadingRange * 2;
	bIsHLODSetup = true;
}

void URuntimePartition::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(URuntimePartition, LoadingRange))
	{
		LoadingRange = FMath::Max(LoadingRange, 0);
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}

URuntimePartition* URuntimePartition::CreateHLODRuntimePartition(int32 HLODIndex) const
{
	URuntimePartition* HLODRuntimePartition = DuplicateObject<URuntimePartition>(this, GetOuter());
	HLODRuntimePartition->InitHLODRuntimePartitionFrom(this, HLODIndex);
	return HLODRuntimePartition;
}

URuntimePartition::FCellDesc URuntimePartition::CreateCellDesc(const FString& InName, bool bInIsSpatiallyLoaded, int32 InLevel, const TArray<const IStreamingGenerationContext::FActorSetInstance*>& InActorSetInstances)
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
	CellDesc.Level = InLevel;

	// Add actor set instances
	CellDesc.ActorSetInstances = InActorSetInstances;

	// Update cell bounds
	for (const IStreamingGenerationContext::FActorSetInstance* ActorSetInstance : InActorSetInstances)
	{
		CellDesc.Bounds += ActorSetInstance->Bounds;
	}

	return CellDesc;
}
#endif
// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionEditorSettings.h"
#include "WorldPartition/WorldPartition.h"

void UWorldPartitionEditorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UWorldPartitionEditorSettings, bEnableLoadingInEditor))
	{	
		if (UWorldPartition* WorldPartition = GWorld ? GWorld->GetWorldPartition() : nullptr)
		{
			WorldPartition->OnEnableLoadingInEditorChanged();
		}
	}
}
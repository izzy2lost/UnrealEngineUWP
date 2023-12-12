// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"

#if WITH_EDITOR

const FStreamingGenerationActorDescView& IStreamingGenerationContext::FActorInstance::GetActorDescView() const
{
	return ActorSetInstance->ActorSetContainerInstance->ActorDescViewMap->FindByGuidChecked(ActorGuid);
}

const FActorContainerID& IStreamingGenerationContext::FActorInstance::GetContainerID() const
{
	return ActorSetInstance->ContainerID;
}

const FTransform& IStreamingGenerationContext::FActorInstance::GetTransform() const
{
	return ActorSetInstance->Transform;
}

const FBox IStreamingGenerationContext::FActorInstance::GetBounds() const
{
	return ActorSetInstance->Bounds;
}
#endif

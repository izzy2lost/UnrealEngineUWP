// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"

class FHLODActorDesc;

class FLoaderAdapterHLOD : public FLoaderAdapterActorList
{
public:
	FLoaderAdapterHLOD(UWorld* InWorld);

	virtual bool PassActorDescFilter(const FWorldPartitionHandle& ActorHandle) const override;

private:
	bool ShouldLoadHLOD(const FHLODActorDesc& HLODActorDesc);	
};

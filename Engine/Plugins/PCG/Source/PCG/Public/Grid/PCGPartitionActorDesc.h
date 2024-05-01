// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Containers/Map.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"

class FPCGPartitionActorDesc : public FPartitionActorDesc
{
public:
	FPCGPartitionActorDesc() = default;
protected:
	virtual void Init(const AActor* InActor) override;
	virtual uint32 GetSizeOf() const override { return sizeof(FPCGPartitionActorDesc); }
	virtual void Serialize(FArchive& Ar) override;

private:
	friend class FPCGActorAndComponentMapping;

	bool bRequiresGuidFixup = false;
	bool bInvalid = false;
};
#endif
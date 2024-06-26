// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CollisionProfile.h"
#include "ISMPartition/ISMComponentDescriptor.h"

#include "PCGISMDescriptor.generated.h"

/** Convenience PCG-side component descriptor so we can adjust defaults to the most common use cases. */
USTRUCT()
struct FPCGSoftISMComponentDescriptor : public FSoftISMComponentDescriptor
{
	GENERATED_BODY()

	PCG_API FPCGSoftISMComponentDescriptor();

public:
	PCG_API bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

template<>
struct TStructOpsTypeTraits<FPCGSoftISMComponentDescriptor> : public TStructOpsTypeTraitsBase2<FPCGSoftISMComponentDescriptor>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};
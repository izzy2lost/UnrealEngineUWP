// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamId.h"
#include "Param/ParamCompatibility.h"

struct FInstancedPropertyBag;

namespace UE::AnimNext
{
	struct FParamResult;
	struct FParamTypeHandle;
}

namespace UE::AnimNext::Private
{
	struct FParamEntry;
}

namespace UE::AnimNext
{

// Stack layers are what actually get pushed/popped onto the layer stack.
// They are designed to be held on an instance, their data updated in place.
// Memory ownership for params is variable depending on the layer's subclass
struct FParamStackLayer
{
	FParamStackLayer() = default;
	virtual ~FParamStackLayer();

	friend struct FParamStack;
	friend struct FParamStackLayerHandle;

	explicit FParamStackLayer(TConstArrayView<TPair<FParamId, Private::FParamEntry>> InParams);

	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData) const;
	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;

	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData);
	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());

	ANIMNEXT_API virtual UObject* AsUObject() { return nullptr; }
	ANIMNEXT_API virtual FInstancedPropertyBag* AsInstancedPropertyBag() { return nullptr; }

	// Params that this layer supplies
	TArray<Private::FParamEntry> Params;

	// ID that the param indices start at. Maps the global param ID range into the range for this layer
	uint32 MinParamId = 0;

	// Storage offset for this layer if it is owned internally, otherwise MAX_uint32
	uint32 OwnedStorageOffset = MAX_uint32;
};

}
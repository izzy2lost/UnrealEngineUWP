// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamCompatibility.h"
#include "UObject/Interface.h"
#include "Param/ParamDefinition.h"
#include "Param/ParamResult.h"

struct FAnimNextParamType;

namespace UE::AnimNext
{
	struct FParamContext;
	struct FParamStackLayerHandle;
	struct FParamDefinition;
}

namespace UE::AnimNext
{

using FObjectAdapterFunction = TUniqueFunction<uint8*(UObject*, FParamId)>;

/**
 * Adapter for a specific parameter. This is designed to wrap native objects and their parameters for the parameter
 * system. Adapters are deferred to by the param stack if they exist for a non-overriden parameter ID 
 */
struct FParamAdapter
{
	FParamAdapter(FParamDefinition&& InDefinition, FObjectAdapterFunction&& InFunction)
		: Definition(MoveTemp(InDefinition))
		, Function(MoveTemp(InFunction))
	{
	}

	// Get the parameter's data for this adapter
	FParamResult GetParamData(FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const;

	// Get the parameter's mutable data for this adapter
	FParamResult GetMutableParamData(FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const;

	FParamDefinition Definition;

	FObjectAdapterFunction Function;
	
	uint32 BufferOffset = MAX_uint32;
};

}

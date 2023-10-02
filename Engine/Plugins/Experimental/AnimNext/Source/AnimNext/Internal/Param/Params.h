// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamDefinition.h"

namespace UE::AnimNext
{
	struct FParameterAdapterCollection;
}

namespace UE::AnimNext
{

// Built-in parameter handling
struct FParams
{
	// Register a single built-in parameter
	static FParamDefinition RegisterBuiltInParameter(const FParamDefinition& InDefinition);

	// Unregister a built-in parameter
	static void UnregisterBuiltInParameter(FName InName);

	// Check whether a parameter name corresponds to a built-in parameter
	static bool IsBuiltInParameter(FName InName);

	// Get the number of registered built-in parameters
	static int32 GetNumBuiltInParameters();

	// Gets a built-in parameter definition. Asserts if it is not present
	static const FParamDefinition& GetBuiltInParameter(FName InName);

	// Finds a built-in parameter definition
	static ANIMNEXT_API const FParamDefinition* FindBuiltInParameter(FName InName);

	// Iterates over all built-in parameters
	static ANIMNEXT_API void ForEachBuiltInParameter(TFunctionRef<void(const FParamDefinition&)> InFunction);
};

}
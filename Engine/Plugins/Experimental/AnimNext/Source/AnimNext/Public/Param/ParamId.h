// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextConfig;
struct FRigVMDispatch_SetLayerParameter;
struct FRigVMDispatch_GetLayerParameter;
struct FRigVMDispatch_GetParameter;

namespace UE::AnimNext
{
	struct FParamAdapter;
	struct FParamDefinition;
	class FModule;
	struct FRemappedLayer;
}

namespace UE::AnimNext::Tests
{
	class FParamStackTest;
}

namespace UE::AnimNext
{

// Global identifier used to index into dense parameter arrays
struct ANIMNEXT_API FParamId
{
	friend struct FParamStack;
	friend class Tests::FParamStackTest;
	friend struct ::FRigVMDispatch_SetLayerParameter;
	friend struct ::FRigVMDispatch_GetLayerParameter;
	friend struct ::FRigVMDispatch_GetParameter;
	friend struct FParamAdapter;
	friend class ::UAnimNextConfig;
	friend struct FParamDefinition;
	friend class FModule;
	friend struct FRemappedLayer;

	static constexpr uint32 InvalidIndex = MAX_uint32;

	FParamId() = default;
	FParamId(const FParamId& InName) = default;
	FParamId& operator=(const FParamId& InName) = default;

	// Make a parameter ID from an FName
	explicit FParamId(FName InName);

	// Get the index of this param
	uint32 ToInt() const
	{
		return ParameterIndex;
	}

	// Get the name that this parameter was created from
	FName ToName() const;

	// Check if this ID represents a valid parameter
	bool IsValid() const
	{
		return ParameterIndex != InvalidIndex;
	}

private:
	// Make a parameter ID from an index (for internal use)
	explicit FParamId(uint32 InParameterIndex)
		: ParameterIndex(InParameterIndex)
	{
	}

	// Make a new parameter ID (internal usage only)
	static uint32 MakeParamId_NoLock(FName InName);

	// Initialize the parameter ID system
	static void Init();

	// Shut down the parameter ID system
	static void Destroy();

	// Refresh all adapters, built in and per-config
	static void RefreshAdapters();

	// Resets/clears all adapters
	static void ResetAdapters();

	// Register all built-in adapters
	static void RegisterBuiltInAdapters();
	
	// Refresh all adapters defined in config
	static void RefreshConfigAdapters();
	
	// Get the maximum parameter ID that can exist at present
	// Note that this can change via concurrent modifications when new parameters are created by 
	// different threads
	static FParamId GetMaxParamId();

	// Get any built in adapter for the supplied parameter
	static const FParamAdapter* GetAdapter(FParamId InId);

	// Get the thread-local scratch area for an adapted parameter id
	static uint8* GetScratchAreaForParamIdAdapter(FParamId InId);

	// Get the thread-local return value for an adapted parameter id
	template<typename ReturnType>
	static ReturnType* GetAdapterReturnValue(FParamId InId)
	{
		return reinterpret_cast<ReturnType*>(GetScratchAreaForParamIdAdapter(InId));
	}
	
#if WITH_DEV_AUTOMATION_TESTS
	// Used to isolate global param IDs from automated tests
	static void BeginTestSandbox();
	static void EndTestSandbox();
#endif

private:
	// Stable index
	uint32 ParameterIndex = InvalidIndex;
};

} // end namespace UE::AnimNext

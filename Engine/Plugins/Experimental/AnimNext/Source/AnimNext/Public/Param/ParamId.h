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

// Global identifier used to avoid re-hashing parameter names
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

	FParamId() = default;
	FParamId(const FParamId& InName) = default;
	FParamId& operator=(const FParamId& InName) = default;

	// Make a parameter ID from an FName, generating the hash
	explicit FParamId(FName InName);

	// Make a parameter ID from a name and hash
	explicit FParamId(FName InName, uint32 InHash);

	// Get the name of this param
	FName GetName() const
	{
		return Name;
	}

	// Get the hash of this param
	uint32 GetHash() const
	{
		return Hash;
	}

	// Check if this ID represents a valid parameter
	bool IsValid() const
	{
		return Hash != 0;
	}

private:
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

private:
	// Parameter name
	FName Name;

	// Name hash
	uint32 Hash = 0;
};

} // end namespace UE::AnimNext

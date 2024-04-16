// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextConfig;
struct FRigVMDispatch_SetLayerParameter;
struct FRigVMDispatch_GetLayerParameter;
struct FRigVMDispatch_GetParameter;

namespace UE::AnimNext
{
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
	friend class ::UAnimNextConfig;
	friend struct FParamDefinition;
	friend class FModule;
	friend struct FRemappedLayer;

	FParamId() = default;
	FParamId(const FParamId& InName) = default;
	FParamId& operator=(const FParamId& InName) = default;

	// Make a parameter ID from an FName, generating the hash
	explicit FParamId(FName InName)
		: Name(InName)
		, InstanceId(NAME_None)
		, Hash(CalculateHash(InName, NAME_None))
	{
	}

	// Make a parameter ID from an FName and instance ID, generating the hash
	explicit FParamId(FName InName, FName InInstanceId)
		: Name(InName)
		, InstanceId(InInstanceId)
		, Hash(CalculateHash(InName, InInstanceId))
	{
	}

	// Make a parameter ID from a name and hash
	explicit FParamId(FName InName, uint32 InHash)
		: Name(InName)
		, Hash(InHash)
	{
		checkSlow(CalculateHash(InName, NAME_None) == InHash);
	}

	// Make a parameter ID from a name, an instance ID and hash
	explicit FParamId(FName InName, FName InInstanceId, uint32 InHash)
		: Name(InName)
		, InstanceId(InInstanceId)
		, Hash(InHash)
	{
		checkSlow(CalculateHash(InName, InInstanceId) == InHash);
	}

	// Get the name of this param
	FName GetName() const
	{
		return Name;
	}

	// Get the instance ID of this param
	FName GetInstanceId() const
	{
		return InstanceId;
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

	// Get the hash of a parameter name/instance ID combination
	static uint32 CalculateHash(FName InName, FName InInstanceId)
	{
		return HashCombineFast(GetTypeHash(InName), GetTypeHash(InInstanceId));
	}
	
private:
	// Parameter name
	FName Name;

	// Parameter instance ID
	FName InstanceId;

	// Name hash
	uint32 Hash = 0;
};

} // end namespace UE::AnimNext

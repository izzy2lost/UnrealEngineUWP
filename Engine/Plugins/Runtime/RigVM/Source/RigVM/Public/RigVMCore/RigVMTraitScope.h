// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

struct FRigVMTrait;
class UScriptStruct;

class RIGVM_API FRigVMTraitScope
{
public:
	FRigVMTraitScope()
		: Trait(nullptr)
		, ScriptStruct(nullptr)
	{
	}
	
	FRigVMTraitScope(FRigVMTrait* InTrait, const UScriptStruct* InScriptStruct)
		: Trait(InTrait)
		, ScriptStruct(InScriptStruct)
	{
	}

	bool IsValid() const
	{
		return (Trait != nullptr) && (ScriptStruct != nullptr);
	}

	template<typename T>
	bool IsA() const
	{
		return ScriptStruct->IsChildOf(T::StaticStruct());
	}

	template<typename T = FRigVMTrait>
	const T* GetTrait() const
	{
		if(IsA<T>())
		{
			return static_cast<T*>(Trait);
		}
		return nullptr;
	}

	template<typename T = FRigVMTrait>
	const T* GetTraitChecked() const
	{
		check(IsA<T>());
		return static_cast<T*>(Trait);
	}

	template<typename T = FRigVMTrait>
	T* GetTrait()
	{
		if(IsA<T>())
		{
			return static_cast<T*>(Trait);
		}
		return nullptr;
	}

	template<typename T = FRigVMTrait>
	T* GetTraitChecked()
	{
		check(IsA<T>());
		return static_cast<T*>(Trait);
	}

	const UScriptStruct* GetScriptStruct() const
	{
		return ScriptStruct;
	}

private:

	FRigVMTrait* Trait;
	const UScriptStruct* ScriptStruct;
};
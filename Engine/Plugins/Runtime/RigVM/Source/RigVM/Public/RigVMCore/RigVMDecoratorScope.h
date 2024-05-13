// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

struct FRigVMDecorator;
class UScriptStruct;

class RIGVM_API FRigVMDecoratorScope
{
public:
	FRigVMDecoratorScope()
		: Decorator(nullptr)
		, ScriptStruct(nullptr)
	{
	}
	
	FRigVMDecoratorScope(FRigVMDecorator* InDecorator, const UScriptStruct* InScriptStruct)
		: Decorator(InDecorator)
		, ScriptStruct(InScriptStruct)
	{
	}

	bool IsValid() const
	{
		return (Decorator != nullptr) && (ScriptStruct != nullptr);
	}

	template<typename T>
	bool IsA() const
	{
		return ScriptStruct->IsChildOf(T::StaticStruct());
	}

	template<typename T = FRigVMDecorator>
	const T* GetDecorator() const
	{
		if(IsA<T>())
		{
			return static_cast<T*>(Decorator);
		}
		return nullptr;
	}

	template<typename T = FRigVMDecorator>
	const T* GetDecoratorChecked() const
	{
		check(IsA<T>());
		return static_cast<T*>(Decorator);
	}

	template<typename T = FRigVMDecorator>
	T* GetDecorator()
	{
		if(IsA<T>())
		{
			return static_cast<T*>(Decorator);
		}
		return nullptr;
	}

	template<typename T = FRigVMDecorator>
	T* GetDecoratorChecked()
	{
		check(IsA<T>());
		return static_cast<T*>(Decorator);
	}

	const UScriptStruct* GetScriptStruct() const
	{
		return ScriptStruct;
	}

private:

	FRigVMDecorator* Decorator;
	const UScriptStruct* ScriptStruct;
};
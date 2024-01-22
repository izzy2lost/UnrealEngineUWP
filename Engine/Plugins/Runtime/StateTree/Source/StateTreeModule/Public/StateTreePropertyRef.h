// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeIndexTypes.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyRef.generated.h"

/**
 * Property ref allows to get a pointer to selected property in StateTree.
 * The expected type of the reference should be set in "RefType" meta specifier.
 *
 * Meta specifiers for the type:
 *  - RefType = "<type>"
 *		- Specifies the type of property to reference.
 *		- Supported types are: bool, byte, int32, int64, float, double, Name, String, Text, UObject pointers, and structs.
 *  - IsRefToArray
 *		- If specified, the reference is to an TArray<RefType>
 *  - Optional
 *		- If specified, the reference can be left unbound, otherwise the compiler report error if the reference is not bound.
 *
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere, meta = (RefType = "float"))
 *	FStateTreePropertyRef RefToFloat;
 * 
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase"))
 *	FStateTreePropertyRef RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere, meta = (RefType = "/Script/ModuleName.TestStructBase", IsRefToArray))
 *	FStateTreePropertyRef RefToArrayOfTests;
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreePropertyRef
{
	GENERATED_BODY()

	FStateTreePropertyRef() = default;

	/** @return pointer to the property if possible, nullptr otherwise. */
	template<class T>
	T* GetMutablePtr(FStateTreeExecutionContext& Context) const
	{
		return Context.GetMutablePropertyPtr<T>(*this);
	}

	/**
	 * Used internally.
	 * @return index to referenced property access
	 */
	FStateTreeIndex16 GetRefAccessIndex() const
	{
		return RefAccessIndex;
	}

private:
	UPROPERTY()
	FStateTreeIndex16 RefAccessIndex;

	friend FStateTreePropertyBindingCompiler;
};

/**
 * TStateTreePropertyRef is a type-safe FStateTreePropertyRef wrapper against the given type.
 * @note When used as a property, this automatically defines PropertyRef property meta-data.
 * 
 * Example:
 *
 *  // Reference to float
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<float> RefToFloat;
 * 
 *  // Reference to FTestStructBase
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<FTestStructBase> RefToTest;
 *
 *  // Reference to TArray<FTestStructBase>
 *	UPROPERTY(EditAnywhere)
 *	TStateTreePropertyRef<TArray<FTestStructBase>> RefToArrayOfTests;
 */
template<class TRef>
struct TStateTreePropertyRef
{
	/** @return pointer to the property if possible, nullptr otherwise. */
	TRef* GetMutablePtr(FStateTreeExecutionContext& Context) const
	{
		return PropertyRef.GetMutablePtr<TRef>(Context);
	}

private:
	FStateTreePropertyRef PropertyRef;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FAnimNextParamType;
struct FUniversalObjectLocator;

namespace UE::AnimNext
{

struct FParamTypeHandle;
struct FParamCompatibility;

struct FParamUtils
{
	// Check to see if a parameter type handle is compatible with another. The order of the parameters
	// imply directionality, e.g. IsCompatible((int64)InLHS, (int32)InRHS) is allowed as no data loss 
	// occurs, but IsCompatible((int32)InLHS, (int64)InRHS) is not as B could be truncated.
	static ANIMNEXT_API FParamCompatibility GetCompatibility(const FParamTypeHandle& InLHS, const FParamTypeHandle& InRHS);

	// Check to see if a parameter handle is compatible with another. The order of the parameters
	// imply directionality, e.g. IsCompatible((int64)InLHS, (int32)InRHS) is allowed as no data loss 
	// occurs, but IsCompatible((int32)InLHS, (int64)InRHS) is not as B could be truncated.
	static ANIMNEXT_API FParamCompatibility GetCompatibility(const FAnimNextParamType& InLHS, const FAnimNextParamType& InRHS);

	// Check whether the supplied function can be used to access/map parameters
	static ANIMNEXT_API bool CanUseFunction(const UFunction* InFunction);

	// Check whether the supplied property can be used to access/map parameters
	static ANIMNEXT_API bool CanUseProperty(const FProperty* InProperty);

	// Convert a UOL to an FName
	static ANIMNEXT_API FName LocatorToName(const FUniversalObjectLocator& InLocator);
};

}
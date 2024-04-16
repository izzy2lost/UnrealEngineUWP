// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Param/AnimNextParamInstanceIdentifier.h"

template<typename T> struct TInstancedStruct;

namespace UE::AnimNext::UncookedOnly
{
	struct FParameterSourceInfo;
	class IParameterSourceType;
}

namespace UE::AnimNext::UncookedOnly
{

class IAnimNextUncookedOnlyModule : public IModuleInterface
{
public:
	// Register a parameter source type that can be used to query information about parameter sources
	// @param   InInstanceIdStruct     The name of struct type for this parameter source's instance ID (must be a child struct of FAnimNextParamInstanceIdentifier)
	// @param   InFactory              The type to register
	virtual void RegisterParameterSourceType(const UScriptStruct* InInstanceIdStruct, TSharedPtr<IParameterSourceType> InType) = 0;
	
	// Unregister a parameter source type previously passed to RegisterParameterSourceType
	// @param   InInstanceIdStruct     The name of struct type for this parameter source's instance ID
	virtual void UnregisterParameterSourceType(const UScriptStruct* InInstanceIdStruct) = 0;

	// Find a parameter source type previously passed to RegisterParameterSourceType
	// @param   InInstanceIdStruct     The name of struct type for this parameter source's instance ID
	virtual TSharedPtr<IParameterSourceType> FindParameterSourceType(const UScriptStruct* InInstanceIdStruct) const = 0;
};

}

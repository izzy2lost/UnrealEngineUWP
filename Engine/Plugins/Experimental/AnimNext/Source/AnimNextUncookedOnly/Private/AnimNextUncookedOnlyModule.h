// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextUncookedOnlyModule.h"

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::UncookedOnly
{

class FModule : public IAnimNextUncookedOnlyModule
{
private:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IAnimNextUncookedOnlyModule interface
	virtual void RegisterParameterSourceType(const UScriptStruct* InInstanceIdStruct, TSharedPtr<IParameterSourceType> InType) override;
	virtual void UnregisterParameterSourceType(const UScriptStruct* InInstanceIdStruct) override;
	virtual TSharedPtr<IParameterSourceType> FindParameterSourceType(const UScriptStruct* InInstanceIdStruct) const override;

	TMap<const UScriptStruct*, TSharedPtr<IParameterSourceType>> ParameterSourceTypes;
	FDelegateHandle OnGetExtraObjectTagsHandle;

	friend struct FUtils;
};

}
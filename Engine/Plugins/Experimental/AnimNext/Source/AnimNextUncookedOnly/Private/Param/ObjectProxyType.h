// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/IParameterSourceType.h"

namespace UE::AnimNext::UncookedOnly
{

// Provides information about object proxy parameter sources
class FObjectProxyType : public IParameterSourceType
{
	// IParameterSourceType interface
	virtual const UStruct* GetStruct(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const override;
	virtual FText GetDisplayText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const override;
	virtual FText GetTooltipText(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId) const override;
	virtual bool FindParameterInfo(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InParameterNames, TArrayView<FParameterSourceInfo> OutInfo) const override;
	virtual void ForEachParameter(const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TFunctionRef<void(FName, const FParameterSourceInfo&)> InFunction) const override;
};

}
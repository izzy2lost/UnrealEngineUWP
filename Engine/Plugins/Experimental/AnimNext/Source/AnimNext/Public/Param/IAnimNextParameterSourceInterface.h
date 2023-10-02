// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Param/ParamStack.h"
#include "IAnimNextParameterSourceInterface.generated.h"

namespace UE::AnimNext
{
	struct FParamContext;
}

UINTERFACE(MinimalAPI, NotBlueprintable)
class UAnimNextParameterSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/** An interface used to apply AnimNext parameters to a context */
class IAnimNextParameterSourceInterface
{
	GENERATED_BODY()

public:
	/** Called back by the system to apply this interface's parameters to the layer */
	virtual void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle) const = 0;

	/** Cache a (immutable) layer handle for this parameter source */
	virtual UE::AnimNext::FParamStackLayerHandle CacheLayer() const = 0;
	
	/* Whether to cache the supplied layer (e.g. if the layout changes dynamically in editor or the handle is invalid) */
	virtual bool ShouldCacheLayer(const UE::AnimNext::FParamStackLayerHandle& InHandle) const { return !InHandle.IsValid(); }
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "PropertyBag.h"
#include "UObject/StrongObjectPtr.h"

class UAnimNextGraph;

namespace UE::AnimNext
{

// Proxy struct used to reference parameter instance data
struct FParametersProxy : public IParameterSource
{
	FParametersProxy() = delete;

	FParametersProxy(UAnimNextGraph* InGraph);

	// IParameterSource interface
	virtual void Update(float DeltaTime) override;
	virtual const FParamStackLayerHandle& GetLayerHandle() const override { return LayerHandle; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// The object that this proxy wraps
	TObjectPtr<UAnimNextGraph> Graph;

	// Copy of the parameter data
	FInstancedPropertyBag PropertyBag;

	// Layer handle - must be updated if PropertyBag changes layout
	FParamStackLayerHandle LayerHandle;
};

}
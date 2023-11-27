// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "PropertyBag.h"

namespace UE::AnimNext
{

// Proxy struct used to hold data in a property bag
struct FPropertyBagProxy : public IParameterSource
{
	FPropertyBagProxy();

	// IParameterSource interface
	virtual void Update() override {};
	virtual const FParamStackLayerHandle& GetLayerHandle() const override { return LayerHandle; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Adds (or replaces) a property with the specified name and sets its value using the specified container
	void AddPropertyAndValue(FName InName, const FProperty* InProperty, const void* InContainerPtr);

private:
	// Property bag that this wraps
	FInstancedPropertyBag PropertyBag;

	// Layer handle - must be updated if PropertyBag is changed
	FParamStackLayerHandle LayerHandle;
};

}
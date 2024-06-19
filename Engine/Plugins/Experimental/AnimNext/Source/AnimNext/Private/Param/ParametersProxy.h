// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/StrongObjectPtr.h"

class UAnimNextModule;

namespace UE::AnimNext
{

// Proxy struct used to reference parameter instance data
struct FParametersProxy : public IParameterSource
{
	FParametersProxy() = delete;

	FParametersProxy(const UAnimNextModule* InModule);

	// IParameterSource interface
	virtual FName GetInstanceId() const override { return NAME_None; }
	virtual void Update(float DeltaTime) override;
	virtual const FParamStackLayerHandle& GetLayerHandle() const override { return LayerHandle; }
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Update the external param copy data we use to maintain the visible state of external parameters
	void UpdateCachedExternalParamData();

	// The object that this proxy wraps
	TObjectPtr<const UAnimNextModule> Module;

	// Copy of the parameter data
	FInstancedPropertyBag PropertyBag;

	// Layer handle - must be updated if PropertyBag changes layout
	FParamStackLayerHandle LayerHandle;

	// Data for an external parameter copy
	struct FExternalParamData
	{
		FExternalParamData() = default;
		
		FExternalParamData(const FParamId& InParamId, const FParamTypeHandle& InTypeHandle, TArrayView<uint8> InInternalData)
			: ParamId(InParamId)
			, TypeHandle(InTypeHandle)
			, InternalData(InInternalData)
		{
		}

		// ID for the param
		FParamId ParamId;
		// Type of the data
		FParamTypeHandle TypeHandle;
		// The data held in the PropertyBag for this param
		TArrayView<uint8> InternalData;
	};

	// All external params we will be copying pre-update
	TArray<FExternalParamData> ExternalParamData;
};

}
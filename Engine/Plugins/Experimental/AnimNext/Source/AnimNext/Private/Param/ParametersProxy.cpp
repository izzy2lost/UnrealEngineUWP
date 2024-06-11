// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParametersProxy.h"

#include "ParamHelpers.h"
#include "Graph/AnimNextGraphState.h"
#include "Module/AnimNextModule.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FParametersProxy::FParametersProxy(const UAnimNextModule* InModule)
	: Module(InModule)
	, PropertyBag(InModule->DefaultState.State)
	, LayerHandle(FParamStack::MakeReferenceLayer(NAME_None, PropertyBag))
{
	check(Module);
	UpdateCachedExternalParamData();
}

void FParametersProxy::Update(float DeltaTime)
{
#if WITH_EDITOR	// Layout should only be changing in editor
	const FInstancedPropertyBag* HandlePropertyBag = LayerHandle.As<FInstancedPropertyBag>();
	if(HandlePropertyBag == nullptr || HandlePropertyBag->GetPropertyBagStruct() != Module->DefaultState.State.GetPropertyBagStruct())
	{
		PropertyBag = Module->DefaultState.State;
		LayerHandle = FParamStack::MakeReferenceLayer(NAME_None, PropertyBag);
		UpdateCachedExternalParamData();
	}
#endif

	// First of all we update public state from external sources, if any.
	// This is to ensure that when running user-defined layer update logic in UpdateLayer()
	// that the visible state is consistent with the external source.
	if(ExternalParamData.Num() > 0)
	{
		FParamStack& ParamStack = FParamStack::Get();
		for(const FExternalParamData& ExternalParam : ExternalParamData)
		{
			TConstArrayView<uint8> Data;
			if(ParamStack.GetParamData(ExternalParam.ParamId, ExternalParam.TypeHandle, Data).IsSuccessful())
			{
				FParamHelpers::Copy(ExternalParam.TypeHandle, Data, ExternalParam.InternalData);
			}
		}
	}

	// Next we update the layer
	Module->UpdateLayer(LayerHandle, DeltaTime);
}

void FParametersProxy::UpdateCachedExternalParamData()
{
	ExternalParamData.Reset();

	if(Module->DefaultState.PublicParameterStartIndex != INDEX_NONE)
	{
		TConstArrayView<FPropertyBagPropertyDesc> Descs = Module->DefaultState.State.GetPropertyBagStruct()->GetPropertyDescs();
		const int32 NumProperties = Descs.Num();
		ExternalParamData.Reserve(NumProperties - Module->DefaultState.PublicParameterStartIndex);
		for(int32 PropertyIndex = Module->DefaultState.PublicParameterStartIndex; PropertyIndex < NumProperties; ++PropertyIndex)
		{
			const FPropertyBagPropertyDesc& Desc = Descs[PropertyIndex];
			check(Desc.CachedProperty);
			uint8* ValuePtr = Desc.CachedProperty->ContainerPtrToValuePtr<uint8>(PropertyBag.GetMutableValue().GetMemory());
			TArrayView<uint8> InternalData(ValuePtr, Desc.CachedProperty->GetSize());
			ExternalParamData.Emplace(FParamId(Desc.Name), FAnimNextParamType(Desc.ValueType, Desc.ContainerTypes.GetFirstContainerType(), Desc.ValueTypeObject).GetHandle(), InternalData);
		}
	}
}

void FParametersProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Module);
	PropertyBag.AddStructReferencedObjects(Collector);
}

}

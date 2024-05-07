// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/PropertyBagProxy.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FPropertyBagProxy::FPropertyBagProxy(FName InInstanceId)
	: InstanceId(InInstanceId)
{
	LayerHandle = FParamStack::MakeReferenceLayer(NAME_None, PropertyBag);
}

FPropertyBagProxy::FPropertyBagProxy(FName InInstanceId, FInstancedPropertyBag&& InPropertyBag)
	: PropertyBag(MoveTemp(InPropertyBag))
	, InstanceId(InInstanceId)
{
	LayerHandle = FParamStack::MakeReferenceLayer(NAME_None, PropertyBag);
}

FName FPropertyBagProxy::GetInstanceId() const
{
	return InstanceId;
}

void FPropertyBagProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	PropertyBag.AddStructReferencedObjects(Collector);
}

void FPropertyBagProxy::ReplaceAllParameters(TConstArrayView<FPropertyBagPropertyDesc> InDescs, TConstArrayView<TConstArrayView<uint8>> InValues)
{
	PropertyBag.ReplaceAllPropertiesAndValues(InDescs, InValues);

	// Recreate the layer handle as the bag layout has changed
	LayerHandle = FParamStack::MakeReferenceLayer(NAME_None, PropertyBag);
}

}

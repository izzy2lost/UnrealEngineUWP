// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/PropertyBagProxy.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FPropertyBagProxy::FPropertyBagProxy()
{
	LayerHandle = FParamStack::MakeReferenceLayer(PropertyBag);
}

void FPropertyBagProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	PropertyBag.AddStructReferencedObjects(Collector);
}

void FPropertyBagProxy::AddPropertyAndValue(FName InName, const FProperty* InProperty, const void* InContainerPtr)
{
	PropertyBag.AddProperty(InName, InProperty);
	PropertyBag.SetValue(InName, InProperty, InContainerPtr);
}

}

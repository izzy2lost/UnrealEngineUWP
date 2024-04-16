// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParametersProxy.h"
#include "Graph/AnimNextGraph.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FParametersProxy::FParametersProxy(UAnimNextGraph* InGraph)
	: Graph(InGraph)
	, PropertyBag(InGraph->PropertyBag)
	, LayerHandle(FParamStack::MakeReferenceLayer(NAME_None, PropertyBag))
{
	check(Graph);
}

void FParametersProxy::Update(float DeltaTime)
{
#if WITH_EDITOR	// Layout should only be changing in editor
	const FInstancedPropertyBag* HandlePropertyBag = LayerHandle.As<FInstancedPropertyBag>();
	if(HandlePropertyBag == nullptr || HandlePropertyBag->GetPropertyBagStruct() != Graph->PropertyBag.GetPropertyBagStruct())
	{
		PropertyBag = Graph->PropertyBag;
		LayerHandle = FParamStack::MakeReferenceLayer(NAME_None, PropertyBag);
	}
#endif

	Graph->UpdateLayer(LayerHandle, DeltaTime);
}

void FParametersProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Graph);
	PropertyBag.AddStructReferencedObjects(Collector);
}

}

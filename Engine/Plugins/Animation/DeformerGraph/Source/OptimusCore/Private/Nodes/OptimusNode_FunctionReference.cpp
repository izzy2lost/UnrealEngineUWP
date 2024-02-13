// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusNode_FunctionReference.h"

#include "IOptimusCoreModule.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusFunctionNodeGraph.h"
#include "OptimusNodePin.h"
#include "OptimusNode_GraphTerminal.h"
#include "OptimusComponentSource.h"
#include "OptimusDeformer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusNode_FunctionReference)


FName UOptimusNode_FunctionReference::GetNodeCategory() const
{
	return FunctionGraph->Category;

}

FText UOptimusNode_FunctionReference::GetDisplayName() const
{
	if (FunctionGraph.IsValid())
	{
		return FText::FromString(FunctionGraph->GetNodeName());
	}

	
	return FText::FromString("<graph missing>");
}


void UOptimusNode_FunctionReference::ConstructNode()
{
	if (ensure(FunctionGraph.IsValid()))
	{
		const FOptimusDataTypeRegistry& TypeRegistry = FOptimusDataTypeRegistry::Get();
		FOptimusDataTypeRef ComponentSourceType = TypeRegistry.FindType(*UOptimusComponentSourceBinding::StaticClass());
		DefaultComponentPin = AddPinDirect(UOptimusNodeSubGraph::GraphDefaultComponentPinName, EOptimusNodePinDirection::Input, {}, ComponentSourceType);
		
		// After a duplicate, the kernel node has no pins, so we need to reconstruct them from
		// the bindings. We can assume that all naming clashes have already been dealt with.
		for (const FOptimusParameterBinding& Binding: FunctionGraph->InputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Input);
		}
		for (const FOptimusParameterBinding& Binding: FunctionGraph->OutputBindings)
		{
			AddPinDirect(Binding, EOptimusNodePinDirection::Output);
		}
	}
}


void UOptimusNode_FunctionReference::PostLoad()
{
	Super::PostLoad();

	// Load the graph into memory
	FunctionGraph.LoadSynchronous();
}


FOptimusRoutedNodePin UOptimusNode_FunctionReference::GetPinCounterpart(
	UOptimusNodePin* InNodePin,
	const FOptimusPinTraversalContext& InTraversalContext
) const
{
	if (!InNodePin || InNodePin->GetOwningNode() != this)
	{
		return {};
	}

	if (!ensure(FunctionGraph.IsValid()))
	{
		return {};
	}

	UOptimusNode_GraphTerminal* CounterpartNode = nullptr;
	if (InNodePin->GetDirection() == EOptimusNodePinDirection::Input)
	{
		CounterpartNode = FunctionGraph->EntryNode.Get();
	}
	else if (InNodePin->GetDirection() == EOptimusNodePinDirection::Output)
	{
		CounterpartNode = FunctionGraph->ReturnNode.Get();
	}

	if (!ensure(CounterpartNode))
	{
		return {};
	}

	FOptimusRoutedNodePin Result{
		CounterpartNode->FindPinFromPath(InNodePin->GetPinNamePath()),
		InTraversalContext
	};
	Result.TraversalContext.ReferenceNesting.Push(this);

	return Result;
}

UOptimusNodeGraph* UOptimusNode_FunctionReference::GetNodeGraphToShow()
{
	return FunctionGraph.Get();
}

UOptimusComponentSourceBinding* UOptimusNode_FunctionReference::GetDefaultComponentBinding(const FOptimusPinTraversalContext& InTraversalContext) const
{
	if (!ensure(DefaultComponentPin.IsValid()))
	{
		return nullptr;
	}
	
	const UOptimusNodeGraph* OwningGraph = GetOwningGraph();
	TSet<UOptimusComponentSourceBinding*> Bindings = OwningGraph->GetComponentSourceBindingsForPin(DefaultComponentPin.Get(), InTraversalContext);
	
	if (!Bindings.IsEmpty() && ensure(Bindings.Num() == 1))
	{
		return Bindings.Array()[0];
	}

	// Default to the primary binding, but only if we're at the top-most level of the graph.
	if (const UOptimusDeformer* Deformer = Cast<UOptimusDeformer>(OwningGraph->GetCollectionOwner()))
	{
		return Deformer->GetPrimaryComponentBinding();
	}

	if (const UOptimusNodeSubGraph* OwningSubGraph = Cast<UOptimusNodeSubGraph>(OwningGraph))
	{
		return OwningSubGraph->GetDefaultComponentBinding(InTraversalContext);
	}

	return nullptr;		
}

FSoftObjectPath UOptimusNode_FunctionReference::GetSerializedGraphPath() const
{
	return FunctionGraph.ToSoftObjectPath();
}

void UOptimusNode_FunctionReference::SetSerializedGraphPath(const FSoftObjectPath& InNewGraphPath)
{
	FunctionGraph = InNewGraphPath;
	// Making sure the pointer is alive such that we avoid loading on demand everywhere
	FunctionGraph.LoadSynchronous();

	SetDisplayName(GetDisplayName());
}

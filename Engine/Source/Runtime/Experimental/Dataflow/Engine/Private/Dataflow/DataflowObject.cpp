// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowObject)

#define LOCTEXT_NAMESPACE "UDataflow"


namespace Dataflow
{
	namespace CVars
	{
		/** Enable the simulation dataflow (for now WIP) */
        DATAFLOWENGINE_API bool bEnableSimulationDataflow = false;
        FAutoConsoleVariableRef CVarEnableSimulationDataflow(TEXT("p.Dataflow.EnableSimulation"), bEnableSimulationDataflow, TEXT("If true enable the use of simulation dataflow (WIP)"));
	}
}

FDataflowAssetEdit::FDataflowAssetEdit(UDataflow* InAsset, FPostEditFunctionCallback InCallback)
	: PostEditCallback(InCallback)
	, Asset(InAsset)
{
}

FDataflowAssetEdit::~FDataflowAssetEdit()
{
	PostEditCallback();
}

Dataflow::FGraph* FDataflowAssetEdit::GetGraph()
{
	if (Asset)
	{
		return Asset->Dataflow.Get();
	}
	return nullptr;
}

UDataflow::UDataflow(const FObjectInitializer& ObjectInitializer)
	: UEdGraph(ObjectInitializer)
	, Dataflow(new Dataflow::FGraph())
{}

void UDataflow::EvaluateTerminalNodeByName(FName NodeName, UObject* Asset)
{
	ensureAlwaysMsgf(false, TEXT("Deprecated use the dataflow blueprint library from now on"));
}

void UDataflow::PostEditCallback()
{
	// mark as dirty for the UObject
}

void UDataflow::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflow* const This = CastChecked<UDataflow>(InThis);

	for(TObjectPtr<const UDataflowEdNode> Target : This->GetRenderTargets())
	{
		Collector.AddReferencedObject(Target);
	}

	This->Dataflow->AddReferencedObjects(Collector);
	Super::AddReferencedObjects(InThis, Collector);
}

#if WITH_EDITOR

void UDataflow::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void UDataflow::PostLoad()
{
#if WITH_EDITOR
	const TSet<FName>& DisabledNodes = Dataflow->GetDisabledNodes();

	for (UEdGraphNode* EdNode : Nodes)
	{
		UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode);

		// Not all nodes are UDataflowEdNode (There is now UDataflowEdNodeComment)
		if (DataflowEdNode)
		{
			DataflowEdNode->SetDataflowGraph(Dataflow);
			DataflowEdNode->UpdatePinsFromDataflowNode();
		}

		if (DisabledNodes.Contains(FName(EdNode->GetName())))
		{
			EdNode->SetEnabledState(ENodeEnabledState::Disabled);
		}
	}
#endif

	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	UObject::PostLoad();
}

void UDataflow::AddRenderTarget(TObjectPtr<const UDataflowEdNode> InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	check(InNode->ShouldRenderNode());
	RenderTargets.AddUnique(InNode);
}

void UDataflow::RemoveRenderTarget(TObjectPtr<const UDataflowEdNode> InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	check(!InNode->ShouldRenderNode());
	RenderTargets.Remove(InNode);
}

void UDataflow::AddWireframeRenderTarget(TObjectPtr<const UDataflowEdNode> InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	check(InNode->ShouldWireframeRenderNode());
	WireframeRenderTargets.AddUnique(InNode);
}

void UDataflow::RemoveWireframeRenderTarget(TObjectPtr<const UDataflowEdNode> InNode)
{
	LastModifiedRenderTarget = Dataflow::FTimestamp::Current();
	check(!InNode->ShouldWireframeRenderNode());
	WireframeRenderTargets.Remove(InNode);
}


void UDataflow::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Dataflow->Serialize(Ar, this);
}

#if WITH_EDITOR
bool UDataflow::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	const FName& Name = InProperty->GetFName();

	if (Name == GET_MEMBER_NAME_CHECKED(ThisClass, Type))
	{
		return Dataflow::CVars::bEnableSimulationDataflow == true;
	}

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE


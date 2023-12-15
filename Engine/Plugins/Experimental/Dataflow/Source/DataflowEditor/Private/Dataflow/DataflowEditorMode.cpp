// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorModeToolkit.h"
#include "Dataflow/DataflowComponentToolTarget.h"
#include "Dataflow/DataflowToolTarget.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowEditor.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "EdModeInteractiveToolsContext.h"
#include "ToolTargetManager.h"
#include "ToolTargets/DynamicMeshComponentToolTarget.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "ToolTargets/StaticMeshToolTarget.h"
#include "ToolTargets/SkeletalMeshComponentToolTarget.h"
#include "ToolTargets/SkeletalMeshToolTarget.h"
#include "Tools/UEdMode.h"
#include "AttributeEditorTool.h"
#include "MeshSelectionTool.h"
#include "MeshVertexPaintTool.h"
#include "MeshAttributePaintTool.h"
#include "ModelingToolTargetUtil.h"
#include "Components/DynamicMeshComponent.h"
#include "EditorModeManager.h"
#include "Selection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditorMode)

#define LOCTEXT_NAMESPACE "UDataflowEditorMode"

const FEditorModeID UDataflowEditorMode::EM_DataflowEditorModeId = TEXT("EM_DataflowAssetEditorMode");

UDataflowEditorMode::UDataflowEditorMode()
{
	Info = FEditorModeInfo(
		EM_DataflowEditorModeId,
		LOCTEXT("DataflowEditorModeName", "Dataflow Tools"),
		FSlateIcon(),
		false);
}

void UDataflowEditorMode::Exit()
{
	UActorComponent::MarkRenderStateDirtyEvent.RemoveAll(this);
	PreviewScene->ResetDynamicMeshComponents();
	
	Super::Exit();
}

void UDataflowEditorMode::Enter()
{
	Super::Enter();
}

void UDataflowEditorMode::ModeTick(float DeltaTime)
{
	Super::ModeTick(DeltaTime);
	if (PreviewScene)
	{
		PreviewScene->GetWorld()->Tick(ELevelTick::LEVELTICK_All, DeltaTime);
	}
}

const FToolTargetTypeRequirements& UDataflowEditorMode::GetToolTargetRequirements()
{
	static const FToolTargetTypeRequirements ToolTargetRequirements =
		FToolTargetTypeRequirements({
			UMaterialProvider::StaticClass(),
			UDynamicMeshCommitter::StaticClass(),
			UDynamicMeshProvider::StaticClass()
			});

	return ToolTargetRequirements;
}

void UDataflowEditorMode::AddToolTargetFactories()
{
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDynamicMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UStaticMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UStaticMeshToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDataflowComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDataflowToolTargetFactory>(GetToolManager()));
}

void UDataflowEditorMode::RegisterTools()
{
	const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommands::Get();
	RegisterTool(CommandInfos.BeginAttributeEditorTool, FDataflowEditorCommandsImpl::BeginAttributeEditorToolIdentifier, NewObject<UAttributeEditorToolBuilder>());

	// For testing...
	//RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshSelectionToolBuilder>());
	//RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshVertexPaintToolBuilder>());
	RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshAttributePaintToolBuilder>());
}

void UDataflowEditorMode::CreateToolkit()
{
	Toolkit = MakeShared<FDataflowEditorModeToolkit>();
}

void UDataflowEditorMode::BindCommands()
{
	const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommands::Get();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	// Hook up to Enter/Esc key presses
	CommandList->MapAction(
		CommandInfos.AcceptOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this]()
		{
			AcceptActiveToolActionOrTool();
			
		}),
		FCanExecuteAction::CreateLambda([this]() {
		return GetInteractiveToolsContext()->CanAcceptActiveTool() || GetInteractiveToolsContext()->CanCompleteActiveTool();
	}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);

	CommandList->MapAction(
		CommandInfos.CancelOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this]() { CancelActiveToolActionOrTool(); }),
		FCanExecuteAction::CreateLambda([this]() {
		return GetInteractiveToolsContext()->CanCompleteActiveTool() || GetInteractiveToolsContext()->CanCancelActiveTool();
	}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);
}

void UDataflowEditorMode::CreateToolTargets(const TArray<TObjectPtr<UObject>>& AssetsIn)
{
	ToolTargets.Reset();
	for (UObject* Object : AssetsIn)
	{
		if (UToolTarget* Target = GetInteractiveToolsContext()->TargetManager->BuildTarget(Object, GetToolTargetRequirements()))
		{
			ToolTargets.Add(Target);
		}
	}
}

void UDataflowEditorMode::InitializeTargets(const TArray<TObjectPtr<UObject>>& AssetsIn)
{
	UBaseCharacterFXEditorMode::InitializeTargets(AssetsIn);
	
	USelection* SelectedComponents = GetModeManager()->GetSelectedComponents();
	SelectedComponents->Modify();
	SelectedComponents->BeginBatchSelectOperation();
	SelectedComponents->DeselectAll();

	PreviewScene->ResetDynamicMeshComponents();
	for (UToolTarget* Target : ToolTargets)
	{
		UE::Geometry::FDynamicMesh3 DynamicMesh = UE::ToolTarget::GetDynamicMeshCopy(Target);

		// @todo(DynamicMeshRendering) : Enable Dynamic Mesh Rendering for dataflow terminals. 
		//  TObjectPtr<UDynamicMeshComponent>& DynamicMeshComponent =
		//	PreviewScene->AddDynamicMeshComponent(MoveTemp(DynamicMesh), UE::ToolTarget::GetMaterialSet(Target).Materials);
		//
		//SelectedComponents->Select(DynamicMeshComponent);
	}
	SelectedComponents->EndBatchSelectOperation();
}

// UEdGraphNode* UDataflowEditorMode::CreateNewNode(const FName& NewNodeTypeName)
// {
// 	const TSharedPtr<const SDataflowGraphEditor> PinnedDataflowGraphEditor = GraphEditor.Pin();
// 	if (!PinnedDataflowGraphEditor)
// 	{
// 		return nullptr;
// 	}
//
// 	const TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> NodeAction =
// 		FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::CreateAction(PreviewScene->GetDataflowDatas().DataflowAsset, NewNodeTypeName);
// 	constexpr UEdGraphPin* FromPin = nullptr;
// 	constexpr bool bSelectNewNode = true;
// 	UEdGraphNode* const NewEdNode = NodeAction->PerformAction(PreviewScene->GetDataflowDatas().DataflowAsset, FromPin,
// 			PinnedDataflowGraphEditor->GetPasteLocation(), bSelectNewNode);
//
// 	PreviewScene->GetDataflowDatas().DataflowAsset->NotifyGraphChanged();
//
// 	return NewEdNode;
// }
//
// void UDataflowEditorMode::RegisterAddNodeCommand(TSharedPtr<FUICommandInfo> AddNodeCommand, const FName& NewNodeType, TSharedPtr<FUICommandInfo> StartToolCommand)
// {
// 	auto AddNode = [this](const FName& NewNodeType)
// 	{
// 		const FName ConnectionType = FManagedArrayCollection::StaticType();
// 		const FName ConnectionName("Collection");
// 		
// 		const UEdGraphNode* const NewNode = CreateNewNode(NewNodeType);
// 		verifyf(NewNode, TEXT("Failed to create a new node: %s"), *NewNodeType.ToString());
//
// 		StartToolForSelectedNode(NewNode);
// 	};
//
// 	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
//
// 	CommandList->MapAction(AddNodeCommand,
// 		FExecuteAction::CreateWeakLambda(this, AddNode, NewNodeType),
// 		FCanExecuteAction::CreateLambda([]() {return true;})
// 	);
//
// 	NodeToolMap.Add(NewNodeType, StartToolCommand);
// }

FBox UDataflowEditorMode::SceneBoundingBox() const
{
	return PreviewScene->GetBoundingBox();
}

#undef LOCTEXT_NAMESPACE


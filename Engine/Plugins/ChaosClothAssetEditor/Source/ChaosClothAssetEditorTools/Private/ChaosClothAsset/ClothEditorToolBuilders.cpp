// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorToolBuilders.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothPatternVertexType.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ChaosClothAsset/ClothEditorContextObject.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/WeightMapNode.h"
#include "ToolContextInterfaces.h"
#include "ToolTargetManager.h"
#include "ContextObjectStore.h"

// Tools
#include "ClothMeshSelectionTool.h"
#include "ClothTransferSkinWeightsTool.h"
#include "ClothWeightMapPaintTool.h"


// ------------------- Weight Map Paint Tool -------------------

void UClothEditorWeightMapPaintToolBuilder::GetSupportedViewModes(const UClothEditorContextObject& ContextObject, TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	using namespace UE::Chaos::ClothAsset;
	const FChaosClothAssetWeightMapNode* const WeightMapNode = ContextObject.GetSingleSelectedNodeOfType<FChaosClothAssetWeightMapNode>();
	if (WeightMapNode)
	{
		if (WeightMapNode->MeshTarget == EChaosClothAssetWeightMapMeshTarget::Simulation)
		{
			Modes.Add(EClothPatternVertexType::Sim3D);
			Modes.Add(EClothPatternVertexType::Sim2D);
		}
		else
		{
			check(WeightMapNode->MeshTarget == EChaosClothAssetWeightMapMeshTarget::Render);
			Modes.Add(EClothPatternVertexType::Render);
		}
	}
	else
	{
		// No node selected. This happens if we start the tool due to pushing the button in the toolbar -- the tool starts before the node selection can change.
		// In this case lock to either sim or render mode, whatever is current.
		// TODO: See if we can have the button action select the node before attempting to start the tool.
		const EClothPatternVertexType CurrentViewMode = ContextObject.GetConstructionViewMode();
		if (CurrentViewMode == EClothPatternVertexType::Render)
		{
			Modes.Add(EClothPatternVertexType::Render);
		}
		else
		{
			Modes.Add(EClothPatternVertexType::Sim3D);
			Modes.Add(EClothPatternVertexType::Sim2D);
		}
	}
}

UMeshSurfacePointTool* UClothEditorWeightMapPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UClothEditorWeightMapPaintTool* PaintTool = NewObject<UClothEditorWeightMapPaintTool>(SceneState.ToolManager);
	PaintTool->SetWorld(SceneState.World);

	if (UClothEditorContextObject* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		PaintTool->SetClothEditorContextObject(ContextObject);
	}

	return PaintTool;
}

// ------------------- Selection Tool -------------------

void UClothMeshSelectionToolBuilder::GetSupportedViewModes(const UClothEditorContextObject& ContextObject, TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	// TODO: When the Secondary Selection set is removed, update this function to be similar to UClothEditorWeightMapPaintToolBuilder::GetSupportedViewModes above
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Render);
}

const FToolTargetTypeRequirements& UClothMeshSelectionToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(UPrimitiveComponentBackedTarget::StaticClass());
	return TypeRequirements;
}

bool UClothMeshSelectionToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	if (UClothEditorContextObject* const ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		return ContextObject->GetSingleSelectedNodeOfType<FChaosClothAssetSelectionNode>() != nullptr && (SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1);
	}

	return false;
}

UInteractiveTool* UClothMeshSelectionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClothMeshSelectionTool* const NewTool = NewObject<UClothMeshSelectionTool>(SceneState.ToolManager);

	UToolTarget* const Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);
	NewTool->SetWorld(SceneState.World);

	if (UClothEditorContextObject* const ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		NewTool->SetClothEditorContextObject(ContextObject);
	}

	return NewTool;
}


// ------------------- Skin Weight Transfer Tool -------------------

void UClothTransferSkinWeightsToolBuilder::GetSupportedViewModes(const UClothEditorContextObject& ContextObject, TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
}

USingleSelectionMeshEditingTool* UClothTransferSkinWeightsToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UClothTransferSkinWeightsTool* NewTool = NewObject<UClothTransferSkinWeightsTool>(SceneState.ToolManager);

	if (UClothEditorContextObject* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		NewTool->SetClothEditorContextObject(ContextObject);
	}

	return NewTool;
}



namespace UE::Chaos::ClothAsset
{
	void GetClothEditorToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
	{
		ToolCDOs.Add(GetMutableDefault<UClothEditorWeightMapPaintTool>());
		ToolCDOs.Add(GetMutableDefault<UClothTransferSkinWeightsTool>());
		ToolCDOs.Add(GetMutableDefault<UClothMeshSelectionTool>());
	}
}

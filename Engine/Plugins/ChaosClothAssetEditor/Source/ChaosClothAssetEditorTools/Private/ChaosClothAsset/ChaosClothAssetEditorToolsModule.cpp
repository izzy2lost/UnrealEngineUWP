// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Dataflow/DataflowToolRegistry.h"
#include "ChaosClothAsset/ClothEditorToolBuilders.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/TransferSkinWeightsNode.h"
#include "ChaosClothAsset/WeightMapNode.h"

namespace UE::Chaos::ClothAsset
{
	class FChaosClothAssetEditorToolsModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			Dataflow::FDataflowToolRegistry& ToolRegistry = Dataflow::FDataflowToolRegistry::Get();
			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetWeightMapNode::StaticType(), NewObject<UClothEditorWeightMapPaintToolBuilder>());
			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetSelectionNode::StaticType(), NewObject<UClothMeshSelectionToolBuilder>());
			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetTransferSkinWeightsNode::StaticType(), NewObject<UClothTransferSkinWeightsToolBuilder>());
		}

		virtual void ShutdownModule() override
		{
			Dataflow::FDataflowToolRegistry& ToolRegistry = Dataflow::FDataflowToolRegistry::Get();
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetWeightMapNode::StaticType());
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetSelectionNode::StaticType());
			ToolRegistry.RemoveNodeToToolMapping(FChaosClothAssetTransferSkinWeightsNode::StaticType());
		}
	};
}

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetEditorToolsModule, ChaosClothAssetEditorTools)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "Dataflow/DataflowToolRegistry.h"
#include "ChaosClothAsset/ClothEditorToolBuilders.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "ChaosClothAsset/TransferSkinWeightsNode.h"
#include "ChaosClothAsset/WeightMapNode.h"
#include "ChaosClothAsset/ClothToolActionCommandBindings.h"

namespace UE::Chaos::ClothAsset
{
	class FChaosClothAssetEditorToolsModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			Dataflow::FDataflowToolRegistry& ToolRegistry = Dataflow::FDataflowToolRegistry::Get();
		
			const TSharedRef<const FClothToolActionCommandBindings> ClothToolActions = MakeShared<FClothToolActionCommandBindings>();

			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetWeightMapNode::StaticType(), NewObject<UClothEditorWeightMapPaintToolBuilder>(), ClothToolActions);
			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetSelectionNode::StaticType(), NewObject<UClothMeshSelectionToolBuilder>(), ClothToolActions);
			ToolRegistry.AddNodeToToolMapping(FChaosClothAssetTransferSkinWeightsNode::StaticType(), NewObject<UClothTransferSkinWeightsToolBuilder>(), ClothToolActions);
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraph_OutlinerItemDetails.h"

#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Graph/AnimNextGraph_AnimationGraph.h"
#include "Graph/AnimNextGraph_EventGraph.h"
#include "InstancedStruct.h"
#include "AnimNextRigVMWorkspaceAssetUserData.h"
#include "RigVMModel/RigVMGraph.h"
#include "WorkspaceItemMenuContext.h"
#include "IWorkspaceEditor.h"
#include "RigVMModel/RigVMClient.h"

#define LOCTEXT_NAMESPACE "FAnimNextGraphItemDetails"

namespace UE::AnimNext::Editor
{
void FAnimNextGraphItemDetails::HandleDoubleClick(const FToolMenuContext& ToolMenuContext) const
{
	const UWorkspaceItemMenuContext* WorkspaceItemContext = ToolMenuContext.FindContext<UWorkspaceItemMenuContext>();
	const UAssetEditorToolkitMenuContext* AssetEditorContext = ToolMenuContext.FindContext<UAssetEditorToolkitMenuContext>(); 
	if (WorkspaceItemContext && AssetEditorContext)
	{
		if(const TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(AssetEditorContext->Toolkit.Pin()))
		{
			const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].Data;
			if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
			{
				const FAnimNextGraphOutlinerData& GraphData = Data.Get<FAnimNextGraphOutlinerData>();							
				if (GraphData.GraphInterface)
				{
					if (URigVMGraph* RigVMGraph = GraphData.GraphInterface->GetRigVMGraph())
					{
						if(const IRigVMClientHost* RigVMClientHost = RigVMGraph->GetImplementingOuter<IRigVMClientHost>())
						{
							if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(RigVMGraph))
							{
								WorkspaceEditor->OpenObjects({EditorObject});
							}
						}
					}
				}
			}
		}
	}
}

UPackage* FAnimNextGraphItemDetails::GetPackage(const FWorkspaceOutlinerItemExport& Export) const 
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.Data;
	if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
	{
		const FAnimNextGraphOutlinerData& GraphData = Data.Get<FAnimNextGraphOutlinerData>();
		if (GraphData.GraphInterface)
		{
			return GraphData.GraphInterface.GetObject()->GetExternalPackage();
		}
	}
	return nullptr;
}

const FSlateBrush* FAnimNextGraphItemDetails::GetItemIcon() const
{
	return FAppStyle::GetBrush(TEXT("GraphEditor.EventGraph_24x"));
}

void FAnimNextGraphItemDetails::RegisterToolMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(TEXT("FAnimNextGraphItemDetails"));
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("WorkspaceOutliner.ItemContextMenu"))
	{
		Menu->AddDynamicSection(TEXT("AnimNextGraphItem"), FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			UWorkspaceItemMenuContext* WorkspaceItemContext = InMenu->FindContext<UWorkspaceItemMenuContext>();
			const UAssetEditorToolkitMenuContext* AssetEditorContext = InMenu->FindContext<UAssetEditorToolkitMenuContext>();			
			if (WorkspaceItemContext && AssetEditorContext)
			{
				FToolMenuSection& Section = InMenu->AddSection("WorkspaceOutliner.ItemContextMenu.RootAsset", FText::FromString(TEXT("Animation Next")));
				if(const TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(AssetEditorContext->Toolkit.Pin()))
				{
					if (WorkspaceItemContext->SelectedExports.Num() == 1)
					{
						TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].Data;
						if (Data.IsValid() && Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
						{							
							Section.AddMenuEntry(
							TEXT("Open"),
							LOCTEXT("GoHereActionLabel", "Open Graph"),
							LOCTEXT("GoHereActionTooltip", "Open selected AnimNext Graph"),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"),
							FUIAction(
								FExecuteAction::CreateWeakLambda(WorkspaceItemContext, [&Data, WorkspaceEditor]()
								{										
									const FAnimNextGraphOutlinerData& GraphData = Data.Get<FAnimNextGraphOutlinerData>();
									if (GraphData.GraphInterface)
									{
										if (URigVMGraph* RigVMGraph = GraphData.GraphInterface->GetRigVMGraph())
										{
											if(const IRigVMClientHost* RigVMClientHost = RigVMGraph->GetImplementingOuter<IRigVMClientHost>())
											{
												if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(RigVMGraph))
												{
													WorkspaceEditor->OpenObjects({EditorObject});
												}
											}
										}
									}
								})
							));
						}
					}
				}
			}
		}));
	}
}

void FAnimNextGraphItemDetails::UnregisterToolMenuExtensions()
{
	UToolMenus::Get()->UnregisterOwnerByName("FAnimNextGraphItemDetails");
}
}

#undef LOCTEXT_NAMESPACE // "FAnimNextGraphItemDetails"
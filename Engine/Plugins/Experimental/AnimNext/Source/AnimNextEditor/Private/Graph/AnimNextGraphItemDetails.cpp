// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphItemDetails.h"

#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Graph/AnimNextModule_AnimationGraph.h"
#include "Module/AnimNextModule_EventGraph.h"
#include "StructUtils/InstancedStruct.h"
#include "Module/AnimNextModuleWorkspaceAssetUserData.h"
#include "RigVMModel/RigVMGraph.h"
#include "WorkspaceItemMenuContext.h"
#include "IWorkspaceEditor.h"
#include "RigVMModel/RigVMClient.h"
#include "ToolMenus.h"

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
			const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].GetData();
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
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
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

					TArray<FWorkspaceOutlinerItemExport> GraphExports;
					Algo::TransformIf(WorkspaceItemContext->SelectedExports, GraphExports, [](const FWorkspaceOutlinerItemExport& Export)
					{
						return Export.GetData().IsValid() && Export.GetData().GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct();
					},
					[](const FWorkspaceOutlinerItemExport& Export)
					{
						return Export;
					});

					if (GraphExports.Num() == WorkspaceItemContext->SelectedExports.Num() && GraphExports.Num() > 0)
					{
						TWeakPtr<UE::Workspace::IWorkspaceEditor> WeakWorkspaceEditor = WorkspaceEditor;
						Section.AddMenuEntry(
						TEXT("OpenGraphMenuEntry"),
						FText::FormatOrdered(LOCTEXT("OpenGraphMenuEntryLabel", "Open Animation {0}|plural(one=Graph,other=Graphs)"), GraphExports.Num()),
						FText::FormatOrdered(LOCTEXT("OpenGraphMenuEntryTooltip", "Open the selected Animation {0}|plural(one=Graph,other=Graphs)"), GraphExports.Num()),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"),
						FUIAction(
							FExecuteAction::CreateWeakLambda(WorkspaceItemContext, [GraphExports, WeakWorkspaceEditor]()
							{
								if (TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
								{
									TArray<UObject*> ObjectsToOpen;
									for (const FWorkspaceOutlinerItemExport& Export : GraphExports)
									{
										const FAnimNextGraphOutlinerData& GraphData = Export.GetData().Get<FAnimNextGraphOutlinerData>();
										if (GraphData.GraphInterface)
										{
											if (URigVMGraph* RigVMGraph = GraphData.GraphInterface->GetRigVMGraph())
											{
												if(const IRigVMClientHost* RigVMClientHost = RigVMGraph->GetImplementingOuter<IRigVMClientHost>())
												{
													if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(RigVMGraph))
													{
														ObjectsToOpen.Add(EditorObject);
													}
												}
											}
										}
									}

									SharedWorkspaceEditor->OpenObjects({ObjectsToOpen});
								}
							})
						));
					}
				}
			}
		}), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
	}
}

void FAnimNextGraphItemDetails::UnregisterToolMenuExtensions()
{
	if(UToolMenus* ToolMenus = UToolMenus::Get())
	{
		ToolMenus->UnregisterOwnerByName("FAnimNextGraphItemDetails");
	}
}
}

#undef LOCTEXT_NAMESPACE // "FAnimNextGraphItemDetails"
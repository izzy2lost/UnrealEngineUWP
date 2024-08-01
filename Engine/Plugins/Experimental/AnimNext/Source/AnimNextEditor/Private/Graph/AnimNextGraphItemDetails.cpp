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
#include "EdGraph/RigVMEdGraph.h"
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
			if (Data.IsValid())
			{
				if (Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
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
				else if (Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
				{
					const FAnimNextGraphFunctionOutlinerData& GraphFunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
					if (GraphFunctionData.EditorObject.IsValid())
					{
						WorkspaceEditor->OpenObjects({ GraphFunctionData.EditorObject.Get()});
					}
				}
				else if (Data.GetScriptStruct() == FAnimNextCollapseGraphOutlinerData::StaticStruct())
				{
					const FAnimNextCollapseGraphOutlinerData& CollapseGraphData = Data.Get<FAnimNextCollapseGraphOutlinerData>();
					if (CollapseGraphData.EditorObject.IsValid())
					{
						UObject* EditorObject = CollapseGraphData.EditorObject.Get();
						WorkspaceEditor->OpenObjects({ EditorObject });
					}
				}
			}
		}
	}
}

UPackage* FAnimNextGraphItemDetails::GetPackage(const FWorkspaceOutlinerItemExport& Export) const 
{
	const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
	if (Data.IsValid())
	{
		if (Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
		{
			const FAnimNextGraphOutlinerData& GraphData = Data.Get<FAnimNextGraphOutlinerData>();
			if (GraphData.GraphInterface)
			{
				return GraphData.GraphInterface.GetObject()->GetExternalPackage();
			}
		}
		else if(Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
		{
			const FAnimNextGraphFunctionOutlinerData& GraphFunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
			if (GraphFunctionData.EditorObject.IsValid())
			{
				return GraphFunctionData.EditorObject->GetPackage();
			}
		}
		else if (Data.GetScriptStruct() == FAnimNextCollapseGraphOutlinerData::StaticStruct())
		{
			const FAnimNextCollapseGraphOutlinerData& CollapseGraphData = Data.Get<FAnimNextCollapseGraphOutlinerData>();
			if (CollapseGraphData.EditorObject.IsValid())
			{
				return CollapseGraphData.EditorObject->GetPackage();
			}
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
							return Export.GetData().IsValid() 
								&& (Export.GetData().GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct()
									|| Export.GetData().GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct()
									|| Export.GetData().GetScriptStruct() == FAnimNextCollapseGraphOutlinerData::StaticStruct());
						},
						[](const FWorkspaceOutlinerItemExport& Export)
						{
							return Export;
						});

					if (GraphExports.Num() == WorkspaceItemContext->SelectedExports.Num() && GraphExports.Num() > 0)
					{
						TWeakPtr<UE::Workspace::IWorkspaceEditor> WeakWorkspaceEditor = WorkspaceEditor;

						TInstancedStruct<FWorkspaceOutlinerItemData>& Data = WorkspaceItemContext->SelectedExports[0].GetData();
						if (Data.IsValid())
						{
							FName EntryText;
							FText EntryLabelText;
							FText EntryTooltipText;
							FSlateIcon EntryIcon;

							if (Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
							{
								EntryText = TEXT("OpenGraphMenuEntry");
								EntryLabelText = FText::FormatOrdered(LOCTEXT("OpenGraphMenuEntryLabel", "Open {0}|plural(one=Animation Graph,other=Graphs)"), GraphExports.Num());
								EntryTooltipText = FText::FormatOrdered(LOCTEXT("OpenGraphMenuEntryTooltip", "Open the selected {0}|plural(one=Animation Graph,other=Graphs)"), GraphExports.Num());
								EntryIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x");
							}
							else if (Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
							{
								EntryText = TEXT("OpenFunctionActionEntry");
								EntryLabelText = FText::FormatOrdered(LOCTEXT("OpenFunctionActionLabel", "Open {0}|plural(one=Function Graph,other=Grapsh)"), GraphExports.Num());
								EntryTooltipText = FText::FormatOrdered(LOCTEXT("OpenFunctionActionTooltip", "Open the selected {0}|plural(one=Function Graph,other=Graphs)"), GraphExports.Num());
								EntryIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Function_16x");
							}
							else if (Data.GetScriptStruct() == FAnimNextCollapseGraphOutlinerData::StaticStruct())
							{
								EntryText = TEXT("OpenCollapseNodeActionEntry");
								EntryLabelText = FText::FormatOrdered(LOCTEXT("OpenCollapseGraphActionLabel", "Open {0}|plural(one=Collapse Graph,other=Graphs)"), GraphExports.Num());
								EntryTooltipText = FText::FormatOrdered(LOCTEXT("OpenCollapseGraphActionTooltip", "Open the selected {0}|plural(one=Collapse Graph,other=Graphs)"), GraphExports.Num());
								EntryIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.SubGraph_16x");
							}

							Section.AddMenuEntry(
								EntryText,
								EntryLabelText,
								EntryTooltipText,
								EntryIcon,
								FUIAction(
									FExecuteAction::CreateWeakLambda(WorkspaceItemContext, [GraphExports, WeakWorkspaceEditor]()
									{
										if (TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
										{
											TArray<UObject*> ObjectsToOpen;
											for (const FWorkspaceOutlinerItemExport& Export : GraphExports)
											{
												const TInstancedStruct<FWorkspaceOutlinerItemData>& Data = Export.GetData();
												if (Data.GetScriptStruct() == FAnimNextGraphOutlinerData::StaticStruct())
												{
													const FAnimNextGraphOutlinerData& GraphData = Export.GetData().Get<FAnimNextGraphOutlinerData>();
													if (GraphData.GraphInterface)
													{
														if (URigVMGraph* RigVMGraph = GraphData.GraphInterface->GetRigVMGraph())
														{
															if (const IRigVMClientHost* RigVMClientHost = RigVMGraph->GetImplementingOuter<IRigVMClientHost>())
															{
																if (UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(RigVMGraph))
																{
																	ObjectsToOpen.Add(EditorObject);
																}
															}
														}
													}
												}
												else if (Data.GetScriptStruct() == FAnimNextGraphFunctionOutlinerData::StaticStruct())
												{
													const FAnimNextGraphFunctionOutlinerData& GraphFunctionData = Data.Get<FAnimNextGraphFunctionOutlinerData>();
													if (GraphFunctionData.EditorObject.IsValid())
													{
														ObjectsToOpen.Add(GraphFunctionData.EditorObject.Get());
													}
												}
												else if (Data.GetScriptStruct() == FAnimNextCollapseGraphOutlinerData::StaticStruct())
												{
													const FAnimNextCollapseGraphOutlinerData& GraphFunctionData = Data.Get<FAnimNextCollapseGraphOutlinerData>();
													if (GraphFunctionData.EditorObject.IsValid())
													{
														ObjectsToOpen.Add(GraphFunctionData.EditorObject.Get());
													}
												}
											}

											SharedWorkspaceEditor->OpenObjects({ ObjectsToOpen });
										}
									})
								));
						}
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

} // UE::AnimNext::Editor

#undef LOCTEXT_NAMESPACE // "FAnimNextGraphItemDetails"
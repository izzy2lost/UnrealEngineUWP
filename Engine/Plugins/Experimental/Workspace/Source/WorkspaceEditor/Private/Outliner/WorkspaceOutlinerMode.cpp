// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceOutlinerMode.h"

#include "WorkspaceItemMenuContext.h"
#include "WorkspaceOutlinerHierarchy.h"
#include "WorkspaceOutlinerTreeItem.h"
#include "Toolkits/AssetEditorToolkitMenuContext.h"
#include "Workspace.h"
#include "IWorkspaceEditor.h"
#include "WorkspaceEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FWorkspaceOutlinerMode"

namespace UE::Workspace
{
FWorkspaceOutlinerMode::FWorkspaceOutlinerMode(SSceneOutliner* InSceneOutliner, const TWeakObjectPtr<UWorkspace>& InWeakWorkspace, const TWeakPtr<IWorkspaceEditor>& InWeakWorkspaceEditor) : ISceneOutlinerMode(InSceneOutliner), WeakWorkspace(InWeakWorkspace), WeakWorkspaceEditor(InWeakWorkspaceEditor)
{
	if (UWorkspace* Workspace = WeakWorkspace.Get())
	{
		Workspace->ModifiedDelegate.AddRaw(this, &FWorkspaceOutlinerMode::OnWorkspaceModified);
	}
}

FWorkspaceOutlinerMode::~FWorkspaceOutlinerMode()
{
	if (UWorkspace* Workspace = WeakWorkspace.Get())
	{
		Workspace->ModifiedDelegate.RemoveAll(this);
	}
}

void FWorkspaceOutlinerMode::Rebuild()
{
	Hierarchy = CreateHierarchy();
}

TSharedPtr<SWidget> FWorkspaceOutlinerMode::CreateContextMenu()
{
	static const FName MenuName("WorkspaceOutliner.ItemContextMenu");
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		FToolMenuOwnerScoped ToolMenuOwnerScope(this);
		if (UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName))
		{
			Menu->AddDynamicSection(TEXT("Assets"), FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				const UAssetEditorToolkitMenuContext* EditorContext = InMenu->FindContext<UAssetEditorToolkitMenuContext>();
				const UWorkspaceItemMenuContext* MenuContext = InMenu->FindContext<UWorkspaceItemMenuContext>();
				if(EditorContext && MenuContext)
				{
					const bool bSelectionContainsTopLevelAsset = MenuContext->SelectedExports.Num() && MenuContext->SelectedExports.ContainsByPredicate([](const FWorkspaceOutlinerItemExport& Export)
					{
						return Export.ParentIdentifier == NAME_None;
					});

					if (bSelectionContainsTopLevelAsset)
					{
						FToolMenuSection& AssetsSection = InMenu->AddSection("Assets", LOCTEXT("AssetSectionLabel", "Assets"));
						AssetsSection.AddMenuEntry(TEXT("OpenAsset"),
							LOCTEXT("OpenAssetLabel", "Open Asset(s)"),
							LOCTEXT("OpenAssetTooltip", "Opens the selected Asset(s)"),
							FSlateIcon(FAppStyle::Get().GetStyleSetName(), "SystemWideCommands.SummonOpenAssetDialog"),
							FUIAction(FExecuteAction::CreateLambda([WeakEditor=EditorContext->Toolkit, SelectedExports=MenuContext->SelectedExports]()
							{
								if (const TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(WeakEditor.Pin()))
								{
									TSet<FSoftObjectPath> AssetPaths;
									for (const FWorkspaceOutlinerItemExport& ItemExport : SelectedExports)
									{
										if (ItemExport.ParentIdentifier == NAME_None)
										{
											AssetPaths.Add(ItemExport.AssetPath);	
										}
									}

									for (const FSoftObjectPath& AssetPath : AssetPaths)
									{
										SharedWorkspaceEditor->OpenAssets({AssetPath.TryLoad()});	
									}
								}
							}))
						);

						AssetsSection.AddMenuEntry(TEXT("RemoveAsset"),
							LOCTEXT("RemoveAssetLabel", "Remove Asset(s)"),
							LOCTEXT("RemoveAssetTooltip", "Removes the selected Asset(s) from the Workspace"),
							FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Delete"),
							FUIAction(FExecuteAction::CreateLambda([WeakEditor=EditorContext->Toolkit, SelectedExports=MenuContext->SelectedExports, EditingObjects=EditorContext->GetEditingObjects()]()
							{
								if (const TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(WeakEditor.Pin()))
								{
									TSet<FSoftObjectPath> AssetPaths;
									for (const FWorkspaceOutlinerItemExport& ItemExport : SelectedExports)
									{
										if (ItemExport.ParentIdentifier == NAME_None)
										{
											AssetPaths.Add(ItemExport.AssetPath);	
										}
									}

									if (EditingObjects.Num() == 1)
									{
										if (UWorkspace* Workspace = Cast<UWorkspace>(EditingObjects[0]))
										{
											for (const FSoftObjectPath& AssetPath : AssetPaths)
											{
												Workspace->RemoveAsset(AssetPath.TryLoad());
											}
										}
									}
								}
							}))
						);
					}
				}
			}));
		}
	}

	{
		UWorkspaceItemMenuContext* MenuContext = NewObject<UWorkspaceItemMenuContext>();

		Algo::TransformIf(SceneOutliner->GetSelectedItems(), MenuContext->SelectedExports,
			[](const FSceneOutlinerTreeItemPtr& InItem)
			{
				return InItem->IsA<FWorkspaceOutlinerTreeItem>();
			},
			[](const FSceneOutlinerTreeItemPtr& InItem)
			{
				return InItem->CastTo<FWorkspaceOutlinerTreeItem>()->Export;
			});	
		
		FToolMenuContext Context;
		Context.AddObject(MenuContext);

		WeakWorkspaceEditor.Pin()->InitToolMenuContext(Context);
		
		return UToolMenus::Get()->GenerateWidget(MenuName, Context);
	}
}

void FWorkspaceOutlinerMode::OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item)
{
	OpenItems({Item} );
}

void FWorkspaceOutlinerMode::OnItemClicked(FSceneOutlinerTreeItemPtr Item)
{
	const FSceneOutlinerItemSelection& Selection = SceneOutliner->GetSelection();
	HandleItemSelection(Selection);
}

FReply FWorkspaceOutlinerMode::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	// TODO JDB these could be in a FUICommandList
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		OpenItems(SceneOutliner->GetSelectedItems());
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Platform_Delete)
	{
		DeleteItems(SceneOutliner->GetSelectedItems());
		return FReply::Handled();
	}

	// TODO JDB more possible actions? (find in content browser?)
	
	return ISceneOutlinerMode::OnKeyDown(InKeyEvent);
}

void FWorkspaceOutlinerMode::HandleItemSelection(const FSceneOutlinerItemSelection& Selection)
{
	if (Selection.Num() == 1)
	{
		TArray<FSceneOutlinerTreeItemPtr> SelectedItems;
		Selection.Get(SelectedItems);			
		
		if (TSharedPtr<IWorkspaceEditor> SharedWorkspaceEditor = WeakWorkspaceEditor.Pin())
		{
			if(FWorkspaceOutlinerTreeItem* TreeItem = SelectedItems[0]->CastTo<FWorkspaceOutlinerTreeItem>())
			{
				if(TreeItem->Export.Data.IsValid())
				{
					TSharedPtr<FStructOnScope> ExportDataView = MakeShared<FStructOnScope>(TreeItem->Export.Data.GetScriptStruct(), TreeItem->Export.Data.GetMutableMemory());
					// TODO JDB handle struct selections
					//SharedWorkspaceEditor->SetDetailsStruct(ExportDataView);
				}
			}
		}
	}
}

void FWorkspaceOutlinerMode::OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection)
{
	HandleItemSelection(Selection);
}

TUniquePtr<ISceneOutlinerHierarchy> FWorkspaceOutlinerMode::CreateHierarchy()
{
	return MakeUnique<FWorkspaceOutlinerHierarchy>(this, WeakWorkspace);
}

void FWorkspaceOutlinerMode::OnWorkspaceModified(UWorkspace* InWorkspace)
{
	ensure(InWorkspace == WeakWorkspace.Get());
	SceneOutliner->FullRefresh();
}

void FWorkspaceOutlinerMode::OpenItems(TArrayView<const FSceneOutlinerTreeItemPtr> Items) const
{
	for (const FSceneOutlinerTreeItemPtr& Item : Items)
	{
		if (const FWorkspaceOutlinerTreeItem* TreeItem = Item->CastTo<FWorkspaceOutlinerTreeItem>())
		{
			if(TreeItem->Export.ParentIdentifier == NAME_None)
			{
				if (const TSharedPtr<UE::Workspace::IWorkspaceEditor> SharedWorkspaceEditor = StaticCastSharedPtr<UE::Workspace::IWorkspaceEditor>(WeakWorkspaceEditor.Pin()))
				{			
					SharedWorkspaceEditor->OpenAssets({TreeItem->Export.AssetPath.TryLoad()});
				}
			}
			else
			{
				if (const TSharedPtr<IWorkspaceOutlinerItemDetails> SharedDetails = FWorkspaceEditorModule::GetOutlinerItemDetails(MakeOutlinerDetailsId(TreeItem->Export)))
				{
					UWorkspaceItemMenuContext* MenuContext = NewObject<UWorkspaceItemMenuContext>();
					MenuContext->SelectedExports.Add(TreeItem->Export);
				
					FToolMenuContext Context(MenuContext);
					WeakWorkspaceEditor.Pin()->InitToolMenuContext(Context);
				
					SharedDetails->HandleDoubleClick(Context);
				}
			}
		}
	}
}

void FWorkspaceOutlinerMode::DeleteItems(TArrayView<const FSceneOutlinerTreeItemPtr> Items) const
{
	FARFilter Filter;	
	for (const FSceneOutlinerTreeItemPtr& Item : Items)
	{
		if (const FWorkspaceOutlinerTreeItem* TreeItem = Item->CastTo<FWorkspaceOutlinerTreeItem>())
		{
			if(TreeItem->Export.ParentIdentifier == NAME_None)
			{
				Filter.SoftObjectPaths.AddUnique(TreeItem->Export.AssetPath);
			}
		}		
	}

	if (UWorkspace* Workspace = WeakWorkspace.Get())
	{
		TArray<FAssetData> AssetDataEntriesToRemove;
		const IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
		AssetRegistry.GetAssets(Filter, AssetDataEntriesToRemove);
		if (AssetDataEntriesToRemove.Num())
		{
			FScopedTransaction Transaction(LOCTEXT("RemoveAssets", "Remove assets from workspace"));
			Workspace->RemoveAssets(AssetDataEntriesToRemove);
		}		
	}
}

}	

#undef LOCTEXT_NAMESPACE // "FWorkspaceOutlinerMode"
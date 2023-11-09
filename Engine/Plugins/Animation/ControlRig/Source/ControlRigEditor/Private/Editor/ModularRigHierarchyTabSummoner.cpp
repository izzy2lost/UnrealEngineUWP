// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ModularRigHierarchyTabSummoner.h"
#include "Editor/SModularRigHierarchy.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ModularRigHierarchyTabSummoner"

const FName FModularRigHierarchyTabSummoner::TabID(TEXT("ModularRigHierarchy"));

FModularRigHierarchyTabSummoner::FModularRigHierarchyTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("ModularRigHierarchyTabLabel", "Modular Rig Hierarchy");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ModularRigHierarchy.TabIcon");

	ViewMenuDescription = LOCTEXT("ModularRigHierarchy_ViewMenu_Desc", "Modular Rig Hierarchy");
	ViewMenuTooltip = LOCTEXT("ModularRigHierarchy_ViewMenu_ToolTip", "Show the Modular Rig Hierarchy tab");
}

FTabSpawnerEntry& FModularRigHierarchyTabSummoner::RegisterTabSpawner(TSharedRef<FTabManager> InTabManager, const FApplicationMode* CurrentApplicationMode) const
{
	FTabSpawnerEntry& SpawnerEntry = FWorkflowTabFactory::RegisterTabSpawner(InTabManager, CurrentApplicationMode);

	SpawnerEntry.SetReuseTabMethod(FOnFindTabToReuse::CreateLambda([](const FTabId& InTabId) ->TSharedPtr<SDockTab> {
	
		return TSharedPtr<SDockTab>();

	}));

	return SpawnerEntry;
}

TSharedRef<SWidget> FModularRigHierarchyTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	ControlRigEditor.Pin()->ModularRigHierarchyTabCount++;
	return SNew(SModularRigHierarchy, ControlRigEditor.Pin().ToSharedRef());
}

TSharedRef<SDockTab> FModularRigHierarchyTabSummoner::SpawnTab(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SDockTab>  DockTab = FWorkflowTabFactory::SpawnTab(Info);
	TWeakPtr<SDockTab> WeakDockTab = DockTab;
	DockTab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([WeakDockTab]()
    {
		int32 HierarchyTabCount = 0;
		if (TSharedPtr<SDockTab> SharedDocTab = WeakDockTab.Pin())
		{
			if(SWidget* Content = &SharedDocTab->GetContent().Get())
			{
				SModularRigHierarchy* RigHierarchy = (SModularRigHierarchy*)Content;
				if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
				{
					HierarchyTabCount = ControlRigEditorForTab->GetModularRigHierarchyTabCount();
				}
			}
		}
		return HierarchyTabCount > 0;
    }));
	DockTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab> DockTab)
	{
		if(SWidget* Content = &DockTab->GetContent().Get())
		{
			SModularRigHierarchy* RigHierarchy = (SModularRigHierarchy*)Content;
			if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
			{
				ControlRigEditorForTab->ModularRigHierarchyTabCount--;
			}
		}
	}));
	return DockTab;
}

#undef LOCTEXT_NAMESPACE 

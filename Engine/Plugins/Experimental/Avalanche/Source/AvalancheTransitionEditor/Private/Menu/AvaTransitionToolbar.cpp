// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionToolbar.h"
#include "AvaTransitionCommands.h"
#include "AvaTransitionEditorUtils.h"
#include "AvaTransitionMenuContext.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ViewModels/AvaTransitionEditorViewModel.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"

#define LOCTEXT_NAMESPACE "AvaTransitionToolbar"

namespace UE::AvaTransitionEditor::Private
{
	static constexpr const TCHAR* TransitionLayerPickerName = TEXT("TransitionLogicLayerPicker");
}

void FAvaTransitionToolbar::ExtendEditorToolbar(UToolMenu* InToolbarMenu)
{
	if (!InToolbarMenu)
	{
		return;
	}

	const bool bReadOnly = Owner.GetSharedData()->IsReadOnly();

	FToolMenuSection& Section = InToolbarMenu->FindOrAddSection(TEXT("TransitionLogic"));

	const FAvaTransitionEditorCommands& Commands = FAvaTransitionEditorCommands::Get();

	FToolMenuEntry& CompileButton = Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.Compile
		, TAttribute<FText>()
		, TAttribute<FText>()
		, TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateSPLambda(this,
			[this]
			{
				return Owner.GetCompiler().GetCompileStatusIcon();
			}))));

	CompileButton.StyleNameOverride = "CalloutToolbar";

	FToolMenuEntry& CompileOptions = Section.AddEntry(FToolMenuEntry::InitComboButton(TEXT("CompileComboButton")
		, FUIAction()
		, FNewToolMenuDelegate::CreateStatic(&FAvaTransitionCompiler::GenerateCompileOptionsMenu)
		, LOCTEXT("CompileOptions_ToolbarTooltip", "Options to customize how State Trees compile")));

	CompileOptions.StyleNameOverride = "CalloutToolbar";
	CompileOptions.ToolBarData.bSimpleComboBox = true;

	if (TSharedPtr<SWidget> LayerPicker = UE::AvaTransitionEditor::CreateTransitionLayerPicker(Owner.GetEditorData()))
	{
		LayerPicker->SetEnabled(!bReadOnly);

		Section.AddSeparator(NAME_None);
		Section.AddEntry(FToolMenuEntry::InitWidget(UE::AvaTransitionEditor::Private::TransitionLayerPickerName
			, LayerPicker.ToSharedRef()
			, LOCTEXT("TransitionLogicLabel", "Transition Logic Layer")));
	}

#if WITH_STATETREE_DEBUGGER
	Section.AddSeparator(NAME_None);
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.ToggleDebug));
#endif
}

void FAvaTransitionToolbar::ExtendTreeToolbar(UToolMenu* InToolbarMenu)
{
	if (!InToolbarMenu)
	{
		return;
	}

	const bool bReadOnly = Owner.GetSharedData()->IsReadOnly();

	const FAvaTransitionEditorCommands& Commands = FAvaTransitionEditorCommands::Get();

	if (!bReadOnly)
	{
		FToolMenuSection& StateSection = InToolbarMenu->FindOrAddSection(TEXT("State"), LOCTEXT("StateActions", "State Actions"));
		StateSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.AddSiblingState, LOCTEXT("AddStateLabel", "Add State")));
	}

	FToolMenuSection& TreeSection = InToolbarMenu->FindOrAddSection(TEXT("Tree"), LOCTEXT("TreeActions", "Tree Actions"));
	if (!bReadOnly)
	{
		TreeSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.ImportTransitionTree));	
	}
	TreeSection.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.ExportTransitionTree));
}

void FAvaTransitionToolbar::SetupReadOnlyCustomization(FReadOnlyAssetEditorCustomization& InReadOnlyCustomization)
{
	const FName PermissionOwner = TEXT("FAvaTransitionToolbar");

	const FAvaTransitionEditorCommands& Commands = FAvaTransitionEditorCommands::Get();

	InReadOnlyCustomization.ToolbarPermissionList.AddAllowListItem(PermissionOwner, UE::AvaTransitionEditor::Private::TransitionLayerPickerName);

#if WITH_STATETREE_DEBUGGER
	InReadOnlyCustomization.ToolbarPermissionList.AddAllowListItem(PermissionOwner, Commands.ToggleDebug->GetCommandName());
#endif
}

TSharedRef<SWidget> FAvaTransitionToolbar::GenerateTreeToolbarWidget()
{
	UToolMenus* const ToolMenus = UToolMenus::Get();
	check(ToolMenus);

	const FName ToolbarName = GetTreeToolbarName();

	if (!ToolMenus->IsMenuRegistered(ToolbarName))
	{
		UToolMenu* const ToolBar = ToolMenus->RegisterMenu(ToolbarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		ToolBar->StyleName = "CalloutToolbar";
		ToolBar->AddDynamicSection("PopulateToolbar", FNewToolMenuDelegate::CreateStatic([](UToolMenu* InToolMenu)
		{
			if (InToolMenu)
			{
				if (UAvaTransitionMenuContext* MenuContext = InToolMenu->FindContext<UAvaTransitionMenuContext>())
				{
					if (TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = MenuContext->GetEditorViewModel())
					{
						EditorViewModel->GetToolbar()->ExtendTreeToolbar(InToolMenu);
					}
				}
			}
		}));
	}

	TSharedPtr<FExtender> Extender;

	UAvaTransitionMenuContext* const ContextObject = NewObject<UAvaTransitionMenuContext>();
	ContextObject->SetEditorViewModel(StaticCastSharedRef<FAvaTransitionEditorViewModel>(Owner.AsShared()));

	FToolMenuContext Context(Owner.GetCommandList(), Extender, ContextObject);
	return ToolMenus->GenerateWidget(ToolbarName, Context);
}

#undef LOCTEXT_NAMESPACE

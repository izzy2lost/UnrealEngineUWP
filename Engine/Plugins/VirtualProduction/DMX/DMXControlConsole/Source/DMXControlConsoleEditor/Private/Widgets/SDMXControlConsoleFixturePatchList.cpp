// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleFixturePatchList.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/Find.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFixturePatchListRowModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "ToolMenus.h"
#include "Widgets/DMXReadOnlyFixturePatchListItem.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchListRow.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleFixturePatchList"

namespace UE::DMXControlConsole::Private
{
	namespace Internal
	{
		/** Internal helper to find patches excluded from the list when in default layout mode */
		bool IsFixturePatchExcludedInDefaultLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode)
		{
			const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
			const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
			if (!EditorConsoleData)
			{
				return true;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				return true;
			}

			const bool bIsMuted = FaderGroup->IsMuted();
			switch (ShowMode)
			{
			case EDMXReadOnlyFixturePatchListShowMode::Active:
				return bIsMuted;
				break;
			case EDMXReadOnlyFixturePatchListShowMode::Inactive:
				return !bIsMuted;
				break;
			default:
				return false;
			}
		}

		/** Internal helper to find patches excluded from the list when in user layout mode */
		bool IsFixturePatchExludedInUserLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode)
		{
			const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
			const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
			if (!EditorConsoleLayouts)
			{
				return false;
			}

			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
			if (!ActiveLayout)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = ActiveLayout->FindFaderGroupByFixturePatch(FixturePatch);
			const bool bIsAddedToUserLayout = IsValid(FaderGroup);

			switch (ShowMode)
			{
			case EDMXReadOnlyFixturePatchListShowMode::Active:
				return !bIsAddedToUserLayout;
				break;
			case EDMXReadOnlyFixturePatchListShowMode::Inactive:
				return bIsAddedToUserLayout;
				break;
			default:
				return false;
			}
		}
	}

	/** Helper that returns true if the default layout is the active layout */
	bool IsDefaultLayoutActive()
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (EditorConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
			return ActiveLayout && ActiveLayout == &EditorConsoleLayouts->GetDefaultLayoutChecked();
		}

		return false;
	}

	/** Helper that returns fixture patches that should be excluded from the list, given the active layout class and the current show mode. */
	TArray<UDMXEntityFixturePatch*> FindFixturePatchesToExclude(const TArray<UDMXEntityFixturePatch*> AllFixturePatches, EDMXReadOnlyFixturePatchListShowMode ShowMode)
	{
		TArray<UDMXEntityFixturePatch*> Result;

		Algo::CopyIf(AllFixturePatches, Result, [ShowMode](const UDMXEntityFixturePatch* FixturePatch)
			{
				if (!FixturePatch)
				{
					return false;
				}

				if (IsDefaultLayoutActive())
				{
					return Internal::IsFixturePatchExcludedInDefaultLayout(FixturePatch, ShowMode);
				}
				else
				{
					return Internal::IsFixturePatchExludedInUserLayout(FixturePatch, ShowMode);
				}
			});

		return Result;
	}	
}

const FName FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled = "FaderGroupEnabled";

SDMXControlConsoleFixturePatchList::~SDMXControlConsoleFixturePatchList()
{
	const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = MakeListDescriptor();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->SaveFixturePatchListDescriptorToConfig(ListDescriptor);
}

void SDMXControlConsoleFixturePatchList::Construct(const FArguments& InArgs)
{	
	// Register the header row filter menu extender
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("DMXEditor.ControlConsoleFixturePatchList.HeaderRowFilterMenu"))
	{
		const FToolMenuInsert SectionInsertLocation("ShowColumnSection", EToolMenuInsertType::Before);

		Menu->AddDynamicSection(
			"FilterActiveAndInactivePatches",
			FNewToolMenuDelegate::CreateSP(this, &SDMXControlConsoleFixturePatchList::ExtendHeaderRowFilterMenu),
			SectionInsertLocation
		);
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = EditorConsoleModel->GetFixturePatchListDescriptor();

	SDMXReadOnlyFixturePatchList::Construct(SDMXReadOnlyFixturePatchList::FArguments()
		.ListDescriptor(ListDescriptor)
		.DMXLibrary(InArgs._DMXLibrary)
		.OnContextMenuOpening(this, &SDMXControlConsoleFixturePatchList::OnContextMenuOpening)
		.OnRowClicked(this, &SDMXControlConsoleFixturePatchList::OnRowClicked)
		.OnRowDoubleClicked(this, &SDMXControlConsoleFixturePatchList::OnRowDoubleClicked)
		.OnRowSelectionChanged(this, &SDMXControlConsoleFixturePatchList::OnSelectionChanged));

	EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
	EditorConsoleModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
	UDMXControlConsoleEditorGlobalLayoutRow::GetOnGlobalLayoutRowChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::OnGlobalLayoutRowChanged);

	RegisterCommands();
	ForceRefresh();
}

FName SDMXControlConsoleFixturePatchList::GetHeaderRowFilterMenuName() const 
{
	// Override the default menu, so it can be customized only for this list class here
	return "DMXEditor.ControlConsoleFixturePatchList.HeaderRowFilterMenu";
}

void SDMXControlConsoleFixturePatchList::ForceRefresh()
{
	using namespace UE::DMXControlConsole::Private;
	const TArray<UDMXEntityFixturePatch*> FixturePatchesToExclude = FindFixturePatchesToExclude(GetFixturePatchesInDMXLibrary(), ShowMode);

	SetExcludedFixturePatches(FixturePatchesToExclude);
	SDMXReadOnlyFixturePatchList::ForceRefresh();

	AdoptSelectionFromData();

	// Listen to data changes
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	if (!EditorConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnFaderGroupAdded().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
	}

	if (!EditorConsoleData->GetOnFaderGroupRemoved().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnFaderGroupRemoved().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
	}

	if (!EditorConsoleLayouts->GetOnActiveLayoutChanged().IsBoundToObject(this))
	{
		EditorConsoleLayouts->GetOnActiveLayoutChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
	}
}

TSharedRef<SHeaderRow> SDMXControlConsoleFixturePatchList::GenerateHeaderRow()
{
	const TSharedRef<SHeaderRow> HeaderRow = SDMXReadOnlyFixturePatchList::GenerateHeaderRow();

	// Insert the fixture group enabled checkbox at column index 1
	constexpr int32 FixtureGroupEnabledColumnIndex = 1;
	HeaderRow->InsertColumn(SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled)
		.DefaultLabel(LOCTEXT("CheckBoxColumnLabel", ""))
		.FixedWidth(32.f)
		.HeaderContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(SBox)
				.WidthOverride(20.f)
				.HeightOverride(20.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SDMXControlConsoleFixturePatchList::GetGlobalFixtureGroupsMutedCheckBoxState)
					.OnCheckStateChanged(this, &SDMXControlConsoleFixturePatchList::OnGlobalFixtureGroupsMutedCheckBoxStateChanged)
				]
			]
		], 
		FixtureGroupEnabledColumnIndex);

	return HeaderRow;
}

TSharedRef<ITableRow> SDMXControlConsoleFixturePatchList::OnGenerateRow(TSharedPtr<FDMXReadOnlyFixturePatchListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return 
		SNew(SDMXControlConsoleFixturePatchListRow, OwnerTable, InItem.ToSharedRef())
		.OnFaderGroupMutedChanged(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
}

void SDMXControlConsoleFixturePatchList::ExtendHeaderRowFilterMenu(UToolMenu* InMenu)
{
	FToolMenuSection& Section = InMenu->AddSection(NAME_None, LOCTEXT("FixturePatchListShowPatchFilterSection", "Filter"));

	auto AddMenuEntryLambda = [this, &Section](const FName& Name, const FText& Label, const FText& ToolTip, const EDMXReadOnlyFixturePatchListShowMode InShowMode)
	{
		Section.AddMenuEntry(
			Name,
			Label,
			ToolTip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::SetShowMode, InShowMode),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsUsingShowMode, InShowMode)
			),
			EUserInterfaceActionType::RadioButton
		);
	};

	AddMenuEntryLambda(
		"ShowAllPatches",
		LOCTEXT("FixturePatchAllPatchesFilter_Label", "All Patches"),
		LOCTEXT("FixturePatchAllPatchesFilter", "Show all the Fixture Patches in the list"),
		EDMXReadOnlyFixturePatchListShowMode::All
	);

	AddMenuEntryLambda(
		"ShowActivePatches",
		LOCTEXT("FixturePatchActivePatchesFilter_Label", "Only Active"),
		LOCTEXT("FixturePatchActivePatchesFilter", "Show only active Fixture Patches in the list"),
		EDMXReadOnlyFixturePatchListShowMode::Active
	);

	AddMenuEntryLambda(
		"ShowInactivePatches",
		LOCTEXT("FixturePatchInactivePatchesFilter_Label", "Only Inactive"),
		LOCTEXT("FixturePatchInactivePatchesFilter", "Show only inactive Fixture Patches in the list"),
		EDMXReadOnlyFixturePatchListShowMode::Inactive
	);
}

void SDMXControlConsoleFixturePatchList::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	const auto MapMuteActionLambda = [this](TSharedPtr<FUICommandInfo> CommandInfo, bool bMute, bool bOnlyActive)
	{
		CommandList->MapAction
		(
			CommandInfo,
			FExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::OnMuteAllFaderGroups, bMute, bOnlyActive),
			FCanExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsAnyFaderGroupMuted, !bMute, bOnlyActive),
			FGetActionCheckState(),
			FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsAnyFaderGroupMuted, !bMute, bOnlyActive)
		);
	};

	MapMuteActionLambda(FDMXControlConsoleEditorCommands::Get().Mute, true, true);
	MapMuteActionLambda(FDMXControlConsoleEditorCommands::Get().MuteAll, true, false);
	MapMuteActionLambda(FDMXControlConsoleEditorCommands::Get().Unmute, false, true);
	MapMuteActionLambda(FDMXControlConsoleEditorCommands::Get().UnmuteAll, false, false);
}

void SDMXControlConsoleFixturePatchList::AdoptSelectionFromData()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	// Do only if the active layout is the default layout
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout || ActiveLayout != &EditorConsoleLayouts->GetDefaultLayoutChecked())
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup || !FaderGroup->HasFixturePatch())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		const TSharedPtr<FDMXReadOnlyFixturePatchListItem>* ItemPtr = Algo::FindByPredicate(GetListItems(), [FixturePatch](const TSharedPtr<FDMXReadOnlyFixturePatchListItem>& Item)
			{
				return FixturePatch == Item->GetFixturePatch();
			});

		if (ItemPtr)
		{
			const bool bIsSelected = FaderGroup->IsActive();
			SetItemSelection(*ItemPtr, bIsSelected, ESelectInfo::Direct);
		}
	}
}

void SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved(const UDMXControlConsoleFaderGroup* FaderGroup)
{
	RequestRefresh();
}

void SDMXControlConsoleFixturePatchList::OnGlobalLayoutRowChanged(UDMXControlConsoleEditorGlobalLayoutRow* ChangedRow)
{
	RequestRefresh();
}

TSharedPtr<SWidget> SDMXControlConsoleFixturePatchList::OnContextMenuOpening()
{
	ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list in SDMXControlConsoleFixturePatchList. Commands should have been registered on Construction."));
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MuteFaderGroupContextMenu", "Mute"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().Mute,
			NAME_None,
			LOCTEXT("MuteContextMenu_Label", "Only Active"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().MuteAll,
			NAME_None,
			LOCTEXT("MuteAllContextMenu_Label", "All"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("UnmuteFaderGroupContextMenu", "Unmute"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().Unmute,
			NAME_None,
			LOCTEXT("UnmuteContextMenu_Label", "Only Active"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().UnmuteAll,
			NAME_None,
			LOCTEXT("UnmuteAllContextMenu_Label", "All"),
			FText::GetEmpty(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
		);
	}
	MenuBuilder.EndSection();

	// Show Add Patch buttons only if the current layout is the user layout
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (ActiveLayout && ActiveLayout != &EditorConsoleLayouts->GetDefaultLayoutChecked())
		{
			TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
			const TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = GetSelectedFixturePatches();
			Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch;
				});

			MenuBuilder.AddWidget(SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches), FText::GetEmpty());
		}
	}

	return MenuBuilder.MakeWidget();
}

void SDMXControlConsoleFixturePatchList::OnSelectionChanged(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	// Continue only if the current layout is the default layout
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout || ActiveLayout != &EditorConsoleLayouts->GetDefaultLayoutChecked())
	{
		return;
	}

	const TArray<TSharedPtr<FDMXReadOnlyFixturePatchListItem>> SelectedFixturePatches = GetSelectedItems();
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
	TArray<UObject*> FaderGroupsToAddToSelection;
	TArray<UObject*> FaderGroupsToRemoveFromSelection;
	for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup || !FaderGroup->HasFixturePatch())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		const TSharedPtr<FDMXReadOnlyFixturePatchListItem>* SelectedItemPtr = Algo::FindByPredicate(SelectedFixturePatches, [FixturePatch](const TSharedPtr<FDMXReadOnlyFixturePatchListItem>& Item)
			{
				const UDMXEntityFixturePatch* OtherFixturePatch = Item.IsValid() ? Item->GetFixturePatch() : nullptr;
				return OtherFixturePatch && OtherFixturePatch == FixturePatch;
			});

		// Set fader group active if it is selected
		const bool bIsSelected = SelectedItemPtr != nullptr;
		FaderGroup->SetIsActive(bIsSelected);
		if (bIsSelected)
		{
			ActiveLayout->AddToActiveFaderGroups(FaderGroup);
			const bool bAutoSelect = EditorConsoleModel->GetAutoSelectActivePatches();
			if (bAutoSelect)
			{
				const TArray<UDMXControlConsoleFaderBase*> AllFaders = FaderGroup->GetAllFaders();
				FaderGroupsToAddToSelection.Append(AllFaders);
			}
		}
		else
		{
			ActiveLayout->RemoveFromActiveFaderGroups(FaderGroup);
			FaderGroupsToRemoveFromSelection.Add(FaderGroup);
			if (SelectedItemPtr)
			{
				// Unselect as if the item was unselected by mouse, raising related events
				SetItemSelection(*SelectedItemPtr, false, ESelectInfo::OnMouseClick);
			}
		}
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->AddToSelection(FaderGroupsToAddToSelection);
	SelectionHandler->RemoveFromSelection(FaderGroupsToRemoveFromSelection);
}

void SDMXControlConsoleFixturePatchList::OnRowClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
{
	if (!ClickedItem.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		EditorConsoleModel->ScrollIntoView(FaderGroup);
	}
}

void SDMXControlConsoleFixturePatchList::OnRowDoubleClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
{
	if (!ClickedItem.IsValid())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (!EditorConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		FaderGroup->SetIsExpanded(true);
	}
}

void SDMXControlConsoleFixturePatchList::OnMuteAllFaderGroups(bool bMute, bool bOnlyActive) const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? EditorConsoleData->GetAllActiveFaderGroups() : EditorConsoleData->GetAllFaderGroups();
		for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FaderGroup->SetMute(bMute);
			}
		}
	}
}

bool SDMXControlConsoleFixturePatchList::IsAnyFaderGroupMuted(bool bMute, bool bOnlyActive) const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? EditorConsoleData->GetAllActiveFaderGroups() : EditorConsoleData->GetAllFaderGroups();
		return Algo::AnyOf(AllFaderGroups, [bMute](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && FaderGroup->IsMuted() == bMute;
			});
	}

	return false;
}

ECheckBoxState SDMXControlConsoleFixturePatchList::GetGlobalFixtureGroupsMutedCheckBoxState() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		// Get all patched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> AllPatchedFaderGroups = EditorConsoleData->GetAllFaderGroups();
		AllPatchedFaderGroups.RemoveAll([](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->HasFixturePatch();
			});

		const bool bAreAllFaderGroupsUnmuted = Algo::AllOf(AllPatchedFaderGroups, [](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->IsMuted();
			});

		if (bAreAllFaderGroupsUnmuted)
		{
			return ECheckBoxState::Checked;
		}

		const bool bIsAnyFaderGroupUnmuted = Algo::AnyOf(AllPatchedFaderGroups, [](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && !FaderGroup->IsMuted();
			});

		return bIsAnyFaderGroupUnmuted ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void SDMXControlConsoleFixturePatchList::OnGlobalFixtureGroupsMutedCheckBoxStateChanged(ECheckBoxState CheckBoxState)
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = EditorConsoleData->GetAllFaderGroups();
		for (UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup && FaderGroup->HasFixturePatch())
			{
				const bool bIsMuted = CheckBoxState == ECheckBoxState::Unchecked;
				FaderGroup->SetMute(bIsMuted);
			}
		}
	}
}

void SDMXControlConsoleFixturePatchList::SetShowMode(EDMXReadOnlyFixturePatchListShowMode NewShowMode)
{
	ShowMode = NewShowMode;
	RequestRefresh();
}

bool SDMXControlConsoleFixturePatchList::IsUsingShowMode(EDMXReadOnlyFixturePatchListShowMode InShowMode) const
{
	return ShowMode == InShowMode;
}

#undef LOCTEXT_NAMESPACE

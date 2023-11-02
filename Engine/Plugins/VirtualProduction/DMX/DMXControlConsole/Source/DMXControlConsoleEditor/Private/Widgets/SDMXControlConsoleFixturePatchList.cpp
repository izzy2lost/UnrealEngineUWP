// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleFixturePatchList.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/Find.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
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
#include "Style/DMXControlConsoleEditorStyle.h"
#include "ToolMenus.h"
#include "Widgets/DMXReadOnlyFixturePatchListItem.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchListRow.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleFixturePatchList"

namespace UE::DMXControlConsoleEditor::Private
{
	namespace Internal
	{
		/** Internal helper to find patches excluded from the list when in default layout mode */
		bool IsFixturePatchExcludedInDefaultLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode, const UDMXControlConsoleEditorModel* InEditorModel)
		{
			const UDMXControlConsoleData* ControlConsoleData = InEditorModel ? InEditorModel->GetControlConsoleData() : nullptr;
			if (!ControlConsoleData)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				return false;
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
		bool IsFixturePatchExcludedInUserLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode, const UDMXControlConsoleEditorModel* InEditorModel)
		{
			const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = InEditorModel ? InEditorModel->GetControlConsoleLayouts() : nullptr;
			if (!ControlConsoleLayouts)
			{
				return false;
			}

			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
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
	bool IsDefaultLayoutActive(const UDMXControlConsoleEditorModel* InEditorModel)
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = InEditorModel ? InEditorModel->GetControlConsoleLayouts() : nullptr;
		if (ControlConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			return ActiveLayout && ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked();
		}

		return false;
	}

	/** Helper that returns fixture patches that should be excluded from the list, given the active layout class and the current show mode. */
	TArray<UDMXEntityFixturePatch*> FindFixturePatchesToExclude(const TArray<UDMXEntityFixturePatch*> AllFixturePatches, EDMXReadOnlyFixturePatchListShowMode ShowMode, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		TArray<UDMXEntityFixturePatch*> Result;

		const UDMXControlConsoleEditorModel* EditorModel = InWeakEditorModel.Get();
		if (EditorModel)
		{
			Algo::CopyIf(AllFixturePatches, Result, [ShowMode, EditorModel](const UDMXEntityFixturePatch* FixturePatch)
				{
					if (!FixturePatch)
					{
						return false;
					}

					if (IsDefaultLayoutActive(EditorModel))
					{
						return Internal::IsFixturePatchExcludedInDefaultLayout(FixturePatch, ShowMode, EditorModel);
					}
					else
					{
						return Internal::IsFixturePatchExcludedInUserLayout(FixturePatch, ShowMode, EditorModel);
					}
				});
		}

		return Result;
	}	
}

const FName FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled = "FaderGroupEnabled";

SDMXControlConsoleFixturePatchList::~SDMXControlConsoleFixturePatchList()
{
	const FName MenuName = GetHeaderRowFilterMenuName();

	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolMenus->RemoveMenu(MenuName);
	}
}

void SDMXControlConsoleFixturePatchList::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
{	
	if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct fixture patch list correctly.")))
	{
		return;
	}

	EditorModel = InEditorModel;
	EditorModelUniqueID = EditorModel->GetUniqueID();

	// Register the header row filter menu extender
	const FName MenuName = GetHeaderRowFilterMenuName();
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName))
	{
		const FToolMenuInsert SectionInsertLocation("ShowColumnSection", EToolMenuInsertType::Before);

		Menu->AddDynamicSection
		(
			"FilterActiveAndInactivePatches",
			FNewToolMenuDelegate::CreateSP(this, &SDMXControlConsoleFixturePatchList::ExtendHeaderRowFilterMenu),
			SectionInsertLocation
		);
	}

	FDMXReadOnlyFixturePatchListDescriptor ListDescriptor;
	UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
	if (EditorData)
	{
		ListDescriptor = EditorData->FixturePatchListDescriptor;
	}

	SDMXReadOnlyFixturePatchList::Construct(SDMXReadOnlyFixturePatchList::FArguments()
		.ListDescriptor(ListDescriptor)
		.DMXLibrary(InArgs._DMXLibrary)
		.OnContextMenuOpening(this, &SDMXControlConsoleFixturePatchList::OnContextMenuOpening)
		.OnRowClicked(this, &SDMXControlConsoleFixturePatchList::OnRowClicked)
		.OnRowDoubleClicked(this, &SDMXControlConsoleFixturePatchList::OnRowDoubleClicked)
		.OnRowSelectionChanged(this, &SDMXControlConsoleFixturePatchList::OnSelectionChanged));

	EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
	UDMXControlConsoleEditorGlobalLayoutRow::GetOnGlobalLayoutRowChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::OnGlobalLayoutRowChanged);

	RegisterCommands();
	ForceRefresh();
}

FName SDMXControlConsoleFixturePatchList::GetHeaderRowFilterMenuName() const
{
	// Override the default menu, so it can be customized only for this list class here
	const FString DefaultFilterMenuNameAsString = TEXT("DMXEditor.ControlConsoleFixturePatchList.HeaderRowFilterMenu");
	const FString EditorModelUniqueIDAsString = FString::FromInt(EditorModelUniqueID);

	const FName FilterMenuName = *(DefaultFilterMenuNameAsString + EditorModelUniqueIDAsString);
	return FilterMenuName;
}

void SDMXControlConsoleFixturePatchList::ForceRefresh()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	using namespace UE::DMXControlConsoleEditor::Private;
	const TArray<UDMXEntityFixturePatch*> FixturePatchesToExclude = FindFixturePatchesToExclude(GetFixturePatchesInDMXLibrary(), ShowMode, EditorModel);

	SetExcludedFixturePatches(FixturePatchesToExclude);
	SDMXReadOnlyFixturePatchList::ForceRefresh();

	AdoptSelectionFromData();

	// Listen to data changes
	UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	if (!ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnFaderGroupAdded().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
	}

	if (!ControlConsoleData->GetOnFaderGroupRemoved().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnFaderGroupRemoved().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
	}

	if (!ControlConsoleLayouts->GetOnActiveLayoutChanged().IsBoundToObject(this))
	{
		ControlConsoleLayouts->GetOnActiveLayoutChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
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
		SNew(SDMXControlConsoleFixturePatchListRow, OwnerTable, InItem.ToSharedRef(), EditorModel)
		.OnFaderGroupMutedChanged(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
}

void SDMXControlConsoleFixturePatchList::ToggleColumnShowState(const FName ColumnID)
{
	SDMXReadOnlyFixturePatchList::ToggleColumnShowState(ColumnID);

	UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
	if (EditorData)
	{
		const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = MakeListDescriptor();
		EditorData->Modify();
		EditorData->FixturePatchListDescriptor = ListDescriptor;
	}
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
			FUIAction
			(
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
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	// Do only if the active layout is the default layout
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout || ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
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
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (EditorModel.IsValid() && ControlConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
			const TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = GetSelectedFixturePatches();
			Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch;
				});

			MenuBuilder.AddWidget(SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches, EditorModel.Get()), FText::GetEmpty());
		}
	}

	return MenuBuilder.MakeWidget();
}

void SDMXControlConsoleFixturePatchList::OnSelectionChanged(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!EditorModel.IsValid() || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	// Continue only if the current layout is the default layout
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout || ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
	{
		return;
	}

	const TArray<TSharedPtr<FDMXReadOnlyFixturePatchListItem>> SelectedFixturePatches = GetSelectedItems();
	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
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

			const UDMXControlConsoleEditorData* EditorData = EditorModel->GetControlConsoleEditorData();
			const bool bAutoSelect = EditorData && EditorData->GetAutoSelectActivePatches();
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

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	SelectionHandler->AddToSelection(FaderGroupsToAddToSelection);
	SelectionHandler->RemoveFromSelection(FaderGroupsToRemoveFromSelection);
}

void SDMXControlConsoleFixturePatchList::OnRowClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
{
	if (!EditorModel.IsValid() || !ClickedItem.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		EditorModel->ScrollIntoView(FaderGroup);
	}
}

void SDMXControlConsoleFixturePatchList::OnRowDoubleClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
{
	if (!EditorModel.IsValid() || !ClickedItem.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return;
	}

	const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
	if (!FixturePatch)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
	if (FaderGroup && FaderGroup->IsActive())
	{
		FaderGroup->SetIsExpanded(true);
	}
}

void SDMXControlConsoleFixturePatchList::OnMuteAllFaderGroups(bool bMute, bool bOnlyActive) const
{
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (ControlConsoleData)
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? ControlConsoleData->GetAllActiveFaderGroups() : ControlConsoleData->GetAllFaderGroups();
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
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (ControlConsoleData)
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = bOnlyActive ? ControlConsoleData->GetAllActiveFaderGroups() : ControlConsoleData->GetAllFaderGroups();
		return Algo::AnyOf(AllFaderGroups, [bMute](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && FaderGroup->IsMuted() == bMute;
			});
	}

	return false;
}

ECheckBoxState SDMXControlConsoleFixturePatchList::GetGlobalFixtureGroupsMutedCheckBoxState() const
{
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (ControlConsoleData)
	{
		// Get all patched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> AllPatchedFaderGroups = ControlConsoleData->GetAllFaderGroups();
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
	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	if (ControlConsoleData)
	{
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
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

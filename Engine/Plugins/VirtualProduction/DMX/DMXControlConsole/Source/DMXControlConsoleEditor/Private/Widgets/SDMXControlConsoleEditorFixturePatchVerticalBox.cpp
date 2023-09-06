// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/DMXReadOnlyFixturePatchListItem.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFixturePatchVerticalBox"

void SDMXControlConsoleEditorFixturePatchVerticalBox::Construct(const FArguments& InArgs)
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	UDMXLibrary* DMXLibrary = EditorConsoleData ? EditorConsoleData->GetDMXLibrary() : nullptr;

	ChildSlot
		.Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateFixturePatchListToolbar()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(FixturePatchList, SDMXControlConsoleFixturePatchList)
				.DMXLibrary(DMXLibrary)
			]
		];
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::ForceRefresh()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	if (EditorConsoleData && FixturePatchList.IsValid())
	{
		UDMXLibrary* NewLibrary = EditorConsoleData->GetDMXLibrary();
		FixturePatchList->SetDMXLibrary(NewLibrary);
	}
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFixturePatchListToolbar()
{
	const auto GenerateAddButtonContentLambda = [](const FText& AddButtonText, const FText& AddButtonToolTip)
		{
			return
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.ColorAndOpacity(FStyleColors::AccentGreen)
					.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(AddButtonText)
					.ToolTipText(AddButtonToolTip)
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
				];
		};

	const TSharedRef<SWidget> FixturePatchListToolbar =
		SNew(SHorizontalBox)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility))

		// Add All Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(8.f, 0.f, 4.f, 8.f)
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.IsEnabled(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled)
			.OnClicked(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked)
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddAllFixturePatchFromList", "Add All Patches"),
					LOCTEXT("AddAllFixturePatchFromList_ToolTip", "Add all Fixture Patches from the list.")
				)
			]
		]

		// Add Combo Button
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.MaxWidth(160.f)
		.HAlign(HAlign_Left)
		.Padding(4.f, 0.f, 8.f, 8.f)
		[
			SNew(SComboButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			.ForegroundColor(FSlateColor::UseStyle())
			.HasDownArrow(true)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnGetMenuContent(this, &SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu)
			.ButtonContent()
			[
				GenerateAddButtonContentLambda
				(
					LOCTEXT("AddFixturePatchFromList", "Add Patch"),
					LOCTEXT("AddFixturePatchFromList_ToolTip", "Add a Fixture Patch from the list.")
				)
			]
		];

	return FixturePatchListToolbar;
}

TSharedRef<SWidget> SDMXControlConsoleEditorFixturePatchVerticalBox::CreateAddPatchMenu()
{
	// Show Add Patch buttons only if the current layout is the user layout
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleLayouts)
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		if (CurrentLayout && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass())
		{
			TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
			TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = FixturePatchList->GetSelectedFixturePatches();
			Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
				{
					return FixturePatch;
				});

			return SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches);
		}
	}

	return SNullWidget::NullWidget;
}

void SDMXControlConsoleEditorFixturePatchVerticalBox::GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (!FaderGroup || !FixturePatch)
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	SelectionHandler->ClearFadersSelection(FaderGroup);

	const FScopedTransaction GenerateFaderGroupFromFixturePatchTransaction(LOCTEXT("GenerateFaderGroupFromFixturePatchTransaction", "Generate Fader Group from Fixture Patch"));
	FaderGroup->PreEditChange(UDMXControlConsoleFaderGroup::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroup::GetSoftFixturePatchPtrPropertyName()));
	FaderGroup->GenerateFromFixturePatch(FixturePatch);
	FaderGroup->PostEditChange();
}

FReply SDMXControlConsoleEditorFixturePatchVerticalBox::OnAddAllPatchesClicked()
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return FReply::Handled();
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return FReply::Handled();
	}

	const FScopedTransaction AddAllPatchesTransaction(LOCTEXT("AddAllPatchesTransaction", "Add All Patches"));
	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = EditorConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		// Remove Fader Groups already in the layout and all unpatched Fader Groups
		TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
		FaderGroups.RemoveAll([&CurrentLayout](const UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && 
					(!FaderGroup->HasFixturePatch() ||
					CurrentLayout->ContainsFaderGroup(FaderGroup));
			});

		CurrentLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = CurrentLayout->AddNewRowToLayout();
		CurrentLayout->PostEditChange();

		if (LayoutRow)
		{
			LayoutRow->PreEditChange(nullptr);
			LayoutRow->AddToLayoutRow(FaderGroups);
			LayoutRow->PostEditChange();

			Algo::ForEach(FaderGroups,[](UDMXControlConsoleFaderGroup* FaderGroup)
				{
					FaderGroup->Modify();
					FaderGroup->SetIsActive(true);
				});
		}
	}

	return FReply::Handled();
}

bool SDMXControlConsoleEditorFixturePatchVerticalBox::IsAddAllPatchesButtonEnabled() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	return IsValid(EditorConsoleData) && IsValid(EditorConsoleData->GetDMXLibrary());
}

EVisibility SDMXControlConsoleEditorFixturePatchVerticalBox::GetFixturePatchListToolbarVisibility() const
{
	bool bIsVisible = false;
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts())
	{
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bIsVisible = IsValid(CurrentLayout) && CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass();
	}

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE

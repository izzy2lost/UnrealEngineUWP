// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupComboBox.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXEditorStyle.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupComboBox"

namespace UE::DMX::Private
{ 
	void SDMXControlConsoleEditorFaderGroupComboBox::Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create fader group toolbar widget correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroupView.IsValid(), TEXT("Invalid fader group view, cannot create fader group toolbar widget correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		FaderGroupView = InFaderGroupView;

		UpdateComboBoxSource();

		ChildSlot
			[
				SAssignNew(FixturePatchesComboBox, SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>)
				.OptionsSource(&ComboBoxSource)
				.OnGenerateWidget(this, &SDMXControlConsoleEditorFaderGroupComboBox::GenerateFixturePatchesComboBoxWidget)
				.OnComboBoxOpening(this, &SDMXControlConsoleEditorFaderGroupComboBox::UpdateComboBoxSource)
				.OnSelectionChanged(this, &SDMXControlConsoleEditorFaderGroupComboBox::OnComboBoxSelectionChanged)
				.ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox")))
				.ItemStyle(&FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("DMXControlConsole.FaderGroupToolbar")))
				.ToolTipText(this, &SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroupFixturePatchNameText)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(4.f)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(FDMXEditorStyle::Get().GetBrush("Icons.FixturePatch"))
						.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroupEditorColor)
					]

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(56.f)
						[
							SNew(STextBlock)
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text(this, &SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroupFixturePatchNameText)
							.OverflowPolicy(ETextOverflowPolicy::Clip)
						]
					]
				]
			];
	}

	UDMXControlConsoleFaderGroup* SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroup() const
	{
		return FaderGroupView.IsValid() ? FaderGroupView.Pin()->GetFaderGroup() : nullptr;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupComboBox::GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef)
	{
		if (FixturePatchRef.IsValid())
		{
			const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef->GetFixturePatch();
			const FLinearColor EditorTagColor = IsValid(FixturePatch) ? FixturePatch->EditorColor : FLinearColor::White;
			const FString FixturePatchName = IsValid(FixturePatch) ? FixturePatch->Name : TEXT("Undefined");

			const TSharedRef<SWidget> ComboBoxWidget =
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.MinDesiredWidth(6.f)
					.MinDesiredHeight(14.f)
					[
						SNew(SImage)
						.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
						.ColorAndOpacity(EditorTagColor)
					]
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.MaxWidth(140.f)
				.Padding(6.f, 0.f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(FText::FromString(FixturePatchName))
					.ToolTipText(FText::FromString(FixturePatchName))
				];

			if (FixturePatch)
			{
				ComboBoxWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupComboBox::IsFixturePatchStillAvailable, FixturePatch));
			}

			return ComboBoxWidget;
		}

		return SNullWidget::NullWidget;
	}

	bool SDMXControlConsoleEditorFaderGroupComboBox::IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const
	{
		if (!InFixturePatch)
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (!ControlConsoleLayouts)
		{
			return false;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = CurrentLayout->GetAllFaderGroups();

		const auto IsFixturePatchInUseLambda = [InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
			{
				if (!FaderGroup.IsValid() || !FaderGroup->IsActive() || !FaderGroup->HasFixturePatch())
				{
					return false;
				}

				const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
				const bool bIsFixturePatchInUse = FixturePatch == InFixturePatch;
				return bIsFixturePatchInUse;
			};

		return !Algo::AnyOf(AllFaderGroups, IsFixturePatchInUseLambda);
	}

	void SDMXControlConsoleEditorFaderGroupComboBox::UpdateComboBoxSource()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		DMXLibrary = ControlConsoleData ? ControlConsoleData->GetDMXLibrary() : nullptr;

		ComboBoxSource.Reset(ComboBoxSource.Num());
		ComboBoxSource.Add(MakeShared<FDMXEntityFixturePatchRef>());

		if (DMXLibrary.IsValid())
		{
			const TArray<UDMXEntityFixturePatch*> FixturePatchesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
			for (UDMXEntityFixturePatch* FixturePatch : FixturePatchesInLibrary)
			{
				if (FixturePatch && IsFixturePatchStillAvailable(FixturePatch))
				{
					const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef = MakeShared<FDMXEntityFixturePatchRef>();
					FixturePatchRef->SetEntity(FixturePatch);
					ComboBoxSource.Add(FixturePatchRef);
				}
			}
		}

		if (FixturePatchesComboBox.IsValid())
		{
			FixturePatchesComboBox->RefreshOptions();
		}
	}

	void SDMXControlConsoleEditorFaderGroupComboBox::OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo)
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (!FaderGroup)
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearElementControllersSelection(FaderGroup);
		
		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const UDMXEntityFixturePatch* FixturePatch = FixturePatchRef.IsValid() ? FixturePatchRef->GetFixturePatch() : nullptr;
		UDMXControlConsoleFaderGroup* FaderGroupToAdd = nullptr;

		const FScopedTransaction GenerateFaderGroupFromComboBoxSelectionTransaction(LOCTEXT("GenerateFaderGroupFromComboBoxSelectionTransaction", "Replace Fader Group"));
		if (FixturePatch)
		{
			// Find Fader Group to Add in Control Console Data
			FaderGroupToAdd = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		}
		else if (ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			// Fader Group to Add is a new Fader Group
			UDMXControlConsoleFaderGroupRow& OwnerRow = FaderGroup->GetOwnerFaderGroupRowChecked();
			OwnerRow.PreEditChange(nullptr);
			FaderGroupToAdd = OwnerRow.AddFaderGroup(FaderGroup->GetIndex());
			OwnerRow.PostEditChange();
		}

		if (FaderGroupToAdd)
		{
			int32 RowIndex = ActiveLayout->GetFaderGroupRowIndex(FaderGroup);
			int32 ColumnIndex = ActiveLayout->GetFaderGroupColumnIndex(FaderGroup);

			// Emplace Fader Group with FaderGroupToAdd
			ActiveLayout->PreEditChange(nullptr);
			if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
			{
				ActiveLayout->RemoveFromLayout(FaderGroupToAdd);
				ActiveLayout->RemoveFromActiveFaderGroups(FaderGroupToAdd);
			}
			else
			{
				ActiveLayout->RemoveFromLayout(FaderGroup);
				ActiveLayout->RemoveFromActiveFaderGroups(FaderGroup);
			}

			ActiveLayout->AddToLayout(FaderGroupToAdd, RowIndex, ColumnIndex);
			ActiveLayout->AddToActiveFaderGroups(FaderGroupToAdd);

			FaderGroupToAdd->Modify();
			FaderGroupToAdd->SetIsActive(true);
			FaderGroupToAdd->SetIsExpanded(FaderGroup->IsExpanded());

			FaderGroup->Modify();
			FaderGroup->SetIsActive(false);

			if (SelectionHandler->IsSelected(FaderGroup))
			{
				constexpr bool bNotifySelectionChange = false;
				SelectionHandler->AddToSelection(FaderGroupToAdd, bNotifySelectionChange);
				SelectionHandler->RemoveFromSelection(FaderGroup);
			}

			if (!FaderGroup->HasFixturePatch())
			{
				FaderGroup->Destroy();
			}
		}

		EditorModel->RequestUpdateEditorModel();
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroupEditorColor() const
	{
		if (const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup())
		{
			return FaderGroup->GetEditorColor();	
		}

		return FLinearColor::White;
	}

	FText SDMXControlConsoleEditorFaderGroupComboBox::GetFaderGroupFixturePatchNameText() const
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = GetFaderGroup();
		if (FaderGroup && FaderGroup->HasFixturePatch())
		{
			return FText::FromString(FaderGroup->GetFixturePatch()->Name);
		}

		return LOCTEXT("UndefinedFixturePatchNameText","Undefined");
	}
}

#undef LOCTEXT_NAMESPACE

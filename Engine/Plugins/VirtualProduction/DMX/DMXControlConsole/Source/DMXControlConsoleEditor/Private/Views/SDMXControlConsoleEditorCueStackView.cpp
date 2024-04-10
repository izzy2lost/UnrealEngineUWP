// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorCueStackView.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleCueStack.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Misc/TransactionObjectEvent.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorCueList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorCueStackView"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorCueStackView::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct cue stack view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;

		UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		if (ControlConsoleData)
		{
			ControlConsoleData->GetOnDMXLibraryChanged().AddSP(this, &SDMXControlConsoleEditorCueStackView::OnDMXLibraryChanged);
		}

		ChildSlot
			[
				SNew(SVerticalBox)

				// Cue Stack toolbar section
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(8.f)
				[
					GenerateCueListToolbar()
				]

				// Cue List View section
				+ SVerticalBox::Slot()
				[
					SAssignNew(CueList, SDMXControlConsoleEditorCueList, EditorModel.Get())
				]
			];
	}

	bool SDMXControlConsoleEditorCueStackView::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		const TArray<UClass*> MatchingContextClasses =
		{
			UDMXControlConsoleData::StaticClass(),
			UDMXControlConsoleCueStack::StaticClass()
		};

		const bool bMatchesContext = Algo::AnyOf(TransactionObjectContexts,
			[this, MatchingContextClasses](const TPair<UObject*, FTransactionObjectEvent>& Pair)
			{
				bool bMatchesClasses = false;
				const UObject* Object = Pair.Key;
				if (IsValid(Object))
				{
					const UClass* ObjectClass = Object->GetClass();
					bMatchesClasses = Algo::AnyOf(MatchingContextClasses, [ObjectClass](UClass* InClass)
						{
							return IsValid(ObjectClass) && ObjectClass->IsChildOf(InClass);
						});
				}

				return bMatchesClasses;
			});

		return bMatchesContext;
	}

	void SDMXControlConsoleEditorCueStackView::PostUndo(bool bSuccess)
	{
		if (CueList.IsValid())
		{
			CueList->RequestRefresh();
		}
	}

	void SDMXControlConsoleEditorCueStackView::PostRedo(bool bSuccess)
	{
		if (CueList.IsValid())
		{
			CueList->RequestRefresh();
		}
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueStackView::GenerateCueListToolbar()
	{
		const TSharedRef<SWidget> CueListToolbar =
			SNew(SHorizontalBox)

			// Add New Cue button section
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SDMXControlConsoleEditorCueStackView::OnAddNewCueClicked)
				.IsEnabled(this, &SDMXControlConsoleEditorCueStackView::IsAddNewCueButtonEnabled)
				.ContentPadding(FMargin(0.f, 4.f))
				[
					GenerateCueListToolbarButtonContent
					(
						LOCTEXT("AddNewCueButton_Label", "Add New Cue"),
						LOCTEXT("AddNewCueButton_ToolTip", "Add a new cue based on the current state of the  control console."),
						FAppStyle::Get().GetBrush("Icons.Plus"),
						FStyleColors::AccentGreen
					)
				]
			]

			// Store Cue button section
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SDMXControlConsoleEditorCueStackView::OnStoreCueClicked)
				.IsEnabled(this, &SDMXControlConsoleEditorCueStackView::IsStoreCueButtonEnabled)
				.ContentPadding(FMargin(0.f, 4.f))
				[
					GenerateCueListToolbarButtonContent
					(
						LOCTEXT("StoreCueButton_Label", "Store"),
						LOCTEXT("StoreCueButton_ToolTip", "Stores the current state of te console in the selected cue."),
						FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.StoreCue"),
						FStyleColors::White
					)
				]
			]

			// Clear Stack button section
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f)
			[
				SNew(SButton)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ForegroundColor(FSlateColor::UseStyle())
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked(this, &SDMXControlConsoleEditorCueStackView::OnClearStackClicked)
				.IsEnabled(this, &SDMXControlConsoleEditorCueStackView::IsClearAllCuesButtonEnabled)
				.ContentPadding(FMargin(0.f, 4.f))
				[
					GenerateCueListToolbarButtonContent
					(
						LOCTEXT("ClearAllCuesButton_Label", "Clear All"),
						LOCTEXT("ClearAllCuesButton_ToolTip", "Clear all the cues in the stack."),
						FAppStyle::Get().GetBrush("Icons.Delete"),
						FStyleColors::White
					)
				]
			];

		return CueListToolbar;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorCueStackView::GenerateCueListToolbarButtonContent(const FText& Label, const FText& ToolTip, const FSlateBrush* IconBrush, const FSlateColor IconColor)
	{
		const TSharedRef<SWidget> CueListToolbarButtonContent =
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(IconBrush)
				.ColorAndOpacity(IconColor)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				.Text(Label)
				.ToolTipText(ToolTip)
				.TextStyle(FAppStyle::Get(), "SmallButtonText")
			];

		return CueListToolbarButtonContent;
	}

	bool SDMXControlConsoleEditorCueStackView::IsAddNewCueButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		return ControlConsoleData && !ControlConsoleData->GetAllFaderGroups().IsEmpty();
	}

	bool SDMXControlConsoleEditorCueStackView::IsStoreCueButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		
		const bool bIsAnyCueSelected = CueList.IsValid() && !CueList->GetSelectedCueItems().IsEmpty();
		return ControlConsoleCueStack && ControlConsoleCueStack->CanStore() && bIsAnyCueSelected;
	}

	bool SDMXControlConsoleEditorCueStackView::IsClearAllCuesButtonEnabled() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		return ControlConsoleCueStack && !ControlConsoleCueStack->GetCuesArray().IsEmpty();
	}

	FReply SDMXControlConsoleEditorCueStackView::OnAddNewCueClicked()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return FReply::Unhandled();
		}

		TArray<UDMXControlConsoleFaderBase*> FadersToCue;
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FadersToCue.Append(FaderGroup->GetAllFaders());
			}
		}

		if (FadersToCue.IsEmpty())
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction AddNewCueTransaction(LOCTEXT("AddNewCueTransaction", "Add Cue"));
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->AddNewCue(FadersToCue);
		ControlConsoleCueStack->PostEditChange();

		if (CueList.IsValid())
		{
			CueList->RequestRefresh();
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorCueStackView::OnStoreCueClicked()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack || !CueList.IsValid())
		{
			return FReply::Unhandled();
		}

		const TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> SelectedItems = CueList->GetSelectedCueItems();
		if (SelectedItems.IsEmpty())
		{
			return FReply::Unhandled();
		}

		const TSharedPtr<FDMXControlConsoleEditorCueListItem> SelectedItem = SelectedItems[0];
		if (!SelectedItem.IsValid())
		{
			return FReply::Unhandled();
		}

		TArray<UDMXControlConsoleFaderBase*> FadersToCue;
		const FDMXControlConsoleCue& SelectedCue = SelectedItem->GetCue();
		const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
		{
			if (FaderGroup)
			{
				FadersToCue.Append(FaderGroup->GetAllFaders());
			}
		}

		ControlConsoleCueStack->UpdateCueData(SelectedCue.CueID, FadersToCue);
		if (CueList.IsValid())
		{
			CueList->RequestRefresh();
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorCueStackView::OnClearStackClicked()
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction ClearAllCuesTransaction(LOCTEXT("ClearAllCuesTransaction", "Clear Cue"));
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->Clear();
		ControlConsoleCueStack->PostEditChange();

		if (CueList.IsValid())
		{
			CueList->RequestRefresh();
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorCueStackView::OnDMXLibraryChanged()
	{
		OnClearStackClicked();
	}
}

#undef LOCTEXT_NAMESPACE

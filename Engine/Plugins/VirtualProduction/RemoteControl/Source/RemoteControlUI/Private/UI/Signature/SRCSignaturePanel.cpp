// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCSignaturePanel.h"
#include "Columns/RCSignatureDescriptionColumn.h"
#include "Columns/RCSignatureLabelColumn.h"
#include "Items/RCSignatureTreeSignatureItem.h"
#include "Misc/MessageDialog.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSignatureRegistry.h"
#include "SRCSignatureTree.h"
#include "ScopedTransaction.h"
#include "Styling/RemoteControlStyles.h"
#include "UI/Panels/SRCDockPanel.h"
#include "UI/RemoteControlPanelStyle.h"
#include "UI/SRemoteControlPanel.h"

#define LOCTEXT_NAMESPACE "SRCSignaturePanel"

void SRCSignaturePanel::Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel)
{
	SRCLogicPanelBase::Construct(SRCLogicPanelBase::FArguments(), InPanel);

	const FRCPanelStyle* RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");

	// Signature Dock Panel
	TSharedRef<SRCMinorPanel> SignatureDockPanel = SNew(SRCMinorPanel)
		.HeaderLabel(LOCTEXT("SignaturesLabel", "Signatures"))
		.EnableFooter(false)
		[
			SAssignNew(SignatureTreeView, SRCSignatureTree, SharedThis(this), InPanel)
			.Columns(
				{
					MakeShared<FRCSignatureLabelColumn>(),
					MakeShared<FRCSignatureDescriptionColumn>(),
				})
		];

	// Add New Signature Button
	TSharedRef<SButton> AddSignatureButton = SNew(SButton)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Add Signature")))
		.IsEnabled_Lambda([LiveMode = InArgs._LiveMode]() { return !LiveMode.Get(); })
		.OnClicked(this, &SRCSignaturePanel::OnAddButtonClicked)
		.ForegroundColor(FSlateColor::UseForeground())
		.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
		.ContentPadding(FMargin(4.f, 2.f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Content()
		[
			SNew(SBox)
			.WidthOverride(RCPanelStyle->IconSize.X)
			.HeightOverride(RCPanelStyle->IconSize.Y)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
			]
		];

	SignatureDockPanel->AddHeaderToolbarItem(EToolbar::Left, AddSignatureButton);

	ChildSlot
	.Padding(RCPanelStyle->PanelPadding)
	[
		SignatureDockPanel
	];
}

URemoteControlSignatureRegistry* SRCSignaturePanel::GetSignatureRegistry() const
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		return Preset->GetSignatureRegistry();
	}
	return nullptr;
}

void SRCSignaturePanel::AddToSignature(const FRCExposesPropertyArgs& InPropertyArgs)
{
	if (!SignatureTreeView.IsValid())
	{
		return;
	}

	URemoteControlSignatureRegistry* Registry = GetSignatureRegistry();
	if (!Registry)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddToSignatureTransaction", "Add to Signature"));
	Registry->Modify();

	bool bFieldsAdded = false;

	TArray<TSharedPtr<FRCSignatureTreeItemBase>> SelectedItems = SignatureTreeView->GetSelectedItems();
	if (SelectedItems.IsEmpty())
	{
		// Make a new signature with temp view model item to add the field
		TSharedRef<FRCSignatureTreeSignatureItem> SignatureItem = MakeShared<FRCSignatureTreeSignatureItem>(Registry->AddSignature(), SignatureTreeView);
		if (SignatureItem->AddField(Registry, InPropertyArgs))
		{
			bFieldsAdded = true;
		}
	}
	else
	{
		for (const TSharedPtr<FRCSignatureTreeItemBase>& Item : SelectedItems)
		{
			if (FRCSignatureTreeSignatureItem* SignatureItem = Item->AsSignatureItem())
			{
				if (SignatureItem->AddField(Registry, InPropertyArgs))
				{
					bFieldsAdded = true;
				}
			}
		}
	}

	if (bFieldsAdded)
	{
		SignatureTreeView->Refresh();
	}
	else
	{
		Transaction.Cancel();
	}
}

bool SRCSignaturePanel::IsListFocused() const
{
	return SignatureTreeView.IsValid() && SignatureTreeView->IsListFocused();
}

TArray<TSharedPtr<FRCLogicModeBase>> SRCSignaturePanel::GetSelectedLogicItems() const
{
	if (SignatureTreeView.IsValid())
	{
		return SignatureTreeView->GetSelectedLogicItems();
	}
	return {};
}

FReply SRCSignaturePanel::RequestDeleteSelectedItem()
{
	if (!SignatureTreeView.IsValid())
	{
		return FReply::Unhandled();
	}

	const EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo
		, LOCTEXT("DeleteSelectedWarning", "Delete the Selected Signatures?"));

	if (UserResponse == EAppReturnType::Yes)
	{
		DeleteSelectedPanelItems();
	}

	return FReply::Handled();
}

FReply SRCSignaturePanel::RequestDeleteAllItems()
{
	if (!SignatureTreeView.IsValid())
	{
		return FReply::Unhandled();
	}

	const EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::YesNo
		, FText::Format(LOCTEXT("DeleteAllWarning", "You are about to delete {0} signatures. Are you sure you want to proceed?"), SignatureTreeView->Num()));

	if (UserResponse == EAppReturnType::Yes)
	{
		return DeleteAllItems();
	}

	return FReply::Handled();
}

bool SRCSignaturePanel::CanCopyItems() const
{
	// todo: Copy/Paste Signatures
	return false;
}

bool SRCSignaturePanel::CanDuplicateItems() const
{
	// todo: Duplicate Signatures
	return false;
}

void SRCSignaturePanel::DeleteSelectedPanelItems()
{
	if (SignatureTreeView.IsValid())
	{
		SignatureTreeView->DeleteSelectedPanelItems();
	}
}

FReply SRCSignaturePanel::OnAddButtonClicked()
{
	URemoteControlSignatureRegistry* SignatureRegistry = GetSignatureRegistry();
	if (!SignatureRegistry)
	{
		return FReply::Unhandled();
	}

	// Add new signature
	{
		FScopedTransaction Transaction(LOCTEXT("NewSignature", "New Signature"));
		SignatureRegistry->Modify();
		SignatureRegistry->AddSignature();
	}

	if (SignatureTreeView.IsValid())
	{
		SignatureTreeView->Refresh();
	}

	return FReply::Handled();
}

FReply SRCSignaturePanel::DeleteAllItems()
{
	URemoteControlSignatureRegistry* SignatureRegistry = GetSignatureRegistry();
	if (!SignatureRegistry)
	{
		return FReply::Unhandled();
	}

	// Don't generate a transaction + modify if the signature container is already empty
	if (SignatureRegistry->GetSignatures().IsEmpty())
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("EmptySignatures", "Empty Signatures"));
	SignatureRegistry->Modify();
	SignatureRegistry->EmptySignatures();

	if (SignatureTreeView.IsValid())
	{
		SignatureTreeView->Refresh();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE 

// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCSignatureDetails.h"
#include "DetailsViewArgs.h"
#include "IStructureDataProvider.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RemoteControlSignatureRegistry.h"
#include "ScopedTransaction.h"
#include "UI/Signature/Items/RCSignatureTreeRootItem.h"
#include "UI/Signature/RCSignatureTreeItemSelection.h"

#define LOCTEXT_NAMESPACE "SRCSignatureDetails"

void SRCSignatureDetails::Construct(const FArguments& InArgs, URemoteControlSignatureRegistry* InSignatureRegistry, const TSharedRef<FRCSignatureTreeItemSelection>& InSelection)
{
	SignatureRegistryWeak = InSignatureRegistry;
	SelectionWeak = InSelection;

	InSelection->OnSelectionChanged().AddSP(this, &SRCSignatureDetails::Refresh);

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.NotifyHook = this;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, FStructureDetailsViewArgs(), /*StructOnScope*/nullptr);
	StructDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SRCSignatureDetails::OnFinishedChangingProperties);

	ChildSlot
	[
		StructDetailsView->GetWidget().ToSharedRef()
	];

	Refresh();
}

SRCSignatureDetails::~SRCSignatureDetails()
{
	if (TSharedPtr<FRCSignatureTreeItemSelection> Selection = SelectionWeak.Pin())
	{
		Selection->OnSelectionChanged().RemoveAll(this);
	}
}

void SRCSignatureDetails::Refresh()
{
	TSharedRef<FStructOnScopeStructureDataProvider> StructProvider = MakeShared<FStructOnScopeStructureDataProvider>();
	StructProvider->SetStructData(GatherStructOnScopes());
	StructDetailsView->SetStructureProvider(StructProvider);
}

TArray<TSharedPtr<FStructOnScope>> SRCSignatureDetails::GatherStructOnScopes() const
{
	TArray<TSharedPtr<FStructOnScope>> StructOnScopes;

	TSharedPtr<FRCSignatureTreeItemSelection> Selection = SelectionWeak.Pin();
	if (!Selection.IsValid())
	{
		return StructOnScopes;
	}

	TConstArrayView<TWeakPtr<FRCSignatureTreeItemBase>> SelectedItems = Selection->GetSelectedItemsView();
	StructOnScopes.Reserve(SelectedItems.Num());

	for (const TWeakPtr<FRCSignatureTreeItemBase>& SelectedItemWeak : SelectedItems)
	{
		TSharedPtr<FRCSignatureTreeItemBase> SelectedItem = SelectedItemWeak.Pin();
		if (!SelectedItem.IsValid())
		{
			continue;
		}

		TSharedPtr<FStructOnScope> StructOnScope = SelectedItem->MakeSelectionStruct();
		if (!StructOnScope.IsValid())
		{
			continue;
		}

		StructOnScopes.Add(MoveTemp(StructOnScope));
	}

	return StructOnScopes;
}

void SRCSignatureDetails::OnFinishedChangingProperties(const FPropertyChangedEvent& InChangeEvent)
{
	// End Transaction
	CurrentTransaction.Reset();
}

void SRCSignatureDetails::NotifyPreChange(FEditPropertyChain* InPropertyAboutToChange)
{
	if (URemoteControlSignatureRegistry* SignatureRegistry = SignatureRegistryWeak.Get())
	{
		// Begin Transaction
		ensureAlways(!CurrentTransaction.IsValid());
		CurrentTransaction = MakeShared<FScopedTransaction>(LOCTEXT("EditSignature", "Edit Signature"));
		SignatureRegistry->Modify();
	}
}

#undef LOCTEXT_NAMESPACE 

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Misc/NotifyHook.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FRCSignatureTreeItemSelection;
class FScopedTransaction;
class FStructOnScope;
class IStructureDetailsView;
class URCSignatureRegistry;
struct FPropertyChangedEvent;

class SRCSignatureDetails : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SRCSignatureDetails) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, URCSignatureRegistry* InSignatureRegistry, const TSharedRef<FRCSignatureTreeItemSelection>& InSelection);

	virtual ~SRCSignatureDetails() override;

	void Refresh();

private:
	TArray<TSharedPtr<FStructOnScope>> GatherStructOnScopes() const;

	void OnFinishedChangingProperties(const FPropertyChangedEvent& InChangeEvent);

	//~ Begin FNotifyHook
	virtual void NotifyPreChange(FEditPropertyChain* InPropertyAboutToChange) override;
	//~ End FNotifyHook

	TSharedPtr<IStructureDetailsView> StructDetailsView;

	TSharedPtr<FScopedTransaction> CurrentTransaction;

	TWeakObjectPtr<URCSignatureRegistry> SignatureRegistryWeak;

	TWeakPtr<FRCSignatureTreeItemSelection> SelectionWeak;
};

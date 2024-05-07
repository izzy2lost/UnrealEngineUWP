// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "RigVMCore/RigVMVariant.h"

DECLARE_DELEGATE_OneParam(FRigVMVariantWidget_OnVariantChanged, const FRigVMVariant&);
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<SWidget>, FRigVMVariantWidget_OnCreateVariantRefRow, const FRigVMVariantRef&);
DECLARE_DELEGATE_OneParam(FRigVMVariantWidget_OnBrowseVariantRef, const FRigVMVariantRef&);

class SRigVMVariantWidget : public SBox
{
public:
	
	SLATE_BEGIN_ARGS(SRigVMVariantWidget)
		: _MaxVariantRefListHeight(200.f)
	{
	}
	SLATE_ATTRIBUTE(FRigVMVariant, Variant)
	SLATE_ATTRIBUTE(TArray<FRigVMVariantRef>, VariantRefs)
	SLATE_EVENT(FRigVMVariantWidget_OnVariantChanged, OnVariantChanged)
	SLATE_EVENT(FRigVMVariantWidget_OnCreateVariantRefRow, OnCreateVariantRefRow);
	SLATE_EVENT(FRigVMVariantWidget_OnBrowseVariantRef, OnBrowseVariantRef)
	SLATE_ATTRIBUTE(float, MaxVariantRefListHeight)
	SLATE_END_ARGS()

	SRigVMVariantWidget();
	virtual ~SRigVMVariantWidget() override;
	
	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	EVisibility GetVariantRefListVisibility() const;
	TSharedPtr<SWidget> CreateDefaultVariantRefRow(const FRigVMVariantRef& InVariantRef) const;
	void RebuildVariantRefList();

	TAttribute<FRigVMVariant> VariantAttribute;
	TAttribute<TArray<FRigVMVariantRef>> VariantRefsAttribute;
	FRigVMVariantWidget_OnVariantChanged OnVariantChanged;
	FRigVMVariantWidget_OnCreateVariantRefRow OnCreateVariantRefRow;
    FRigVMVariantWidget_OnBrowseVariantRef OnBrowseVariantRef;

	TArray<FRigVMVariantRef> VariantRefs;
	uint32 VariantRefHash;
	TSharedPtr<SVerticalBox> VariantRefListBox;
};
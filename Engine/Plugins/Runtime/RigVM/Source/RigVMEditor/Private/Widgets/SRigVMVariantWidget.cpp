// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SRigVMVariantWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "SRigVMVariantWidget"

SRigVMVariantWidget::SRigVMVariantWidget()
	: VariantRefHash(UINT32_MAX)
{
}

SRigVMVariantWidget::~SRigVMVariantWidget()
{
}

void SRigVMVariantWidget::Construct(
	const FArguments& InArgs)
{
	VariantAttribute = InArgs._Variant;
	
	OnVariantChanged = InArgs._OnVariantChanged;

	VariantRefsAttribute = InArgs._VariantRefs;
	OnCreateVariantRefRow = InArgs._OnCreateVariantRefRow;
	OnBrowseVariantRef = InArgs._OnBrowseVariantRef;

	if(!OnCreateVariantRefRow.IsBound())
	{
		OnCreateVariantRefRow.BindSP(this, &SRigVMVariantWidget::CreateDefaultVariantRefRow);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([this]()
			{
				const FRigVMVariant Variant = VariantAttribute.Get();
				return FText::FromString(Variant.Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 8, 0, 0)
		[
			SAssignNew(TagWidget, SRigVMVariantTagWidget)
			.OnGetTags(InArgs._OnGetTags)
			.OnAddTag(InArgs._OnAddTag)
			.OnRemoveTag(InArgs._OnRemoveTag)
			.CanAddTags(InArgs._CanAddTags)
			.EnableContextMenu(InArgs._EnableTagContextMenu)
			.MinDesiredLabelWidth(50.f)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 8, 0, 0)
		[
			SNew(SScrollBox)
			.Visibility(this, &SRigVMVariantWidget::GetVariantRefListVisibility)
			+ SScrollBox::Slot()
			.MaxSize(InArgs._MaxVariantRefListHeight)
			[
				SAssignNew(VariantRefListBox, SVerticalBox)
			]
		]
	];

	SetCanTick(true);
}

void SRigVMVariantWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SBox::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	uint32 NewHash = 0;
	const TArray<FRigVMVariantRef> NewVariantRefs = VariantRefsAttribute.Get();
	for(const FRigVMVariantRef& NewVariantRef : NewVariantRefs)
	{
		NewHash = HashCombine(NewHash, GetTypeHash(NewVariantRef));
	}

	if(NewHash != VariantRefHash)
	{
		VariantRefHash = NewHash;
		VariantRefs = NewVariantRefs;
		VariantRefs.Sort([](const FRigVMVariantRef& A, const FRigVMVariantRef& B)
		{
			return A.ObjectPath.ToString().Compare(B.ObjectPath.ToString()) < 0;
		});
		
		RebuildVariantRefList();
	}
}

EVisibility SRigVMVariantWidget::GetVariantRefListVisibility() const
{
	return VariantRefs.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedPtr<SWidget> SRigVMVariantWidget::CreateDefaultVariantRefRow(const FRigVMVariantRef& InVariantRef) const
{
	return SNew(STextBlock)
	.Text(FText::FromString(InVariantRef.ObjectPath.ToString()));
}

void SRigVMVariantWidget::RebuildVariantRefList()
{
	VariantRefListBox->ClearChildren();

	for(FRigVMVariantRef VariantRef : VariantRefs)
	{
		TSharedPtr<SWidget> Widget = OnCreateVariantRefRow.Execute(VariantRef);
		Widget->SetOnMouseDoubleClick(
			FPointerEventHandler::CreateLambda(
				[VariantRef, this](const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply
				{
					(void)OnBrowseVariantRef.ExecuteIfBound(VariantRef);
					return FReply::Handled();
				}
			)
		);
		VariantRefListBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 4, 0, 0)
		[
			Widget.ToSharedRef()
		];
	}
}

#undef LOCTEXT_NAMESPACE // SRigVMVariantWidget
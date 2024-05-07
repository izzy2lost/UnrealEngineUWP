// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMVariantDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Misc/UObjectToken.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SRigVMLogWidget.h"

#define LOCTEXT_NAMESPACE "RigVMVariantDetailCustomization"

class FUObjectToken;

void FRigVMVariantDetailCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		InStructPropertyHandle->CreatePropertyValueWidget()
	];

	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	ensure(Objects.Num() == 1); // This is in here to ensure we are only showing the modifier details in the blueprint editor

	for (UObject* Object : Objects)
	{
		if (Object->IsA<URigVMBlueprint>())
		{
			BlueprintBeingCustomized = Cast<URigVMBlueprint>(Object);
		}
	}
}

void FRigVMVariantDetailCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if (InStructPropertyHandle->IsValidHandle())
	{
		uint32 NumChildren = 0;
		InStructPropertyHandle->GetNumChildren(NumChildren);

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			StructBuilder.AddProperty(InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
		}

		// matching variants
		StructBuilder.AddCustomRow(FText::GetEmpty())
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]()
		{
			return IsAssetVariant() ? EVisibility::Visible : EVisibility::Collapsed;
		}))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Matching Variants")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SAssignNew(VariantLog, SRigVMLogWidget)
			.LogLabel(LOCTEXT("Variants", "Variants"))
			.LogName(TEXT("RigVMAssetVariants"))
			.ShowFilters(false)
			.AllowClear(false)
			.DiscardDuplicates(true)
			.ScrollToBottom(false)
			.HeightOverride(120)
		];
	}

	RefreshVariantLog();
}

bool FRigVMVariantDetailCustomization::IsAssetVariant() const
{
	if (BlueprintBeingCustomized)
	{
		const FRigVMVariant& Variant = BlueprintBeingCustomized->AssetVariant;
		TArray<FRigVMVariantRef> Variants = URigVMBuildData::Get()->FindAssetVariantRefs(Variant.Guid);
		return Variants.Num() > 1;
	}
	return false;
}

FText FRigVMVariantDetailCustomization::GetVariantGuidText() const
{
	if (BlueprintBeingCustomized)
	{
		const FGuid Guid = BlueprintBeingCustomized->AssetVariant.Guid;
		if(Guid.IsValid())
		{
			return FText::FromString(Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
	}
	return FText();
}

void FRigVMVariantDetailCustomization::RefreshVariantLog()
{
	check(VariantLog);

	VariantLog->GetListing()->ClearMessages();

	if (BlueprintBeingCustomized)
	{
		TArray<FRigVMVariantRef> Variants = URigVMBuildData::Get()->FindAssetVariantRefs(BlueprintBeingCustomized->AssetVariant.Guid);
		Variants = Variants.FilterByPredicate([this](const FRigVMVariantRef& VariantRef) { return VariantRef.ObjectPath != BlueprintBeingCustomized->GetPathName(); });
		Variants.Sort([](const FRigVMVariantRef& A, const FRigVMVariantRef& B)
		{
			return A.ObjectPath.ToString().Compare(B.ObjectPath.ToString()) < 0;
		});

		for (FRigVMVariantRef& Variant : Variants)
		{
			const TSharedRef<FAssetNameToken> AssetToken = FAssetNameToken::Create(Variant.ObjectPath.ToString());
			const TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Info);
			Message->AddToken(AssetToken);
			VariantLog->GetListing()->AddMessage(Message);
		}
	}
}

#undef LOCTEXT_NAMESPACE

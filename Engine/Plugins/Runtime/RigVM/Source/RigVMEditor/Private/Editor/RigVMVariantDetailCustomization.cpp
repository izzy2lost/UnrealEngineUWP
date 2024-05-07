// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMVariantDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Editor/RigVMEditorTools.h"
#include "Misc/UObjectToken.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SRigVMVariantWidget.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RigVMVariantDetailCustomization"

class FUObjectToken;

void FRigVMVariantDetailCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	ensure(Objects.Num() == 1); // This is in here to ensure we are only showing the modifier details in the blueprint editor

	for (UObject* Object : Objects)
	{
		if (Object->IsA<URigVMBlueprint>())
		{
			BlueprintBeingCustomized = Cast<URigVMBlueprint>(Object);
			break;
		}
	}

	HeaderRow
	.OverrideResetToDefault(FResetToDefaultOverride::Hide())
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SRigVMVariantWidget)
		.Variant(this, &FRigVMVariantDetailCustomization::GetVariant)
		.VariantRefs(this, &FRigVMVariantDetailCustomization::GetVariantRefs)
		.OnVariantChanged(this, &FRigVMVariantDetailCustomization::OnVariantChanged)
		.OnBrowseVariantRef(this, &FRigVMVariantDetailCustomization::OnBrowseVariantRef)
	];
}

void FRigVMVariantDetailCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// nothing to do here
}

FRigVMVariant FRigVMVariantDetailCustomization::GetVariant() const
{
	if (BlueprintBeingCustomized)
	{
		return BlueprintBeingCustomized->AssetVariant;
	}
	return FRigVMVariant();
}

TArray<FRigVMVariantRef> FRigVMVariantDetailCustomization::GetVariantRefs() const
{
	if (BlueprintBeingCustomized)
	{
		const FRigVMVariant& Variant = BlueprintBeingCustomized->AssetVariant;
		TArray<FRigVMVariantRef> Variants = URigVMBuildData::Get()->FindAssetVariantRefs(Variant.Guid);
		const FRigVMVariantRef MyVariantRef = FRigVMVariantRef(BlueprintBeingCustomized->GetPathName(), BlueprintBeingCustomized->AssetVariant);
		Variants.RemoveAll([MyVariantRef](const FRigVMVariantRef& VariantRef) -> bool
		{
			return VariantRef == MyVariantRef;
		});
		return Variants;
	}
	return TArray<FRigVMVariantRef>();
}

void FRigVMVariantDetailCustomization::OnVariantChanged(const FRigVMVariant& InNewVariant)
{
	if(BlueprintBeingCustomized)
	{
		FScopedTransaction Transaction(LOCTEXT("ChangedVariantInfo", "Changed Blueprint Variant Information"));
		BlueprintBeingCustomized->Modify();
		BlueprintBeingCustomized->AssetVariant = InNewVariant;
	}
}

void FRigVMVariantDetailCustomization::OnBrowseVariantRef(const FRigVMVariantRef& InVariantRef)
{
	const FAssetData AssetData = UE::RigVM::Editor::Tools::FindAssetFromAnyPath(InVariantRef.ObjectPath.ToString(), true);
	if(AssetData.IsValid())
	{
		const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.Get().SyncBrowserToAssets({AssetData});
	}
}

#undef LOCTEXT_NAMESPACE

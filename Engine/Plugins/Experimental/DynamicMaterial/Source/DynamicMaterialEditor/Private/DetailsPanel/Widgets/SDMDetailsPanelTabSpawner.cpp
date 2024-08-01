// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsPanel/Widgets/SDMDetailsPanelTabSpawner.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DynamicMaterialEditorStyle.h"
#include "IAssetTools.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelBase.h"
#include "Model/DynamicMaterialModelFactory.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMDetailsPanelTabSpawner"

void SDMDetailsPanelTabSpawner::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle)
{
	PropertyHandle = InPropertyHandle;

	TArray<UObject*> OuterObjects;
	InPropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	UObject* Value = nullptr;
	InPropertyHandle->GetValue(Value);

	// @formatter:off
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowClear(true)
			.AllowedClass(UDynamicMaterialModelBase::StaticClass())
			.DisplayBrowse(true)
			.DisplayThumbnail(false)
			.DisplayCompactSize(true)
			.DisplayUseSelected(true)
			.EnableContentPicker(true)
			.ObjectPath(this, &SDMDetailsPanelTabSpawner::GetEditorPath)
			.OnObjectChanged(this, &SDMDetailsPanelTabSpawner::OnEditorChanged)
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		.AutoHeight()
		[
			SNew(SButton)
			.OnClicked(this, &SDMDetailsPanelTabSpawner::OnButtonClicked)
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(this, &SDMDetailsPanelTabSpawner::GetButtonText)
			]
		]
	];
	// @formatter:on
}

UDynamicMaterialModelBase* SDMDetailsPanelTabSpawner::GetMaterialModelBase() const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return nullptr;
	}

	UObject* Value = nullptr;
	PropertyHandle->GetValue(Value);

	return Cast<UDynamicMaterialModelBase>(Value);
}

void SDMDetailsPanelTabSpawner::SetMaterialModelBase(UDynamicMaterialModelBase* InNewModel)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	PropertyHandle->SetValueFromFormattedString(InNewModel ? InNewModel->GetPathName() : "");
}

FText SDMDetailsPanelTabSpawner::GetButtonText() const
{
	if (GetMaterialModelBase())
	{
		return LOCTEXT("OpenMaterialDesignerModel", "Edit with Material Designer");
	}

	return LOCTEXT("CreateMaterialDesignerModel", "Create with Material Designer");
}

FReply SDMDetailsPanelTabSpawner::OnButtonClicked()
{
	if (GetMaterialModelBase())
	{
		return OpenDynamicMaterialModelTab();
	}

	return CreateDynamicMaterialModel();
}

FReply SDMDetailsPanelTabSpawner::CreateDynamicMaterialModel()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	// We already have a builder, so we don't need to create one
	if (MaterialModelBase)
	{
		return FReply::Unhandled();
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return FReply::Unhandled();
	}

	FString PackageName, AssetName;
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.CreateUniqueAssetName(
		UDynamicMaterialModelFactory::BaseDirectory / UDynamicMaterialModelFactory::BaseName + FGuid::NewGuid().ToString(),
		"",
		PackageName,
		AssetName
	);

	UPackage* Package = CreatePackage(*PackageName);
	check(Package);

	UDynamicMaterialModelFactory* DynamicMaterialModelFactory = NewObject<UDynamicMaterialModelFactory>();
	check(DynamicMaterialModelFactory);

	UDynamicMaterialModel* NewModel = Cast<UDynamicMaterialModel>(DynamicMaterialModelFactory->FactoryCreateNew(
		UDynamicMaterialModelBase::StaticClass(),
		Package,
		*AssetName,
		RF_Standalone | RF_Public,
		nullptr,
		GWarn
	));

	FAssetRegistryModule::AssetCreated(NewModel);

	PropertyHandle->SetValueFromFormattedString(NewModel->GetPathName());

	return OpenDynamicMaterialModelTab();
}

FReply SDMDetailsPanelTabSpawner::ClearDynamicMaterialModel()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	// We don't have a builder, so we don't need to clear it
	if (!MaterialModelBase)
	{
		return FReply::Unhandled();
	}

	SetMaterialModelBase(nullptr);

	return FReply::Handled();
}

FReply SDMDetailsPanelTabSpawner::OpenDynamicMaterialModelTab()
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	// We don't have a builder, so we can't open it
	if (!MaterialModelBase)
	{
		return FReply::Unhandled();
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.OpenEditorForAssets({MaterialModelBase});

	return FReply::Handled();
}

FString SDMDetailsPanelTabSpawner::GetEditorPath() const
{
	UDynamicMaterialModelBase* MaterialModelBase = GetMaterialModelBase();

	return MaterialModelBase ? MaterialModelBase->GetPathName() : "";
}

void SDMDetailsPanelTabSpawner::OnEditorChanged(const FAssetData& InAssetData)
{
	SetMaterialModelBase(Cast<UDynamicMaterialModelBase>(InAssetData.GetAsset()));
}

#undef LOCTEXT_NAMESPACE

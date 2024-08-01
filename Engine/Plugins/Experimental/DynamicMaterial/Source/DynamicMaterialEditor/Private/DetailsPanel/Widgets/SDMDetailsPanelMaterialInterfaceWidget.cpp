// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsPanel/Widgets/SDMDetailsPanelMaterialInterfaceWidget.h"

#include "Components/PrimitiveComponent.h"
#include "DMObjectMaterialProperty.h"
#include "DynamicMaterialEditorStyle.h"
#include "IDynamicMaterialEditorModule.h"
#include "Material/DynamicMaterialInstance.h"
#include "Material/DynamicMaterialInstanceFactory.h"
#include "Model/DynamicMaterialModel.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMDetailsPanelMaterialInterfaceWidget"

void SDMDetailsPanelMaterialInterfaceWidget::Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle)
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

	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	FObjectPropertyBase* const ObjectProperty = CastField<FObjectPropertyBase>(PropertyHandle->GetProperty());

	UClass* const ObjectClass = ObjectProperty
		? ToRawPtr(ObjectProperty->PropertyClass)
		: ToRawPtr(UMaterialInterface::StaticClass());

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
			.AllowedClass(ObjectClass)
			.DisplayBrowse(true)
			.DisplayCompactSize(false)
			.DisplayThumbnail(true)
			.DisplayUseSelected(true)
			.EnableContentPicker(true)
			.PropertyHandle(PropertyHandle)
			.ThumbnailPool(InArgs._ThumbnailPool)
		]
		+ SVerticalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(10.f, 5.f, 10.f, 5.f)
		.AutoHeight()
		[
			SNew(SButton)
			.OnClicked(this, &SDMDetailsPanelMaterialInterfaceWidget::OnButtonClicked)
			.IsEnabled(InPropertyHandle, &IPropertyHandle::IsEditable)
			.Content()
			[
				SNew(STextBlock)
				.TextStyle(FDynamicMaterialEditorStyle::Get(), "RegularFont")
				.Text(this, &SDMDetailsPanelMaterialInterfaceWidget::GetButtonText)
			]
		]
	];
	// @formatter:on
}

UObject* SDMDetailsPanelMaterialInterfaceWidget::GetAsset() const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return nullptr;
	}

	UObject* Value = nullptr;
	PropertyHandle->GetValue(Value);

	return Value;
}

UDynamicMaterialInstance* SDMDetailsPanelMaterialInterfaceWidget::GetDynamicMaterialInstance() const
{
	return Cast<UDynamicMaterialInstance>(GetAsset());
}

void SDMDetailsPanelMaterialInterfaceWidget::SetAsset(UObject* NewAsset)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return;
	}

	PropertyHandle->SetValueFromFormattedString(NewAsset ? NewAsset->GetPathName() : "");
}

void SDMDetailsPanelMaterialInterfaceWidget::SetDynamicMaterialInstance(UDynamicMaterialInstance* NewInstance)
{
	SetAsset(NewInstance);
}

FText SDMDetailsPanelMaterialInterfaceWidget::GetButtonText() const
{
	if (GetDynamicMaterialInstance())
	{
		return LOCTEXT("OpenMaterialDesignerModel", "Edit with Material Designer");
	}

	return LOCTEXT("CreateMaterialDesignerModel", "Create with Material Designer");
}

FReply SDMDetailsPanelMaterialInterfaceWidget::OnButtonClicked()
{
	if (GetDynamicMaterialInstance())
	{
		return OpenDynamicMaterialInstanceTab();
	}

	return CreateDynamicMaterialInstance();
}

FReply SDMDetailsPanelMaterialInterfaceWidget::CreateDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We already have an instance, so we don't need to create one
	if (Instance)
	{
		return FReply::Unhandled();
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (OuterObjects.IsEmpty())
	{
		return FReply::Unhandled();
	}

	UDynamicMaterialInstanceFactory* DynamicMaterialInstanceFactory = NewObject<UDynamicMaterialInstanceFactory>();
	check(DynamicMaterialInstanceFactory);

	UDynamicMaterialInstance* NewInstance = Cast<UDynamicMaterialInstance>(DynamicMaterialInstanceFactory->FactoryCreateNew(
		UDynamicMaterialInstance::StaticClass(),
		OuterObjects[0],
		"DynamicMaterialInstance",
		RF_NoFlags,
		nullptr,
		GWarn
	));

	PropertyHandle->SetValueFromFormattedString(NewInstance->GetPathName());

	return OpenDynamicMaterialInstanceTab();
}

FReply SDMDetailsPanelMaterialInterfaceWidget::ClearDynamicMaterialInstance()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have an instance, so we don't need to clear it (and don't clear non-MDIs)
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	SetDynamicMaterialInstance(nullptr);

	return FReply::Handled();
}

FReply SDMDetailsPanelMaterialInterfaceWidget::OpenDynamicMaterialInstanceTab()
{
	UDynamicMaterialInstance* Instance = GetDynamicMaterialInstance();

	// We don't have a MDI, so don't try to open it.
	if (!Instance)
	{
		return FReply::Unhandled();
	}

	const IDynamicMaterialEditorModule& MaterialDesignerModule = IDynamicMaterialEditorModule::Get();
	constexpr bool bInvokeTab = true;

	TArray<UObject*> Outers;
	PropertyHandle->GetOuterObjects(Outers);

	if (Outers.Num() == 0)
	{
		MaterialDesignerModule.OpenMaterialModel(Instance->GetMaterialModel(), nullptr, bInvokeTab);
		return FReply::Handled();
	}

	UWorld* OuterWorld = Outers[0]->GetWorld();
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Outers[0]);

	if (!PrimitiveComponent)
	{
		MaterialDesignerModule.OpenMaterialModel(Instance->GetMaterialModel(), OuterWorld, bInvokeTab);
		return FReply::Handled();
	}

	const int32 NumMaterials = PrimitiveComponent->GetNumMaterials();

	for (int32 Index = 0; Index < NumMaterials; ++Index)
	{
		if (PrimitiveComponent->GetMaterial(Index) == Instance)
		{
			MaterialDesignerModule.OpenMaterialObjectProperty({PrimitiveComponent, Index}, OuterWorld, bInvokeTab);
			return FReply::Handled();
		}
	}

	MaterialDesignerModule.OpenMaterialModel(Instance->GetMaterialModel(), OuterWorld, bInvokeTab);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/Customizations/CEEditorEffectorComponentDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Effector/CEEffectorComponent.h"
#include "Effector/CEEffectorExtensionBase.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "CEEditorEffectorComponentDetailCustomization"

void FCEEditorEffectorComponentDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const FName ComponentClassName = UCEEffectorComponent::StaticClass()->GetFName();

	// Remove extension array property
	TSharedRef<IPropertyHandle> ActiveExtensionsProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEEffectorComponent, ActiveExtensions), UCEEffectorComponent::StaticClass());

	if (!ActiveExtensionsProperty->IsValidHandle())
	{
		return;
	}

	InDetailBuilder.HideProperty(ActiveExtensionsProperty);

	// Everything needs to be below Effector category
	const FName EffectorCategoryName = TEXT("Effector");
	int32 StartOrder = InDetailBuilder.EditCategory(EffectorCategoryName).GetSortOrder() + 1;

	const FName EffectorSectionName = UE::ClonerEffector::EffectorSection::EffectorSection.SectionName;
	const TSharedRef<FPropertySection> EffectorSection = PropertyModule.FindOrCreateSection(ComponentClassName, EffectorSectionName, FText::FromName(EffectorSectionName));
	EffectorSection->AddCategory(EffectorCategoryName);

	// Shape
	{
		TSharedRef<IPropertyHandle> ActiveTypeProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEEffectorComponent, ActiveType), UCEEffectorComponent::StaticClass());

		if (!ActiveTypeProperty->IsValidHandle())
		{
			return;
		}

		const FName TypeCategoryName = ActiveTypeProperty->GetDefaultCategoryName();

		IDetailCategoryBuilder& TypeCategory = InDetailBuilder.EditCategory(TypeCategoryName);
		TypeCategory.SetSortOrder(StartOrder++);

		const FName TypeSectionName = UE::ClonerEffector::EffectorSection::ShapeSection.SectionName;
		const TSharedRef<FPropertySection> TypeSection = PropertyModule.FindOrCreateSection(ComponentClassName, TypeSectionName, FText::FromName(TypeSectionName));
		TypeSection->AddCategory(TypeCategoryName);
	}

	// Mode
	{
		TSharedRef<IPropertyHandle> ActiveModeProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEEffectorComponent, ActiveMode), UCEEffectorComponent::StaticClass());

		if (!ActiveModeProperty->IsValidHandle())
		{
			return;
		}

		const FName ModeCategoryName = ActiveModeProperty->GetDefaultCategoryName();

		IDetailCategoryBuilder& ModeCategory = InDetailBuilder.EditCategory(ModeCategoryName);
		ModeCategory.SetSortOrder(StartOrder++);

		const FName ModeSectionName = UE::ClonerEffector::EffectorSection::ModeSection.SectionName;
		const TSharedRef<FPropertySection> ModeSection = PropertyModule.FindOrCreateSection(ComponentClassName, ModeSectionName, FText::FromName(ModeSectionName));
		ModeSection->AddCategory(ModeCategoryName);
	}

	TFunction<void(const TSharedPtr<IPropertyHandle>&, IDetailCategoryBuilder&)> AddObjectProperty = [&AddObjectProperty](const TSharedPtr<IPropertyHandle>& InPropertyHandle, IDetailCategoryBuilder& InCategoryBuilder)
	{
		uint32 NumChildren = 0;
		InPropertyHandle->GetNumChildren(NumChildren);

		for (uint32 ChildrenIndex = 0; ChildrenIndex < NumChildren; ChildrenIndex++)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = InPropertyHandle->GetChildHandle(ChildrenIndex);

			if (ChildHandle->IsCategoryHandle() && NumChildren == 1)
			{
				AddObjectProperty(ChildHandle, InCategoryBuilder);
			}
			else
			{
				InCategoryBuilder.AddProperty(ChildHandle);
			}
		}
	};

	uint32 NumElements = 0;
	TSharedPtr<IPropertyHandleArray> ActiveExtensionArrayProperty = ActiveExtensionsProperty->AsArray();
	ActiveExtensionArrayProperty->GetNumElements(NumElements);

	for (uint32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
	{
		TSharedPtr<IPropertyHandle> ActiveExtensionProperty = ActiveExtensionArrayProperty->GetElement(ElementIndex);

		UObject* ObjectValue;
		FPropertyAccess::Result ReadResult = ActiveExtensionProperty->GetValue(ObjectValue);

		if (ReadResult != FPropertyAccess::Success)
		{
			continue;
		}

		UCEEffectorExtensionBase* ActiveExtension = Cast<UCEEffectorExtensionBase>(ObjectValue);

		if (!ActiveExtension)
		{
			continue;
		}

		const FName ExtensionSectionName = ActiveExtension->GetExtensionSection().SectionName;

		IDetailCategoryBuilder& ExtensionCategory = InDetailBuilder.EditCategory(ExtensionSectionName);
		ExtensionCategory.SetSortOrder(StartOrder + ActiveExtension->GetExtensionSection().SectionOrder);

		const TSharedRef<FPropertySection> ExtensionSection = PropertyModule.FindOrCreateSection(ComponentClassName, ExtensionSectionName, FText::FromName(ExtensionSectionName));
		ExtensionSection->AddCategory(ExtensionSectionName);

		AddObjectProperty(ActiveExtensionProperty->GetChildHandle(0), ExtensionCategory);
	}
}

void FCEEditorEffectorComponentDetailCustomization::RemoveEmptySections()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	const FName ComponentClassName = UCEEffectorComponent::StaticClass()->GetFName();

	// Remove sections
	PropertyModule.RemoveSection(ComponentClassName, TEXT("Streaming"));
}

#undef LOCTEXT_NAMESPACE

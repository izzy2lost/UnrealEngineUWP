// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/CEEditorEffectorComponentDetailCustomization.h"

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
	// Remove extension array property
	TSharedRef<IPropertyHandle> ActiveExtensionsProperty = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCEEffectorComponent, ActiveExtensions), UCEEffectorComponent::StaticClass());

	if (!ActiveExtensionsProperty->IsValidHandle())
	{
		return;
	}

	InDetailBuilder.HideProperty(ActiveExtensionsProperty);

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

	TArray<FName> CategoriesPriority
	{
		TEXT("Effector"),
		TEXT("Shape"),
		TEXT("Mode")
	};

	uint32 NumElements = 0;
	TSharedPtr<IPropertyHandleArray> ActiveExtensionArrayProperty = ActiveExtensionsProperty->AsArray();
	ActiveExtensionArrayProperty->GetNumElements(NumElements);

	for (uint32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++)
	{
		TSharedPtr<IPropertyHandle> ActiveExtensionProperty = ActiveExtensionArrayProperty->GetElement(ElementIndex);

		UObject* ObjectValue;
		ActiveExtensionProperty->GetValue(ObjectValue);

		UCEEffectorExtensionBase* ActiveExtension = Cast<UCEEffectorExtensionBase>(ObjectValue);

		if (!ActiveExtension)
		{
			continue;
		}

		const FName ExtensionCategory = ActiveExtension->GetExtensionCategory();
		CategoriesPriority.AddUnique(ExtensionCategory);
		IDetailCategoryBuilder& CategoryBuilder = InDetailBuilder.EditCategory(ExtensionCategory);
		AddObjectProperty(ActiveExtensionProperty->GetChildHandle(0), CategoryBuilder);
	}

	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	static const FName ClassName = UCEEffectorComponent::StaticClass()->GetFName();

	int32 SortOrder = InDetailBuilder.EditCategory(TEXT("Transform")).GetSortOrder();

	for (const FName& Category : CategoriesPriority)
	{
		IDetailCategoryBuilder& CategoryBuilder = InDetailBuilder.EditCategory(Category);
		CategoryBuilder.SetSortOrder(SortOrder++);

		const TSharedRef<FPropertySection> Section = PropertyModule.FindOrCreateSection(ClassName, Category, FText::FromName(Category));
		Section->AddCategory(Category);
	}
}

#undef LOCTEXT_NAMESPACE

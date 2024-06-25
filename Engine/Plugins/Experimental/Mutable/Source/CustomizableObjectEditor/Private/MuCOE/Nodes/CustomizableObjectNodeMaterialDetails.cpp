// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/PinViewer/SPinViewer.h"
#include "SSearchableComboBox.h"

class IDetailLayoutBuilder;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeMaterialDetails::MakeInstance()
{
	return MakeShared<FCustomizableObjectNodeMaterialDetails>();
}


void FCustomizableObjectNodeMaterialDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Adding Pin viewr to the details
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	// Get material node
	if (DetailBuilder.GetObjectsOfTypeBeingCustomized<UCustomizableObjectNodeMaterial>().Num())
	{
		MaterialNode = DetailBuilder.GetObjectsOfTypeBeingCustomized<UCustomizableObjectNodeMaterial>()[0];
	}
	else
	{
		return;
	}

	DefaultTextColor = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText").ColorAndOpacity;

	IDetailCategoryBuilder& CustomCategory = DetailBuilder.EditCategory("CustomizableObject");
	TSharedRef<IPropertyHandle> MaterialProperty = DetailBuilder.GetProperty("Material");
	TSharedRef<IPropertyHandle> ComponentNameProperty = DetailBuilder.GetProperty("MeshComponentName");

	// Added Material property manually to display it above of the rest of properties
	CustomCategory.AddProperty(MaterialProperty);

	// Custom widget for the Components property
	CustomCategory.AddProperty(ComponentNameProperty).CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("ComponentComboboxText", "Component:"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		SAssignNew(ComponentComboBoxWidget, SSearchableComboBox)
		.InitiallySelectedItem(SelectedComponentName)
		.OptionsSource(&ComponentNames)
		.OnSelectionChanged(this, &FCustomizableObjectNodeMaterialDetails::OnComponentNameSelectionChanged)
		.OnGenerateWidget(this, &FCustomizableObjectNodeMaterialDetails::GenerateComponentNameComboEntryWidget)
		.OnComboBoxOpening(this, &FCustomizableObjectNodeMaterialDetails::OnOpenComponentsCombobox)
		.Content()
		[
			SAssignNew(ComponentNameTextWidget, STextBlock)
			.Text(this, &FCustomizableObjectNodeMaterialDetails::GenerateSelectedComponentNameWidget)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	];

	// Init ComboBox
	GetComponentNames();

	if (TSharedPtr<FString>* ComponentName = ComponentNames.FindByPredicate([&](TSharedPtr<FString> ComponentName) { return MaterialNode->GetMeshComponentName() == *ComponentName; }))
	{
		SelectedComponentName = *ComponentName;
		ComponentNameTextWidget->SetColorAndOpacity(DefaultTextColor);
	}
	else
	{
		SelectedComponentName = MakeShareable(new FString(MaterialNode->GetMeshComponentName().ToString()));
		ComponentNameTextWidget->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	}
}


void FCustomizableObjectNodeMaterialDetails::GetComponentNames()
{
	ComponentNames.Empty();

	if (UCustomizableObject* RootObject = Cast<UCustomizableObject>(MaterialNode->GetOutermostObject()))
	{
		// ensure we get the root object of the tree
		RootObject = GetRootObject(RootObject);

		if (RootObject)
		{
			for (int32 ComponentIndex = 0; ComponentIndex < RootObject->GetPrivate()->MutableMeshComponents.Num(); ++ComponentIndex)
			{
				ComponentNames.Add(MakeShareable(new FString(RootObject->GetPrivate()->MutableMeshComponents[ComponentIndex].Name.ToString())));
			}
		}
	}
}


void FCustomizableObjectNodeMaterialDetails::OnComponentNameSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		SelectedComponentName = Selection;
		MaterialNode->SetComponentName(FName(*Selection));

		ComponentNameTextWidget->SetColorAndOpacity(DefaultTextColor);
	}
}


TSharedRef<SWidget> FCustomizableObjectNodeMaterialDetails::GenerateComponentNameComboEntryWidget(TSharedPtr<FString> InItem) const
{
	return SNew(STextBlock).Text(FText::FromString(*InItem.Get()));
}


FText FCustomizableObjectNodeMaterialDetails::GenerateSelectedComponentNameWidget() const
{
	if (MaterialNode.IsValid() && SelectedComponentName.IsValid())
	{
		return FText::FromString(*SelectedComponentName.Get());
	}

	return FText();
}


void FCustomizableObjectNodeMaterialDetails::OnOpenComponentsCombobox()
{
	GetComponentNames();
	ComponentComboBoxWidget->RefreshOptions();
}

#undef LOCTEXT_NAMESPACE

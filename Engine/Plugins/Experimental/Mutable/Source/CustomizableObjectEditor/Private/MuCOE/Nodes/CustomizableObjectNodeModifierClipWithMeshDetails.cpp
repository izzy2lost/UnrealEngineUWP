// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithMeshDetails.h"

#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"
#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"

class IPropertyHandle;


#define LOCTEXT_NAMESPACE "CustomizableObjectNodeModifierClipWithMeshDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeModifierClipWithMeshDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeModifierClipWithMeshDetails);
}


void FCustomizableObjectNodeModifierClipWithMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeModifierBaseDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	DetailBuilderPtr = &DetailBuilder;

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if (DetailsView->GetSelectedObjects().Num())
	{
		Node = Cast<UCustomizableObjectNodeModifierClipWithMesh>(DetailsView->GetSelectedObjects()[0].Get());
	}

	IDetailCategoryBuilder& CustomizableObjectToClipCategory = DetailBuilder.EditCategory("CustomizableObjectToClip");

	if(Node != nullptr)
	{
		SelectedCO = Node->CustomizableObjectToClipWith;

		TSharedRef<IPropertyHandle> ParentObjectProperty = DetailBuilder.GetProperty("ParentObject");

		// Hidding unnecessary properties
		DetailBuilder.HideProperty("CustomizableObjectToClipWith");
		DetailBuilder.HideProperty("ArrayMaterialNodeToClipWithID");

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Node->ClipMeshPin());
			ConnectedPin && !Cast<UCustomizableObjectNodeStaticMesh>(ConnectedPin->GetOwningNode()))
		{
			DetailBuilder.HideProperty("Transform");
		}

		ArrayMaterialNodeOption.Empty();
		ArrayMaterialNodeName.Empty();

		TArray<FGuid> ArrayMaterialNodeGuid;
		TSharedPtr<FString> SelectedMethod;

		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeModifierClipWithMeshDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0.0f, 3.0f, 0.0f, 10.0f))
			.FillWidth(0.15f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CustomizableObjectText", "Object:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.85f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UCustomizableObject::StaticClass())
				.OnObjectChanged(this, &FCustomizableObjectNodeModifierClipWithMeshDetails::ClipperCustomizableObjectSelectionChanged)
				.ObjectPath(SelectedCO != nullptr ? SelectedCO->GetPathName() : "")
				.ForceVolatile(true)
			]
		];

		SelectAMethod = MakeShareable(new FString("Select a Method"));

		// Filling ComboBox options
		ClippingMethods.Empty();
		ClippingMethods.Add(MakeShareable(new FString("Mesh Section")));
		ClippingMethods.Add(MakeShareable(new FString("Tags")));
		ClippingMethods.Add(MakeShareable(new FString("Mesh Section & Tags")));

		// Initializing the inital value of the ComboBox
		SelectedMethod = SetInitialClippingMethod();
		
		// This flag is now up to date and can be used to hide ui.
		if (!Node->bUseTags)
		{
			DetailBuilder.HideProperty("Tags");
		}

		// ComboBox to select the Clipping method (material or/and tags)
		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeModifierClipWithMeshDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0f, 8.0f, 0.0f, 5.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ClippingMethod", "Clipping Method:"))
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 5.0f, 0.0f, 5.0f))
			[
				SNew(STextComboBox)
				.OptionsSource(&ClippingMethods)
				.InitiallySelectedItem(SelectedMethod)
				.OnSelectionChanged(this, &FCustomizableObjectNodeModifierClipWithMeshDetails::OnClippingMethodComboBoxSelectionChanged)
			]
		];


		// Filling materials ComboBox with all the possible materials
		int32 NumElementBeforeAdd;
		int32 NumElementAfterAdd;
		TSharedPtr<FString> ItemToSelect;
		TArray<UCustomizableObjectNodeMaterial*> ArrayMaterialNodes;
		if (SelectedCO != nullptr)
		{
			SelectedCO->GetPrivate()->GetSource()->GetNodesOfClass<UCustomizableObjectNodeMaterial>(ArrayMaterialNodes);
		}

		for (const UCustomizableObjectNodeMaterial* MaterialNode : ArrayMaterialNodes)
		{
			if (MaterialNode->GetMaterial())
			{
				NumElementBeforeAdd = ArrayMaterialNodeName.Num();
				ArrayMaterialNodeName.AddUnique(MaterialNode->GetMaterial()->GetName());
				NumElementAfterAdd = ArrayMaterialNodeName.Num();

				if (NumElementBeforeAdd != NumElementAfterAdd)
				{
					ArrayMaterialNodeGuid.Add(MaterialNode->NodeGuid);
				}
			}
		}

		// Adding a none option
		ArrayMaterialNodeOption.Add(MakeShareable(new FString("None")));
		ItemToSelect = ArrayMaterialNodeOption.Last();

		// Initializing the Selected material if it was already selected
		const int32 MaxIndex = ArrayMaterialNodeName.Num();
		for (int32 i = 0; i < MaxIndex; ++i)
		{
			ArrayMaterialNodeOption.Add(MakeShareable(new FString(ArrayMaterialNodeName[i])));

			if (Node->ArrayMaterialNodeToClipWithID.Find(ArrayMaterialNodeGuid[i]) != INDEX_NONE)
			{
				ItemToSelect = ArrayMaterialNodeOption.Last();
			}
		}

        ArrayMaterialNodeOption.Sort(CompareNames);

		// Widget that shows the Tags widget or/and the materials ComboBox
		if (Node->bUseMaterials)
		{
			CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeModifierClipWithMeshDetails", "Blocks"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.15f)
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MaterialsText", "Material:"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.85f)
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextComboBox)
						.OptionsSource(&ArrayMaterialNodeOption)
						.InitiallySelectedItem(ItemToSelect)
						.OnSelectionChanged(this, &FCustomizableObjectNodeModifierClipWithMeshDetails::OnMeshClipWithMeshNodeComboBoxSelectionChanged)
					]
				]
			];
		}
	}
	else
	{
		CustomizableObjectToClipCategory.AddCustomRow(LOCTEXT("Node", "Node"))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Node not found", "Node not found"))
			];
	}
}


void FCustomizableObjectNodeModifierClipWithMeshDetails::ClipperCustomizableObjectSelectionChanged(const FAssetData& AssetData)
{
	UCustomizableObject* NewClipperCO = Cast<UCustomizableObject>(AssetData.GetAsset());

	if (Node != nullptr)
	{
		Node->CustomizableObjectToClipWith = NewClipperCO;
		Node->ArrayMaterialNodeToClipWithID.Empty();
	}

	if (DetailBuilderPtr)
	{
		DetailBuilderPtr->ForceRefreshDetails();
	}
}


TSharedPtr<FString> FCustomizableObjectNodeModifierClipWithMeshDetails::SetInitialClippingMethod()
{
	TSharedPtr<FString> InitialMethod = SelectAMethod;

	if (!Node->bUseMaterials && !Node->bUseTags)
	{
		if (Node->ArrayMaterialNodeToClipWithID.Num() > 0)
		{
			InitialMethod = ClippingMethods[0];
			Node->bUseMaterials = true;
		}

		if (Node->RequiredTags.Num() > 0)
		{
			InitialMethod = ClippingMethods[1];
			Node->bUseTags = true;
		}

		if (Node->bUseMaterials && Node->bUseTags)
		{
			InitialMethod = ClippingMethods[2];
		}
	}
	else
	{
		if (Node->bUseMaterials && !Node->bUseTags)
		{
			InitialMethod = ClippingMethods[0];
		}
		else if (!Node->bUseMaterials && Node->bUseTags)
		{
			InitialMethod = ClippingMethods[1];
		}
		else
		{
			InitialMethod = ClippingMethods[2];
		}
	}

	return InitialMethod;
}


void FCustomizableObjectNodeModifierClipWithMeshDetails::OnClippingMethodComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid())
	{
		if (*Selection == "Tags")
		{
			Node->bUseMaterials = false;
			Node->bUseTags = true;
			Node->ArrayMaterialNodeToClipWithID.Empty();
		}
		else if(*Selection == "Material")
		{
			Node->bUseMaterials = true;
			Node->bUseTags = false;
			Node->RequiredTags.Empty();
		}
		else
		{
			Node->bUseMaterials = true;
			Node->bUseTags = true;
		}

		DetailBuilderPtr->ForceRefreshDetails();
	}	
}


void FCustomizableObjectNodeModifierClipWithMeshDetails::OnMeshClipWithMeshNodeComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && (SelectedCO != nullptr))
	{
		TArray<UCustomizableObjectNodeMaterial*> ArrayMaterialNode;
		SelectedCO->GetPrivate()->GetSource()->GetNodesOfClass<UCustomizableObjectNodeMaterial>(ArrayMaterialNode);

		const FScopedTransaction Transaction(LOCTEXT("ChangedClipMeshWithMeshMaterialTransaction", "Changed Clip Mesh With Mesh Material"));
		Node->Modify();
		Node->ArrayMaterialNodeToClipWithID.Empty();
		for (const UCustomizableObjectNodeMaterial* MaterialNode : ArrayMaterialNode)
		{
			if (MaterialNode && MaterialNode->GetMaterial() && *Selection == MaterialNode->GetMaterial()->GetName())
			{
				Node->ArrayMaterialNodeToClipWithID.Add(MaterialNode->NodeGuid);
			}
		}
	}
}


FReply FCustomizableObjectNodeModifierClipWithMeshDetails::OnAddTagPressed()
{
	if (Node)
	{
		Node->RequiredTags.Add("");
		DetailBuilderPtr->ForceRefreshDetails();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE

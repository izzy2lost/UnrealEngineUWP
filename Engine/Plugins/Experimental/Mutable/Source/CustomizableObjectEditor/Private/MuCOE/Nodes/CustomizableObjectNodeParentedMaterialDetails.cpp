// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterialDetails.h"

#include "DetailLayoutBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailsView.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/STextComboBox.h"

class IPropertyHandle;
class UObject;


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeParentedMaterialDetails::MakeInstance()
{
	return MakeShareable(new FCustomizableObjectNodeParentedMaterialDetails);
}


void FCustomizableObjectNodeParentedMaterialDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	Node = nullptr;
	NodeParentedMaterial = nullptr;

	TSharedPtr<IPropertyHandle> ParentProperty;

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailBuilder.GetDetailsView()->GetSelectedObjects();
	if (SelectedObjects.Num())
	{
		Node = Cast<UCustomizableObjectNode>(SelectedObjects[0].Get());

		if (UCustomizableObjectNodeEditMaterialBase* NodeEditMaterial = Cast<UCustomizableObjectNodeEditMaterialBase>(SelectedObjects[0].Get()))
		{
			NodeParentedMaterial = NodeEditMaterial;
			ParentProperty = DetailBuilder.GetProperty("ParentMaterialObject", UCustomizableObjectNodeEditMaterialBase::StaticClass());
		}
		else if (UCustomizableObjectNodeExtendMaterial* NodeExtendMaterial = Cast<UCustomizableObjectNodeExtendMaterial>(SelectedObjects[0].Get()))
		{
			NodeParentedMaterial = NodeExtendMaterial;
			ParentProperty = DetailBuilder.GetProperty("ParentMaterialObject", UCustomizableObjectNodeExtendMaterial::StaticClass());
		}
	}

	IDetailCategoryBuilder& ParentCategory = DetailBuilder.EditCategory("Parent");

	if (NodeParentedMaterial)
	{
		// Get parent materials
		TArray<UCustomizableObjectNodeMaterialBase*> ParentMaterialBaseNodes = NodeParentedMaterial->GetPossibleParentMaterialNodes();

		ParentMaterialOptionReferences.Empty();
		for (UCustomizableObjectNodeMaterialBase* ParentMaterial : ParentMaterialBaseNodes)
		{
			UCustomizableObject* Object = Cast<UCustomizableObject>(ParentMaterial->GetGraph()->GetOuter());
			FMaterialReference MaterialReference = { Object, ParentMaterial->NodeGuid };

			ParentMaterialOptionReferences.Add(MaterialReference);
		}

		// Get option names
		ParentMaterialOptionNames = GetComboBoxNames(ParentMaterialBaseNodes);
		check(ParentMaterialOptionReferences.Num() == ParentMaterialOptionNames.Num());

		// Sort items (selection sort)
		const int Num = ParentMaterialOptionReferences.Num();
		for (int32 I = 0; I < Num - 1; ++I)
		{
			int32 MinIndex = I;
			for (int32 J = I + 1; J < Num;  J++)
			{
				if (*ParentMaterialOptionNames[I] < *ParentMaterialOptionNames[MinIndex])
				{
					MinIndex = J;
				}
			}

			ParentMaterialOptionNames.Swap(MinIndex, I);
			ParentMaterialOptionReferences.Swap(MinIndex, I);
		}

		// Select item
		TSharedPtr<FString> SelectedItem;
		for (int32 Option = 0; Option < ParentMaterialBaseNodes.Num(); ++Option)
		{
			if (ParentMaterialBaseNodes[Option] == NodeParentedMaterial->GetParentMaterialNode())
			{
				SelectedItem = ParentMaterialOptionNames[Option];
				break;
			}
		}
		
		ParentCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeParentedMaterialDetails", "Blocks"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialText", "Material: "))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, ParentProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
					.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
					.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
					[
						SNew(STextComboBox)
						.OptionsSource(&ParentMaterialOptionNames)
						.InitiallySelectedItem(SelectedItem)
						.OnSelectionChanged(this, &FCustomizableObjectNodeParentedMaterialDetails::OnParentComboBoxSelectionChanged, ParentProperty.ToSharedRef())
					]
				]
			]
		];
	}
	else
	{
		ParentCategory.AddCustomRow(LOCTEXT("FCustomizableObjectNodeEditMaterialBaseDetails", "Node"))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Node not found", "Node not found"))
		];
	}
}


void FCustomizableObjectNodeParentedMaterialDetails::OnParentComboBoxSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo, TSharedRef<IPropertyHandle> ParentProperty)
{
	check(ParentMaterialOptionNames.Num() == ParentMaterialOptionReferences.Num());
	
	for (int32 OptionIndex = 0; OptionIndex < ParentMaterialOptionNames.Num(); ++OptionIndex)
	{
		if (ParentMaterialOptionNames[OptionIndex] == Selection)
		{
			const FScopedTransaction Transaction(LOCTEXT("ChangedParentMaterialTransaction", "Changed Parent Material"));
			Node->Modify();
			NodeParentedMaterial->SetParentNode(ParentMaterialOptionReferences[OptionIndex].Object.Get(), ParentMaterialOptionReferences[OptionIndex].Id);
			break;
		}
	}
}


TArray<TSharedPtr<FString>> FCustomizableObjectNodeParentedMaterialDetails::GetComboBoxNames(const TArray<UCustomizableObjectNodeMaterialBase*>& ParentMaterialNodes) const
{
	TArray<TSharedPtr<FString>> Result;

	for (UCustomizableObjectNodeMaterialBase* ParentMaterialNode : ParentMaterialNodes)
	{
		const FString ObjectName = ParentMaterialNode->GetParentObjectNodes(ParentMaterialNode->GetLOD())[0]->ObjectName; // Since ParentMaterialNode comes from ParentMaterialNodes, it will always have at least a ParentObjectNode.
		FString Name = ObjectName + TEXT(" - ");

		if (UCustomizableObjectNodeCopyMaterial* CopyMaterialNode = Cast<UCustomizableObjectNodeCopyMaterial>(ParentMaterialNode))
		{
			Name += LOCTEXT("CopyMaterialParentHeader", "Copy Material - ").ToString();

			if (UCustomizableObjectNodeMaterial* NodeMaterial = CopyMaterialNode->GetMaterialNode())
			{
				Name += GetComboBoxParentMaterialName(NodeMaterial) + FString(" + ");
			}
			else
			{
				Name += LOCTEXT("MissingParentMaterial", "[Missing Parent Material]").ToString();
			}
			
			const UCustomizableObjectNodeSkeletalMesh* NodeSkeletalMesh = CopyMaterialNode->GetMeshNode();
			if (NodeSkeletalMesh && NodeSkeletalMesh->SkeletalMesh)
			{
				Name += NodeSkeletalMesh->SkeletalMesh->GetName();
			}
			else
			{
				Name += LOCTEXT("MissingSkeletalMesh", "[Missing Skeletal Mesh]").ToString();
			}
		}
		else if (UCustomizableObjectNodeMaterial* MaterialNode = Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
		{
			Name += GetComboBoxParentMaterialName(MaterialNode);
		}
		else
		{
			check(false); // Error. All inputs must have a valid name.
		}

		Result.Add(MakeShareable(new FString(Name)));
	}

	return Result;
}


FString FCustomizableObjectNodeParentedMaterialDetails::GetComboBoxParentMaterialName(const UCustomizableObjectNodeMaterial* ParentMaterial) const
{
	return ParentMaterial->GetMaterial() ? ParentMaterial->GetMaterial()->GetName() : LOCTEXT("MissingMaterial", "[Missing Material]").ToString();
}


#undef LOCTEXT_NAMESPACE

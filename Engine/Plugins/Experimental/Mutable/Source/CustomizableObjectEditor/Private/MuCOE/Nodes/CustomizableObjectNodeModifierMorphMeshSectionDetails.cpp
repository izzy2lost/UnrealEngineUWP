// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSectionDetails.h"

#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierExtendMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Attribute.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeModifierMorphMeshSectionDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeModifierMorphMeshSectionDetails );
}


void FCustomizableObjectNodeModifierMorphMeshSectionDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeModifierBaseDetails::CustomizeDetails(DetailBuilder);

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeModifierMorphMeshSection>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	if (!Node)
	{
		return;
	}

	// This property is not relevant for this node
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierWithMaterial, ReferenceMaterial), UCustomizableObjectNodeModifierWithMaterial::StaticClass());
	
	// Add a morph selection widget.
	IDetailCategoryBuilder& MorphCategory = DetailBuilder.EditCategory( "Morph" );

	// Scan for hint morph names
	{
		TArray<UCustomizableObjectNode*> CandidateNodes;
		Node->GetPossiblyModifiedNodes(CandidateNodes);

		// For now just show the first one
		for (UCustomizableObjectNode* Candidate : CandidateNodes)
		{
			USkeletalMesh* SkeletalMesh = nullptr;

			if (UCustomizableObjectNodeMaterialBase* MaterialNode = Cast<UCustomizableObjectNodeMaterialBase>(Candidate))
			{
				if (MaterialNode->OutputPin())
				{
					const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*MaterialNode->OutputPin(), false);
					if (SourceMeshPin)
					{
						UCustomizableObjectNodeSkeletalMesh* SkeletalNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode());
						if (SkeletalNode)
						{
							SkeletalMesh = SkeletalNode->SkeletalMesh;
						}
					}
				}
			}

			else if (UCustomizableObjectNodeModifierExtendMeshSection* ExtendNode = Cast<UCustomizableObjectNodeModifierExtendMeshSection>(Candidate))
			{
				if (ExtendNode->OutputPin())
				{
					const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*ExtendNode->OutputPin(), false);
					if (SourceMeshPin)
					{
						UCustomizableObjectNodeSkeletalMesh* SkeletalNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode());
						if (SkeletalNode)
						{
							SkeletalMesh = SkeletalNode->SkeletalMesh;
						}
					}
				}
			}

			if (SkeletalMesh)
			{
				const TArray<TObjectPtr<UMorphTarget>>& Morphs = SkeletalMesh->GetMorphTargets();
				for (TObjectPtr<UMorphTarget> Morph : Morphs)
				{
					if (Morph)
					{
						FString MorphTargetName = Morph->GetName();
						MorphOptionsSource.AddUnique(MakeShared<FString>(MorphTargetName));
					}
				}
			}
		}

		// TODO: Add all morphs if no candidate is found? Add both in different menu sections?
	}

	TSharedRef<IPropertyHandle> MorphTargetNameProperty = DetailBuilder.GetProperty("MorphTargetName");
	MorphCategory.AddCustomRow(LOCTEXT("MorphMaterialDetails_Target", "Target"))
		[
			SNew(SProperty, MorphTargetNameProperty)
				.ShouldDisplayName(false)
				.CustomWidget()
				[
					SNew(SBorder)
						.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
						.Padding(FMargin(0.0f, 0.0f, 10.0f, 0.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(10.0f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
									.Text(LOCTEXT("MorphMaterialDetails_MorphTarget", "Morph Target"))
									.Font(IDetailLayoutBuilder::GetDetailFont())
							]

							+ SHorizontalBox::Slot()
							.FillWidth(10.0f)
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Center)
							[
								SAssignNew(this->MorphCombo , SMutableSearchComboBox)
								.ButtonStyle(FAppStyle::Get(), "NoBorder")
								.OptionsSource(&MorphOptionsSource)
								.OnSelectionChanged(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnMorphTargetComboBoxSelectionChanged)
								.Content()
								[
									SNew(SEditableTextBox)
									.Font(IDetailLayoutBuilder::GetDetailFont())
									.Text_Lambda([&]() { return Node ? FText::FromString(Node->MorphTargetName) : FText();})
									.OnTextChanged(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnMorphTargetComboBoxSelectionChanged)
								]
							]
						]
				]
		];
}


void FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnMorphTargetComboBoxSelectionChanged(const FText& NewText)
{
	if (Node && Node->MorphTargetName != NewText.ToString())
	{
		Node->MorphTargetName = NewText.ToString();
		Node->Modify();
	}
}



#undef LOCTEXT_NAMESPACE

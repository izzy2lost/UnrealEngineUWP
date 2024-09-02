// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSectionDetails.h"

#include "MuCOE/CustomizableObjectEditorUtilities.h"
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
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeModifierMorphMeshSection>( DetailsView->GetSelectedObjects()[0].Get() );
	}


	// This property is not relevant for this node
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierWithMaterial, ReferenceMaterial), UCustomizableObjectNodeModifierWithMaterial::StaticClass());
	
	IDetailCategoryBuilder& ModifierCategory = DetailBuilder.EditCategory("Modifier");
	ModifierCategory.SetSortOrder(-10000);

	IDetailCategoryBuilder& BlocksCategory = DetailBuilder.EditCategory( "Customizable Object" );

	if (Node)
	{
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

				else if (UCustomizableObjectNodeModifierExtendMeshSection* ExtendNode = Cast<UCustomizableObjectNodeModifierExtendMeshSection>(Candidate))
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

				if (SkeletalMesh)
				{
					const TArray<TObjectPtr<UMorphTarget>>& Morphs = SkeletalMesh->GetMorphTargets();
					for (TObjectPtr<UMorphTarget> Morph : Morphs)
					{
						FString MorphTargetName = Morph->GetName();
						OptionsSource.AddUnique(MakeShared<FString>(MorphTargetName));
					}
				}
			}

			// TODO: Add all morphs if no candidate is found? Add both in different menu sections?
		}
		FilteredOptionsSource = OptionsSource;

		// Morph target selection
		TSharedRef<IPropertyHandle> MorphTargetNameProperty = DetailBuilder.GetProperty("MorphTargetName");
		BlocksCategory.AddCustomRow(LOCTEXT("MorphMaterialDetails_Target", "Target"))
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
									SAssignNew(this->MorphCombo , SComboButton)
									.ButtonStyle(FAppStyle::Get(), "NoBorder")
									.ButtonContent()
									[
										SNew(SEditableTextBox)
										.Font(IDetailLayoutBuilder::GetDetailFont())
										.Text_Lambda([&]() { return Node ? FText::FromString(Node->MorphTargetName) : FText();})
										.OnTextChanged(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnMorphTargetComboBoxSelectionChanged)
									]
									.MenuContent()
									[
										SNew(SVerticalBox)

											+ SVerticalBox::Slot()
											.AutoHeight()
											[
												SAssignNew(this->SearchField, SEditableTextBox)
													.HintText(LOCTEXT("Search", "Search"))
													.OnTextChanged(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSearchTextChanged)
													.OnTextCommitted(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSearchTextCommitted)
											]

											+ SVerticalBox::Slot()
											[
												SAssignNew(this->ComboListView, SListView< TSharedPtr<FString> >)
													.ListItemsSource(&FilteredOptionsSource)
													.OnGenerateRow(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::GenerateMenuItemRow)
													.OnSelectionChanged(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSelectionChanged)
													.OnKeyDownHandler(this, &FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnKeyDownHandler)
													.SelectionMode(ESelectionMode::Single)
											]
									]
								]
							]
					]
			];
	}
	else
	{
		BlocksCategory.AddCustomRow( LOCTEXT("MorphMaterialDetails_Node", "Node") )
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "MorphMaterialDetails_NodeNotFound", "Node not found" ) )
		];
	}
}


void FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSearchTextChanged(const FText& ChangedText)
{
	SearchText = ChangedText;

	RefreshOptions();
}

void FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if ((InCommitType == ETextCommit::Type::OnEnter) && FilteredOptionsSource.Num() > 0)
	{
		ComboListView->SetSelection(FilteredOptionsSource[0], ESelectInfo::OnKeyPress);
	}
}


void FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnMorphTargetComboBoxSelectionChanged(const FText& NewText)
{	
	if (Node && Node->MorphTargetName!=NewText.ToString())
	{
		Node->MorphTargetName = NewText.ToString();
		Node->Modify();
	}
}

void FCustomizableObjectNodeModifierMorphMeshSectionDetails::RefreshOptions()
{
	// Need to refresh filtered list whenever options change
	FilteredOptionsSource.Reset();

	if (SearchText.IsEmpty())
	{
		FilteredOptionsSource.Append(OptionsSource);
	}
	else
	{
		TArray<FString> SearchTokens;
		SearchText.ToString().ParseIntoArrayWS(SearchTokens);

		for (const TSharedPtr<FString>& Option : OptionsSource)
		{
			bool bAllTokensMatch = true;
			for (const FString& SearchToken : SearchTokens)
			{
				if (Option->Find(SearchToken, ESearchCase::Type::IgnoreCase) == INDEX_NONE)
				{
					bAllTokensMatch = false;
					break;
				}
			}

			if (bAllTokensMatch)
			{
				FilteredOptionsSource.Add(Option);
			}
		}
	}

	ComboListView->RequestListRefresh();
}


TSharedRef<ITableRow> FCustomizableObjectNodeModifierMorphMeshSectionDetails::GenerateMenuItemRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SComboRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(*InItem))
		];
}


void FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnSelectionChanged(TSharedPtr<FString> ProposedSelection, ESelectInfo::Type SelectInfo)
{
	if (!ProposedSelection || !Node)
	{
		return;
	}

	// Ensure that the proposed selection is different from selected
	if (*ProposedSelection != Node->MorphTargetName)
	{
		Node->MorphTargetName = *ProposedSelection;
	}

	// close combo as long as the selection wasn't from navigation
	if (SelectInfo != ESelectInfo::OnNavigation)
	{
		MorphCombo->SetIsOpen(false);
	}
	else
	{
		ComboListView->RequestScrollIntoView(ProposedSelection, 0);
	}
}


FReply FCustomizableObjectNodeModifierMorphMeshSectionDetails::OnKeyDownHandler(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		// Select the first selected item on hitting enter
		TArray<TSharedPtr<FString>> SelectedItems = ComboListView->GetSelectedItems();
		if (SelectedItems.Num() > 0)
		{
			OnSelectionChanged(SelectedItems[0], ESelectInfo::OnKeyPress);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE

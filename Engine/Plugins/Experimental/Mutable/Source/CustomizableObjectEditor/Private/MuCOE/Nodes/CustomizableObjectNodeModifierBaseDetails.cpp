// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierBaseDetails.h"

#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "IDetailsView.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Attribute.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectDetails"


TSharedRef<IDetailCustomization> FCustomizableObjectNodeModifierBaseDetails::MakeInstance()
{
	return MakeShareable( new FCustomizableObjectNodeModifierBaseDetails );
}


void FCustomizableObjectNodeModifierBaseDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	FCustomizableObjectNodeDetails::CustomizeDetails(DetailBuilder);

	const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
	if ( DetailsView->GetSelectedObjects().Num() )
	{
		Node = Cast<UCustomizableObjectNodeModifierBase>( DetailsView->GetSelectedObjects()[0].Get() );
	}

	if (!Node)
	{
		return;
	}
	
	// Move modifier conditions to the top.
	IDetailCategoryBuilder& ModifierCategory = DetailBuilder.EditCategory("Modifier");
	ModifierCategory.SetSortOrder(-10000);

	// Add the required tags widget
	{
		RefreshTagOptions();

		RequiredTagsPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierBase, RequiredTags), UCustomizableObjectNodeModifierBase::StaticClass());
		DetailBuilder.HideProperty(RequiredTagsPropertyHandle);

		RequiredTagsPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged));
		RequiredTagsPropertyHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged));

		TagsPolicyPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierBase, MultipleTagPolicy), UCustomizableObjectNodeModifierBase::StaticClass());
		TagsPolicyPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged));

		ModifierCategory.AddCustomRow(FText::FromString(TEXT("Required Tags")))
			.PropertyHandleList({ RequiredTagsPropertyHandle })
			.NameContent()
				.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Top)
					.Padding(FMargin(0,4.0f,0,4.0f))
					[
						SNew(STextBlock)
							.Text(LOCTEXT("tableodifierDetails_RequiredTags", "Required Tags"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
					]
			]
			.ValueContent()
				.HAlign(HAlign_Fill)
			[
				SNew(SVerticalBox)

					// Header with the "add tag" UI
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(10.0f)
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Center)
						[
							SAssignNew(this->TagCombo, SMutableSearchComboBox)
								.ButtonStyle(FAppStyle::Get(), "NoBorder")
								.MenuButtonBrush(FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
								.OptionsSource(&TagComboOptionsSource)
								.AllowAddNewOptions(true)
								.OnSelectionChanged(this, &FCustomizableObjectNodeModifierBaseDetails::OnTagComboBoxSelectionChanged)
						]
					]

					// List of tags
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(this->TagList, SListView<TSharedPtr<FTagUIData>>)
							.ListItemsSource(&CurrentTagsSource)
							.OnGenerateRow(this, &FCustomizableObjectNodeModifierBaseDetails::GenerateTagMenuItemRow)
							.SelectionMode(ESelectionMode::None)							
					]

					// No-tags warning: shown only if there are no required tags defined
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("ModifierDetails_NoRequiredTagsWarning", "Warning: There are no required tags, so this modifier will not do anything."))
							.AutoWrapText(true)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Visibility_Lambda([&]() 
								{
									return (!Node || Node->RequiredTags.IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
								})
					]
			];

	}
}


void FCustomizableObjectNodeModifierBaseDetails::OnRequiredTagsPropertyChanged()
{
	// This seems necessary to detect the "Reset to default" actions.
	RefreshTagOptions();
}


void FCustomizableObjectNodeModifierBaseDetails::RefreshTagOptions()
{
	// Tag combo options
	{
		TagComboOptionsSource.SetNum(0,EAllowShrinking::No);

		// Scan all potential receivers
		UCustomizableObject* ThisNodeObject = GetRootObject(*Node);
		UCustomizableObject* RootObject = GetRootObject(ThisNodeObject);

		TSet<UCustomizableObject*> AllCustomizableObject;
		GetAllObjectsInGraph(RootObject, AllCustomizableObject);

		for (const UCustomizableObject* CustObject : AllCustomizableObject)
		{
			if (CustObject)
			{
				for (const TObjectPtr<UEdGraphNode>& CandidateNode : CustObject->GetPrivate()->GetSource()->Nodes)
				{
					UCustomizableObjectNode* Typed = Cast<UCustomizableObjectNode>(CandidateNode);
					if (Typed)
					{
						TArray<FString>* EnableTags = Typed->GetEnableTags();
						if (EnableTags)
						{
							for (const FString& Tag : *EnableTags)
							{
								if (!Tag.IsEmpty())
								{
									TagComboOptionsSource.AddUnique(MakeShared<FString>(Tag));
								}
							}
						}
					}
				}
			}
		}

		// TODO: add material nodes and options to create tags for them, show hierarchy, ...
	}

	// Current Tags
	if (Node)
	{
		CurrentTagsSource.SetNum(0, EAllowShrinking::No);

		for (const FString& Tag : Node->RequiredTags)
		{
			TSharedPtr<FTagUIData> Data = MakeShared<FTagUIData>();
			Data->DisplayName = Tag;
			Data->Tag = Tag;
			CurrentTagsSource.Add(Data);
		}
	}

	if (TagList)
	{
		TagList->RequestListRefresh();
	}
}


TSharedRef<ITableRow> FCustomizableObjectNodeModifierBaseDetails::GenerateTagMenuItemRow(TSharedPtr<FTagUIData> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!InItem)
	{
		return SNew(SComboRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew(STextBlock)
					.Text(FText::FromString("No item."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}

	return SNew(SComboRow<TSharedPtr<FString>>, OwnerTable)
		[
			SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(InItem->DisplayName))
						.Font(IDetailLayoutBuilder::GetDetailFont())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "NoBorder")
						.ToolTipText(LOCTEXT("RemoveModifierTag","Remove this tag from the modifier."))
						.OnClicked_Lambda([this,InItem]() 
							{
								if (Node && InItem )
								{
									if (Node->RequiredTags.Contains(InItem->Tag))
									{
										Node->RequiredTags.Remove(InItem->Tag);
										Node->Modify();
										OnRequiredTagsPropertyChanged();
									}
								}
								return FReply::Handled();
							})
						[
							SNew(SImage)
								.Image(FAppStyle::GetBrush(TEXT("Icons.MinusCircle")))
						]
				]
		];
}


void FCustomizableObjectNodeModifierBaseDetails::OnTagComboBoxSelectionChanged(const FText& NewText)
{
	FString Tag = NewText.ToString();
	if (Node && !Node->RequiredTags.Contains(Tag))
	{
		Node->RequiredTags.Add(Tag);
		Node->Modify();
		OnRequiredTagsPropertyChanged();
	}
}


#undef LOCTEXT_NAMESPACE

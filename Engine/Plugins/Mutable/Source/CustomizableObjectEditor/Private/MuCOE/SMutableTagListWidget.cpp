// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableTagListWidget.h"

#include "MuCOE/CustomizableObjectEditorUtilities.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierBase.h"
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



void SMutableTagListWidget::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;
	TagArray = InArgs._TagArray;
	EmptyListText = InArgs._EmptyListText;

	OnTagListChangedDelegate = InArgs._OnTagListChanged;

	RefreshOptions();

	ChildSlot
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
						.OnSelectionChanged(this, &SMutableTagListWidget::OnTagComboBoxSelectionChanged)
				]
		]

		// List of tags
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(this->TagListWidget, SListView<TSharedPtr<FTagUIData>>)
				.ListItemsSource(&CurrentTagsSource)
				.OnGenerateRow(this, &SMutableTagListWidget::GenerateTagMenuItemRow)
				.SelectionMode(ESelectionMode::None)
		]

		// Shown only if there are no tags defined
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			SNew(STextBlock)
				.Text(EmptyListText)
				.AutoWrapText(true)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Visibility_Lambda([&]()
					{
						return (!TagArray || TagArray->IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
					})
		]
	];
}


void SMutableTagListWidget::RefreshOptions()
{
	// Tag combo options
	{
		TagComboOptionsSource.SetNum(0, EAllowShrinking::No);

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
							for (const FString& OneTag : *EnableTags)
							{
								if (!OneTag.IsEmpty())
								{
									bool bContained = TagComboOptionsSource.ContainsByPredicate([&](const TSharedPtr<FString>& Candidate) 
										{
											return Candidate.IsValid() && *Candidate == OneTag;
										});
									if (!bContained)
									{
										TagComboOptionsSource.Add(MakeShared<FString>(OneTag));
									}
								}
							}
						}
					}

					UCustomizableObjectNodeModifierBase* TypedModifier = Cast<UCustomizableObjectNodeModifierBase>(CandidateNode);
					if (TypedModifier)
					{
						for (const FString& OneTag : TypedModifier->RequiredTags)
						{
							if (!OneTag.IsEmpty())
							{
								bool bContained = TagComboOptionsSource.ContainsByPredicate([&](const TSharedPtr<FString>& Candidate)
									{
										return Candidate.IsValid() && *Candidate == OneTag;
									});
								if (!bContained)
								{
									TagComboOptionsSource.Add(MakeShared<FString>(OneTag));
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
	if (TagArray)
	{
		CurrentTagsSource.SetNum(0, EAllowShrinking::No);

		for (const FString& OneTag : *TagArray)
		{
			TSharedPtr<FTagUIData> Data = MakeShared<FTagUIData>();
			Data->DisplayName = OneTag;
			Data->Tag = OneTag;
			CurrentTagsSource.Add(Data);
		}
	}

	if (TagListWidget)
	{
		TagListWidget->RequestListRefresh();
	}
}


void SMutableTagListWidget::OnTagComboBoxSelectionChanged(const FText& NewText)
{
	FString OneTag = NewText.ToString();
	if (TagArray && !TagArray->Contains(OneTag))
	{
		TagArray->Add(OneTag);
		OnTagListChangedDelegate.ExecuteIfBound();
	}
}


TSharedRef<ITableRow> SMutableTagListWidget::GenerateTagMenuItemRow(TSharedPtr<FTagUIData> InItem, const TSharedRef<STableViewBase>& OwnerTable)
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
						.ToolTipText(LOCTEXT("RemoveModifierTag", "Remove this tag from the modifier."))
						.OnClicked_Lambda([this, InItem]()
							{
								if (Node && InItem)
								{
									if (TagArray->Contains(InItem->Tag))
									{
										TagArray->Remove(InItem->Tag);
										OnTagListChangedDelegate.ExecuteIfBound();
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



#undef LOCTEXT_NAMESPACE

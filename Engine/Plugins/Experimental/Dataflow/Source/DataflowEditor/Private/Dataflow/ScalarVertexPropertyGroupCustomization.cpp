// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScalarVertexPropertyGroupCustomization.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "ScalarVertexPropertyGroupCustomization"

namespace Dataflow
{
	TSharedRef<IPropertyTypeCustomization> FScalarVertexPropertyGroupCustomization::MakeInstance()
	{
		return MakeShareable(new FScalarVertexPropertyGroupCustomization);
	}

	void FScalarVertexPropertyGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		uint32 NumChildren;
		const FPropertyAccess::Result Result = PropertyHandle->GetNumChildren(NumChildren);

		ChildPropertyHandle = (Result == FPropertyAccess::Success && NumChildren) ? PropertyHandle->GetChildHandle(0) : nullptr;

		GroupNames.Reset();

		HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget(PropertyHandle->GetPropertyDisplayName())
			]
			.ValueContent()
			.MinDesiredWidth(250)
			.MaxDesiredWidth(350.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MaxWidth(145.f)
				[
					SAssignNew(ComboButton, SComboButton)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.OnGetMenuContent(this, &FScalarVertexPropertyGroupCustomization::OnGetMenuContent)
					.ButtonContent()
					[
						SNew(SEditableTextBox)
						.Text_Raw(this, &FScalarVertexPropertyGroupCustomization::GetText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &FScalarVertexPropertyGroupCustomization::OnTextCommitted)
					]
				]
			];
	}

	FText FScalarVertexPropertyGroupCustomization::GetText() const
	{
		FText Text;
		if (ChildPropertyHandle)
		{
			ChildPropertyHandle->GetValueAsFormattedText(Text);
		}
		return Text;
	}

	void FScalarVertexPropertyGroupCustomization::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		if (ChildPropertyHandle)
		{
			FText CurrentText;
			ChildPropertyHandle->GetValueAsFormattedText(CurrentText);

			if (!NewText.ToString().Equals(CurrentText.ToString(), ESearchCase::CaseSensitive))
			{
				FString String = NewText.ToString();
				//FClothDataflowTools::MakeCollectionName(String);
				ChildPropertyHandle->SetValueFromFormattedString(String);
			}
		}
	}

	void FScalarVertexPropertyGroupCustomization::OnSelectionChanged(TSharedPtr<FText> ItemSelected, ESelectInfo::Type /*SelectInfo*/)
	{
		if (ChildPropertyHandle)
		{
			// Set the child property's value
			if (ItemSelected)
			{
				FText CurrentText;
				ChildPropertyHandle->GetValueAsFormattedText(CurrentText);

				if (!ItemSelected->EqualTo(CurrentText))
				{
					ChildPropertyHandle->SetValueFromFormattedString(ItemSelected->ToString());
				}

				if (TSharedPtr<SComboButton> PinnedComboButton = ComboButton.Pin())
				{
					PinnedComboButton->SetIsOpen(false);
				}
			}
		}
	}

	TSharedRef<ITableRow> FScalarVertexPropertyGroupCustomization::MakeCategoryViewWidget(TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable)
	{
		if (Item)
		{
			return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew(STextBlock).Text(*Item)
			];
		}
		else
		{
			return SNew(STableRow<TSharedPtr<FString>>, OwnerTable);
		}
	}

	TSharedRef<SWidget> FScalarVertexPropertyGroupCustomization::OnGetMenuContent()
	{
		GroupNames.Reset();

		// Find all group names in the parent node's collection
		FName NodeType;
		TArray<FName> CollectionGroupNames;
		if (const FDataflowCollectionAddScalarVertexPropertyNode* const ScalarVertexPropertyNode = GetOwnerStruct<FDataflowCollectionAddScalarVertexPropertyNode>())
		{
			CollectionGroupNames = ScalarVertexPropertyNode->GetCachedCollectionGroupNames();
			NodeType = ScalarVertexPropertyNode->GetType();

			const TArray<FName> AvailableTargetGroups = DataflowAddScalarVertexPropertyCallbackRegistry::Get().GetTargetGroupNames();

			CollectionGroupNames = CollectionGroupNames.FilterByPredicate([&AvailableTargetGroups](const FName& Name)
				{
					return AvailableTargetGroups.Contains(Name);
				});

			for (const FName& CollectionGroupName : CollectionGroupNames)
			{
				GroupNames.Add(MakeShareable(new FText(FText::FromName(CollectionGroupName))));
			}
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SNew(SListView<TSharedPtr<FText>>)
					.ListItemsSource(&GroupNames)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &FScalarVertexPropertyGroupCustomization::MakeCategoryViewWidget)
					.OnSelectionChanged(this, &FScalarVertexPropertyGroupCustomization::OnSelectionChanged)
			];
	}

	template<typename T>
	T* FScalarVertexPropertyGroupCustomization::GetOwnerStruct() const
	{
		if (ChildPropertyHandle)
		{
			if (const TSharedPtr<IPropertyHandle> PropertyHandle = ChildPropertyHandle->GetParentHandle())
			{
				if (const TSharedPtr<IPropertyHandle> OwnerHandle = PropertyHandle->GetParentHandle())  // Assume that the group struct is only used at the node struct level
				{
					if (const TSharedPtr<IPropertyHandleStruct> OwnerHandleStruct = OwnerHandle->AsStruct())
					{
						if (TSharedPtr<FStructOnScope> StructOnScope = OwnerHandleStruct->GetStructData())
						{
							if (StructOnScope->GetStruct() == T::StaticStruct())
							{
								return reinterpret_cast<T*>(StructOnScope->GetStructMemory());
							}
						}
					}
				}
			}
		}
		return nullptr;
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE

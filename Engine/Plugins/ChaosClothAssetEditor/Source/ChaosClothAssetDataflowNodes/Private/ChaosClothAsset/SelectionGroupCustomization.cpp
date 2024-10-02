// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SelectionGroupCustomization.h"
#include "ChaosClothAsset/AttributeNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"  // For MakeCollectionName
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/DeleteElementNode.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DataflowNodeSelectionGroupCustomization"

namespace UE::Chaos::ClothAsset
{
	namespace Private
	{
		// Return the FManagedArrayCollection with the specified name from the property held by the top level struct owner of ChildPropertyHandle
		FManagedArrayCollection GetPropertyCollection(
			UE::Dataflow::FContext& Context,
			const TSharedPtr<IPropertyHandle>& ChildPropertyHandle,
			const FName CollectionPropertyName = FName(TEXT("Collection")))
		{
			static const FManagedArrayCollection EmptyCollection;

			TSharedPtr<IPropertyHandle> OwnerHandle = ChildPropertyHandle;
			while (TSharedPtr<IPropertyHandle> ParentHandle = OwnerHandle->GetParentHandle())
			{
				OwnerHandle = MoveTemp(ParentHandle);
			}
			if (const TSharedPtr<IPropertyHandleStruct> OwnerHandleStruct = OwnerHandle->AsStruct())
			{
				if (const TSharedPtr<FStructOnScope> StructOnScope = OwnerHandleStruct->GetStructData())
				{
					if (const UStruct* const Struct = StructOnScope->GetStruct())
					{
						if (Struct->IsChildOf<FDataflowNode>())
						{
							const FDataflowNode* const DataflowNode = reinterpret_cast<FDataflowNode*>(StructOnScope->GetStructMemory());

							if (const FProperty* const Property = Struct->FindPropertyByName(CollectionPropertyName))
							{
								if (const FStructProperty* const StructProperty = CastField<FStructProperty>(Property))
								{
									if (StructProperty->GetCPPType(nullptr, CPPF_None) == TEXT("FManagedArrayCollection"))
									{
										if (const FDataflowInput* const DataflowInput = DataflowNode->FindInput(StructProperty->ContainerPtrToValuePtr<FManagedArrayCollection*>(DataflowNode)))
										{
											return DataflowInput->GetValue(Context, EmptyCollection);
										}
									}
								}
							}
						}
					}
				}
			}
			return EmptyCollection;
		}
	}  // End namespace Private

	TSharedRef<IPropertyTypeCustomization> FSelectionGroupCustomization::MakeInstance()
	{
		return MakeShareable(new FSelectionGroupCustomization);
	}

	void FSelectionGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		DataflowGraphEditor = SDataflowGraphEditor::GetSelectedGraphEditor();

		uint32 NumChildren;
		const FPropertyAccess::Result Result = PropertyHandle->GetNumChildren(NumChildren);

		ChildPropertyHandle = (Result == FPropertyAccess::Success && NumChildren) ? PropertyHandle->GetChildHandle(0) : TSharedPtr<IPropertyHandle>();

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
					.OnGetMenuContent(this, &FSelectionGroupCustomization::OnGetMenuContent)
					.ButtonContent()
					[
						SNew(SEditableTextBox)
						.Text_Raw(this, &FSelectionGroupCustomization::GetText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &FSelectionGroupCustomization::OnTextCommitted)
					]
				]
			];
	}

	FText FSelectionGroupCustomization::GetText() const
	{
		FText Text;
		ChildPropertyHandle->GetValueAsFormattedText(Text);
		return Text;
	}

	void FSelectionGroupCustomization::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		FText CurrentText;
		ChildPropertyHandle->GetValueAsFormattedText(CurrentText);

		if (!NewText.ToString().Equals(CurrentText.ToString(), ESearchCase::CaseSensitive))
		{
			FString String = NewText.ToString();
			FClothDataflowTools::MakeCollectionName(String);
			ChildPropertyHandle->SetValueFromFormattedString(String);
		}
	}

	void FSelectionGroupCustomization::OnSelectionChanged(TSharedPtr<FText> ItemSelected, ESelectInfo::Type /*SelectInfo*/)
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

			ComboButton->SetIsOpen(false);
		}
	}

	TSharedRef<ITableRow> FSelectionGroupCustomization::MakeCategoryViewWidget(TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew(STextBlock).Text(*Item.Get())
			];
	}

	TSharedRef<SWidget> FSelectionGroupCustomization::OnGetMenuContent()
	{
		// Retrieve context if any, or use an empty context
		const TSharedPtr<const SDataflowGraphEditor> DataflowGraphEditorPtr = DataflowGraphEditor.Pin();
		const TSharedPtr<UE::Dataflow::FContext> Context = DataflowGraphEditorPtr ? DataflowGraphEditorPtr->GetDataflowContext() : TSharedPtr<UE::Dataflow::FContext>();

		// Find all group names in the parent selection node's collection
		GroupNames.Reset();
		UE::Dataflow::FContextThreaded EmptyContext;
		FManagedArrayCollection Collection = Private::GetPropertyCollection(Context.IsValid() ? *Context : EmptyContext, ChildPropertyHandle);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(Collection));
		FCollectionClothFacade Cloth(ClothCollection);
		for (const FName& GroupName : ClothCollection->GroupNames())
		{
			if (Cloth.IsValidClothCollectionGroupName(GroupName))  // Restrict to the cloth facade groups
			{
				GroupNames.Add(MakeShareable(new FText(FText::FromName(GroupName))));
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
				.OnGenerateRow(this, &FSelectionGroupCustomization::MakeCategoryViewWidget)
				.OnSelectionChanged(this, &FSelectionGroupCustomization::OnSelectionChanged)
			];
	}

	template<typename T>
	T* FSelectionGroupCustomization::GetOwnerStruct() const
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
		return nullptr;
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE

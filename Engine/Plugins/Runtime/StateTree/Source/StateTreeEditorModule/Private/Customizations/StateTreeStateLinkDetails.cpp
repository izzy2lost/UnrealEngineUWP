// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeStateLinkDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegates.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "StateTreeEditorStyle.h"
#include "StateTreePropertyHelpers.h"
#include "TextStyleDecorator.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

TMap<FObjectKey, FStateTreeStateLinkDetails::FStateExpansionState> FStateTreeStateLinkDetails::StateExpansionStates;


TSharedRef<IPropertyTypeCustomization> FStateTreeStateLinkDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeStateLinkDetails);
}

void FStateTreeStateLinkDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	NameProperty = StructProperty->GetChildHandle(TEXT("Name"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));
	LinkTypeProperty = StructProperty->GetChildHandle(TEXT("LinkType"));

	if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
	{
		static const FName NAME_DirectStatesOnly = "DirectStatesOnly";
		static const FName NAME_SubtreesOnly = "SubtreesOnly";
		
		bDirectStatesOnly = MetaDataProperty->HasMetaData(NAME_DirectStatesOnly);
		bSubtreesOnly = MetaDataProperty->HasMetaData(NAME_SubtreesOnly);
	}
	
	CacheStates();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboButton, SComboButton)
			.OnGetMenuContent(this, &FStateTreeStateLinkDetails::GenerateStatePicker)
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0,0,4,0)
				[
					SNew(SImage)
					.ToolTipText(LOCTEXT("MissingState", "The specified state cannot be found."))
					.Visibility_Lambda([this]()
					{
						return IsValidLink() ? EVisibility::Collapsed : EVisibility::Visible;
					})
					.Image(FAppStyle::GetBrush("Icons.ErrorWithColor"))
				]

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0, 2.0f, 4.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
					.Image(this, &FStateTreeStateLinkDetails::GetCurrentStateIcon)
					.ColorAndOpacity(this, &FStateTreeStateLinkDetails::GetCurrentStateColor)
				]
				
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SRichTextBlock)
					.Text(this, &FStateTreeStateLinkDetails::GetCurrentStateDesc)
					.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal"))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Normal")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Bold")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Italic")))
					+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Details.Subdued")))
				]
			]
		];

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeStateLinkDetails::OnIdentifierChanged);
}

void FStateTreeStateLinkDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FStateTreeStateLinkDetails::OnIdentifierChanged(const UStateTree& StateTree)
{
	CacheStates();
}

void FStateTreeStateLinkDetails::CacheStates(TSharedPtr<FStateTreeStateItem> ParentNode, const UStateTreeState* State)
{
	if (State == nullptr)
	{
		return;
	}

	bool bShouldAdd = true;
	if (bSubtreesOnly && State->Type != EStateTreeStateType::Subtree)
	{
		bShouldAdd = false;
	}

	if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
	{
		bShouldAdd = false;
	}
	
	if (bShouldAdd)
	{
		TSharedRef<FStateTreeStateItem> StateItem = MakeShared<FStateTreeStateItem>();
		StateItem->Desc = FText::FromName(State->Name);
		StateItem->TransitionType = EStateTreeTransitionType::GotoState;
		StateItem->StateID = State->ID;
		StateItem->Color = FLinearColor(1.f, 1.f, 1.f, 0.25f);
		StateItem->bIsSubTree = State->Type == EStateTreeStateType::Subtree;

		// Figure out icon.
		if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
		{
			StateItem->Icon = FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.SelectNone");
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryEnterState)
		{
			StateItem->Icon = FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");			
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder)
		{
			if (State->Children.IsEmpty()
				|| State->Type == EStateTreeStateType::Linked
				|| State->Type == EStateTreeStateType::LinkedAsset)
			{
				StateItem->Icon = FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryEnterState");			
			}
			else
			{
				StateItem->Icon = FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TrySelectChildrenInOrder");
			}
		}
		else if (State->SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions)
		{
			StateItem->Icon = FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.TryFollowTransitions");
		}

		// Linked states
		if (State->Type == EStateTreeStateType::Linked)
		{
			StateItem->bIsLinked = true;
			StateItem->LinkedDesc = FText::FromName(State->LinkedSubtree.Name);
		}
		else if (State->Type == EStateTreeStateType::LinkedAsset)
		{
			StateItem->bIsLinked = true;
			StateItem->LinkedDesc = FText::FromString(GetNameSafe(State->LinkedAsset.Get()));
		}
		
		ParentNode->Children.Add(StateItem);

		ParentNode = StateItem;
	}

	for (UStateTreeState* ChildState : State->Children)
	{
		CacheStates(ParentNode, ChildState);
	}
}

void FStateTreeStateLinkDetails::CacheStates()
{
	RootItem = MakeShared<FStateTreeStateItem>();

	if (!bDirectStatesOnly)
	{
		RootItem->Children.Add(MakeShared<FStateTreeStateItem>(
			LOCTEXT("TransitionNoneRich", "<i>None</>"),
			LOCTEXT("TransitionNoneTooltip", "No transition."),
			FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.None"),
			EStateTreeTransitionType::None));

		RootItem->Children.Add(MakeShared<FStateTreeStateItem>(
			LOCTEXT("TransitionNextStateRich", "<i>Next State</>"),
			LOCTEXT("TransitionNextTooltip", "Goto next sibling State."),
			FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Next"),
			EStateTreeTransitionType::NextState));

		RootItem->Children.Add(MakeShared<FStateTreeStateItem>(
			LOCTEXT("TransitionNextSelectableStateRich", "<i>Next Selectable State</>"),
			LOCTEXT("TransitionNextSelectableTooltip", "Goto next sibling state, whose enter conditions pass."),
			FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Next"),
			EStateTreeTransitionType::NextSelectableState));

		RootItem->Children.Add(MakeShared<FStateTreeStateItem>(
			LOCTEXT("TransitionTreeSucceededRich", "<i>Tree Succeeded</>"),
			LOCTEXT("TransitionTreeSuccessTooltip", "Complete tree with success."),
			FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Succeeded"),
			EStateTreeTransitionType::Succeeded));

		RootItem->Children.Add(MakeShared<FStateTreeStateItem>(
			LOCTEXT("TransitionTreeFailedRich", "<i>Tree Failed</>"),
			LOCTEXT("TransitionTreeFailedTooltip", "Complete tree with failure."),
			FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.Transition.Failed"),
			EStateTreeTransitionType::Failed));
	}
	
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < OuterObjects.Num(); ObjectIdx++)
	{
		if (const UStateTree* OuterStateTree = OuterObjects[ObjectIdx]->GetTypedOuter<UStateTree>())
		{
			if (const UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(OuterStateTree->EditorData))
			{
				WeakStateTree = OuterStateTree;
				for (const UStateTreeState* SubTree : TreeData->SubTrees)
				{
					CacheStates(RootItem, SubTree);
				}
			}
			break;
		}
	}

	FilteredRootItem = RootItem;
}

TSharedRef<SWidget> FStateTreeStateLinkDetails::GenerateStatePicker()
{
	check(ComboButton);
	
	CacheStates();

	TSharedRef<SWidget> MenuWidget = 
		SNew(SBox)
		.MinDesiredWidth(300.f)
		.MaxDesiredHeight(400.f)
		.Padding(2)	
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(4, 2, 4, 2)
			.AutoHeight()
			[
				SAssignNew(SearchBox, SSearchBox)
				.OnTextChanged(this, &FStateTreeStateLinkDetails::OnSearchBoxTextChanged)
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(StateItemTree, STreeView<TSharedPtr<FStateTreeStateItem>>)
				.SelectionMode(ESelectionMode::Single)
				.ItemHeight(20.0f)
				.TreeItemsSource(&FilteredRootItem->Children)
				.OnGenerateRow(this, &FStateTreeStateLinkDetails::GenerateStateItemRow)
				.OnGetChildren(this, &FStateTreeStateLinkDetails::GetStateItemChildren)
				.OnSelectionChanged(this, &FStateTreeStateLinkDetails::OnStateItemSelected)
				.OnExpansionChanged(this, &FStateTreeStateLinkDetails::OnStateItemExpansionChanged)
			]
		];

	// Restore category expansion state from previous use.
	RestoreExpansionState();
	
	// Expand and select currently selected item.
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	if (GetCurrentStateItem(Path))
	{
		// Expand all categories up to the selected item.
		bIsRestoringExpansion = true;
		for (int32 Index = 0; Index < Path.Num() - 1; Index++)
		{
			StateItemTree->SetItemExpansion(Path[Index], true);
		}
		bIsRestoringExpansion = false;
		
		StateItemTree->SetItemSelection(Path.Last(), true);
		StateItemTree->RequestScrollIntoView(Path.Last());
	}
	
	ComboButton->SetMenuContentWidgetToFocus(SearchBox);

	return MenuWidget;
}

TSharedRef<ITableRow> FStateTreeStateLinkDetails::GenerateStateItemRow(TSharedPtr<FStateTreeStateItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SHorizontalBox> Container = SNew(SHorizontalBox);

	// Icon
	Container->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0, 2.0f, 4.0f, 2.0f)
		.AutoWidth()
		[
			SNew(SImage)
			.Visibility(Item->Icon ? EVisibility::Visible : EVisibility::Collapsed)
			.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
			.Image(Item->Icon)
			.ColorAndOpacity(Item->Color)
		];

	// Name
	Container->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SRichTextBlock)
			.Text(Item->Desc)
			.TextStyle(&FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal"))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ToolTipText(Item->TooltipText)
			.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
			+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT(""), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Normal")))
			+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("b"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Bold")))
			+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("i"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Italic")))
			+SRichTextBlock::Decorator(FTextStyleDecorator::Create(TEXT("s"), FStateTreeEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("Normal.Subdued")))
		];

	// Link
	if (Item->bIsLinked)
	{
		// Link icon
		Container->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(FStateTreeEditorStyle::Get().GetBrush("StateTreeEditor.StateLinked"))
		];

		// Linked name
		Container->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(Item->LinkedDesc)
			];
	}
	
	return SNew(STableRow<TSharedPtr<FStateTreeStateItem>>, OwnerTable)
		[
			Container
		];
}

void FStateTreeStateLinkDetails::GetStateItemChildren(TSharedPtr<FStateTreeStateItem> Item, TArray<TSharedPtr<FStateTreeStateItem>>& OutItems) const
{
	if (Item.IsValid())
	{
		OutItems = Item->Children;
	}
}

void FStateTreeStateLinkDetails::OnStateItemSelected(TSharedPtr<FStateTreeStateItem> SelectedItem, ESelectInfo::Type Type)
{
	// Skip selection from code
	if (Type == ESelectInfo::Direct)
	{
		return;
	}
	
	if (SelectedItem
		&& NameProperty
		&& IDProperty)
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), StructProperty->GetPropertyDisplayName()));

		LinkTypeProperty->SetValue((uint8)SelectedItem->TransitionType);

		if (SelectedItem->TransitionType == EStateTreeTransitionType::GotoState)
		{
			NameProperty->SetValue(FName(SelectedItem->Desc.ToString()), EPropertyValueSetFlags::NotTransactable);
			UE::StateTree::PropertyHelpers::SetStructValue<FGuid>(IDProperty, SelectedItem->StateID, EPropertyValueSetFlags::NotTransactable);
		}
		else
		{
			// Clear name and id.
			NameProperty->SetValue(FName(), EPropertyValueSetFlags::NotTransactable);
			UE::StateTree::PropertyHelpers::SetStructValue<FGuid>(IDProperty, FGuid(), EPropertyValueSetFlags::NotTransactable);
		}
	}

	check(ComboButton);
	ComboButton->SetIsOpen(false);
}

void FStateTreeStateLinkDetails::OnStateItemExpansionChanged(TSharedPtr<FStateTreeStateItem> ExpandedItem, bool bInExpanded) const
{
	// Do not save expansion state we're restoring expansion state, or when showing filtered results. 
	if (bIsRestoringExpansion || FilteredRootItem != RootItem)
	{
		return;
	}

	if (ExpandedItem.IsValid() && ExpandedItem->StateID.IsValid())
	{
		FStateExpansionState& ExpansionState = StateExpansionStates.FindOrAdd(FObjectKey(WeakStateTree.Get()));
		if (bInExpanded)
		{
			ExpansionState.CollapsedStates.Remove(ExpandedItem->StateID);
		}
		else
		{
			ExpansionState.CollapsedStates.Add(ExpandedItem->StateID);
		}
	}
}

void FStateTreeStateLinkDetails::OnSearchBoxTextChanged(const FText& NewText)
{
	if (!StateItemTree.IsValid())
	{
		return;
	}
	
	FilteredRootItem.Reset();

	TArray<FString> FilterStrings;
	NewText.ToString().ParseIntoArrayWS(FilterStrings);
	FilterStrings.RemoveAll([](const FString& String) { return String.IsEmpty(); });
	
	if (FilterStrings.IsEmpty())
	{
		// Show all when there's no filter string.
		FilteredRootItem = RootItem;
		StateItemTree->SetTreeItemsSource(&FilteredRootItem->Children);
		RestoreExpansionState();
		StateItemTree->RequestTreeRefresh();
		return;
	}

	FilteredRootItem = MakeShared<FStateTreeStateItem>();
	FilterStateItemChildren(FilterStrings, /*bParentMatches*/false, RootItem->Children, FilteredRootItem->Children);

	StateItemTree->SetTreeItemsSource(&FilteredRootItem->Children);
	ExpandAll(FilteredRootItem->Children);

	// Update selection based on filtered items.
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	const EStateTreeTransitionType TransitionType = GetTransitionType().Get(EStateTreeTransitionType::Failed);
	FGuid StateID;
	if (IDProperty
		&& UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, StateID) == FPropertyAccess::Success)
	{
		// Use RootItem (not filtered) to get any result even during search.
		if (FindStateByIDRecursive(FilteredRootItem, TransitionType, StateID, Path))
		{
			StateItemTree->SetItemSelection(Path.Last(), true);
		}
	}

	StateItemTree->RequestTreeRefresh();
}


int32 FStateTreeStateLinkDetails::FilterStateItemChildren(const TArray<FString>& FilterStrings, const bool bParentMatches,
															const TArray<TSharedPtr<FStateTreeStateItem>>& SourceArray,
															TArray<TSharedPtr<FStateTreeStateItem>>& OutDestArray)
{
	int32 NumFound = 0;

	auto MatchFilter = [&FilterStrings](const TSharedPtr<FStateTreeStateItem>& SourceItem)
	{
		const FString ItemName = SourceItem->Desc.ToString();
		for (const FString& Filter : FilterStrings)
		{
			if (ItemName.Contains(Filter))
			{
				return true;
			}
		}
		return false;
	};

	for (const TSharedPtr<FStateTreeStateItem>& SourceItem : SourceArray)
	{
		// Check if our name matches the filters
		// If bParentMatches is true, the search matched a parent category.
		const bool bMatchesFilters = bParentMatches || MatchFilter(SourceItem);

		int32 NumChildren = 0;
		if (bMatchesFilters)
		{
			NumChildren++;
		}

		// if we don't match, then we still want to check all our children
		TArray<TSharedPtr<FStateTreeStateItem>> FilteredChildren;
		NumChildren += FilterStateItemChildren(FilterStrings, bMatchesFilters, SourceItem->Children, FilteredChildren);

		// then add this item to the destination array
		if (NumChildren > 0)
		{
			TSharedPtr<FStateTreeStateItem>& NewItem = OutDestArray.Add_GetRef(MakeShared<FStateTreeStateItem>());
			*NewItem = *SourceItem;
			NewItem->Children = FilteredChildren;

			NumFound += NumChildren;
		}
	}

	return NumFound;
}

void FStateTreeStateLinkDetails::RestoreExpansionState()
{
	check(StateItemTree.IsValid());
	
	FStateExpansionState& ExpansionState = StateExpansionStates.FindOrAdd(FObjectKey(WeakStateTree.Get()));

	bIsRestoringExpansion = true;

	// Default state is expanded.
	ExpandAll(FilteredRootItem->Children);

	// Collapse the ones that are specifically collapsed.
	for (const FGuid& StateID : ExpansionState.CollapsedStates)
	{
		TArray<TSharedPtr<FStateTreeStateItem>> Path;
		if (FindStateByIDRecursive(FilteredRootItem, EStateTreeTransitionType::GotoState, StateID, Path))
		{
			StateItemTree->SetItemExpansion(Path.Last(), false);
		}
	}

	bIsRestoringExpansion = false;
}

void FStateTreeStateLinkDetails::ExpandAll(const TArray<TSharedPtr<FStateTreeStateItem>>& Items)
{
	for (const TSharedPtr<FStateTreeStateItem>& Item : Items)
	{
		StateItemTree->SetItemExpansion(Item, true);
		ExpandAll(Item->Children);
	}
}

bool FStateTreeStateLinkDetails::FindStateByIDRecursive(const TSharedPtr<FStateTreeStateItem>& Item, const EStateTreeTransitionType TransitionType, const FGuid StateID, TArray<TSharedPtr<FStateTreeStateItem>>& OutPath)
{
	OutPath.Push(Item);

	if (!Item->Desc.IsEmpty()
		&& Item->TransitionType == TransitionType
		&& Item->StateID == StateID)
	{
		return true;
	}

	for (const TSharedPtr<FStateTreeStateItem>& ChildItem : Item->Children)
	{
		if (FindStateByIDRecursive(ChildItem, TransitionType, StateID, OutPath))
		{
			return true;
		}
	}

	OutPath.Pop();

	return false;
}

bool FStateTreeStateLinkDetails::GetCurrentStateItem(TArray<TSharedPtr<FStateTreeStateItem>>& OutPath) const
{
	OutPath.Reset();
	const EStateTreeTransitionType TransitionType = GetTransitionType().Get(EStateTreeTransitionType::Failed);
	FGuid StateID;
	if (IDProperty
		&& UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, StateID) == FPropertyAccess::Success)
	{
		// Use RootItem (not filtered) to get any result even during search.
		return FindStateByIDRecursive(RootItem, TransitionType, StateID, OutPath);
	}
	return false;
}

FText FStateTreeStateLinkDetails::GetCurrentStateDesc() const
{
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	if (GetCurrentStateItem(Path))
	{
		return Path.Last()->Desc;
	}
	// Could not find item, try to display old name as a hint what is missing.
	if (NameProperty)
	{
		FName OldName;
		if (NameProperty->GetValue(OldName) == FPropertyAccess::Success)
		{
			return FText::FromName(OldName);
		}
	}
	return LOCTEXT("TransitionInvalid", "Invalid");
}

const FSlateBrush* FStateTreeStateLinkDetails::GetCurrentStateIcon() const
{
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	if (GetCurrentStateItem(Path))
	{
		return Path.Last()->Icon;
	}
	return nullptr;
}

FSlateColor FStateTreeStateLinkDetails::GetCurrentStateColor() const
{
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	if (GetCurrentStateItem(Path))
	{
		return Path.Last()->Color;
	}
	return {};
}

bool FStateTreeStateLinkDetails::IsValidLink() const
{
	TArray<TSharedPtr<FStateTreeStateItem>> Path;
	if (GetCurrentStateItem(Path))
	{
		return true;
	}
	// The state is missing.
	return false;
}

TOptional<EStateTreeTransitionType> FStateTreeStateLinkDetails::GetTransitionType() const
{
	if (LinkTypeProperty)
	{
		uint8 Value;
		if (LinkTypeProperty->GetValue(Value) == FPropertyAccess::Success)
		{
			return EStateTreeTransitionType(Value);
		}
	}
	return TOptional<EStateTreeTransitionType>();
}

#undef LOCTEXT_NAMESPACE

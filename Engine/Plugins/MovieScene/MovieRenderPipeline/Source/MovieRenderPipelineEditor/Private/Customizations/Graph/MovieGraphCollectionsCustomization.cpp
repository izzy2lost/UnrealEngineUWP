// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphCollectionsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Graph/MoviePipelineRenderLayerSubsystem.h"
#include "Graph/Nodes/MovieGraphCollectionNode.h"
#include "MovieRenderPipelineStyle.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SPositiveActionButton.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEditorCollectionCustomization"

void SMovieGraphCollectionTreeViewIndent::Construct(const FArguments& InArgs)
{
	const int NumIndents = InArgs._NumIndents.Get();
	const TSharedRef<SHorizontalBox> IndentContainer = SNew(SHorizontalBox);
	
	ChildSlot
	.VAlign(VAlign_Fill)
	[
		// A single indent doesn't change the background color; more indents get a darker background
		SNew(SBorder)
		.Padding(0)
		.VAlign(VAlign_Fill)
		.BorderImage((NumIndents <= 1)
			? FAppStyle::Get().GetBrush("NoBorder")
			: FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.IndentedRowBackground"))
		[
			IndentContainer
		]
	];

	// Add a new indent for each level of indent requested
	for (int32 Index = 0; Index < NumIndents; ++Index)
	{
		IndentContainer->AddSlot()
		.AutoWidth()
		.Padding(0)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("DetailsView.ArrayDropShadow"))
			[
				SNew(SBox)
				.WidthOverride(16.f)
			]
		];
	}
}

TSharedRef<FMovieGraphCollectionTreeDragOp> FMovieGraphCollectionTreeDragOp::New(const TSharedPtr<FCollectionCustomizationTreeElement>& InElement)
{
	TSharedRef<FMovieGraphCollectionTreeDragOp> Operation = MakeShared<FMovieGraphCollectionTreeDragOp>();
	Operation->Element = InElement;
	Operation->Construct();
	
	return Operation;
}

void SMovieGraphCollectionTreeView::Construct(const FArguments& InArgs)
{
	STreeView<TSharedPtr<FCollectionCustomizationTreeElement>>::FArguments SuperArgs;

	DetailBuilder = InArgs._DetailBuilder;
	Collection = InArgs._Collection;

	SuperArgs
	.ItemHeight(28)
	.TreeItemsSource(&RootElements)
	.SelectionMode(ESelectionMode::Single)
	.OnGenerateRow_Lambda([this](const TSharedPtr<FCollectionCustomizationTreeElement>& TreeItem, const TSharedPtr<STableViewBase>& OwnerTable)
	{
		return GenerateTreeRow(TreeItem, OwnerTable.ToSharedRef());
	})
	.OnGetChildren(this, &SMovieGraphCollectionTreeView::GetChildrenForTree)
	.OnExpansionChanged(this, &SMovieGraphCollectionTreeView::OnExpansionChanged)
	.HeaderRow
	(
		SNew(SHeaderRow)
		.Visibility(EVisibility::Collapsed)

		+ SHeaderRow::Column(SMovieGraphCollectionTreeItem::ColumnID_Name)
		.DefaultLabel(LOCTEXT("NameColumnLabel", "Name"))
		.SortMode(EColumnSortMode::None)
		.FillWidth(0.5f)

		+ SHeaderRow::Column(SMovieGraphCollectionTreeItem::ColumnID_Value)
		.DefaultLabel(LOCTEXT("ValueColumnLabel", "Value"))
		.SortMode(EColumnSortMode::None)
		.FillWidth(0.5f)
	);
	
	STreeView<TSharedPtr<FCollectionCustomizationTreeElement>>::Construct(SuperArgs);
}

void SMovieGraphCollectionTreeView::OnExpansionChanged(const TSharedPtr<FCollectionCustomizationTreeElement> InElement, const bool bInExpanded)
{
	if (bInExpanded)
	{
		ExpandedElements.Add(InElement->GetHash());
	}
	else
	{
		ExpandedElements.Remove(InElement->GetHash());
	}
}

void SMovieGraphCollectionTreeView::GetChildrenForTree(const TSharedPtr<FCollectionCustomizationTreeElement> InItem, TArray<TSharedPtr<FCollectionCustomizationTreeElement>>& OutChildren)
{
	OutChildren.Append(InItem->GetChildren());
}

void SMovieGraphCollectionTreeView::RestoreExpansionStateRecursive(const TSharedPtr<FCollectionCustomizationTreeElement>& InElement)
{
	SetItemExpansion(InElement, ExpandedElements.Contains(InElement->GetHash()));
	
	for (const TSharedPtr<FCollectionCustomizationTreeElement>& ChildElement : InElement->GetChildren())
	{
		RestoreExpansionStateRecursive(ChildElement);
	}
}

TSharedRef<ITableRow> SMovieGraphCollectionTreeView::GenerateTreeRow(TSharedPtr<FCollectionCustomizationTreeElement> InTreeElement, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (InTreeElement->Type == FCollectionCustomizationTreeElement::EElementType::QueryProperty)
	{
		if (InTreeElement->Query && !InTreeElement->Query->GetWidgets().IsEmpty())
		{
			return SNew(SMovieGraphCollectionTreeItem_Stretch, SharedThis(this), InTreeElement);
		}
	}

	if ((InTreeElement->Type == FCollectionCustomizationTreeElement::EElementType::EmptyCollection) ||
		(InTreeElement->Type == FCollectionCustomizationTreeElement::EElementType::EmptyConditionGroup))
	{
		return SNew(SMovieGraphCollectionTreeItem_Stretch, SharedThis(this), InTreeElement);
	}
	
	return SNew(SMovieGraphCollectionTreeItem, SharedThis(this), InTreeElement, DetailBuilder);
}

void SMovieGraphCollectionTreeView::AddRootElement()
{
	const TSharedPtr<FCollectionCustomizationTreeElement> RootElement = MakeShared<FCollectionCustomizationTreeElement>(FCollectionCustomizationTreeElement::EElementType::Root);
	RootElement->Collection = Collection.Get();
	
	RootElements.Add(RootElement);

	// Expand the root element by default because it is never visible and the user cannot expand it themselves
	constexpr bool bIsExpanded = true;
	SetItemExpansion(RootElement, bIsExpanded);
	ExpandedElements.Add(RootElement->GetHash());
}

void SMovieGraphCollectionTreeView::AddRootConditionGroupElements(const TObjectPtr<UMovieGraphCollection>& InCollection) const
{
	// Add the collection's condition groups. These will be the root elements of the tree, and each condition group will take
	// over adding its own children to the tree.
	const TArray<UMovieGraphConditionGroup*> ConditionGroups = InCollection->GetConditionGroups();
	for (int32 Index = 0; Index < ConditionGroups.Num(); ++Index)
	{
		AddNewConditionGroupElement(ConditionGroups[Index]);
	}
}

void SMovieGraphCollectionTreeView::AddNewConditionGroupElement(UMovieGraphConditionGroup* InConditionGroup) const
{
	const TSharedPtr<FCollectionCustomizationTreeElement> NewElement = MakeShared<FCollectionCustomizationTreeElement>(FCollectionCustomizationTreeElement::EElementType::ConditionGroup);
	NewElement->ConditionGroup = InConditionGroup;
	NewElement->Collection = Collection.Get();
}

void SMovieGraphCollectionTreeView::AddConditionGroup()
{
	if (Collection.IsValid())
	{
		UMovieGraphConditionGroup* NewConditionGroup = Collection.Get()->AddConditionGroup();
		
		AddNewConditionGroupElement(NewConditionGroup);

		if (!RootElements.IsEmpty())
		{
			// Needed so the "empty collection" notice is cleared out (if this is the first condition group being added to the collection).
			RootElements[0]->ClearCachedChildren();
		}
		
		RefreshView();
	}
}

void SMovieGraphCollectionTreeView::RemoveConditionGroup(const TWeakPtr<FCollectionCustomizationTreeElement>& InConditionGroupElement)
{
	if (!InConditionGroupElement.IsValid())
	{
		return;
	}
	
	const TSharedPtr<FCollectionCustomizationTreeElement> TreeElementPin = InConditionGroupElement.Pin();
	if (!TreeElementPin->Collection)
	{
		return;
	}

	TreeElementPin->Collection->RemoveConditionGroup(TreeElementPin->ConditionGroup);

	if (TreeElementPin->ParentElement.IsValid())
	{
		TreeElementPin->ParentElement.Pin()->ClearCachedChildren();
	}

	RefreshView();
}

void SMovieGraphCollectionTreeView::RefreshView()
{
	RequestTreeRefresh();
    
    // Restore tree expansion state after the refresh
    for (const TSharedPtr<FCollectionCustomizationTreeElement>& RootElement : RootElements)
    {
    	RestoreExpansionStateRecursive(RootElement);
    }
}

void SMovieGraphCollectionTreeOpTypeWidget::Construct(const FArguments& InArgs)
{
	bIsConditionGroup = InArgs._IsConditionGroup.Get();
	WeakTreeElement = InArgs._WeakTreeElement.Get();
	
	ChildSlot
	[
		SNew(SComboBox<FName>)
		.ButtonStyle(FAppStyle::Get(), "NoBorder")
		.ForegroundColor(FSlateColor::UseForeground())
		.IsEnabled(this, &SMovieGraphCollectionTreeOpTypeWidget::IsWidgetEnabled)
		.OptionsSource(bIsConditionGroup ? GetOpTypes<EMovieGraphConditionGroupOpType>() : GetOpTypes<EMovieGraphConditionGroupQueryOpType>())
		.OnSelectionChanged(this, &SMovieGraphCollectionTreeOpTypeWidget::SetOpType)
		.OnGenerateWidget_Lambda([this](const FName& InOpType)
		{
			return GetOpTypeContents(InOpType, bIsConditionGroup);
		})
		.Content()
		[
			GetOpTypeContents(NAME_None, bIsConditionGroup)
		]
	];
}

template <typename T>
TArray<FName>* SMovieGraphCollectionTreeOpTypeWidget::GetOpTypes()
{
	static_assert(TIsEnum<T>::Value, "Provided type must be an enum");
		
	static TArray<FName> OpTypes;

	if (OpTypes.IsEmpty())
	{
		const UEnum* OpTypeEnum = StaticEnum<T>();

		// -1 to skip the implicit "MAX" added at compile time
		for (int32 Index = 0; Index < OpTypeEnum->NumEnums() - 1; ++Index)
		{
			OpTypes.Add(FName(OpTypeEnum->GetDisplayNameTextByIndex(Index).ToString()));
		}
	}

	return &OpTypes;
}

TSharedRef<SWidget> SMovieGraphCollectionTreeOpTypeWidget::GetOpTypeContents(const FName& InOpName, const bool bIsConditionGroupOp) const
{
	TSharedPtr<SWidget> OpTypeTextBlock;

	// This method may only be called once in some scenarios, so provide the option of a text lambda vs. constant text if the
	// text needs to be updated dynamically when the op type changes.
	if (InOpName == NAME_None)
	{
		SAssignNew(OpTypeTextBlock, STextBlock)
		.Text_Lambda([this, bIsConditionGroupOp]()
		{
			return FText::FromName(bIsConditionGroupOp ? GetCurrentConditionGroupOpType() : GetCurrentConditionGroupQueryOpType());
		})
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}
	else
	{
		SAssignNew(OpTypeTextBlock, STextBlock)
		.Text(FText::FromName(InOpName))
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}
		
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 7.f, 0)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.Settings"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			OpTypeTextBlock.ToSharedRef()
		];
}

bool SMovieGraphCollectionTreeOpTypeWidget::IsWidgetEnabled() const
{
	if (WeakTreeElement.IsValid())
	{
		if (bIsConditionGroup)
		{
			if (const TObjectPtr<UMovieGraphConditionGroup> ConditionGroup = WeakTreeElement.Pin()->ConditionGroup)
			{
				// Disable the first condition group
				return !ConditionGroup->IsFirstConditionGroup();
			}
		}
		else
		{
			if (const TObjectPtr<UMovieGraphConditionGroupQueryBase> ConditionGroupQuery = WeakTreeElement.Pin()->Query)
			{
				// Disable the first query, or disable it if it was flagged as disabled
				return !ConditionGroupQuery->IsFirstConditionGroupQuery() && ConditionGroupQuery->IsEnabled();
			}
		}
	}
	
	return true;
}

void SMovieGraphCollectionTreeOpTypeWidget::SetOpType(const FName InNewOpType, ESelectInfo::Type SelectInfo) const
{
	if (!WeakTreeElement.IsValid())
	{
		return;
	}
	
	if (bIsConditionGroup)
	{
		if (const TObjectPtr<UMovieGraphConditionGroup>& ConditionGroup = WeakTreeElement.Pin()->ConditionGroup)
		{
			const UEnum* OpTypeEnum = StaticEnum<EMovieGraphConditionGroupOpType>();
			ConditionGroup->SetOperationType(static_cast<EMovieGraphConditionGroupOpType>(OpTypeEnum->GetValueByName(InNewOpType)));
		}
	}
	else
	{
		if (const TObjectPtr<UMovieGraphConditionGroupQueryBase>& Query = WeakTreeElement.Pin()->Query)
		{
			const UEnum* OpTypeEnum = StaticEnum<EMovieGraphConditionGroupQueryOpType>();
			Query->SetOperationType(static_cast<EMovieGraphConditionGroupQueryOpType>(OpTypeEnum->GetValueByName(InNewOpType)));
		}
	}
}

FName SMovieGraphCollectionTreeOpTypeWidget::GetCurrentConditionGroupOpType() const
{
	if (WeakTreeElement.IsValid())
	{
		if (const TObjectPtr<UMovieGraphConditionGroup>& ConditionGroup = WeakTreeElement.Pin()->ConditionGroup)
		{
			const UEnum* OpTypeEnum = StaticEnum<EMovieGraphConditionGroupOpType>();
			const EMovieGraphConditionGroupOpType OpType = ConditionGroup->GetOperationType();
			const FText DisplayNameText = OpTypeEnum->GetDisplayNameTextByValue(static_cast<__underlying_type(EMovieGraphConditionGroupOpType)>(OpType));
			
			return FName(DisplayNameText.ToString());
		}
	}
		
	return FName();
}

FName SMovieGraphCollectionTreeOpTypeWidget::GetCurrentConditionGroupQueryOpType() const
{
	if (WeakTreeElement.IsValid())
	{
		if (const TObjectPtr<UMovieGraphConditionGroupQueryBase>& Query = WeakTreeElement.Pin()->Query)
		{
			const UEnum* OpTypeEnum = StaticEnum<EMovieGraphConditionGroupQueryOpType>();
			const EMovieGraphConditionGroupQueryOpType OpType = Query->GetOperationType();
			const FText DisplayNameText = OpTypeEnum->GetDisplayNameTextByValue(static_cast<__underlying_type(EMovieGraphConditionGroupQueryOpType)>(OpType));
			
			return FName(DisplayNameText.ToString());
		}
	}
		
	return FName();
}

void SMovieGraphCollectionTreeAddQueryContentWidget::Construct(const FArguments& InArgs)
{
	TWeakPtr<FCollectionCustomizationTreeElement> WeakTreeElement = InArgs._WeakTreeElement.Get();
	TWeakPtr<SMovieGraphCollectionTreeView> WeakTreeView = InArgs._WeakTreeView.Get();

	if (!WeakTreeElement.IsValid() || !WeakTreeView.IsValid())
	{
		return;
	}

	// Call the underlying query to get the contents of the menu. The query will call back to the widget if something was added so the tree view has
	// an opportunity to refresh itself.
	TSharedRef<SWidget> AddMenuContents = WeakTreeElement.Pin()->Query->GetAddMenuContents(
		UMovieGraphConditionGroupQueryBase::FMovieGraphConditionGroupQueryContentsChanged::CreateLambda([WeakTreeElement, WeakTreeView]()
    {
    	if (WeakTreeView.IsValid())
    	{
    		if (WeakTreeElement.IsValid())
    		{
    			WeakTreeElement.Pin()->ClearCachedChildren();
    		}
    		
    		WeakTreeView.Pin()->RefreshView();
    		FSlateApplication::Get().DismissAllMenus();
    	}
    }));

    // Generate a (multi-layered) icon for the "Add" menu
    const TSharedRef<SLayeredImage> AddIcon =
    	SNew(SLayeredImage)
    	.ColorAndOpacity(FSlateColor::UseForeground())
    	.Image(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Background"));
    AddIcon->AddLayer(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Overlay"));

	ChildSlot
	[
		SNew(SComboButton)
		.ToolTipText(LOCTEXT("AddConditionGroupQueryContent", "Add content to this query."))
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.ContentPadding(0)
		.HasDownArrow(false)
		.IsEnabled_Lambda([WeakTreeElement]()
		{
			return WeakTreeElement.IsValid() && WeakTreeElement.Pin()->IsQueryEnabled();
		})
		.OnGetMenuContent_Lambda([AddMenuContents]()
		{
			return AddMenuContents;
		})
		.Visibility_Lambda([AddMenuContents]()
		{
			// Queries can opt to not show an Add menu by returning NullWidget
			return (AddMenuContents != SNullWidget::NullWidget) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.ButtonContent()
		[
			AddIcon
		]
	];
}

void SMovieGraphCollectionTreeQueryTypeSelectorWidget::Construct(const FArguments& InArgs)
{
	WeakTreeElement = InArgs._WeakTreeElement.Get();
	WeakTreeView = InArgs._WeakTreeView.Get();

	ChildSlot
	[
		SNew(SComboBox<UClass*>)
		.IsEnabled_Lambda([this]()
		{
			return WeakTreeElement.IsValid() && WeakTreeElement.Pin()->IsQueryEnabled();
		})
		.OptionsSource(GetAvailableQueryTypes())
		.OnSelectionChanged(this, &SMovieGraphCollectionTreeQueryTypeSelectorWidget::SetQueryType)
		.OnGenerateWidget_Lambda([this](UClass* InClass)
		{
			return GetQueryTypeContents(InClass);
		})
		.Content()
		[
			GetQueryTypeContents(nullptr)
		]
	];
}

TArray<UClass*>* SMovieGraphCollectionTreeQueryTypeSelectorWidget::GetAvailableQueryTypes()
{
	static TArray<UClass*> QueryTypes;

	if (QueryTypes.IsEmpty())
	{
		for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
		{
			UClass* Class = *ClassIterator;

			if (Class->IsChildOf(UMovieGraphConditionGroupQueryBase::StaticClass()) &&
				!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_Hidden))
			{
				QueryTypes.Add(Class);
			}
		}
	}

	return &QueryTypes;
}

TSharedRef<SWidget> SMovieGraphCollectionTreeQueryTypeSelectorWidget::GetQueryTypeContents(UClass* InTypeClass) const
{
	TSharedPtr<SWidget> QueryTypeTextBlock;

	// This method may only be called once in some scenarios, so provide the option of a text lambda vs. constant text if the
	// text needs to be updated dynamically when the query type changes.
	if (InTypeClass == nullptr)
	{
		SAssignNew(QueryTypeTextBlock, STextBlock)
		.Text_Lambda([this]()
		{
			return GetCurrentQueryTypeDisplayName();
		})
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		// For below, assign the query type to the current query type
		InTypeClass = GetCurrentQueryType();
	}
	else
	{
		SAssignNew(QueryTypeTextBlock, STextBlock)
		.Text_Lambda([InTypeClass]()
		{
			if (const UMovieGraphConditionGroupQueryBase* Query = Cast<UMovieGraphConditionGroupQueryBase>(InTypeClass->GetDefaultObject()))
			{
				return Query->GetDisplayName();
			}

			// Show the UClass display name as a backup
			return InTypeClass->GetDisplayNameText();
		})
		.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
	}

	// Defer to the query to get the icon that should be displayed
	const FSlateIcon QueryTypeIcon = InTypeClass
		? GetDefault<UMovieGraphConditionGroupQueryBase>(InTypeClass)->GetIcon()
		: FSlateIcon();
	
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 7.f, 0)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(QueryTypeIcon.GetIcon())
			.ColorAndOpacity(FSlateColor::UseForeground())
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			QueryTypeTextBlock.ToSharedRef()
		];
}

FText SMovieGraphCollectionTreeQueryTypeSelectorWidget::GetCurrentQueryTypeDisplayName() const
{
	if (WeakTreeElement.IsValid())
	{
		if (const UClass* CurrentQueryType = GetCurrentQueryType())
		{
			check(CurrentQueryType->IsChildOf(UMovieGraphConditionGroupQueryBase::StaticClass()));
			
			return Cast<UMovieGraphConditionGroupQueryBase>(CurrentQueryType->GetDefaultObject())->GetDisplayName();
		}
	}
		
	return FText();
}

UClass* SMovieGraphCollectionTreeQueryTypeSelectorWidget::GetCurrentQueryType() const
{
	if (WeakTreeElement.IsValid())
	{
		return WeakTreeElement.Pin()->Query->GetClass();
	}

	return nullptr;
}

void SMovieGraphCollectionTreeQueryTypeSelectorWidget::SetQueryType(UClass* InNewQueryType, ESelectInfo::Type SelectInfo) const
{
	if (!WeakTreeElement.IsValid())
	{
		return;
	}
	
	const TSharedPtr<FCollectionCustomizationTreeElement> TreeElementPin = WeakTreeElement.Pin();

	// Swap the old query with a new one
	if (const TObjectPtr<UMovieGraphConditionGroup>& ConditionGroup = TreeElementPin->ConditionGroup)
	{
		const auto& ExistingQuery = TreeElementPin->Query;
		const int32 ExistingQueryIndex = ConditionGroup->GetQueries().Find(ExistingQuery);

		// Add the new query at the index of the current query. If the query couldn't be found for some reason, default to the end of the array (-1)
		ConditionGroup->AddQuery(InNewQueryType, ExistingQueryIndex != INDEX_NONE ? ExistingQueryIndex : -1);
	
		ConditionGroup->RemoveQuery(ExistingQuery);
	}

	// Changing the query type removes the current query and adds a new one to the parent condition group, which means the parent's children
	// need to be refreshed
	if (TreeElementPin->ParentElement.IsValid())
	{
		TreeElementPin->ParentElement.Pin()->ClearCachedChildren();			
	}

	if (WeakTreeView.IsValid())
	{
		WeakTreeView.Pin()->RefreshView();
	}
}

FCollectionCustomizationTreeElement::FCollectionCustomizationTreeElement(const EElementType InType)
	: Type(InType)
{
		
}

bool FCollectionCustomizationTreeElement::IsQueryEnabled() const
{
	return Query ? Query->IsEnabled() : false;
}

const TArray<TSharedPtr<FCollectionCustomizationTreeElement>>& FCollectionCustomizationTreeElement::GetChildren() const
{
	// Returned the cached children if they are available
	if (!ChildrenCache.IsEmpty())
	{
		return ChildrenCache;
	}

	/** Creates a new child element of the specified type, with the associated pointers to the objects needed for that type. */
	auto AddChildElement = [this](
		const EElementType ElementType, UMovieGraphConditionGroup* InConditionGroup = nullptr,
		UMovieGraphConditionGroupQueryBase* InQuery = nullptr, FProperty* InQueryProperty = nullptr)
	{
		const TSharedPtr<FCollectionCustomizationTreeElement> NewElement = MakeShared<FCollectionCustomizationTreeElement>(ElementType);
		NewElement->Collection = Collection;
		NewElement->ConditionGroup = InConditionGroup;
		NewElement->Query = InQuery;
		NewElement->QueryProperty = InQueryProperty;
		NewElement->ParentElement = AsWeak();

		ChildrenCache.Add(NewElement);
	};

	if (Type == EElementType::Root)
	{
		const TArray<UMovieGraphConditionGroup*>& ConditionGroups = Collection->GetConditionGroups();

		if (ConditionGroups.IsEmpty())
		{
			AddChildElement(EElementType::EmptyCollection);
		}
		else
		{
			for (UMovieGraphConditionGroup* Group : ConditionGroups)
			{
				AddChildElement(EElementType::ConditionGroup, Group);
			}
		}
	}
	
	if (Type == EElementType::ConditionGroup)
	{
		const TArray<UMovieGraphConditionGroupQueryBase*>& ConditionGroupQueries = ConditionGroup->GetQueries();
		
		if (ConditionGroupQueries.IsEmpty())
		{
			AddChildElement(EElementType::EmptyConditionGroup);
		}
		else
		{
			for (UMovieGraphConditionGroupQueryBase* GroupQuery : ConditionGroup->GetQueries())
			{
				AddChildElement(EElementType::Query, ConditionGroup, GroupQuery);
			}
		}
	}

	if (Type == EElementType::Query)
	{
		// If the query provides widgets, add a row for each widget. Otherwise, add a row for each EditAnywhere property within the query.
		const TArray<TSharedRef<SWidget>> QueryWidgets = Query->GetWidgets();
		if (!QueryWidgets.IsEmpty())
		{
			for (const TSharedRef<SWidget>& Widget : QueryWidgets)
			{
				FProperty* UnknownProperty = nullptr;
				AddChildElement(EElementType::QueryProperty, ConditionGroup, Query, UnknownProperty);
			}
		}
		else
		{
			for (TFieldIterator<FProperty> PropIt(Query->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Prop = *PropIt;
				if (!Prop->HasAllPropertyFlags(CPF_Edit))
				{
					continue;
				}

				AddChildElement(EElementType::QueryProperty, ConditionGroup, Query, Prop);
			}
		}
	}

	return ChildrenCache;
}

void FCollectionCustomizationTreeElement::ClearCachedChildren() const
{
	ChildrenCache.Empty();
}

uint32 FCollectionCustomizationTreeElement::GetHash() const
{
	if (ElementHash != 0)
	{
		return ElementHash;
	}

	switch (Type)
	{
	case EElementType::Root:
	case EElementType::EmptyCollection:
	case EElementType::EmptyConditionGroup:
		ElementHash = GetTypeHash(true);	// There will only ever be one root, or one of the "empty" notices under a given parent
		break;
	case EElementType::ConditionGroup:
		ElementHash = GetTypeHash(ConditionGroup);
		break;
	case EElementType::Query:
		ElementHash = GetTypeHash(Query);
		break;
	case EElementType::QueryProperty:
		ElementHash = GetTypeHash(QueryProperty);
		break;
	}
		
	if (ParentElement.IsValid())
	{
		ElementHash = HashCombine(ElementHash, ParentElement.Pin()->GetHash());
	}

	return ElementHash;
}

TArray<TSharedRef<SWidget>> SMovieGraphCollectionTreeItem_Stretch::GetColumnWidgets() const
{
	if (!WeakTreeElement.IsValid())
	{
		return { SNullWidget::NullWidget };
	}

	const TSharedPtr<FCollectionCustomizationTreeElement> TreeElement = WeakTreeElement.Pin();

	if (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::QueryProperty)
	{
		// Display the widgets specified by the query
		return TreeElement->Query->GetWidgets();
	}

	if (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::EmptyCollection ||
		TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::EmptyConditionGroup)
	{
		const FText NoticeText = (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::EmptyCollection)
			? LOCTEXT("EmptyCollectionNotice", "Add a condition group to this collection.")
			: LOCTEXT("EmptyConditionGroupNotice", "Add a condition to this condition group.");
			
		return {
			SNew(SBox)
			.MinDesiredHeight(50)
			.Padding(10.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Content()
			[
				SNew(STextBlock)
				.Text(NoticeText)
			]
		};
	}
	
	return { SNullWidget::NullWidget };
}

void SMovieGraphCollectionTreeItem_Stretch::Construct(
	const FArguments& InArgs, const TSharedPtr<SMovieGraphCollectionTreeView>& InOwnerTable, const TSharedPtr<FCollectionCustomizationTreeElement>& InTreeElement)
{
	check(InTreeElement);
	WeakTreeElement = InTreeElement.ToWeakPtr();

	TSharedPtr<SVerticalBox> WidgetContainer;

	// Add a new indented row which has a designated place for widgets to be added (WidgetContainer)
	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Fill)
		[
			SNew(SMovieGraphCollectionTreeViewIndent)
			.NumIndents(2)
		]

		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		[
			SNew(SBorder)
			.Padding(0, 0, 0, 1.f)
			.HAlign(HAlign_Fill)
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomBorder"))
			[
				SAssignNew(WidgetContainer, SVerticalBox)
			]
		]
	];

	// Add all widgets that are generated by the column
	for (const TSharedRef<SWidget>& Widget : GetColumnWidgets())
	{
		WidgetContainer->AddSlot()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.IsEnabled_Lambda([InTreeElement]()
				{
					return InTreeElement->IsQueryEnabled();
				})
				[
					Widget
				]
			];
	}
	
	ConstructInternal(
		FArguments()
			.ShowSelection(true)
			.Style(FMovieRenderPipelineStyle::Get(), "MovieRenderGraph.CollectionsTree.Row"),
		InOwnerTable.ToSharedRef());
}

void SMovieGraphCollectionTreeItem::Construct(const FArguments& InArgs, const TSharedPtr<SMovieGraphCollectionTreeView>& InOwnerTable, const TSharedPtr<FCollectionCustomizationTreeElement>& InTreeElement, const TWeakPtr<IDetailLayoutBuilder>& InDetailBuilder)
{
	WeakTreeElement = InTreeElement;
	WeakTreeView = InOwnerTable.ToWeakPtr();
	DetailBuilder = InDetailBuilder;

	SMultiColumnTableRow<TSharedPtr<FCollectionCustomizationTreeElement>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FCollectionCustomizationTreeElement>>::FArguments()
			.Padding(0)
			.Style(FMovieRenderPipelineStyle::Get(), "MovieRenderGraph.CollectionsTree.Row")
			.OnDragDetected(this, &SMovieGraphCollectionTreeItem::HandleOnDragDetected)
			.OnCanAcceptDrop(this, &SMovieGraphCollectionTreeItem::HandleOnCanAcceptDrop)
			.OnAcceptDrop(this, &SMovieGraphCollectionTreeItem::HandleOnAcceptDrop)
		, InOwnerTable.ToSharedRef());
}

FReply SMovieGraphCollectionTreeItem::HandleOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& MouseEvent) const
{
	if (!WeakTreeView.IsValid())
	{
		return FReply::Unhandled();
	}
	
	TArray<TSharedPtr<FCollectionCustomizationTreeElement>> SelectedElements = WeakTreeView.Pin()->GetSelectedItems();

	if ((SelectedElements.Num() != 1) || !SelectedElements[0].IsValid())
	{
		return FReply::Unhandled();
	}

	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const FText DragDropText = SelectedElements[0]->Type == FCollectionCustomizationTreeElement::EElementType::ConditionGroup
			? LOCTEXT("DragConditionGroup", "Move 1 Condition Group")
			: LOCTEXT("DragConditionGroupQuery", "Move 1 Condition");
					
		const TSharedRef<FMovieGraphCollectionTreeDragOp> DragDropOp = FMovieGraphCollectionTreeDragOp::New(SelectedElements[0]);
		DragDropOp->CurrentHoverText = DragDropText;
					
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> SMovieGraphCollectionTreeItem::HandleOnCanAcceptDrop(const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone, const TSharedPtr<FCollectionCustomizationTreeElement> TargetItem)
{
	TOptional<EItemDropZone> ReturnedDropZone;
	const TSharedPtr<FMovieGraphCollectionTreeDragOp> DragDropOp = DragDropEvent.GetOperationAs<FMovieGraphCollectionTreeDragOp>();

	// Can't drag an element onto another element -- only changing order is allowed (above/below)
	if (DropZone == EItemDropZone::OntoItem)
	{
		DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
		return ReturnedDropZone;
	}
				
	if (DragDropOp.IsValid() && DragDropOp->Element.IsValid())
	{
		// All tree items can be moved *except* query properties
		if (TargetItem.Get()->Type != FCollectionCustomizationTreeElement::EElementType::QueryProperty)
		{
			// For now, elements cannot be moved to a different parent
			if (TargetItem.Get()->ParentElement == DragDropOp->Element->ParentElement)
			{
				DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Ok"));
				ReturnedDropZone = DropZone;
			}
			else
			{
				DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			}
		}
	}

	return ReturnedDropZone;
}

FReply SMovieGraphCollectionTreeItem::HandleOnAcceptDrop(const FDragDropEvent& DragDropEvent, const EItemDropZone DropZone, const TSharedPtr<FCollectionCustomizationTreeElement> TargetItem) const
{
	const TSharedPtr<FMovieGraphCollectionTreeDragOp> DragDropOp = DragDropEvent.GetOperationAs<FMovieGraphCollectionTreeDragOp>();
	if (!DragDropOp.IsValid())
	{
		return FReply::Unhandled();
	}

	// Figure out the new order of elements
	if (TargetItem->Type == FCollectionCustomizationTreeElement::EElementType::ConditionGroup)
	{
		if (TargetItem->Collection)
		{
			const TArray<UMovieGraphConditionGroup*>& ConditionGroups = TargetItem->Collection->GetConditionGroups();
						
			// Find the index of the target
			const int32 TargetIndex = ConditionGroups.Find(TargetItem->ConditionGroup);
			if (TargetIndex != INDEX_NONE)
			{
				FScopedTransaction Transaction(LOCTEXT("ReorderConditionGroup", "Reorder Condition Group"));
				const int32 NewIndex = DropZone == EItemDropZone::AboveItem ? TargetIndex : TargetIndex + 1;
				TargetItem->Collection->MoveConditionGroupToIndex(DragDropOp.Get()->Element->ConditionGroup, NewIndex);
							
				DragDropOp.Get()->Element->ParentElement.Pin()->ClearCachedChildren();
				RefreshTreeView();
			}
		}
	}
	else if (TargetItem->Type == FCollectionCustomizationTreeElement::EElementType::Query)
	{
		if (TargetItem->ConditionGroup)
		{
			const TArray<UMovieGraphConditionGroupQueryBase*>& Queries = TargetItem->ConditionGroup->GetQueries();

			// Find the index of the target
			const int32 TargetIndex = Queries.Find(TargetItem->Query);
			if (TargetIndex != INDEX_NONE)
			{
				FScopedTransaction Transaction(LOCTEXT("ReorderConditionGroupQuery", "Reorder Condition Group Query"));
				const int32 NewIndex = DropZone == EItemDropZone::AboveItem ? TargetIndex : TargetIndex + 1;
				TargetItem->ConditionGroup->MoveQueryToIndex(DragDropOp.Get()->Element->Query, NewIndex);
							
				DragDropOp.Get()->Element->ParentElement.Pin()->ClearCachedChildren();
				RefreshTreeView();
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SMovieGraphCollectionTreeItem::GenerateConditionGroupColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement)
{
	if (ColumnName == ColumnID_Name)
	{
		// Generate the condition group name
		const int32 GroupIndex = (TreeElement->ConditionGroup && TreeElement->Collection)
			? TreeElement->Collection->GetConditionGroups().Find(TreeElement->ConditionGroup)
			: 0;
		const FString GroupName = FString::Printf(TEXT("Condition Group %i"), (GroupIndex + 1));
		
		return
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.Padding(0)
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.Padding(0)
				[
					SNew(SMovieGraphCollectionTreeViewIndent)
					.NumIndents(1)
				]
		
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SExpanderArrow, SharedThis(this))
					.IndentAmount(0)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 2.f, 0)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("ClassIcon.GroupActor"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 2.f, 0)
				[
					SNew(STextBlock)
					.Text(FText::FromString(GroupName))
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
				]
			];
	}

	if (ColumnName == ColumnID_Value)
	{
		return
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomLeftBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ToolTip(SNew(SToolTip).Text(LOCTEXT("AddConditionGroupQuery", "Add a condition to this condition group.")))
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked_Lambda([this]()
					{
						if (WeakTreeElement.IsValid())
						{
							const TSharedPtr<FCollectionCustomizationTreeElement> TreeElementPin = WeakTreeElement.Pin();
							
							if (const TObjectPtr<UMovieGraphConditionGroup> ConditionGroup = TreeElementPin->ConditionGroup)
							{
								// Use the Actor Name query as the default
								ConditionGroup->AddQuery(UMovieGraphConditionGroupQuery_ActorName::StaticClass());

								if (TreeElementPin->ParentElement.IsValid())
								{
									TreeElementPin->ParentElement.Pin()->ClearCachedChildren();
								}
								
								RefreshTreeView();
							}
						}

						return FReply::Handled();
					})
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SComboButton)
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.ContentPadding(0)
					.ForegroundColor(FSlateColor::UseForeground())
					.HasDownArrow(true)
					.OnGetMenuContent_Lambda([this]()
					{
						FMenuBuilder ViewOptions(true, nullptr);

						ViewOptions.AddMenuEntry(
							LOCTEXT("DeleteConditionGroup", "Delete"),
							LOCTEXT("DeleteConditionGroupTooltip", "Delete this condition group and the queries in it."),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([this]()
								{
									if (WeakTreeView.IsValid())
									{
										WeakTreeView.Pin()->RemoveConditionGroup(WeakTreeElement);
									}
                                }),
								FCanExecuteAction()));

						return ViewOptions.MakeWidget();
					})
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(2.f)
				.FillWidth(1.f)
				[
					SNew(SMovieGraphCollectionTreeOpTypeWidget)
					.IsConditionGroup(true)
					.WeakTreeElement(WeakTreeElement)
				]
			];
	}
	
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SMovieGraphCollectionTreeItem::GenerateConditionGroupQueryColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement)
{
	if (ColumnName == ColumnID_Name)
	{
		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			[
				SNew(SMovieGraphCollectionTreeViewIndent)
				.NumIndents(2)
			]

			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
						.IndentAmount(0)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4.f, 0)
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([TreeElement]()
						{
							return TreeElement->IsQueryEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([TreeElement](ECheckBoxState NewCheckState)
						{
							if (TreeElement.IsValid() && TreeElement->Query)
							{
								TreeElement->Query->SetEnabled(NewCheckState == ECheckBoxState::Checked);
							}
						})
					]

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.Padding(0, 0, 7.f, 0)
					[
						SNew(SMovieGraphCollectionTreeQueryTypeSelectorWidget)
						.WeakTreeElement(WeakTreeElement)
						.WeakTreeView(WeakTreeView)
					]
				]
			];
	}

	if (ColumnName == ColumnID_Value)
	{			
		return
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomLeftBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(4.f, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SMovieGraphCollectionTreeAddQueryContentWidget)
					.WeakTreeElement(WeakTreeElement)
					.WeakTreeView(WeakTreeView)
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SComboButton)
					.IsEnabled_Lambda([this]()
					{
						return WeakTreeElement.IsValid() ? WeakTreeElement.Pin()->IsQueryEnabled() : false;
					})
					.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
					.ContentPadding(0)
					.ForegroundColor(FSlateColor::UseForeground())
					.HasDownArrow(true)
					.OnGetMenuContent_Lambda([this]()
					{
						FMenuBuilder ViewOptions(true, nullptr);

						ViewOptions.AddMenuEntry(
							LOCTEXT("DeleteConditionGroupQuery", "Delete"),
							LOCTEXT("DeleteConditionGroupQueryTooltip", "Delete this condition group query."),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([this]()
								{
									if (WeakTreeElement.IsValid())
									{
										const TSharedPtr<FCollectionCustomizationTreeElement> TreeElementPin = WeakTreeElement.Pin();
										
										TreeElementPin->ConditionGroup->RemoveQuery(TreeElementPin->Query);

										if (TreeElementPin->ParentElement.IsValid())
										{
											TreeElementPin->ParentElement.Pin()->ClearCachedChildren();
										}
										
										RefreshTreeView();
									}
								}),
								FCanExecuteAction()));

						return ViewOptions.MakeWidget();
					})
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(2.f)
				.FillWidth(1.f)
				[
					SNew(SMovieGraphCollectionTreeOpTypeWidget)
					.IsConditionGroup(false)
					.WeakTreeElement(WeakTreeElement)
				]
			];
	}
	
	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SMovieGraphCollectionTreeItem::GenerateQueryPropertyColumnWidget(const FName& ColumnName, const TSharedPtr<FCollectionCustomizationTreeElement>& TreeElement)
{
	if (!DetailBuilder.IsValid())
	{
		return SNullWidget::NullWidget;
	}
		
	// Get the property handle for the property within the query. This is needed in order to get the name/value widgets for it.
	const TSharedPtr<IPropertyHandle> QueryPropertyHandle =
		DetailBuilder.Pin()->AddObjectPropertyData({TreeElement->QueryProperty->GetOwnerUObject()}, TreeElement->QueryProperty->GetFName());
		
	if (!QueryPropertyHandle || !QueryPropertyHandle.IsValid())
	{
		return SNullWidget::NullWidget;
	}
		
	if (ColumnName == ColumnID_Name)
	{
		return
			SNew(SBorder)
			.Padding(0)
			.IsEnabled_Lambda([this]()
			{
				return WeakTreeElement.IsValid() ? WeakTreeElement.Pin()->IsQueryEnabled() : false;
			})
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				[
					SNew(SMovieGraphCollectionTreeViewIndent)
					.NumIndents(2)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.IndentAmount(0)
				]

				+ SHorizontalBox::Slot()
				[
					QueryPropertyHandle->CreatePropertyNameWidget()
				]
			];
	}

	if (ColumnName == ColumnID_Value)
	{
		return
			SNew(SBorder)
			.Padding(5.f, 2.f)
			.IsEnabled_Lambda([this]()
			{
				return WeakTreeElement.IsValid() ? WeakTreeElement.Pin()->IsQueryEnabled() : false;
			})
			.BorderImage(FMovieRenderPipelineStyle::Get().GetBrush("MovieRenderGraph.CollectionsTree.BottomLeftBorder"))
			[
				QueryPropertyHandle->CreatePropertyValueWidget()
			];
	}
		
	return SNullWidget::NullWidget;
}

void SMovieGraphCollectionTreeItem::RefreshTreeView() const
{
	if (WeakTreeView.IsValid())
	{
		WeakTreeView.Pin()->RefreshView();
	}
}

TSharedRef<SWidget> SMovieGraphCollectionTreeItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	const TSharedPtr<FCollectionCustomizationTreeElement> TreeElement = WeakTreeElement.Pin();
	if (!TreeElement.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	if (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::Root)
	{
		// The root element isn't visible, but serves as the single entry point to the tree's hierarchy
		return SNullWidget::NullWidget;
	}

	if (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::ConditionGroup)
	{
		return GenerateConditionGroupColumnWidget(ColumnName, TreeElement);
	}

	if (TreeElement->Type == FCollectionCustomizationTreeElement::EElementType::Query)
	{
		return GenerateConditionGroupQueryColumnWidget(ColumnName, TreeElement);
	}
		
	return GenerateQueryPropertyColumnWidget(ColumnName, TreeElement);
}

TSharedRef<IDetailCustomization> FMovieGraphCollectionsCustomization::MakeInstance()
{
	return MakeShared<FMovieGraphCollectionsCustomization>();
}

void FMovieGraphCollectionsCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UMovieGraphCollectionNode>> CollectionNodes =
		InDetailBuilder.GetObjectsOfTypeBeingCustomized<UMovieGraphCollectionNode>();
	if (CollectionNodes.Num() != 1)
	{
		// Showing more than one collection node is not supported
		return;
	}

	const TWeakObjectPtr<UMovieGraphCollectionNode> CollectionNode = CollectionNodes[0];
	if (!CollectionNode.IsValid())
	{
		return;
	}

	const TObjectPtr<UMovieGraphCollection> Collection = CollectionNode->Collection;

	// The Collection property is being completely replaced by a custom STreeView
	const TSharedRef<IPropertyHandle> CollectionProperty = InDetailBuilder.GetProperty("Collection");
	CollectionProperty->MarkHiddenByCustomization();

	IDetailCategoryBuilder& CollectionCategory = InDetailBuilder.EditCategory("Collection");
		
	// Display the "Add Condition Group" button
	CollectionCategory.AddCustomRow(FText())
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SPositiveActionButton)
			.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
			.Text(LOCTEXT("AddNewConditionGroup", "Condition Group"))
			.OnClicked_Lambda([this]
			{
				if (TreeView.IsValid())
				{
					TreeView->AddConditionGroup();
					TreeView->RefreshView();
				}

				return FReply::Handled();
			})
		]
	];

	// Add the collection name property
	CollectionCategory.AddExternalObjectProperty({Collection}, FName(TEXT("CollectionName")));

	FDetailWidgetRow& ConditionsWidgetRow = InDetailBuilder.EditCategory("Conditions")
		.AddCustomRow(FText::FromString(TEXT("Conditions")));
	
	ConditionsWidgetRow.WholeRowWidget
	[
		SNew(SBox)
		.VAlign(VAlign_Fill)
		.Padding(0)
		[
			SAssignNew(TreeView, SMovieGraphCollectionTreeView)
			.DetailBuilder(DetailBuilder)
			.Collection(MakeWeakObjectPtr(Collection))
		]
	];

	// Add the root elements to the tree; root elements will take care of adding children underneath them
	TreeView->AddRootElement();
}

void FMovieGraphCollectionsCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder)
{
	DetailBuilder = InDetailBuilder;
	CustomizeDetails(*InDetailBuilder);
}

#undef LOCTEXT_NAMESPACE

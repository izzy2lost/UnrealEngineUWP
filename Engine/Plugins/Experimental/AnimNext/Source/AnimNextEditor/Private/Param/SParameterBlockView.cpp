// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterBlockView.h"

#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Param/AnimNextParameterBlockEntry.h"
#include "DetailLayoutBuilder.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "EditorUtils.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "UncookedOnlyUtils.h"
#include "ParameterBlockViewMenuContext.h"
#include "PropertyBagDetails.h"
#include "SAddParametersDialog.h"
#include "SourceControlOperations.h"
#include "SPinTypeSelector.h"
#include "SSimpleButton.h"
#include "Framework/Commands/GenericCommands.h"
#include "Param/IAnimNextParameterBlockReferenceInterface.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Param/AnimNextParameterBlockBindingReference.h"
#include "ToolMenus.h"
#include "ScopedTransaction.h"
#include "SParameterPicker.h"
#include "SSimpleComboButton.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlockGraph.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "IStructureDataProvider.h"
#include "Misc/NotifyHook.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockView"

namespace UE::AnimNext::Editor
{

namespace ParameterBlockView
{

static FName ContextMenuName(TEXT("AnimNext.ParameterBlockView.ContextMenu"));
static FName Column_RevisionControl(TEXT("RevisionControl"));
static FName Column_ModifiedStatus(TEXT("ModifiedStatus"));
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName Column_Container(TEXT("Container"));
static FName Column_Value(TEXT("Value"));

}

// An entry category displayed in the parameters table
template<typename ItemType>
class SCategoryHeaderTableRow : public STableRow<ItemType>
{
public:
	SLATE_BEGIN_ARGS(SCategoryHeaderTableRow)
		{}
		SLATE_DEFAULT_SLOT(typename SCategoryHeaderTableRow::FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		STableRow<ItemType>::ChildSlot
			.Padding(0.0f, 2.0f, .0f, 0.0f)
			[
				SAssignNew(ContentBorder, SBorder)
					.BorderImage(this, &SCategoryHeaderTableRow::GetBackgroundImage)
					.Padding(FMargin(3.0f, 5.0f))
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(5.0f)
							.AutoWidth()
							[
								SNew(SExpanderArrow, STableRow< ItemType >::SharedThis(this))
							]
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								InArgs._Content.Widget
							]
					]
			];

		STableRow < ItemType >::ConstructInternal(
			typename STableRow< ItemType >::FArguments()
			.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
			InOwnerTableView
		);
	}

	const FSlateBrush* GetBackgroundImage() const
	{
		if (STableRow<ItemType>::IsHovered())
		{
			return FAppStyle::Get().GetBrush("Brushes.Secondary");
		}
		else
		{
			return FAppStyle::Get().GetBrush("Brushes.Header");
		}
	}

	virtual void SetContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

	virtual void SetRowContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

	virtual const FSlateBrush* GetBorder() const
	{
		return nullptr;
	}

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			STableRow<ItemType>::ToggleExpansion();
			return FReply::Handled();
		}
		else
		{
			return FReply::Unhandled();
		}
	}
private:
	TSharedPtr<SBorder> ContentBorder;
};

// An entry displayed in the parameters view
struct FParameterBlockViewEntry
{

	FParameterBlockViewEntry() = default;

	FParameterBlockViewEntry(UAnimNextParameterBlockEntry* InEntry)
		: WeakEntry(InEntry)
	{}

	FParameterBlockViewEntry(EParameterBlockCategoryType InCategoryType, const FText& InCategoryName)
		: CategoryType(InCategoryType)
		, CategoryName(InCategoryName)
	{}

	bool PassesFilter(const FString& InFilterText) const
	{
		if(UAnimNextParameterBlockEntry* Entry = WeakEntry.Get())
		{
			return Entry->GetDisplayName().ToString().Contains(InFilterText);
		}

		return false;
	}

	void ResetChildren(int32 NewSize = 0)
	{
		Children.Reset(NewSize);
	}

	void GetChildrenRecursive(TArray< TSharedRef<FParameterBlockViewEntry> >& OutChildren)
	{
		for (TSharedRef<FParameterBlockViewEntry>& Entry : Children)
		{
			OutChildren.Add(Entry);
			Entry->GetChildrenRecursive(OutChildren);
		}
	}

	// Ptr to the underlying entry
	TWeakObjectPtr<UAnimNextParameterBlockEntry> WeakEntry;

	// Widget used to rename items
	TWeakPtr<SInlineEditableTextBlock> NameWidget;

	EParameterBlockCategoryType CategoryType = EParameterBlockCategoryType::Invalid;
	FText CategoryName;

	// Children when entry is a category
	TArray< TSharedRef<FParameterBlockViewEntry> > Children;

	// Flag to indicate a rename was requested
	bool bRenameWhenScrolledIntoView = false;
};

void SParameterBlockView::Construct(const FArguments& InArgs, UAnimNextParameterBlock_EditorData* InEditorData)
{
	using namespace ParameterBlockView;
	
	check(InEditorData);

	EditorData = InEditorData;

	Categories = {
		MakeShared<FParameterBlockViewEntry>(EParameterBlockCategoryType::Parameter, LOCTEXT("ParameterBlockParameterCategory", "Parameters")),
		MakeShared<FParameterBlockViewEntry>(EParameterBlockCategoryType::Graph, LOCTEXT("ParameterBlockGraphCategory", "Graphs"))
	};

	// Cache asset data for block for comparisons/filtering
	BlockAssetData = FAssetData(UncookedOnly::FUtils::GetBlock(EditorData));

	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;
	OnOpenGraphDelegate = InArgs._OnOpenGraph;
	OnDeleteEntriesDelegate = InArgs._OnDeleteEntries;

	EditorData->ModifiedDelegate.AddSP(this, &SParameterBlockView::HandleBlockModified);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f)
			[
				SNew(SSearchBox)
				.OnTextChanged_Lambda([this](FText InText)
				{
					FilterText = InText;
					RefreshFilter();
				})
			]
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(EntriesList, STreeView<TSharedRef<FParameterBlockViewEntry>>)
			.TreeItemsSource(&FilteredEntries)
			.OnGenerateRow(this, &SParameterBlockView::HandleGenerateRow)
			.OnGetChildren(this, &SParameterBlockView::HandleGetChildren)
			.OnItemScrolledIntoView(this, &SParameterBlockView::HandleItemScrolledIntoView)
			.OnSelectionChanged(this, &SParameterBlockView::HandleSelectionChanged)
			.ItemHeight(20.0f)
			.HeaderRow(
				SNew(SHeaderRow)
				+SHeaderRow::Column(Column_RevisionControl)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Icon"))
						.ToolTipText(LOCTEXT("RevisionControlStatusTooltip", "Revision control status of this parameter"))
					]
				]

				+SHeaderRow::Column(Column_ModifiedStatus)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("ContentBrowser.ContentDirty"))
						.ToolTipText(LOCTEXT("ModifiedStatusTooltip", "Modified status of this parameter"))
					]
				]

				+SHeaderRow::Column(Column_Name)
				.DefaultLabel(LOCTEXT("NameColumnHeader", "Name"))
				.ToolTipText(LOCTEXT("NameColumnHeaderTooltip", "The name of the parameter"))
				.FillWidth(20.0f)

				+SHeaderRow::Column(Column_Type)
				.DefaultLabel(LOCTEXT("TypeColumnHeader", "Type"))
				.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The type of the parameter"))
				.ManualWidth(145.0f)

				+SHeaderRow::Column(Column_Value)
				.DefaultLabel(LOCTEXT("ValueColumnHeader", "Value"))
				.ToolTipText(LOCTEXT("ValueColumnHeaderTooltip", "The value or binding to the parameter"))
				.FillWidth(10.0f)

				+ SHeaderRow::Column(Column_Container)
				.DefaultLabel(LOCTEXT("ContainerColumnHeader", "Owner Block"))
				.ToolTipText(LOCTEXT("ContainerColumnHeaderTooltip", "The block containing the parameter"))
				.FillWidth(10.0f)
			)
		]
	];

	// Update revision control status of all our files
	if (ISourceControlModule::Get().IsEnabled())
	{
		TArray<UPackage*> Packages = EditorData->GetPackage()->GetExternalPackages();

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), Packages);
	}

	BindCommands();

	RegisterActiveTimer(1.0f / 60.0f, FWidgetActiveTimerDelegate::CreateLambda(
		[this](double InCurrentTime, float InDeltaTime)
		{
			if(bRefreshRequested)
			{
				RefreshEntries();
				bRefreshRequested = false;
			}

			return EActiveTimerReturnType::Continue;
		}));

	RefreshEntries();

	for (TSharedRef<FParameterBlockViewEntry>& Category : Categories)
	{
		EntriesList->SetItemExpansion(Category, true);
	}
}

FReply SParameterBlockView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SParameterBlockView::RequestRefresh()
{
	bRefreshRequested = true;
}

template<typename ItemType, typename ComparisonType>
void RestoreExpansionState(TSharedPtr< STreeView<ItemType> > InTree, const TArray<ItemType>& ItemSource, const TSet<ItemType>& OldExpansionState, ComparisonType ComparisonFunction)
{
	check(InTree.IsValid());

	// Iterate over new tree items
	for (int32 ItemIdx = 0; ItemIdx < ItemSource.Num(); ItemIdx++)
	{
		ItemType NewItem = ItemSource[ItemIdx];

		// Look through old expansion state
		for (typename TSet<ItemType>::TConstIterator OldExpansionIter(OldExpansionState); OldExpansionIter; ++OldExpansionIter)
		{
			const ItemType OldItem = *OldExpansionIter;
			// See if this matches this new item
			if (ComparisonFunction(OldItem, NewItem))
			{
				// It does, so expand it
				InTree->SetItemExpansion(NewItem, true);
			}
		}
	}
}

static bool CompareGraphActionNode(TSharedRef<FParameterBlockViewEntry> A, TSharedRef<FParameterBlockViewEntry> B)
{
	if (A->CategoryType != B->CategoryType)
	{
		return false;
	}

	// First check grouping is the same
	if (A->CategoryName.ToString() != B->CategoryName.ToString())
	{
		return false;
	}

	return true;
}

void SParameterBlockView::RefreshEntries()
{
	// First, save off current expansion state
	TSet< TSharedRef<FParameterBlockViewEntry> > OldExpansionState;
	EntriesList->GetExpandedItems(OldExpansionState);

	// Add empty categories, the filtering will add the elements to the correct one
	Entries.Reset();

	TArray<TSharedRef<FParameterBlockViewEntry>> EntriesToSelect;

	for (UAnimNextParameterBlockEntry* Entry : EditorData->Entries)
	{
		TSharedRef<FParameterBlockViewEntry> NewEntry = MakeShared<FParameterBlockViewEntry>(Entry);

		Entries.Add(NewEntry);

		if (PendingSelection.Contains(Entry))
		{
			EntriesToSelect.Add(NewEntry);
		}
	}

	// Restore the expanded items
	TArray< TSharedRef<FParameterBlockViewEntry> > AllEntries;
	for (const TSharedRef<FParameterBlockViewEntry> & Entry : Entries)
	{
		AllEntries.Add(Entry);
		Entry->GetChildrenRecursive(AllEntries);
	}
	RestoreExpansionState< TSharedRef<FParameterBlockViewEntry> >(EntriesList, AllEntries, OldExpansionState, CompareGraphActionNode);

	PendingSelection.Empty();

	RefreshFilter();

	if(EntriesToSelect.Num() > 0)
	{
		EntriesList->SetItemSelection(EntriesToSelect, true);
	}
}

void SParameterBlockView::RefreshFilter()
{
	FilteredEntries = Categories;

	for (auto& Category : Categories)
	{
		Category->ResetChildren();
	}

	TSharedRef<FParameterBlockViewEntry> ParametersCategory = GetCategory(EParameterBlockCategoryType::Parameter);
	TSharedRef<FParameterBlockViewEntry> GraphsCategory = GetCategory(EParameterBlockCategoryType::Graph);

	check(ParametersCategory->CategoryType == EParameterBlockCategoryType::Parameter);
	check(GraphsCategory->CategoryType == EParameterBlockCategoryType::Graph);

	// add the entries as categories children
	const FString FilterTextAsString = FilterText.ToString();
	for (const TSharedRef<FParameterBlockViewEntry>& Entry : Entries)
	{
		if (Entry->PassesFilter(FilterTextAsString))
		{
			const UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get();
			if (const IAnimNextParameterBlockParameterInterface* Parameter = Cast<IAnimNextParameterBlockParameterInterface>(BlockEntry))
			{
				ParametersCategory->Children.Add(Entry);
			}
			else if (Cast<IAnimNextParameterBlockGraphInterface>(BlockEntry))
			{
				GraphsCategory->Children.Add(Entry);
			}
		}
	}

	EntriesList->RequestListRefresh();
}

void SParameterBlockView::BindCommands()
{
	UICommandList = MakeShared<FUICommandList>();

	UICommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SParameterBlockView::HandleDelete),
		FCanExecuteAction::CreateSP(this, &SParameterBlockView::HasValidSelection));

	UICommandList->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SParameterBlockView::HandleRename),
		FCanExecuteAction::CreateSP(this, &SParameterBlockView::HasValidSingleSelection));
}

void SParameterBlockView::HandleBlockModified(UAnimNextParameterBlock_EditorData* InEditorData)
{
	check(InEditorData == EditorData);

	RequestRefresh();
}

TSharedRef<SWidget> SParameterBlockView::HandleGetContextContent()
{
	using namespace ParameterBlockView;
	
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ToolMenus->IsMenuRegistered(ContextMenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(ContextMenuName);

		FToolMenuSection& Section = Menu->AddSection("EntryOperations", LOCTEXT("EntryOperationsMenuSection", "Block Entry"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
	}

	UParameterBlockViewMenuContext* MenuContext = NewObject<UParameterBlockViewMenuContext>();
	MenuContext->ParameterBlockView = SharedThis(this);
	return ToolMenus->GenerateWidget(ContextMenuName, FToolMenuContext(MenuContext));
}

void SParameterBlockView::HandleDelete()
{
	auto DeleteSelected = [this](TArray<TSharedRef<FParameterBlockViewEntry>> SelectedItems)
	{
		TArray<UAnimNextParameterBlockEntry*> EntriesToRemove;
		Algo::Transform(SelectedItems, EntriesToRemove, [](const TSharedRef<FParameterBlockViewEntry>& InEntry) { return InEntry->WeakEntry.Get(); });

		OnDeleteEntriesDelegate.ExecuteIfBound(EntriesToRemove);

		{
			FScopedTransaction Transaction(FText::FormatOrdered(LOCTEXT("DeleteParameterBlockEntry", "Delete parameter block {0}|plural(one=entry,other=entries)"), EntriesList->GetNumItemsSelected()));
			EditorData->RemoveEntries(EntriesToRemove);
		}
	};

	if (EntriesList->GetNumItemsSelected() > 0)
	{
		TArray<TSharedRef<FParameterBlockViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		DeleteSelected(SelectedItems);
	}
}

void SParameterBlockView::HandleRename()
{
	auto RenameSelected = [this](TSharedPtr<STreeView<TSharedRef<FParameterBlockViewEntry>>>& InEntriesList, const TSharedRef<FParameterBlockViewEntry>& SelectedItem)
	{
		SelectedItem->bRenameWhenScrolledIntoView = true;
		InEntriesList->RequestScrollIntoView(SelectedItem);
	};

	if(EntriesList->GetNumItemsSelected() == 1)
	{
		TArray<TSharedRef<FParameterBlockViewEntry>> SelectedItems = EntriesList->GetSelectedItems();
		RenameSelected(EntriesList, SelectedItems[0]);
	}
}

bool SParameterBlockView::HasValidSelection() const
{
	return EntriesList->GetNumItemsSelected() > 0;
}

bool SParameterBlockView::HasValidSingleSelection() const
{
	return EntriesList->GetNumItemsSelected() == 1;
}

void SParameterBlockView::HandleItemScrolledIntoView(TSharedRef<FParameterBlockViewEntry> InEntry, const TSharedPtr<ITableRow>& InWidget)
{
	if(InEntry->bRenameWhenScrolledIntoView)
	{
		InEntry->bRenameWhenScrolledIntoView = false;

		if(TSharedPtr<SInlineEditableTextBlock> NameWidget = InEntry->NameWidget.Pin())
		{
			NameWidget->EnterEditingMode();
		}
	}
}

class SParameterBlockViewRow : public SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>, private FNotifyHook
{
	SLATE_BEGIN_ARGS(SParameterBlockViewRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<SParameterBlockView> InView, TSharedRef<FParameterBlockViewEntry> InEntry)
	{
		WeakView = InView;
		Entry = InEntry;

		SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>::FArguments(), InOwnerTableView);
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if(IAnimNextParameterBlockGraphInterface* GraphInterface = Cast<IAnimNextParameterBlockGraphInterface>(Entry->WeakEntry.Get()))
		{
			if(TSharedPtr<SParameterBlockView> View = WeakView.Pin())
			{
				View->OnOpenGraphDelegate.ExecuteIfBound(GraphInterface->GetGraph());
			}
			return FReply::Handled();
		}

		return SMultiColumnTableRow<TSharedRef<FParameterBlockViewEntry>>::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	}
	
	//~ Begin FNotifyHook Interface
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override
	{
		if (TSharedPtr<SParameterBlockView> View = WeakView.Pin())
		{
			if (UAnimNextParameterBlock_EditorData* EditorData = View->EditorData)
			{
				if (UAnimNextParameterBlockEntry * BlockEntry = EditorData->FindBinding(PropertyAboutToChange->GetFName()))
				{
					UAnimNextParameterBlock* ReferencedBlock = UE::AnimNext::UncookedOnly::FUtils::GetBlock(EditorData);
					ReferencedBlock->Modify(); // needed to enable the transaction when we modify the PropertyBag
				}
			}
		}
	}

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override
	{
		if (TSharedPtr<SParameterBlockView> View = WeakView.Pin())
		{
			if (UAnimNextParameterBlock_EditorData* EditorData = View->EditorData)
			{
				if (UAnimNextParameterBlockEntry* BlockEntry = EditorData->FindBinding(PropertyChangedEvent.GetMemberPropertyName()))
				{
					BlockEntry->MarkPackageDirty(); // needed to show the changed sign a the table when we modify the PropertyBag
				}
			}
		}
	}
	// End of FNotifyHook

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterBlockView;
		
		if(InColumnName == Column_RevisionControl)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(20.0f)
				[
					SNew(SImage)
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							UPackage* Package = BlockEntry->GetExternalPackage();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(Package, EStateCacheUsage::Use))
							{
								return FText::Format(LOCTEXT("RevisionControlStatusFormat", "File: {0}\nStatus: {1}"), FText::FromName(Package->GetFName()),  State->GetDisplayTooltip());
							}
						}

						return LOCTEXT("RevisionControlStatus", "Revision control status of this parameter");
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							if(FSourceControlStatePtr State = SourceControlProvider.GetState(BlockEntry->GetExternalPackage(), EStateCacheUsage::Use))
							{
								return State->GetIcon().GetSmallIcon();
							}
						}
						return nullptr;
					})
				];
		}
		else if(InColumnName == Column_ModifiedStatus)
		{
			return
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.HeightOverride(20.0f)
				[
					SNew(SImage)
					.ToolTipText_Lambda([this]()
					{
						FTextBuilder TextBuilder;
						TextBuilder.AppendLine(LOCTEXT("ModifiedTooltip", "Modified Status"));

						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							const UPackage* ExternalPackage = BlockEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								TextBuilder.AppendLine(FText::FromName(ExternalPackage->GetFName()));
							}
						}

						return TextBuilder.ToText();
					})
					.Image_Lambda([this]() -> const FSlateBrush*
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							bool bIsDirty = false;
							const UPackage* ExternalPackage = BlockEntry->GetExternalPackage();
							check(ExternalPackage);
							if(ExternalPackage->IsDirty())
							{
								bIsDirty = true;
							}


							return bIsDirty ? FAppStyle::GetBrush("ContentBrowser.ContentDirty") : nullptr;
						}
						return nullptr;
					})
				];
		}
		else if(InColumnName == Column_Type)
		{
			if(Entry->WeakEntry.Get()->Implements<UAnimNextParameterBlockParameterInterface>())
			{
				return
					SNew(SBox)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SPinTypeSelector, FGetPinTypeTree::CreateStatic(&Editor::FUtils::GetFilteredVariableTypeTree))
							.TargetPinType_Lambda([this]()
							{
								if(const IAnimNextParameterBlockParameterInterface* Binding = Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get()))
								{
									return UncookedOnly::FUtils::GetPinTypeFromParamType(Binding->GetParamType());
								}

								return FEdGraphPinType();
							})
							.OnPinTypeChanged_Lambda([this](const FEdGraphPinType& PinType)
							{
								if(IAnimNextParameterBlockParameterInterface* Binding = Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get()))
								{
									const FAnimNextParamType ParamType = UncookedOnly::FUtils::GetParamTypeFromPinType(PinType);
									if(ParamType.IsValid())
									{
										Binding->SetParamType(ParamType);
									}
								}
							})
							.Schema(GetDefault<UPropertyBagSchema>())
							.bAllowArrays(true)
							.TypeTreeFilter(ETypeTreeFilter::None)
							.Font(IDetailLayoutBuilder::GetDetailFont())
					];
			}
		}
		else if(InColumnName == Column_Name)
		{
			return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.Visibility_Lambda([this]() { return DoesItemHaveChildren() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(Entry->NameWidget, SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SParameterBlockViewRow::IsSelectedExclusively)
					.IsReadOnly_Lambda([this]()
					{
						if (Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get()) 
							|| Cast<IAnimNextParameterBlockGraphInterface>(Entry->WeakEntry.Get()))
						{
							return false;
						}
						return true;
					})
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						if(InCommitType == ETextCommit::OnEnter)
						{
							if (IAnimNextParameterBlockParameterInterface* Parameter = Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get()))
							{
								FScopedTransaction Transaction(LOCTEXT("SetParameterName", "Set Parameter name"));

								Parameter->SetParameterName(*InNewText.ToString());
							}
							else if(IAnimNextParameterBlockGraphInterface* Graph = Cast<IAnimNextParameterBlockGraphInterface>(Entry->WeakEntry.Get()))
							{
								FScopedTransaction Transaction(LOCTEXT("SetGraphName", "Set graph name"));

								Graph->SetGraphName(*InNewText.ToString());
							}
						}
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						if(IAnimNextParameterBlockGraphInterface* Binding = Cast<IAnimNextParameterBlockGraphInterface>(Entry->WeakEntry.Get()))
						{
							// Make sure the new name only contains valid characters
							if (!FName::IsValidXName(NewString, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorText))
							{
								return false;
							}
						}

						if(const IAnimNextParameterBlockReferenceInterface* Reference = Cast<IAnimNextParameterBlockReferenceInterface>(Entry->WeakEntry.Get()))
						{
							const FName Name(*NewString);
							if(!FUtils::DoesParameterNameExistInAsset(Name, Reference->GetBlock()))
							{
								OutErrorText = LOCTEXT("Error_NameDoesNotExist", "This name does not exist in the specified block");
								return false;
							}
						}

						return true;
					})
					.Text_Lambda([this]()
					{
						if(IAnimNextParameterBlockParameterInterface* Parameter = Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get()))
						{
							return UncookedOnly::FUtils::GetParameterDisplayNameText(Parameter->GetParameterName());
						}

						if(IAnimNextParameterBlockGraphInterface* Graph = Cast<IAnimNextParameterBlockGraphInterface>(Entry->WeakEntry.Get()))
						{
							return FText::FromName(Graph->GetGraphName());
						}
						return FText::GetEmpty();
					})
					.ToolTipText_Lambda([this]()
					{
						if(UAnimNextParameterBlockEntry* BlockEntry = Entry->WeakEntry.Get())
						{
							return BlockEntry->GetDisplayNameTooltip();
						}
						return FText::GetEmpty();
					})
				];
		}
		else if(InColumnName == Column_Value)
		{
			TSharedRef< SWidget > ColumnWidget = SNullWidget::NullWidget;

			if (TSharedPtr<SParameterBlockView> View = WeakView.Pin())
			{
				const IAnimNextParameterBlockParameterInterface* Binding = Cast<IAnimNextParameterBlockParameterInterface>(Entry->WeakEntry.Get());
				if (Binding != nullptr)
				{
					if (UAnimNextParameterBlock_EditorData* EditorData = View->EditorData)
					{
						UAnimNextParameterBlock* ReferencedBlock = UE::AnimNext::UncookedOnly::FUtils::GetBlock(EditorData);

						FInstancedPropertyBag &PropertyBag = ReferencedBlock->GetPropertyBag();
						const FName ParameterName = Binding->GetParameterName();

						if (PropertyBag.FindPropertyDescByName(ParameterName))
						{
							FSinglePropertyParams SinglePropertyArgs;
							SinglePropertyArgs.NamePlacement = EPropertyNamePlacement::Hidden;
							SinglePropertyArgs.NotifyHook = this;

							FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
							const TSharedPtr<ISinglePropertyView> SingleStructPropertyView = PropertyEditorModule.CreateSingleProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(PropertyBag), ParameterName, SinglePropertyArgs);
							if (SingleStructPropertyView.IsValid())
							{
								ColumnWidget = SingleStructPropertyView.ToSharedRef();
							}
						}
					}
				}
			}

			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					ColumnWidget
				];
		}
		else if (InColumnName == Column_Container)
		{
			TSharedPtr<SHorizontalBox> HorizontalBox;

			TSharedRef<SWidget> Widget =
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(HorizontalBox, SHorizontalBox)
				];

			if (TSharedPtr<SParameterBlockView> View = WeakView.Pin())
			{
				if (UAnimNextParameterBlock_EditorData* EditorData = View->EditorData)
				{
					const UAnimNextParameterBlock* ReferencedBlock = UE::AnimNext::UncookedOnly::FUtils::GetBlock(EditorData);

					HorizontalBox->AddSlot()
					[
						SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text_Lambda([WeakBlock = TWeakObjectPtr<const UAnimNextParameterBlock>(ReferencedBlock)]()
								{
									if (const UAnimNextParameterBlock* Block = WeakBlock.Get())
									{
										return FText::FromString(Block->GetName());
									}

									return LOCTEXT("MissingParameterBlock", "Missing Parameter Block");
								})
							.ToolTipText_Lambda([WeakBlock = TWeakObjectPtr<const UAnimNextParameterBlock>(ReferencedBlock)]()
								{
									if (const UAnimNextParameterBlock* Block = WeakBlock.Get())
									{
										return FText::FromString(Block->GetPathName());
									}

									return LOCTEXT("MissingParameterBlock", "Missing Parameter Block");
								})
					];
				}
			}

			return Widget;
		}

		return SNullWidget::NullWidget;
	}

	TWeakPtr<SParameterBlockView> WeakView;
	TSharedPtr<FParameterBlockViewEntry> Entry;
};

TSharedRef<ITableRow> SParameterBlockView::HandleGenerateRow(TSharedRef<FParameterBlockViewEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	if (InEntry->CategoryType != EParameterBlockCategoryType::Invalid)
	{
		TSharedPtr< STableRow< TSharedPtr<FParameterBlockViewEntry> > > TableRow;
		TableRow = SNew(SCategoryHeaderTableRow< TSharedPtr<FParameterBlockViewEntry> >, InOwnerTable);

		TSharedPtr<SHorizontalBox> RowContainer;
		TableRow->SetRowContent
		(
			SAssignNew(RowContainer, SHorizontalBox)
		);

		const FMargin RowPadding = FMargin(0, 2);
		TSharedPtr<SHorizontalBox> RowContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.Text(InEntry->CategoryName)
			];

		RowContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				SNew(SExpanderArrow, TableRow)
					.BaseIndentLevel(1)
			];

		RowContainer->AddSlot()
			.FillWidth(1.0)
			.Padding(RowPadding)
			[
				RowContent.ToSharedRef()
			];


		if (InEntry->CategoryType == EParameterBlockCategoryType::Parameter)
		{
			RowContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				SNew(SSimpleComboButton)
				.Text(LOCTEXT("AddParameterButton", "Add Parameter"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.HasDownArrow(true)
				.OnGetMenuContent_Lambda([this]()
				{
					FParameterPickerArgs Args;
					Args.bMultiSelect = false;
					Args.bShowBlocks = false;
					Args.bShowBoundParameters = false;
					Args.bShowBuiltInParameters = false; // Built-In paameters Disabled for MVP
				Args.OnFilterParameter = FOnFilterParameter::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
					{
						// Skip params that are already bound in this block
						if(InParameterBinding.Block == BlockAssetData)
						{
							return EFilterParameterResult::Exclude;
						}
						
						return EFilterParameterResult::Include;
					});

					Args.OnAddParameter = FOnAddParameter::CreateLambda([this](const FParameterToAdd& ParameterToAdd)
					{
						FSlateApplication::Get().DismissAllMenus();

						check(EditorData->FindBinding(ParameterToAdd.Name) == nullptr);
						FScopedTransaction Transaction(LOCTEXT("AddParameter", "Add parameter"));
						UAnimNextParameterBlockParameter* Parameter = EditorData->AddParameter(ParameterToAdd.Name, ParameterToAdd.Type);

						PendingSelection.Empty();
						PendingSelection.Add(Parameter);
					});
					Args.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
					{
						FSlateApplication::Get().DismissAllMenus();			
						PendingSelection.Empty();
						UAnimNextParameterBlockEntry* Binding = EditorData->FindBinding(InParameterBinding.Parameter);
						if (Binding == nullptr)
						{
							const FAnimNextParamType Type = UE::AnimNext::UncookedOnly::FUtils::GetParameterTypeFromName(InParameterBinding.Parameter);
							if (Type.IsValid())
							{
								Binding = EditorData->AddParameter(InParameterBinding.Parameter, Type);
							};
							PendingSelection.Add(Binding);	
						}
					});
					
					return SNew(SParameterPicker)
						.Args(Args);
				})
			];
		}
		else if (InEntry->CategoryType == EParameterBlockCategoryType::Graph)
		{
			RowContainer->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Right)
			[
				SNew(SSimpleButton)
				.Text(LOCTEXT("AddGraphButton", "Add Graph"))
				.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
				.OnClicked_Lambda([this]()
				{
					FScopedTransaction Transaction(LOCTEXT("AddGraph", "Add Graph"));

					PendingSelection.Empty();

					// Create a new entry for the graph
					UAnimNextParameterBlockGraph* Graph = EditorData->AddGraph(TEXT("NewGraph"));
					PendingSelection.Add(Graph);

					return FReply::Handled();
				})
			];
		}

		return TableRow.ToSharedRef();
	}
	else
	{
		return SNew(SParameterBlockViewRow, InOwnerTable, SharedThis(this), InEntry);
	}
}

void SParameterBlockView::HandleGetChildren(TSharedRef<FParameterBlockViewEntry> InEntry, TArray<TSharedRef<FParameterBlockViewEntry>>& OutChildren)
{
	if (InEntry->CategoryType != EParameterBlockCategoryType::Invalid)
	{
		OutChildren = InEntry->Children;
	}
}

void SParameterBlockView::HandleSelectionChanged(TSharedPtr<FParameterBlockViewEntry> InEntry, ESelectInfo::Type InSelectionType)
{
	if(OnSelectionChangedDelegate.IsBound())
	{
		TArray<UObject*> SelectedItems;
		SelectedItems.Reserve(EntriesList->GetNumItemsSelected());
		for(const TSharedRef<FParameterBlockViewEntry>& SelectedItem : EntriesList->GetSelectedItems())
		{
			if(UAnimNextParameterBlockEntry* Entry = SelectedItem->WeakEntry.Get())
			{
				SelectedItems.Add(Entry);
			}
		}
		OnSelectionChangedDelegate.Execute(SelectedItems);
	}
}

EFilterParameterResult SParameterBlockView::HandleFilterLinkedParameter(const FParameterBindingReference& InParameterBinding)
{
	// Don't display parameters that are already bound in this block
	if(InParameterBinding.Block.IsValid())
	{
		if(BlockAssetData == InParameterBinding.Block)
		{
			return EFilterParameterResult::Exclude;
		}
	}

	return EFilterParameterResult::Include;
}

TSharedRef<FParameterBlockViewEntry> SParameterBlockView::GetCategory(EParameterBlockCategoryType CategoryType)
{
	static TSharedRef<FParameterBlockViewEntry> InvalidCategory = MakeShared<FParameterBlockViewEntry>(EParameterBlockCategoryType::Invalid, LOCTEXT("ParameterBlockInvalidCategory", "Parameters"));
	
	for (TSharedRef<FParameterBlockViewEntry>& Category : Categories)
	{
		if (Category->CategoryType == CategoryType)
		{
			return Category;
		}
	}

	return TSharedRef<FParameterBlockViewEntry>();
}

}

#undef LOCTEXT_NAMESPACE
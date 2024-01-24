// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyTreeView.h"

#include "Filters/SBasicFilterBar.h"
#include "PropertyFilter_ByPropertyType.h"
#include "PropertyFrontendFilter.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/View/DisplayUtils.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertiesView"

namespace UE::ConcertSharedSlate
{
	/** Exposes SetFrontendFilterActive so we can manually enable the default filters */
	class SReplicationFilterBar : public SBasicFilterBar<TSharedPtr<FReplicatedPropertyData>>
	{
		using Super = SBasicFilterBar<TSharedPtr<FReplicatedPropertyData>>;
	public:

		SLATE_BEGIN_ARGS(SReplicationFilterBar)
		{}
			SLATE_EVENT(FOnFilterChanged, OnFilterChanged)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TArray<SPropertyTreeView::FFilterRef> AllFilters)
		{
			Super::Construct(
			Super::FArguments()
				.FilterPillStyle(EFilterPillStyle::Basic)
				.CustomFilters(MoveTemp(AllFilters))
				.OnFilterChanged(InArgs._OnFilterChanged)
				.UseSectionsForCategories(true)
				);
		}

		// Expose from SBasicFilterBar
		using Super::SetFrontendFilterActive;

		void SetFilterVisuallyEnabled(const SPropertyTreeView::FFilterRef& Filter, bool bEnabled)
		{
			const TSharedRef<SFilter>* FilterWidget = Filters.FindByPredicate([&Filter](const TSharedRef<SFilter>& FilterWidget)
			{
				return FilterWidget->GetFrontendFilter() == Filter;
			});
			if (ensure(FilterWidget))
			{
				FilterWidget->Get().SetEnabled(bEnabled);
			}
		}
	};
	
	void SPropertyTreeView::Construct(const FArguments& InArgs)
	{
		SelectedObjectsAttribute = InArgs._SelectedObjects;
		NameModel = InArgs._NameModel;
		
		const FBuildFilterBarResult Filters = BuildFilterBar();
		ChildSlot
		[
			SAssignNew(ReplicatedProperties, SReplicationTreeView<FReplicatedPropertyData>)
				.RootItemsSource(InArgs._RootItemsSource)
				.OnGetChildren(InArgs._OnGetChildren)
				.Columns(InArgs._Columns)
				.ExpandableColumnLabel(InArgs._ExpandableColumnLabel)
				.PrimarySort(InArgs._PrimarySort)
				.SecondarySort(InArgs._SecondarySort)
				.SelectionMode(InArgs._SelectionMode)
				.FilterItem(this, &SPropertyTreeView::PassesFilters)
				.LeftOfSearchBar()
				[
					SNew(SHorizontalBox)

					// The combo button for selecting the property filters
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SBasicFilterBar<TSharedPtr<FReplicatedPropertyData>>::MakeAddFilterButton(FilterBar.ToSharedRef())
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						InArgs._LeftOfSearchBar.Widget
					]
				]
				.RightOfSearchBar() [ InArgs._RightOfSearchBar.Widget ]
				.RowBelowSearchBar() [ FilterBar.ToSharedRef() ]
				.NoItemsContent() [ SNew(STextBlock).Text(this, &SPropertyTreeView::GetAllFilteredText) ]
		];

		// For better UX, hide certain properties by default (e.g. why would you want to replicate bools?)
		// Do this AFTER initializing ReplicatedProperties because it triggers the OnItemsChanged callback.
		for (const FFilterRef& Filter : Filters.DisabledByDefault)
		{
			FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Unchecked);
		}
		// Show all the other filters as enabled (not greyed out: blue) - they will not run their logic since they are inverse.
		for (const FFilterRef& Filter : Filters.EnabledByDefault)
		{
			// Makes it appear on the bar
			FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Checked);
			// Functionally makes it affect the search
			FilterBar->SetFrontendFilterActive(Filter, true);
			// Visually makes the button blue (i.e. so it looks enabled)
			FilterBar->SetFilterVisuallyEnabled(Filter, true);
		}
	}

	void SPropertyTreeView::OnItemsChanged() const
	{
		ReplicatedProperties->OnItemsChanged();
	}

	void SPropertyTreeView::RequestResortForColumn(const FName& ColumnId)
	{
		ReplicatedProperties->RequestResortForColumn(ColumnId);
	}

	SPropertyTreeView::FBuildFilterBarResult SPropertyTreeView::BuildFilterBar()
	{
		TSharedRef<FFilterCategory> CommonCategory = MakeShared<FFilterCategory>(
			LOCTEXT("CommonCategory.Name", "Common"),
			LOCTEXT("CommonCategory.ToolTip", "Include commonly replicated properties.")
		);
		TSharedRef<FFilterCategory> UncommonCategory = MakeShared<FFilterCategory>(
			LOCTEXT("UncommonCategory.Advanced", "Uncommon"),
			LOCTEXT("UncommonCategory.ToolTip", "Include uncommonly replicated properties.")
		);

		FBuildFilterBarResult Result;
		using FFrontendFilter = TPropertyFrontendFilter<FPropertyFilter_ByPropertyType>;
		// Do not show under search bar
		Result.DisabledByDefault =
		{
			// Ordering matters for the drop-down menu next to settings
			// Common
			MakeShared<FFrontendFilter>(MoveTemp(CommonCategory), LOCTEXT("Ints", "Integer"), LOCTEXT("Ints.Tooltip", "Includes: uint16, uint32, uint64, int16, int32, int64"), TSet<FFieldClass*>{ FUInt16Property::StaticClass(), FUInt32Property::StaticClass(), FUInt64Property::StaticClass(), FInt16Property::StaticClass(), FIntProperty::StaticClass(), FInt64Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(CommonCategory), LOCTEXT("Floats", "Float"), LOCTEXT("Floats.Tooltip", "Includes: float, double"), TSet<FFieldClass*>{ FFloatProperty::StaticClass(), FDoubleProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(CommonCategory), LOCTEXT("Struct", "Struct"), TSet<FFieldClass*>{ FStructProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(CommonCategory), LOCTEXT("Containers", "Containers"), LOCTEXT("Containers.Tooltip", "Includes: array, set, map"), TSet<FFieldClass*>{ FArrayProperty::StaticClass(), FSetProperty::StaticClass(), FMapProperty::StaticClass() }),

			// Uncommon
			MakeShared<FFrontendFilter>(MoveTemp(UncommonCategory), LOCTEXT("Bool", "Boolean"), TSet<FFieldClass*>{ FBoolProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(UncommonCategory), LOCTEXT("Enum", "Enum"), TSet<FFieldClass*>{ FEnumProperty::StaticClass(), FByteProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(UncommonCategory), LOCTEXT("Text", "Text"), LOCTEXT("Text.Tooltip", "Includes: FName, FString, FText"), TSet<FFieldClass*>{ FNameProperty::StaticClass(), FStrProperty::StaticClass(), FTextProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(UncommonCategory), LOCTEXT("SoftPtr", "Soft Ptr"), TSet<FFieldClass*>{ FSoftObjectProperty::StaticClass() }),
		};
		// Show up under search bar as enabled
		Result.EnabledByDefault =
		{
			// We'll not enable any filters by default because that's the default for other places in the engine, like the Content Browser, too
		};
		// Show up in menu
		TArray<FFilterRef> AllFilters;
		AllFilters.Append(Result.EnabledByDefault);
		AllFilters.Append(Result.DisabledByDefault);
		
		FilterBar = SNew(SReplicationFilterBar, MoveTemp(AllFilters))
			.OnFilterChanged(this, &SPropertyTreeView::OnItemsChanged);
		
		return Result;
	}

	bool SPropertyTreeView::PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const
	{
		return FilterBar->GetAllActiveFilters()->Num() == 0 // Return all items when none enabled
			|| PassesAnyFilters(ReplicatedPropertyData);
	}

	bool SPropertyTreeView::PassesAnyFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const
	{
		TSharedPtr<TFilterCollection<TSharedPtr<FReplicatedPropertyData>>> FilterCollection = FilterBar->GetAllActiveFilters();
		for (int32 Index = 0; Index < FilterCollection->Num(); Index++)
		{
			if (FilterCollection->GetFilterAtIndex(Index)->PassesFilter(ReplicatedPropertyData))
			{
				return true;
			}
		}
		return false;
	}

	FText SPropertyTreeView::GetAllFilteredText() const
	{
		const TArray<FSoftObjectPath> Objects = SelectedObjectsAttribute.Get();
		if (Objects.IsEmpty())
		{
			return FText::GetEmpty();
		}

		TSet<FString> Names;
		Algo::Transform(Objects, Names, [this](const FSoftObjectPath& ObjectPath)
		{
			return DisplayUtils::GetObjectDisplayText(ObjectPath, NameModel).ToString();
		});
		
		const FText ObjectsText = FText::FromString(FString::Join(Names, TEXT(", ")));
		return FText::Format(LOCTEXT("AllFitleredFmt", "All properties filtered for selected {0}|plural(one=object,other=objects): {1}."),
			Objects.Num(),
			ObjectsText
		);
	}
}

#undef LOCTEXT_NAMESPACE
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
			FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Checked);
		}
		// Our filters are inverse (FFilterBase::IsInverseFilter) so that means these disabled filters will be active now (so e.g. bool will be filtered out by default now)
		FilterBar->DisableAllFilters();
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
		TSharedRef<FFilterCategory> GeneralCategory = MakeShared<FFilterCategory>(
			LOCTEXT("CommonCategory.Name", "General"),
			LOCTEXT("CommonCategory.ToolTip", "Exclude general properties.")
		);
		TSharedRef<FFilterCategory> NumericCategory = MakeShared<FFilterCategory>(
			LOCTEXT("NumericCategory.Name", "Numeric"),
			LOCTEXT("NumericCategory.ToolTip", "Excludes numeric properties.")
		);
		TSharedRef<FFilterCategory> ContainersCategory = MakeShared<FFilterCategory>(
			LOCTEXT("ContainersCategory.Name", "Containers"),
			LOCTEXT("ContainersCategory.ToolTip", "Exclude container properties.")
		);

		FBuildFilterBarResult Result;
		using FFrontendFilter = TPropertyFrontendFilter<FPropertyFilter_ByPropertyType>;
		Result.DisabledByDefault =
		{
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("Bool", "Boolean"), TSet<FFieldClass*>{ FBoolProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Byte", "Byte"), TSet<FFieldClass*>{ FByteProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("Name", "Name"), TSet<FFieldClass*>{ FNameProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("String", "String"), TSet<FFieldClass*>{ FStrProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("Enum", "Enum"), TSet<FFieldClass*>{ FEnumProperty::StaticClass() })
		};
		Result.EnabledByDefault =
		{
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Ints", "Integers"), LOCTEXT("Ints.Tooltip", "Includes integer types: uint16, uint32, uint64, int16, int32, int64"), TSet<FFieldClass*>{ FUInt16Property::StaticClass(), FUInt32Property::StaticClass(), FUInt64Property::StaticClass(), FInt16Property::StaticClass(), FIntProperty::StaticClass(), FInt64Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Float", "Float"), TSet<FFieldClass*>{ FFloatProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Double", "Double"), TSet<FFieldClass*>{ FDoubleProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("Struct", "Struct"), TSet<FFieldClass*>{ FStructProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(ContainersCategory), LOCTEXT("Array", "Array"), TSet<FFieldClass*>{ FArrayProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(ContainersCategory), LOCTEXT("Set", "Set"), TSet<FFieldClass*>{ FSetProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(ContainersCategory), LOCTEXT("Map", "Map"), TSet<FFieldClass*>{ FMapProperty::StaticClass() }),
		};
		
		TArray<FFilterRef> AllFilters =
		{
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("UInt16", "UInt16"), TSet<FFieldClass*>{ FUInt16Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("UInt32", "UInt32"), TSet<FFieldClass*>{ FUInt32Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("UInt64", "UInt64"), TSet<FFieldClass*>{ FUInt64Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Int16", "Int16"), TSet<FFieldClass*>{ FInt16Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Int32", "Int32"), TSet<FFieldClass*>{ FIntProperty::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(NumericCategory), LOCTEXT("Int64", "Int64"), TSet<FFieldClass*>{ FInt64Property::StaticClass() }),
			MakeShared<FFrontendFilter>(MoveTemp(GeneralCategory), LOCTEXT("SoftPtr", "Soft Ptr"), TSet<FFieldClass*>{ FSoftObjectProperty::StaticClass() }),
		};
		AllFilters.Append(Result.DisabledByDefault);
		AllFilters.Append(Result.EnabledByDefault);
		
		FilterBar = SNew(SReplicationFilterBar, MoveTemp(AllFilters))
			.OnFilterChanged(this, &SPropertyTreeView::OnItemsChanged);
		
		return Result;
	}

	bool SPropertyTreeView::PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const
	{
		return FilterBar->GetAllActiveFilters()->PassesAllFilters(ReplicatedPropertyData);
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
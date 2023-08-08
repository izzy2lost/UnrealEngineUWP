// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicatedPropertiesView.h"

#include "Filters/SBasicFilterBar.h"
#include "PropertyFilter_ByPropertyType.h"
#include "PropertyFrontendFilter.h"
#include "StreamEditor/View/ObjectViewer/ReplicatedPropertyData.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "SReplicatedPropertiesView"

namespace UE::MultiUserReplicationEditor
{
	void SReplicatedPropertiesView::Construct(const FArguments& InArgs)
	{
		const FBuildFilterBarResult Filters = BuildFilterBar();
		
		ChildSlot
		[
			SAssignNew(ReplicatedProperties, SReplicationTreeView<TSharedPtr<FReplicatedPropertyData>>)
				.RootItemsSource(InArgs._RootItemsSource)
				.OnGetChildren(InArgs._OnGetChildren)
				.Columns(InArgs._Columns)
				.ExpandableColumnLabel(InArgs._ExpandableColumnLabel)
				.SelectionMode(InArgs._SelectionMode)
				.FilterItem(this, &SReplicatedPropertiesView::PassesFilters)
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
				.RowBelowSearchBar()
				[
					FilterBar.ToSharedRef()
				]
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
			FilterBar->SetFilterCheckState(Filter, ECheckBoxState::Checked);
		}
	}

	void SReplicatedPropertiesView::OnItemsChanged() const
	{
		ReplicatedProperties->OnItemsChanged();
	}

	SReplicatedPropertiesView::FBuildFilterBarResult SReplicatedPropertiesView::BuildFilterBar()
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
		
		FilterBar = SNew(SBasicFilterBar<TSharedPtr<FReplicatedPropertyData>>)
			.FilterPillStyle(EFilterPillStyle::Basic)
			.CustomFilters(MoveTemp(AllFilters))
			.OnFilterChanged(this, &SReplicatedPropertiesView::OnItemsChanged)
			.UseSectionsForCategories(true);
		
		return Result;
	}

	bool SReplicatedPropertiesView::PassesFilters(const TSharedPtr<FReplicatedPropertyData>& ReplicatedPropertyData) const
	{
		return FilterBar->GetAllActiveFilters()->PassesAllFilters(ReplicatedPropertyData);
	}
}

#undef LOCTEXT_NAMESPACE
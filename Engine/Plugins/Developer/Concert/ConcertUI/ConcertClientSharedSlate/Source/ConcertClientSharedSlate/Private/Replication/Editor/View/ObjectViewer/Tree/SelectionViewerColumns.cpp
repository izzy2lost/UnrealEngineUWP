// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectionViewerColumns.h"

#include "Replication/Editor/Model/DisplayUtils.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/ObjectEditor/SBaseReplicationStreamEditor.h"
#include "Replication/PropertyChainUtils.h"

#include "Internationalization/Internationalization.h"
#include "GameFramework/Actor.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "ReplicationObjectColumns"

namespace UE::ConcertClientSharedSlate::ReplicationObjectColumns
{
	const FName IconColumnId = TEXT("IconColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");

	FReplicationObjectColumn IconColumn(TSharedRef<IObjectToPropertiesModel> Model, const float ColumnWidth)
	{
		return FReplicationObjectColumn(
			FReplicationObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([Model](const FReplicationObjectColumn::FBuildArgs& Args)
				{
					return SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(DisplayUtils::GetObjectIcon(*Model, Args.RowData.GetObjectPath()).GetOptionalIcon())
						];
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationColumnOrder::Icon)),
			SHeaderRow::Column(IconColumnId)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(ColumnWidth)
			);
	}
	
	FReplicationObjectColumn LabelColumn()
	{
		return FReplicationObjectColumn(
			FReplicationObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationObjectColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetObjectDisplayText(Args.RowData.GetObjectPath()));
				})
				.PopulateSearchItems_Lambda([](const FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectDisplayText(ObjectData.GetObjectPath()).ToString());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationColumnOrder::Label)),
			SHeaderRow::Column(LabelColumnId)
				.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
				.FillWidth(1.f)
			);
	}
	
	FReplicationObjectColumn TypeColumn(TSharedRef<IObjectToPropertiesModel> Model)
	{
		return FReplicationObjectColumn(
			FReplicationObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([Model](const FReplicationObjectColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetObjectTypeText(*Model, Args.RowData.GetObjectPath()));
				})
				.PopulateSearchItems_Lambda([Model](const FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectTypeText(Model.Get(), ObjectData.GetObjectPath()).ToString());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationColumnOrder::Type)),
			SHeaderRow::Column(TypeColumnId)
				.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
				.FillWidth(1.f)
			);
	}
}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "ReplicationPropertyColumns"

namespace UE::ConcertClientSharedSlate::ReplicationPropertyColumns
{
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");
	
	FReplicationPropertyColumn LabelColumn()
	{
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetPropertyDisplayText(Args.RowData.GetProperty()));
				})
				.PopulateSearchItems_Lambda([](const FReplicatedPropertyData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetPropertyDisplayText(ObjectData.GetProperty()).ToString());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::Label)),
			SHeaderRow::Column(LabelColumnId)
				.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
				.FillSized(500.f)
			);
	}
	
	FReplicationPropertyColumn TypeColumn()
	{
		static auto GetDisplayText = [](const FReplicatedPropertyData& Args)
		{
			UClass* Class = Args.GetOwningClass().TryLoadClass<UObject>();
			const FProperty* Property = Class ? ConcertSyncCore::PropertyChain::ResolveProperty(*Class, Args.GetProperty()) : nullptr;
			return Property ? FText::FromString(Property->GetCPPType()) : LOCTEXT("Unknown", "Unknown");	
		};
		
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(GetDisplayText(Args.RowData));
				})
				.PopulateSearchItems_Lambda([](const FReplicatedPropertyData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(GetDisplayText(ObjectData).ToString());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::Type)),
			SHeaderRow::Column(TypeColumnId)
				.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
				.FillWidth(1.f)
			);
	}

	namespace Private
	{
		static ECheckBoxState OnGetPropertyCheckboxState(
			const FConcertPropertyChain& PropertyChain,
			const IReplicationStreamViewer& Viewer,
			const IObjectToPropertiesModel& Model
			)
		{
			const TArray<FSoftObjectPath> SelectedObjectPaths = Viewer.GetObjectsBeingPropertyEdited();
			return GetPropertyCheckboxStateBasedOnSelection(PropertyChain, SelectedObjectPaths, Model);
		}

		static void OnPropertyCheckboxChanged(
			bool bIsChecked,
			const FConcertPropertyChain& PropertyChain,
			const IReplicationStreamViewer& Viewer,
			IEditableObjectToPropertiesModel& Model
			)
		{
			const TArray Properties{ PropertyChain };
			const TArray<FSoftObjectPath> SelectedObjects = Viewer.GetObjectsBeingPropertyEdited();

			if (bIsChecked)
			{
				// We cannot proceed if any of the selected objects cannot be loaded because we cannot obtain its class
				for (const FSoftObjectPath& Path : SelectedObjects)
				{
					UObject* Object = Path.ResolveObject();
					if (!Object && !Model.ContainsObjects({ Path }))
					{
						return;
					}
				}
				
				for (const FSoftObjectPath& Path : Viewer.GetObjectsBeingPropertyEdited())
				{
					UObject* Object = Path.ResolveObject();
					if (!Model.ContainsObjects({ Path }))
					{
						Model.AddObjects({ Object });
					}
					Model.AddProperties(Path, Properties);
				}
			}
			else
			{
				for (const FSoftObjectPath& Path : Viewer.GetObjectsBeingPropertyEdited())
				{
					Model.RemoveProperties(Path, Properties);
					// If this does not resolve it is not too bad if the subobject is not removed...
					UObject* Object = Path.ResolveObject();
					const bool bNeedsToRemoveNonRoot = Object && !Object->IsA<AActor>() && Model.GetNumProperties(Path) == 0;
					if (bNeedsToRemoveNonRoot)
					{
						Model.RemoveObjects({ Path });
					}
				}
			}
		}
	}
	
	FReplicationPropertyColumn ReplicatesColumns(
		TWeakPtr<IReplicationStreamViewer> Viewer,
		TWeakPtr<IEditableObjectToPropertiesModel> Model,
		const float ColumnWidth,
		const int32 Priority
		)
	{
		return MakePropertyCheckboxColumn(
			FGetColumnCheckboxState::CreateLambda(
				[Viewer, Model](const FConcertPropertyChain& Property)
				{
					const TSharedPtr<IReplicationStreamViewer> ViewerPin = Viewer.Pin();
					const TSharedPtr<IEditableObjectToPropertiesModel> ModelPin = Model.Pin();
					return ensure(ViewerPin && ModelPin) ? Private::OnGetPropertyCheckboxState(Property, *ViewerPin, *ModelPin) : ECheckBoxState::Undetermined;
				}),
			FOnColumnCheckboxChanged::CreateLambda(
				[Viewer, Model](bool bIsChecked, const FConcertPropertyChain& Property)
				{
					const TSharedPtr<IReplicationStreamViewer> ViewerPin = Viewer.Pin();
					const TSharedPtr<IEditableObjectToPropertiesModel> ModelPin = Model.Pin();
					if (ensure(ViewerPin && ModelPin))
					{
						Private::OnPropertyCheckboxChanged(bIsChecked, Property, *ViewerPin, *ModelPin);
					}
				}),
			FText::GetEmpty(),
			LOCTEXT("Replicates.ToolTip", "Select whether this property should be replicated"),
			ColumnWidth, 
			Priority
		);
	}

	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(const FConcertPropertyChain& Property, TConstArrayView<FSoftObjectPath> Selection, const IObjectToPropertiesModel& Model)
	{
		ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;
		for (const FSoftObjectPath& SelectedObject : Selection)
		{
			const bool bContainsProperty = Model.ContainsProperties(SelectedObject, { Property });
			const ECheckBoxState StateForThisObject = bContainsProperty
				? ECheckBoxState::Checked
				: ECheckBoxState::Unchecked;
			// The first object?
			if (CheckBoxState == ECheckBoxState::Undetermined)
			{
				CheckBoxState = StateForThisObject;
			}
			else if (CheckBoxState != StateForThisObject)
			{
				// Two boxes do not have the same value
				return ECheckBoxState::Undetermined;
			}
		}
		return CheckBoxState;
	}
	
	bool SortBySelectionThenByName_PropertyPredicate(
		const TArray<FSoftObjectPath>& SelectedObjects,
		const IObjectToPropertiesModel& Model,
		const FReplicatedPropertyData& Left,
		const FReplicatedPropertyData& Right
		)
	{
		const ECheckBoxState LeftCheckboxState = GetPropertyCheckboxStateBasedOnSelection(Left.GetProperty(), SelectedObjects, Model);
		const ECheckBoxState RightCheckboxState = GetPropertyCheckboxStateBasedOnSelection(Right.GetProperty(), SelectedObjects, Model);
		
		// Secondary sort by name
		if (LeftCheckboxState == RightCheckboxState)
		{
			return DisplayUtils::GetPropertyDisplayString(Left.GetProperty()) < DisplayUtils::GetPropertyDisplayString(Right.GetProperty());
		}

		// Selected properties should appear first
		return LeftCheckboxState == ECheckBoxState::Checked
			&& (RightCheckboxState == ECheckBoxState::Unchecked || RightCheckboxState == ECheckBoxState::Undetermined);
	}
}

#undef LOCTEXT_NAMESPACE
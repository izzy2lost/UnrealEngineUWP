// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectionViewerColumns.h"

#include "ReplicatedPropertyData.h"
#include "StreamEditor/View/ObjectViewer/ReplicatedObjectData.h"

#include "StreamEditor/Model/DisplayUtils.h"
#include "Internationalization/Internationalization.h"
#include "StreamEditor/Model/IEditableObjectToPropertiesModel.h"
#include "StreamEditor/View/ObjectEditor/SPropertyReplicationSelectionEditor.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "ReplicationObjectColumns"

namespace UE::MultiUserReplicationEditor::ReplicationObjectColumns
{
	const FName IconColumnId = TEXT("IconColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");

	FReplicationObjectColumn IconColumn(TSharedRef<IObjectToPropertiesModel> Model)
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
							.Image(DisplayUtils::GetObjectIcon(*Model, Args.RowData->GetObjectPath()).GetOptionalIcon())
						];
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationColumnOrder::Icon)),
			SHeaderRow::Column(IconColumnId)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(FMultiUserReplicationEditorStyle::Get().GetFloat("ReplicationTreeView.IconColumnWidth"))
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
						.Text(DisplayUtils::GetObjectDisplayText(Args.RowData->GetObjectPath()));
				})
				.PopulateSearchItems_Lambda([](const TSharedPtr<FReplicatedObjectData>& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectDisplayText(ObjectData->GetObjectPath()).ToString());
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
						.Text(DisplayUtils::GetObjectTypeText(*Model, Args.RowData->GetObjectPath()));
				})
				.PopulateSearchItems_Lambda([Model](const TSharedPtr<FReplicatedObjectData>& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectTypeText(Model.Get(), ObjectData->GetObjectPath()).ToString());
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

namespace UE::MultiUserReplicationEditor::ReplicationPropertyColumns
{
	const FName ReplicatesColumnId = TEXT("ReplicatesColumn");
	const FName LabelColumnId = TEXT("LabelColumn");

	FReplicationPropertyColumn ReplicatesColumns(TSharedRef<SPropertyReplicationSelectionEditor> EditorWidget, TSharedRef<IEditableObjectToPropertiesModel> Model)
	{
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([EditorWidget = MoveTemp(EditorWidget), Model = MoveTemp(Model)](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(SCheckBox)
						.ToolTipText(LOCTEXT("ReplicatesCheckbox.Tooltip", "Should replicate?"))
						.IsChecked_Lambda([EditorWidget, Model, RowData = Args.RowData]()
						{
							return GetPropertyCheckboxStateBasedOnSelection(*RowData.Get(), EditorWidget->GetSelectedObjects(), *Model);
						})
						.OnCheckStateChanged_Lambda([EditorWidget, Model, RowData = Args.RowData](ECheckBoxState NewState)
						{
							TArray Properties{ RowData->GetProperty() };
							for (const TSharedPtr<FReplicatedObjectData>& SelectedObject : EditorWidget->GetSelectedObjects())
							{
								if (NewState == ECheckBoxState::Checked)
								{
									Model->AddProperties(SelectedObject->GetObjectPath(), Properties);
								}
								else
								{
									Model->RemoveProperties(SelectedObject->GetObjectPath(), Properties);
								}
							}
						});
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::ReplicatesCheckbox)),
			SHeaderRow::Column(ReplicatesColumnId)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(FMultiUserReplicationEditorStyle::Get().GetFloat("ReplicationTreeView.IconColumnWidth"))
			);
	}
	
	FReplicationPropertyColumn LabelColumn()
	{
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetPropertyDisplayText(Args.RowData->GetProperty()));
				})
				.PopulateSearchItems_Lambda([](const TSharedPtr<FReplicatedPropertyData>& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetPropertyDisplayText(ObjectData->GetProperty()).ToString());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::Label)),
			SHeaderRow::Column(LabelColumnId)
				.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
				.FillWidth(1.f)
			);
	}

	ECheckBoxState GetPropertyCheckboxStateBasedOnSelection(const FReplicatedPropertyData& RowData, TConstArrayView<TSharedPtr<FReplicatedObjectData>> Selection, const IObjectToPropertiesModel& Model)
	{
		ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;
		for (const TSharedPtr<FReplicatedObjectData>& SelectedObject : Selection)
		{
			const bool bContainsProperty = Model.ContainsProperties(SelectedObject->GetObjectPath(), { RowData.GetProperty() });
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
}

#undef LOCTEXT_NAMESPACE
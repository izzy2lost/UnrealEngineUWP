// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDefaultReplicationStreamEditor.h"

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/View/Tree/SelectionViewerColumns.h"

#include "SBaseReplicationStreamEditor.h"

namespace UE::ConcertClientSharedSlate
{
	void SDefaultReplicationStreamEditor::Construct(
		const FArguments& InArgs,
		TSharedRef<IEditableReplicationStreamModel> InPropertiesModel,
		TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource)
	{
		PropertiesModel = InPropertiesModel;
		
		using namespace ReplicationColumns;
		using namespace ReplicationColumns::Property;
		const FReplicationPropertyColumn ReplicatesColumn = ReplicatesColumns(
			SharedThis(this),
			InPropertiesModel,
			TReplicationColumnDelegates<FReplicatedPropertyData>::FIsEnabled::CreateLambda([IsEnabled = InArgs._IsEditingEnabled](const FReplicatedPropertyData&)
			{
				return !IsEnabled.IsBound() || IsEnabled.Get();
			}),
			InArgs._EditingDisabledToolTipText
			);
		TArray<FReplicationPropertyColumn> PropertyColumns = InArgs._AdditionalPropertyColumns;
		PropertyColumns.Add(ReplicatesColumn);

		// Set both primary and secondary in case one is overriden but always use the override.
		const FColumnSortInfo PrimaryPropertySort = InArgs._PrimaryObjectSort.IsValid()
			? InArgs._PrimaryObjectSort
			: FColumnSortInfo{ ReplicatesColumnId, EColumnSortMode::Ascending };
		const FColumnSortInfo SecondaryPropertySort = InArgs._SecondaryObjectSort.IsValid()
			? InArgs._SecondaryObjectSort
			: FColumnSortInfo{ LabelColumnId, EColumnSortMode::Ascending };
		
		ChildSlot
		[
			SAssignNew(WrappedEditor, SBaseReplicationStreamEditor, InPropertiesModel, InObjectSelectionSource, InPropertySelectionSource)
				.AdditionalObjectColumns(InArgs._AdditionalObjectColumns)
				.PrimaryObjectSort(InArgs._PrimaryObjectSort)
				.SecondaryObjectSort(InArgs._SecondaryObjectSort)
				.AdditionalPropertyColumns(PropertyColumns)
				.PrimaryPropertySort(PrimaryPropertySort)
				.SecondaryPropertySort(SecondaryPropertySort)
				.SubobjectModel(InArgs._SubobjectModel)
				.OnExtendObjectsContextMenu(InArgs._OnExtendObjectsContextMenu)
				.LeftOfObjectSearchBar()
				[
					InArgs._LeftOfObjectSearchBar.Widget
				]
				.LeftOfPropertySearchBar()
				[
					InArgs._LeftOfPropertySearchBar.Widget
				]
				.IsEditingEnabled(InArgs._IsEditingEnabled)
				.EditingDisabledToolTipText(InArgs._EditingDisabledToolTipText)
		];
	}

	void SDefaultReplicationStreamEditor::Refresh()
	{
		return WrappedEditor->Refresh();
	}

	void SDefaultReplicationStreamEditor::RequestObjectColumnResort(const FName& ColumnId)
	{
		WrappedEditor->RequestObjectColumnResort(ColumnId);
	}

	void SDefaultReplicationStreamEditor::RequestPropertyColumnResort(const FName& ColumnId)
	{
		WrappedEditor->RequestPropertyColumnResort(ColumnId);
	}

	TArray<FSoftObjectPath> SDefaultReplicationStreamEditor::GetObjectsBeingPropertyEdited() const
	{
		return WrappedEditor->GetObjectsBeingPropertyEdited();
	}
}

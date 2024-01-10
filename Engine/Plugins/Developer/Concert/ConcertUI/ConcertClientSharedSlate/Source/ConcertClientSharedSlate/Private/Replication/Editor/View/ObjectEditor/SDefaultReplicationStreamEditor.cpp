// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDefaultReplicationStreamEditor.h"

#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/View/ClientEditorColumns.h"
#include "Replication/Editor/View/SelectionViewerColumns.h"

namespace UE::ConcertClientSharedSlate
{
	void SDefaultReplicationStreamEditor::Construct(const FArguments& InArgs, ConcertSharedSlate::FCreateEditorParams EditorParams)
	{
		using namespace ConcertSharedSlate;
		using namespace ConcertSharedSlate::ReplicationColumns;
		using namespace ConcertSharedSlate::ReplicationColumns::Property;
		using namespace ConcertClientSharedSlate::ReplicationColumns::Property;
		
		PropertiesModel = EditorParams.DataModel;

		const FReplicationPropertyColumn ReplicatesColumn = ReplicatesColumns(
			SharedThis(this),
			PropertiesModel,
			TReplicationColumnDelegates<FReplicatedPropertyData>::FIsEnabled::CreateLambda([IsEnabled = EditorParams.IsEditingEnabled](const FReplicatedPropertyData&)
			{
				return !IsEnabled.IsBound() || IsEnabled.Get();
			}),
			EditorParams.EditingDisabledToolTipText
			);
		TArray<FReplicationPropertyColumn>& PropertyColumns = EditorParams.ViewerParams.AdditionalPropertyColumns;
		PropertyColumns.Add(ReplicatesColumn);

		// Set both primary and secondary in case one is overriden but always use the override.
		EditorParams.ViewerParams.PrimaryPropertySort = EditorParams.ViewerParams.PrimaryPropertySort.IsValid()
			? EditorParams.ViewerParams.PrimaryPropertySort
			: FColumnSortInfo{ ReplicatesColumnId, EColumnSortMode::Ascending };
		EditorParams.ViewerParams.SecondaryPropertySort = EditorParams.ViewerParams.SecondaryPropertySort.IsValid()
			? EditorParams.ViewerParams.SecondaryPropertySort
			: FColumnSortInfo{ LabelColumnId, EColumnSortMode::Ascending };

		WrappedEditor = CreateBaseStreamEditor(MoveTemp(EditorParams));
		ChildSlot
		[
			WrappedEditor.ToSharedRef()
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

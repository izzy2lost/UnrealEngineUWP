// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDefaultReplicationStreamEditor.h"

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SelectionViewerColumns.h"

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
		
		ChildSlot
		[
			SAssignNew(WrappedEditor, SBaseReplicationStreamEditor, InPropertiesModel, InObjectSelectionSource, InPropertySelectionSource)
				.AdditionalObjectColumns(InArgs._AdditionalObjectColumns)
				.AdditionalPropertyColumns(PropertyColumns)
				.SubobjectView(InArgs._SubobjectView)
				.SubobjectModel(InArgs._SubobjectModel)
				.OnExtendObjectsContextMenu(InArgs._OnExtendObjectsContextMenu)
				.SortPropertyRowPredicate(this, &SDefaultReplicationStreamEditor::SortPropertiesPredicate)
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
				.ReplicationSettings(InArgs._ReplicationSettings)
		];
	}

	void SDefaultReplicationStreamEditor::Refresh()
	{
		return WrappedEditor->Refresh();
	}

	TArray<FSoftObjectPath> SDefaultReplicationStreamEditor::GetSelectedTopLevelObjects() const
	{
		return WrappedEditor->GetSelectedTopLevelObjects();
	}

	TArray<FSoftObjectPath> SDefaultReplicationStreamEditor::GetObjectsBeingPropertyEdited() const
	{
		return WrappedEditor->GetObjectsBeingPropertyEdited();
	}

	bool SDefaultReplicationStreamEditor::SortPropertiesPredicate(
		const FReplicatedPropertyData& Left,
		const FReplicatedPropertyData& Right
		) const
	{
		return ReplicationColumns::Property::SortBySelectionThenByName_PropertyPredicate(
			WrappedEditor->GetObjectsBeingPropertyEdited(),
			*PropertiesModel,
			Left,
			Right
			);
	}
}

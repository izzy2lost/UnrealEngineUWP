// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Item/SourceModelBuilders.h"
#include "StreamEditor/Model/Property/IPropertySourceModel.h"
#include "StreamEditor/View/ObjectViewer/SPropertyReplicationSelectionViewer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SHorizontalBox;

namespace UE::MultiUserReplicationEditor
{
	class IEditableObjectToPropertiesModel;
	class FFakeObjectToPropertiesEditorModel;
	class IPropertySelectionSourceModel;
	class IObjectSelectionSourceModel;
	class IObjectToPropertiesModel;
	
	enum class EReplicatedObjectChangeReason : uint8;
	
	struct FSelectableObjectInfo;
	
	/**
	 * Root widget for editing UMultiUserPropertyReplicationSelection.
	 * This widget extends SPropertyReplicationSelectionViewer by using IEditableObjectToPropertiesModel for editing.
	 */
	class MULTIUSERREPLICATIONEDITOR_API SPropertyReplicationSelectionEditor : public SPropertyReplicationSelectionViewer
	{
	public:
		
		SLATE_BEGIN_ARGS(SPropertyReplicationSelectionEditor)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs,
			TSharedRef<IEditableObjectToPropertiesModel> InPropertiesModel,
			TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		    TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource
		);

	private:

		/** For reading and writting to the edited asset */
		TSharedPtr<IEditableObjectToPropertiesModel> EditablePropertiesModel;
		/**
		 * Fakes to SPropertyReplicationSelectionViewer that all UClass properties are contained.
		 * We inject checkboxes to SPropertyReplicationSelectionViewer which do the actual adding and removing.
		 */
		TSharedPtr<FFakeObjectToPropertiesEditorModel> PropertiesModelAdapter;
		
		/** For deciding which objects can be added to EditablePropertiesModel. */
		TSharedPtr<IObjectSelectionSourceModel> ObjectSelectionSource;
		/** For deciding which properties can be added to EditablePropertiesModel. */
		TSharedPtr<IPropertySelectionSourceModel> PropertySelectionSource;

		/** Contains the widgets generated for PropertySelectionSource (since they depend on the selected objects). */
		TSharedPtr<SHorizontalBox> AddPropertyWidgetContainer;
		
		void OnObjectsChanged(TArrayView<UObject*> AddedObjects, TArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason);
		void OnPropertiesChanged();

		// Customizing left objects search bar
		TSharedRef<SWidget> BuildRootAddObjectWidgets() const;
		void OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd) const;

		// Respond to object editing events
		void OnDeleteObjects_PassByValue(TArray<TSharedPtr<FReplicatedObjectData>> CopiedObjectsToDelete) const { OnDeleteObjects(CopiedObjectsToDelete); }
		void OnDeleteObjects(const TArray<TSharedPtr<FReplicatedObjectData>>& ObjectsToDelete) const;
		TSharedPtr<SWidget> OnObjectsContextMenuOpening() const;
		void AddObjectSourceContextMenuOptions(FMenuBuilder& MenuBuilder) const;
		
		// Utils for building item source widgets
		ConcertSharedSlate::FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs MakeObjectSourceBuilderArgs() const;

		/** Util for sorting property data by*/
		bool SortBySelectionThenByName_PropertyPredicate(const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right) const;
	};
}

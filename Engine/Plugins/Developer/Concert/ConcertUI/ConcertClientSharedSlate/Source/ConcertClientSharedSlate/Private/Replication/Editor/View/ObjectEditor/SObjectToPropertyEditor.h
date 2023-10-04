// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Model/Item/SourceModelBuilders.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"
#include "Replication/Editor/View/ObjectViewer/SObjectToPropertyView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

namespace UE::ConcertClientSharedSlate
{
	class IReplicationSubobjectView;
}

class SHorizontalBox;

namespace UE::ConcertClientSharedSlate
{
	class IEditableObjectToPropertiesModel;
	class FFakeObjectToPropertiesEditorModel;
	class IObjectSelectionSourceModel;
	class IObjectToPropertiesModel;
	class IPropertySelectionSourceModel;
	class IReplicationSubobjectView;
	
	enum class EReplicatedObjectChangeReason : uint8;
	
	struct FSelectableObjectInfo;
	
	/**
	 * Root widget for editing UMultiUserPropertyReplicationSelection.
	 * This widget extends SObjectToPropertyViewer by using IEditableObjectToPropertiesModel for editing.
	 *
	 * This widget assumes that the IReplicationSubobjectView::GetSelectedObjects can report subobjects that are
	 * not in the model yet. Such objects would be added to the model.
	 */
	class CONCERTCLIENTSHAREDSLATE_API SObjectToPropertyEditor : public SObjectToPropertyView
	{
	public:
		
		SLATE_BEGIN_ARGS(SObjectToPropertyEditor)
		{}
			/** Optional. Placed between root object outliner and property editor. */
			SLATE_ARGUMENT(TSharedPtr<IReplicationSubobjectView>, SubobjectView)
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
		 * Fakes to SObjectToPropertyViewer that all UClass properties are contained.
		 * We inject checkboxes to SObjectToPropertyViewer which do the actual adding and removing.
		 */
		TSharedPtr<FFakeObjectToPropertiesEditorModel> PropertiesModelAdapter;
		
		/** For deciding which objects can be added to EditablePropertiesModel. */
		TSharedPtr<IObjectSelectionSourceModel> ObjectSelectionSource;
		/** For deciding which properties can be added to EditablePropertiesModel. */
		TSharedPtr<IPropertySelectionSourceModel> PropertySelectionSource;

		/** Contains the widgets generated for PropertySelectionSource (since they depend on the selected objects). */
		TSharedPtr<SHorizontalBox> AddPropertyWidgetContainer;
		
		void OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason);
		void OnPropertiesChanged();

		// Customizing left objects search bar
		TSharedRef<SWidget> BuildRootAddObjectWidgets() const;
		void OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd) const;
		
		// Replicates property column events
		ECheckBoxState OnGetPropertyCheckboxState(const FConcertPropertyChain& PropertyChain);
		void OnPropertyCheckboxChanged(bool bIsChecked, const FConcertPropertyChain& PropertyChain);

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

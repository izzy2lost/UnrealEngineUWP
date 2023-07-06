// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyReplicationSelectionEditor.h"

#include "FakeObjectToPropertiesEditorModel.h"
#include "StreamEditor/Model/DisplayUtils.h"
#include "StreamEditor/Model/IEditableObjectToPropertiesModel.h"
#include "StreamEditor/Model/Object/IObjectSelectionSourceModel.h"
#include "Model/Item/SourceModelBuilders.h"
#include "StreamEditor/Model/Property/IPropertySelectionSourceModel.h"
#include "StreamEditor/View/ObjectViewer/ReplicatedObjectData.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "StreamEditor/View/ObjectViewer/ReplicatedPropertyData.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SPropertyReplicationSelectionEditor"

namespace UE::MultiUserReplicationEditor
{
	void SPropertyReplicationSelectionEditor::Construct(const FArguments& InArgs,
		TSharedRef<IEditableObjectToPropertiesModel> InPropertiesModel,
		TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource)
	{
		ObjectSelectionSource = MoveTemp(InObjectSelectionSource);
		PropertySelectionSource = MoveTemp(InPropertySelectionSource);
		
		EditablePropertiesModel = MoveTemp(InPropertiesModel);
		EditablePropertiesModel->OnObjectsChanged().AddSP(this, &SPropertyReplicationSelectionEditor::OnObjectsChanged);
		EditablePropertiesModel->OnPropertiesChanged().AddSP(this, &SPropertyReplicationSelectionEditor::OnPropertiesChanged);
		PropertiesModelAdapter = MakeShared<FFakeObjectToPropertiesEditorModel>(EditablePropertiesModel.ToSharedRef(), PropertySelectionSource.ToSharedRef());
		
		SPropertyReplicationSelectionViewer::Construct(
			SPropertyReplicationSelectionViewer::FArguments()
				.AdditionalPropertyColumns({ ReplicationPropertyColumns::ReplicatesColumns(SharedThis(this), EditablePropertiesModel.ToSharedRef()) })
				.OnDeleteObjects(this, &SPropertyReplicationSelectionEditor::OnDeleteObjects)
				.OnObjectsContextMenuOpening(this, &SPropertyReplicationSelectionEditor::OnObjectsContextMenuOpening)
				.SortPropertyRowPredicate(this, &SPropertyReplicationSelectionEditor::SortBySelectionThenByName_PropertyPredicate)
				.LeftOfObjectSearchBar()
				[
					BuildRootAddObjectWidgets()
				]
				.LeftOfPropertySearchBar()
				[
					SAssignNew(AddPropertyWidgetContainer, SHorizontalBox)
				],
			PropertiesModelAdapter.ToSharedRef()
			);
	}

	void SPropertyReplicationSelectionEditor::OnObjectsChanged(TArrayView<UObject*> AddedObjects, TArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		RefreshObjectData();

		if (ChangeReason == EReplicatedObjectChangeReason::ChangedDirectly && !AddedObjects.IsEmpty())
		{
			for (const UObject* Object : AddedObjects)
			{
				// Example: Right-click component > Add Outer. You want to see the both the newly added actor and the component that you right-clicked.
				bool bHasChildren = false;
				GetObjectChildren(Object, [&bHasChildren](auto) mutable { bHasChildren = true; });
				if (bHasChildren)
				{
					SetObjectsExpanded(TArray{ Object }, true);
				}

				// Example: Add a component to an actor. You want to see the object you just created.
				if (const FSoftObjectPath ParentObject = GetParent(Object); !ParentObject.IsNull())
				{
					SetObjectsExpanded(TArray{ ParentObject }, true);
				}
			}
			
			// After adding an object, you usually instantly want to edit it so make the properties show up in the property view. Do this after making sure the item is visible.
			SetSelectedObjects(AddedObjects, true);
		}
	}

	void SPropertyReplicationSelectionEditor::OnPropertiesChanged()
	{
		RefreshPropertyData();
	}

	TSharedRef<SWidget> SPropertyReplicationSelectionEditor::BuildRootAddObjectWidgets() const
	{
		using namespace ConcertSharedSlate;
		
		const TSharedRef<SHorizontalBox> Root = SNew(SHorizontalBox);
		const FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs Args = MakeObjectSourceBuilderArgs();
		for (const TSourceSelectionCategory<FSelectableObjectInfo>& Category : ObjectSelectionSource->GetRootSources())
		{
			Root->AddSlot()
				.AutoWidth()
				[
					FSourceModelBuilders<FSelectableObjectInfo>::BuildCategory(Category, Args)	
				];
		}
		return Root;
	}

	void SPropertyReplicationSelectionEditor::OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd) const
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(
			ObjectsToAdd,
			Objects,
			[](const FSelectableObjectInfo& SelectableObject) { return SelectableObject.Object.IsValid(); },
			[](const FSelectableObjectInfo& SelectableObject){ return SelectableObject.Object.Get(); }
			);
		
		EditablePropertiesModel->AddObjects(Objects);
	}

	void SPropertyReplicationSelectionEditor::OnDeleteObjects(const TArray<TSharedPtr<FReplicatedObjectData>>& ObjectsToDelete) const
	{
		TArray<FSoftObjectPath> DeleteObjectPaths;
		Algo::Transform(ObjectsToDelete, DeleteObjectPaths, [](const TSharedPtr<FReplicatedObjectData>& Data) { return Data->GetObjectPath(); });
		EditablePropertiesModel->RemoveObjects(DeleteObjectPaths);
	}

	TSharedPtr<SWidget> SPropertyReplicationSelectionEditor::OnObjectsContextMenuOpening() const
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		AddObjectSourceContextMenuOptions(MenuBuilder);
		MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteItems", "Delete"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPropertyReplicationSelectionEditor::OnDeleteObjects_PassByValue, GetSelectedObjects())),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		
		return MenuBuilder.MakeWidget();
	}

	void SPropertyReplicationSelectionEditor::AddObjectSourceContextMenuOptions(FMenuBuilder& MenuBuilder) const
	{
		using namespace ConcertSharedSlate;
		
		// Context menu generation is only supported for single items
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = GetSelectedObjects();
		if (SelectedObjects.Num() != 1)
		{
			return;
		}
		
		const TArray<TSharedRef<IObjectSourceModel>> ContextMenuOptions = ObjectSelectionSource->GetContextMenuOptions(SelectedObjects[0]->GetObjectPath());
		if (ContextMenuOptions.IsEmpty())
		{
			return;
		}
		
		const FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs Args = MakeObjectSourceBuilderArgs();
		for (const TSharedRef<IObjectSourceModel>& SourceModel : ContextMenuOptions)
		{
			bool bCanAddAnyObject = false; 
			SourceModel->EnumerateSelectableItems([this, &bCanAddAnyObject](const FSelectableObjectInfo& SelectableOption) mutable
			{
				bCanAddAnyObject |= !EditablePropertiesModel->ContainsObjects({ SelectableOption.Object.Get() });
				return bCanAddAnyObject ? EBreakBehavior::Break : EBreakBehavior::Continue;
			});

			// Skip showing context menu options which will not add anything new.
			if (bCanAddAnyObject)
			{
				FSourceModelBuilders<FSelectableObjectInfo>::AddOptionToMenu(SourceModel, Args, MenuBuilder);
			}
		}
		
		MenuBuilder.AddSeparator();
	}

	ConcertSharedSlate::FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs SPropertyReplicationSelectionEditor::MakeObjectSourceBuilderArgs() const
	{
		using namespace ConcertSharedSlate;
		
		return FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs
		{
			FSourceModelBuilders<FSelectableObjectInfo>::FOnItemsSelected::CreateSP(this, &SPropertyReplicationSelectionEditor::OnObjectsSelectedForAdding),
			FSourceModelBuilders<FSelectableObjectInfo>::FGetItemDisplayString::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectDisplayString(*Item.Object.Get()) : TEXT("");
			}),
			FSourceModelBuilders<FSelectableObjectInfo>::FGetItemIcon::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectIcon(*Item.Object.Get()) : FSlateIcon();
			}),
			FSourceModelBuilders<FSelectableObjectInfo>::FIsItemSelected::CreateLambda([this](const FSelectableObjectInfo& Item)
			{
				return EditablePropertiesModel->ContainsObjects({ Item.Object.Get() } );
			})
		}; 
	}

	bool SPropertyReplicationSelectionEditor::SortBySelectionThenByName_PropertyPredicate(const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right) const
	{
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = GetSelectedObjects();
		const ECheckBoxState LeftCheckboxState = ReplicationPropertyColumns::GetPropertyCheckboxStateBasedOnSelection(*Left, SelectedObjects, *EditablePropertiesModel);
		const ECheckBoxState RightCheckboxState = ReplicationPropertyColumns::GetPropertyCheckboxStateBasedOnSelection(*Right, SelectedObjects, *EditablePropertiesModel);
		
		// Secondary sort by name
		if (LeftCheckboxState == RightCheckboxState)
		{
			return DisplayUtils::GetPropertyDisplayString(Left->GetProperty()) < DisplayUtils::GetPropertyDisplayString(Right->GetProperty());
		}

		// Selected properties should appear first
		return LeftCheckboxState == ECheckBoxState::Checked
			&& (RightCheckboxState == ECheckBoxState::Unchecked || RightCheckboxState == ECheckBoxState::Undetermined);
	}
}

#undef LOCTEXT_NAMESPACE
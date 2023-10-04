// Copyright Epic Games, Inc. All Rights Reserved.

#include "SObjectToPropertyEditor.h"

#include "FakeObjectToPropertiesEditorModel.h"
#include "Model/Item/SourceModelBuilders.h"
#include "Replication/Editor/Model/DisplayUtils.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/Editor/Model/Object/IObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"

#include "Algo/AnyOf.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SObjectToPropertyEditor"

namespace UE::ConcertClientSharedSlate
{
	void SObjectToPropertyEditor::Construct(const FArguments& InArgs,
		TSharedRef<IEditableObjectToPropertiesModel> InPropertiesModel,
		TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource)
	{
		ObjectSelectionSource = MoveTemp(InObjectSelectionSource);
		PropertySelectionSource = MoveTemp(InPropertySelectionSource);
		
		EditablePropertiesModel = MoveTemp(InPropertiesModel);
		EditablePropertiesModel->OnObjectsChanged().AddSP(this, &SObjectToPropertyEditor::OnObjectsChanged);
		EditablePropertiesModel->OnPropertiesChanged().AddSP(this, &SObjectToPropertyEditor::OnPropertiesChanged);
		PropertiesModelAdapter = MakeShared<FFakeObjectToPropertiesEditorModel>(EditablePropertiesModel.ToSharedRef(), PropertySelectionSource.ToSharedRef());

		using namespace ReplicationPropertyColumns;
		const FReplicationPropertyColumn ReplicatesColumn =
			ReplicatesColumns(
				FGetPropertyCheckboxState::CreateSP(this, &SObjectToPropertyEditor::OnGetPropertyCheckboxState),
				FOnPropertyCheckboxChanged::CreateSP(this, &SObjectToPropertyEditor::OnPropertyCheckboxChanged)
				);
		
		SObjectToPropertyView::Construct(
			SObjectToPropertyView::FArguments()
				.AdditionalPropertyColumns({ ReplicatesColumn })
				.SubobjectView(InArgs._SubobjectView)
				.OnDeleteObjects(this, &SObjectToPropertyEditor::OnDeleteObjects)
				.OnObjectsContextMenuOpening(this, &SObjectToPropertyEditor::OnObjectsContextMenuOpening)
				.SortPropertyRowPredicate(this, &SObjectToPropertyEditor::SortBySelectionThenByName_PropertyPredicate)
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

	void SObjectToPropertyEditor::OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		RefreshObjectData();
	}

	void SObjectToPropertyEditor::OnPropertiesChanged()
	{
		RefreshPropertyData();
	}

	TSharedRef<SWidget> SObjectToPropertyEditor::BuildRootAddObjectWidgets() const
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

	void SObjectToPropertyEditor::OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd) const
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

	ECheckBoxState SObjectToPropertyEditor::OnGetPropertyCheckboxState(const FConcertPropertyChain& PropertyChain)
	{
		const TArray<FSoftObjectPath> SelectedObjectPaths = GetSelectedObjectShowingProperties();
		return ReplicationPropertyColumns::GetPropertyCheckboxStateBasedOnSelection(PropertyChain, SelectedObjectPaths, *EditablePropertiesModel);
	}

	void SObjectToPropertyEditor::OnPropertyCheckboxChanged(bool bIsChecked, const FConcertPropertyChain& PropertyChain)
	{
		TArray Properties{ PropertyChain };
		for (const FSoftObjectPath& Path : GetSelectedObjectShowingProperties())
		{
			UObject* Object = Path.ResolveObject();
			if (!Object)
			{
				continue;
			}
			
			if (bIsChecked)
			{
				if (!EditablePropertiesModel->ContainsObjects({ Path }))
				{
					EditablePropertiesModel->AddObjects({ Object });
				}
				EditablePropertiesModel->AddProperties(Path, Properties);
			}
			else
			{
				EditablePropertiesModel->RemoveProperties(Path, Properties);
				const bool bNeedsToRemoveNonRoot = !Object->IsA<AActor>() && EditablePropertiesModel->GetNumProperties(Path) == 0;
				if (bNeedsToRemoveNonRoot)
				{
					EditablePropertiesModel->RemoveObjects({ Path });
				}
			}
		}
	}

	void SObjectToPropertyEditor::OnDeleteObjects(const TArray<TSharedPtr<FReplicatedObjectData>>& ObjectsToDelete) const
	{
		TArray<FSoftObjectPath> DeleteObjectPaths;
		Algo::Transform(ObjectsToDelete, DeleteObjectPaths, [](const TSharedPtr<FReplicatedObjectData>& Data) { return Data->GetObjectPath(); });

		// We want to delete children not listed in the outliner, such as components and other subobjects
		TArray<FSoftObjectPath> ObjectAndChildren;
		EditablePropertiesModel->ForEachReplicatedObject([&ObjectsToDelete, &ObjectAndChildren](const FSoftObjectPath& ReplicatedObject)
		{
			const bool bIsChildOfDeletedObject = Algo::AnyOf(ObjectsToDelete, [&ReplicatedObject](const TSharedPtr<FReplicatedObjectData>& ObjectData)
			{
				// ReplicatedObject is a child of a deleted object if the path before it contains one of the deleted objects
				return ReplicatedObject.ToString().Contains(ObjectData->GetObjectPath().ToString());
			});
			if (bIsChildOfDeletedObject)
			{
				ObjectAndChildren.Add(ReplicatedObject);
			}
			return EBreakBehavior::Continue;
		});
		
		checkf(ObjectAndChildren.Num() >= ObjectsToDelete.Num(), TEXT("Above algorithm is broken."));
		EditablePropertiesModel->RemoveObjects(ObjectAndChildren);
	}

	TSharedPtr<SWidget> SObjectToPropertyEditor::OnObjectsContextMenuOpening() const
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		AddObjectSourceContextMenuOptions(MenuBuilder);
		MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteItems", "Delete"),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SObjectToPropertyEditor::OnDeleteObjects_PassByValue, GetSelectedOutlinerObjects())),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		
		return MenuBuilder.MakeWidget();
	}

	void SObjectToPropertyEditor::AddObjectSourceContextMenuOptions(FMenuBuilder& MenuBuilder) const
	{
		using namespace ConcertSharedSlate;
		
		// Context menu generation is only supported for single items
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = GetSelectedOutlinerObjects();
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

	ConcertSharedSlate::FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs SObjectToPropertyEditor::MakeObjectSourceBuilderArgs() const
	{
		using namespace ConcertSharedSlate;
		
		return FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs
		{
			FSourceModelBuilders<FSelectableObjectInfo>::FOnItemsSelected::CreateSP(this, &SObjectToPropertyEditor::OnObjectsSelectedForAdding),
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

	bool SObjectToPropertyEditor::SortBySelectionThenByName_PropertyPredicate(const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right) const
	{
		const TArray<FSoftObjectPath> SelectedObjects = GetSelectedObjectShowingProperties();
		const ECheckBoxState LeftCheckboxState = ReplicationPropertyColumns::GetPropertyCheckboxStateBasedOnSelection(Left->GetProperty(), SelectedObjects, *EditablePropertiesModel);
		const ECheckBoxState RightCheckboxState = ReplicationPropertyColumns::GetPropertyCheckboxStateBasedOnSelection(Right->GetProperty(), SelectedObjects, *EditablePropertiesModel);
		
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
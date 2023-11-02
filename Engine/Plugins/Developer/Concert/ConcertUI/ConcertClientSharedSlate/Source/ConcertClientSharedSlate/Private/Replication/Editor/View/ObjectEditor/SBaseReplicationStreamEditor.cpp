// Copyright Epic Games, Inc. All Rights Reserved.

#include "SBaseReplicationStreamEditor.h"

#include "FakeObjectToPropertiesEditorModel.h"
#include "Model/Item/SourceModelBuilders.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/Editor/Model/Object/IObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/IPropertySelectionSourceModel.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/DisplayUtils.h"
#include "Replication/Editor/View/ObjectViewer/SReplicationStreamViewer.h"
#include "Replication/Settings/ConcertReplicationEditorSettings.h"

#include "Algo/AnyOf.h"
#include "UObject/Class.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SBaseReplicationStreamEditor"

namespace UE::ConcertClientSharedSlate
{
	void SBaseReplicationStreamEditor::Construct(
		const FArguments& InArgs,
		TSharedRef<IEditableObjectToPropertiesModel> InPropertiesModel,
		TSharedRef<IObjectSelectionSourceModel> InObjectSelectionSource,
		TSharedRef<IPropertySelectionSourceModel> InPropertySelectionSource)
	{
		ObjectSelectionSource = MoveTemp(InObjectSelectionSource);
		PropertySelectionSource = MoveTemp(InPropertySelectionSource);
		
		EditablePropertiesModel = MoveTemp(InPropertiesModel);
		EditablePropertiesModel->OnObjectsChanged().AddSP(this, &SBaseReplicationStreamEditor::OnObjectsChanged);
		EditablePropertiesModel->OnPropertiesChanged().AddSP(this, &SBaseReplicationStreamEditor::OnPropertiesChanged);
		PropertiesModelAdapter = MakeShared<FFakeObjectToPropertiesEditorModel>(EditablePropertiesModel.ToSharedRef(), PropertySelectionSource.ToSharedRef());

		IsEditingEnabledAttribute = InArgs._IsEditingEnabled;
		EditingDisabledToolTipTextAttribute = InArgs._EditingDisabledToolTipText;
		ReplicationSettingsAttribute = InArgs._ReplicationSettings;
		OnExtendObjectsContextMenuDelegate = InArgs._OnExtendObjectsContextMenu;
		
		ChildSlot
		[
			SAssignNew(ReplicationViewer, SReplicationStreamViewer, PropertiesModelAdapter.ToSharedRef())
				.AdditionalObjectColumns(InArgs._AdditionalObjectColumns)
				.AdditionalPropertyColumns(InArgs._AdditionalPropertyColumns)
				.SubobjectView(InArgs._SubobjectView)
				.OnDeleteObjects(this, &SBaseReplicationStreamEditor::OnDeleteObjects)
				.OnObjectsContextMenuOpening(this, &SBaseReplicationStreamEditor::OnObjectsContextMenuOpening)
				.SortPropertyRowPredicate(InArgs._SortPropertyRowPredicate)
				.LeftOfObjectSearchBar()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						BuildRootAddObjectWidgets()
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						InArgs._LeftOfObjectSearchBar.Widget
					]
				]
				.LeftOfPropertySearchBar()
				[
					InArgs._LeftOfPropertySearchBar.Widget
				]
				.NoOutlinerObjects(LOCTEXT("NoObjects", "Add objects to replicate"))
		];
	}

	SBaseReplicationStreamEditor::~SBaseReplicationStreamEditor()
	{
		EditablePropertiesModel->OnObjectsChanged().RemoveAll(this);
		EditablePropertiesModel->OnPropertiesChanged().RemoveAll(this);
	}

	void SBaseReplicationStreamEditor::Refresh()
	{
		ReplicationViewer->RefreshObjectData();
		ReplicationViewer->RefreshSubobjectData();
		ReplicationViewer->RefreshPropertyData();
	}

	TArray<FSoftObjectPath> SBaseReplicationStreamEditor::GetSelectedTopLevelObjects() const
	{
		return ReplicationViewer->GetSelectedTopLevelObjects();
	}

	TArray<FSoftObjectPath> SBaseReplicationStreamEditor::GetObjectsBeingPropertyEdited() const
	{
		return ReplicationViewer->GetObjectsBeingPropertyEdited();
	}

	bool SBaseReplicationStreamEditor::IsEditingDisabled() const
	{
		return (IsEditingEnabledAttribute.IsBound() || IsEditingEnabledAttribute.IsSet())
			&& !IsEditingEnabledAttribute.Get();
	}

	FText SBaseReplicationStreamEditor::GetEditingDisabledText() const
	{
		return (EditingDisabledToolTipTextAttribute.IsBound() || EditingDisabledToolTipTextAttribute.IsSet())
			? EditingDisabledToolTipTextAttribute.Get()
			: FText::GetEmpty();
	}

	void SBaseReplicationStreamEditor::OnObjectsChanged(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		ReplicationViewer->RefreshObjectData();

		// Newly added objects should be automatically selected
		if (!AddedObjects.IsEmpty())
		{
			AutoAddObjectsAndPropertiesFromSettings(AddedObjects);
			
			TArray<FSoftObjectPath> TopLevelObjects;
			Algo::TransformIf(AddedObjects, TopLevelObjects, [this](const UObject* Object)
			{
				return PropertiesModelAdapter->IsTopLevelObject(Object);
			}, [](const UObject* Object){ return Object; });
			ReplicationViewer->SelectTopLevelObjects(TopLevelObjects);
		}
	}

	void SBaseReplicationStreamEditor::AutoAddObjectsAndPropertiesFromSettings(TConstArrayView<UObject*> AddedObjects)
	{
		if (!bIsAddingFromSelection)
		{
			return;
		}
		
		const FConcertReplicationEditorSettings* AutoPopulateSettings = ReplicationSettingsAttribute.IsBound()
		  ? ReplicationSettingsAttribute.Get()
		  : nullptr;
		if (!AutoPopulateSettings)
		{
			return;
		}

		for (const UObject* AddedObject : AddedObjects)
		{
			TArray<FConcertPropertyChain> AdditionalProperties;
			AutoPopulateSettings->AddDefaultPropertiesFromSettings(*AddedObject->GetClass(), [&AdditionalProperties](FConcertPropertyChain&& Chain)
			{
				AdditionalProperties.Emplace(MoveTemp(Chain));
			});
			EditablePropertiesModel->AddProperties({ AddedObject }, AdditionalProperties);

			TArray<UObject*> AdditionalObjectsToAdd;
			AutoPopulateSettings->AddAdditionalObjectsFromSettings(*AddedObject, [&AdditionalObjectsToAdd](UObject& FurtherObject)
			{
				AdditionalObjectsToAdd.Add(&FurtherObject);
			});
			EditablePropertiesModel->AddObjects(AdditionalObjectsToAdd);
		}
	}

	void SBaseReplicationStreamEditor::OnPropertiesChanged()
	{
		ReplicationViewer->RefreshPropertyData();
	}

	TSharedRef<SWidget> SBaseReplicationStreamEditor::BuildRootAddObjectWidgets()
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

	void SBaseReplicationStreamEditor::OnObjectsSelectedForAdding(TArray<FSelectableObjectInfo> ObjectsToAdd)
	{
		TArray<UObject*> Objects;
		Algo::TransformIf(
			ObjectsToAdd,
			Objects,
			[](const FSelectableObjectInfo& SelectableObject) { return SelectableObject.Object.IsValid(); },
			[](const FSelectableObjectInfo& SelectableObject){ return SelectableObject.Object.Get(); }
			);

		TGuardValue<bool> GuardObjectSelection(bIsAddingFromSelection, true);
		EditablePropertiesModel->AddObjects(Objects);
	}

	void SBaseReplicationStreamEditor::OnDeleteObjects(const TArray<TSharedPtr<FReplicatedObjectData>>& ObjectsToDelete) const
	{
		if (IsEditingDisabled())
		{
			return;
		}
		
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

		// Viewer has an internal cache of the selected object. Setting the displayed object to the cached one is a no-op.
		// Clear the cache so it displays correctly if the user adds the object back straight after removing it.
		ReplicationViewer->ClearSubobjectSelection();
	}

	TSharedPtr<SWidget> SBaseReplicationStreamEditor::OnObjectsContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		AddObjectSourceContextMenuOptions(MenuBuilder);
		MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteItems", "Delete"),
				TAttribute<FText>::CreateLambda([this](){ return GetEditingDisabledText(); }),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SBaseReplicationStreamEditor::OnDeleteObjects_PassByValue, ReplicationViewer->GetSelectedOutlinerObjects()),
					FCanExecuteAction::CreateLambda([this]() { return !IsEditingDisabled(); })
					),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		
		OnExtendObjectsContextMenuDelegate.ExecuteIfBound(MenuBuilder);
		return MenuBuilder.MakeWidget();
	}

	void SBaseReplicationStreamEditor::AddObjectSourceContextMenuOptions(FMenuBuilder& MenuBuilder)
	{
		using namespace ConcertSharedSlate;
		
		// Context menu generation is only supported for single items
		const TArray<TSharedPtr<FReplicatedObjectData>> SelectedObjects = ReplicationViewer->GetSelectedOutlinerObjects();
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

	ConcertSharedSlate::FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs SBaseReplicationStreamEditor::MakeObjectSourceBuilderArgs()
	{
		using namespace ConcertSharedSlate;
		using FBuilderDelegates = FSourceModelBuilders<FSelectableObjectInfo>;
		
		return FSourceModelBuilders<FSelectableObjectInfo>::FItemPickerArgs
		{
			FBuilderDelegates::FOnItemsSelected::CreateSP(this, &SBaseReplicationStreamEditor::OnObjectsSelectedForAdding),
			FBuilderDelegates::FGetItemDisplayString::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectDisplayString(*Item.Object.Get()) : TEXT("");
			}),
			FBuilderDelegates::FGetItemIcon::CreateLambda([](const FSelectableObjectInfo& Item)
			{
				return Item.Object.IsValid() ? DisplayUtils::GetObjectIcon(*Item.Object.Get()) : FSlateIcon();
			}),
			FBuilderDelegates::FIsItemSelected::CreateLambda([this](const FSelectableObjectInfo& Item)
			{
				return EditablePropertiesModel->ContainsObjects({ Item.Object.Get() } );
			}),
			TAttribute<bool>::CreateLambda([this]() { return !IsEditingDisabled(); }),
			TAttribute<FText>::CreateLambda([this]() { return GetEditingDisabledText(); }),
			EItemPickerFlags::DisplayOptionListInline
		}; 
	}
}

#undef LOCTEXT_NAMESPACE
// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaPageRemoteControlProps.h"

#include "AvaRCPropertyItem.h"
#include "IAvaMediaModule.h"
#include "Playback/AvalancheRemoteControl.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvalancheManagedInstanceCache.h"
#include "Playlist/AvalanchePage.h"
#include "SAvaRCPropertyItemRow.h"
#include "SlateOptMacros.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "AvaPageRemoteControlProps"

const FName SAvaPageRemoteControlProps::PropertyColumnName = "PropertyColumn";
const FName SAvaPageRemoteControlProps::ValueColumnName = "ValueColumn";

FAvaRCPropertyHeaderRowExtensionDelegate SAvaPageRemoteControlProps::HeaderRowExtensionDelegate;
TMap<FName, TArray<FAvaRCPropertyTableRowExtensionDelegate>> SAvaPageRemoteControlProps::TableRowExtensionDelegates;

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TArray<FAvaRCPropertyTableRowExtensionDelegate>& SAvaPageRemoteControlProps::GetTableRowExtensionDelegates(FName InExtensionName)
{
	return TableRowExtensionDelegates.FindOrAdd(InExtensionName);
}

void SAvaPageRemoteControlProps::Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;
	ActivePageId = FAvalanchePage::InvalidPageId;

	TSharedRef<SHeaderRow> HeaderRow =
		SNew(SHeaderRow)
		+ SHeaderRow::Column(PropertyColumnName)
		.DefaultLabel(LOCTEXT("Property", "Property"))
		.FixedWidth(150.f)
		+ SHeaderRow::Column(ValueColumnName)
		.DefaultLabel(LOCTEXT("Value", "Value"))
		.FillWidth(1.f);

	HeaderRowExtensionDelegate.Broadcast(SharedThis(this), HeaderRow);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(PropertyContainer, SListView<FAvaRCPropertyItemPtr>)
			.ListItemsSource(&PropertyItems)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SAvaPageRemoteControlProps::OnGenerateControllerRow)
			.HeaderRow(HeaderRow)
		]
	];

	Refresh({});
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

SAvaPageRemoteControlProps::~SAvaPageRemoteControlProps()
{
}

void SAvaPageRemoteControlProps::UpdateDefaultValuesAndRefresh(const TArray<int32>& InSelectedPageIds)
{
	// Remark:
	// This is used in the "reimport page" work flow. We want to be able to
	// reimport multiple pages at the same time.
	
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

		if (IsValid(Playlist))
		{
			EAvaRemoteControlChanges Changes = EAvaRemoteControlChanges::None;
			for (const int32 PageId : InSelectedPageIds)
			{
				FAvalanchePage& Page = PlaylistEditor->GetPlaylist()->GetPage(PageId);

				if (Page.IsValidPage())
				{
					ManagedInstances = FAvaPlaylistEditor::GetManagedInstancesForPage(Playlist, Page);
					
					if (!ManagedInstances.IsEmpty())
					{
						FAvalancheRemoteControlValues MergedDefaultRCValues;
						FAvaPlaylistEditor::MergeDefaultRemoteControlValues(ManagedInstances, MergedDefaultRCValues);

						// Using the playlist API for event propagation.
						constexpr bool bUpdateDefaults = true;
						Changes |= Playlist->UpdateRemoteControlValues(ActivePageId, MergedDefaultRCValues, bUpdateDefaults);
					}
				}
			}
			
			if (Changes != EAvaRemoteControlChanges::None)
			{
				PlaylistEditor->MarkAsModified();
			}
			
			Refresh(InSelectedPageIds);
		}
	}
}

TSharedRef<ITableRow> SAvaPageRemoteControlProps::OnGenerateControllerRow(FAvaRCPropertyItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return InItem->CreateWidget(SharedThis(this), InOwnerTable);
}


void SAvaPageRemoteControlProps::RefreshTable(const TSet<FGuid>& InEntityIds)
{
	for (const FAvaRCPropertyItemPtr& PropertyItem : PropertyItems)
	{
		const TSharedPtr<FRemoteControlEntity> Entity = PropertyItem->GetEntity();
		if (Entity && (InEntityIds.IsEmpty() || InEntityIds.Contains(Entity->GetId())))
		{
			TSharedPtr<ITableRow> TableRow = PropertyContainer->WidgetFromItem(PropertyItem);

			if (TableRow.IsValid())
			{
				const TSharedRef<SAvaRCPropertyItemRow> ItemRow = StaticCastSharedRef<SAvaRCPropertyItemRow>(TableRow.ToSharedRef());
				ItemRow->UpdateValue();
			}
		}
	}
}

void SAvaPageRemoteControlProps::Refresh(const TArray<int32>& InSelectedPageIds)
{
	if (!PropertyContainer.IsValid())
	{
		return;
	}

	ActivePageId = InSelectedPageIds.IsEmpty() ? FAvalanchePage::InvalidPageId : InSelectedPageIds[0];
	
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		const UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();

		if (IsValid(Playlist))
		{
			ManagedInstances.Reset();

			if (FAvalanchePage* ActivePage = GetActivePage())
			{
				ManagedInstances = FAvaPlaylistEditor::GetManagedInstancesForPage(Playlist, *ActivePage);

				for (const TSharedPtr<FAvalancheManagedInstance>& ManagedInstance : ManagedInstances)
				{
					URemoteControlPreset* Preset = ManagedInstance ? ManagedInstance->GetRemoteControlPreset() : nullptr;
					if (Preset)
					{
						BindRemoteControlDelegates(Preset);	
					}
				}

				if (!ManagedInstances.IsEmpty())
				{
					FAvalancheRemoteControlValues MergedDefaultRCValues;
					FAvaPlaylistEditor::MergeDefaultRemoteControlValues(ManagedInstances, MergedDefaultRCValues);
					
					// Prune any extra stale values. This happens if templates are changed.
					if (ActivePage->PruneRemoteControlValues(MergedDefaultRCValues) != EAvaRemoteControlChanges::None)
					{
						UE_LOG(LogAvaMediaRemoteControl, Log, TEXT("Page %d had stale values that where pruned."), ActivePage->GetPageId());
						PlaylistEditor->MarkAsModified();
					}
				}
			}
		}

		struct FAvaPropertyDetails
		{
			TSharedRef<FRemoteControlEntity> Entity;
			bool bEntityControlled;
		};

		TArray<FAvaPropertyDetails> NewItems;

		for (const TSharedPtr<FAvalancheManagedInstance>& ManagedInstance : ManagedInstances)
		{
			URemoteControlPreset* RemoteControlPreset = ManagedInstance ? ManagedInstance->GetRemoteControlPreset() : nullptr;
			if (!RemoteControlPreset)
			{
				continue;
			}
			
			const TArray<TWeakPtr<FRemoteControlEntity>> ExposedEntities = RemoteControlPreset->GetExposedEntities<FRemoteControlEntity>();
			NewItems.Reserve(ExposedEntities.Num());
			
			for (const TWeakPtr<FRemoteControlEntity>& EntityWeakPtr : ExposedEntities)
			{
				if (const TSharedPtr<FRemoteControlEntity> Entity = EntityWeakPtr.Pin())
				{
					const FAvalancheRemoteControlValue* EntityValue = GetSelectedPageEntityValue(Entity);

					if (!EntityValue)
					{
						// If the page doesn't already have a value, we get it from the template's default values.
						const FAvalancheRemoteControlValue* const DefaultEntityValue = ManagedInstance->GetDefaultRemoteControlValues().GetEntityValue(Entity->GetId()); 
						
						if (!DefaultEntityValue)
						{
							UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Entity \"%s\" (id:%s) doesn't have a template default value."),
								*Entity->GetLabel().ToString(), *Entity->GetId().ToString());
							// TODO: UX improvement: instead of skipping, could add empty element, with error mark (and error message in tooltip).
							continue;
						}

						// Ensure the default values have the default flag.
						ensure(DefaultEntityValue->bIsDefault == true);
						
						// WYSIWYG (Solution):
						// Capture the default value (flagged as default) in the current page to ensure all values will be applied to runtime RCP.
						if (!SetSelectedPageEntityValue(Entity, *DefaultEntityValue))
						{
							UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Entity \"%s\" (id:%s): failed to set value in currently selected page."),
								*Entity->GetLabel().ToString(), *Entity->GetId().ToString());
						}

						EntityValue = DefaultEntityValue;
					}
					
					// Update Exposed entity value with value from page.
					using namespace UE::AvalancheRemoteControl;
					if (const EAvaRemoteControlResult Result = SetValueOfEntity(Entity, EntityValue->Value); Failed(Result))
					{
						UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Entity \"%s\" (id:%s): failed to set entity value: %s."),
							*Entity->GetLabel().ToString(), *Entity->GetId().ToString(), *EnumToString(Result));
						// TODO: UX improvement: instead of skipping, could add empty element, with error mark (and error message in tooltip).
						continue;
					}

					{
						const bool bEntityControlled = ManagedInstance->GetDefaultRemoteControlValues().EntitiesControlledByController.Contains(Entity->GetId());
						NewItems.Add({Entity.ToSharedRef(), bEntityControlled});
					}
				}
			}
		}

		PropertyContainer->RebuildList();

		bool bRecreateList = NewItems.Num() != PropertyItems.Num();

		if (!bRecreateList)
		{
			for (int32 PropertyIdx = 0; PropertyIdx < NewItems.Num(); ++PropertyIdx)
			{
				const FAvaRCPropertyItemPtr& PropertyItem = PropertyItems[PropertyIdx];
				const FAvaPropertyDetails& NewItem = NewItems[PropertyIdx];

				if (!PropertyItem.IsValid())
				{
					bRecreateList = true;
					break;
				}

				TSharedPtr<FRemoteControlEntity> PropertyEntity = PropertyItem->GetEntity();

				if (!PropertyEntity.IsValid())
				{
					bRecreateList = true;
					break;
				}

				if (PropertyEntity.Get() != NewItem.Entity.ToSharedPtr().Get())
				{
					bRecreateList = true;
					break;
				}
			}
		}

		if (!bRecreateList)
		{
			RefreshTable();
			return;
		}

		PropertyItems.Empty();

		for (const FAvaPropertyDetails& NewItem : NewItems)
		{
			PropertyItems.Add(MakeShared<FAvaRCPropertyItem>(NewItem.Entity, NewItem.bEntityControlled));
		}
	}
}

// Remarks:
// This is called by URemoteControlPreset::OnEndFrame() as a result of an entity being modified.
// However, it doesn't seem to be called (or not always) if the entity is modified by a controller action. 
void SAvaPageRemoteControlProps::OnRemoteControlExposedPropertiesModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedProperties)
{
	if (!IsValid(InPreset) ||!HasRemoteControlPreset(InPreset))
	{
		return;
	}

	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		bool bModified = false;

		for (const FGuid& Id : InModifiedProperties)
		{
			if (const TSharedPtr<FRemoteControlEntity> Entity = InPreset->GetExposedEntity<FRemoteControlEntity>(Id).Pin())
			{
				FAvalancheRemoteControlValue EntityValue;

				{
					using namespace UE::AvalancheRemoteControl;
					if (const EAvaRemoteControlResult Result = GetValueOfEntity(Entity, EntityValue.Value); Failed(Result))
					{
						UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Unable to get value of entity \"%s\": %s"),
							*Entity->GetLabel().ToString(), *EnumToString(Result));
						continue;
					}
				}
				
				const FAvalancheRemoteControlValue* StoredEntityValue = GetSelectedPageEntityValue(Entity);

				if (StoredEntityValue && StoredEntityValue->IsSameValueAs(EntityValue))
				{
					continue;	// Skip if value is identical.
				}

				if (!SetSelectedPageEntityValue(Entity, EntityValue))
				{
					UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Unable to set page entity value for: \"%s\""), *Entity->GetLabel().ToString());
					continue;
				}

				bModified = true;
			}
		}

		if (bModified)
		{
			PlaylistEditor->MarkAsModified();
		}
	}
}

void SAvaPageRemoteControlProps::OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds)
{
	if (!IsValid(InPreset) || !HasRemoteControlPreset(InPreset))
	{
		return;
	}

	TSet<FGuid> EntityIds;
	for (const FGuid& ControllerId : InModifiedControllerIds)
	{
		UE::AvalancheRemoteControl::GetEntitiesControlledByController(InPreset, InPreset->GetController(ControllerId), EntityIds);
	}

	// If a controller changed, we need to propagate the refresh of the field's widgets.
	// Optimization: only refresh the widgets that are related to the modified controllers.
	RefreshTable(EntityIds);

	// It seems OnPropertyChangedDelegate (OnExposedPropertiesModified()) is not called when properties are
	// changed by controllers. Ensure the values are saved by calling our handler directly.
	OnRemoteControlExposedPropertiesModified(InPreset, EntityIds);
}

void SAvaPageRemoteControlProps::BindRemoteControlDelegates(URemoteControlPreset* InPreset)
{
	if (IsValid(InPreset))
	{
		if (!InPreset->OnEntityExposed().IsBoundToObject(this))
		{
			InPreset->OnEntityExposed().AddSP(this, &SAvaPageRemoteControlProps::OnRemoteControlEntitiesExposed);
		}

		if (!InPreset->OnEntityUnexposed().IsBoundToObject(this))
		{
			InPreset->OnEntityUnexposed().AddSP(this, &SAvaPageRemoteControlProps::OnRemoteControlEntitiesUnexposed);
		}

		if (!InPreset->OnEntitiesUpdated().IsBoundToObject(this))
		{
			InPreset->OnEntitiesUpdated().AddSP(this, &SAvaPageRemoteControlProps::OnRemoteControlEntitiesUpdated);
		}

		if (!InPreset->OnExposedPropertiesModified().IsBoundToObject(this))
		{
			InPreset->OnExposedPropertiesModified().AddSP(this, &SAvaPageRemoteControlProps::OnRemoteControlExposedPropertiesModified);
		}

		if (!InPreset->OnControllerModified().IsBoundToObject(this))
		{
			InPreset->OnControllerModified().AddSP(this, &SAvaPageRemoteControlProps::OnRemoteControlControllerModified);
		}
	}
}

bool SAvaPageRemoteControlProps::HasRemoteControlPreset(const URemoteControlPreset* InPreset) const
{
	for (const TSharedPtr<FAvalancheManagedInstance>& ManagedInstance : ManagedInstances)
	{
		if (ManagedInstance && ManagedInstance->GetRemoteControlPreset() == InPreset)
		{
			return true;
		}
	}
	return false;
}

FAvalanchePage* SAvaPageRemoteControlProps::GetActivePage() const
{
	if (ActivePageId == FAvalanchePage::InvalidPageId)
	{
		return nullptr;
	}

	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		if (PlaylistEditor->IsPlaylistValid())
		{
			FAvalanchePage& Page = PlaylistEditor->GetPlaylist()->GetPage(ActivePageId);
			return Page.IsValidPage() ? &Page : nullptr;
		}
	}

	return nullptr;
}

const FAvalancheRemoteControlValue* SAvaPageRemoteControlProps::GetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity) const
{
	if (InRemoteControlEntity.IsValid())
	{
		if (const FAvalanchePage* Page = GetActivePage())
		{
			return Page->GetRemoteControlEntityValue(InRemoteControlEntity->GetId()); 
		}
	}

	return nullptr;
}

bool SAvaPageRemoteControlProps::SetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FAvalancheRemoteControlValue& InValue) const
{
	if (InRemoteControlEntity.IsValid())
	{
		if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
		{
			if (PlaylistEditor->IsPlaylistValid())
			{
				// Using the playlist API for event propagation.
				UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist(); 
				return Playlist->SetRemoteControlEntityValue(ActivePageId, InRemoteControlEntity->GetId(), InValue);
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

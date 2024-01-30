// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRCControllerPanel.h"

#include "AvaRCControllerItem.h"
#include "Behaviour/Builtin/Path/RCSetAssetByPathBehaviour.h"
#include "Controller/RCController.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Playback/AvalancheRemoteControl.h"
#include "Playlist/AvaPlaylistEditor.h"
#include "Playlist/AvalanchePage.h"
#include "Playlist/AvalancheManagedInstanceCache.h"
#include "RCVirtualProperty.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "AvaRCControllerPanel"

const FName SAvaRCControllerPanel::ControllerColumnName = "ControllerColumn";
const FName SAvaRCControllerPanel::ValueColumnName = "ValueColumn";

FAvaRCControllerHeaderRowExtensionDelegate SAvaRCControllerPanel::HeaderRowExtensionDelegate;
TMap<FName, TArray<FAvaRCControllerTableRowExtensionDelegate>> SAvaRCControllerPanel::TableRowExtensionDelegates;

TArray<FAvaRCControllerTableRowExtensionDelegate>& SAvaRCControllerPanel::GetTableRowExtensionDelegates(FName InExtensionName)
{
	return TableRowExtensionDelegates.FindOrAdd(InExtensionName);
}

void SAvaRCControllerPanel::Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor)
{
	PlaylistEditorWeak = InPlaylistEditor;
	ActivePageId = FAvalanchePage::InvalidPageId;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SAssignNew(ControllerContainer, SListView<FAvaRCControllerItemPtr>)
			.ListItemsSource(&ControllerItems)
			.SelectionMode(ESelectionMode::None)
			.OnGenerateRow(this, &SAvaRCControllerPanel::OnGenerateControllerRow)
			.HeaderRow(
				SNew(SHeaderRow)
				.CanSelectGeneratedColumn(true)
				+ SHeaderRow::Column(ControllerColumnName)
				.DefaultLabel(LOCTEXT("Controller", "Controller"))
				.FillWidth(0.2f)
				+ SHeaderRow::Column(ValueColumnName)
				.DefaultLabel(LOCTEXT("Value", "Value"))
				.FillWidth(0.8f)
			)
		]
	];
	
	Refresh({});
}

bool SAvaRCControllerPanel::HasRemoteControlPreset(const URemoteControlPreset* InPreset) const
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

void SAvaRCControllerPanel::OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds)
{
	Refresh(InSelectedPageIds);
}

void SAvaRCControllerPanel::UpdatePropertyRowGenerators(int32 InNumGenerators)
{
	if (PropertyRowGenerators.Num() != InNumGenerators)
	{
		TArray<TUniquePtr<FPropertyRowGeneratorWrapper>> GeneratorPool = MoveTemp(PropertyRowGenerators);

		PropertyRowGenerators.Empty(InNumGenerators);

		for (int32 Index = 0; Index < InNumGenerators; ++Index)
		{
			if (Index < GeneratorPool.Num())
			{
				PropertyRowGenerators.Add(MoveTemp(GeneratorPool[Index]));
			}
			else
			{
				PropertyRowGenerators.Add(MakeUnique<FPropertyRowGeneratorWrapper>(this));
			}
		}
	}
}

void SAvaRCControllerPanel::RefreshForManagedInstance(int32 InInstanceIndex, const FAvalancheManagedInstance& InManagedInstance, const FAvalanchePage& InPage)
{
	URemoteControlPreset* const Preset = InManagedInstance.GetRemoteControlPreset();

	if (!Preset || !PropertyRowGenerators.IsValidIndex(InInstanceIndex)
		|| !PropertyRowGenerators[InInstanceIndex] || !PropertyRowGenerators[InInstanceIndex]->PropertyRowGenerator)
	{
		return;
	}
	
	BindRemoteControlDelegates(Preset);

	if (const TSharedPtr<FStructOnScope> StructOnScope = Preset->GetControllerContainerStructOnScope())
	{
		IPropertyRowGenerator& PropertyRowGenerator = *PropertyRowGenerators[InInstanceIndex]->PropertyRowGenerator;
		
		// We need one of those for each Preset.
		PropertyRowGenerator.SetStructure(StructOnScope);
		PropertyRowGenerators[InInstanceIndex]->PresetWeak = Preset;	// Keep track for proper event routing.
		
		const TArray<TSharedRef<IDetailTreeNode>>& RootTreeNodes = PropertyRowGenerator.GetRootTreeNodes();
		check(RootTreeNodes.Num() <= 1);

		for (const TSharedRef<IDetailTreeNode>& RootTreeNode : RootTreeNodes)
		{
			TArray<TSharedRef<IDetailTreeNode>> Children;
			RootTreeNode->GetChildren(Children);

			for (TSharedRef<IDetailTreeNode>& Child : Children)
			{
				FProperty* const Property = Child->CreatePropertyHandle()->GetProperty();
				check(Property);

				if (Property->IsA<FStrProperty>() || Property->IsA<FTextProperty>())
				{
					Property->SetMetaData(TEXT("MultiLine"), TEXT("true"));
				}

				URCVirtualPropertyBase* const VirtualProperty = Preset->GetController(Property->GetFName());

				if (!VirtualProperty)
				{
					continue;
				}

				// Apply the page value to the controller (sync the managed RCP's controller to the page value).
				{
					const FAvalancheRemoteControlValue* ControllerValueFromPage = InPage.GetRemoteControlControllerValue(VirtualProperty->Id);
					if (!ControllerValueFromPage)
					{
						// If the value is not set in the page, fallback to default value from the template.
						ControllerValueFromPage = InManagedInstance.GetDefaultRemoteControlValues().GetControllerValue(VirtualProperty->Id);

						// Note: we are not adding the controller's default value in the page. (Tentative)
						// Reason: Given that controller values are only applied for the managed RCP (ui),
						// there is no need to save it in the page unless the user has modified from default.
						// This way, we only keep user modified values.
						
						if (!ControllerValueFromPage)
						{
							UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Controller \"%s\" (id:%s) doesn't have a template default value."),
								*VirtualProperty->DisplayName.ToString(), *VirtualProperty->Id.ToString());
						}
					}
					if (ControllerValueFromPage)
					{
						using namespace UE::AvalancheRemoteControl;
						FAvalancheRemoteControlValue CurrentControllerValue;
						EAvaRemoteControlResult RemoteControlResult = GetValueOfController(VirtualProperty, CurrentControllerValue.Value);
						if (!Failed(RemoteControlResult))
						{
							if (!CurrentControllerValue.IsSameValueAs(*ControllerValueFromPage))
							{
								// This serves only to sync the Managed RCP's controller values to the page values.
								// We want to be able to do this without affecting the entity values (preserve WYSIWYG).
								// So, we temporarily disable the controller's behaviours.
								FScopedPushControllerBehavioursEnable PushBehavioursEnable(VirtualProperty, false);
								if (Failed(RemoteControlResult = SetValueOfController(VirtualProperty, ControllerValueFromPage->Value)))
								{
									UE_LOG(LogAvaMediaRemoteControl, Error,
										TEXT("Controller \"%s\" (id:%s): failed to set value in currently selected page: %s."),
										*VirtualProperty->DisplayName.ToString(), *VirtualProperty->Id.ToString(), *EnumToString(RemoteControlResult));
								}
							}
						}
						else
						{
							UE_LOG(LogAvaMediaRemoteControl, Error,
								TEXT("Controller \"%s\" (id:%s): failed to get value in currently selected page: %s."),
								*VirtualProperty->DisplayName.ToString(), *VirtualProperty->Id.ToString(), *EnumToString(RemoteControlResult));
						}
					}
				}

				if (URCController* const Controller = Cast<URCController>(VirtualProperty))
				{
					for (URCBehaviour* const Behavior : Controller->Behaviours)
					{
						if (URCSetAssetByPathBehaviour* const AssetByPathBehavior = Cast<URCSetAssetByPathBehaviour>(Behavior))
						{
							AssetByPathBehavior->UpdateTargetEntity();
						}
					}

					ControllerItems.Add(MakeShared<FAvaRCControllerItem>(InInstanceIndex, InManagedInstance.GetSourceAssetPath().GetAssetFName(), Controller, Child));
				}
			}
		}
	}
}


void SAvaRCControllerPanel::Refresh(const TArray<int32>& InSelectedPageIds)
{
	// Request to Rebuild on next tick
	ControllerContainer->RebuildList();

	ActivePageId = InSelectedPageIds.IsEmpty() ? FAvalanchePage::InvalidPageId : InSelectedPageIds[0];

	const UAvalanchePlaylist* Playlist = GetPlaylist();
	const FAvalanchePage& Page = GetActivePage(Playlist);
	
	if (!Page.IsValidPage())
	{
		ControllerItems.Empty();
       	ManagedInstances.Reset();
		return;
	}
		
	ManagedInstances = FAvaPlaylistEditor::GetManagedInstancesForPage(Playlist, Page);

	int32 NumItems = 0;
	for (const TSharedPtr<FAvalancheManagedInstance>& ManagedInstance : ManagedInstances)
	{
		const URemoteControlPreset* Preset = ManagedInstance ? ManagedInstance->GetRemoteControlPreset() : nullptr;
		if (Preset)
		{
			NumItems += Preset->GetNumControllers();
		}
	}

	ControllerItems.Empty(NumItems);
	
	UpdatePropertyRowGenerators(ManagedInstances.Num());
	UpdatePageSummary(false);

	for (int32 Index = 0; Index < ManagedInstances.Num(); ++Index)
	{
		RefreshForManagedInstance(Index, *ManagedInstances[Index], Page);
	}

	ControllerItems.Sort([](const FAvaRCControllerItemPtr& A, const FAvaRCControllerItemPtr& B)
	{
		if (A->GetInstanceIndex() == B->GetInstanceIndex())
		{
			return A->GetDisplayIndex() < B->GetDisplayIndex();
		}
		return A->GetInstanceIndex() < B->GetInstanceIndex();
	});
}

TSharedRef<ITableRow> SAvaRCControllerPanel::OnGenerateControllerRow(FAvaRCControllerItemPtr InItem
	, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return InItem->CreateWidget(SharedThis(this), InOwnerTable);
}

void SAvaRCControllerPanel::UpdateDefaultValuesAndRefresh()
{
	// Remark: The RC values might be already updated in SAvaPageRemoteControlProps.
	// But order of callback is not guaranteed. Code could reach here first, so
	// it needs to update and refresh just in case. Calling UpdateRemoteControlValues
	// multiple time (from different code paths) is harmless (fast if nothing changed).
	
	const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin();
	if (!PlaylistEditor)
	{
		return;
	}
	
	UAvalanchePlaylist* Playlist = PlaylistEditor->GetPlaylist();
	if (!Playlist)
	{
		return;
	}
	
	const FAvalanchePage& Page = GetActivePage(Playlist);
	if (!Page.IsValidPage())
	{
		return;
	}
	
	ManagedInstances = FAvaPlaylistEditor::GetManagedInstancesForPage(Playlist, ActivePageId);
					
	if (ManagedInstances.IsEmpty())
	{
		return;
	}
	
	FAvalancheRemoteControlValues MergedDefaultRCValues;
	FAvaPlaylistEditor::MergeDefaultRemoteControlValues(ManagedInstances, MergedDefaultRCValues);

	// Using the playlist API for event propagation.
	constexpr bool bUpdateDefaults = true;	
	if (Playlist->UpdateRemoteControlValues(ActivePageId, MergedDefaultRCValues, bUpdateDefaults) != EAvaRemoteControlChanges::None)
	{
		PlaylistEditor->MarkAsModified();
	}
	
	Refresh({ActivePageId});
}

void SAvaRCControllerPanel::OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds)
{
	if (!IsValid(InPreset) || !HasRemoteControlPreset(InPreset))
	{
		return;
	}

	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		bool bModified = false;

		for (const FGuid& Id : InModifiedControllerIds)
		{
			if (URCVirtualPropertyBase* Controller = InPreset->GetController(Id))
			{
				using namespace UE::AvalancheRemoteControl;
				FAvalancheRemoteControlValue ControllerValue;
				const EAvaRemoteControlResult Result = GetValueOfController(Controller, ControllerValue.Value);

				if (Failed(Result))
				{
					UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Unable to get value of controller \"%s\" (id:%s): %s."),
						   *Controller->DisplayName.ToString(), *Controller->Id.ToString(), *EnumToString(Result));
					continue;
				}

				const FAvalancheRemoteControlValue* StoredControllerValue = GetSelectedPageControllerValue(Controller);

				if (StoredControllerValue && ControllerValue.IsSameValueAs(*StoredControllerValue))
				{
					continue;	// Skip if value is identical.
				}

				if (!SetSelectedPageControllerValue(Controller, ControllerValue))
				{
					UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Unable to set page controller value for: \"%s\""), *Controller->DisplayName.ToString());
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


void SAvaRCControllerPanel::BindRemoteControlDelegates(URemoteControlPreset* InPreset)
{
	if (IsValid(InPreset))
	{
		if (!InPreset->OnControllerAdded().IsBoundToObject(this))
		{
			InPreset->OnControllerAdded().AddSP(this, &SAvaRCControllerPanel::OnRemoteControlControllerAdded);
		}

		if (!InPreset->OnControllerRemoved().IsBoundToObject(this))
		{
			InPreset->OnControllerRemoved().AddSP(this, &SAvaRCControllerPanel::OnRemoteControlControllerRemoved);
		}

		if (!InPreset->OnControllerRenamed().IsBoundToObject(this))
		{
			InPreset->OnControllerRenamed().AddSP(this, &SAvaRCControllerPanel::OnRemoteControlControllerRenamed);
		}

		if (!InPreset->OnControllerModified().IsBoundToObject(this))
		{
			InPreset->OnControllerModified().AddSP(this, &SAvaRCControllerPanel::OnRemoteControlControllerModified);
		}
	}
}

const FAvalancheRemoteControlValue* SAvaRCControllerPanel::GetSelectedPageControllerValue(const URCVirtualPropertyBase* InController) const
{
	const FAvalanchePage& Page = GetActivePage();
	if (IsValid(InController) && Page.IsValidPage())
	{
		return Page.GetRemoteControlControllerValue(InController->Id);
	}
	return nullptr;
}

bool SAvaRCControllerPanel::SetSelectedPageControllerValue(const URCVirtualPropertyBase* InController, const FAvalancheRemoteControlValue& InValue) const
{
	UAvalanchePlaylist* Playlist = GetPlaylist();
	if (IsValid(InController) && Playlist)
	{
		// Using the playlist API for event propagation.
		return Playlist->SetRemoteControlControllerValue(ActivePageId, InController->Id, InValue);
	}
	return false;
}

void SAvaRCControllerPanel::UpdatePageSummary(bool bInIsPresetChanged)
{
	FAvalanchePage& Page = GetActivePageMutable();
	if (Page.IsValidPage())
	{
		TArray<const URemoteControlPreset*> Presets;
		Presets.Reserve(ManagedInstances.Num());
		for (const TSharedPtr<FAvalancheManagedInstance>& ManagedInstance : ManagedInstances)
		{
			if (ManagedInstance && ManagedInstance->GetRemoteControlPreset())
			{
				Presets.Add(ManagedInstance->GetRemoteControlPreset());
			}
		}					
		Page.UpdatePageSummary(Presets, bInIsPresetChanged);
	}
}

UAvalanchePlaylist* SAvaRCControllerPanel::GetPlaylist() const
{
	if (const TSharedPtr<FAvaPlaylistEditor> PlaylistEditor = PlaylistEditorWeak.Pin())
	{
		return PlaylistEditor->GetPlaylist();
	}
	return nullptr;
}

const FAvalanchePage& SAvaRCControllerPanel::GetActivePage(const UAvalanchePlaylist* InPlaylist) const
{
	if (IsValid(InPlaylist) && ActivePageId != FAvalanchePage::InvalidPageId)
	{
		return InPlaylist->GetPage(ActivePageId);
	}
	return FAvalanchePage::NullPage;
}

FAvalanchePage& SAvaRCControllerPanel::GetActivePageMutable(UAvalanchePlaylist* InPlaylist) const
{
	if (IsValid(InPlaylist) && ActivePageId != FAvalanchePage::InvalidPageId)
	{
		return InPlaylist->GetPage(ActivePageId);
	}
	return FAvalanchePage::NullPage;
}

SAvaRCControllerPanel::FPropertyRowGeneratorWrapper::FPropertyRowGeneratorWrapper(SAvaRCControllerPanel* InParentPanel)
	: ParentPanel(InParentPanel)
{
	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties   = true;
	Args.bAllowMultipleTopLevelObjects = false;
	Args.NotifyHook = this;
	PropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);
}

SAvaRCControllerPanel::FPropertyRowGeneratorWrapper::~FPropertyRowGeneratorWrapper()
{
	if (PropertyRowGenerator)
	{
		PropertyRowGenerator->OnFinishedChangingProperties().RemoveAll(this);
	}
}

void SAvaRCControllerPanel::FPropertyRowGeneratorWrapper::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	FNotifyHook::NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);
	
	if (URemoteControlPreset* Preset = PresetWeak.Get())
	{
		Preset->OnModifyController(InPropertyChangedEvent);
		if (ParentPanel && InPropertyChangedEvent.ChangeType & EPropertyChangeType::ValueSet)
		{
			ParentPanel->UpdatePageSummary(true);
		}
	}
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/NotifyHook.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;
class FAvaRCControllerItem;
class FAvalancheManagedInstance;
class IPropertyRowGenerator;
class ITableRow;
class SAvaRCControllerPanel;
class SHeaderRow;
class STableViewBase;
class UAvalanchePlaylist;
class URCVirtualPropertyBase;
class URemoteControlPreset;
struct FAvalanchePage;
struct FAvalancheRemoteControlValue;
struct FPropertyChangedEvent;
template <typename ItemType> class SListView;

using FAvaRCControllerItemPtr = TSharedPtr<FAvaRCControllerItem>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaRCControllerHeaderRowExtensionDelegate, TSharedRef<SAvaRCControllerPanel> Panel, 
	TSharedRef<SHeaderRow>& HeaderRow)
DECLARE_DELEGATE_ThreeParams(FAvaRCControllerTableRowExtensionDelegate, TSharedRef<SAvaRCControllerPanel> Panel, 
	TSharedRef<const FAvaRCControllerItem> ItemPtr, TSharedPtr<SWidget>& CurrentWidget)

class SAvaRCControllerPanel : public SCompoundWidget
{
public:
	static const FName ControllerColumnName;
	static const FName ValueColumnName;

	static FAvaRCControllerHeaderRowExtensionDelegate& GetHeaderRowExtensionDelegate() { return HeaderRowExtensionDelegate; }
	static TArray<FAvaRCControllerTableRowExtensionDelegate>& GetTableRowExtensionDelegates(FName InExtensionName);

	SLATE_BEGIN_ARGS(SAvaRCControllerPanel) {}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	
	bool HasRemoteControlPreset(const URemoteControlPreset* InPreset) const;

	void OnPageSelectionChanged(const TArray<int32>& InSelectedPageIds);
	
	void Refresh(const TArray<int32>& InSelectedPageIds);
	
	TSharedRef<ITableRow> OnGenerateControllerRow(FAvaRCControllerItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

private:
	static FAvaRCControllerHeaderRowExtensionDelegate HeaderRowExtensionDelegate;
	static TMap<FName, TArray<FAvaRCControllerTableRowExtensionDelegate>> TableRowExtensionDelegates;

	void UpdatePropertyRowGenerators(int32 InNumGenerators);
	
	void RefreshForManagedInstance(int32 InInstanceIndex, const FAvalancheManagedInstance& InManagedInstance, const FAvalanchePage& InPage);
	
	/** Update the current page's remote control values from the defaults then refresh the widget. */
	void UpdateDefaultValuesAndRefresh();

	void OnRemoteControlControllerAdded(URemoteControlPreset* InPreset, const FName NewControllerName, const FGuid& InControllerId) { UpdateDefaultValuesAndRefresh(); }
	void OnRemoteControlControllerRemoved(URemoteControlPreset* InPreset, const FGuid& InControllerId) { UpdateDefaultValuesAndRefresh(); }
	void OnRemoteControlControllerRenamed(URemoteControlPreset* InPreset, const FName InOldLabel, const FName InNewLabel) { UpdateDefaultValuesAndRefresh(); }
	void OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds);
	void BindRemoteControlDelegates(URemoteControlPreset* InPreset);

	const FAvalancheRemoteControlValue* GetSelectedPageControllerValue(const URCVirtualPropertyBase* InController) const;
	bool SetSelectedPageControllerValue(const URCVirtualPropertyBase* InController, const FAvalancheRemoteControlValue& InValue) const;
	void UpdatePageSummary(bool bInIsPresetChanged);

	UAvalanchePlaylist* GetPlaylist() const;
	const FAvalanchePage& GetActivePage(const UAvalanchePlaylist* InPlaylist) const;
	const FAvalanchePage& GetActivePage() const { return GetActivePage(GetPlaylist()); }
	FAvalanchePage& GetActivePageMutable(UAvalanchePlaylist* InPlaylist) const;
	FAvalanchePage& GetActivePageMutable() const { return GetActivePageMutable(GetPlaylist()); }

	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	class FPropertyRowGeneratorWrapper : public FNotifyHook
	{
	public:
		TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
		TWeakObjectPtr<URemoteControlPreset> PresetWeak;
		SAvaRCControllerPanel* ParentPanel = nullptr;

		FPropertyRowGeneratorWrapper(SAvaRCControllerPanel* InParentPanel);
		virtual ~FPropertyRowGeneratorWrapper();

		//~ Begin FNotifyHook
		virtual void NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged) override;
		//~ End FNotifyHook
	};
	
	TArray<TUniquePtr<FPropertyRowGeneratorWrapper>> PropertyRowGenerators;

	TArray<TSharedPtr<FAvalancheManagedInstance>> ManagedInstances;
	
	TSharedPtr<SListView<FAvaRCControllerItemPtr>> ControllerContainer;
	
	TArray<FAvaRCControllerItemPtr> ControllerItems;

	int32 ActivePageId = -1;
};

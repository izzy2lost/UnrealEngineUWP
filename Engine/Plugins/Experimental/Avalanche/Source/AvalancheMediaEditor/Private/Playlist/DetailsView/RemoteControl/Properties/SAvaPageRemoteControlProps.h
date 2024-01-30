// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RemoteControlPreset.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/ITableRow.h"

class FAvaPlaylistEditor;
class FAvaRCPropertyItem;
class FAvalancheManagedInstance;
class SAvaPageRemoteControlProps;
struct FAvalanchePage;
struct FAvalancheRemoteControlValue;

using FAvaRCPropertyItemPtr = TSharedPtr<FAvaRCPropertyItem>;

DECLARE_MULTICAST_DELEGATE_TwoParams(FAvaRCPropertyHeaderRowExtensionDelegate, TSharedRef<SAvaPageRemoteControlProps> Panel,
	TSharedRef<SHeaderRow>& HeaderRow)
DECLARE_DELEGATE_ThreeParams(FAvaRCPropertyTableRowExtensionDelegate, TSharedRef<SAvaPageRemoteControlProps> Panel,
	TSharedRef<const FAvaRCPropertyItem> ItemPtr, TSharedPtr<SWidget>& CurrentWidget)

/**
 * The page props implementation for remote control fields.
 */
class SAvaPageRemoteControlProps : public SCompoundWidget
{
public:
	static const FName PropertyColumnName;
	static const FName ValueColumnName;

	static FAvaRCPropertyHeaderRowExtensionDelegate& GetHeaderRowExtensionDelegate() { return HeaderRowExtensionDelegate; }
	static TArray<FAvaRCPropertyTableRowExtensionDelegate>& GetTableRowExtensionDelegates(FName InExtensionName);

	SLATE_BEGIN_ARGS(SAvaPageRemoteControlProps) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FAvaPlaylistEditor> InPlaylistEditor);
	virtual ~SAvaPageRemoteControlProps() override;

	/** Update the current page's remote control values from the defaults then refresh the widget. */
	void UpdateDefaultValuesAndRefresh(const TArray<int32>& InSelectedPageIds);

	/** Refreshes the content of this widget. */
	void Refresh(const TArray<int32>& InSelectedPageIds);

private:
	static FAvaRCPropertyHeaderRowExtensionDelegate HeaderRowExtensionDelegate;
	static TMap<FName, TArray<FAvaRCPropertyTableRowExtensionDelegate>> TableRowExtensionDelegates;

	void OnRemoteControlEntitiesExposed(URemoteControlPreset* InPreset, const FGuid& InEntityId) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlEntitiesUnexposed(URemoteControlPreset* InPreset, const FGuid& InEntityId) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlEntitiesUpdated(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedEntities) { UpdateDefaultValuesAndRefresh({GetActivePageId()}); }
	void OnRemoteControlExposedPropertiesModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedProperties);
	void OnRemoteControlControllerModified(URemoteControlPreset* InPreset, const TSet<FGuid>& InModifiedControllerIds);

	void BindRemoteControlDelegates(URemoteControlPreset* InPreset);

	bool HasRemoteControlPreset(const URemoteControlPreset* InPreset) const;

	/** Returns the currently selected page if 1 page is currently selected, returns nullptr otherwise. */
	FAvalanchePage* GetActivePage() const;

	/** Returns the currently selected page id if 1 page is currently selected, returns InvalidPageId otherwise. */
	int32 GetActivePageId() const { return ActivePageId; }

	/**
	 * Get Selected Page's entity value corresponding to the given entity (using entity Id to match).
	 * @return pointer to page's entity value, null if not found.
	 */
	const FAvalancheRemoteControlValue* GetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity) const;

	/**
	 * Set (or add) Selected Page's entity value corresponding to the given entity (using entity Id to match).
	 * @return true if it succeeded, false otherwise.
	 */
	bool SetSelectedPageEntityValue(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FAvalancheRemoteControlValue& InValue) const;

	TSharedRef<ITableRow> OnGenerateControllerRow(FAvaRCPropertyItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	void RefreshTable(const TSet<FGuid>& InEntityIds = TSet<FGuid>());

	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;
	
	TArray<TSharedPtr<FAvalancheManagedInstance>> ManagedInstances;

	/** The widget that lists the property rows. */
	TSharedPtr<SListView<FAvaRCPropertyItemPtr>> PropertyContainer;
	
	/** The data used to back the properties container list view. */
	TArray<FAvaRCPropertyItemPtr> PropertyItems;

	int32 ActivePageId = -1;
};

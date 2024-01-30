// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "EditorUndoClient.h"
#include "Engine/EngineTypes.h"
#include "IAvaOutlinerModule.h"
#include "Item/AvaOutlinerItemId.h"
#include "Item/AvaOutlinerItemProxy.h"
#include "Item/IAvaOutlinerItem.h"
#include "ItemProxies/AvaOutlinerItemProxyRegistry.h"
#include "ItemProxies/IAvaOutlinerItemProxyFactory.h"
#include "TickableEditorObject.h"

class FAvaEditorSelection;
class FAvaOutlinerTreeRoot;
class FAvaOutlinerView;
class FEditorModeTools;
class FTransaction;
class FUICommandList;
class IAvaOutlinerAction;
class IAvaOutlinerProvider;
class UAvaOutlinerSubsystem;
enum class EItemDropZone;
struct FAvaOutlinerSaveState;
struct FAvaSceneItem;

/** 
 * The Outliner Object that is commonly instanced once per World
 * (unless for advanced use where there are different outliner instances with different item ordering and behaviors).
 * This is the object that dictates core outliner behavior like how items are sorted, which items are allowed, etc.
 * Views are the objects that take this core behavior and show a part of it (e.g. through filters).
 */
class AVALANCHEOUTLINER_API FAvaOutliner
	: public TSharedFromThis<FAvaOutliner>
	, public FTickableEditorObject
	, public FEditorUndoClient
{
public:
	FAvaOutliner(IAvaOutlinerProvider& InOutlinerProvider);
	
	virtual ~FAvaOutliner() override;
	
	/**
	 * Gets the Outliner Subsystem of the World this Outliner is responsible for
	 * NOTE: The OutlinerSubsystem's Outliner Instance should be the same as this one in the default implementation 
	 * but can differ if there are multiple FAvaOutliner instances being used in custom implementations
	 */
	UAvaOutlinerSubsystem* GetOutlinerSubsystem() const;

	/**
	 * Determines whether the given actor can be presented in the Outliner, at all.
	 * This is a permanent check unlike filters that are temporary.
	 */
	bool IsActorAllowedInOutliner(const AActor* InActor) const;

	/**
	 * Determines whether the given scene component can be presented in the Outliner, at all.
	 * This is a permanent check unlike filters that are temporary.
	 */
	bool IsComponentAllowedInOutliner(const USceneComponent* InComponent) const;

	bool CanProcessActorSpawn(AActor* InActor) const;
	
	/** Sets the Command List that the Outliner Views will use to append their Command Lists to */
	void SetBaseCommandList(const TSharedPtr<FUICommandList>& InBaseCommandList);

	TSharedPtr<FUICommandList> GetBaseCommandList() const;

	/** Serializes the Outliner Save State to the given archive */
	void Serialize(FArchive& Ar);

	DECLARE_MULTICAST_DELEGATE(FOnOutlinerLoaded);
	FOnOutlinerLoaded OnOutlinerLoaded;

	IAvaOutlinerProvider& GetProvider() const { return OutlinerProvider; }

	FAvaOutlinerItemProxyRegistry& GetItemProxyRegistry() { return ItemProxyRegistry; }

	/** Gathers the Type Names of all the Item Proxies that are registered both in the outliner proxy registry and the module's */
	TArray<FName> GetRegisteredItemProxyTypeNames() const;

	/** Gathers all previously existing and new Item Proxies for a given Item */
	void GetItemProxiesForItem(const FAvaOutlinerItemPtr& InItem, TArray<TSharedPtr<FAvaOutlinerItemProxy>>& OutItemProxies);

	/** Tries to find the Item Proxy Factory for the given Item Proxy Type Name */
	IAvaOutlinerItemProxyFactory* GetItemProxyFactory(FName InItemProxyTypeName) const
	{
		// First look for the Registry in Outliner
		if (IAvaOutlinerItemProxyFactory* Factory = ItemProxyRegistry.GetItemProxyFactory(InItemProxyTypeName))
		{
			return Factory;
		}
		// Fallback to finding the Factory in the Module if the Outliner did not find it
		return IAvaOutlinerModule::Get().GetItemProxyRegistry().GetItemProxyFactory(InItemProxyTypeName);
	}

	/** Tries to find the Item Proxy Factory for the given Item Proxy Type Name */
	template<typename InItemProxyType, typename = typename TEnableIf<TIsDerivedFrom<InItemProxyType, FAvaOutlinerItemProxy>::IsDerived>::Type>
	IAvaOutlinerItemProxyFactory* GetItemProxyFactory() const
	{
		// First look for the Registry in Outliner
		if (IAvaOutlinerItemProxyFactory* Factory = ItemProxyRegistry.GetItemProxyFactory<InItemProxyType>())
		{
			return Factory;
		}
		// Fallback to finding the Factory in the Module if the Outliner did not find it
		return IAvaOutlinerModule::Get().GetItemProxyRegistry().GetItemProxyFactory<InItemProxyType>();
	}

	/**
	 * Tries to get the Item Proxy Factory for the given Item Proxy type, first trying the Outliner Registry then the Module's
	 * then returns an existing item proxy created via the factory, or creates one if there's no existing item proxy
	 * @returns the Item Proxy created by the Factory. Can be null if no factory was found or if the factory intentionally returns null
	 */
	template<typename InItemProxyType, typename = typename TEnableIf<TIsDerivedFrom<InItemProxyType, FAvaOutlinerItemProxy>::IsDerived>::Type>
	TSharedPtr<FAvaOutlinerItemProxy> GetOrCreateItemProxy(const FAvaOutlinerItemPtr& InParentItem)
	{
		if (!InParentItem.IsValid() || !InParentItem->IsAllowedInOutliner())
		{
			return nullptr;
		}
		
		IAvaOutlinerItemProxyFactory* const Factory = GetItemProxyFactory<InItemProxyType>();
		if (!Factory)
		{
			return nullptr;
		}
		
		TSharedPtr<FAvaOutlinerItemProxy> OutItemProxy;
		if (FAvaOutlinerItemPtr ExistingItemProxy = FindItem(FAvaOutlinerItemId(InParentItem, *Factory)))
		{
			check(ExistingItemProxy->IsA<FAvaOutlinerItemProxy>());
			ExistingItemProxy->SetParent(InParentItem);
			OutItemProxy = StaticCastSharedPtr<FAvaOutlinerItemProxy>(ExistingItemProxy);
		}
		else
		{
			OutItemProxy = Factory->CreateItemProxy(*this, InParentItem);
		}
		RegisterItem(OutItemProxy);
		return OutItemProxy;
	}
	
	const TSharedRef<FAvaOutlinerSaveState>& GetSaveState() const { return SaveState; }

	/** Returns whether the Outliner is in Read-only mode */
	bool IsOutlinerLocked() const;

	void HandleUndoRedoTransaction(const FTransaction* Transaction, bool bIsUndo);

	//~ Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	//~ End FEditorUndoClient

	//~ Begin FTickableObjectBase
	virtual TStatId GetStatId() const override;
	virtual void Tick(float InDeltaTime) override;
	//~ End FTickableObjectBase

	/**
	 * Creates a new set of items in the outliner based on the given template items
	 * @param InItems the template items to use for duplication
	 * @param InRelativeItem the existing outliner item that items should use as positional reference
	 * @param InRelativeDropZone where to put the duplicate items with relation to InRelativeItem (above, below, onto)
	 */
	void DuplicateItems(TArray<FAvaOutlinerItemPtr> InItems
		, FAvaOutlinerItemPtr InRelativeItem
		, TOptional<EItemDropZone> InRelativeDropZone);
	
	/** Register a new Outliner View to the Outliner to the given id, replacing the old view that was bound to the given id */
	TSharedPtr<FAvaOutlinerView> RegisterOutlinerView(int32 InOutlinerViewId);

	/** Unregisters the Outliner View bound to the given id */
	void UnregisterOutlinerView(int32 InOutlinerViewId);

	/** Sets the given Outliner View Id as the most recent Outliner View */
	void UpdateRecentOutlinerViews(int32 InOutlinerViewId);

	/** Gets the Outliner View bound to the given id */
	TSharedPtr<FAvaOutlinerView> GetOutlinerView(int32 InOutlinerViewId) const;

	/** Gets the outliner view that was most recently used (i.e. called FAvaOutliner::UpdateRecentOutlinerViews) */
	TSharedPtr<FAvaOutlinerView> GetMostRecentOutlinerView() const;

	/** Executes the given predicate for each Outliner View registered */
	void ForEachOutlinerView(const TFunction<void(const TSharedPtr<FAvaOutlinerView>& InOutlinerView)>& InPredicate) const;

	/** Registers the given Item, replacing the old one. */
	void RegisterItem(const FAvaOutlinerItemPtr& InItem);

	/** Unregisters the Item having the given ItemId */
	void UnregisterItem(const FAvaOutlinerItemId& InItemId);

	/**
	 * Instantiates a new item action without adding it to the Pending Actions Queue.
	 * This should only be used directly when planning to enqueue multiple actions.
	 * @see FAvaOutliner::EnqueueItemActions
	 */
	template<typename InItemActionType, typename = typename TEnableIf<TIsDerivedFrom<InItemActionType, IAvaOutlinerAction>::IsDerived>::Type, typename ...InArgTypes>
	TSharedRef<InItemActionType> NewItemAction(InArgTypes&&... InArgs)
	{
		return MakeShared<InItemActionType>(Forward<InArgTypes>(InArgs)...);
	}

	/**
	 * Instantiates a single new item action and immediately adds it to the Pending Actions Queue.
	 * Ideal for when dealing with a single action.
	 * For multiple actions use FAvaOutliner::EnqueueItemActions.
	 */
	template<typename InItemActionType, typename = typename TEnableIf<TIsDerivedFrom<InItemActionType, IAvaOutlinerAction>::IsDerived>::Type, typename ...InArgTypes>
	void EnqueueItemAction(InArgTypes&&... InArgs)
	{
		EnqueueItemActions({ NewItemAction<InItemActionType>(Forward<InArgTypes>(InArgs)...) });
	}

	/** Adds the given actions to the Pending Action Queue */
	void EnqueueItemActions(const TArray<TSharedPtr<IAvaOutlinerAction>>& InItemActions);

	/** Adds the given actions to the Pending Action Queue */
	void EnqueueItemActions(TArray<TSharedPtr<IAvaOutlinerAction>>&& InItemActions) noexcept;

	/** Returns the number of actions that been added to the queue so far before triggering a refresh */
	int32 GetPendingItemActionCount() const { return PendingActions.Num(); }

	/** Instantiates a new Item and automatically registers it to the Outliner */
	template<typename InItemType, typename = typename TEnableIf<TIsDerivedFrom<InItemType, IAvaOutlinerItem>::IsDerived>::Type, typename ...InArgTypes>
	TSharedRef<InItemType> FindOrAdd(InArgTypes&&... InArgs)
	{
		TSharedRef<InItemType> Item = MakeShared<InItemType>(*this, Forward<InArgTypes>(InArgs)...);

		// If an existing item already exists and has a valid state, use that and forget about the newly created
		FAvaOutlinerItemPtr ExistingItem = FindItem(Item->GetItemId());
		if (ExistingItem.IsValid() && ExistingItem->IsItemValid() && ExistingItem->IsA<InItemType>())
		{
			return StaticCastSharedPtr<InItemType>(ExistingItem).ToSharedRef();
		}
		
		if (Item->IsAllowedInOutliner())
		{
			RegisterItem(Item);
		}
		
		return Item;
	}

	/** Returns whether the Outliner is currently in need of a Refresh */
	bool NeedsRefresh() const;

	/** Ensures that the next time Refresh is called in tick, Refresh will be called */
	void RequestRefresh();

	/**
	 * Flushes the Pending Actions from the Queue while also updating the state of the Outliner.
	 * Calling it directly is forcing it to happen.
	 * If a refresh is needed it will be called on the next tick automatically.
	 */
	void Refresh();

	/**
	 * Finds the Registered Item that has the given Id
	 * @returns the Item with the given Id, or null if the item does not exist or was not registered to the Outliner
	 */
	FAvaOutlinerItemPtr FindItem(const FAvaOutlinerItemId& InItemId) const;

	/**
	 * Gets the color pair (color name, linear color) related to the Item
	 * @param InItem the item to query
	 * @param bRecurseParent whether to get the color of the parent (recursively) if the given item does not have a color by itself
	 * @returns the matching color pair or unset if item is invalid or no color could be found.
	 */
	TOptional<FAvaOutlinerColorPair> FindItemColor(const FAvaOutlinerItemPtr& InItem, bool bRecurseParent = true) const;

	/** Pairs the Item with the given color name, overriding the inherited color if different */
	void SetItemColor(const FAvaOutlinerItemPtr& InItem, const FName& InColorName);

	/** Removes the Color pairing of the given Item (can still have an inherited color though) */
	void RemoveItemColor(const FAvaOutlinerItemPtr& InItem);

	/** Returns the color map from the Outliner Settings */
	const TMap<FName, FLinearColor>& GetColorMap() const;

	/**
	 * Replaces the Item's Id in the Item Map. This can be due to an object item changing it's object
	 * (e.g. a bp component getting destroyed and recreated, the item should be the same but the underlying component will not be)
	 */
	void NotifyItemIdChanged(const FAvaOutlinerItemId& OldId, const FAvaOutlinerItemPtr& InItem);

	/** Returns the currently selected items in the most recent outliner view (since this list can vary between outliner views) */
	TArray<FAvaOutlinerItemPtr> GetSelectedItems() const;

	/** Returns the number of currently selected items in the most recent outliner view */
	int32 GetSelectedItemCount() const;

	/** Gets the Tree Root Item of the Outliner */
	TSharedRef<FAvaOutlinerTreeRoot> GetTreeRoot() const
	{
		return RootItem;
	}

	/**
	 * Selects the given Items on all Outliner Views
	 * @param InItems the items to select
	 * @param InFlags how the items should be selected (appended, notify of selections, etc)
	 */
	void SelectItems(const TArray<FAvaOutlinerItemPtr>& InItems, EAvaOutlinerItemSelectionFlags InFlags = EAvaOutlinerItemSelectionFlags::SignalSelectionChange) const;

	/**
	 * Clears the Item Selection from all Outliner Views
	 * @param bSignalSelectionChange whether to notify the change in selection
	 */
	void ClearItemSelection(bool bSignalSelectionChange) const;

	/**
	 * Adds or Removes the Ignore Notify Flags to prevent certain actions from automatically happening when they're triggered
	 * @param InFlag the ignore flag to add or remove
	 * @param bIgnore whether to add (true) or remove (false) the flag
	 */
	void SetIgnoreNotify(EAvaOutlinerIgnoreNotifyFlags InFlag, bool bIgnore)
	{
		if (bIgnore)
		{
			EnumAddFlags(IgnoreNotifyFlags, InFlag);
		}
		else
		{
			EnumRemoveFlags(IgnoreNotifyFlags, InFlag);
		}
	}

	/** Gets the closest item to all the given items while also being their common ancestor */
	static FAvaOutlinerItemPtr FindLowestCommonAncestor(const TArray<FAvaOutlinerItemPtr>& Items);

	/**
	 * Compares the absolute order of the items in the Outliner and returns true if A comes before B in the outliner.
	 * Useful to use when sorting Items.
	 */
	static bool CompareOutlinerItemOrder(const FAvaOutlinerItemPtr& A, const FAvaOutlinerItemPtr& B);

	/** Converts the given Outliner Item to a Scene Item that can be serialized in the Scene Tree */
	static FAvaSceneItem MakeSceneItemFromOutlinerItem(const FAvaOutlinerItemPtr& InItem);

	/**
	 * Helper function to sort the given array of items based on their ordering in the Outliner
	 * @see FAvaOutliner::CompareOutlinerItemOrder
	 */
	static void SortItems(TArray<FAvaOutlinerItemPtr>& OutOutlinerItems, bool bInReverseOrder = false);

	/** Normalizes the given Items by removing selected items that have their parent item also in the selection */
	static void NormalizeItems(TArray<FAvaOutlinerItemPtr>& InOutItems);

	/**
	 * Gets all the Selected Items and puts/attaches them under the given Grouping Actor.
	 * Requires that the Grouping Actor is valid and spawned in the World.
	 */
	void GroupSelection(AActor* InGroupingActor, const TOptional<FAttachmentTransformRules>& InTransformRules = TOptional<FAttachmentTransformRules>());

	/** Gets the Editor Mode Tools used to handle selections */
	FEditorModeTools* GetModeTools() const;

	/** Have the given Selected Items sync to the USelection Instances of Mode Tools */
	void SyncModeToolsSelection(const TArray<FAvaOutlinerItemPtr>& InSelectedItems) const;

	/** Called when the objects have been selected and notified through USelection Instances in Mode Tools */
	void OnObjectSelectionChanged(const FAvaEditorSelection& InEditorSelection);

	/** Gets the World the Outliner is working with */
	UWorld* GetWorld() const;

	/** Gets all the Actors that have as their AActor::GetSceneOutlinerParent the given InParentActor */
	TArray<TWeakObjectPtr<AActor>> GetActorSceneOutlinerChildren(AActor* InParentActor) const;

	/** Tries to add the new Actor to the Outliner if not added already and if the Outliner allows the given actor to be added */
	void OnActorSpawned(AActor* InActor);

	/**
	 * Should be called when Actors have been copied and give Ava Outliner opportunity to add to the Buffer to copy the Outliner data for those Actors
	 * @param InOutCopiedData the data to process / append to for copy
	 * @param InCopiedActors the actors to copy
	 */
	void OnActorsCopied(FString& InOutCopiedData, TConstArrayView<AActor*> InCopiedActors);

	/**
	 * Should be called when Actors have been pasted to parse the data that was filled in by FAvaOutliner::OnActorsCopied
	 * @param InPastedData pasted string data
	 * @param InPastedActors map of the original actor name to its created actor on paste
	 */
	void OnActorsPasted(FStringView InPastedData, const TMap<FName, AActor*>& InPastedActors);

	/**
	 * Handles when Actors have been Duplicated
	 * @param InDuplicateActorMap the map of the Duplicate Actors to their Templates
	 * @param InRelativeItem he item to use as positional reference to where to place the duplicate items in the outliner
	 * @param InRelativeDropZone where to put the duplicate items relative to the InRelativeItem (above, below, onto)
	 */
	void OnActorsDuplicated(const TMap<AActor*, AActor*>& InDuplicateActorMap
		, FAvaOutlinerItemPtr InRelativeItem = nullptr
		, TOptional<EItemDropZone> InRelativeDropZone = TOptional<EItemDropZone>());

	/** Called when an Actor has been destroyed. This enqueues the Removal the Actor Item to the Pending Action Queue */
	void OnActorDestroyed(AActor* InActor);

	/** Called when an Actor's attachment has changed. This triggers a refresh */
	void OnActorAttachmentChanged(AActor* InActor, const AActor* InParent, bool bAttach);

	/** Called the engine replaces an object. A common example is when a BP Component is destroyed, and replaced */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap);

	/** Gets the Default Transform to use when spawning actors via the Outliner (e.g. dragging a spawnable asset into an item in the outliner) */
	FTransform GetActorDefaultSpawnTransform() const;

	/** Marks the Outliner dirty. This triggers IAvaOutlinerProvider::OnOutlinerModified on next tick */
	void SetOutlinerModified();

private:
	void AddItem(const FAvaOutlinerItemPtr& InItem);
	void RemoveItem(const FAvaOutlinerItemId& InItemId);
	void ForEachItem(TFunctionRef<void(const FAvaOutlinerItemPtr&)> InFunc);

	/** Interface providing the Outliner with logic such as Actor Duplication, Mode Tools, and extensibility options */
	IAvaOutlinerProvider& OutlinerProvider;

	/** the base command list gotten from the outliner provider to append our command list to */
	TWeakPtr<FUICommandList> BaseCommandListWeak;

	/** the map of the registered items */
	TMap<FAvaOutlinerItemId, FAvaOutlinerItemPtr> ItemMap;

	TMap<FAvaOutlinerItemId, FAvaOutlinerItemPtr> ItemsPendingAdd;

	TSet<FAvaOutlinerItemId> ItemsPendingRemove;

	/** Map of an Actor to its Child Actors defined as such via AActor::GetSceneOutlinerParent */
	TMap<TWeakObjectPtr<AActor>, TArray<TWeakObjectPtr<AActor>>> SceneOutlinerParentMap;

	/** The root of all the items in the outliner */
	TSharedRef<FAvaOutlinerTreeRoot> RootItem;

	/** Outliner's Item Proxy Factory Registry Instance. This takes precedence over the Module's Factory Registry */
	FAvaOutlinerItemProxyRegistry ItemProxyRegistry;
	
	/** the current pending actions before refresh is called */
	TArray<TSharedPtr<IAvaOutlinerAction>> PendingActions;

	/** the list of objects pending selection processing. filled in when getting notifies from the USelection instances */
	TSharedPtr<TArray<TWeakObjectPtr<UObject>>> ObjectsLastSelected;

	/** the save state object to help serialize the outliner state */
	TSharedRef<FAvaOutlinerSaveState> SaveState;

	/** the map of registered outliner views */
	TMap<int32, TSharedPtr<FAvaOutlinerView>> OutlinerViews;

	/** List of Outliner View Ids in order from least recent to most recent (i.e. Index 0 is least recent) */
	TArray<int32> RecentOutlinerViews;

	/** the current events to ignore and not handle automatically */
	EAvaOutlinerIgnoreNotifyFlags IgnoreNotifyFlags = EAvaOutlinerIgnoreNotifyFlags::None;

	/** Flag indicating whether the Outliner has been changed this tick and should call IAvaOutlinerProvider::OnOutlinerModified next tick */
	bool bOutlinerDirty = false;
	
	/** Flag indicating Refreshing is taking place */
	bool bRefreshing = false;

	/** Flag indicating that a refresh must take place next tick */
	bool bRefreshRequested = false;

	/** Flag indicating that the Item Map is iterating */
	bool bIteratingItemMap = false;
};

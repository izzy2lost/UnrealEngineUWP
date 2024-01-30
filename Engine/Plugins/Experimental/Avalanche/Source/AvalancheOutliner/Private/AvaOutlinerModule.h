// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaOutlinerModule.h"
#include "ItemProxies/AvaOutlinerItemProxyRegistry.h"

class IAvaFilterExpressionFactory;

class FAvaOutlinerModule : public IAvaOutlinerModule
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	//~ Begin IAvaOutlinerModule
	virtual FOnInitOutlinerToolMenu& GetOnInitOutlinerToolBar() override { return OnInitOutlinerToolBar; }
	virtual FOnInitOutlinerToolMenu& GetOnInitOutlinerItemContextMenu() override { return OnInitOutlinerItemContextMenu; }
	virtual FOnExtendOutlinerToolMenu& GetOnExtendOutlinerToolBar() override  { return OnExtendOutlinerToolBar; }
	virtual FOnExtendOutlinerToolMenu& GetOnExtendOutlinerItemContextMenu() override   { return OnExtendOutlinerItemContextMenu; }
	virtual FOnExtendOutlinerColumns& GetOnExtendOutlinerColumns() override { return OnExtendOutlinerColumns; }
	virtual FAvaOutlinerItemProxyRegistry& GetItemProxyRegistry() override { return ItemProxyRegistry; }
	virtual FOnExtendItemProxiesForItem& GetOnExtendItemProxiesForItem() override { return OnGetItemProxiesForItem; }
	virtual void UnregisterFilterExpressionFactory(const FName& InFilterIdentifier) override;
	virtual void UnregisterFilterSuggestionFactory(const FName& InFilterIdentifier) override;
	virtual bool CanFilterSupportComparisonOperation(const FName& InFilterKey, const ETextFilterComparisonOperation InOperation) const override;
	virtual bool FilterExpression(const FName& InFilterKey, const IAvaOutlinerItem& InItem, const FAvaTextFilterArgs& InArgs) const override;
	virtual TArray<TSharedPtr<IAvaFilterSuggestionFactory>> GetSuggestions(const EAvaFilterSuggestionType& InSuggestionType) const override;
	virtual FSlateIcon FindOverrideIcon(TSharedPtr<const FAvaOutlinerItem> InItem) const override;
	//~ End IAvaOutlinerModule

protected:
	/** Delegate to extend the Tool Menu Context of the Outliner ToolBar */
	FOnInitOutlinerToolMenu OnInitOutlinerToolBar;

	/** Delegate to extend the Tool Menu Context of the Outliner Item Context Menu */
	FOnInitOutlinerToolMenu OnInitOutlinerItemContextMenu;

	/** Delegate to extend the Outliner ToolBar */
	FOnExtendOutlinerToolMenu OnExtendOutlinerToolBar;

	/** Delegate to extend the Item Context Menu */
	FOnExtendOutlinerToolMenu OnExtendOutlinerItemContextMenu;

	/** Delegates to add in additional Outliner Columns */
	FOnExtendOutlinerColumns OnExtendOutlinerColumns;

	/** Delegate to add in Item Proxies for a given Item / Item Type */
	FOnExtendItemProxiesForItem OnGetItemProxiesForItem;

	/** Module Instance of the Item Registry */
	FAvaOutlinerItemProxyRegistry ItemProxyRegistry;

	//~ Begin IAvaOutlinerModule
	virtual void UnregisterOverriddenIcon_Internal(const FAvaTypeId& InItemTypeId, const FName& InSpecializationIdentifier) override;
	virtual void RegisterOverridenIcon_Internal(const FAvaTypeId& InItemTypeId, const TSharedRef<IAvaOutlinerIconCustomization>& InAvaOutlinerIconCustomization) override;
	virtual void RegisterFilterExpressionFactory_Internal(const TSharedRef<IAvaFilterExpressionFactory>& InFilterExpressionFactory) override;
	virtual void RegisterFilterSuggestionFactory_Internal(const TSharedRef<IAvaFilterSuggestionFactory>& InFilterSuggestionFactory) override;
	//~ End IAvaOutlinerModule

private:
	/**
	 * Get the customization for the given item if any
	 * @param InItem Item to search the customization for
	 * @return The customization for the item if any, nullptr otherwise
	 */
	TSharedPtr<IAvaOutlinerIconCustomization> GetCustomizationForItem(const TSharedPtr<const FAvaOutlinerItem>& InItem) const;

	/**
	 * Registers default filter expression factories.
	 */
	void RegisterFilterExpressionFactories();

	/**
	  * Registers default filter suggestion factories.
	  */
	void RegisterFilterSuggestionFactories();

private:
	/** Holds all the FilterExpressionFactory */
	TMap<FName, TSharedPtr<IAvaFilterExpressionFactory>> FilterExpressionFactories;

	/** Holds all the FilterSuggestionFactory */
	TMap<FName, TSharedPtr<IAvaFilterSuggestionFactory>> FilterSuggestionsFactories;

	/**
	 * Hold the key of the map containing the IconCustomizations
	 */
	struct FIconCustomizationKey
	{
		/** OutlinerClass FName (ex. AvaOutlinerItem) */
		FAvaTypeId ItemTypeId = FAvaTypeId::Invalid();

		/** Specialization identifier (ex. AvaShapeActor class FName for ActorIcon customization) */
		FName CustomizationSpecializationIdentifier;

		bool operator==(const FIconCustomizationKey& InOther) const
		{
			return ItemTypeId == InOther.ItemTypeId
				&& CustomizationSpecializationIdentifier == InOther.CustomizationSpecializationIdentifier;
		}

		friend uint32 GetTypeHash(FIconCustomizationKey const& InCustomizationKey)
		{
			return HashCombine(GetTypeHash(InCustomizationKey.ItemTypeId)
				, GetTypeHash(InCustomizationKey.CustomizationSpecializationIdentifier));
		}
	};

	/** Holds all the IconCustomizations */
	TMap<FIconCustomizationKey, TSharedPtr<IAvaOutlinerIconCustomization>> IconRegistry;
};

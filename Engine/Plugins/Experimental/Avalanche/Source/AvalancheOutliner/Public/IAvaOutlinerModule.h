// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Filters/IAvaFilterExpressionFactory.h"
#include "Filters/IAvaFilterSuggestionFactory.h"
#include "Icon/AvaOutlinerIconCustomization.h"
#include "Item/AvaOutlinerItem.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FAvaOutliner;
class FAvaOutlinerItem;
class FAvaOutlinerItemProxy;
class FAvaOutlinerItemProxyRegistry;
class UToolMenu;
struct FAvaTextFilterArgs;
struct FSlateIcon;
struct FToolMenuContext;

class IAvaOutlinerModule : public IModuleInterface
{
public:
	/** Returns the instance of this module, assuming it's loaded */
	static IAvaOutlinerModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IAvaOutlinerModule>(TEXT("AvalancheOutliner"));
	};
	/** Returns whether this module has been loaded  */
	static bool IsLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("AvalancheOutliner"));
	};

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnInitOutlinerToolMenu, FToolMenuContext&);

	/** Called when Initializing Outliner ToolBar to extend its Tool Menu Context */
	virtual FOnInitOutlinerToolMenu& GetOnInitOutlinerToolBar() = 0;

	/** Called when Initializing Outliner Item Context Menu to extend its Tool Menu Context */
	virtual FOnInitOutlinerToolMenu& GetOnInitOutlinerItemContextMenu() = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnExtendOutlinerToolMenu, UToolMenu*);

	/** Option to extend the Outliner ToolBar */
	virtual FOnExtendOutlinerToolMenu& GetOnExtendOutlinerToolBar() = 0;

	/** Option to extend the Outliner Item Context Menu */
	virtual FOnExtendOutlinerToolMenu& GetOnExtendOutlinerItemContextMenu() = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnExtendOutlinerColumns, class FAvaOutlinerColumnExtender&);

	/** Extends Outliner Columns regardless of the Outliner Provider used */
	virtual FOnExtendOutlinerColumns& GetOnExtendOutlinerColumns() = 0;

	/** Get the Module's Item Proxy Registry. Used as back up in case the Outliner's Instance does not have the Item type registered */
	virtual FAvaOutlinerItemProxyRegistry& GetItemProxyRegistry() = 0;

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnExtendItemProxiesForItem, FAvaOutliner&, const FAvaOutlinerItemPtr&, TArray<TSharedPtr<FAvaOutlinerItemProxy>>&);

	/** Gets the Delegate to call for adding in the Item Proxies to put under the given Item */
	virtual FOnExtendItemProxiesForItem& GetOnExtendItemProxiesForItem() = 0;

	/**
 	 * Create and Register the given filter expression factory
 	 * @tparam InFilterExpressionFactoryType The type of filter expression factory to create and register
 	 * @tparam InArgsType Constructor args types if needed
 	 * @param InArgs Additional args passed to the constructor of the filter expression factory if needed
 	 */
	template<typename InFilterExpressionFactoryType, typename... InArgsType
			UE_REQUIRES(TIsDerivedFrom<InFilterExpressionFactoryType, IAvaFilterExpressionFactory>::Value)>
	void RegisterFilterExpressionFactory(InArgsType&&... InArgs)
	{
		RegisterFilterExpressionFactory_Internal(IAvaFilterExpressionFactory::MakeInstance<InFilterExpressionFactoryType>(Forward<InArgsType>(InArgs)...));
	}

	/**
  	 * Create and Register the given filter suggestion factory
  	 * @tparam InSuggestionFactoryType The type of filter suggestion factory to create and register
  	 * @tparam InArgsType Constructor args types if needed
  	 * @param InArgs Additional args passed to the constructor of the filter suggestion factory if needed
  	 */
	template<typename InSuggestionFactoryType, typename... InArgsType
			UE_REQUIRES(TIsDerivedFrom<InSuggestionFactoryType, IAvaFilterSuggestionFactory>::Value)>
	void RegisterFilterSuggestionFactory(InArgsType&&... InArgs)
	{
		RegisterFilterSuggestionFactory_Internal(IAvaFilterSuggestionFactory::MakeInstance<InSuggestionFactoryType>(Forward<InArgsType>(InArgs)...));
	}

	/**
	 * Register an icon customization for the given OutlinerItemClass
	 * @tparam InOutlinerItemClass Outliner class to add a customization for
	 * @tparam InIconCustomization Icon customization to add
	 * @tparam InArgsType Additional constructor parameter type for the customization class (ex. UClass* for Object/Actor customization)
	 * @param InArgs Additional constructor parameter for the customization class
	 * @return The customization created
	 */
	template<typename InOutlinerItemClass, typename InIconCustomization, typename... InArgsType
		UE_REQUIRES(TIsDerivedFrom<InOutlinerItemClass, FAvaOutlinerItem>::Value && TIsDerivedFrom<InIconCustomization, IAvaOutlinerIconCustomization>::Value)>
	InIconCustomization& RegisterOverriddenIcon(InArgsType&&... InArgs)
	{
		TSharedRef<InIconCustomization> OutlinerIconCustomization = MakeShared<InIconCustomization>(Forward<InArgsType>(InArgs)...);
		RegisterOverridenIcon_Internal(TAvaType<InOutlinerItemClass>::GetTypeId(), OutlinerIconCustomization);
		return OutlinerIconCustomization.Get();
	}

	/**
	 * Unregister an icon customization
	 * @tparam InOutlinerItemClass Outliner item class (ex. FAvaOutlinerItem)
	 * @param InAdditionalKeyParameter The additional key parameter to found the customization (ex. ObjectClass FName for ObjectIcon customization)
	 */
	template<typename InOutlinerItemClass
		UE_REQUIRES(TIsDerivedFrom<InOutlinerItemClass, FAvaOutlinerItem>::Value)>
	void UnregisterOverriddenIcon(FName InAdditionalKeyParameter)
	{
		UnregisterOverriddenIcon_Internal(TAvaType<InOutlinerItemClass>::GetTypeId(), InAdditionalKeyParameter);
	}

	/**
	 * Get the custom icon for the item if a customization for it exist
	 * @param InItem The item to search the icon for
	 * @return The custom icon for the item if a customization is found, FSlateIcon() otherwise
	 */
	virtual FSlateIcon FindOverrideIcon(TSharedPtr<const FAvaOutlinerItem> InItem) const = 0;

	/**
 	 * Unregister a previously registered filter expression factory. 
 	 * @param InFilterIdentifier 
 	 */
	virtual void UnregisterFilterSuggestionFactory(const FName& InFilterIdentifier) = 0;

	/**
	 * Unregister a previously registered filter expression factory. 
	 * @param InFilterIdentifier 
	 */
	virtual void UnregisterFilterExpressionFactory(const FName& InFilterIdentifier) = 0;

	/**
	 *	Check if current filter expression factory support the comparison operation
	 * @param InFilterKey Filter key to get the filter expression factory needed
	 * @param InOperation Operation to check if supported
	 * @return True if operation is supported, False otherwise
	 */
	virtual bool CanFilterSupportComparisonOperation(const FName& InFilterKey, const ETextFilterComparisonOperation InOperation) const = 0;

	/**
	 *	Evaluate the expression and return the result 
	 * @param InFilterKey Filter Key to get the Factory
	 * @param InItem Item that is currently checked
	 * @param InArgs Args to evaluate the expression see FAvaTextFilterArgs for more information
	 * @return True if the expression evaluated to True, False otherwise
	 */
	virtual bool FilterExpression(const FName& InFilterKey, const IAvaOutlinerItem& InItem, const FAvaTextFilterArgs& InArgs) const = 0;

	/**
	 * Get all suggestions with the given type (Generic/ItemBased/All)
	 * @param InSuggestionType Type of suggestion to get, see EFilterSuggestionType for more information
	 * @return An Array containing all suggestions of the given type
	 */
	virtual TArray<TSharedPtr<IAvaFilterSuggestionFactory>> GetSuggestions(const EAvaFilterSuggestionType& InSuggestionType) const = 0;

protected:

	/**
	 * Adds the given Customization to the Map containing all of them if not already added.
	 * @param InItemTypeId Outliner Item Type Id
	 * @param InAvaOutlinerIconCustomization Customization to add
	 */
	virtual void RegisterOverridenIcon_Internal(const FAvaTypeId& InItemTypeId, const TSharedRef<IAvaOutlinerIconCustomization>& InAvaOutlinerIconCustomization) = 0;

	/**
	 * Unregister the customization with the given key
	 * @param InItemTypeId Outliner Item Type Id
	 * @param InSpecializationIdentifier Specialization identifier (ex. AActor class FName for ActorIconCustomization)
	 */
	virtual void UnregisterOverriddenIcon_Internal(const FAvaTypeId& InItemTypeId, const FName& InSpecializationIdentifier) = 0;

	/**
	 * Adds the given factory to the Map containing all of them if not already added.
	 * @param InFilterExpressionFactory Filter expression factory to register.
	 */
	virtual void RegisterFilterExpressionFactory_Internal(const TSharedRef<IAvaFilterExpressionFactory>& InFilterExpressionFactory) = 0;

	/**
	 * Adds the given factory to the Map containing all of them if not already added.
	 * @param InFilterSuggestionFactory Filter suggestion factory to register.
	 */
	virtual void RegisterFilterSuggestionFactory_Internal(const TSharedRef<IAvaFilterSuggestionFactory>& InFilterSuggestionFactory) = 0;
};

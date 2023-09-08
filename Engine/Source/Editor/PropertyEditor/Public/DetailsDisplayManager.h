//  Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DetailsViewStyleKey.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

DECLARE_DELEGATE(FOnDetailsNeedsUpdate)

/** An @code FDetailsDisplayManager @endcode provides an API to tweak various settings of your details view, and
 * provides some utility methods to work with Details.  */
class FDetailsDisplayManager : public TSharedFromThis<FDetailsDisplayManager>
{
public:
	FOnDetailsNeedsUpdate OnDetailsNeedsUpdate;

	PROPERTYEDITOR_API FDetailsDisplayManager() : bIsOuterCategory(false)
	{
	}

	PROPERTYEDITOR_API virtual ~FDetailsDisplayManager();

	/**
	 * Returns a boolean indicating if the Component Editor should be hidden 
	 */
	virtual bool ShouldHideComponentEditor();

	/**
	 * Returns a @code bool @endcode indicating whether this @code DetailsViewObjectFilter @endcode instance
	 * should show a category menu
	 */
	virtual bool ShouldShowCategoryMenu();

	/**
	 * Sets the name of the object defined by the currently active category
	 *
	 * @param InCategoryObjectName the name of the category object
	 */
	void SetCategoryObjectName(FName InCategoryObjectName);

	/**
	 * Gets the category menu SWidget and returns a shared pointer to it
	 *
	 *  @param InCategoryObjectName the name of the category
	 */
	virtual TSharedPtr<SWidget> GetCategoryMenu(FName InCategoryObjectName);

	/**
	 * Updates the current details view
	 */
	PROPERTYEDITOR_API void UpdateView() const;

	/**
	 * Returns the @code FDetailsViewStyleKey @endcode that is the Key to the current FDetailsViewStyle style
	 */
	virtual const FDetailsViewStyleKey& GetDetailsViewStyleKey() const;

	/** sets whether the currently active category is an Outer category*/
	void SetIsOuterCategory(bool bInIsOuterCategory);

	/**
	 * Returns the padding for details panel rows which are not outer Category rows
	 */
	FMargin GetRowPadding() const;

	/**
	 * Returns the padding for any buttons for Category rows
	 */
	FMargin GetCategoryButtonsPadding() const;

	/**
	 * returns a boolean indicating whether or not the currently active categpory is an inner category
	 */	
	bool GetIsInnerCategory() const;

	/**
   	 * Returns a bool indicating whether or not the scrollbar is showing on the details view
   	 */
	PROPERTYEDITOR_API virtual bool GetIsScrollbarShowing() const;
	
	/**
	* Set a bool indicating whether or not the scrollbar is showing on the details view
	*
	* @param bInIsScrollbarShowing a bool indicating whether or not the scrollbar is showing on the details view
	*/
	PROPERTYEDITOR_API virtual void SetIsScrollbarShowing(bool bInIsScrollbarShowing);
	
	/**
	* Returns an FMargin containing the padding for the entire details view. Note that if there is a scrollbar present,
	* this padding takes that into account
	*/
	PROPERTYEDITOR_API virtual FMargin GetTablePadding() const;

protected:

	/**
	 * The name of the object defined by the currently active category
	 */
	FName CategoryObjectName;

	/**
	 * A bool indicating whether or not the currently active category is an outer category
	 */
	bool bIsOuterCategory;

	/**
	 * A bool indicating whether or not the scrollbar is showing on the details view
	 */
	bool bIsScrollbarShowing;

};


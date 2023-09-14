//  Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DetailsViewStyleKey.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

DECLARE_DELEGATE(FOnDetailsNeedsUpdate)

class FDetailsViewStyle;

/** An @code FDetailsDisplayManager @endcode provides an API to tweak various settings of your details view, and
 * provides some utility methods to work with Details.  */
class FDetailsDisplayManager : public TSharedFromThis<FDetailsDisplayManager>
{
public:
	FOnDetailsNeedsUpdate OnDetailsNeedsUpdate;

	PROPERTYEDITOR_API FDetailsDisplayManager();

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
	 * Returns the @code FDetailsViewStyle @endcode that is the current FDetailsViewStyle style
	 */
	PROPERTYEDITOR_API const FDetailsViewStyle* GetDetailsViewStyle() const;
	
	    /**
		* Returns a bool indicating whether or not the scrollbar is needed on the details view. Note that the "needed"
		* here means that in this value the work has been done to figure out if the scrollbar should show, and
		* anything can query this to see if it needs to alter the display accordingly
		*/
	PROPERTYEDITOR_API virtual bool GetIsScrollBarNeeded() const;
	
	/**
	* Set a bool indicating whether or not the scrollbar is needed on the details view. Note that the "needed"
	* here means that in this value the work has been done to figure out if the scrollbar should show, and
	* anything can query this to see if it needs to alter the display accordingly
	*
	* @param bInIsScrollBarNeeded a bool indicating whether or not the scrollbar is Needed on the details view
	*/
	PROPERTYEDITOR_API virtual void SetIsScrollBarNeeded(bool bInIsScrollBarNeeded);
	
	/**
	 * Returns the FMargin which provides the padding around the whole details view table
	 */
	FMargin GetTablePadding() const;

protected:

	
	/**
	 * The primary style key for the details view. 
	 */
	FDetailsViewStyleKey PrimaryStyleKey;
	
	/**
	 * The name of the object defined by the currently active category
	 */
	FName CategoryObjectName;

	/**
	 * A bool indicating whether or not the currently active category is an outer category
	 */
	bool bIsOuterCategory;

	/**
	* a bool indicating whether or not the scrollbar is needed on the details view. Note that the "needed"
	* here means that in this value the work has been done to figure out if the scrollbar should show 
	 */
	bool bIsScrollBarNeeded = false;

};


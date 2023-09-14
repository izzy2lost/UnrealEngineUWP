//  Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "DetailsViewStyleKey.h"
#include "Layout/Margin.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateWidgetStyle.h"
#include "UObject/NameTypes.h"

/**
 * A Class which holds information regarding the style of a Details View
 */
class FDetailsViewStyle : public FSlateWidgetStyle
{
public:

	/**
	 * The default constructor of this @code FDetailsViewStyle @endcode.
	 */
	FDetailsViewStyle();
	
	/**
	 * Constructs this @code FDetailsViewStyle @endcode with the specified parameters
	 *
	 * @param InKey the Key for  the @code FDetailsViewStyle @endcode
	 */
	FDetailsViewStyle(FDetailsViewStyleKey& InKey);

	/**
	 * Copy constructor for @code FDetailsViewStyle @endcode.
	 *
	 * @param InStyle the @code FDetailsViewStyle @endcode instance from which to initialize
	 * this @code FDetailsViewStyle @endcode.
	 */
	FDetailsViewStyle(FDetailsViewStyle& InStyle);

	/**
	 * Const copy constructor for @code FDetailsViewStyle @endcode.
	 *
	 * @param InStyle the @code FDetailsViewStyle @endcode instance from which to initialize
	 * this @code FDetailsViewStyle @endcode.
	 */
	FDetailsViewStyle(const FDetailsViewStyle& InStyle);

	/**
	 * Returns the padding for the outer Category row
	 */
	 FMargin GetOuterCategoryRowPadding() const;
	
	/**
	 * Returns the padding for details panel rows which are not outer Category rows
	 */
	FMargin GetRowPadding(bool bIsOuterCategory) const;

	/**
	 * The equality operator for @code FDetailsViewStyle @endcode
	 */
	bool operator==(FDetailsViewStyle& OtherLayoutType) const;

	/**
	 * The assignment operator for @code FDetailsViewStyle @endcode
	 */
	FDetailsViewStyle& operator=(FDetailsViewStyleKey& OtherLayoutTypeKey);

	/**
   	 * Gets the @code FName @endcode Name of this style.
   	 */
	virtual const FName GetTypeName() const override;

	/**
	 * Returns the background image for the Category row, minus the scrollbar well
	 *
 	 * @param bShowBorder a bool to indicate whether the border should be shown, at all
	 * @param bIsCategoryExpanded a bool that indicates whether this Category is expanded 
	 * @param bIsScrollBarVisible a bool that indicates whether the scrollbar for the details view is visible
	 */	
	const FSlateBrush* GetBackgroundImageForCategoryRow(
		const bool bShowBorder,
		const bool bIsInnerCategory,
		const bool bIsCategoryExpanded,
		const bool bIsScrollBarVisible) const;

	/**
	 * Returns the background image for the scroll bar well
	 *
	 * @param bShowBorder a bool to indicate whether the border should be shown, at all
	 * @param bIsInnerCategory a bool that indicates whether this background is for an inner Category
	 * @param bIsCategoryExpanded a bool that indicates whether this Category is expanded 
	 * @param bIsScrollBarVisible a bool that indicates whether the scrollbar for the details view is visible
	 */	
	const FSlateBrush* GetBackgroundImageForScrollBarWell(
			const bool bShowBorder,
			const bool bIsInnerCategory,
			const bool bIsCategoryExpanded, 
			const bool bIsScrollBarVisible) const;

	/**
	 * Initializes all Details View Styles
	 */
	static void InitializeDetailsViewStyles();


	/**
	 * Constructs this FDetailsViewStyle with the specified parameters
	 *
	 * @param InKey the name of the FDetailsViewStyle
	 * @param InTopCategoryPadding the top padding for the Category row
	 */
	FDetailsViewStyle(
		const FDetailsViewStyleKey& InKey,
		float InTopCategoryPadding = 0.f);

	/**
	 * Returns the FMargin which provides the padding around the whole details view table
	 *
	 * @param bIsScrollBarVisible whether the scrollbar is visible
	 */
	FMargin GetTablePadding(bool bIsScrollBarVisible) const;

	/**
	 * Returns the FMargin which provides the padding around the Category buttons
	 */
	FMargin GetCategoryButtonsMargin() const
	{
		return CategoryButtonsMargin;
	}
	
	/**
	* Returns the @code FDetailsViewStyle @endcode for which this is the @code FDetailsViewStyleKey @endcode .
	*
	* @param InKey the name of the FDetailsViewStyle
	*/
	static const FDetailsViewStyle* GetStyle(const FDetailsViewStyleKey& InKey);
	
private:

	void Initialize( const FDetailsViewStyle* Style );

	/**
   	 * Initializes this FDetailsViewStyle with the specified parameters
   	 *
   	 * @param InKey the name of the FDetailsViewStyle
   	 * @param InTopCategoryPadding the top padding for the Category row
   	 */
	void Initialize( FDetailsViewStyleKey& InKey, 
		const float InTopCategoryPadding);

	/**
	* Initializes this FDetailsViewStyle with the style specified by @code FDetailsViewStyleKey @endcode InKey
	*
	* @param InKey the name of the FDetailsViewStyle
	*/
	void Initialize(FDetailsViewStyleKey& InKey);

	/** the Name of the Style */
	FDetailsViewStyleKey Key;

	/**
	 * A map of @code FDetailsViewStyleKey @endcode instances to the
	 * @code const FDetailsViewStyle* @endcode instances for which they are the keys. 
	 */
	static inline TMap<FName, const FDetailsViewStyle*> StyleKeyToStyleTemplateMap;
	
	/** the Slate Units of the top padding for an outer Category row */
	float TopCategoryPadding = 0.f;
	
 	/**
	* The FMargin which provides the padding around the whole details view table with scrollbar
	*/
	FMargin TablePaddingWithScrollbar = FMargin(0, 0, 16, 1);

	/**
	* The FMargin which provides the padding around the whole details view table with no scrollbar
	*/
	FMargin TablePaddingWithNoScrollbar = FMargin(0, 0, 0, 1);
	
	/**
	* The FMargin which provides the padding around the Category buttons
	*/
	FMargin CategoryButtonsMargin = FMargin(0, 0,  10, 0);
	
	
};


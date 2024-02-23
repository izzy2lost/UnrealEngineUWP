// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ToolkitBuilder.h"
#include "ToolkitStyle.h"
#include "Layout\CategoryDrivenContentBuilderBase.h"

/**
 * A builder which creates a widget that has a vertical toolbar category picker on the left
 * hand side which populates the content on the right side.
 */
class  FCategoryDrivenContentBuilder : public FCategoryDrivenContentBuilderBase
{
public:
	DECLARE_DELEGATE_RetVal_OneParam( TSharedRef<SWidget>, FProvideSelectedCategoryContent, const FName&)

	/**
	 * A delegate which provides the const FName& Command name as a parameter to indicates which category was clicked, and returns
	 * a TSharedRef<SWidget> meant to populate the full content for that category
	 */ 
	FProvideSelectedCategoryContent  ProvideSelectedCategoryContentDelegate;
	
public:
	/**
	 * initializes this FCategoryDrivenContentBuilder with the given FCategoryDrivenContentBuilderArgs
	 * @param Args the parameter object which provides the information to initialize this FCategoryDrivenContentBuilder 
	 */
	WIDGETREGISTRATION_API explicit FCategoryDrivenContentBuilder(FCategoryDrivenContentBuilderArgs& Args);
	
	/**
	 * destroys the FCategoryDrivenContentBuilder and frees any resources
	 */
	WIDGETREGISTRATION_API virtual ~FCategoryDrivenContentBuilder() override;

private:
	/**
	 * Given the active category name, update the  content
	 * 
	 * @param ActiveCategoryName 
	 */
	WIDGETREGISTRATION_API virtual void ProvideSelectedCategoryContent( FName ActiveCategoryName = NAME_None ) override;

	FName ActiveCommandName;

public:
	/*
	 * Initializes the data necessary to build the category toolbar
	 */
	WIDGETREGISTRATION_API virtual void InitializeCategoryToolbar() override;

	/*
	 * Refreshes the existing widget.
	 */
	WIDGETREGISTRATION_API virtual void UpdateWidget() override;

	/**
	 * Returns true if the FUICommandInfo with the name CommandName is the active tool palette,
	 * else it returns false
	 *
	 * @param CommandName the name of the FUICommandInfo we are checking to see if it is the active tool palette
	 */
	WIDGETREGISTRATION_API virtual ECheckBoxState IsActiveToolPalette(FName CommandName) const override
	{
		return ActiveCommandName == CommandName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/*
	 * Sets the commands that will load the categories
	 */
	WIDGETREGISTRATION_API void SetCommands(TArray<TSharedPtr<FUICommandInfo>> InContentLoaderCommands);
	
private:
	TArray<TSharedPtr<FUICommandInfo>> ContentLoaderCommands;
};


 
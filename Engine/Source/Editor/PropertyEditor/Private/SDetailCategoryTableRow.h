// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DetailTreeNode.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "PropertyEditorClipboardPrivate.h"
#include "SDetailTableRowBase.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

class SDetailCategoryTableRow : public SDetailTableRowBase
{
public:
	SLATE_BEGIN_ARGS(SDetailCategoryTableRow)
		: _InnerCategory(false)
		, _ShowBorder(true)
		, _ObjectName(NAME_Name)
	{}
		SLATE_ARGUMENT(FText, DisplayName)
		SLATE_ARGUMENT(bool, InnerCategory)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, HeaderContent)
		SLATE_ARGUMENT(bool, ShowBorder)
		SLATE_ARGUMENT(TSharedPtr<FOnPasteFromText>, PasteFromText)
		SLATE_ARGUMENT(FName, ObjectName)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FDetailTreeNode> InOwnerTreeNode, const TSharedRef<STableViewBase>& InOwnerTableView);

	// ~Begin SWidget Interface
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// ~Begin End Interface

protected:
	virtual bool OnContextMenuOpening( FMenuBuilder& MenuBuilder ) override;
	
private:
	EVisibility IsSeparatorVisible() const;

	/**
	* Gets the @code const FSlateBrush* @endcode which holds the background image for the Category Row, minus the ScrollBar Well
	*/
	const FSlateBrush* GetBackgroundImage() const;

	/**
	* Gets the @code const FSlateBrush* @endcode which holds the background image for the ScrollBar Well
	*/
	const FSlateBrush* GetBackgroundImageForScrollBarWell() const;
	FSlateColor GetInnerBackgroundColor() const;
	FSlateColor GetOuterBackgroundColor() const;

	void OnCopyCategory();
	bool CanCopyCategory() const;
	void OnPasteCategory();
	bool CanPasteCategory();

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	/** Initializes the @code TSharedPtr<FDetailsDisplayManager> DisplayManager @endcode for this category */
	void InitializeDisplayManager();

private:
	/** Cached category name. */
	FText DisplayName;

	/** The name of the object that is specified by this category */
	FName ObjectName;
	
	/** Previously parsed clipboard data. */
	UE::PropertyEditor::Internal::FClipboardData PreviousClipboardData;

	bool bIsInnerCategory = false;
	bool bShowBorder = false;
	FUIAction CopyAction;
	FUIAction PasteAction;

	/** Delegate handling pasting an optionally tagged text snippet */
	TSharedPtr<FOnPasteFromText> OnPasteFromTextDelegate;

	/** Animation curve for displaying pulse */
	FCurveSequence PulseAnimation;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Layout/WidgetPath.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FText;
class ITableRow;
class STableViewBase;
class SToolTip;
class SWidget;
class SWindow;
struct FGeometry;
struct FTranslationPickerTextItem;

#define LOCTEXT_NAMESPACE "TranslationPicker"

class FTranslationPickerInputProcessor;

/** Translation picker floating window to show details of FText(s) under cursor, and allow in-place translation via TranslationPickerEditWindow */
class STranslationPickerFloatingWindow : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(STranslationPickerFloatingWindow) {}

	SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)

	SLATE_END_ARGS()

	virtual ~STranslationPickerFloatingWindow();

	void Construct(const FArguments& InArgs);

private:
	friend class FTranslationPickerInputProcessor;

	FReply Close();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	/** Pull the FText reference out of an SWidget */
	void PickTextFromWidget(TSharedRef<SWidget> Widget);

	/** Pull the FText reference out of the child widgets of an SWidget */
	void PickTextFromChildWidgets(TSharedRef<SWidget> Widget);

	/** Switch from floating window to edit window */
	bool SwitchToEditWindow();

	/** Update text list items */
	void UpdateListItems();

	/** Toggle 3D viewport mouse turning */
	void SetViewportMouseIgnoreLook(bool bLookIgnore);

	/** Get world from editor or engine */
	UWorld* GetWorld() const;

	TSharedRef<ITableRow> TextListView_OnGenerateWidget(TSharedPtr<FTranslationPickerTextItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Input processor used to capture key and mouse events */
	TSharedPtr<FTranslationPickerInputProcessor> InputProcessor;

	/** Handle to the window that contains this widget */
	TWeakPtr<SWindow> ParentWindow;

	/** Contents of the window */
	TSharedPtr<SToolTip> WindowContents;

	/** The FTexts that we have found under the cursor */
	TArray<FText> PickedTexts;

	/** List items for the text list */
	TArray<TSharedPtr<FTranslationPickerTextItem>> TextListItems;

	/** List of all texts */
	typedef SListView<TSharedPtr<FTranslationPickerTextItem>> STextListView;
	TSharedPtr<STextListView> TextListView;

	/** The path widgets we were hovering over last tick */
	FWeakWidgetPath LastTickHoveringWidgetPath;

	bool bMouseLookInputIgnored = false;
};

#undef LOCTEXT_NAMESPACE

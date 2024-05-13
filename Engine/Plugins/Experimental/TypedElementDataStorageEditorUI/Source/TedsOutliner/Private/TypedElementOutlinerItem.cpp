// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerItem.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "TypedElementOutlinerTreeItem"

const FSceneOutlinerTreeItemType FTypedElementOutlinerTreeItem::Type(&ISceneOutlinerTreeItem::Type);

FTypedElementOutlinerTreeItem::FTypedElementOutlinerTreeItem(const TypedElementRowHandle& InRowHandle,
	FBaseTEDSOutlinerMode& InMode)
	: ISceneOutlinerTreeItem(Type)
	, RowHandle(InRowHandle)
	, Mode(InMode)
{
	
}

bool FTypedElementOutlinerTreeItem::IsValid() const
{
	return true; // TEDS-Outliner TODO: check with TEDS if the item is valid?
}

FSceneOutlinerTreeItemID FTypedElementOutlinerTreeItem::GetID() const
{
	return FSceneOutlinerTreeItemID(RowHandle);
}

FString FTypedElementOutlinerTreeItem::GetDisplayString() const
{
	return TEXT("TEDS Item"); // TEDS-Outliner TODO: Used for searching by name, how to get this from TEDS
}

bool FTypedElementOutlinerTreeItem::CanInteract() const
{
	return true; // TEDS-Outliner TODO: check item constness from TEDS maybe?
}

TSharedRef<SWidget> FTypedElementOutlinerTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner,
	const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return Mode.CreateLabelWidgetForItem(RowHandle);
}

void FTypedElementOutlinerTreeItem::GenerateContextMenu(UToolMenu* Menu, SSceneOutliner& Outliner)
{
	FToolMenuSection& Section = Menu->AddSection("Copy", LOCTEXT("CopySection", "Copy"));

	Section.AddMenuEntry(
		"CopyRowHandle",
		LOCTEXT("CopyRowHandle_Title", "Copy row handle"),
		LOCTEXT("CopyRowHandle_Tooltip", "Copy the row handle of this row to the clipboard."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				const FString ClipboardString(LexToString<FString>(RowHandle));
				FPlatformApplicationMisc::ClipboardCopy(*ClipboardString);
			}),
			FCanExecuteAction()
		)
	);
}

TypedElementRowHandle FTypedElementOutlinerTreeItem::GetRowHandle() const
{
	return RowHandle;
}

#undef LOCTEXT_NAMESPACE
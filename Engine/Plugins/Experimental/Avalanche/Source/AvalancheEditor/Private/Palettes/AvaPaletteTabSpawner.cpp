// Copyright Epic Games, Inc. All Rights Reserved.

#include "Palettes/AvaPaletteTabSpawner.h"
#include "AvaPaletteExtension.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Styling/AppStyle.h"
#include "Widgets/SAvaPaletteBase.h"

#define LOCTEXT_NAMESPACE "AvaPaletteTabSpawner"

FAvaPaletteTabSpawner::FAvaPaletteTabSpawner(const TSharedRef<IAvaEditor>& InEditor, const FAvaPaletteTabInfo& InPaletteTabInfo)
	: FAvaTabSpawner(InEditor, InPaletteTabInfo.Id)
	, PaletteTabInfo(InPaletteTabInfo)
{
	TabLabel = InPaletteTabInfo.Name;
	TabTooltipText = InPaletteTabInfo.ToolTip.IsEmpty() ? InPaletteTabInfo.Name : InPaletteTabInfo.ToolTip;
	TabIcon = InPaletteTabInfo.Icon;
}

FName FAvaPaletteTabSpawner::GetTabID()
{
	return PaletteTabInfo.Id;
}

TSharedRef<SWidget> FAvaPaletteTabSpawner::CreateTabBody()
{
	if (!PaletteTabInfo.ConstructionCallback.IsBound())
	{
		return GetNullWidget();
	}

	TSharedPtr<IAvaEditor> Editor = EditorWeak.Pin();

	if (!Editor.IsValid())
	{
		return GetNullWidget();
	}

	TSharedPtr<IToolkitHost> ToolkitHost = Editor->GetToolkitHost();

	if (!ToolkitHost.IsValid())
	{
		return GetNullWidget();
	}

	TSharedRef<SAvaPaletteBase> PaletteWidget = PaletteTabInfo.ConstructionCallback.Execute(ToolkitHost.ToSharedRef());
	PaletteWidget->OnCreatedInEditor(EditorWeak.Pin());

	return PaletteWidget;
}

FTabSpawnerEntry& FAvaPaletteTabSpawner::RegisterTabSpawner(const TSharedRef<FTabManager>& InTabManager,
	const TSharedPtr<FWorkspaceItem>& InWorkspaceMenu)
{
	TSharedPtr<FWorkspaceItem> GroupItem;

	const FText PalettesGroupName = LOCTEXT("SubMenuLabel", "Palettes");

	for (const TSharedRef<FWorkspaceItem>& Item : InWorkspaceMenu->GetChildItems())
	{
		if (Item->GetDisplayName().ToString() == PalettesGroupName.ToString())
		{
			GroupItem = Item;
			break;
		}
	}

	if (!GroupItem.IsValid())
	{
		GroupItem = InWorkspaceMenu->AddGroup(
			PalettesGroupName
			, LOCTEXT("SubMenuTooltip", "Open an Avalanche palette")
			, FSlateIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed"))
		);
	}

	return FAvaTabSpawner::RegisterTabSpawner(InTabManager, nullptr)
		.SetGroup(GroupItem.ToSharedRef());
}

#undef LOCTEXT_NAMESPACE

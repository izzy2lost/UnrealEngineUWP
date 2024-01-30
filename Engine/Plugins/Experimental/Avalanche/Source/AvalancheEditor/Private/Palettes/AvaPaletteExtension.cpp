// Copyright Epic Games, Inc. All Rights Reserved.

#include "Palettes/AvaPaletteExtension.h"
#include "AvaPaletteTabSpawner.h"
#include "Editor.h"
#include "Framework/Docking/LayoutExtender.h"
#include "LevelEditor.h"
#include "Selection/AvaEditorSelection.h"
#include "Widgets/AvaViewportColorPickerActorClassRegistry.h"
#include "Widgets/AvaViewportColorPickerDelegates.h"

FAvaPaletteExtension::~FAvaPaletteExtension()
{
	FAvaViewportColorPickerDelegates::GetOnColorPicked().RemoveAll(this);
}

void FAvaPaletteExtension::Construct(const TSharedRef<IAvaEditor>& InEditor)
{
	Super::Construct(InEditor);

	RegisterPalettes();
}

void FAvaPaletteExtension::Activate()
{
	Super::Activate();

	FAvaViewportColorPickerDelegates::GetOnColorPicked().AddSP(this, &FAvaPaletteExtension::OnColorPicked);
}

void FAvaPaletteExtension::Deactivate()
{
	Super::Deactivate();

	FAvaViewportColorPickerDelegates::GetOnColorPicked().RemoveAll(this);
}

void FAvaPaletteExtension::RegisterTabSpawners(const TSharedRef<IAvaEditor>& InEditor) const
{
	Super::RegisterTabSpawners(InEditor);

	for (const FAvaPaletteTabInfo& PaletteTabInfo : RegisteredPalettes)
	{
		InEditor->AddTabSpawner<FAvaPaletteTabSpawner>(InEditor, PaletteTabInfo);
	}
}

void FAvaPaletteExtension::ExtendLevelEditorLayout(FLayoutExtender& InExtender) const
{
	Super::ExtendLevelEditorLayout(InExtender);

	FName PreviousTab = LevelEditorTabIds::PlacementBrowser;
	ELayoutExtensionPosition Placement = ELayoutExtensionPosition::Below;

	for (const FAvaPaletteTabInfo& PaletteTabInfo : RegisteredPalettes)
	{
		InExtender.ExtendLayout(
			PreviousTab
			, Placement
			, FTabManager::FTab(PaletteTabInfo.Id, ETabState::OpenedTab)
		);

		PreviousTab = PaletteTabInfo.Id;
		Placement = ELayoutExtensionPosition::After;
	}
}

void FAvaPaletteExtension::NotifyOnSelectionChanged(const FAvaEditorSelection& InSelection)
{
	Super::NotifyOnSelectionChanged(InSelection);

	TSharedPtr<IToolkitHost> ToolkitHost = GetToolkitHost();

	if (!ToolkitHost.IsValid())
	{
		return;
	}

	TArray<AActor*> SelectedActors = InSelection.GetSelectedObjects<AActor>();

	if (SelectedActors.Num() == 1)
	{
		const AActor* const ColorSource = SelectedActors[0];
		FAvaColorChangeData NewColorData;

		if (FAvaViewportColorPickerActorClassRegistry::GetColorDataFromActor(ColorSource, NewColorData))
		{
			FAvaViewportColorPickerDelegates::BroadcastColorSourceSelected(ToolkitHost.ToSharedRef(), NewColorData);
		}
	}
}

bool FAvaPaletteExtension::RegisterPalette(const FAvaPaletteTabInfo& InPaletteTabInfo)
{
	const bool bAlreadyRegistered = RegisteredPalettes.ContainsByPredicate(
		[&InPaletteTabInfo](const FAvaPaletteTabInfo& InRegisteredPalette)
		{
			return InPaletteTabInfo.Id == InRegisteredPalette.Id;
		}
	);

	if (!ensureMsgf(!bAlreadyRegistered, TEXT("Motion Design palette already registered: %s"), *InPaletteTabInfo.Id.ToString()))
	{
		return false;
	}

	RegisteredPalettes.Add(InPaletteTabInfo);

	return true;
}

void FAvaPaletteExtension::RegisterPalettes()
{
}

void FAvaPaletteExtension::OnColorPicked(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData)
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();

	if (!Editor.IsValid())
	{
		return;
	}

	if (Editor->GetToolkitHost() != InEditor)
	{
		return;
	}

	FEditorModeTools* ModeTools = Editor->GetEditorModeTools();

	if (!ModeTools)
	{
		return;
	}

	USelection* ActorSelection = ModeTools->GetSelectedActors();

	if (!ActorSelection)
	{
		return;
	}

	TArray<AActor*> SelectedActors;
	ActorSelection->GetSelectedObjects(SelectedActors);
	bool bHasColourReceiver = false;

	for (AActor* Actor : SelectedActors)
	{
		if (FAvaViewportColorPickerActorClassRegistry::ApplyColorDataToActor(Actor, InNewColorData))
		{
			bHasColourReceiver = true;
		}
	}

	if (!bHasColourReceiver)
	{
		return;
	}

	GEditor->RedrawLevelEditingViewports();
}

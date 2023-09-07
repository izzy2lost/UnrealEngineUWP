// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSolversTracksTab.h"

#include "ChaosVDEngine.h"
#include "ChaosVDStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/SChaosVDSolverTracks.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<SDockTab> FChaosVDSolversTracksTab::HandleTabSpawned(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> ViewportTab =
	SNew(SDockTab)
	.TabRole(ETabRole::PanelTab)
	.Label(LOCTEXT("SolverTracksTabLabel", "Available Solvers"))
	.ToolTipText(LOCTEXT("SolverTracksTabToolTip", "Playback controls for the available solvers on the current Frame"));

	if (const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin())
	{
		ViewportTab->SetContent
		(
			SAssignNew(SolverTracksWidget, SChaosVDSolverTracks, MainTabPtr->GetChaosVDEngineInstance()->GetPlaybackController())
		);
	}
	else
	{
		ViewportTab->SetContent(GenerateErrorWidget());
	}

	ViewportTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("TabIconPlaybackViewport"));

	OnTabSpawned().Broadcast(ViewportTab);

	return ViewportTab;
}

#undef LOCTEXT_NAMESPACE

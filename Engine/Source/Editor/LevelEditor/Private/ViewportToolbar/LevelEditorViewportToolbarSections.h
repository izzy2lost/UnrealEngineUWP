// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"
#include "ToolMenuEntry.h"

class FExtender;
class FLevelEditorViewportClient;
class SLevelViewport;
class SWidget;
class UToolMenu;
struct FToolMenuSection;

namespace UE::LevelEditor
{
void CreateCameraSpawnMenu(UToolMenu* InMenu);
void CreateBookmarksMenu(UToolMenu* InMenu, TWeakPtr<SLevelViewport> InViewport);
void CreateCameraSpeedMenu(UToolMenu* InMenu, const TWeakPtr<SLevelViewport>& InLevelViewportWeak);

FToolMenuEntry CreateFOVMenu(TWeakPtr<SLevelViewport> InLevelViewportWeak);
FToolMenuEntry CreateFarViewPlaneMenu(TWeakPtr<SLevelViewport> InInLevelViewportWeak);
FToolMenuEntry CreateFarViewPlaneMenu(TWeakPtr<SLevelViewport> InInLevelViewportWeak);
FToolMenuEntry CreateCameraSpeedSlider(TWeakPtr<SLevelViewport> InLevelViewportWeak);
FToolMenuEntry CreateCameraSpeedScalarSlider(TWeakPtr<SLevelViewport> InLevelViewportWeak);

bool ShowViewportRealtimeWarning(FLevelEditorViewportClient& ViewportClient);

TSharedPtr<FExtender> GetViewModesLegacyExtenders();
void PopulateViewModesMenu(UToolMenu* InMenu, TSharedRef<::SLevelViewport> InViewport);
void ExtendViewModesSubmenu(FName InViewModesSubmenuName);

FToolMenuEntry CreateShowFoliageSubmenu();
FToolMenuEntry CreateShowHLODsSubmenu();
FToolMenuEntry CreateShowLayersSubmenu();
FToolMenuEntry CreateShowSpritesSubmenu();
#if STATS
FToolMenuEntry CreateShowStatsSubmenu();
#endif
FToolMenuEntry CreateShowVolumesSubmenu();
FToolMenuEntry CreateViewportToolbarShowSubmenu();

FToolMenuEntry CreateFeatureLevelPreviewSubmenu();
FToolMenuEntry CreateMaterialQualityLevelSubmenu();
FToolMenuEntry CreateViewportToolbarPerformanceAndScalabilitySubmenu();

void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport);
TSharedRef<SWidget> BuildVolumeControlCustomWidget();
FToolMenuEntry CreateLevelEditorViewportToolbarSettingsSubmenu();

FToolMenuEntry CreateLevelEditorViewportToolbarCameraSubmenu();

} // namespace UE::LevelEditor

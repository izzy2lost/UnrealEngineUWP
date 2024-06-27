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

FToolMenuEntry CreateViewportToolbarSnappingSubmenu();

} // namespace UE::LevelEditor

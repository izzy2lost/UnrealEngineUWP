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

FToolMenuEntry CreateViewportToolbarTransformsSection();

FToolMenuEntry CreateViewportToolbarSelectionSection();

TSharedPtr<FExtender> GetViewModesLegacyExtenders();
void PopulateViewModesMenu(UToolMenu* InMenu, TSharedRef<SLevelViewport> InViewport);
FToolMenuEntry CreateViewportToolbarViewModesSubmenu();

FToolMenuEntry CreateFeatureLevelPreviewSubmenu();
FToolMenuEntry CreateMaterialQualityLevelSubmenu();
FToolMenuEntry CreateViewportToolbarPerformanceAndScalabilitySubmenu();

void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport);
TSharedRef<SWidget> BuildVolumeControlCustomWidget();
FToolMenuEntry CreateLevelEditorViewportToolbarSettingsSubmenu();

} // namespace UE::LevelEditor

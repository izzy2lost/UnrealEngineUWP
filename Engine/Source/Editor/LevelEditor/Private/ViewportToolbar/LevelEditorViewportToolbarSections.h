// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class FLevelEditorViewportClient;
class SLevelViewport;
class SWidget;
class UToolMenu;
struct FToolMenuSection;

namespace UE::LevelEditor
{

bool ShowViewportRealtimeWarning(FLevelEditorViewportClient& ViewportClient);

void AddViewportToolbarTransformsSection(FToolMenuSection& InSection);

void AddFeatureLevelPreviewSubmenu(FToolMenuSection& Section);
void AddMaterialQualityLevelSubmenu(FToolMenuSection& Section);
void AddViewportToolbarPerformanceAndScalabilitySubmenu(FToolMenuSection& InSection);

void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport);
TSharedRef<SWidget> BuildVolumeControlCustomWidget();
void AddLevelEditorViewportToolbarSettingsSubmenu(FToolMenuSection& InSection);

} // namespace UE::LevelEditor

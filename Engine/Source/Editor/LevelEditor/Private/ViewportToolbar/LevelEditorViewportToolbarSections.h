// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class SLevelViewport;
class UToolMenu;
struct FToolMenuSection;

namespace UE::LevelEditor
{

void AddViewportToolbarTransformsSection(FToolMenuSection& InSection);
void AddMaterialQualityLevelSubmenu(FToolMenuSection& Section);
void AddFeatureLevelPreviewSubmenu(FToolMenuSection& Section);
void GenerateViewportLayoutsMenu(UToolMenu* InMenu, TSharedPtr<::SLevelViewport> InViewport);
void AddLevelEditorViewportToolbarSettingsSection(FToolMenuSection& InSection);

} // namespace UE::LevelEditor

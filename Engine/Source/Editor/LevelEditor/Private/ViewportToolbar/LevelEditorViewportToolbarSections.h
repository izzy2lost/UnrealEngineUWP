// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FToolMenuSection;
class UToolMenu;

namespace UE::LevelEditor
{

void AddViewportToolbarTransformsSection(UToolMenu* InMenu);
void AddMaterialQualityLevelSubmenu(FToolMenuSection& Section);
void AddFeatureLevelPreviewSubmenu(FToolMenuSection& Section);
void AddLevelEditorViewportToolbarSettingsSection(UToolMenu* InMenu);

} // namespace UE::LevelEditor

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Visibility.h"
#include "Math/MathFwd.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointerFwd.h"

class FName;
class SDMMaterialEditor;
class SWidget;
class UDMMenuContext;
class UDynamicMaterialInstance;
class UDynamicMaterialModelBase;
class UToolMenu;
struct FToolMenuSection;
struct FUIAction;

class FDMToolBarMenus final
{
public:
	static TSharedRef<SWidget> MakeEditorLayoutMenu(const TSharedPtr<SDMMaterialEditor>& InEditorWidget = nullptr);

private:
	static void AddToolBarExportMenu(UToolMenu* InMenu);

	static void AddToolBarAdvancedSection(UToolMenu* InMenu);

	static void AddToolBarSettingsMenu(UToolMenu* InMenu);

	static void AddToolBarEditorLayoutMenu(UToolMenu* InMenu);

	static void AddToolBarBoolOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName, const FUIAction InAction);

	static void AddToolBarIntOptionMenuEntry(FToolMenuSection& InSection, const FName& InPropertyName,
		TAttribute<bool> InIsEnabledAttribute = TAttribute<bool>(),
		TAttribute<EVisibility> InVisibilityAttribute = TAttribute<EVisibility>());

	static void OpenMaterialEditorFromContext(UDMMenuContext* InMenuContext);

	static void ExportMaterialInstanceFromInstance(TWeakObjectPtr<UDynamicMaterialInstance> InMaterialInstanceWeak);

	static void ExportMaterialModelFromModel(TWeakObjectPtr<UDynamicMaterialModelBase> InMaterialModelBaseWeak);

	static void SnapshotMaterial(TWeakObjectPtr<UDynamicMaterialModelBase> InMaterialModelBaseWeak, FIntPoint InTextureSize);

	static void CreateSnapshotMaterialMenu(UToolMenu* InMenu);
};

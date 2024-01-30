// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Viewport/AvaViewportQualitySettings.h"
#include "AvalancheEditorSettings.generated.h"

USTRUCT()
struct FAvaPaletteSettings
{
	GENERATED_BODY()

	UPROPERTY(Config, VisibleAnywhere, Category = "Interface")
	FName Name;

	// Name is the TMap key
	UPROPERTY(Config, EditAnywhere, Category = "Interface")
	bool bEnablePalette = true;

	UPROPERTY(Config)
	bool bExpanded = false;
};

USTRUCT()
struct FAvaPaletteTabSettings
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, EditFixedSize, Category = "Interface", meta = (TitleProperty = "Name", FullyExpand))
	TArray<FAvaPaletteSettings> Palettes;
};

/**
 * Avalanche Editor Settings
 */
UCLASS(Config=EditorPerProjectUserSettings, meta = (DisplayName = "Editor"))
class UAvalancheEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAvalancheEditorSettings();

	virtual ~UAvalancheEditorSettings() override = default;

	static UAvalancheEditorSettings* Get();

	/** Whether to Automatically Include the Attached Actors when performing Edit Actions such as Cut, Copy, Duplicate. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bAutoIncludeAttachedActorsInEditActions = true;

	/** When Grouping Actors with a Null Actor, whether to keep the relative transform of these Actors */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	bool bKeepRelativeTransformWhenGrouping = false;

	/*
	 * Distance from the camera that new actors are created via the toolbox or drag and drop.
	 * Also sets the distance from the origin that new Camera Preview Viewport cameras are created.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float CameraDistance = 500.0f;

	/**  */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Toolbox")
	TArray<FString> FavoriteTools;

	/**  */
	UPROPERTY(Config, EditAnywhere, EditFixedSize, Category = "Interface", meta = (EditFixedOrder, ShowOnlyInnerProperties))
	TArray<FAvaPaletteTabSettings> PaletteTabs;

	/** Default viewport quality settings for all newly created Avalanche blueprints. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Quality")
	FAvaViewportQualitySettings DefaultViewportQualitySettings = FAvaViewportQualitySettings(true);

	/** Viewport quality settings user presets. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Quality")
	TMap<FName, FAvaViewportQualitySettings> ViewportQualityPresets;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettingsChanged, const UAvalancheEditorSettings* InSettings, FName InSetting)
	FOnSettingsChanged OnChanged;

	//~ Begin UObject
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject

	void OpenEditorSettingsWindow() const;

	/**
	 * AvaCineCamera uses CameraDistance property to setup manual focus in its default object.
	 * This function updates that default value to match the one from AvalancheEditorSettings
	 */
	void UpdateAvaCineCameraDefaults() const;
};

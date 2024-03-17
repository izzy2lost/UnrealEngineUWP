// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

class SDMTextureUVVisualizer;
class UDMMaterialStage;
class UDMTextureUV;

/**
 * Material Designer Texture UV Visualizer Popout
 *
 * Houses a Texture UV editor and a few buttons to control it.
 *
 * The popout specifically expands the visible area of the preview to 3x the normal size
 * on the smallest axis. The other axis is expanded to match the aspect ratio.
 */
class SDMTextureUVVisualizerPopout : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SDMTextureUVVisualizerPopout) {}
	SLATE_END_ARGS()

	/** The TextureUV should be a sub-property of the stage */
	void Construct(const FArguments& InArgs, UDMMaterialStage* InMaterialStage, UDMTextureUV* InTextureUV);

protected:
	TSharedPtr<SDMTextureUVVisualizer> Visualizer;

	FReply OnToggleModeClicked();

	FText GetModeButtonText() const;

	FVector2D GetHorizontalBarSize() const;

	FVector2D GetSideBlockSize() const;
};

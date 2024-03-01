// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"

namespace UE::ImageWidgets::Sample
{
	/**
	 * Provides commands used for the color viewer sample.
	 */
	class FColorViewerCommands : public TCommands<FColorViewerCommands>
	{
	public:
		FColorViewerCommands();

		virtual void RegisterCommands() override;

		TSharedPtr<FUICommandInfo> RandomizeColor; // Button and hotkey for setting a random color
		TSharedPtr<FUICommandInfo> ToneMappingRGB; // Toggle button for switching to RGB (no tone mapping)
		TSharedPtr<FUICommandInfo> ToneMappingLum; // Toggle button for switching to luminance (grayscale tone mapping)
	};
}

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

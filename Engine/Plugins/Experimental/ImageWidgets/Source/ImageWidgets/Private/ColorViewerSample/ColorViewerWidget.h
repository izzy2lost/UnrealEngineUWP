// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

#include "ColorViewer.h"

#include <SImageViewport.h>

namespace UE::ImageWidgets::Sample
{
	/**
	 * Widget that contains and configures the image widgets.
	 */
	class SColorViewerWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SColorViewerWidget)
			{
			}

		SLATE_END_ARGS()

		/** Gets called by Slate for construction of this widget. */
		void Construct(const FArguments& Args);

		/** Forwards key presses to the command bindings. */
		virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

	private:
		/** Functions for adding tone mapping buttons as viewport toolbar extensions. */
		void AddRandomColorButton(FToolBarBuilder& ToolbarBuilder) const;
		void AddToneMappingButtons(FToolBarBuilder& ToolbarBuilder) const;

		/** Binds all of the commands. */
		void BindCommands();

		/** The image viewer implementation that contains the image data and renders the image. */
		TSharedPtr<FColorViewer> ColorViewer;

		/** The image viewport in which the image gets displayed in. */
		TSharedPtr<SImageViewport> Viewport;

		/** The commands used by this sample widget. */
		TSharedPtr<FUICommandList> CommandList;
	};
}

#endif // IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE

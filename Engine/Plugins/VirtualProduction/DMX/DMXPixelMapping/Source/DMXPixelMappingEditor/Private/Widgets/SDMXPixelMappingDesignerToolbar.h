// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Styling/SlateTypes.h"
#include "SViewportToolBar.h"

class FDMXPixelMappingToolkit;


namespace UE::DMX
{
	class SDMXPixelMappingDesignerToolbar
		: public SViewportToolBar
	{
	public:
		SLATE_BEGIN_ARGS(SDMXPixelMappingDesignerToolbar)
			{}

			SLATE_EVENT(FOnClicked, OnZoomToFitClicked)

		SLATE_END_ARGS()

		/** Constructs this widget */
		void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit);

	private:
		/** Generates the grid snapping menu */
		TSharedRef<SWidget> GenerateSnapGridMenu();

		/** Returns the grid snapping label */
		FText GetSnapGridLabel() const;

		/** Gets the check box state according to grid snapping being enabled */
		ECheckBoxState GetSnapGridEnabledCheckState() const;

		/** Called when the grid snapping checkbox state changed */
		void OnSnapGridCheckStateChanged(ECheckBoxState NewCheckBoxState);

		/** The toolkit that owns this widget */
		TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
	};
}

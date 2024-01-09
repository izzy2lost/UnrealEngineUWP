// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Styling/SlateTypes.h"
#include "SViewportToolBar.h"

enum class ECheckBoxState : uint8;
class FDMXPixelMappingToolkit;
namespace UE::DMX { enum class EDMXPixelMappingTransformHandleMode : uint8; }


namespace UE::DMX
{
	class SDMXPixelMappingDesignerToolbar
		: public SViewportToolBar
	{
	public:
		SLATE_BEGIN_ARGS(SDMXPixelMappingDesignerToolbar)
			{}

			/** Event raised when zoom to fit was clicked */
			SLATE_EVENT(FOnClicked, OnZoomToFitClicked)

		SLATE_END_ARGS()

		~SDMXPixelMappingDesignerToolbar();

		/** Constructs this widget */
		void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit);

	private:
		/** Builds the toolbar for this widget */
		void RegisterToolbarMenu(const FArguments& InArgs);

		/** Generates the grid snapping menu */
		TSharedRef<SWidget> GenerateSnapGridMenu();

		/** Returns the grid snapping label */
		FText GetSnapGridLabel() const;

		/** Gets the check box state according to grid snapping being enabled */
		ECheckBoxState GetSnapGridEnabledCheckState() const;

		/** Called when the grid snapping checkbox state changed */
		void OnSnapGridCheckStateChanged(ECheckBoxState NewCheckBoxState);

		/** Called when a transform handle mode was selected. The checkbox state can be ignored. Instead clicking always enables. */
		void OnTransformHandleModeSelected(ECheckBoxState DummyCheckBoxState, UE::DMX::EDMXPixelMappingTransformHandleMode NewTransformHandleMode);

		/** Returns the current checkbox state of the transform mode */
		ECheckBoxState GetCheckboxStateForTransormHandleMode(UE::DMX::EDMXPixelMappingTransformHandleMode TransformHandleMode) const;

		/** The toolkit that owns this widget */
		TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
	};
}

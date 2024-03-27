// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SBaseCharacterFXEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class ADataflowActor;
class FAdvancedPreviewScene;
class UDataflowEditorMode;
class FDataflowConstructionViewportClient;

// ----------------------------------------------------------------------------------

class SDataflowSimulationViewport : public SBaseCharacterFXEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SDataflowSimulationViewport) {}
	SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, ViewportClient)
		SLATE_END_ARGS()

	SDataflowSimulationViewport();

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

	// SEditorViewport
	virtual void BindCommands() override;
	virtual bool IsVisible() const override;
	virtual void OnFocusViewportToSelection() override;

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

private:
	float GetViewMinInput() const;
	float GetViewMaxInput() const;

	UDataflowEditorMode* GetEdMode() const;
};


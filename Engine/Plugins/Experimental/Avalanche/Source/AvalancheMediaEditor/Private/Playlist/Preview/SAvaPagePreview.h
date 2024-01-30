// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAvaPlaylistEditor;
class FMenuBuilder;
class FUICommandList;
class UTextureRenderTarget2D;

class SAvaPagePreview : public SCompoundWidget
{
public:	
	SLATE_BEGIN_ARGS(SAvaPagePreview) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaPlaylistEditor>& InPlaylistEditor);
	SAvaPagePreview();
	virtual ~SAvaPagePreview() override;

	TSharedRef<SWidget> CreatePagePreviewToolBar(const TSharedRef<FUICommandList>& InCommandList);
	TSharedRef<SWidget> OnGenerateSettingsMenu();

protected:
	const FSlateBrush* GetPreviewBrush() const;
	bool IsBlendingEnabled() const;
	EVisibility GetCheckerboardVisibility() const;
	void SetPreviewResolution(FIntPoint InResolution) const;
	bool IsPreviewResolution(FIntPoint InResolution) const;
	
	UTextureRenderTarget2D* GetPreviewRenderTarget() const;
	
	void AddResolutionMenuEntry(FMenuBuilder& InOutMenuBuilder, const FText& Label, const FIntPoint& InResolution);
	
	void HandleCheckerboardActionExecute() const;
	void HandleSettingsActionExecute() const;

protected:
	TWeakPtr<FAvaPlaylistEditor> PlaylistEditorWeak;

	struct FPreviewBrush;
	TUniquePtr<FPreviewBrush> PreviewBrush;
};
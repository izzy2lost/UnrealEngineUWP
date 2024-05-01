// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Widgets/SCompoundWidget.h"
class UTextureGraph;
// struct FPropertyChangedEvent;
//
// class FPaperExtractSpritesViewportClient;
// struct Rect;

// struct FPaperExtractedSprite
// {
// 	FString Name;
// 	FIntRect Rect;
// };

//////////////////////////////////////////////////////////////////////////
// STG_ExportDialog

class STG_ExportDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STG_ExportDialog) {}
	SLATE_END_ARGS()

	// Constructs this widget with InArgs
	void Construct(const FArguments& InArgs, UTextureGraph* InTextureGraph);

	virtual ~STG_ExportDialog();

	// Show the dialog, returns true if successfully extracted sprites
	static bool ShowWindow(UTextureGraph* SourceTextureGraph);

private:
	// // Handler for Extract button
	// FReply ExtractClicked();
	//
	// // Handler for Cancel button
	// FReply CancelClicked();
	// void OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent);
	// void CloseContainingWindow();
	//
	// // Calculate a list of extracted sprite rects
	// void PreviewExtractedSprites();
	//
	// // Actually create extracted sprites
	// void CreateExtractedSprites();
	//
	// // Sets the details panel appropriately for the currently selected panel
	// void SetDetailsViewForActiveMode();

private:
	// Source texture graph to export
	UTextureGraph* SourceTextureGraph;
	// bool bDirtyExtractedSprites;
	//
	// class UPaperExtractSpritesSettings* ExtractSpriteSettings;
	// class UPaperExtractSpriteGridSettings* ExtractSpriteGridSettings;

	TSharedPtr<class IDetailsView> MainPropertyView;
	TSharedPtr<class IDetailsView> DetailsPropertyView;
	// TArray<FPaperExtractedSprite> ExtractedSprites;
};


//////////////////////////////////////////////////////////////////////////
// SPaperExtractSpritesViewport
//
// class SPaperExtractSpritesViewport : public SPaperEditorViewport
// {
// public:
// 	SLATE_BEGIN_ARGS(SPaperExtractSpritesViewport) {}
// 	SLATE_END_ARGS()
//
// 	~SPaperExtractSpritesViewport();
// 	void Construct(const FArguments& InArgs, UTexture2D* Texture, const TArray<FPaperExtractedSprite>& ExtractedSprites, const class UPaperExtractSpritesSettings* Settings, class STG_ExportDialog* InDialog);
//
// protected:
// 	// SPaperEditorViewport interface
// 	virtual FText GetTitleText() const override;
// 	// End of SPaperEditorViewport interface
//
// private:
// 	TWeakObjectPtr<class UTexture2D> TexturePtr;
// 	TSharedPtr<class FPaperExtractSpritesViewportClient> TypedViewportClient;
// 	class STG_ExportDialog* Dialog;
// };

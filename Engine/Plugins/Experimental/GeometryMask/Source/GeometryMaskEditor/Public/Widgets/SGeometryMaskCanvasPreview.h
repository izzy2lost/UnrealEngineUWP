// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryMaskTypes.h"
#include "Types/SlateStructs.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"

class UGeometryMaskCanvas;
class UMaterialInstanceDynamic;
class UTexture;

/** Displays a named GeometryMaskCanvas. */
class GEOMETRYMASKEDITOR_API SGeometryMaskCanvasPreview
	: public SCompoundWidget
	, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SGeometryMaskCanvasPreview) {}
		SLATE_ATTRIBUTE(FName, CanvasName)
		SLATE_ATTRIBUTE(EGeometryMaskColorChannel, Channel)
		SLATE_ATTRIBUTE(bool, Invert)
		SLATE_ATTRIBUTE(bool, SolidBackground)
		SLATE_ATTRIBUTE(float, Opacity)
	SLATE_END_ARGS()

	SGeometryMaskCanvasPreview();

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Get the name of the currently referenced canvas. */
	const FName GetCanvasName() const;

	/** Sets the name of the currently referenced canvas, and resolves the canvas itself. */
	void SetCanvasName(const FName InCanvasName);

	/** Get the color channel to display for the canvas.  */
	const EGeometryMaskColorChannel GetColorChannel(const bool bOnlyValid = true) const;

	/** Set the color channel to display for the canvas.  */
	void SetColorChannel(const EGeometryMaskColorChannel InColorChannel);

	/** Get whether to invert the display of the canvas.  */
	const bool IsInverted() const;

	/** Set whether to invert the display of the canvas.  */
	void SetInvert(const bool bInInverted);

	/** Get whether a solid background is used or not (vs. compositing with alpha on widgets below). */
	const bool HasSolidBackground() const;

	/** Set whether a solid background is used or not (vs. compositing with alpha on widgets below).  */
	void SetSolidBackground(const bool bInHasSolidBackground);

	/** Get the overall opacity multiplier value. */
	const float GetOpacity() const;

	/** Set the overall opacity multiplier value. */
	void SetOpacity(const float InValue);

	/** Get the currently referenced canvas. */
	UGeometryMaskCanvas* GetCanvas() const;

	/** Get the aspect ratio of the referenced canvas. */
	FOptionalSize GetAspectRatio();

	// ~Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override;
	// ~End FGCObject Interface

private:
	bool TryResolveCanvas();

	void UpdateBrush(const UGeometryMaskCanvas* InCanvas, UTexture* InTexture);

private:
	TSharedPtr<FSlateBrush> PreviewBrush;
	FSoftObjectPath PreviewMaterialPath;
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;

	FSoftObjectPath DefaultTexturePath;
	TObjectPtr<UTexture> DefaultTexture;

	TWeakObjectPtr<UGeometryMaskCanvas> Canvas;

	/** Name of the currently referenced canvas. */
	TAttribute<FName> CanvasName;

	/** Color channel to display for the canvas.  */
	TAttribute<EGeometryMaskColorChannel> ColorChannel;

	/** Whether to invert the display of the canvas.  */
	TAttribute<bool> bInvert;

	/*** Whether a solid background is used or not (vs. compositing with alpha on widgets below). */
	TAttribute<bool> bHasSolidBackground;

	/** Multiplies the overall opacity. */
	TAttribute<float> OpacityMultiplier;

	/** Stores the aspect ratio of the referenced canvas. */
	TAttribute<FOptionalSize> AspectRatio;
};

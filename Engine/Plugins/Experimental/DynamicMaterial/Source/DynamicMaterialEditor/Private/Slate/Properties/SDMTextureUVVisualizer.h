// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "SlateMaterialBrush.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FScopedTransaction;
class SImage;
class SWidget;
class UDMMaterialComponent;
class UDMMaterialStage;
class UDMTextureUV;
class UMaterialInterface;
enum class EDMUpdateType : uint8;

/**
 * Material Designer Texture UV Visualizer 
 *
 * Ability to edit Texture UV settings in a visual manner.
 */
class SDMTextureUVVisualizer : public SCompoundWidget
{
public:
	enum class EScrubbingMode : uint8
	{
		None,
		Offset,
		Rotation,
		Scale,
		Pivot
	};

	enum class EHandleAxis : uint8
	{
		None,
		X,
		Y,
		XY
	};

	SLATE_BEGIN_ARGS(SDMTextureUVVisualizer)
		: _IsPopout(false)
		{}
		SLATE_ARGUMENT(bool, IsPopout)
	SLATE_END_ARGS()

	SDMTextureUVVisualizer();

	/** The TextureUV should be a sub-property of the stage */
	void Construct(const FArguments& InArgs, UDMMaterialStage* InMaterialStage, UDMTextureUV* InTextureUV);

	EScrubbingMode GetScrubbingMode() const;

	bool IsInPivotEditMode() const;

	void SetInPivotEditMode(bool bInEditingPivot);

	void TogglePivotEditMode();

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent) const override;
	virtual int32 OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect, 
		FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool bInParentEnabled) const override;
	//~ End SWidget

protected:
	TWeakObjectPtr<UDMMaterialStage> StageWeak;
	TWeakObjectPtr<UDMTextureUV> TextureUVWeak;
	TSharedPtr<SImage> StageImage;
	TWeakObjectPtr<UMaterialInterface> PreviewMaterialWeak;
	FSlateMaterialBrush Brush;
	bool bIsPopout;
	bool bPivotEditMode;
	FVector2f CurrentAbsoluteSize;
	FVector2f CurrentAbsoluteCenter;
	EScrubbingMode ScrubbingMode;
	FVector2f ScrubbingStartAbsoluteCenter;
	FVector2f ScrubbingStartAbsoluteMouse;
	EHandleAxis HandleAxis;
	FVector2D ValueStart;
	bool bInvertScale;
	TSharedPtr<FScopedTransaction> ScrubbingTransaction;

	void OnStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	bool HasValidGeometry() const;

	float GetCircleHandleBaseRadius() const;

	FVector2f ApplyTextureUVTransform(const FVector2f& InUV) const;

	FVector2f GetCenterHandleLocation(const FVector2f& InSize) const;

	FVector2f GetPivotLocation(const FVector2f& InSize) const;

	FVector2f GetAbsoluteCenterHandleLocation() const;

	FVector2f GetAbsolutePivotLocation() const;

	/** Degrees clockwise from +Y axis */
	float GetCircleHandleRadiusAtAngle(float InAngle) const;

	EHandleAxis GetCenterHandleAxis(const FVector2f& InAbsolutePosition) const;

	EHandleAxis GetCircleHandleAxis(const FVector2f& InAbsolutePosition) const;

	bool TryClickCenterHandle(const FVector2f& InMousePosition, bool bInResetToDefault);

	bool TryClickCircleHandle(const FVector2f& InMousePosition, bool bInResetToDefault);

	void SetScrubbingMode(EScrubbingMode InMode, EHandleAxis InAxis);

	void UpdateScrub();
	void UpdateScrub_Offset();
	void UpdateScrub_Rotation();
	void UpdateScrub_Scale();
	void UpdateScrub_Pivot();
};

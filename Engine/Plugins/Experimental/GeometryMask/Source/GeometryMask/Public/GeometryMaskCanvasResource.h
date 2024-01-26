// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "GeometryMaskCanvasResource.generated.h"

class FGeometryMaskPostProcess_Blur;
class FGeometryMaskPostProcess_DistanceField;
class FGeometryMaskPostProcess_SobelTest;
class FCanvas;
class FSceneView;
class IGeometryMaskPostProcess;
class UCanvas;
class UTextureRenderTarget2D;
enum class EGeometryMaskColorChannel : uint8;

using FOnGeometryMaskCanvasDraw = TMulticastDelegate<void(FCanvas*)>;

/**  */
UCLASS()
class GEOMETRYMASK_API UGeometryMaskCanvasResource
	: public UObject
{
	GENERATED_BODY()

public:
	/** (R, G, B, A) */
	static constexpr int32 MaxNumChannels = 4;
	static constexpr int32 MaxTextureSize = 8192;
	
	UGeometryMaskCanvasResource();

	/** Will return the first available color channel without a canvas assigned. EGeometryMaskColorChannel::None if not available. */
	const EGeometryMaskColorChannel GetNextAvailableColorChannel() const;

	/** Requests usage of the given color channel for this resource. Will return true if successful. */
	bool Checkout(const EGeometryMaskColorChannel InColorChannel, const FName InRequestingCanvasName);

	/** Returns/frees the color channel associated with the given canvas name. Returns true if canvas name found. */
	bool Checkin(const FName InRequestingCanvasName);

	void UpdateViewportSize();

	void SetViewportSize(const FIntPoint& InViewportSize);

	/** Returns required viewport padding, in pixels - determined by certain effects. */
	int32 GetViewportPadding() const;

	UTextureRenderTarget2D* GetRenderTargetTexture();

	FOnGeometryMaskCanvasDraw& OnDrawToCanvas() { return OnDrawToCanvasDelegate; }

	void UpdateRenderParameters(EGeometryMaskColorChannel InColorChannel, bool bInApplyBlur, double InBlurStrength, bool bInApplyFeather, int32 InOuterFeatherRadius, int32 InInnerFeatherRadius);

	/** Updates the canvas, intended to be called every frame. */
	void Update(UWorld* InWorld, FSceneView& InView);

private:
	/** Draws all writers to the canvas. */
	void Draw(UWorld* InWorld, FSceneView& InView);

private:
	UPROPERTY(NoClear, EditFixedSize, meta = (EditFixedOrder))
	TMap<EGeometryMaskColorChannel, FName> DependentCanvasNames;

	/** Underlying UCanvas object. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvas> CanvasObject;

	/** The default viewport size to use when it can't be resolved from the actual viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "Auto", Setter, Category = "Rendering", meta = (AllowPrivateAccess = "true"))
	FIntPoint ViewportSize = FIntPoint(1920, 1080);
	
	/** The underlying Render Target texture. */
	UPROPERTY(DuplicateTransient)
	TObjectPtr<UTextureRenderTarget2D> RenderTargetTexture;

	FOnGeometryMaskCanvasDraw OnDrawToCanvasDelegate;

	/** The last resolved ViewProjectionMatrix. */
	FMatrix CachedViewProjectionMatrix;

	bool bApplyBlur = false;
	bool bApplyDF = false;

	TSharedPtr<FGeometryMaskPostProcess_Blur> PostProcess_Blur;
	TSharedPtr<FGeometryMaskPostProcess_DistanceField> PostProcess_DistanceField;
};

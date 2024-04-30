// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMRenderTargetRenderer.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/Text/TextLayout.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "DMRenderTargetTextRenderer.generated.h"

class FCanvasTextItem;
class UCanvas;
class UFont;

USTRUCT()
struct FDMTextLine
{
	GENERATED_BODY()

	UPROPERTY()
	FString Line;

	UPROPERTY()
	float Width = 0.f;
};

UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Render Target Text Renderer"))
class DYNAMICMATERIAL_API UDMRenderTargetTextRenderer : public UDMRenderTargetRenderer
{
	GENERATED_BODY()

	friend struct FDMRenderTargetTextRenderer;

public:
	UDMRenderTargetTextRenderer();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UFont* GetFont() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetFont(UFont* InFont);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FText& GetText() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetTextColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTextColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetBackgroundColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetBackgroundColor(const FLinearColor& InBackgroundColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextJustify::Type GetJustify() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetJustify(ETextJustify::Type InJustify);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetKerning() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetKerning(float InKerning);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetLineHeight() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetLineHeight(float InLineHeight);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetPaddingLeft() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPaddingLeft(float InPadding);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetPaddingRight() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPaddingRight(float InPadding);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetPaddingTop() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPaddingTop(float InPadding);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetPaddingBottom() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPaddingBottom(float InPadding);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FIntPoint& GetTextScale() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTextScale(const FIntPoint& InTextScale);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FIntPoint& GetTextureSizeOverride() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTextureSizeOverride(const FIntPoint& InTextureSizeOverride);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetHasOutline() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetHasOutline(bool bInHasOutline);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetOutlineColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetOutlineColor(const FLinearColor& InOutlineColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetHasShadow() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetHasShadow(bool bInHasShadow);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetShadowColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetShadowColor(const FLinearColor& InShadowColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetShadowOffset() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetShadowOffset(const FVector2D& InShadowOffset);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetHasGlow() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetHasGlow(bool bInHasGlow);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetGlowColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetGlowColor(const FLinearColor& InGlowColor);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetGlowInnerRadius() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetGlowInnerRadius(const FVector2D& InGlowInnerRadius);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetGlowOuterRadius() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetGlowOuterRadius(const FVector2D& InGlowOuterRadius);

#if WITH_EDITOR
	//~ Begin IDMJsonSerializable
	virtual TSharedPtr<FJsonValue> JsonSerialize() const override;
	virtual bool JsonDeserialize(const TSharedPtr<FJsonValue>& InJsonValue) override;
	//~ End IDMJsonSerializable

	//~ Begin UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject
#endif

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetFont, BlueprintSetter = SetFont, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	UFont* Font = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetText, BlueprintSetter = SetText, Category = "Material Designer|Text",
		meta = (MultiLine, NotKeyframeable, AllowPrivateAccess = "true"))
	FText Text = FText::GetEmpty();

	UPROPERTY()
	TArray<FDMTextLine> Lines = {};

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetTextColor, BlueprintSetter = SetTextColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetJustify, BlueprintSetter = SetJustify, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Left;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetKerning, BlueprintSetter = SetKerning, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	float Kerning = 0.f;

	/** Multiplier on the base font height. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetLineHeight, BlueprintSetter = SetLineHeight, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", UIMin = 0.1, UIMax = 2.0))
	float LineHeight = 1.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetPaddingLeft, BlueprintSetter = SetPaddingLeft, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingLeft = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetPaddingRight, BlueprintSetter = SetPaddingRight, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingRight = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetPaddingTop, BlueprintSetter = SetPaddingTop, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingTop = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetPaddingBottom, BlueprintSetter = SetPaddingBottom, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingBottom = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetTextScale, BlueprintSetter = SetTextScale, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", ClampMin = 1, UIMin = 1, ClampMax = 64, UIMax = 64,
			ToolTip = "Increasing this will increase the resolution of the text."))
	FIntPoint TextScale = FIntPoint(10, 10);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetTextureSizeOverride, BlueprintSetter = SetTextureSizeOverride, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", ToolTip = "Override the size of the render target. Set to 0 to disable."))
	FIntPoint TextureSizeOverride = FIntPoint(0, 0);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHasOutline, BlueprintSetter = SetHasOutline, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	bool bOutline = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetOutlineColor, BlueprintSetter = SetOutlineColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bOutline", EditConditionHides = true))
	FLinearColor OutlineColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHasShadow, BlueprintSetter = SetHasShadow, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	bool bShadow = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetShadowColor, BlueprintSetter = SetShadowColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bShadow", EditConditionHides = true))
	FLinearColor ShadowColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetShadowOffset, BlueprintSetter = SetShadowOffset, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bShadow", EditConditionHides = true))
	FVector2D ShadowOffset = FVector2D(1.0, 1.0);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHasGlow, BlueprintSetter = SetHasGlow, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	bool bGlow = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetGlowColor, BlueprintSetter = SetGlowColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true))
	FLinearColor GlowColor = FLinearColor::White;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetGlowInnerRadius, BlueprintSetter = SetGlowInnerRadius, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true, ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	FVector2D GlowInnerRadius = FVector2D(0.5, 0.5);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetGlowOuterRadius, BlueprintSetter = SetGlowOuterRadius, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true, ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	FVector2D GlowOuterRadius = FVector2D(0.5, 0.5);

	bool bRecalculateTextSize = false;

	void CalculateLineSizes();

	FIntPoint GetRequiredTextureSize() const;

	void UpdateTextLines();

	FCanvasTextItem CreateTextItem(const FVector2D& InPosition, const FText& InText) const;

	void UpdateTextureSize();

	//~ Begin UDMRenderTargetRenderer
	virtual void UpdateRenderTarget_Internal() override;
	//~ End UDMRenderTargetRenderer
};

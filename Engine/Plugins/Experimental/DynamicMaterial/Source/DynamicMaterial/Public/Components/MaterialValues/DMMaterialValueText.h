// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/Text/TextLayout.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "DMMaterialValueText.generated.h"

class FCanvasTextItem;
class UCanvas;
class UDMMaterialValueTexture;
class UFont;
class UTextureRenderTarget2D;

USTRUCT()
struct FDMTextLine
{
	GENERATED_BODY()

	UPROPERTY()
	FString Line;

	UPROPERTY()
	float Width;
};

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIAL_API UDMMaterialValueText : public UDMMaterialValueTexture
{
	GENERATED_BODY()

	friend class SDMComponentEdit;
	friend struct FDMMaterialValueText;

public:
	UDMMaterialValueText();

#if WITH_EDITOR
	//~ Begin UDMMaterialValue
	virtual bool AllowEditValue() const override { return false; }
	//~ End UDMMaterialValue
#endif

	//~ Begin UObject
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	UFont* GetFont() const;
	void SetFont(UFont* InFont);

	const FText& GetText() const;
	void SetText(const FText& InText);

	const FLinearColor& GetColor() const;
	void SetColor(const FLinearColor& InColor);

	const FLinearColor& GetBackgroundColor() const;
	void SetBackgroundColor(const FLinearColor& InBackgroundColor);

	ETextJustify::Type GetJustify() const;
	void SetJustify(ETextJustify::Type InJustify);

	float GetKerning() const;
	void SetKerning(float InKerning);

	float GetLineHeight() const;
	void SetLineHeight(float InLineHeight);

	float GetPaddingLeft() const;
	void SetPaddingLeft(float InPadding);

	float GetPaddingRight() const;
	void SetPaddingRight(float InPadding);

	float GetPaddingTop() const;
	void SetPaddingTop(float InPadding);

	float GetPaddingBottom() const;
	void SetPaddingBottom(float InPadding);

	const FIntPoint& GetTextScale() const;
	void SetTextScale(const FIntPoint& InTextScale);

	const FIntPoint& GetTextureSizeOverride() const;
	void SetTextureSizeOverride(const FIntPoint& InTextureSizeOverride);

	bool GetHasOutline() const;
	void SetHasOutline(bool bInHasOutline);

	const FLinearColor& GetOutlineColor() const;
	void SetOutlineColor(const FLinearColor& InOutlineColor);

	bool GetHasShadow() const;
	void SetHasShadow(bool bInHasShadow);

	const FLinearColor& GetShadowColor() const;
	void SetShadowColor(const FLinearColor& InShadowColor);

	const FVector2D& GetShadowOffset() const;
	void SetShadowOffset(const FVector2D& InShadowOffset);

	bool GetHasGlow() const;
	void SetHasGlow(bool bInHasGlow);

	const FLinearColor& GetGlowColor() const;
	void SetGlowColor(const FLinearColor& InGlowColor);

	const FVector2D& GetGlowInnerRadius() const;
	void SetGlowInnerRadius(const FVector2D& InGlowInnerRadius);

	const FVector2D& GetGlowOuterRadius() const;
	void SetGlowOuterRadius(const FVector2D& InGlowOuterRadius);
	
protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetFont, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	UFont* Font = nullptr;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetText, Category = "Material Designer",
		meta = (MultiLine, NotKeyframeable, AllowPrivateAccess = "true"))
	FText Text = FText::GetEmpty();

	UPROPERTY()
	TArray<FDMTextLine> Lines = {};

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetColor, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor Color = FLinearColor::White;

	/** Clear color of the render target. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetBackgroundColor, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor BackgroundColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetJustify, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Left;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetKerning, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	float Kerning = 0.f;

	/** Multiplier on the base font height. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetLineHeight, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", UIMin = 0.1, UIMax = 2.0))
	float LineHeight = 1.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetPaddingLeft, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingLeft = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetPaddingRight, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingRight = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetPaddingTop, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingTop = 0.f;

	/** Extra space adding beyond the edge of the glyphs. Useful for shadows, glows, etc. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetPaddingBottom, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	float PaddingBottom = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetTextScale, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", ClampMin = 1, UIMin = 1, ClampMax = 64, UIMax = 64,
			ToolTip = "Increasing this will increase the resolution of the text."))
	FIntPoint TextScale = FIntPoint(10, 10);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetTextureSizeOverride, Category = "Material Designer",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", ToolTip = "Override the size of the render target. Set to 0 to disable."))
	FIntPoint TextureSizeOverride = FIntPoint(0, 0);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetHasOutline, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	bool bOutline = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetOutlineColor, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition = "bOutline", EditConditionHides = true))
	FLinearColor OutlineColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetHasShadow, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	bool bShadow = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetShadowColor, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition = "bShadow", EditConditionHides = true))
	FLinearColor ShadowColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetShadowOffset, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition="bShadow", EditConditionHides = true))
	FVector2D ShadowOffset = FVector2D(1.0, 1.0);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetHasGlow, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true"))
	bool bGlow = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetGlowColor, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true))
	FLinearColor GlowColor = FLinearColor::White;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetGlowInnerRadius, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true, ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	FVector2D GlowInnerRadius = FVector2D(0.5, 0.5);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Setter = SetGlowOuterRadius, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", EditCondition = "bGlow", EditConditionHides = true, ClampMin = 0, UIMin = 0, ClampMax = 1, UIMax = 1))
	FVector2D GlowOuterRadius = FVector2D(0.5, 0.5);

	UPROPERTY(Instanced)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	FDelegateHandle EndOfFrameDelegateHandle;

	bool bNeedsResize = false;

	static UTextureRenderTarget2D* CreateRenderTarget(UObject* InOuter, const FIntPoint& InSize, const FLinearColor& InClearColor);

	void CalculateLineSizes();

	FIntPoint GetRequiredTextureSize() const;

	void UpdateTextLines();

	void RequestUpdateTextTexture(bool bInNeedsResize);

	void UpdateTextTexture();

	FCanvasTextItem CreateTextItem(const FVector2D& InPosition, const FText& InText) const;
};

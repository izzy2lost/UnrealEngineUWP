// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/RenderTargetRenderers/DMRenderTargetWidgetRendererBase.h"
#include "Delegates/IDelegateInstance.h"
#include "Fonts/FontCache.h"
#include "Framework/Text/TextLayout.h"
#include "InstancedStruct.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"
#include "DMRenderTargetTextRenderer.generated.h"

class FCanvasTextItem;
class STextBlock;
class SWidget;
class UCanvas;
class UFont;

USTRUCT()
struct FDMTextLine
{
	GENERATED_BODY()

	UPROPERTY()
	FString Line;

	TSharedPtr<STextBlock> Widget;
};

UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Render Target Text Renderer"))
class DYNAMICMATERIAL_API UDMRenderTargetTextRenderer : public UDMRenderTargetWidgetRendererBase
{
	GENERATED_BODY()

	friend struct FDMRenderTargetTextRenderer;

public:
	UDMRenderTargetTextRenderer();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FSlateFontInfo& GetFontInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetFontInfo(const FSlateFontInfo& InFontInfo);

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
	bool GetHasHighlight() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetHasHighlight(bool bInHasHighlight);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FLinearColor& GetHighlightColor() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetHighlightColor(const FLinearColor& InHighlightColor);

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
	bool GetAutoWrapText() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetAutoWrapText(bool bInAutoWrap);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetWrapTextAt() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetWrapTextAt(float InWrapAt);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextWrappingPolicy GetWrappingPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetWrappingPolicy(ETextWrappingPolicy InWrappingPolicy);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextJustify::Type GetJustify() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetJustify(ETextJustify::Type InJustify);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextTransformPolicy GetTransformPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetTransformPolicy(ETextTransformPolicy InTransformPolicy);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextFlowDirection GetFlowDirection() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetFlowDirection(ETextFlowDirection InFlowDirection);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	ETextShapingMethod GetShapingMethod() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetShapingMethod(ETextShapingMethod InShapingMethod);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TInstancedStruct<FSlateBrush>& GetStrikeBrush() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetStrikeBrush(const TInstancedStruct<FSlateBrush>& InStrikeBrush);

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
	bool IsOverridingRenderTargetSize() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetOverrideRenderTargetSize(bool bInOverride);

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
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetFontInfo, BlueprintSetter = SetFontInfo, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	FSlateFontInfo FontInfo;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetText, BlueprintSetter = SetText, Category = "Material Designer|Text",
		meta = (MultiLine, NotKeyframeable, AllowPrivateAccess = "true"))
	FText Text = FText::GetEmpty();

	UPROPERTY()
	TArray<FDMTextLine> Lines = {};

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetTextColor, BlueprintSetter = SetTextColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor TextColor = FLinearColor::White;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHasHighlight, BlueprintSetter = SetHasHighlight, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	bool bHasHighlight = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHighlightColor, BlueprintSetter = SetHighlightColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bHasHighlight", EditConditionHides = true))
	FLinearColor HighlightColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetHasShadow, BlueprintSetter = SetHasShadow, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	bool bHasShadow = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetShadowColor, BlueprintSetter = SetShadowColor, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bHasShadow", EditConditionHides = true))
	FLinearColor ShadowColor = FLinearColor::Black;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetShadowOffset, BlueprintSetter = SetShadowOffset, Category = "Material Designer|Text",
		meta = (AllowPrivateAccess = "true", EditCondition = "bHasShadow", EditConditionHides = true))
	FVector2D ShadowOffset = FVector2D(1.0, 1.0);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetAutoWrapText, BlueprintSetter = SetAutoWrapText, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	bool bAutoWrapText = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetWrapTextAt, BlueprintSetter = SetWrapTextAt, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", EditCondition="bAutoWrapText", EditConditionHides))
	float WrapTextAt = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetWrappingPolicy, BlueprintSetter = SetWrappingPolicy, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", EditCondition = "bAutoWrapText", EditConditionHides))
	ETextWrappingPolicy WrappingPolicy = ETextWrappingPolicy::DefaultWrapping;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetJustify, BlueprintSetter = SetJustify, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	TEnumAsByte<ETextJustify::Type> Justify = ETextJustify::Left;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetTransformPolicy, BlueprintSetter = SetTransformPolicy, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	ETextTransformPolicy TransformPolicy = ETextTransformPolicy::None;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetFlowDirection, BlueprintSetter = SetFlowDirection, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	ETextFlowDirection FlowDirection = ETextFlowDirection::Auto;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetShapingMethod, BlueprintSetter = SetShapingMethod, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	ETextShapingMethod ShapingMethod = ETextShapingMethod::Auto;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetStrikeBrush, BlueprintSetter = SetStrikeBrush, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true"))
	TInstancedStruct<FSlateBrush> StrikeBrush;

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

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter = SetOverrideRenderTargetSize, BlueprintSetter = SetOverrideRenderTargetSize, Category = "Material Designer|Text",
		meta = (NotKeyframeable, AllowPrivateAccess = "true", ToolTip = "When true, will change the size of the render target to fit the text."))
	bool bOverrideRenderTargetSize = true;

	bool bRecalculateTextSize = false;

	void UpdateTextLines();

	TSharedRef<STextBlock> CreateTextWidget(const FText& InText) const;

	void SetCustomTextureSize();

	//~ Begin UDMRenderTargetWidgetRendererBase
	virtual void CreateWidgetInstance() override;
	//~ End UDMRenderTargetWidgetRendererBase

	//~ Begin UDMRenderTargetRenderer
	virtual void UpdateRenderTarget_Internal() override;
	//~ End UDMRenderTargetRenderer
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/RenderTargetRenderers/DMRenderTargetTextRenderer.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/MaterialValues/DMMaterialValueRenderTarget.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"

#if WITH_EDITOR
#include "Dom/JsonValue.h"
#endif

#define LOCTEXT_NAMESPACE "DMRenderTargetTextRenderer"

namespace UE::DynamicMaterial::Private
{
	const FIntPoint MinimumTextTextureSize = FIntPoint(5, 10);
	const FIntPoint MaxTexScale = FIntPoint(64, 64);
}

#if WITH_EDITOR
struct FDMRenderTargetTextRenderer
{
	static const inline FName FontName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, Font);
	static const inline FName TextName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, Text);
	static const inline FName TextColorName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, TextColor);
	static const inline FName JustifyName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, Justify);
	static const inline FName KerningName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, Kerning);
	static const inline FName LineHeightName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, LineHeight);
	static const inline FName PaddingLeftName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, PaddingLeft);
	static const inline FName PaddingRightName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, PaddingRight);
	static const inline FName PaddingTopName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, PaddingTop);
	static const inline FName PaddingBottomName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, PaddingBottom);
	static const inline FName TextScaleName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, TextScale);
	static const inline FName TextureSizeOverrideName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, TextureSizeOverride);
	static const inline FName bOutlineName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, bOutline);
	static const inline FName OutlineColorName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, OutlineColor);
	static const inline FName bShadowName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, bShadow);
	static const inline FName ShadowColorName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, ShadowColor);
	static const inline FName ShadowOffsetName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, ShadowOffset);
	static const inline FName bGlowName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, bGlow);
	static const inline FName GlowColorName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, GlowColor);
	static const inline FName GlowInnerRadiusName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, GlowInnerRadius);
	static const inline FName GlowOuterRadiusName = GET_MEMBER_NAME_CHECKED(UDMRenderTargetTextRenderer, GlowOuterRadius);

	static const inline TSet<FName> PropertyNames = {
		FontName, TextName, TextColorName, JustifyName, KerningName, LineHeightName, PaddingLeftName, PaddingRightName,
		PaddingTopName, PaddingBottomName, TextScaleName, TextureSizeOverrideName, bOutlineName, OutlineColorName, bShadowName,
		ShadowColorName, ShadowOffsetName, bGlowName, GlowColorName, GlowInnerRadiusName, GlowOuterRadiusName
	};
};
#endif

UDMRenderTargetTextRenderer::UDMRenderTargetTextRenderer()
{
#if WITH_EDITOR
	EditableProperties.Append(FDMRenderTargetTextRenderer::PropertyNames.Array());
#endif
}

#if WITH_EDITOR
TSharedPtr<FJsonValue> UDMRenderTargetTextRenderer::JsonSerialize() const
{
	return MakeShared<FJsonValueNull>();
}

bool UDMRenderTargetTextRenderer::JsonDeserialize(const TSharedPtr<FJsonValue>& InJsonValue)
{
	return true;
}

void UDMRenderTargetTextRenderer::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == FDMRenderTargetTextRenderer::TextName)
	{
		UpdateTextLines();
	}
	else if (PropertyName == FDMRenderTargetTextRenderer::FontName
		|| PropertyName == FDMRenderTargetTextRenderer::KerningName
		|| PropertyName == FDMRenderTargetTextRenderer::LineHeightName
		|| PropertyName == FDMRenderTargetTextRenderer::PaddingLeftName
		|| PropertyName == FDMRenderTargetTextRenderer::PaddingRightName
		|| PropertyName == FDMRenderTargetTextRenderer::PaddingTopName
		|| PropertyName == FDMRenderTargetTextRenderer::PaddingBottomName
		|| PropertyName == FDMRenderTargetTextRenderer::TextScaleName
		|| PropertyName == FDMRenderTargetTextRenderer::TextureSizeOverrideName)
	{
		TextScale.X = FMath::Clamp(TextScale.X, 1, UE::DynamicMaterial::Private::MaxTexScale.X);
		TextScale.Y = FMath::Clamp(TextScale.Y, 1, UE::DynamicMaterial::Private::MaxTexScale.Y);
		bRecalculateTextSize = true;
		AsyncUpdateRenderTarget();
	}
	else if (PropertyName == FDMRenderTargetTextRenderer::bGlowName
		|| PropertyName == FDMRenderTargetTextRenderer::bShadowName
		|| PropertyName == FDMRenderTargetTextRenderer::bOutlineName)
	{
		// Cause details panel refresh.
		Update(EDMUpdateType::Structure);
		AsyncUpdateRenderTarget();
	}
	else if (FDMRenderTargetTextRenderer::PropertyNames.Contains(PropertyName))
	{
		AsyncUpdateRenderTarget();
	}
}
#endif

UFont* UDMRenderTargetTextRenderer::GetFont() const
{
	return Font;
}

void UDMRenderTargetTextRenderer::SetFont(UFont* InFont)
{
	if (Font == InFont)
	{
		return;
	}

	Font = InFont;

	UpdateTextureSize();
}

const FText& UDMRenderTargetTextRenderer::GetText() const
{
	return Text;
}

void UDMRenderTargetTextRenderer::SetText(const FText& InText)
{
	if (Text.EqualTo(InText, ETextComparisonLevel::Default))
	{
		return;
	}

	Text = InText;

	UpdateTextLines();
	UpdateTextureSize();
}

const FLinearColor& UDMRenderTargetTextRenderer::GetTextColor() const
{
	return TextColor;
}

void UDMRenderTargetTextRenderer::SetTextColor(const FLinearColor& InColor)
{
	if (TextColor == InColor)
	{
		return;
	}

	TextColor = InColor;

	AsyncUpdateRenderTarget();
}

const FLinearColor& UDMRenderTargetTextRenderer::GetBackgroundColor() const
{
	if (UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue())
	{
		return RenderTargetValue->GetClearColor();
	}

	return FLinearColor::Black;
}

void UDMRenderTargetTextRenderer::SetBackgroundColor(const FLinearColor& InBackgroundColor)
{
	if (UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue())
	{
		RenderTargetValue->SetClearColor(InBackgroundColor);
	}
}

ETextJustify::Type UDMRenderTargetTextRenderer::GetJustify() const
{
	return Justify;
}

void UDMRenderTargetTextRenderer::SetJustify(ETextJustify::Type InJustify)
{
	if (Justify == InJustify)
	{
		return;
	}

	Justify = InJustify;

	AsyncUpdateRenderTarget();
}

float UDMRenderTargetTextRenderer::GetKerning() const
{
	return Kerning;
}

void UDMRenderTargetTextRenderer::SetKerning(float InKerning)
{
	if (Kerning == InKerning)
	{
		return;
	}

	Kerning = InKerning;

	UpdateTextureSize();
}

const FLinearColor& UDMRenderTargetTextRenderer::GetOutlineColor() const
{
	return OutlineColor;
}

void UDMRenderTargetTextRenderer::SetOutlineColor(const FLinearColor& InOutlineColor)
{
	if (OutlineColor == InOutlineColor)
	{
		return;
	}

	OutlineColor = InOutlineColor;

	AsyncUpdateRenderTarget();
}

float UDMRenderTargetTextRenderer::GetLineHeight() const
{
	return LineHeight;
}

void UDMRenderTargetTextRenderer::SetLineHeight(float InLineHeight)
{
	if (LineHeight == InLineHeight)
	{
		return;
	}

	LineHeight = InLineHeight;

	UpdateTextureSize();
}

float UDMRenderTargetTextRenderer::GetPaddingLeft() const
{
	return PaddingLeft;
}

void UDMRenderTargetTextRenderer::SetPaddingLeft(float InPaddingLeft)
{
	if (PaddingLeft == InPaddingLeft)
	{
		return;
	}

	PaddingLeft = InPaddingLeft;

	UpdateTextureSize();
}

float UDMRenderTargetTextRenderer::GetPaddingRight() const
{
	return PaddingRight;
}

void UDMRenderTargetTextRenderer::SetPaddingRight(float InPaddingRight)
{
	if (PaddingRight == InPaddingRight)
	{
		return;
	}

	PaddingRight = InPaddingRight;

	UpdateTextureSize();
}

float UDMRenderTargetTextRenderer::GetPaddingTop() const
{
	return PaddingTop;
}

void UDMRenderTargetTextRenderer::SetPaddingTop(float InPaddingTop)
{
	if (PaddingTop == InPaddingTop)
	{
		return;
	}

	PaddingTop = InPaddingTop;

	UpdateTextureSize();
}

float UDMRenderTargetTextRenderer::GetPaddingBottom() const
{
	return PaddingBottom;
}

void UDMRenderTargetTextRenderer::SetPaddingBottom(float InPaddingBottom)
{
	if (PaddingBottom == InPaddingBottom)
	{
		return;
	}

	PaddingBottom = InPaddingBottom;

	UpdateTextureSize();
}

const FIntPoint& UDMRenderTargetTextRenderer::GetTextScale() const
{
	return TextScale;
}

void UDMRenderTargetTextRenderer::SetTextScale(const FIntPoint& InTextScale)
{
	FIntPoint NewTextScale = InTextScale;
	NewTextScale.X = FMath::Clamp(InTextScale.X, 1, UE::DynamicMaterial::Private::MaxTexScale.X);
	NewTextScale.Y = FMath::Clamp(InTextScale.Y, 1, UE::DynamicMaterial::Private::MaxTexScale.Y);

	if (TextScale == NewTextScale)
	{
		return;
	}

	TextScale = NewTextScale;

	UpdateTextureSize();
}

const FIntPoint& UDMRenderTargetTextRenderer::GetTextureSizeOverride() const
{
	return TextureSizeOverride;
}

void UDMRenderTargetTextRenderer::SetTextureSizeOverride(const FIntPoint& InTextureSizeOverride)
{
	if (TextureSizeOverride == InTextureSizeOverride)
	{
		return;
	}

	TextureSizeOverride = InTextureSizeOverride;

	UpdateTextureSize();
}

bool UDMRenderTargetTextRenderer::GetHasOutline() const
{
	return bOutline;
}

void UDMRenderTargetTextRenderer::SetHasOutline(bool bInHasOutline)
{
	if (bOutline == bInHasOutline)
	{
		return;
	}

	bOutline = bInHasOutline;

	AsyncUpdateRenderTarget();
}

bool UDMRenderTargetTextRenderer::GetHasShadow() const
{
	return bShadow;
}

void UDMRenderTargetTextRenderer::SetHasShadow(bool bInHasShadow)
{
	if (bShadow == bInHasShadow)
	{
		return;
	}

	bShadow = bInHasShadow;

	AsyncUpdateRenderTarget();
}

const FLinearColor& UDMRenderTargetTextRenderer::GetShadowColor() const
{
	return ShadowColor;
}

void UDMRenderTargetTextRenderer::SetShadowColor(const FLinearColor& InShadowColor)
{
	if (ShadowColor == InShadowColor)
	{
		return;
	}

	ShadowColor = InShadowColor;

	AsyncUpdateRenderTarget();
}

const FVector2D& UDMRenderTargetTextRenderer::GetShadowOffset() const
{
	return ShadowOffset;
}

void UDMRenderTargetTextRenderer::SetShadowOffset(const FVector2D& InShadowOffset)
{
	if (ShadowOffset == InShadowOffset)
	{
		return;
	}

	ShadowOffset = InShadowOffset;

	AsyncUpdateRenderTarget();
}

bool UDMRenderTargetTextRenderer::GetHasGlow() const
{
	return bGlow;
}

void UDMRenderTargetTextRenderer::SetHasGlow(bool bInHasGlow)
{
	if (bGlow == bInHasGlow)
	{
		return;
	}

	bGlow = bInHasGlow;

	AsyncUpdateRenderTarget();
}

const FLinearColor& UDMRenderTargetTextRenderer::GetGlowColor() const
{
	return GlowColor;
}

void UDMRenderTargetTextRenderer::SetGlowColor(const FLinearColor& InGlowColor)
{
	if (GlowColor == InGlowColor)
	{
		return;
	}

	GlowColor = InGlowColor;

	AsyncUpdateRenderTarget();
}

const FVector2D& UDMRenderTargetTextRenderer::GetGlowInnerRadius() const
{
	return GlowInnerRadius;
}

void UDMRenderTargetTextRenderer::SetGlowInnerRadius(const FVector2D& InGlowInnerRadius)
{
	if (GlowInnerRadius == InGlowInnerRadius)
	{
		return;
	}

	GlowInnerRadius = InGlowInnerRadius;

	AsyncUpdateRenderTarget();
}

const FVector2D& UDMRenderTargetTextRenderer::GetGlowOuterRadius() const
{
	return GlowOuterRadius;
}

void UDMRenderTargetTextRenderer::SetGlowOuterRadius(const FVector2D& InGlowOuterRadius)
{
	if (GlowOuterRadius == InGlowOuterRadius)
	{
		return;
	}

	GlowOuterRadius = InGlowOuterRadius;

	AsyncUpdateRenderTarget();
}

void UDMRenderTargetTextRenderer::CalculateLineSizes()
{
	FTextSizingParameters Params(0, 0, 0, 0, Font);
	Params.Scaling = FVector2D(1, 1);

	for (FDMTextLine& Line : Lines)
	{
		Params.DrawXL = 0;
		Params.DrawYL = 0;

		UCanvas::CanvasStringSize(Params, *Line.Line);

		int32 RequiredWidth = Params.DrawXL;
		RequiredWidth += Line.Line.Len() > 0 ? ((Line.Line.Len() - 1) * Kerning) : 0;

		Line.Width = RequiredWidth;
	}
}

FIntPoint UDMRenderTargetTextRenderer::GetRequiredTextureSize() const
{
	if (TextureSizeOverride.X > 0 && TextureSizeOverride.Y > 0)
	{
		return TextureSizeOverride;
	}

	if (!IsValid(Font) || Lines.IsEmpty())
	{
		return UE::DynamicMaterial::Private::MinimumTextTextureSize;
	}

	const float MaxCharHeight = Font->GetMaxCharHeight();

	if (MaxCharHeight <= 0)
	{
		return UE::DynamicMaterial::Private::MinimumTextTextureSize;
	}

	FVector2f Size = FVector2f::ZeroVector;

	for (const FDMTextLine& Line : Lines)
	{
		Size.X = FMath::Max(Size.X, Line.Width);
		Size.Y += MaxCharHeight * LineHeight;
	}

	Size.X += PaddingLeft + PaddingRight;
	Size.Y += PaddingTop + PaddingBottom;

	Size.X = FMath::Max(Size.X, UE::DynamicMaterial::Private::MinimumTextTextureSize.X);
	Size.Y = FMath::Max(Size.Y, UE::DynamicMaterial::Private::MinimumTextTextureSize.Y);

	FIntPoint RequiredSize = FIntPoint::ZeroValue;
	RequiredSize.X = FMath::CeilToInt(Size.X * TextScale.X);
	RequiredSize.Y = FMath::CeilToInt(Size.Y * TextScale.Y);

	return RequiredSize;
}

void UDMRenderTargetTextRenderer::UpdateTextLines()
{
	const FString TextStr = Text.ToString();
	TArray<FString> NewLines;
	TextStr.ParseIntoArray(NewLines, TEXT("\n"), /* Cull empty */ false);

	Lines.Empty();
	Lines.Reserve(NewLines.Num());

	for (FString& NewLine : NewLines)
	{
		if (NewLine.EndsWith(TEXT("\r")))
		{
			NewLine.LeftChopInline(1);
		}

		Lines.Add({NewLine, -1.f});
	}

	bRecalculateTextSize = true;
	AsyncUpdateRenderTarget();
}

FCanvasTextItem UDMRenderTargetTextRenderer::CreateTextItem(const FVector2D& InPosition, const FText& InText) const
{
	FCanvasTextItem TextItem(InPosition, InText, Font, TextColor);

	TextItem.BlendMode = SE_BLEND_Translucent; // Must be translucent for fonts!
	TextItem.HorizSpacingAdjust = Kerning;

	if (bOutline)
	{
		TextItem.bOutlined = true;
		TextItem.OutlineColor = OutlineColor;
	}
	else
	{
		TextItem.bOutlined = false;
	}

	if (bShadow)
	{
		TextItem.EnableShadow(ShadowColor, ShadowOffset);
	}
	else
	{
		TextItem.DisableShadow();
	}

	if (bGlow)
	{
		TextItem.FontRenderInfo.GlowInfo.bEnableGlow = true;
		TextItem.FontRenderInfo.GlowInfo.GlowColor = GlowColor;
		TextItem.FontRenderInfo.GlowInfo.GlowInnerRadius = GlowInnerRadius;
		TextItem.FontRenderInfo.GlowInfo.GlowOuterRadius = GlowOuterRadius;
	}
	else
	{
		TextItem.FontRenderInfo.GlowInfo.bEnableGlow = false;
	}

	return TextItem;
}

void UDMRenderTargetTextRenderer::UpdateTextureSize()
{
	UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue();

	if (!RenderTargetValue)
	{
		return;
	}

	const FIntPoint RequiredSize = GetRequiredTextureSize();

	RenderTargetValue->SetTextureSize(RequiredSize);
	RenderTargetValue->FlushCreateRenderTarget();
}

void UDMRenderTargetTextRenderer::UpdateRenderTarget_Internal()
{
	Super::UpdateRenderTarget();

	if (!IsValid(Font) || Lines.IsEmpty())
	{
		return;
	}

	UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue();

	if (!RenderTargetValue)
	{
		return;
	}

	if (bRecalculateTextSize)
	{
		CalculateLineSizes();
		UpdateTextureSize();
	}

	UTextureRenderTarget2D* RenderTarget = RenderTargetValue->GetRenderTarget();

	if (!RenderTarget)
	{
		return;
	}

	RenderTarget->UpdateResourceImmediate(true);

	FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();

	if (!RenderTargetResource)
	{
		return;
	}

	static const FGameTime GameTime = FGameTime::CreateUndilated(0, 0);

	FCanvas RenderCanvas(
		RenderTargetResource,
		nullptr,
		GameTime,
		GEngine->GetDefaultWorldFeatureLevel(),
		FCanvas::ECanvasDrawMode::CDM_ImmediateDrawing
	);

	const FVector2D RenderTargetSize = FVector2D(RenderTarget->SizeX, RenderTarget->SizeY);

	FCanvasTileItem TileItem = FCanvasTileItem(FVector2D::ZeroVector, RenderTargetSize, RenderTarget->ClearColor);
	RenderCanvas.DrawItem(TileItem);

	const float MaxCharHeight = Font->GetMaxCharHeight();

	FCanvasTextItem TextItem = CreateTextItem(FVector2D::ZeroVector, FText::GetEmpty());

	const float LeftOffset = PaddingLeft * static_cast<float>(TextScale.X);
	const float RightOffset = PaddingRight * static_cast<float>(TextScale.X);

	TextItem.Position.X = LeftOffset;
	TextItem.Position.Y = PaddingTop * static_cast<float>(TextScale.Y);
	TextItem.Scale.X = static_cast<float>(TextScale.X);
	TextItem.Scale.Y = static_cast<float>(TextScale.Y);

	if (!FMath::IsNearlyEqual(LineHeight, 1.f))
	{
		TextItem.Position.Y += MaxCharHeight * (LineHeight - 1.f) * 0.5f * static_cast<float>(TextScale.Y);
	}

	const float ActualLineHeight = MaxCharHeight * LineHeight * static_cast<float>(TextScale.Y);

	for (const FDMTextLine& Line : Lines)
	{
		switch (Justify)
		{
			default:
			case ETextJustify::Left:
			case ETextJustify::InvariantLeft:
				TextItem.Position.X = LeftOffset;
				break;

			case ETextJustify::Center:
				TextItem.Position.X = (RenderTarget->SizeX - (Line.Width * TextItem.Scale.X)) * 0.5f;
				break;

			case ETextJustify::Right:
			case ETextJustify::InvariantRight:
				TextItem.Position.X = RenderTarget->SizeX - (Line.Width * TextItem.Scale.X) - RightOffset;
				break;
		}

		TextItem.Text = FText::FromString(Line.Line);
		RenderCanvas.DrawItem(TextItem);
		TextItem.Position.Y += ActualLineHeight;
	}

	RenderCanvas.Flush_GameThread();

	RenderTarget->UpdateResourceImmediate(false);
}

#undef LOCTEXT_NAMESPACE

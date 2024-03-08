// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialValues/DMMaterialValueText.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionText"

namespace UE::DynamicMaterial::Private
{
	const FIntPoint MinimumTextTextureSize = FIntPoint(5, 10);
	const FIntPoint MaxTexScale = FIntPoint(64, 64);
}

#if WITH_EDITOR
struct FDMMaterialValueText
{
	static const inline FName FontName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, Font);
	static const inline FName TextName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, Text);
	static const inline FName ColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, Color);
	static const inline FName BackgroundColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, BackgroundColor);
	static const inline FName JustifyName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, Justify);
	static const inline FName KerningName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, Kerning);
	static const inline FName LineHeightName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, LineHeight);
	static const inline FName PaddingLeftName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, PaddingLeft);
	static const inline FName PaddingRightName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, PaddingRight);
	static const inline FName PaddingTopName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, PaddingTop);
	static const inline FName PaddingBottomName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, PaddingBottom);
	static const inline FName TextScaleName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, TextScale);
	static const inline FName TextureSizeOverrideName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, TextureSizeOverride);
	static const inline FName bOutlineName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, bOutline);
	static const inline FName OutlineColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, OutlineColor);
	static const inline FName bShadowName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, bShadow);
	static const inline FName ShadowColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, ShadowColor);
	static const inline FName ShadowOffsetName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, ShadowOffset);
	static const inline FName bGlowName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, bGlow);
	static const inline FName GlowColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, GlowColor);
	static const inline FName GlowInnerRadiusName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, GlowInnerRadius);
	static const inline FName GlowOuterRadiusName = GET_MEMBER_NAME_CHECKED(UDMMaterialValueText, GlowOuterRadius);

	static const inline TSet<FName> PropertyNames = {
		FontName, TextName, ColorName, BackgroundColorName, JustifyName, KerningName, LineHeightName, PaddingLeftName, PaddingRightName,
		PaddingTopName, PaddingBottomName, TextScaleName, TextureSizeOverrideName, bOutlineName, OutlineColorName, bShadowName,
		ShadowColorName, ShadowOffsetName, bGlowName, GlowColorName, GlowInnerRadiusName, GlowOuterRadiusName
	};
};
#endif

UDMMaterialValueText::UDMMaterialValueText()
	: UDMMaterialValueTexture()
{
	Type = EDMValueType::VT_Text;

#if WITH_EDITOR
	EditableProperties.Add(FDMMaterialValueText::FontName);
	EditableProperties.Add(FDMMaterialValueText::TextName);
	EditableProperties.Add(FDMMaterialValueText::ColorName);
	EditableProperties.Add(FDMMaterialValueText::BackgroundColorName);
	EditableProperties.Add(FDMMaterialValueText::JustifyName);
	EditableProperties.Add(FDMMaterialValueText::KerningName);
	EditableProperties.Add(FDMMaterialValueText::LineHeightName);
	EditableProperties.Add(FDMMaterialValueText::PaddingLeftName);
	EditableProperties.Add(FDMMaterialValueText::PaddingRightName);
	EditableProperties.Add(FDMMaterialValueText::PaddingTopName);
	EditableProperties.Add(FDMMaterialValueText::PaddingBottomName);
	EditableProperties.Add(FDMMaterialValueText::TextScaleName);
	EditableProperties.Add(FDMMaterialValueText::TextureSizeOverrideName);
	EditableProperties.Add(FDMMaterialValueText::bOutlineName);
	EditableProperties.Add(FDMMaterialValueText::OutlineColorName);
	EditableProperties.Add(FDMMaterialValueText::bShadowName);
	EditableProperties.Add(FDMMaterialValueText::ShadowColorName);
	EditableProperties.Add(FDMMaterialValueText::ShadowOffsetName);
	EditableProperties.Add(FDMMaterialValueText::bGlowName);
	EditableProperties.Add(FDMMaterialValueText::GlowColorName);
	EditableProperties.Add(FDMMaterialValueText::GlowInnerRadiusName);
	EditableProperties.Add(FDMMaterialValueText::GlowOuterRadiusName);
#endif
}

void UDMMaterialValueText::PostLoad()
{
	Super::PostLoad();

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

UFont* UDMMaterialValueText::GetFont() const
{
	return Font;
}

void UDMMaterialValueText::SetFont(UFont* InFont)
{
	if (Font == InFont)
	{
		return;
	}

	Font = InFont;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FText& UDMMaterialValueText::GetText() const
{
	return Text;
}

void UDMMaterialValueText::SetText(const FText& InText)
{
	if (Text.EqualTo(InText, ETextComparisonLevel::Default))
	{
		return;
	}

	Text = InText;

	UpdateTextLines();
	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FLinearColor& UDMMaterialValueText::GetColor() const
{
	return Color;
}

void UDMMaterialValueText::SetColor(const FLinearColor& InColor)
{
	if (Color == InColor)
	{
		return;
	}

	Color = InColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialValueText::GetBackgroundColor() const
{
	return BackgroundColor;
}

void UDMMaterialValueText::SetBackgroundColor(const FLinearColor& InBackgroundColor)
{
	if (BackgroundColor == InBackgroundColor)
	{
		return;
	}

	BackgroundColor = InBackgroundColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

ETextJustify::Type UDMMaterialValueText::GetJustify() const
{
	return Justify;
}

void UDMMaterialValueText::SetJustify(ETextJustify::Type InJustify)
{
	if (Justify == InJustify)
	{
		return;
	}

	Justify = InJustify;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

float UDMMaterialValueText::GetKerning() const
{
	return Kerning;
}

void UDMMaterialValueText::SetKerning(float InKerning)
{
	if (Kerning == InKerning)
	{
		return;
	}

	Kerning = InKerning;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FLinearColor& UDMMaterialValueText::GetOutlineColor() const
{
	return OutlineColor;
}

void UDMMaterialValueText::SetOutlineColor(const FLinearColor& InOutlineColor)
{
	if (OutlineColor == InOutlineColor)
	{
		return;
	}

	OutlineColor = InOutlineColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

float UDMMaterialValueText::GetLineHeight() const
{
	return LineHeight;
}

void UDMMaterialValueText::SetLineHeight(float InLineHeight)
{
	if (LineHeight == InLineHeight)
	{
		return;
	}

	LineHeight = InLineHeight;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialValueText::GetPaddingLeft() const
{
	return PaddingLeft;
}

void UDMMaterialValueText::SetPaddingLeft(float InPaddingLeft)
{
	if (PaddingLeft == InPaddingLeft)
	{
		return;
	}

	PaddingLeft = InPaddingLeft;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialValueText::GetPaddingRight() const
{
	return PaddingRight;
}

void UDMMaterialValueText::SetPaddingRight(float InPaddingRight)
{
	if (PaddingRight == InPaddingRight)
	{
		return;
	}

	PaddingRight = InPaddingRight;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialValueText::GetPaddingTop() const
{
	return PaddingTop;
}

void UDMMaterialValueText::SetPaddingTop(float InPaddingTop)
{
	if (PaddingTop == InPaddingTop)
	{
		return;
	}

	PaddingTop = InPaddingTop;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialValueText::GetPaddingBottom() const
{
	return PaddingBottom;
}

void UDMMaterialValueText::SetPaddingBottom(float InPaddingBottom)
{
	if (PaddingBottom == InPaddingBottom)
	{
		return;
	}

	PaddingBottom = InPaddingBottom;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FIntPoint& UDMMaterialValueText::GetTextScale() const
{
	return TextScale;
}

void UDMMaterialValueText::SetTextScale(const FIntPoint& InTextScale)
{
	FIntPoint NewTextScale = InTextScale;
	NewTextScale.X = FMath::Clamp(InTextScale.X, 1, UE::DynamicMaterial::Private::MaxTexScale.X);
	NewTextScale.Y = FMath::Clamp(InTextScale.Y, 1, UE::DynamicMaterial::Private::MaxTexScale.Y);

	if (TextScale == NewTextScale)
	{
		return;
	}

	TextScale = NewTextScale;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FIntPoint& UDMMaterialValueText::GetTextureSizeOverride() const
{
	return TextureSizeOverride;
}

void UDMMaterialValueText::SetTextureSizeOverride(const FIntPoint& InTextureSizeOverride)
{
	if (TextureSizeOverride == InTextureSizeOverride)
	{
		return;
	}

	TextureSizeOverride = InTextureSizeOverride;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

bool UDMMaterialValueText::GetHasOutline() const
{
	return bOutline;
}

void UDMMaterialValueText::SetHasOutline(bool bInHasOutline)
{
	if (bOutline == bInHasOutline)
	{
		return;
	}

	bOutline = bInHasOutline;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

bool UDMMaterialValueText::GetHasShadow() const
{
	return bShadow;
}

void UDMMaterialValueText::SetHasShadow(bool bInHasShadow)
{
	if (bShadow == bInHasShadow)
	{
		return;
	}

	bShadow = bInHasShadow;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialValueText::GetShadowColor() const
{
	return ShadowColor;
}

void UDMMaterialValueText::SetShadowColor(const FLinearColor& InShadowColor)
{
	if (ShadowColor == InShadowColor)
	{
		return;
	}

	ShadowColor = InShadowColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialValueText::GetShadowOffset() const
{
	return ShadowOffset;
}

void UDMMaterialValueText::SetShadowOffset(const FVector2D& InShadowOffset)
{
	if (ShadowOffset == InShadowOffset)
	{
		return;
	}

	ShadowOffset = InShadowOffset;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

bool UDMMaterialValueText::GetHasGlow() const
{
	return bGlow;
}

void UDMMaterialValueText::SetHasGlow(bool bInHasGlow)
{
	if (bGlow == bInHasGlow)
	{
		return;
	}

	bGlow = bInHasGlow;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialValueText::GetGlowColor() const
{
	return GlowColor;
}

void UDMMaterialValueText::SetGlowColor(const FLinearColor& InGlowColor)
{
	if (GlowColor == InGlowColor)
	{
		return;
	}

	GlowColor = InGlowColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialValueText::GetGlowInnerRadius() const
{
	return GlowInnerRadius;
}

void UDMMaterialValueText::SetGlowInnerRadius(const FVector2D& InGlowInnerRadius)
{
	if (GlowInnerRadius == InGlowInnerRadius)
	{
		return;
	}

	GlowInnerRadius = InGlowInnerRadius;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialValueText::GetGlowOuterRadius() const
{
	return GlowOuterRadius;
}

void UDMMaterialValueText::SetGlowOuterRadius(const FVector2D& InGlowOuterRadius)
{
	if (GlowOuterRadius == InGlowOuterRadius)
	{
		return;
	}

	GlowOuterRadius = InGlowOuterRadius;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

#if WITH_EDITOR
void UDMMaterialValueText::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == FDMMaterialValueText::TextName)
	{
		UpdateTextLines();
		RequestUpdateTextTexture(/* Needs Resize */ true);
	}
	else if (PropertyName == FDMMaterialValueText::FontName
		|| PropertyName == FDMMaterialValueText::KerningName
		|| PropertyName == FDMMaterialValueText::LineHeightName
		|| PropertyName == FDMMaterialValueText::PaddingLeftName
		|| PropertyName == FDMMaterialValueText::PaddingRightName
		|| PropertyName == FDMMaterialValueText::PaddingTopName
		|| PropertyName == FDMMaterialValueText::PaddingBottomName
		|| PropertyName == FDMMaterialValueText::TextScaleName
		|| PropertyName == FDMMaterialValueText::TextureSizeOverrideName)
	{
		TextScale.X = FMath::Clamp(TextScale.X, 1, UE::DynamicMaterial::Private::MaxTexScale.X);
		TextScale.Y = FMath::Clamp(TextScale.Y, 1, UE::DynamicMaterial::Private::MaxTexScale.Y);
		RequestUpdateTextTexture(/* Needs Resize */ true);
	}
	else if (PropertyName == FDMMaterialValueText::bGlowName
		|| PropertyName == FDMMaterialValueText::bShadowName
		|| PropertyName == FDMMaterialValueText::bOutlineName)
	{
		// Cause details panel refresh.
		Update(EDMUpdateType::Structure);
		RequestUpdateTextTexture(/* Needs Resize */ false);
	}
	else if (FDMMaterialValueText::PropertyNames.Contains(PropertyName))
	{
		RequestUpdateTextTexture(/* Needs Resize */ false);
	}
}
#endif

UTextureRenderTarget2D* UDMMaterialValueText::CreateRenderTarget(UObject* InOuter, const FIntPoint& InSize, 
	const FLinearColor& InClearColor)
{
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(InOuter, NAME_None, 
		EObjectFlags::RF_Transactional | EObjectFlags::RF_DuplicateTransient | EObjectFlags::RF_TextExportTransient
	);

	check(RenderTarget);
	RenderTarget->RenderTargetFormat = RTF_RGBA16f;
	RenderTarget->ClearColor = InClearColor;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->bCanCreateUAV = false;
	RenderTarget->InitAutoFormat(InSize.X, InSize.Y);
	RenderTarget->UpdateResourceImmediate(true);

	return RenderTarget;
}

void UDMMaterialValueText::CalculateLineSizes()
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

FIntPoint UDMMaterialValueText::GetRequiredTextureSize() const
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

void UDMMaterialValueText::UpdateTextLines()
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
}

void UDMMaterialValueText::RequestUpdateTextTexture(bool bInNeedsResize)
{
	bNeedsResize = bInNeedsResize;

	if (EndOfFrameDelegateHandle.IsValid())
	{
		return;
	}

	EndOfFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UDMMaterialValueText::UpdateTextTexture);
}

void UDMMaterialValueText::UpdateTextTexture()
{
	if (EndOfFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndOfFrameDelegateHandle);
		EndOfFrameDelegateHandle.Reset();
	}

	if (!IsValid(Font) || Lines.IsEmpty())
	{
		return;
	}

	if (bNeedsResize)
	{
		CalculateLineSizes();
	}
		
	const bool bValidRenderTarget = IsValid(RenderTarget.Get());

	if (!bValidRenderTarget)
	{
		bNeedsResize = true;
	}
	else if (bNeedsResize && TextureSizeOverride.X > 0 && TextureSizeOverride.X == RenderTarget->SizeX
		&& TextureSizeOverride.Y > 0 && TextureSizeOverride.Y == RenderTarget->SizeY)
	{
		bNeedsResize = false;
	}

	if (bNeedsResize)
	{
		FIntPoint RequiredSize = GetRequiredTextureSize();
		RequiredSize.X = FMath::Max(RequiredSize.X, UE::DynamicMaterial::Private::MinimumTextTextureSize.X);
		RequiredSize.Y = FMath::Max(RequiredSize.Y, UE::DynamicMaterial::Private::MinimumTextTextureSize.Y);

		if (bValidRenderTarget && RequiredSize.X == RenderTarget->SizeX && RequiredSize.X == RenderTarget->SizeY)
		{
			bNeedsResize = false;
		}

		if (bNeedsResize)
		{
			RenderTarget = CreateRenderTarget(this, RequiredSize, FLinearColor::Black);
			SetValue(RenderTarget);
			bNeedsResize = false;
		}
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

	FCanvasTileItem TileItem = FCanvasTileItem(FVector2D::ZeroVector, RenderTargetSize, BackgroundColor);
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

FCanvasTextItem UDMMaterialValueText::CreateTextItem(const FVector2D& InPosition, const FText& InText) const
{
	FCanvasTextItem TextItem(InPosition, InText, Font, Color);

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

#undef LOCTEXT_NAMESPACE

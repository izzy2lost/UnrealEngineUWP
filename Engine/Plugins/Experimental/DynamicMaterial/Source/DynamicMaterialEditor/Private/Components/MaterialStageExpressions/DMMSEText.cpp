// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEText.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/DMMaterialStage.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DynamicMaterialEditorModule.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionText"

namespace UE::DynamicMaterialEditor::Private
{
	const FIntPoint MinimumTextTextureSize = FIntPoint(5, 10);
	const FIntPoint MaxTexScale = FIntPoint(64, 64);
}

struct FDMMaterialStageExpressionText
{
	static const inline FName FontName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, Font);
	static const inline FName TextName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, Text);
	static const inline FName ColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, Color);
	static const inline FName BackgroundColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, BackgroundColor);
	static const inline FName JustifyName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, Justify);
	static const inline FName KerningName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, Kerning);
	static const inline FName LineHeightName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, LineHeight);
	static const inline FName PaddingLeftName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, PaddingLeft);
	static const inline FName PaddingRightName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, PaddingRight);
	static const inline FName PaddingTopName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, PaddingTop);
	static const inline FName PaddingBottomName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, PaddingBottom);
	static const inline FName TextScaleName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, TextScale);
	static const inline FName TextureSizeOverrideName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, TextureSizeOverride);
	static const inline FName bOutlineName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, bOutline);
	static const inline FName OutlineColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, OutlineColor);
	static const inline FName bShadowName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, bShadow);
	static const inline FName ShadowColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, ShadowColor);
	static const inline FName ShadowOffsetName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, ShadowOffset);
	static const inline FName bGlowName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, bGlow);
	static const inline FName GlowColorName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, GlowColor);
	static const inline FName GlowInnerRadiusName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, GlowInnerRadius);
	static const inline FName GlowOuterRadiusName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionText, GlowOuterRadius);

	static const inline TSet<FName> PropertyNames = {
		FontName, TextName, ColorName, BackgroundColorName, JustifyName, KerningName, LineHeightName, PaddingLeftName, PaddingRightName,
		PaddingTopName, PaddingBottomName, TextScaleName, TextureSizeOverrideName, bOutlineName, OutlineColorName, bShadowName,
		ShadowColorName, ShadowOffsetName, bGlowName, GlowColorName, GlowInnerRadiusName, GlowOuterRadiusName
	};
};

UDMMaterialStageExpressionText::UDMMaterialStageExpressionText()
	: UDMMaterialStageExpressionTextureSampleBase(
		LOCTEXT("Text", "Text"),
		UMaterialExpressionTextureSample::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Other);

	EditableProperties.Add(FDMMaterialStageExpressionText::FontName);
	EditableProperties.Add(FDMMaterialStageExpressionText::TextName);
	EditableProperties.Add(FDMMaterialStageExpressionText::ColorName);
	EditableProperties.Add(FDMMaterialStageExpressionText::BackgroundColorName);
	EditableProperties.Add(FDMMaterialStageExpressionText::JustifyName);
	EditableProperties.Add(FDMMaterialStageExpressionText::KerningName);
	EditableProperties.Add(FDMMaterialStageExpressionText::LineHeightName);
	EditableProperties.Add(FDMMaterialStageExpressionText::PaddingLeftName);
	EditableProperties.Add(FDMMaterialStageExpressionText::PaddingRightName);
	EditableProperties.Add(FDMMaterialStageExpressionText::PaddingTopName);
	EditableProperties.Add(FDMMaterialStageExpressionText::PaddingBottomName);
	EditableProperties.Add(FDMMaterialStageExpressionText::TextScaleName);
	EditableProperties.Add(FDMMaterialStageExpressionText::TextureSizeOverrideName);
	EditableProperties.Add(FDMMaterialStageExpressionText::bOutlineName);
	EditableProperties.Add(FDMMaterialStageExpressionText::OutlineColorName);
	EditableProperties.Add(FDMMaterialStageExpressionText::bShadowName);
	EditableProperties.Add(FDMMaterialStageExpressionText::ShadowColorName);
	EditableProperties.Add(FDMMaterialStageExpressionText::ShadowOffsetName);
	EditableProperties.Add(FDMMaterialStageExpressionText::bGlowName);
	EditableProperties.Add(FDMMaterialStageExpressionText::GlowColorName);
	EditableProperties.Add(FDMMaterialStageExpressionText::GlowInnerRadiusName);
	EditableProperties.Add(FDMMaterialStageExpressionText::GlowOuterRadiusName);
}

bool UDMMaterialStageExpressionText::IsInputVisible(int32 InInputIndex) const
{
	return InInputIndex != 0;
}

void UDMMaterialStageExpressionText::AddDefaultInput(int32 InInputIndex) const
{
	if (InInputIndex == 0)
	{
		UDMMaterialStage* Stage = GetStage();
		check(Stage);

		UDMMaterialStageInputValue* NewInput = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			Stage,
			InInputIndex,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);
		check(NewInput);

		const_cast<UDMMaterialStageExpressionText*>(this)->TextureValue = Cast<UDMMaterialValueTexture>(NewInput->GetValue());
		check(TextureValue);

		FIntPoint RequiredSize = GetRequiredTextureSize();
		RequiredSize.X = FMath::Max(5, RequiredSize.X);
		RequiredSize.Y = FMath::Max(10, RequiredSize.Y);
		const_cast<UDMMaterialStageExpressionText*>(this)->RenderTarget = CreateRenderTarget(TextureValue, RequiredSize, FLinearColor::Black);

		TextureValue->SetValue(RenderTarget);
		return;
	}

	Super::AddDefaultInput(InInputIndex);
}

void UDMMaterialStageExpressionText::PostLoad()
{
	Super::PostLoad();

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

UFont* UDMMaterialStageExpressionText::GetFont() const
{
	return Font;
}

void UDMMaterialStageExpressionText::SetFont(UFont* InFont)
{
	if (Font == InFont)
	{
		return;
	}

	Font = InFont;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FText& UDMMaterialStageExpressionText::GetText() const
{
	return Text;
}

void UDMMaterialStageExpressionText::SetText(const FText& InText)
{
	if (Text.EqualTo(InText, ETextComparisonLevel::Default))
	{
		return;
	}

	Text = InText;

	UpdateTextLines();
	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FLinearColor& UDMMaterialStageExpressionText::GetColor() const
{
	return Color;
}

void UDMMaterialStageExpressionText::SetColor(const FLinearColor& InColor)
{
	if (Color == InColor)
	{
		return;
	}

	Color = InColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialStageExpressionText::GetBackgroundColor() const
{
	return BackgroundColor;
}

void UDMMaterialStageExpressionText::SetBackgroundColor(const FLinearColor& InBackgroundColor)
{
	if (BackgroundColor == InBackgroundColor)
	{
		return;
	}

	BackgroundColor = InBackgroundColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

ETextJustify::Type UDMMaterialStageExpressionText::GetJustify() const
{
	return Justify;
}

void UDMMaterialStageExpressionText::SetJustify(ETextJustify::Type InJustify)
{
	if (Justify == InJustify)
	{
		return;
	}

	Justify = InJustify;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

float UDMMaterialStageExpressionText::GetKerning() const
{
	return Kerning;
}

void UDMMaterialStageExpressionText::SetKerning(float InKerning)
{
	if (Kerning == InKerning)
	{
		return;
	}

	Kerning = InKerning;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FLinearColor& UDMMaterialStageExpressionText::GetOutlineColor() const
{
	return OutlineColor;
}

void UDMMaterialStageExpressionText::SetOutlineColor(const FLinearColor& InOutlineColor)
{
	if (OutlineColor == InOutlineColor)
	{
		return;
	}

	OutlineColor = InOutlineColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

float UDMMaterialStageExpressionText::GetLineHeight() const
{
	return LineHeight;
}

void UDMMaterialStageExpressionText::SetLineHeight(float InLineHeight)
{
	if (LineHeight == InLineHeight)
	{
		return;
	}

	LineHeight = InLineHeight;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialStageExpressionText::GetPaddingLeft() const
{
	return PaddingLeft;
}

void UDMMaterialStageExpressionText::SetPaddingLeft(float InPaddingLeft)
{
	if (PaddingLeft == InPaddingLeft)
	{
		return;
	}

	PaddingLeft = InPaddingLeft;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialStageExpressionText::GetPaddingRight() const
{
	return PaddingRight;
}

void UDMMaterialStageExpressionText::SetPaddingRight(float InPaddingRight)
{
	if (PaddingRight == InPaddingRight)
	{
		return;
	}

	PaddingRight = InPaddingRight;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialStageExpressionText::GetPaddingTop() const
{
	return PaddingTop;
}

void UDMMaterialStageExpressionText::SetPaddingTop(float InPaddingTop)
{
	if (PaddingTop == InPaddingTop)
	{
		return;
	}

	PaddingTop = InPaddingTop;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

float UDMMaterialStageExpressionText::GetPaddingBottom() const
{
	return PaddingBottom;
}

void UDMMaterialStageExpressionText::SetPaddingBottom(float InPaddingBottom)
{
	if (PaddingBottom == InPaddingBottom)
	{
		return;
	}

	PaddingBottom = InPaddingBottom;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FIntPoint& UDMMaterialStageExpressionText::GetTextScale() const
{
	return TextScale;
}

void UDMMaterialStageExpressionText::SetTextScale(const FIntPoint& InTextScale)
{
	FIntPoint NewTextScale = InTextScale;
	NewTextScale.X = FMath::Clamp(InTextScale.X, 1, UE::DynamicMaterialEditor::Private::MaxTexScale.X);
	NewTextScale.Y = FMath::Clamp(InTextScale.Y, 1, UE::DynamicMaterialEditor::Private::MaxTexScale.Y);

	if (TextScale == NewTextScale)
	{
		return;
	}

	TextScale = NewTextScale;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

const FIntPoint& UDMMaterialStageExpressionText::GetTextureSizeOverride() const
{
	return TextureSizeOverride;
}

void UDMMaterialStageExpressionText::SetTextureSizeOverride(const FIntPoint& InTextureSizeOverride)
{
	if (TextureSizeOverride == InTextureSizeOverride)
	{
		return;
	}

	TextureSizeOverride = InTextureSizeOverride;

	RequestUpdateTextTexture(/* Needs Resize */ true);
}

bool UDMMaterialStageExpressionText::GetHasOutline() const
{
	return bOutline;
}

void UDMMaterialStageExpressionText::SetHasOutline(bool bInHasOutline)
{
	if (bOutline == bInHasOutline)
	{
		return;
	}

	bOutline = bInHasOutline;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

bool UDMMaterialStageExpressionText::GetHasShadow() const
{
	return bShadow;
}

void UDMMaterialStageExpressionText::SetHasShadow(bool bInHasShadow)
{
	if (bShadow == bInHasShadow)
	{
		return;
	}

	bShadow = bInHasShadow;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialStageExpressionText::GetShadowColor() const
{
	return ShadowColor;
}

void UDMMaterialStageExpressionText::SetShadowColor(const FLinearColor& InShadowColor)
{
	if (ShadowColor == InShadowColor)
	{
		return;
	}

	ShadowColor = InShadowColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialStageExpressionText::GetShadowOffset() const
{
	return ShadowOffset;
}

void UDMMaterialStageExpressionText::SetShadowOffset(const FVector2D& InShadowOffset)
{
	if (ShadowOffset == InShadowOffset)
	{
		return;
	}

	ShadowOffset = InShadowOffset;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

bool UDMMaterialStageExpressionText::GetHasGlow() const
{
	return bGlow;
}

void UDMMaterialStageExpressionText::SetHasGlow(bool bInHasGlow)
{
	if (bGlow == bInHasGlow)
	{
		return;
	}

	bGlow = bInHasGlow;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FLinearColor& UDMMaterialStageExpressionText::GetGlowColor() const
{
	return GlowColor;
}

void UDMMaterialStageExpressionText::SetGlowColor(const FLinearColor& InGlowColor)
{
	if (GlowColor == InGlowColor)
	{
		return;
	}

	GlowColor = InGlowColor;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialStageExpressionText::GetGlowInnerRadius() const
{
	return GlowInnerRadius;
}

void UDMMaterialStageExpressionText::SetGlowInnerRadius(const FVector2D& InGlowInnerRadius)
{
	if (GlowInnerRadius == InGlowInnerRadius)
	{
		return;
	}

	GlowInnerRadius = InGlowInnerRadius;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

const FVector2D& UDMMaterialStageExpressionText::GetGlowOuterRadius() const
{
	return GlowOuterRadius;
}

void UDMMaterialStageExpressionText::SetGlowOuterRadius(const FVector2D& InGlowOuterRadius)
{
	if (GlowOuterRadius == InGlowOuterRadius)
	{
		return;
	}

	GlowOuterRadius = InGlowOuterRadius;

	RequestUpdateTextTexture(/* Needs Resize */ false);
}

void UDMMaterialStageExpressionText::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == FDMMaterialStageExpressionText::TextName)
	{
		UpdateTextLines();
		RequestUpdateTextTexture(/* Needs Resize */ true);
	}
	else if (PropertyName == FDMMaterialStageExpressionText::FontName
		|| PropertyName == FDMMaterialStageExpressionText::KerningName
		|| PropertyName == FDMMaterialStageExpressionText::LineHeightName
		|| PropertyName == FDMMaterialStageExpressionText::PaddingLeftName
		|| PropertyName == FDMMaterialStageExpressionText::PaddingRightName
		|| PropertyName == FDMMaterialStageExpressionText::PaddingTopName
		|| PropertyName == FDMMaterialStageExpressionText::PaddingBottomName
		|| PropertyName == FDMMaterialStageExpressionText::TextScaleName
		|| PropertyName == FDMMaterialStageExpressionText::TextureSizeOverrideName)
	{
		TextScale.X = FMath::Clamp(TextScale.X, 1, UE::DynamicMaterialEditor::Private::MaxTexScale.X);
		TextScale.Y = FMath::Clamp(TextScale.Y, 1, UE::DynamicMaterialEditor::Private::MaxTexScale.Y);
		RequestUpdateTextTexture(/* Needs Resize */ true);
	}
	else if (PropertyName == FDMMaterialStageExpressionText::bGlowName
		|| PropertyName == FDMMaterialStageExpressionText::bShadowName
		|| PropertyName == FDMMaterialStageExpressionText::bOutlineName)
	{
		// Cause details panel refresh.
		Update(EDMUpdateType::Structure);
		RequestUpdateTextTexture(/* Needs Resize */ false);
	}
	else if (FDMMaterialStageExpressionText::PropertyNames.Contains(PropertyName))
	{
		RequestUpdateTextTexture(/* Needs Resize */ false);
	}
}

UTextureRenderTarget2D* UDMMaterialStageExpressionText::CreateRenderTarget(UObject* InOuter, const FIntPoint& InSize, 
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

void UDMMaterialStageExpressionText::CalculateLineSizes()
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

FIntPoint UDMMaterialStageExpressionText::GetRequiredTextureSize() const
{
	if (TextureSizeOverride.X > 0 && TextureSizeOverride.Y > 0)
	{
		return TextureSizeOverride;
	}

	if (!IsValid(Font) || Lines.IsEmpty())
	{
		return UE::DynamicMaterialEditor::Private::MinimumTextTextureSize;
	}

	const float MaxCharHeight = Font->GetMaxCharHeight();

	if (MaxCharHeight <= 0)
	{
		return UE::DynamicMaterialEditor::Private::MinimumTextTextureSize;
	}

	FVector2f Size = FVector2f::ZeroVector;

	for (const FDMTextLine& Line : Lines)
	{
		Size.X = FMath::Max(Size.X, Line.Width);
		Size.Y += MaxCharHeight * LineHeight;
	}

	Size.X += PaddingLeft + PaddingRight;
	Size.Y += PaddingTop + PaddingBottom;

	Size.X = FMath::Max(Size.X, UE::DynamicMaterialEditor::Private::MinimumTextTextureSize.X);
	Size.Y = FMath::Max(Size.Y, UE::DynamicMaterialEditor::Private::MinimumTextTextureSize.Y);

	FIntPoint RequiredSize = FIntPoint::ZeroValue;
	RequiredSize.X = FMath::CeilToInt(Size.X * TextScale.X);
	RequiredSize.Y = FMath::CeilToInt(Size.Y * TextScale.Y);

	return RequiredSize;
}

void UDMMaterialStageExpressionText::UpdateTextLines()
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

void UDMMaterialStageExpressionText::RequestUpdateTextTexture(bool bInNeedsResize)
{
	bNeedsResize = bInNeedsResize;

	if (EndOfFrameDelegateHandle.IsValid())
	{
		return;
	}

	EndOfFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UDMMaterialStageExpressionText::UpdateTextTexture);
}

void UDMMaterialStageExpressionText::UpdateTextTexture()
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
		RequiredSize.X = FMath::Max(RequiredSize.X, UE::DynamicMaterialEditor::Private::MinimumTextTextureSize.X);
		RequiredSize.Y = FMath::Max(RequiredSize.Y, UE::DynamicMaterialEditor::Private::MinimumTextTextureSize.Y);

		if (bValidRenderTarget && RequiredSize.X == RenderTarget->SizeX && RequiredSize.X == RenderTarget->SizeY)
		{
			bNeedsResize = false;
		}

		if (bNeedsResize)
		{
			RenderTarget = CreateRenderTarget(TextureValue, RequiredSize, FLinearColor::Black);
			TextureValue->SetValue(RenderTarget);
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

FCanvasTextItem UDMMaterialStageExpressionText::CreateTextItem(const FVector2D& InPosition, const FText& InText) const
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

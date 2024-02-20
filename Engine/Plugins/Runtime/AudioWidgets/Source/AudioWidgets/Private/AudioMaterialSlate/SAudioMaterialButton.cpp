// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/SAudioMaterialButton.h"
#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "SlateOptMacros.h"
#include "Components/Widget.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAudioMaterialButton::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	AudioMaterialButtonStyle = InArgs._AudioMaterialButtonStyle;
	bIsPressedAttribute = InArgs._bIsPressedAttribute;
	OnIsPressedStateChanged = InArgs._OnBooleanValueChanged;

	ApplyNewMaterial();

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioMaterialButton::SetPressedState(bool InPressedState)
{
	CommitNewState(InPressedState);
}

void SAudioMaterialButton::ApplyNewMaterial()
{
	if (AudioMaterialButtonStyle)
	{
		AudioMaterialButtonStyle->CreateDynamicMaterial(Owner.Get());
	}
}

int32 SAudioMaterialButton::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (AudioMaterialButtonStyle)
	{
		UMaterialInstanceDynamic* DynamicMaterial = AudioMaterialButtonStyle->GetDynamicMaterial();

		if (IsValid(DynamicMaterial))
		{
			DynamicMaterial->SetVectorParameterValue(FName("MainColor"), AudioMaterialButtonStyle->ButtonMainColor);
			DynamicMaterial->SetVectorParameterValue(FName("ShadowColor"), AudioMaterialButtonStyle->ButtonShadowColor);
			DynamicMaterial->SetVectorParameterValue(FName("SmoothBevelColor"), AudioMaterialButtonStyle->ButtonAccentColor);
			DynamicMaterial->SetVectorParameterValue(FName("Color_1"), AudioMaterialButtonStyle->ButtonPressedMainColor);
			DynamicMaterial->SetVectorParameterValue(FName("Color_2"), AudioMaterialButtonStyle->ButtonPressedShadowColor);
			DynamicMaterial->SetVectorParameterValue(FName("LedColor"), AudioMaterialButtonStyle->ButtonPressedOutlineColor);
			DynamicMaterial->SetScalarParameterValue(FName("Click"), bIsPressedAttribute.Get());

			DynamicMaterial->SetScalarParameterValue(FName("LocalWidth"), AllottedGeometry.GetLocalSize().X);
			DynamicMaterial->SetScalarParameterValue(FName("LocalHeigth"), AllottedGeometry.GetLocalSize().Y);			
		}

		const bool bEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FLinearColor FinalColorAndOpacity(InWidgetStyle.GetColorAndOpacityTint());

		FSlateBrush Brush;
		Brush.SetResourceObject(DynamicMaterial);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, AllottedGeometry.ToPaintGeometry(),&Brush, DrawEffects, FinalColorAndOpacity);
	}

	return LayerId;
}

FVector2D SAudioMaterialButton::ComputeDesiredSize(float) const
{
	if (AudioMaterialButtonStyle)
	{
		return FVector2D(AudioMaterialButtonStyle->DesiredSize);
	}

	return FVector2D::ZeroVector;
}

FReply SAudioMaterialButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton))
	{
		CommitNewState(!bIsPressedAttribute.Get());

		FReply Reply = FReply::Handled().CaptureMouse(SharedThis(this));
		return Reply;
	}

	return FReply::Unhandled();
}

FReply SAudioMaterialButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && this->HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SAudioMaterialButton::CommitNewState(bool InPressedState)
{
	bIsPressedAttribute.Set(InPressedState);
	OnIsPressedStateChanged.ExecuteIfBound(InPressedState);
}

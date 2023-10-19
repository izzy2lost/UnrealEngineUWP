// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Misc/DefaultValueHelper.h"
#include "ScopedTransaction.h"
#include "SGraphPin.h"
#include "Editor.h"
#include "NumericPropertyParams.h"

template <typename NumericType>
class SGraphPinNumSlider : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinNumSlider) 
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FProperty* InProperty)
	{
		PinProperty = InProperty;
		SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);

	}

protected:
	// SGraphPin interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override
	{
		const typename TNumericPropertyParams<NumericType>::FMetaDataGetter MetaDataGetter = TNumericPropertyParams<NumericType>::FMetaDataGetter::CreateLambda([&](const FName& Key)
			{
				return (PinProperty) ? PinProperty->GetMetaData(Key) : FString();
			});

		TNumericPropertyParams<NumericType> NumericPropertyParams(PinProperty, PinProperty ? MetaDataGetter : nullptr);
		
		const bool bAllowSpin = !(PinProperty && PinProperty->GetBoolMetaData("NoSpinbox"));

		return SNew(SBox)
			.MinDesiredWidth(18)
			.MaxDesiredWidth(400)
			[
				SNew(SNumericEntryBox<NumericType>)
				.MinDesiredValueWidth(100.0f)
				.EditableTextBoxStyle(FAppStyle::Get(), "Graph.EditableTextBox")
				.BorderForegroundColor(FSlateColor::UseForeground())
				.Visibility(this, &SGraphPinNumSlider::GetDefaultValueVisibility)
				.IsEnabled(this, &SGraphPinNumSlider::GetDefaultValueIsEditable)
				.Value(this, &SGraphPinNumSlider::GetNumericValue)
				.MinValue(NumericPropertyParams.MinValue)
				.MaxValue(NumericPropertyParams.MaxValue)
				.MinSliderValue(NumericPropertyParams.MinSliderValue)
				.MaxSliderValue(NumericPropertyParams.MaxSliderValue)
				.SliderExponent(NumericPropertyParams.SliderExponent)
				.Delta(NumericPropertyParams.Delta)
				.LinearDeltaSensitivity(NumericPropertyParams.GetLinearDeltaSensitivityAttribute())
				.AllowWheel(bAllowSpin)
				.WheelStep(NumericPropertyParams.WheelStep)
				.AllowSpin(bAllowSpin)
				.OnValueCommitted(this, &SGraphPinNumSlider::OnValueCommitted)
				.OnValueChanged(this, &SGraphPinNumSlider::OnValueChanged)
				.OnBeginSliderMovement(this, &SGraphPinNumSlider::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SGraphPinNumSlider::OnEndSliderMovement)
			];
	}

	void OnValueChanged( NumericType NewValue )
	{
		SliderValue = NewValue;
	}

	void OnValueCommitted( NumericType NewValue, ETextCommit::Type CommitInfo )
	{
		if (GraphPinObj->IsPendingKill())
		{
			return;
		}

		if (LastSliderCommittedValue != NewValue)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangeNumberPinValue", "Change Number Pin Value"));
			GraphPinObj->Modify();
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, *LexToSanitizedString(NewValue));
			LastSliderCommittedValue = NewValue;
		}
	}

	/**
	* Called when the slider begins to move.
	*/
	void OnBeginSliderMovement()
	{
		SliderValue = LastSliderCommittedValue = GetNumericValue().GetValue();
		bIsUsingSlider = true;
	}

	/**
	* Called when the slider stops moving.
	*/
	void OnEndSliderMovement( NumericType NewValue )
	{
		bIsUsingSlider = false;
	}
	
	TOptional<NumericType> GetNumericValue() const
	{
		NumericType Num = NumericType();
		LexFromString(Num, *GraphPinObj->GetDefaultAsString());
		return bIsUsingSlider ? SliderValue : Num;
	}

private:
	FProperty* PinProperty;
	NumericType LastSliderCommittedValue;
	NumericType SliderValue;
	bool bIsUsingSlider = false;
};

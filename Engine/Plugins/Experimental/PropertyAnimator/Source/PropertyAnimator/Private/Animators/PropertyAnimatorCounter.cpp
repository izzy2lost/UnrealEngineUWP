// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorCounter.h"

#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "Settings/PropertyAnimatorSettings.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"

void FPropertyAnimatorCounterFormat::EnsureCharactersLength()
{
	if (DecimalCharacter.Len() > 1)
	{
		DecimalCharacter.RemoveAt(1, DecimalCharacter.Len() - 1);
	}

	if (PaddingCharacter.Len() > 1)
	{
		PaddingCharacter.RemoveAt(1, PaddingCharacter.Len() - 1);
	}

	if (GroupingCharacter.Len() > 1)
	{
		GroupingCharacter.RemoveAt(1, GroupingCharacter.Len() - 1);
	}
}

FString FPropertyAnimatorCounterFormat::FormatNumber(double InNumber) const
{
	const bool bPositive = InNumber >= 0;
	InNumber = FMath::Abs(InNumber);

	if (RoundingMode != EPropertyAnimatorCounterRoundingMode::None)
	{
		switch (RoundingMode)
		{
		case EPropertyAnimatorCounterRoundingMode::Round:
			InNumber = FMath::RoundToDouble(InNumber);
		break;
		case EPropertyAnimatorCounterRoundingMode::Floor:
			InNumber = FMath::FloorToDouble(InNumber);
		break;
		case EPropertyAnimatorCounterRoundingMode::Ceil:
			InNumber = FMath::CeilToDouble(InNumber);
		break;
		default:
		break;
		}
	}

	const int32 IntegerCount = MinIntegerCount;
	const int32 DecimalCount = MaxDecimalCount;

	FString IntegerPart = LexToString(FMath::TruncToInt(InNumber));
	FString DecimalPart;

	if (IntegerPart.Len() > IntegerCount && bTruncate)
	{
		IntegerPart.RemoveAt(0, IntegerPart.Len() - IntegerCount);
	}

	if (DecimalCount > 0)
	{
		DecimalPart = FString::SanitizeFloat(FMath::Fractional(InNumber));

		DecimalPart.RemoveFromStart(TEXT("0."));

		if (DecimalPart.Len() < DecimalCount)
		{
			DecimalPart.InsertAt(DecimalPart.Len() - 1, FString::ChrN(DecimalCount - DecimalPart.Len(), TEXT('0')));
		}

		if (DecimalPart.Len() > DecimalCount)
		{
			DecimalPart.RemoveAt(DecimalCount, DecimalPart.Len());
		}

		if (!DecimalCharacter.IsEmpty())
		{
			DecimalPart.InsertAt(0, DecimalCharacter[0]);
		}
	}

	if (!PaddingCharacter.IsEmpty() && IntegerPart.Len() < IntegerCount)
	{
		IntegerPart.InsertAt(0, FString::ChrN(IntegerCount - IntegerPart.Len(), PaddingCharacter[0]));
	}

	if (!GroupingCharacter.IsEmpty())
	{
		int32 ThousandsIndex = IntegerPart.Len() - 1;
		int32 ThousandsCount = 0;
		while (ThousandsIndex > 0)
		{
			if (IntegerPart[ThousandsIndex] != TEXT('.'))
			{
				ThousandsCount++;

				if (FMath::Modulo(ThousandsCount, GroupingSize) == 0 && ThousandsIndex != 0)
				{
					IntegerPart.InsertAt(ThousandsIndex, GroupingCharacter[0]);
				}
			}

			ThousandsIndex--;
		}
	}

	FString NumberString = IntegerPart + DecimalPart;

	if (bUseSign)
	{
		NumberString.InsertAt(0, bPositive ? TEXT('+') : TEXT('-'));
	}

	return NumberString;
}

UPropertyAnimatorCounter::UPropertyAnimatorCounter()
{
	if (!IsTemplate())
	{
		const TArray<FName> AvailableNames = GetAvailableFormatNames();

		if (!AvailableNames.IsEmpty())
		{
			PresetFormatName = AvailableNames[0];
		}
	}
}

void UPropertyAnimatorCounter::SetDisplayPattern(const FText& InPattern)
{
	DisplayPattern = InPattern;
}

void UPropertyAnimatorCounter::SetUseCustomFormat(bool bInUseCustom)
{
	if (bUseCustomFormat == bInUseCustom)
	{
		return;
	}

	bUseCustomFormat = bInUseCustom;
	OnUseCustomFormatChanged();
}

void UPropertyAnimatorCounter::SetPresetFormatName(FName InPresetName)
{
	if (InPresetName.IsEqual(PresetFormatName))
	{
		return;
	}

	if (!GetAvailableFormatNames().Contains(InPresetName))
	{
		return;
	}

	PresetFormatName = InPresetName;
}

void UPropertyAnimatorCounter::SetCustomFormat(const FPropertyAnimatorCounterFormat* InFormat)
{
	if (InFormat)
	{
		CustomFormat = TInstancedStruct<FPropertyAnimatorCounterFormat>::Make(*InFormat);
	}
	else
	{
		CustomFormat.Reset();
	}

	OnCustomFormatChanged();
}

const FPropertyAnimatorCounterFormat* UPropertyAnimatorCounter::GetCustomFormat() const
{
	return CustomFormat.GetPtr<FPropertyAnimatorCounterFormat>();
}

FString UPropertyAnimatorCounter::FormatNumber(double InNumber) const
{
	FText Output = FText::GetEmpty();

	if (!bUseCustomFormat)
	{
		if (const UPropertyAnimatorSettings* AnimatorSettings = GetDefault<UPropertyAnimatorSettings>())
		{
			if (const FPropertyAnimatorCounterFormat* Format = AnimatorSettings->GetCounterFormat(PresetFormatName))
			{
				Output = FText::Format(DisplayPattern, FText::FromString(Format->FormatNumber(InNumber)));
			}
		}
	}
	else
	{
		if (const FPropertyAnimatorCounterFormat* Format = CustomFormat.GetPtr<FPropertyAnimatorCounterFormat>())
		{
			Output = FText::Format(DisplayPattern, FText::FromString(Format->FormatNumber(InNumber)));
		}
	}

	return Output.ToString();
}

#if WITH_EDITOR
FName UPropertyAnimatorCounter::GetUseCustomFormatPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCounter, bUseCustomFormat);
}

void UPropertyAnimatorCounter::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	// PECP for instanced struct does not provide the correct MemberProperty
	if (InPropertyChangedEvent.Property
		&& InPropertyChangedEvent.Property->GetOwnerStruct() == FPropertyAnimatorCounterFormat::StaticStruct())
	{
		OnCustomFormatChanged();
	}
	else if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCounter, bUseCustomFormat))
	{
		OnUseCustomFormatChanged();
	}
}

void UPropertyAnimatorCounter::OnAnimatorRegistered(FPropertyAnimatorCoreMetadata& InMetadata)
{
	Super::OnAnimatorRegistered(InMetadata);

	InMetadata.Name = TEXT("Counter");
}

void UPropertyAnimatorCounter::OpenPropertyAnimatorSettings()
{
	if (const UPropertyAnimatorSettings* AnimatorSettings = GetDefault<UPropertyAnimatorSettings>())
	{
		AnimatorSettings->OpenSettings();
	}
}

void UPropertyAnimatorCounter::SaveCustomFormatAsPreset()
{
	if (const FPropertyAnimatorCounterFormat* Format = CustomFormat.GetPtr<FPropertyAnimatorCounterFormat>())
	{
		if (UPropertyAnimatorSettings* AnimatorSettings = GetMutableDefault<UPropertyAnimatorSettings>())
		{
			if (AnimatorSettings->AddCounterFormat(*Format))
			{
				PresetFormatName = Format->FormatName;
				bUseCustomFormat = false;
				CustomFormat.Reset();
			}
		}
	}
}
#endif

void UPropertyAnimatorCounter::EvaluateProperties(FInstancedPropertyBag& InParameters)
{
	const double TimeElapsed = InParameters.GetValueDouble(TimeElapsedParameterName).GetValue();

	EvaluateEachLinkedProperty([this, TimeElapsed](
		UPropertyAnimatorCoreContext* InContext
		, const FPropertyAnimatorCoreData& InResolvedProperty
		, FInstancedPropertyBag& InEvaluatedValues
		, int32 InRangeIndex
		, int32 InRangeMax)->bool
	{
		const FName DisplayName(InResolvedProperty.GetPathHash());

		InEvaluatedValues.AddProperty(DisplayName, EPropertyBagPropertyType::String);
		InEvaluatedValues.SetValueString(DisplayName, FormatNumber(TimeElapsed));

		return true;
	});
}

TArray<FName> UPropertyAnimatorCounter::GetAvailableFormatNames() const
{
	TArray<FName> FormatNames;

	if (const UPropertyAnimatorSettings* AnimatorSettings = GetDefault<UPropertyAnimatorSettings>())
	{
		FormatNames = AnimatorSettings->GetCounterFormatNames().Array();
	}

	return FormatNames;
}

void UPropertyAnimatorCounter::OnCustomFormatChanged()
{
	if (FPropertyAnimatorCounterFormat* Format = CustomFormat.GetMutablePtr<FPropertyAnimatorCounterFormat>())
	{
		Format->EnsureCharactersLength();
	}
}

void UPropertyAnimatorCounter::OnUseCustomFormatChanged()
{
	// Start from the current selected preset format
	if (bUseCustomFormat && !CustomFormat.IsValid())
	{
		if (const UPropertyAnimatorSettings* AnimatorSettings = GetDefault<UPropertyAnimatorSettings>())
		{
			CustomFormat = TInstancedStruct<FPropertyAnimatorCounterFormat>::Make(*AnimatorSettings->GetCounterFormat(PresetFormatName));
		}
	}
}

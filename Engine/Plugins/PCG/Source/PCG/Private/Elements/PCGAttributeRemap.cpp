// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeRemap.h"

#include "PCGContext.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#define LOCTEXT_NAMESPACE "PCGAttributeRemapElement"

namespace PCGAttributeRemapElement
{
	struct FPCGAttributeRemapParams
	{
		double InRangeMin;
		double InRangeMax;
		double Slope;
		double Intercept;
		bool bClampToUnitRange;
		bool bIgnoreValuesOutsideInputRange;
	};

	// Fallback for all unsupported types, does nothing.
	template <typename T>
	void Remap(T& InOutValue, const FPCGAttributeRemapParams& InParams) {}

	// Int/Float version, utilizing the concepts.
	template <typename T> requires (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
	void Remap(T& InOutValue, const FPCGAttributeRemapParams& InParams)
	{
		if (!InParams.bIgnoreValuesOutsideInputRange || (InOutValue >= InParams.InRangeMin && InOutValue <= InParams.InRangeMax))
		{
			InOutValue = InParams.Slope * (InOutValue - InParams.InRangeMin) + InParams.Intercept;
			if (InParams.bClampToUnitRange)
			{
				InOutValue = FMath::Clamp<T>(InOutValue, T(0.0), T(1.0));
			}
		}
	}

	template <>
	void Remap<FVector2D>(FVector2D& InOutValue, const FPCGAttributeRemapParams& InParams)
	{
		Remap(InOutValue.X, InParams);
		Remap(InOutValue.Y, InParams);
	}

	template <>
	void Remap<FVector>(FVector& InOutValue, const FPCGAttributeRemapParams& InParams)
	{
		Remap(InOutValue.X, InParams);
		Remap(InOutValue.Y, InParams);
		Remap(InOutValue.Z, InParams);
	}

	template <>
	void Remap<FVector4>(FVector4& InOutValue, const FPCGAttributeRemapParams& InParams)
	{
		Remap(InOutValue.X, InParams);
		Remap(InOutValue.Y, InParams);
		Remap(InOutValue.Z, InParams);
		Remap(InOutValue.W, InParams);
	}

	template <>
	void Remap<FRotator>(FRotator& InOutValue, const FPCGAttributeRemapParams& InParams)
	{
		Remap(InOutValue.Roll, InParams);
		Remap(InOutValue.Pitch, InParams);
		Remap(InOutValue.Yaw, InParams);
	}
}

#if WITH_EDITOR
FName UPCGAttributeRemapSettings::GetDefaultNodeName() const
{
	return FName(TEXT("AttributeRemap"));
}

FText UPCGAttributeRemapSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Attribute Remap");
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGAttributeRemapSettings::GetPreconfiguredInfo() const
{
	return { FPCGPreConfiguredSettingsInfo{0, LOCTEXT("DensityNodeTitle", "Density Remap")} };
}
#endif // WITH_EDITOR

FString UPCGAttributeRemapSettings::GetAdditionalTitleInformation() const
{
	return FString::Printf(TEXT("%s -> %s"), *InputSource.ToString(), *OutputTarget.ToString());
}

void UPCGAttributeRemapSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	switch (PreconfigureInfo.PreconfiguredIndex)
	{
	case 0:
		InputSource.SetPointProperty(EPCGPointProperties::Density);
		bClampToUnitRange = true;
		break;
	default:
		break;
	}
}

FPCGAttributePropertyInputSelector UPCGAttributeRemapSettings::GetInputSource(uint32 Index) const
{
	return Index == 0 ? InputSource : FPCGAttributePropertyInputSelector{};
}

bool UPCGAttributeRemapSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	return PCG::Private::IsOfTypes<int32, int64, float, double, FVector2D, FVector, FVector4, FRotator, FQuat>(TypeId);
}

FPCGElementPtr UPCGAttributeRemapSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeRemapElement>();
}

bool FPCGAttributeRemapElement::DoOperation(PCGMetadataOps::FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeRemapElement::Execute);

	const UPCGAttributeRemapSettings* Settings = CastChecked<UPCGAttributeRemapSettings>(OperationData.Settings);

	PCGAttributeRemapElement::FPCGAttributeRemapParams Params{};
	Params.bClampToUnitRange = Settings->bClampToUnitRange;
	Params.bIgnoreValuesOutsideInputRange = Settings->bIgnoreValuesOutsideInputRange;

	Params.InRangeMin = FMath::Min(Settings->InRangeMin, Settings->InRangeMax);
	Params.InRangeMax = FMath::Max(Settings->InRangeMin, Settings->InRangeMax);

	const double OutRangeTrueMin = FMath::Min(Settings->OutRangeMin, Settings->OutRangeMax);
	const double OutRangeTrueMax = FMath::Max(Settings->OutRangeMin, Settings->OutRangeMax);

	const double InRangeDifference = Params.InRangeMax - Params.InRangeMin;
	const double OutRangeDifference = OutRangeTrueMax - OutRangeTrueMin;

	// When InRange is a point leave the Slope at 0 so that Density = Intercept
	if (InRangeDifference == 0)
	{
		Params.Slope = 0;
		Params.Intercept = (OutRangeTrueMin + OutRangeTrueMax) / 2.f;
	}
	else
	{
		Params.Slope = OutRangeDifference / InRangeDifference;
		Params.Intercept = OutRangeTrueMin;
	}

	auto RemapFunc = [this, &Params, &OperationData]<typename AttributeType>(AttributeType) -> bool
	{
		using FinalType = std::conditional_t<std::is_same_v<AttributeType, FQuat>, FRotator, AttributeType>;
		return DoUnaryOp<FinalType>(OperationData, [&Params](const FinalType& Value) -> FinalType 
		{ 
			FinalType Out = Value;
			PCGAttributeRemapElement::Remap<FinalType>(Out, Params); 
			return Out; 
		});
	};

	return PCGMetadataAttribute::CallbackWithRightType(OperationData.MostComplexInputType, RemapFunc);
}

#undef LOCTEXT_NAMESPACE

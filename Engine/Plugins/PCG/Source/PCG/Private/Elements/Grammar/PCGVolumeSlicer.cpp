// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Grammar/PCGVolumeSlicer.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Grammar/PCGGrammarParser.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGVolumeSlicerElement"

#if WITH_EDITOR
FName UPCGVolumeSlicerSettings::GetDefaultNodeName() const
{
	return FName(TEXT("VolumeSlicer"));
}

FText UPCGVolumeSlicerSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Volume Slicer");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGVolumeSlicerSettings::CreateElement() const
{
	return MakeShared<FPCGVolumeSlicerElement>();
}

TArray<FPCGPinProperties> UPCGVolumeSlicerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Result;
	FPCGPinProperties& InputPin = Result.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spline);
	InputPin.SetRequiredPin();

	if (bModuleInfoAsInput)
	{
		FPCGPinProperties& ModuleInfoPin = Result.Emplace_GetRef(PCGSlicingBaseConstants::ModulesInfoPinLabel, EPCGDataType::Param);
		ModuleInfoPin.SetRequiredPin();
	}

	return Result;
}

TArray<FPCGPinProperties> UPCGVolumeSlicerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Result;
	Result.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);

	return Result;
}

bool FPCGVolumeSlicerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGVolumeSlicerElement::Execute);

	check(InContext);

	const UPCGVolumeSlicerSettings* Settings = InContext->GetInputSettings<UPCGVolumeSlicerSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	const UPCGParamData* ModuleInfoParamData = nullptr;
	PCGSlicingBase::FPCGModulesInfoMap ModulesInfo = GetModulesInfoMap(InContext, Settings, ModuleInfoParamData);

	const bool bMatchAndSetAttributes = Settings->bForwardAttributesFromModulesInfo && ModuleInfoParamData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSplineData* InputSplineData = Cast<const UPCGSplineData>(Input.Data);

		if (!InputSplineData)
		{
			continue;
		}

		const TArray<FInterpCurvePointVector>& ControlPoints = InputSplineData->SplineStruct.SplineCurves.Position.Points;
		if (ControlPoints.Num() < 2)
		{
			continue;
		}

		FVector ExtrudeVector = Settings->ExtrudeVector;

		auto GetValueFromAttribute = [InputSplineData, InContext]<typename T>(const FPCGAttributePropertyInputSelector& InSelector, T& OutValue) -> bool
		{
			const FPCGAttributePropertyInputSelector Selector = InSelector.CopyAndFixLast(InputSplineData);
			const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputSplineData, Selector);
			if (!Accessor)
			{
				PCGLog::Metadata::LogFailToCreateAccessor(Selector, InContext);
				return false;
			}

			if (!Accessor->Get(OutValue, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
			{
				PCGLog::Metadata::LogFailToGetAttribute<T>(Selector, Accessor.Get(), InContext);
				return false;
			}

			return true;
		};

		if (Settings->bExtrudeVectorAsAttribute && !GetValueFromAttribute(Settings->ExtrudeVectorAttribute, ExtrudeVector))
		{
			continue;
		}

		const double ExtrudeLength = ExtrudeVector.Length();
		if (FMath::IsNearlyZero(ExtrudeLength))
		{
			continue;
		}

		const FVector ExtrudeDirection = ExtrudeVector / ExtrudeLength;

		double MinSize = 0.0;
		PCGGrammar::FTokenizedGrammar TokenizedGrammar = GetTokenizedGrammar(InContext, InputSplineData, Settings, ModulesInfo, MinSize);

		if (TokenizedGrammar.IsEmpty())
		{
			continue;
		}

		TArray<PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>> Instances;
		double RemainingLength = 0.0;
		const bool bHeightSubdivideSuccess = PCGSlicingBase::Subdivide(TokenizedGrammar, ExtrudeLength, Instances, RemainingLength);

		if (!bHeightSubdivideSuccess)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("SubdivideFailed", "Grammar doesn't fit."), InContext);
			continue;
		}

		FVector CurrentDisplacement = FVector::ZeroVector;
		int32 SplineIndex = 0;

		for (const PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>& Instance : Instances)
		{
			for (int i = 0; i < Instance.NumRepeat; ++i)
			{
				for (int32 SymbolIndex = 0; SymbolIndex < Instance.Module->Symbols.Num(); ++SymbolIndex)
				{
					const FName Symbol = Instance.Module->Symbols[SymbolIndex];
					const FVector Size = ExtrudeDirection * Instance.Module->SymbolSizes[SymbolIndex] * (FVector::OneVector + Instance.ExtraScales[SymbolIndex]);
					const FPCGSlicingSubmodule& CurrentBlock = ModulesInfo[Symbol];
					FPCGSplineStruct NewSpline = InputSplineData->SplineStruct;
					for (FInterpCurvePointVector& ControlPoint : NewSpline.SplineCurves.Position.Points)
					{
						ControlPoint.OutVal += CurrentDisplacement;
					}

					UPCGSplineData* NewSplineData = FPCGContext::NewObject_AnyThread<UPCGSplineData>(InContext);
					NewSplineData->Initialize(NewSpline);
					NewSplineData->InitializeFromData(InputSplineData);

					NewSplineData->Metadata->FindOrCreateAttribute<FName>(Settings->SymbolAttributeName, Symbol, false, false);

					if (!bMatchAndSetAttributes && Settings->bOutputDebugColorAttribute)
					{
						NewSplineData->Metadata->FindOrCreateAttribute<FVector4>(Settings->DebugColorAttributeName, FVector4(CurrentBlock.DebugColor, 1.0), false, false);
					}

					if (Settings->bOutputSizeAttribute)
					{
						NewSplineData->Metadata->FindOrCreateAttribute<FVector>(Settings->SizeAttributeName, Size, false, false);
					}

					if (!bMatchAndSetAttributes && Settings->bOutputScalableAttribute)
					{
						NewSplineData->Metadata->FindOrCreateAttribute<bool>(Settings->ScalableAttributeName, CurrentBlock.bScalable, false, false);
					}

					if (Settings->bOutputSplineIndexAttribute)
					{
						NewSplineData->Metadata->FindOrCreateAttribute<int32>(Settings->SplineIndexAttributeName, SplineIndex++, false, false);
					}

					Outputs.Add_GetRef(Input).Data = NewSplineData;
					CurrentDisplacement += Size;
				}
			}
		}
	}

	if (bMatchAndSetAttributes)
	{
		MatchAndSetAttributes(Inputs, Outputs, ModuleInfoParamData, Settings);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

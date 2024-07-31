// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Grammar/PCGSplineSlicer.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Helpers/PCGHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#define LOCTEXT_NAMESPACE "PCGSplineSlicerElement"

namespace PCGSplineSlicerHelpers
{
	struct FParameters
	{
		FPCGContext* Context = nullptr;

		PCGSlicingBase::FPCGModulesInfoMap ModulesInfo;
		TMap<FString, PCGGrammar::FTokenizedGrammar> CachedModules;
		bool bAcceptIncompleteSlicing = false;
		double ModuleHeight = 0.0;

		const UPCGPolyLineData* PolyLineData = nullptr;
		TArray<FPCGPoint>* OutPoints = nullptr;
		UPCGMetadata* OutputMetadata = nullptr;

		FPCGMetadataAttribute<FName>* SymbolAttribute = nullptr;
		FPCGMetadataAttribute<FVector4>* DebugColorAttribute = nullptr;
		FPCGMetadataAttribute<int32>* ModuleIndexAttribute = nullptr;
		FPCGMetadataAttribute<bool>* IsFirstPointAttribute = nullptr;
		FPCGMetadataAttribute<bool>* IsFinalPointAttribute = nullptr;
	};

	double GetLinearDistanceBetweenPolyLineAlphas(const UPCGPolyLineData* PolyLineData, double FirstAlpha, double SecondAlpha)
	{
		check(PolyLineData);

		FirstAlpha = FMath::Clamp(FirstAlpha, 0, 1);
		SecondAlpha = FMath::Clamp(SecondAlpha, 0, 1);
		const FVector FirstPoint = PolyLineData->GetLocationAtAlpha(FirstAlpha);
		const FVector SecondPoint = PolyLineData->GetLocationAtAlpha(SecondAlpha);
		return FVector::Distance(FirstPoint, SecondPoint);
	}

	// Using a numerical method (s.a. bisection), determine the spline alpha from a segment length. Bisection can be slower, but guaranteed to converge.
	static double FindRootAtLinearDistance_Bisection(const UPCGPolyLineData* PolyLineData, const double SegmentLength, const double StartingAlpha, const double Tolerance = 1)
	{
		check(PolyLineData);

		// Bisect a number of times, before taking an estimate
		static constexpr uint16 BisectionCountLimit = 64u;

		double Low = StartingAlpha;
		double High = 1.0;
		double Estimate = 0.0;

		for (uint16 BisectionCount = 0u; BisectionCount < BisectionCountLimit; ++BisectionCount)
		{
			Estimate = (Low + High) * 0.5;
			// TODO: If performance is an issue, this can deal with Dist^2 instead to save the sqrt. But, it will skew the tolerance.
			const double LinearDistance = GetLinearDistanceBetweenPolyLineAlphas(PolyLineData, StartingAlpha, Estimate);
			check(LinearDistance >= 0);

			if (FMath::IsNearlyEqual(LinearDistance, SegmentLength, Tolerance))
			{
				return Estimate;
			}

			if (LinearDistance < SegmentLength)
			{
				Low = Estimate;
			}
			else
			{
				High = Estimate;
			}
		}

		// Couldn't find the root yet, so estimate
		return (Low + High) * 0.5;
	}

	static void Process(FParameters& InOutParameters, const FString& InGrammar)
	{
		if (!InOutParameters.CachedModules.Contains(InGrammar))
		{
			double MinSize;
			InOutParameters.CachedModules.Emplace(InGrammar, PCGSlicingBase::GetTokenizedGrammar(InOutParameters.Context, InGrammar, InOutParameters.ModulesInfo, MinSize));
		}

		const PCGGrammar::FTokenizedGrammar& CurrentTokenizedGrammar = InOutParameters.CachedModules[InGrammar];

		if (CurrentTokenizedGrammar.IsEmpty())
		{
			return;
		}

		const double SplineLength = InOutParameters.PolyLineData->GetLength();

		/* Implementation Note: Subdivided spline length will always be equal or greater than discretized linear length, depending on the curvature of the spline.
		 * For extremely long or curvy splines, this can result in the tokenized grammar being cut short. Alternative subdivision solutions may need to be explored.
		 */
		TArray<PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>> ModulesInstances;
		double RemainingSubdivide;
		if (!Subdivide(CurrentTokenizedGrammar, SplineLength, ModulesInstances, RemainingSubdivide, InOutParameters.Context))
		{
			return;
		}

		if (!InOutParameters.bAcceptIncompleteSlicing && !FMath::IsNearlyZero(RemainingSubdivide))
		{
			PCGLog::LogWarningOnGraph(LOCTEXT("FailSliceFullLength", "The spline has an incomplete slicing (grammar doesn't fit the whole segment)."), InOutParameters.Context);
			return;
		}

		const bool bHasMetadata = InOutParameters.SymbolAttribute || InOutParameters.DebugColorAttribute || InOutParameters.ModuleIndexAttribute || InOutParameters.IsFirstPointAttribute || InOutParameters.IsFinalPointAttribute;

		int32 ModuleIndexCounter = 0; // Keep track for the output attribute
		double SplineAlpha = 0.0; // Will be incremented as we progress through the spline
		for (int32 ModuleInstanceIndex = 0; ModuleInstanceIndex < ModulesInstances.Num(); ModuleInstanceIndex++)
		{
			const PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>& ModuleInstance = ModulesInstances[ModuleInstanceIndex];
			for (int32 SubmoduleInstanceIndex = 0; SubmoduleInstanceIndex < ModuleInstance.NumRepeat; ++SubmoduleInstanceIndex)
			{
				for (int32 SymbolIndex = 0; SymbolIndex < ModuleInstance.Module->Symbols.Num(); ++SymbolIndex)
				{
					const FName Symbol = ModuleInstance.Module->Symbols[SymbolIndex];
					const FPCGSlicingSubmodule& SlicingSubmodule = InOutParameters.ModulesInfo[Symbol];
					const double SubmoduleSize = SlicingSubmodule.Size;

					// Move to the next segment of the spline
					const FVector SegmentStartPoint = InOutParameters.PolyLineData->GetLocationAtAlpha(SplineAlpha);
					const double PreviousAlpha = SplineAlpha;

					// Use a numerical method to find the root of where the module lands on the spline
					SplineAlpha = FindRootAtLinearDistance_Bisection(InOutParameters.PolyLineData, SubmoduleSize, SplineAlpha);

					const FVector SegmentEndPoint = InOutParameters.PolyLineData->GetLocationAtAlpha(SplineAlpha);
					const FVector SliceVector = SegmentEndPoint - SegmentStartPoint;
					const FVector SliceDirection = SliceVector.GetSafeNormal();

					// At the end of the spline, truncate an unfinished submodule and end the process
					if (FMath::IsNearlyEqual(SplineAlpha, 1))
					{
						if (SliceVector.Length() < SubmoduleSize)
						{
							if (InOutParameters.IsFinalPointAttribute && !InOutParameters.OutPoints->IsEmpty())
							{
								InOutParameters.IsFinalPointAttribute->SetValue(InOutParameters.OutPoints->Last().MetadataEntry, true);
							}

							return;
						}
					}

					// Since its discretized, we won't take the transform's position, but we'll use the up vector--to create the module rotation--and the scale
					FTransform CenterPointTransform = InOutParameters.PolyLineData->GetTransformAtAlpha((SplineAlpha + PreviousAlpha) * 0.5);

					const FVector Position = SegmentStartPoint + (SliceVector * 0.5) + FVector(0, 0, InOutParameters.ModuleHeight * 0.5);
					const FRotator Rotation = FRotationMatrix::MakeFromXZ(SliceDirection, CenterPointTransform.GetRotation().GetUpVector()).Rotator();
					const FVector Scale = FVector::OneVector + (SliceDirection * ModuleInstance.ExtraScales[SymbolIndex]);

					FPCGPoint& OutPoint = InOutParameters.OutPoints->Emplace_GetRef(FTransform(Rotation, Position, Scale), /*InDensity=*/1, PCGHelpers::ComputeSeedFromPosition(Position));

					const double HalfSubmoduleSize = SlicingSubmodule.Size * 0.5;
					OutPoint.SetLocalBounds(FBox(FVector(-HalfSubmoduleSize, 0, 0), FVector(HalfSubmoduleSize, 1, InOutParameters.ModuleHeight)));

					// Now, handle the metadata attributes
					if (!bHasMetadata)
					{
						continue;
					}

					InOutParameters.OutputMetadata->InitializeOnSet(OutPoint.MetadataEntry);
					if (InOutParameters.SymbolAttribute)
					{
						InOutParameters.SymbolAttribute->SetValue(OutPoint.MetadataEntry, Symbol);
					}

					if (InOutParameters.DebugColorAttribute)
					{
						InOutParameters.DebugColorAttribute->SetValue(OutPoint.MetadataEntry, FVector4(SlicingSubmodule.DebugColor, 1.0));
					}

					if (InOutParameters.ModuleIndexAttribute)
					{
						InOutParameters.ModuleIndexAttribute->SetValue(OutPoint.MetadataEntry, ModuleIndexCounter++);
					}

					const bool bIsFirstModule = (ModuleInstanceIndex == 0) && (SubmoduleInstanceIndex == 0) && (SymbolIndex == 0);
					if (bIsFirstModule && InOutParameters.IsFirstPointAttribute)
					{
						InOutParameters.IsFirstPointAttribute->SetValue(OutPoint.MetadataEntry, true);
					}

					const bool bIsFinalModule = (ModuleInstanceIndex == ModulesInstances.Num() - 1) && (SubmoduleInstanceIndex == ModuleInstance.NumRepeat - 1) && (SymbolIndex == ModuleInstance.Module->Symbols.Num() - 1);
					if (bIsFinalModule && InOutParameters.IsFinalPointAttribute)
					{
						InOutParameters.IsFinalPointAttribute->SetValue(OutPoint.MetadataEntry, true);
					}
				}
			}
		}
	}
}

FPCGElementPtr UPCGSplineSlicerSettings::CreateElement() const
{
	return MakeShared<FPCGSplineSlicerElement>();
}

TArray<FPCGPinProperties> UPCGSplineSlicerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPin = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::PolyLine);
	InputPin.SetRequiredPin();

	if (bModuleInfoAsInput)
	{
		FPCGPinProperties& ModuleInfoPin = PinProperties.Emplace_GetRef(PCGSlicingBaseConstants::ModulesInfoPinLabel, EPCGDataType::Param);
		ModuleInfoPin.SetRequiredPin();
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGSplineSlicerSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

bool FPCGSplineSlicerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplineSlicerElement::Execute);

	const UPCGSplineSlicerSettings* Settings = InContext->GetInputSettings<UPCGSplineSlicerSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	const UPCGParamData* ModuleInfoParamData = nullptr;
	const FPCGModulesInfoMap ModulesInfo = GetModulesInfoMap(InContext, Settings, ModuleInfoParamData);

	double ModuleHeight = Settings->bModuleHeightAsAttribute ? 0.0 : Settings->ModuleHeight;

	PCGSplineSlicerHelpers::FParameters Parameters
	{
		.Context = InContext,
		.ModulesInfo = ModulesInfo,
		.bAcceptIncompleteSlicing = Settings->bAcceptIncompleteSlicing
	};

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPolyLineData* InputPolyLineData = Cast<const UPCGPolyLineData>(Input.Data);
		if (!InputPolyLineData)
		{
			continue;
		}

		if (Settings->bModuleHeightAsAttribute)
		{
			check(InputPolyLineData->Metadata);
			const FPCGAttributePropertyInputSelector Selector = Settings->ModuleHeightAttribute.CopyAndFixLast(InputPolyLineData);
			TUniquePtr<const IPCGAttributeAccessor> HeightAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPolyLineData, Selector);

			if (!HeightAccessor)
			{
				PCGLog::Metadata::LogFailToCreateAccessor(Selector, InContext);
				continue;
			}

			if (!HeightAccessor->Get<double>(ModuleHeight, FPCGAttributeAccessorKeysEntries(PCGInvalidEntryKey), EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
			{
				PCGLog::Metadata::LogFailToGetAttribute(Selector, InContext);
				continue;
			}
		}

		UPCGPointData* OutputPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(InContext);
		OutputPointData->InitializeFromData(InputPolyLineData);
		FPCGTaggedData& Output = Outputs.Emplace_GetRef(Input);
		Output.Data = OutputPointData;

		// Update 'per-input' parameters
		Parameters.PolyLineData = InputPolyLineData;
		Parameters.OutPoints = &OutputPointData->GetMutablePoints();
		Parameters.OutputMetadata = OutputPointData->Metadata;
		Parameters.ModuleHeight = ModuleHeight;

		auto CreateAndValidateAttribute = [InContext, OutputMetadata = OutputPointData->Metadata]<typename T>(const FName AttributeName, const T DefaultValue, const bool bShouldCreate, FPCGMetadataAttribute<T>*& OutAttribute) -> bool
		{
			if (bShouldCreate)
			{
				OutAttribute = OutputMetadata->FindOrCreateAttribute<T>(AttributeName, DefaultValue, false, true);
				if (!OutAttribute)
				{
					PCGLog::Metadata::LogFailToCreateAttribute<T>(AttributeName, InContext);
					return false;
				}
			}

			return true;
		};

		if (!CreateAndValidateAttribute(Settings->SymbolAttributeName, FName(NAME_None), true, Parameters.SymbolAttribute)
			|| !CreateAndValidateAttribute(Settings->DebugColorAttributeName, FVector4::Zero(), Settings->bOutputDebugColorAttribute, Parameters.DebugColorAttribute)
			|| !CreateAndValidateAttribute(Settings->ModuleIndexAttributeName, -1, Settings->bOutputModuleIndexAttribute, Parameters.ModuleIndexAttribute)
			|| !CreateAndValidateAttribute(Settings->IsFirstAttributeName, false, Settings->bOutputExtremityAttributes, Parameters.IsFirstPointAttribute)
			|| !CreateAndValidateAttribute(Settings->IsFinalAttributeName, false, Settings->bOutputExtremityAttributes, Parameters.IsFinalPointAttribute))
		{
			continue;
		}

		if (Settings->GrammarSelection.bGrammarAsAttribute)
		{
			const FPCGAttributePropertyInputSelector Selector = Settings->GrammarSelection.GrammarAttribute.CopyAndFixLast(InputPolyLineData);
			const TUniquePtr<const IPCGAttributeAccessor> GrammarAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPolyLineData, Selector);
			const FPCGAttributeAccessorKeysEntries Keys(PCGInvalidEntryKey);
			if (!GrammarAccessor)
			{
				PCGLog::Metadata::LogFailToCreateAccessor(Selector, InContext);
				continue;
			}

			auto Process = [&Parameters](const FString& InGrammar, int32 Index) -> void
			{
				PCGSplineSlicerHelpers::Process(Parameters, InGrammar);
			};

			PCGMetadataElementCommon::ApplyOnAccessor<FString>(Keys, *GrammarAccessor, Process);
		}
		else
		{
			PCGSplineSlicerHelpers::Process(Parameters, Settings->GrammarSelection.GrammarString);
		}
	}

	if (Settings->bForwardAttributesFromModulesInfo && ModuleInfoParamData)
	{
		MatchAndSetAttributes(Inputs, Outputs, ModuleInfoParamData, Settings);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

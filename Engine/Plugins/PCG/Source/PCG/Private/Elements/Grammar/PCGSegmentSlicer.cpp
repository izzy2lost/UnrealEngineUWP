// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Grammar/PCGSegmentSlicer.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Grammar/PCGGrammarParser.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGSegmentSlicerElement"

class PCGSegmentSlicerHelpers
{
public:
	struct FParameters
	{
		FPCGMetadataAttribute<FName>* SymbolAttribute = nullptr;
		FPCGMetadataAttribute<FVector4>* DebugColorAttribute = nullptr;
		FPCGMetadataAttribute<int32>* ModuleIndexAttribute = nullptr;
		FPCGMetadataAttribute<bool>* IsFirstPointAttribute = nullptr;
		FPCGMetadataAttribute<bool>* IsFinalPointAttribute = nullptr;
		FPCGMetadataAttribute<int32>* ExtremityNeighborIndexAttribute = nullptr;

		PCGSlicingBase::FPCGModulesInfoMap ModulesInfo;
		TMap<FString, PCGGrammar::FTokenizedGrammar> CachedModules;
		TArray<int32> CornerIndexes;

		FVector SlicingDirection;
		FVector PerpendicularSlicingDirection;

		const UPCGSegmentSlicerSettings* Settings = nullptr;
		FPCGContext* Context = nullptr;
		const TArray<FPCGPoint>* InPoints = nullptr;
		TArray<FPCGPoint>* OutPoints = nullptr;
		UPCGMetadata* OutputMetadata = nullptr;
	};

	static void Process(FParameters& InOutParameters, const FString& InGrammar, const bool bFlipAxis, int32 Index)
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

		const FPCGPoint& Point = (*InOutParameters.InPoints)[Index];
		FTransform TransformNoTranslation = Point.Transform;
		TransformNoTranslation.SetLocation(FVector::ZeroVector);

		const int32 FirstModuleIndex = InOutParameters.OutPoints->Num();

		FBox Segment = Point.GetLocalBounds();
		FVector PointScaledSize = Point.GetScaledLocalSize();
		if (bFlipAxis)
		{
			// Swap coordinates on the slicing direction
			const FVector PreviousMin = Segment.Min;
			Segment.Min = Segment.Min * InOutParameters.PerpendicularSlicingDirection + Segment.Max * InOutParameters.SlicingDirection;
			Segment.Max = Segment.Max * InOutParameters.PerpendicularSlicingDirection + PreviousMin * InOutParameters.SlicingDirection;
			PointScaledSize *= (InOutParameters.PerpendicularSlicingDirection - InOutParameters.SlicingDirection);
		}

		const FVector Direction = TransformNoTranslation.TransformVectorNoScale(InOutParameters.SlicingDirection).GetSafeNormal();
		const FVector OtherDirection = TransformNoTranslation.TransformVectorNoScale(PointScaledSize * InOutParameters.PerpendicularSlicingDirection) * 0.5;
		const FVector HalfExtents2D = PointScaledSize * InOutParameters.PerpendicularSlicingDirection * 0.5;
		const double Size = PointScaledSize.Dot(InOutParameters.SlicingDirection);

		TArray<PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>> ModulesInstances;
		double RemainingSubdivide;
		const bool bSubdivideSuccess = PCGSlicingBase::Subdivide(CurrentTokenizedGrammar, Size, ModulesInstances, RemainingSubdivide, InOutParameters.Context);

		if (!bSubdivideSuccess)
		{
			return;
		}

		if (!InOutParameters.Settings->bAcceptIncompleteSlicing && !FMath::IsNearlyZero(RemainingSubdivide))
		{
			PCGLog::LogWarningOnGraph(LOCTEXT("FailSliceFullLength", "One segment has an incomplete slicing (grammar doesn't fit the whole segment)."), InOutParameters.Context);
			return;
		}

		// Now we have our segment subdivided, create the final points
		const FVector InitialPos = Point.Transform.TransformPosition(Segment.Min);
		FVector CurrentPos = InitialPos;
		int32 ModuleIndex = 0;

		for (int32 ModuleInstanceIndex = 0; ModuleInstanceIndex < ModulesInstances.Num(); ModuleInstanceIndex++)
		{
			const PCGSlicingBase::TPCGSubDivModuleInstance<PCGGrammar::FTokenizedModule>& ModuleInstance = ModulesInstances[ModuleInstanceIndex];
			for (int32 j = 0; j < ModuleInstance.NumRepeat; ++j)
			{
				for (int32 SymbolIndex = 0; SymbolIndex < ModuleInstance.Module->Symbols.Num(); ++SymbolIndex)
				{
					const FName Symbol = ModuleInstance.Module->Symbols[SymbolIndex];
					const FVector Scale = FVector::OneVector + (InOutParameters.SlicingDirection * ModuleInstance.ExtraScales[SymbolIndex]);
					const FPCGSlicingSubmodule& SlicingSubmodule = InOutParameters.ModulesInfo[Symbol];

					const bool bIsFirstModule = (ModuleInstanceIndex == 0) && (j == 0) && (SymbolIndex == 0);
					const bool bIsFinalModule = (ModuleInstanceIndex == ModulesInstances.Num() - 1) && (j == ModuleInstance.NumRepeat - 1) && (SymbolIndex == ModuleInstance.Module->Symbols.Num() - 1);
					const double HalfDisplacement = SlicingSubmodule.Size * 0.5;
					const double HalfScaledDisplacement = Scale.Dot(InOutParameters.SlicingDirection) * HalfDisplacement;

					const FVector LocalBoundsExtents = InOutParameters.SlicingDirection * HalfDisplacement + HalfExtents2D;
					const FVector HalfStep = HalfScaledDisplacement * Direction;
					const FVector Position = CurrentPos + HalfStep;
					CurrentPos = Position + HalfStep;

					FPCGPoint& OutPoint = InOutParameters.OutPoints->Emplace_GetRef(Point);
					OutPoint.Transform = FTransform(Point.Transform.GetRotation(), Position + OtherDirection, Scale);
					OutPoint.SetLocalBounds(FBox(-LocalBoundsExtents, LocalBoundsExtents));
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
						InOutParameters.ModuleIndexAttribute->SetValue(OutPoint.MetadataEntry, ModuleIndex++);
					}

					if (bIsFirstModule && InOutParameters.IsFirstPointAttribute)
					{
						InOutParameters.IsFirstPointAttribute->SetValue(OutPoint.MetadataEntry, true);
					}

					if (bIsFinalModule && InOutParameters.IsFinalPointAttribute)
					{
						InOutParameters.IsFinalPointAttribute->SetValue(OutPoint.MetadataEntry, true);
					}
				}
			}
		}

		if (bFlipAxis)
		{
			InOutParameters.CornerIndexes.Add(InOutParameters.OutPoints->Num() - 1);
			InOutParameters.CornerIndexes.Add(FirstModuleIndex);
		}
		else
		{
			InOutParameters.CornerIndexes.Add(FirstModuleIndex);
			InOutParameters.CornerIndexes.Add(InOutParameters.OutPoints->Num() - 1);
		}
	}
};

#if WITH_EDITOR
FName UPCGSegmentSlicerSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SegmentSlicer"));
}

FText UPCGSegmentSlicerSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Segment Slicer");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGSegmentSlicerSettings::CreateElement() const
{
	return MakeShared<FPCGSegmentSlicerElement>();
}

TArray<FPCGPinProperties> UPCGSegmentSlicerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Result;
	FPCGPinProperties& InputPin = Result.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPin.SetRequiredPin();

	if (bModuleInfoAsInput)
	{
		FPCGPinProperties& ModuleInfoPin = Result.Emplace_GetRef(PCGSlicingBaseConstants::ModulesInfoPinLabel, EPCGDataType::Param);
		ModuleInfoPin.SetRequiredPin();
	}

	return Result;
}

TArray<FPCGPinProperties> UPCGSegmentSlicerSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

bool FPCGSegmentSlicerElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSegmentSlicerElement::Execute);

	check(InContext);

	const UPCGSegmentSlicerSettings* Settings = InContext->GetInputSettings<UPCGSegmentSlicerSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	PCGSegmentSlicerHelpers::FParameters Parameters{};

	Parameters.SlicingDirection = FVector::ZeroVector;

	switch (Settings->SlicingAxis)
	{
	case EPCGSplitAxis::X:
		Parameters.SlicingDirection = FVector::XAxisVector;
		break;
	case EPCGSplitAxis::Y:
		Parameters.SlicingDirection = FVector::YAxisVector;
		break;
	case EPCGSplitAxis::Z:
		Parameters.SlicingDirection = FVector::ZAxisVector;
		break;
	default:
		PCGLog::LogErrorOnGraph(LOCTEXT("InvalidAxis", "Invalid Slicing Axis enum value."), InContext);
		return true;
	}

	Parameters.PerpendicularSlicingDirection = FVector::OneVector - Parameters.SlicingDirection;

	const UPCGParamData* ModuleInfoParamData = nullptr;
	Parameters.ModulesInfo = GetModulesInfoMap(InContext, Settings, ModuleInfoParamData);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<const UPCGPointData>(Input.Data);

		if (!InputPointData)
		{
			continue;
		}

		TUniquePtr<const IPCGAttributeAccessor> GrammarAccessor;
		TUniquePtr<const IPCGAttributeAccessor> FlipAxisAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> Keys;

		if (Settings->GrammarSelection.bGrammarAsAttribute)
		{
			const FPCGAttributePropertyInputSelector Selector = Settings->GrammarSelection.GrammarAttribute.CopyAndFixLast(InputPointData);
			GrammarAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, Selector);
			Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, Selector);
			if (!GrammarAccessor || !Keys)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("GrammarAccessor", "Attribute {0} was not found for the grammar."), Selector.GetDisplayText()), InContext);
				continue;
			}
		}

		if (Settings->bFlipAxisAsAttribute)
		{
			const FPCGAttributePropertyInputSelector Selector = Settings->FlipAxisAttribute.CopyAndFixLast(InputPointData);
			FlipAxisAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputPointData, Selector);
			if (!Keys)
			{
				Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputPointData, Selector);
			}

			if (!FlipAxisAccessor || !Keys)
			{
				PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("FlipAxisAccessor", "Attribute {0} was not found for the flip axis attribute."), Selector.GetDisplayText()), InContext);
				continue;
			}
		}

		UPCGPointData* OutputPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(InContext);
		OutputPointData->InitializeFromData(InputPointData);

		Parameters.Settings = Settings;
		Parameters.Context = InContext;
		Parameters.InPoints = &InputPointData->GetPoints();
		Parameters.OutPoints = &OutputPointData->GetMutablePoints();
		Parameters.OutputMetadata = OutputPointData->Metadata;

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
			|| !CreateAndValidateAttribute(Settings->IsFinalAttributeName, false, Settings->bOutputExtremityAttributes, Parameters.IsFinalPointAttribute)
			|| !CreateAndValidateAttribute(Settings->ExtremityNeighborIndexAttributeName, -1, Settings->bOutputExtremityNeighborIndexAttribute, Parameters.ExtremityNeighborIndexAttribute))
		{
			continue;
		}


		if (Settings->GrammarSelection.bGrammarAsAttribute && Settings->bFlipAxisAsAttribute)
		{
			auto Process = [&Parameters](const FString& InGrammar, const bool bFlipAxis, int32 Index) -> void
			{
				PCGSegmentSlicerHelpers::Process(Parameters, InGrammar, bFlipAxis, Index);
			};

			PCGMetadataElementCommon::ApplyOnMultiAccessors<FString, bool>(*Keys, { GrammarAccessor.Get(), FlipAxisAccessor.Get() }, Process);
		}
		else if (Settings->GrammarSelection.bGrammarAsAttribute)
		{
			auto Process = [&Parameters, bShouldFlipAxis = Settings->bShouldFlipAxis](const FString& InGrammar, int32 Index) -> void
			{
				PCGSegmentSlicerHelpers::Process(Parameters, InGrammar, bShouldFlipAxis, Index);
			};

			PCGMetadataElementCommon::ApplyOnAccessor<FString>(*Keys, *GrammarAccessor, Process);
		}
		else if (Settings->bFlipAxisAsAttribute)
		{
			auto Process = [&Parameters, Grammar = Settings->GrammarSelection.GrammarString](const bool bFlipAxis, int32 Index) -> void
			{
				PCGSegmentSlicerHelpers::Process(Parameters, Grammar, bFlipAxis, Index);
			};

			PCGMetadataElementCommon::ApplyOnAccessor<bool>(*Keys, *FlipAxisAccessor, Process);
		}
		else
		{
			for (int32 SegmentIndex = 0; SegmentIndex < Parameters.InPoints->Num(); ++SegmentIndex)
			{
				PCGSegmentSlicerHelpers::Process(Parameters, Settings->GrammarSelection.GrammarString, Settings->bShouldFlipAxis, SegmentIndex);
			}
		}

		if (!Parameters.OutPoints->IsEmpty())
		{
			// Set the extremity neighbor indexes
			if (Parameters.ExtremityNeighborIndexAttribute)
			{
				// Go 2 by 2
				check(Parameters.CornerIndexes.Num() % 2 == 0);

				for (int32 j = 0; j < Parameters.CornerIndexes.Num(); j += 2)
				{
					const int32 PreviousModuleIndex = Parameters.CornerIndexes[j == 0 ? Parameters.CornerIndexes.Num() - 1 : j - 1];
					const int32 NextModuleIndex = Parameters.CornerIndexes[j == Parameters.CornerIndexes.Num() - 2 ? 0 : j + 2];

					Parameters.ExtremityNeighborIndexAttribute->SetValue((*Parameters.OutPoints)[Parameters.CornerIndexes[j]].MetadataEntry, PreviousModuleIndex);
					Parameters.ExtremityNeighborIndexAttribute->SetValue((*Parameters.OutPoints)[Parameters.CornerIndexes[j+1]].MetadataEntry, NextModuleIndex);
				}
			}

			FPCGTaggedData& Output = Outputs.Emplace_GetRef(Input);
			Output.Data = OutputPointData;
		}
	}

	if (Settings->bForwardAttributesFromModulesInfo && ModuleInfoParamData)
	{
		MatchAndSetAttributes(Inputs, Outputs, ModuleInfoParamData, Settings);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

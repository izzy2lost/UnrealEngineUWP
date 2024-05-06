// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGRandomChoice.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGRandomChoiceElement"

UPCGRandomChoiceSettings::UPCGRandomChoiceSettings()
{
	bUseSeed = true;
}

#if WITH_EDITOR
FName UPCGRandomChoiceSettings::GetDefaultNodeName() const
{
	return FName(TEXT("RandomChoice"));
}

FText UPCGRandomChoiceSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Random Choice");
}

FText UPCGRandomChoiceSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Chooses points randomly through ratio or a fixed number of points.\n"
		"Chosen/Discarded Points will be in the same order than they appear in the input data.");
}
#endif

EPCGDataType UPCGRandomChoiceSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	// We have dynamic pins to true because we can change the number of output pins.
	// But the output type is fully defined (point) so just return the pin allowed type.
	check(InPin);
	return InPin->Properties.AllowedTypes;
}

TArray<FPCGPinProperties> UPCGRandomChoiceSettings::InputPinProperties() const
{
	return Super::DefaultPointInputPinProperties();
}

TArray<FPCGPinProperties> UPCGRandomChoiceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGRandomChoiceConstants::ChosenPointsLabel,
		EPCGDataType::Point,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true);

	if (bOutputDiscardedPoints)
	{
		PinProperties.Emplace(PCGRandomChoiceConstants::DiscardedPointsLabel,
			EPCGDataType::Point,
			/*bAllowMultipleConnections=*/true,
			/*bAllowMultipleData=*/true);
	}

	return PinProperties;
}

FPCGElementPtr UPCGRandomChoiceSettings::CreateElement() const
{
	return MakeShared<FPCGRandomChoiceElement>();
}

bool FPCGRandomChoiceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGRandomChoiceElement::Execute);

	check(Context);

	const UPCGRandomChoiceSettings* Settings = Context->GetInputSettings<UPCGRandomChoiceSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (int i = 0; i < Inputs.Num(); ++i)
	{
		const FPCGTaggedData& CurrentInput = Inputs[i];

		const UPCGPointData* InputPointData = Cast<UPCGPointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, FText::Format(LOCTEXT("InvalidPointData", "Input {0} is not point data"), i));
			continue;
		}

		const TArray<FPCGPoint>& InPoints = InputPointData->GetPoints();

		int NumOfElementsToKeep = 0;

		if (Settings->bFixedMode)
		{
			NumOfElementsToKeep = FMath::Clamp(Settings->FixedNumber, 0, InPoints.Num());
		}
		else
		{
			NumOfElementsToKeep = FMath::CeilToInt(InPoints.Num() * FMath::Clamp(Settings->Ratio, 0, 1));
		}

		auto CreateData = [InputPointData, &Outputs, &CurrentInput](const int NumPoints, const FName PinLabel) -> TArray<FPCGPoint>*
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(CurrentInput);
			UPCGPointData* OutPointData = NewObject<UPCGPointData>();
			OutPointData->InitializeFromData(InputPointData);

			TArray<FPCGPoint>& Points = OutPointData->GetMutablePoints();
			Points.Reserve(NumPoints);

			Output.Data = OutPointData;
			Output.Pin = PinLabel;

			return &Points;
		};

		if (NumOfElementsToKeep == 0)
		{
			// We keep no points, forward the input to the Discarded Points, and create an empty point data on chosen for parity.
			if (Settings->bOutputDiscardedPoints)
			{
				FPCGTaggedData& DiscardedOutput = Outputs.Add_GetRef(CurrentInput);
				DiscardedOutput.Pin = PCGRandomChoiceConstants::DiscardedPointsLabel;
			}

			CreateData(0, PCGRandomChoiceConstants::ChosenPointsLabel);
			continue;
		}
		else if (NumOfElementsToKeep == InPoints.Num())
		{
			// We keep all the points, forward the input to the Chosen Points, and create an empty point data on discarded for parity.
			FPCGTaggedData& ChosenOutput = Outputs.Add_GetRef(CurrentInput);
			ChosenOutput.Pin = PCGRandomChoiceConstants::ChosenPointsLabel;

			if (Settings->bOutputDiscardedPoints)
			{
				CreateData(0, PCGRandomChoiceConstants::DiscardedPointsLabel);
			}

			continue;
		}

		// TODO: While shuffling is the most intuitive way of selecting randomly points, it is still inefficient in memory, especially if we have a lot of points and want to select just a few of them.
		// Perhaps we could chose another algorithm in that case.
		// Like for example:
		// * Pick a number in [0, n[
		// * Pick a number in[0, n - 1[, then for each previously selected number, if it's larger, add +1
		// It's O(n) cpu + O(n) memory vs O(s^2) cpu + O(s) memory. (n = total number of points, s = number of points to keep)
		int32 Seed = Context->GetSeed();
		if (ensure(!InPoints.IsEmpty()))
		{
			// Combine the seed with the first point so that multiple data produces different results.
			Seed = PCGHelpers::ComputeSeed(Seed, InPoints[0].Seed);
		}

		FRandomStream RandStream(Seed);
		TArray<int32> ShuffledIndexes;
		ShuffledIndexes.Reserve(InPoints.Num());

		// Instead of swapping directly, we swap the indexes since it cheaper than copying points around
		for (int j = 0; j < InPoints.Num(); ++j)
		{
			ShuffledIndexes.Add(j);
		}

		// We only have to shuffle until we reached the number of elements to keep.
		for (int j = 0; j < NumOfElementsToKeep; ++j)
		{
			const int RandomElement = RandStream.RandRange(j, InPoints.Num() - 1);
			if (RandomElement != j)
			{
				Swap(ShuffledIndexes[j], ShuffledIndexes[RandomElement]);
			}
		}

		auto AddToOutput = [&CreateData, &InPoints, &ShuffledIndexes](const int StartIndex, const int Count, const FName Label)
		{
			// Order needs to be stable to sort this part of the array
			Algo::Sort(MakeArrayView(ShuffledIndexes.GetData() + StartIndex, Count));

			TArray<FPCGPoint>* Points = CreateData(Count, Label);
			check(Points);
			for (int j = 0; j < Count; ++j)
			{
				Points->Add(InPoints[ShuffledIndexes[StartIndex + j]]);
			}
		};

		AddToOutput(0, NumOfElementsToKeep, PCGRandomChoiceConstants::ChosenPointsLabel);

		if (Settings->bOutputDiscardedPoints)
		{
			AddToOutput(NumOfElementsToKeep, InPoints.Num() - NumOfElementsToKeep, PCGRandomChoiceConstants::DiscardedPointsLabel);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
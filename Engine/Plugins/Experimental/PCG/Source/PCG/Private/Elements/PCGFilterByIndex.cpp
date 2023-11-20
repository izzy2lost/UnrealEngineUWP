// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByIndex.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/Parsing/PCGIndexing.h"
#include "Helpers/Parsing/PCGParsing.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGFilterByIndex)

#define LOCTEXT_NAMESPACE "PCGFilterByIndexElement"

namespace PCGFilterByIndexConstants
{
	const FName NodeName = FName(TEXT("FilterByIndex"));
	const FText NodeTitle = LOCTEXT("NodeTitle", "Filter By Index");

	// After so many characters on the node, it will truncate with an ellipsis ...
	static constexpr int32 IndexExpressionTruncation = 12;
}

#if WITH_EDITOR
FName UPCGFilterByIndexSettings::GetDefaultNodeName() const
{
	return PCGFilterByIndexConstants::NodeName;
}

FText UPCGFilterByIndexSettings::GetDefaultNodeTitle() const
{
	return PCGFilterByIndexConstants::NodeTitle;
}

FText UPCGFilterByIndexSettings::GetNodeTooltipText() const
{
	return LOCTEXT("FilterByIndexNodeTooltip", "Filters data in the collection according to user selected indices");
}
#endif

EPCGDataType UPCGFilterByIndexSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);

	if (!InPin->IsOutputPin())
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	// Output pin narrows to union of inputs on first pin
	const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
}

FName UPCGFilterByIndexSettings::AdditionalTaskName() const
{
	FString NodeName = PCGFilterByIndexConstants::NodeTitle.ToString();
	NodeName += TEXT(" : ");
	NodeName += SelectedIndices.Len() <= PCGFilterByIndexConstants::IndexExpressionTruncation ? SelectedIndices : SelectedIndices.Left(PCGFilterByIndexConstants::IndexExpressionTruncation - 3) + TEXT("...");
	return FName(NodeName);
}

TArray<FPCGPinProperties> UPCGFilterByIndexSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInFilterLabel, EPCGDataType::Any);
	PinProperties.Emplace(PCGPinConstants::DefaultOutFilterLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGFilterByIndexSettings::CreateElement() const
{
	return MakeShared<FPCGFilterByIndexElement>();
}

bool FPCGFilterByIndexElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGFilterByIndexElement::ExecuteInternal);

	const UPCGFilterByIndexSettings* Settings = Context->GetInputSettings<UPCGFilterByIndexSettings>();
	check(Settings);

	if (Context->InputData.GetInputs().IsEmpty())
	{
		return true;
	}

	PCGIndexing::FPCGIndexCollection FilteredIndices(Context->InputData.GetInputs().Num());
	// Parse the indices and switch through possible issues.
	switch (PCGParser::ParseIndexRanges(FilteredIndices, Settings->SelectedIndices))
	{
		// Error cases are caught for early out.
		case PCGParser::EPCGParserResult::InvalidCharacter:
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("ErrorInvalidCharacter", "Invalid character in index selection string."));
			return true;

		case PCGParser::EPCGParserResult::InvalidExpression:
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("ErrorInvalidExpression", "Invalid expression in index selection string."));
			return true;

		// If the expression is empty, treat it as though there are simply no selected indices. But, also log.
		case PCGParser::EPCGParserResult::EmptyExpression:
			PCGE_LOG_C(Log, LogOnly, Context, LOCTEXT("WarningEmptyExpression", "Empty expression in index selection string."));
			break;

		case PCGParser::EPCGParserResult::Success:
			break;

		default:
			checkNoEntry();
			return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	Outputs = Context->InputData.GetInputs();

	for (int Index = 0; Index < Outputs.Num(); ++Index)
	{
		bool bIndexIsFiltered = FilteredIndices.ContainsIndex(Index);

		bIndexIsFiltered = Settings->bInvertFilter ? !bIndexIsFiltered : bIndexIsFiltered;

		if (bIndexIsFiltered)
		{
			Outputs[Index].Pin = PCGPinConstants::DefaultInFilterLabel;
		}
		else
		{
			Outputs[Index].Pin = PCGPinConstants::DefaultOutFilterLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
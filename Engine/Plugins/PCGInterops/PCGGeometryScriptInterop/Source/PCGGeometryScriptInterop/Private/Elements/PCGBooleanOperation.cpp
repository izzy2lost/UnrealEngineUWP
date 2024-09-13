// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGBooleanOperation.h"

#include "PCGContext.h"
#include "Data/PCGDynamicMeshData.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGBooleanOperationElement"

namespace PCGBooleanOperation
{
	static const FName InputAPinLabel = TEXT("InA");
	static const FName InputBPinLabel = TEXT("InB");
}

#if WITH_EDITOR
FName UPCGBooleanOperationSettings::GetDefaultNodeName() const
{
	return FName(TEXT("BooleanOperation"));
}

FText UPCGBooleanOperationSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Boolean Operation");
}

FText UPCGBooleanOperationSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Do a boolean operation between 2 dynamic meshes.");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGBooleanOperationSettings::CreateElement() const
{
	return MakeShared<FPCGBooleanOperationElement>();
}

TArray<FPCGPinProperties> UPCGBooleanOperationSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace_GetRef(PCGBooleanOperation::InputAPinLabel, EPCGDataType::DynamicMesh).SetRequiredPin();
	Properties.Emplace_GetRef(PCGBooleanOperation::InputBPinLabel, EPCGDataType::DynamicMesh).SetRequiredPin();
	return Properties;
}

bool FPCGBooleanOperationElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGBooleanOperationElement::Execute);

	check(InContext);

	const UPCGBooleanOperationSettings* Settings = InContext->GetInputSettings<UPCGBooleanOperationSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> InputsA = InContext->InputData.GetInputsByPin(PCGBooleanOperation::InputAPinLabel);
	const TArray<FPCGTaggedData> InputsB = InContext->InputData.GetInputsByPin(PCGBooleanOperation::InputBPinLabel);

	if (InputsA.IsEmpty() || InputsB.IsEmpty())
	{
		return true;
	}

	if (Settings->bBoolEachAWithEveryB && InputsA.Num() != 1 && InputsB.Num() != 1 && InputsA.Num() != InputsB.Num())
	{
		PCGLog::LogErrorOnGraph(LOCTEXT("MismatchNumInputs", "There is a mismatch between the number of inputs. If BoolEachAWithEveryB is false, we only support N:1, 1:N and N:N operations"), InContext);
		return true;
	}

	// We can only steal the input if input A is not used multiple times.
	const bool bCanStealInput = InputsB.Num() == 1 || (!Settings->bBoolEachAWithEveryB && InputsA.Num() == InputsB.Num());
	const int32 NumIterations = Settings->bBoolEachAWithEveryB ? InputsA.Num() * InputsB.Num() : FMath::Max(InputsA.Num(), InputsB.Num());

	for (int32 i = 0; i < NumIterations; ++i)
	{
		const FPCGTaggedData& InputA = InputsA[Settings->bBoolEachAWithEveryB ? i / InputsB.Num() : i % InputsA.Num()];
		const FPCGTaggedData& InputB = InputsB[Settings->bBoolEachAWithEveryB ? i % InputsB.Num() : i % InputsB.Num()];

		const UPCGDynamicMeshData* InputMeshA = Cast<const UPCGDynamicMeshData>(InputA.Data);
		const UPCGDynamicMeshData* InputMeshB = Cast<const UPCGDynamicMeshData>(InputB.Data);

		if (!InputMeshA || !InputMeshB)
		{
			PCGLog::InputOutput::LogInvalidInputDataError(InContext);
			continue;
		}

		UPCGDynamicMeshData* OutputMeshData = bCanStealInput ? CopyOrSteal(InputA, InContext) : CastChecked<UPCGDynamicMeshData>(InputMeshA->DuplicateData(InContext));
		check(OutputMeshData);

		// Second mesh is required to be non const, but it won't be modified (Geometry Script API is not const friendly), hence the const_cast.
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(OutputMeshData->GetMutableDynamicMesh(), FTransform::Identity,
	const_cast<UDynamicMesh*>(InputMeshB->GetDynamicMesh()), FTransform::Identity,
			Settings->BooleanOperation, Settings->BooleanOperationOptions);
		
		FPCGTaggedData& OutputData = InContext->OutputData.TaggedData.Emplace_GetRef(InputA);
		if (Settings->TagInheritanceMode == EPCGBooleanOperationTagInheritanceMode::B)
		{
			OutputData.Tags = InputB.Tags;
		}
		else if (Settings->TagInheritanceMode == EPCGBooleanOperationTagInheritanceMode::Both)
		{
			OutputData.Tags.Append(InputB.Tags);
		}

		OutputData.Data = OutputMeshData;
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSortPoints.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGSortPointsElement"

FPCGElementPtr UPCGSortPointsSettings::CreateElement() const
{
	return MakeShared<FPCGSortPointsElement>();
}

bool FPCGSortPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSortPointsElement::Execute);
	
	check(Context);

	const UPCGSortPointsSettings* Settings = Context->GetInputSettings<UPCGSortPointsSettings>();
	check(Settings);
	
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (FPCGTaggedData& CurrentInput : Inputs)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSortPointsElement::InputLoop);

		const FPCGTaggedData& Input = CurrentInput;

		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);

		if (PointData) 
		{
			const TArray<FPCGPoint>& InputPoints = PointData->GetPoints();
			FPCGAttributePropertyInputSelector InputSource = Settings->InputSource.CopyAndFixLast(PointData);

			FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
			UPCGPointData* OutputData = NewObject<UPCGPointData>();
			OutputData->InitializeFromData(PointData);

			TArray<FPCGPoint>& OutputPoints = OutputData->GetMutablePoints();
			OutputPoints = InputPoints;
			Output.Data = OutputData;

			TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSource);
			TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(PointData, InputSource);
			
			if (Accessor.IsValid() && Keys.IsValid())
			{
				PCGAttributeAccessorHelpers::SortByAttribute(*Accessor, *Keys, OutputPoints, Settings->SortMethod == EPCGSortMethod::Ascending);
			}
			else
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidAccessor","Attribute does not exist or cannot be sorted."));
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComputeCommon.h"

#include "PCGParamData.h"
#include "Data/PCGPointData.h"

namespace PCGComputeHelpers
{
	int GetElementCount(const UPCGData* InData)
	{
		if (const UPCGPointData* PointData = Cast<UPCGPointData>(InData))
		{
			return PointData->GetPoints().Num();
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData))
		{
			if (const UPCGMetadata* Metadata = ParamData->ConstMetadata())
			{
				return Metadata->GetItemCountForChild();
			}
		}

		return 0;
	}

	bool IsTypeAllowedAsInput(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedInputTypes) == PCGComputeConstants::AllowedInputTypes;
	}

	bool IsTypeAllowedAsOutput(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedOutputTypes) == PCGComputeConstants::AllowedOutputTypes;
	}

	bool IsTypeAllowedInDataCollection(EPCGDataType Type)
	{
		return (Type | PCGComputeConstants::AllowedDataCollectionTypes) == PCGComputeConstants::AllowedDataCollectionTypes;
	}
}

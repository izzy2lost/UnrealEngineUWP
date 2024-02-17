// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowContextCachingFactory.h"

#include "ChaosLog.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Misc/MessageDialog.h"

namespace Dataflow
{
	FContextCachingFactory* FContextCachingFactory::Instance = nullptr;

	FContextCacheElementBase* FContextCachingFactory::Serialize(FArchive& Ar, FContextCacheData&& Element)
	{
		FContextCacheElementBase* RetVal = nullptr;
		if (CachingMap.Contains(Element.Type))
		{
			RetVal = CachingMap[Element.Type](Ar, Element.Data);
			if (Ar.IsSaving())
			{
				check(RetVal == nullptr);
			}
			else if( Ar.IsLoading())
			{
				check(RetVal != nullptr);
			}
		}
		else
		{
			UE_LOG(LogChaos, Warning,
				TEXT("Warning : Dataflow missing context chaching callback type(%s)"), *Element.Type.ToString());
		}
		return RetVal;
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDTraceImplicitObjectProcessor.h"

#include "Chaos/ImplicitObject.h"
#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDImplicitObjectDataWrapper.h"
#include "Serialization/MemoryReader.h"
#include "Trace/ChaosVDTraceProvider.h"

using FChaosVDImplicitObjectWrapper = FChaosVDImplicitObjectDataWrapper<Chaos::FImplicitObjectPtr, Chaos::FChaosArchive>;

FChaosVDTraceImplicitObjectProcessor::FChaosVDTraceImplicitObjectProcessor(): IChaosVDDataProcessor(FChaosVDImplicitObjectWrapper::WrapperTypeName)
{
}

bool FChaosVDTraceImplicitObjectProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	if (!ensure(ProviderSharedPtr->GetNameTable().IsValid()))
	{
		return false;
	}

	FChaosVDImplicitObjectWrapper WrappedGeometryData;
	const bool bSuccess = ReadDataFromBuffer<FChaosVDImplicitObjectWrapper, Chaos::FChaosArchive>(InData, WrappedGeometryData, ProviderSharedPtr->GetNameTable().ToSharedRef());

	if (bSuccess)
	{
		if (TSharedPtr<FChaosVDRecording> Recording = ProviderSharedPtr->GetRecordingForSession())
		{
			Recording->AddImplicitObject(WrappedGeometryData.Hash, WrappedGeometryData.ImplicitObject);
		}
	}

	return bSuccess;
}

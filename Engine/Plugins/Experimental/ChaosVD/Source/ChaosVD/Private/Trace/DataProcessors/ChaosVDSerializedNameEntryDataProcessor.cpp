// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDSerializedNameEntryDataProcessor.h"

#include "ChaosVisualDebugger/ChaosVDSerializedNameTable.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Serialization/MemoryReader.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDSerializedNameEntryDataProcessor::FChaosVDSerializedNameEntryDataProcessor()
	: IChaosVDDataProcessor(Chaos::VisualDebugger::FChaosVDSerializedNameEntry::WrapperTypeName)
{
}

bool FChaosVDSerializedNameEntryDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	Chaos::VisualDebugger::FChaosVDSerializedNameEntry NameEntry;
	
	FMemoryReader MemReader(InData);
	MemReader << NameEntry;

	ProviderSharedPtr->GetNameTable()->AddNameToTable(NameEntry);

	return true;
}

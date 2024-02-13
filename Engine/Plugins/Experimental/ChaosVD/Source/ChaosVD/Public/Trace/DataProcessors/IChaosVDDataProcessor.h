// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FChaosVDTraceProvider;

namespace Chaos::VisualDebugger
{
	template<typename TDataToSerialize>
	bool ReadDataFromBuffer(const TArray<uint8>& InDataBuffer, TDataToSerialize& Data, const TSharedRef<FChaosVDSerializableNameTable>& InNameTableInstance)
	{
		FChaosVDMemoryReader MemReader(InDataBuffer, InNameTableInstance);
		MemReader.SetShouldSkipUpdateCustomVersion(true);

		Data.Serialize(MemReader);

		return !MemReader.IsError() && !MemReader.IsCriticalError();
	}

	template<typename TDataToSerialize, typename TArchive>
	bool ReadDataFromBuffer(const TArray<uint8>& InDataBuffer, TDataToSerialize& Data, const TSharedRef<FChaosVDSerializableNameTable>& InNameTableInstance)
	{
		FChaosVDMemoryReader MemReader(InDataBuffer, InNameTableInstance);
		TArchive Ar(MemReader);
		Ar.SetShouldSkipUpdateCustomVersion(true);

		Data.Serialize(Ar);

		return !Ar.IsError() && !Ar.IsCriticalError();
	}
}

/** Interface for all used for any class that is able to process traced Chaos Visual Debugger binary data */
class IChaosVDDataProcessor
{
public:
	virtual ~IChaosVDDataProcessor() = default;

	explicit IChaosVDDataProcessor(FStringView InCompatibleType) : CompatibleType(InCompatibleType)
	{
	}

	/** Type name this data processor can interpret */
	FStringView GetCompatibleTypeName() const { return CompatibleType; }
	/** Called with the raw serialized data to be processed */
	virtual bool ProcessRawData(const TArray<uint8>& InData) { return false; }

	/** Sets the Trace Provider that is storing the data being analyzed */
	void SetTraceProvider(const TSharedPtr<FChaosVDTraceProvider>& InProvider) { TraceProvider = InProvider; }

protected:
	TWeakPtr<FChaosVDTraceProvider> TraceProvider;
	FStringView CompatibleType;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDSerializedNameTable.h"
#include "HAL/Platform.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/SharedPointer.h"

class FName;
class FArchive;

namespace Chaos::VisualDebugger
{
	FORCEINLINE void NameTableFNameSerializer(FArchive& Ar ,FName& Name, const TSharedRef<FChaosVDSerializableNameTable>& InNameTable)
	{
		// Serialize the FName as a CVD Name ID
		if (Ar.IsLoading())
		{
			uint64 NameID = 0;
			Ar << NameID;
				
			Name = InNameTable->GetNameFromTable(NameID);
		}
		else
		{
			uint64 NameID = InNameTable->AddNameToTable(Name);
			Ar << NameID;
		}
	}

	/** Preferred memory writer for CVD recorded structs. It has support for FName serialization and de-duplication */
	class FChaosVDMemoryWriter final : public FMemoryWriter
	{
	public:
		FChaosVDMemoryWriter(TArray<uint8>& InBytes, const TSharedRef<FChaosVDSerializableNameTable>& InNameTableInstance)
			: FMemoryWriter(InBytes), NameTableInstance(InNameTableInstance)
		{
		}

		virtual FArchive& operator<<(FName& Name) override
		{
			NameTableFNameSerializer(*this, Name, NameTableInstance);
			return *this;
		}

		TSharedRef<FChaosVDSerializableNameTable> NameTableInstance;
	};

	/** Preferred memory reader for CVD recorded structs. It has support for FName serialization and de-duplication */
	class FChaosVDMemoryReader final : public FMemoryReader
	{
	public:
		FChaosVDMemoryReader(const TArray<uint8>& InBytes, const TSharedRef<FChaosVDSerializableNameTable>& InNameTableInstance)
			: FMemoryReader(InBytes), NameTableInstance(InNameTableInstance)
		{
		}

		virtual FArchive& operator<<(FName& Name) override
		{
			NameTableFNameSerializer(*this, Name, NameTableInstance);
			return *this;
		}

		TSharedRef<FChaosVDSerializableNameTable> NameTableInstance;
	};
}

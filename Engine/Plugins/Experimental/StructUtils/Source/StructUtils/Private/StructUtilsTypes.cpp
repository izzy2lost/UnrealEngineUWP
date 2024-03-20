// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructUtilsTypes.h"
#include "Serialization/ArchiveCrc32.h"
#include "Hash/CityHash.h"
#include "StructView.h"
#include "InstancedStruct.h"
#include "SharedStruct.h"
#include "Serialization/ArchiveUObject.h"


namespace UE::StructUtils
{
	uint32 GetStructCrc32(const UScriptStruct& ScriptStruct, const uint8* StructMemory, const uint32 CRC /*= 0*/)
	{
		FArchiveCrc32 Ar(HashCombine(CRC, PointerHash(&ScriptStruct)));
		if (StructMemory)
		{
			UScriptStruct& NonConstScriptStruct = const_cast<UScriptStruct&>(ScriptStruct);
			NonConstScriptStruct.SerializeItem(Ar, const_cast<uint8*>(StructMemory), nullptr);
		}
		return Ar.GetCrc();
	}

	template<typename T>
	uint32 GetStructCrc32Helper(const T& Struct, const uint32 CRC /*= 0*/)
	{
		if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
		{
			return GetStructCrc32(*ScriptStruct, Struct.GetMemory(), CRC);
		}
		return 0;
	}

	uint32 GetStructCrc32(const FStructView& StructView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(StructView, CRC);
	}

	uint32 GetStructCrc32(const FConstStructView& StructView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(StructView, CRC);
	}

	uint32 GetStructCrc32(const FSharedStruct& SharedView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(SharedView, CRC);
	}

	uint32 GetStructCrc32(const FConstSharedStruct& SharedView, const uint32 CRC /*= 0*/)
	{
		return GetStructCrc32Helper(SharedView, CRC);
	}

	class FArchiveCityHash64 : public FArchiveUObject
	{
	public:
		explicit FArchiveCityHash64(const uint64 InHash = 0)
			: Hash(InHash)
		{}

		/**
		 * @return The hash computed so far.
		 */
		uint64 GetHash() const { return Hash; }

		//~ Begin FArchive Interface
		virtual void Serialize(void* Data, int64 Num) override
		{
			const char* BytePointer = static_cast<const char*>(Data);
			while (Num > 0)
			{
				const int32 BytesToHash = static_cast<int32>(FMath::Min(Num, int64(MAX_int32)));

				Hash = CityHash64WithSeed(BytePointer, BytesToHash, Hash);

				Num -= BytesToHash;
				BytePointer += BytesToHash;
			}
		}

		virtual FArchive& operator<<(class FName& Name) override
		{
			Hash = HashString(Name.ToString(), Hash);
			return *this;
		}

		virtual FArchive& operator<<(class UObject*& Object) override
		{
			Hash = HashString(GetPathNameSafe(Object), Hash);
			return *this;
		}

		virtual FString GetArchiveName() const override { return TEXT("FArchiveCityHash64"); }
		//~ End FArchive Interface

		FORCEINLINE static uint64 HashString(const FString& InString, const uint32 InHash = 0)
		{
			return CityHash64WithSeed(reinterpret_cast<const char*>(*InString), InString.GetAllocatedSize(), InHash);
		}

	private:
		uint64 Hash;
	};

	uint64 GetStructHash64(const UScriptStruct& ScriptStruct, const uint8* StructMemory)
	{
		const uint64 BaseHash = FArchiveCityHash64::HashString(ScriptStruct.GetPathName());

		const bool bHasTypeHash = ScriptStruct.GetCppStructOps()->HasGetTypeHash();
		if (bHasTypeHash)
		{
			const uint32 StructHash = ScriptStruct.GetStructTypeHash(StructMemory);
			return CityHash128to64(Uint128_64(BaseHash, StructHash));
		}
		else if (StructMemory)
		{
			FArchiveCityHash64 Ar(BaseHash);
			UScriptStruct& NonConstScriptStruct = const_cast<UScriptStruct&>(ScriptStruct);
			NonConstScriptStruct.SerializeItem(Ar, const_cast<uint8*>(StructMemory), nullptr);

			return Ar.GetHash();
		}
		
		return BaseHash;
	}

	template<typename T>
	uint64 GetStructHash64Helper(const T& Struct)
	{
		if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
		{
			return GetStructHash64(*ScriptStruct, Struct.GetMemory());
		}
		return 0;
	}

	uint64 GetStructHash64(const FStructView& StructView)
	{
		return GetStructHash64Helper(StructView);
	}

	uint64 GetStructHash64(const FConstStructView& StructView)
	{
		return GetStructHash64Helper(StructView);
	}

	uint64 GetStructHash64(const FSharedStruct& SharedView)
	{
		return GetStructHash64Helper(SharedView);
	}

	uint64 GetStructHash64(const FConstSharedStruct& SharedView)
	{
		return GetStructHash64Helper(SharedView);
	}

	namespace Private
	{
#if WITH_ENGINE && WITH_EDITOR
		const UUserDefinedStruct* GStructureToReinstance = nullptr;
		UObject* GCurrentReinstanceOuterObject = nullptr;

		FStructureToReinstanceScope::FStructureToReinstanceScope(const UUserDefinedStruct* StructureToReinstance)
		{
			OldStructureToReinstance = GStructureToReinstance;
			GStructureToReinstance = StructureToReinstance;
		}
	
		FStructureToReinstanceScope::~FStructureToReinstanceScope()
		{
			GStructureToReinstance = OldStructureToReinstance;
		}

		FCurrentReinstanceOuterObjectScope::FCurrentReinstanceOuterObjectScope(UObject* CurrentReinstanceOuterObject)
		{
			OldCurrentReinstanceOuterObject = GCurrentReinstanceOuterObject;
			GCurrentReinstanceOuterObject = CurrentReinstanceOuterObject;
		}

		FCurrentReinstanceOuterObjectScope::~FCurrentReinstanceOuterObjectScope()
		{
			GCurrentReinstanceOuterObject = OldCurrentReinstanceOuterObject;
		}

		const UUserDefinedStruct* GetStructureToReinstance()
		{
			return GStructureToReinstance;
		}
	
		UObject* GetCurrentReinstanceOuterObject()
		{
			return GCurrentReinstanceOuterObject;
		}

#endif
	}
}

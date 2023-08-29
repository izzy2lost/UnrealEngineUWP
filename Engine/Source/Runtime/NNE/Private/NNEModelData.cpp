// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEModelData.h"

#include "EditorFramework/AssetImportData.h"
#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeFormat.h"
#include "Serialization/CustomVersion.h"
#include "UObject/WeakInterfacePtr.h"

#if WITH_EDITOR
#include "Containers/StringFwd.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataRequestOwner.h"
#include "Memory/CompositeBuffer.h"
#endif // WITH_EDITOR

namespace UE::NNE::ModelData
{
	enum Version : uint32
	{
		V0 = 0, // Initial
		V1 = 1, // TargetRuntimes and AssetImportData
		V2 = 2, // Re-arrange fields and store only ModelData in cooked assets
		// New versions can be added above this line
		VersionPlusOne,
		Latest = VersionPlusOne - 1
	};

	const FGuid GUID(0x9513202e, 0xeba1b279, 0xf17fe5ba, 0xab90c3f2);
	FCustomVersionRegistration NNEModelDataVersion(GUID, Version::Latest, TEXT("NNEModelDataVersion"));// Always save with the latest version

	const uint32 DDCAssetVersion = 0; // Increase this value to force rebuilding cache entries

	FString GetRuntimesAsString(TArrayView<const FString> Runtimes)
	{
		if (Runtimes.Num() == 0)
		{
			return TEXT("All");
		}

		FString RuntimesAsOneString;
		bool bIsFirstRuntime = true;

		for (const FString& Runtime : Runtimes)
		{
			if (!bIsFirstRuntime)
			{
				RuntimesAsOneString += TEXT(", ");
			}
			RuntimesAsOneString += Runtime;
			bIsFirstRuntime = false;
		}
		return RuntimesAsOneString;
	}

#if WITH_EDITOR

	inline FString GetDDCRequestId(const FString& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier)
	{
		// RuntimeName and FileId are embedded to the id to ensure no potential collision between the runtime/assets
		return RuntimeName + "-" + FileId + "-DDCv" + FString::FromInt(DDCAssetVersion) + "-" + ModelDataIdentifier;
	}

	inline UE::DerivedData::FCacheKey CreateCacheKey(const FString& FileId, const FString& RequestId)
	{
		return { UE::DerivedData::FCacheBucket(FWideStringView(*FileId)), FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(RequestId))) };
	}

	inline void PutIntoDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier, FSharedBuffer& Data, uint32 MemoryAlignment)
	{
		FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
		FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

		TArray<UE::DerivedData::FCachePutValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Put-") + RequestId;
		Requests[0].Key = CreateCacheKey(FileIdStr, RequestId);
		Requests[0].Value = UE::DerivedData::FValue::Compress(FCompositeBuffer(MakeSharedBufferFromArray(TArray<uint32>({ MemoryAlignment })), Data));

		UE::DerivedData::FRequestOwner BlockingPutOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().PutValue(Requests, BlockingPutOwner);
		BlockingPutOwner.Wait();
	}

	inline FSharedBuffer GetFromDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier, uint32& OutMemoryAlignment)
	{
		FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
		FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

		TArray<UE::DerivedData::FCacheGetValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Get-") + RequestId;
		Requests[0].Key = CreateCacheKey(FileIdStr, RequestId);

		FSharedBuffer Result;
		UE::DerivedData::FRequestOwner BlockingGetOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().GetValue(Requests, BlockingGetOwner, [&Result, &OutMemoryAlignment](UE::DerivedData::FCacheGetValueResponse&& Response)
		{
			if (Response.Value.HasData() && Response.Value.GetRawSize() > sizeof(uint32))
			{
				FCompressedBufferReader Reader(Response.Value.GetData());
				uint32 MemoryAlignment = ((uint32*)Reader.Decompress(0, sizeof(uint32)).GetData())[0];
				uint64 DataSize = Response.Value.GetRawSize() - sizeof(uint32);

				void* Data = FMemory::Malloc(DataSize, MemoryAlignment);
				if (Reader.TryDecompressTo(FMutableMemoryView(Data, DataSize), sizeof(uint32)))
				{
					OutMemoryAlignment = MemoryAlignment;
					Result = FSharedBuffer::TakeOwnership(Data, DataSize, FMemory::Free);
				}
				else
				{
					FMemory::Free(Data);
				}
			}
		});
		BlockingGetOwner.Wait();
		return Result;
	}

#endif // WITH_EDITOR

	inline FSharedBuffer CreateModelData(const FString& RuntimeName, FString FileType, const TArray<uint8>& FileData, FGuid FileId, const ITargetPlatform* TargetPlatform, uint32& OutMemoryAlignment)
	{
		TWeakInterfacePtr<INNERuntime> NNERuntime = UE::NNE::GetRuntime<INNERuntime>(RuntimeName);
		if (NNERuntime.IsValid())
		{
			if (NNERuntime->CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
			{
				uint32 TmpOutMemoryAlignment = 0;
				TArray<uint8> ModelData = NNERuntime->CreateModelData(FileType, FileData, FileId, TargetPlatform, TmpOutMemoryAlignment);

				// Make sure a runtime does fulfill its own alignment requirement
				checkf(TmpOutMemoryAlignment <= 1 || (((uintptr_t)(const void *)(ModelData.GetData())) % TmpOutMemoryAlignment == 0), TEXT("Runtimes must return ModelData that is aligned with OutMemoryAlignment!"))

				if (ModelData.Num() > 0)
				{
					OutMemoryAlignment = TmpOutMemoryAlignment;
					return MakeSharedBufferFromArray(MoveTemp(ModelData));
				}
			}
		}
		else
		{
			UE_LOG(LogNNE, Error, TEXT("UNNEModelData: No runtime '%s' found. Valid runtimes are: "), *RuntimeName);
			TArrayView<TWeakInterfacePtr<INNERuntime>> Runtimes = UE::NNE::GetAllRuntimes();
			for (int32 i = 0; i < Runtimes.Num(); i++)
			{
				UE_LOG(LogNNE, Error, TEXT("- %s"), *Runtimes[i]->GetRuntimeName());
			}
		}
		return {};
	}

} // UE::NNE::ModelData

void UNNEModelData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add(FAssetRegistryTag("TargetRuntimes", UE::NNE::ModelData::GetRuntimesAsString(GetTargetRuntimes()), FAssetRegistryTag::TT_Alphabetical));
	Super::GetAssetRegistryTags(OutTags);
}

void UNNEModelData::Serialize(FArchive& Ar)
{
	// Store the asset version (no effect in load)
	Ar.UsingCustomVersion(UE::NNE::ModelData::GUID);

	if (Ar.IsSaving())
	{
		bool bWriteModelData = true;
		if (Ar.IsCooking())
		{
			// Optimize storage: FileData is not required anymore because we have the model and can cook it for every runtime
			TArray<FString> TmpTargetRuntimes;
			Ar << TmpTargetRuntimes;
			FString TmpFileType;
			Ar << TmpFileType;
			TArray<uint8> TmpFileData;
			Ar << TmpFileData;
			Ar << FileId;

			// Cooking must recreate all model data but only if file data is still available
			if (FileData.Num() > 0)
			{
				ModelData.Reset();

				// No target runtime means all currently registered ones
				TArray<FString, TInlineAllocator<10>> CookRuntimeNames;
				if (GetTargetRuntimes().IsEmpty())
				{
					for (const TWeakInterfacePtr<INNERuntime>& Runtime : UE::NNE::GetAllRuntimes())
					{
						CookRuntimeNames.Add(Runtime->GetRuntimeName());
					}
				}
				else
				{
					CookRuntimeNames.Append(GetTargetRuntimes());
				}

				for (const FString& RuntimeName : CookRuntimeNames)
				{
					uint32 MemoryAlignment = 0;
					FSharedBuffer CreatedData = UE::NNE::ModelData::CreateModelData(RuntimeName, FileType, FileData, FileId, Ar.GetArchiveState().CookingTarget(), MemoryAlignment);
					if (CreatedData.GetSize() > 0)
					{
						ModelData.Add(RuntimeName, MakeTuple(CreatedData, MemoryAlignment));
#if WITH_EDITOR
						TWeakInterfacePtr<INNERuntime> NNERuntime = UE::NNE::GetRuntime<INNERuntime>(RuntimeName);
						if (NNERuntime.IsValid())
						{
							FString ModelDataIdentifier = NNERuntime->GetModelDataIdentifier(FileType, FileData, FileId, Ar.GetArchiveState().CookingTarget());
							if (ModelDataIdentifier.Len() > 0)
							{
								UE::NNE::ModelData::PutIntoDDC(FileId, RuntimeName, ModelDataIdentifier, CreatedData, MemoryAlignment);
							}
							else
							{
								UE_LOG(LogNNE, Warning, TEXT("UNNEModelData: Runtime '%s' returned an empty string as a ModelDataIdentifier while cooking. GetModelDataIdentifier should always return a valid identifier."), *RuntimeName);
							}
						}
						else
						{
							UE_LOG(LogNNE, Warning, TEXT("UNNEModelData: Runtime '%s' is among the cooked runtimes but instance is invalid."), *RuntimeName);
						}
#endif //WITH_EDITOR
					}
				}
			}
		}
		else
		{
			// Only cooked assets optimize storage
			Ar << TargetRuntimes;
			Ar << FileType;
			Ar << FileData;
			Ar << FileId;

#if WITH_EDITOR
			// In editor (when not cooking), no model data is stored as model data can always be recreated and unnecessary data in subversion control should be avoided
			bWriteModelData = false;
#endif //WITH_EDITOR
		}

		if (bWriteModelData)
		{
			TArray<FString> RuntimeNames;
			ModelData.GetKeys(RuntimeNames);
			int32 NumItems = RuntimeNames.Num();

			Ar << NumItems;
			for (int32 i = 0; i < NumItems; i++)
			{
				Ar << RuntimeNames[i];

				uint32 MemoryAlignment = ModelData[RuntimeNames[i]].Get<1>();
				Ar << MemoryAlignment;

				uint64 DataSize = ModelData[RuntimeNames[i]].Get<0>().GetSize();
				Ar << DataSize;

				Ar.Serialize((void*)ModelData[RuntimeNames[i]].Get<0>().GetData(), DataSize);
			}
		}
		else
		{
			int32 NumItems = 0;
			Ar << NumItems;
		}
	}
	else
	{
		// Read the archive

		TObjectPtr<class UAssetImportData> AssetImportData;
		int32 NumItems;
		FString Name;
		uint32 MemoryAlignment;
		uint64 DataSize;
		TArray<uint8> Data;
		void* RawData;
		int32 Index;

		switch (Ar.CustomVer(UE::NNE::ModelData::GUID))
		{
		case UE::NNE::ModelData::Version::V0:
			TargetRuntimes.Empty();
			Ar << FileType;
			Ar << FileData;
			Ar << FileId;
			Ar << NumItems;
			for (Index = 0; Index < NumItems; Index++)
			{
				Ar << Name;
				Ar << Data;
				ModelData.Add(Name, MakeTuple(MakeSharedBufferFromArray(MoveTemp(Data)), 0));
			}
			UE_LOG(LogNNE, Warning, TEXT("[DEPRECATION] UNNEModelData: The asset %s (v0) is deprecated. Please right-click the asset and select 'Save' to update it to the latest version."), *this->GetName());
			break;

		case UE::NNE::ModelData::Version::V1:
			TargetRuntimes.Empty();
			if (!Ar.IsLoadingFromCookedPackage())
			{
				Ar << TargetRuntimes;
				Ar << AssetImportData;
			}
			Ar << FileType;
			Ar << FileData;
			Ar << FileId;
			Ar << NumItems;
			for (Index = 0; Index < NumItems; Index++)
			{
				Ar << Name;
				Ar << Data;
				ModelData.Add(Name, MakeTuple(MakeSharedBufferFromArray(MoveTemp(Data)), 0));
			}
			UE_LOG(LogNNE, Warning, TEXT("[DEPRECATION] UNNEModelData: The asset %s (v1) is deprecated. Please right-click the asset and select 'Save' to update it to the latest version."), *this->GetName());
			break;

		case UE::NNE::ModelData::Version::V2:
			Ar << TargetRuntimes;
			Ar << FileType;
			Ar << FileData;
			Ar << FileId;
			Ar << NumItems;
			for (Index = 0; Index < NumItems; Index++)
			{
				Ar << Name;
				Ar << MemoryAlignment;
				Ar << DataSize;
				RawData = FMemory::Malloc(DataSize, MemoryAlignment);
				Ar.Serialize(RawData, DataSize);
				ModelData.Add(Name, MakeTuple(FSharedBuffer::TakeOwnership(RawData, DataSize, FMemory::Free), MemoryAlignment));
			}
			break;

		default:
			UE_LOG(LogNNE, Error, TEXT("UNNEModelData: Unknown asset version %d: Deserialisation failed, please reimport the original model."), Ar.CustomVer(UE::NNE::ModelData::GUID));
			break;
		}
	}
}

void UNNEModelData::Init(const FString& Type, TConstArrayView<uint8> Buffer)
{
	TargetRuntimes.Empty();
	FileType = Type;
	FileData = Buffer;
	FPlatformMisc::CreateGuid(FileId);
	ModelData.Empty();
}

TArrayView<const FString> UNNEModelData::GetTargetRuntimes() const 
{ 
	return TargetRuntimes;
}

void UNNEModelData::SetTargetRuntimes(TArrayView<const FString> RuntimeNames)
{
	TargetRuntimes = RuntimeNames;

	if (RuntimeNames.Num() > 0)
	{
		TArray<FString, TInlineAllocator<10>> CookedRuntimes;
		ModelData.GetKeys(CookedRuntimes);
		for (const FString& Runtime : CookedRuntimes)
		{
			if (!TargetRuntimes.Contains(Runtime))
			{
				ModelData.Remove(Runtime);
			}
		}
		ModelData.Compact();
	}
}

FString UNNEModelData::GetFileType()
{
	return FileType;
}

TConstArrayView<uint8> UNNEModelData::GetFileData()
{
	return FileData;
}

void UNNEModelData::ClearFileDataAndFileType()
{
	FileType = "";
	FileData.Empty();
}

FGuid UNNEModelData::GetFileId()
{
	return FileId;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNEModelData::GetModelData(const FString& RuntimeName)
{
	// Check model data is supporting the requested target runtime
	TArrayView<const FString> TargetRuntimesNames = GetTargetRuntimes();
	if (!TargetRuntimesNames.IsEmpty() && !TargetRuntimesNames.Contains(RuntimeName))
	{
		UE_LOG(LogNNE, Error, TEXT("UNNEModelData: Runtime '%s' is not among the target runtimes. Target runtimes are: "), *RuntimeName);
		for (const FString& TargetRuntimesName : TargetRuntimesNames)
		{
			UE_LOG(LogNNE, Error, TEXT("- %s"), *TargetRuntimesName);
		}
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	// Check if we have a local cache hit
	TTuple<FSharedBuffer, uint32>* LocalData = ModelData.Find(RuntimeName);
	if (LocalData)
	{
		return MakeShared<UE::NNE::FSharedModelData>(LocalData->Get<0>());
	}

	// After this point FileData is required to either get the cache id or recreate it from scratch
	if (FileData.Num() < 1)
	{
		UE_LOG(LogNNE, Error, TEXT("UNNEModelData: Cannot create model data from empty file data."));
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

#if WITH_EDITOR
	TWeakInterfacePtr<INNERuntime> NNERuntime = UE::NNE::GetRuntime<INNERuntime>(RuntimeName);
	if (!NNERuntime.IsValid())
	{
		UE_LOG(LogNNE, Error, TEXT("UNNEModelData: Runtime '%s' is among the target runtimes but instance is invalid."), *RuntimeName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	FString ModelDataIdentifier = NNERuntime->GetModelDataIdentifier(FileType, FileData, FileId, nullptr);
	if (ModelDataIdentifier.Len() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("UNNEModelData: Runtime '%s' returned an empty string as a ModelDataIdentifier. GetModelDataIdentifier should always return a valid identifier."), *RuntimeName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	// Check if we have a DDC cache hit
	uint32 RemoteMemoryAlignment = 0;
	FSharedBuffer RemoteData = UE::NNE::ModelData::GetFromDDC(FileId, RuntimeName, ModelDataIdentifier, RemoteMemoryAlignment);
	if (RemoteData.GetSize() > 0)
	{
		ModelData.Add(RuntimeName, MakeTuple(RemoteData, RemoteMemoryAlignment));

		return MakeShared<UE::NNE::FSharedModelData>(RemoteData);
	}
#endif //WITH_EDITOR

	// Try to create the model
	uint32 CreatedMemoryAlignment = 0;
	FSharedBuffer CreatedData = UE::NNE::ModelData::CreateModelData(RuntimeName, FileType, FileData, FileId, nullptr, CreatedMemoryAlignment);
	if (CreatedData.GetSize() < 1)
	{
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	// Cache the model
	ModelData.Add(RuntimeName, MakeTuple(CreatedData, CreatedMemoryAlignment));

#if WITH_EDITOR
	// And put it into DDC
	UE::NNE::ModelData::PutIntoDDC(FileId, RuntimeName, ModelDataIdentifier, CreatedData, CreatedMemoryAlignment);
#endif //WITH_EDITOR

	return MakeShared<UE::NNE::FSharedModelData>(CreatedData);
}

void UNNEModelData::ClearModelData()
{
	ModelData.Empty();
}

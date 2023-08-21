// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEModelData.h"

#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeFormat.h"
#include "Serialization/CustomVersion.h"
#include "UObject/WeakInterfacePtr.h"
#include "EditorFramework/AssetImportData.h"

#if WITH_EDITOR
#include "Containers/StringFwd.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/Guid.h"
#endif

enum Type
{
	Initial = 0,
	TargetRuntimesAndAssetImportData = 1,
	// -----<new versions can be added before this line>-------------------------------------------------
	// - this needs to be the last line (see note below)
	VersionPlusOne,
	LatestVersion = VersionPlusOne - 1
};

const FGuid UNNEModelData::GUID(0x9513202e, 0xeba1b279, 0xf17fe5ba, 0xab90c3f2);
FCustomVersionRegistration NNEModelDataVersion(UNNEModelData::GUID, LatestVersion, TEXT("NNEModelDataVersion"));//Always save with the latest version

#if WITH_EDITOR

inline FString GetDDCRequestId(const FString& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier)
{
	//RuntimeName and FileId are embedded to the id to ensure no potential collision between the runtime/assets
	return RuntimeName + "-" + FileId + "-" + ModelDataIdentifier;
}

inline UE::DerivedData::FCacheKey CreateCacheKey(const FString& FileId, const FString& RequestId)
{
	return { UE::DerivedData::FCacheBucket(FWideStringView(*FileId)), FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(RequestId))) };
}

inline FSharedBuffer GetFromDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier)
{
	FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
	FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

	UE::DerivedData::FCacheGetValueRequest GetRequest;
	GetRequest.Name = FString("Get-") + RequestId;
	GetRequest.Key = CreateCacheKey(FileIdStr, RequestId);
	FSharedBuffer RawDerivedData;
	UE::DerivedData::FRequestOwner BlockingGetOwner(UE::DerivedData::EPriority::Blocking);
	UE::DerivedData::GetCache().GetValue({ GetRequest }, BlockingGetOwner, [&RawDerivedData](UE::DerivedData::FCacheGetValueResponse&& Response)
		{
			RawDerivedData = Response.Value.GetData().Decompress();
		});
	BlockingGetOwner.Wait();
	return RawDerivedData;
}

inline void PutIntoDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier, FSharedBuffer& Data)
{
	FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
	FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

	UE::DerivedData::FCachePutValueRequest PutRequest;
	PutRequest.Name = FString("Put-") + RequestId;
	PutRequest.Key = CreateCacheKey(FileIdStr, RequestId);
	PutRequest.Value = UE::DerivedData::FValue::Compress(Data);
	UE::DerivedData::FRequestOwner BlockingPutOwner(UE::DerivedData::EPriority::Blocking);
	UE::DerivedData::GetCache().PutValue({ PutRequest }, BlockingPutOwner);
	BlockingPutOwner.Wait();
}

#endif

inline TArray<uint8> CreateRuntimeDataBlob(const FString& RuntimeName, FString FileType, const TArray<uint8>& FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	TWeakInterfacePtr<INNERuntime> NNERuntime = UE::NNE::GetRuntime<INNERuntime>(RuntimeName);
	if (NNERuntime.IsValid())
	{
		return NNERuntime->CreateModelData(FileType, FileData, FileId, TargetPlatform);
	}
	else
	{
		UE_LOG(LogNNE, Error, TEXT("UNNEModelData: No runtime '%s' found. Valid runtimes are: "), *RuntimeName);
		TArrayView<TWeakInterfacePtr<INNERuntime>> Runtimes = UE::NNE::GetAllRuntimes();
		for (int i = 0; i < Runtimes.Num(); i++)
		{
			UE_LOG(LogNNE, Error, TEXT("- %s"), *Runtimes[i]->GetRuntimeName());
		}
		return {};
	}
}

void UNNEModelData::Init(const FString& Type, TConstArrayView<uint8> Buffer)
{
	FileType = Type;
	FileData = Buffer;
	FPlatformMisc::CreateGuid(FileId);
	ModelData.Empty();
}

FString UNNEModelData::GetFileType()
{
	return FileType;
}

TConstArrayView<uint8> UNNEModelData::GetFileData()
{
	return FileData;
}

FGuid UNNEModelData::GetFileId()
{
	return FileId;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNEModelData::GetModelData(const FString& RuntimeName)
{
#if WITH_EDITORONLY_DATA
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
#endif //WITH_EDITORONLY_DATA

	// Check if we have a local cache hit
	FSharedBuffer* LocalData = ModelData.Find(RuntimeName);
	if (LocalData)
	{
		return MakeShared<UE::NNE::FSharedModelData>(*LocalData);
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
	FSharedBuffer RemoteData = GetFromDDC(FileId, RuntimeName, ModelDataIdentifier);
	if (RemoteData.GetSize() > 0)
	{
		ModelData.Add(RuntimeName, RemoteData);
		
		return MakeShared<UE::NNE::FSharedModelData>(RemoteData);
	}
#endif //WITH_EDITOR

	// Try to create the model
	TArray<uint8> RuntimeDataBlob = CreateRuntimeDataBlob(RuntimeName, FileType, FileData, FileId, nullptr);
	FSharedBuffer CreatedData = MakeSharedBufferFromArray(MoveTemp(RuntimeDataBlob));
	if (CreatedData.GetSize() < 1)
	{
		
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	// Cache the model
	ModelData.Add(RuntimeName, CreatedData);

#if WITH_EDITOR
	// And put it into DDC
	PutIntoDDC(FileId, RuntimeName, ModelDataIdentifier, CreatedData);
#endif //WITH_EDITOR
	
	return MakeShared<UE::NNE::FSharedModelData>(CreatedData);
}

void UNNEModelData::Serialize(FArchive& Ar)
{
	// Store the asset version (no effect in load)
	Ar.UsingCustomVersion(UNNEModelData::GUID);

#if WITH_EDITORONLY_DATA
	// Recreate each model data when cooking
	if (Ar.IsCooking() && Ar.IsSaving())
	{
		ModelData.Reset();

		TArray<FString, TInlineAllocator<10>> CookedRuntimeNames;
		CookedRuntimeNames.Append(GetTargetRuntimes());

		//No target runtime means all currently registered ones.
		if (GetTargetRuntimes().IsEmpty())
		{
			for (const TWeakInterfacePtr<INNERuntime>& Runtime : UE::NNE::GetAllRuntimes())
			{
				CookedRuntimeNames.Add(Runtime->GetRuntimeName());
			}
		}

		for (const FString& RuntimeName : CookedRuntimeNames)
		{
			TArray RuntimeDataBlob = CreateRuntimeDataBlob(RuntimeName, FileType, FileData, FileId, Ar.GetArchiveState().CookingTarget());
			if (RuntimeDataBlob.Num() > 0)
			{
				FSharedBuffer CreatedData = MakeSharedBufferFromArray(MoveTemp(RuntimeDataBlob));
				ModelData.Add(RuntimeName, CreatedData);
#if WITH_EDITOR
				TWeakInterfacePtr<INNERuntime> NNERuntime = UE::NNE::GetRuntime<INNERuntime>(RuntimeName);
				if (NNERuntime.IsValid())
				{
					FString ModelDataIdentifier = NNERuntime->GetModelDataIdentifier(FileType, FileData, FileId, Ar.GetArchiveState().CookingTarget());
					if (ModelDataIdentifier.Len() > 0)
					{
						PutIntoDDC(FileId, RuntimeName, ModelDataIdentifier, CreatedData);
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

		// Dummy data for fields not required in the game
		TArray<uint8> EmptyData;
		TArray<FString> RuntimeNames;
		ModelData.GetKeys(RuntimeNames);
		int32 NumItems = RuntimeNames.Num();

		Ar << FileType;
		Ar << EmptyData;
		Ar << FileId;
		Ar << NumItems;

		for (int i = 0; i < NumItems; i++)
		{
			Ar << RuntimeNames[i];
			FSharedBuffer Data = ModelData[RuntimeNames[i]];
			TArray<uint8> DataToSerialize(static_cast<const uint8*>(Data.GetData()), Data.GetSize());
			Ar << DataToSerialize;
		}
	}
	else
#endif //WITH_EDITORONLY_DATA
	{
		int32 NumItems = 0;

#if WITH_EDITORONLY_DATA
		if (Ar.CustomVer(UNNEModelData::GUID) >= TargetRuntimesAndAssetImportData)
		{
			Ar << TargetRuntimes;
			Ar << AssetImportData;
		}
		else 
		{
			// AssetImportData should always be valid
			AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
		}
#endif //WITH_EDITORONLY_DATA

		Ar << FileType;
		Ar << FileData;
		Ar << FileId;
		Ar << NumItems;

		if (Ar.IsLoading())
		{
			for (int i = 0; i < NumItems; i++)
			{
				FString Name;
				Ar << Name;
				TArray<uint8> Data;
				Ar << Data;
				FSharedBuffer SharedData = MakeSharedBufferFromArray(MoveTemp(Data));
				ModelData.Add(Name, SharedData);
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
namespace UE::NNE::ModelDataHelpers
{
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
} // UE::NNE::ModelDataHelpers

void UNNEModelData::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
	Super::PostInitProperties();
}

void UNNEModelData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}

	OutTags.Add(FAssetRegistryTag("TargetRuntimes", UE::NNE::ModelDataHelpers::GetRuntimesAsString(GetTargetRuntimes()), FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags(OutTags);
}

void UNNEModelData::SetTargetRuntimes(TArrayView<const FString> RuntimeNames)
{
	TargetRuntimes = RuntimeNames;

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

#endif //WITH_EDITORONLY_DATA
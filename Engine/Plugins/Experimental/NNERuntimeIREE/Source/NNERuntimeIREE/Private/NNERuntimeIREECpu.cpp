// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREECpu.h"

#ifdef WITH_NNE_RUNTIME_IREE

#include "EngineAnalytics.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "IO/IoHash.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NNERuntimeIREECommon.h"
#include "NNERuntimeIREECpuModel.h"
#include "Serialization/MemoryReader.h"

#if WITH_EDITOR
#include "Containers/StringFwd.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Memory/SharedBuffer.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"
#endif // WITH_EDITOR

namespace UE::NNERuntimeIREECpu::Private
{
#if WITH_EDITOR
	inline FString GetDDCRequestId(const FString& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier)
	{
		//RuntimeName and FileId are embedded to the id to ensure no potential collision between the runtime/assets
		return RuntimeName + "-" + FileId + "-" + ModelDataIdentifier + "-lib";
	}

	inline UE::DerivedData::FCacheKey CreateCacheKey(const FString& RequestId)
	{
		return { UE::DerivedData::FCacheBucket(TEXT("NNEModelData")), FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(RequestId))) };
	}

	inline void PutIntoDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier, const FSharedBuffer& Data)
	{
		check(!Data.IsNull());
		check(Data.GetSize() > 0);

		FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
		FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

		TArray<UE::DerivedData::FCachePutValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Put-") + RequestId;
		Requests[0].Key = CreateCacheKey(RequestId);
		Requests[0].Value = UE::DerivedData::FValue::Compress(Data);

		UE::DerivedData::FRequestOwner BlockingPutOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().PutValue(Requests, BlockingPutOwner);
		BlockingPutOwner.Wait();
	}

	inline FSharedBuffer GetFromDDC(const FGuid& FileId, const FString& RuntimeName, const FString& ModelDataIdentifier)
	{
		FString FileIdStr = FileId.ToString(EGuidFormats::Digits);
		FString RequestId = GetDDCRequestId(FileIdStr, RuntimeName, ModelDataIdentifier);

		TArray<UE::DerivedData::FCacheGetValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Get-") + RequestId;
		Requests[0].Key = CreateCacheKey(RequestId);

		FSharedBuffer Result;
		UE::DerivedData::FRequestOwner BlockingGetOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().GetValue(Requests, BlockingGetOwner, [&Result](UE::DerivedData::FCacheGetValueResponse&& Response)
			{
				if (Response.Value.HasData() && Response.Value.GetRawSize() > sizeof(uint32))
				{
					Result = Response.Value.GetData().Decompress();
				}
			});
		BlockingGetOwner.Wait();
		return Result;
	}

	struct FBuildConfig : FJsonSerializable
	{
		FString TargetPlatformName;
		FString SharedLibraryExtension;
		FString ModelCompilationCommand;
		FString ModelCompilationArguments;
		FString SharedLibraryCompilationCommand;
		FString SharedLibraryCompilationArguments;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("TargetPlatformName", TargetPlatformName);
			JSON_SERIALIZE("SharedLibraryExtension", SharedLibraryExtension);
			JSON_SERIALIZE("ModelCompilationCommand", ModelCompilationCommand);
			JSON_SERIALIZE("ModelCompilationArguments", ModelCompilationArguments);
			JSON_SERIALIZE("SharedLibraryCompilationCommand", SharedLibraryCompilationCommand);
			JSON_SERIALIZE("SharedLibraryCompilationArguments", SharedLibraryCompilationArguments);
		END_JSON_SERIALIZER
	};

	bool GetEnvVariable(const FString& Name, FString& Result)
	{
#if PLATFORM_WINDOWS
		DWORD Size = GetEnvironmentVariableA(StringCast<ANSICHAR>(*Name).Get(), nullptr, 0);
		if (Size > 0)
		{
			char* Data = new char[Size];
			GetEnvironmentVariableA(StringCast<ANSICHAR>(*Name).Get(), Data, Size);
			Result = FString(Data);
			delete[] Data;
			return true;
		}
		return false;
#elif PLATFORM_LINUX
		return false;
#elif PLATFORM_MAC
		return false;
#else
		return false;
#endif
	}

	bool ResolveEnvironmentVariables(FString& String)
	{
		FString ResultString = String;
		int32 StartIndex = String.Find("$ENV{", ESearchCase::CaseSensitive);
		while (StartIndex != INDEX_NONE)
		{
			StartIndex += 5;
			int32 EndIndex = String.Find("}", ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex);
			if (EndIndex > StartIndex)
			{
				FString EnvironmentVariableName = String.Mid(StartIndex, EndIndex - StartIndex);
				FString EnvironmentVariableValue;
				if (!GetEnvVariable(EnvironmentVariableName, EnvironmentVariableValue))
				{
					return false;
				}
				else
				{
					ResultString = ResultString.Replace(*(FString("$ENV{") + EnvironmentVariableName + FString("}")), *EnvironmentVariableValue, ESearchCase::CaseSensitive);
				}
			}
			else
			{
				return false;
			}
			StartIndex = String.Find("$ENV{", ESearchCase::CaseSensitive, ESearchDir::FromStart, EndIndex);
		}
		String = ResultString;
		return true;
	}

	bool LoadBuildConfig(const FString& InTargetPlatformDisplayName, const FString& InBuildConfigFileName, FBuildConfig& OutBuildConfig)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		FString PluginDir = FPaths::ConvertRelativePathToFull(*IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir());
		TArray<FString> BuildConfigFilePaths =
		{
			FPaths::Combine(FPaths::ConvertRelativePathToFull(*IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir()), "Config", InBuildConfigFileName),
			FPaths::Combine(FPaths::ConvertRelativePathToFull(*FPaths::EngineDir()), "Platforms", InTargetPlatformDisplayName, "Plugins", UE_PLUGIN_NAME, "Config", InBuildConfigFileName),
			FPaths::Combine(FPaths::ConvertRelativePathToFull(*FPaths::EngineDir()), "Platforms", InTargetPlatformDisplayName, "Plugins", "Experimental", UE_PLUGIN_NAME, "Config", InBuildConfigFileName)
		};

		FString BuildConfigFileString = "";
		for (FString BuildConfigFilePath : BuildConfigFilePaths)
		{
			if (PlatformFile.FileExists(*BuildConfigFilePath)) 
			{
				if (FFileHelper::LoadFileToString(BuildConfigFileString, *BuildConfigFilePath))
				{
					TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(BuildConfigFileString);
					TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
					if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid() && OutBuildConfig.FromJson(JsonObject))
					{
						if (ResolveEnvironmentVariables(OutBuildConfig.ModelCompilationCommand))
						{
							if (ResolveEnvironmentVariables(OutBuildConfig.SharedLibraryCompilationCommand))
							{
								return true;
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not resolve all environment variables in %s"), *OutBuildConfig.SharedLibraryCompilationCommand);
								return false;
							}
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not resolve all environment variables in %s"), *OutBuildConfig.ModelCompilationCommand);
							return false;
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not read build config file %s"), *BuildConfigFilePath);
					return false;
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not find any build config file"));
		return false;
	}
#endif // WITH_EDITOR

	FString GetModelCpuDataIdentifier(const FString& FileIdString, const FString& PlatformDisplayName)
	{
		return FileIdString + "-" + UNNERuntimeIREECpu::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeIREECpu::Version) + "-" + FString::FromInt(UNNERuntimeIREECpu::MemoryAlignment) + "-" + PlatformDisplayName;
	}

	FString GetLibraryEntryPointName(const FString& HeaderString)
	{
		FString SearchString = "iree_hal_executable_library_header_t**";
		int32 Start = HeaderString.Find(SearchString);
		if (Start < 0)
		{
			return "";
		}
		Start += SearchString.Len();
		int32 End = HeaderString.Find("(", ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (End <= Start)
		{
			return "";
		}
		return HeaderString.Mid(Start, End - Start).TrimStartAndEnd();
	}

	bool GetModuleMetaData(const FString& FileType, TConstArrayView<uint8> FileData, UE::NNERuntimeIREE::FModuleMetaData* ModuleMetaData)
	{
		check(ModuleMetaData);
		FString FileDataString = "";
		FileDataString.AppendChars((char*)FileData.GetData(), FileData.Num());
		return ModuleMetaData->ParseFromString(FileDataString);
	}
} // UE::NNERuntimeIREECpu::Private

FGuid UNNERuntimeIREECpu::GUID = FGuid((int32)'I', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeIREECpu::Version = 0x00000001;
uint32 UNNERuntimeIREECpu::MemoryAlignment = IREE_HAL_HEAP_BUFFER_ALIGNMENT;

FString UNNERuntimeIREECpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeIREECpu");
}

bool UNNERuntimeIREECpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const
{
#if WITH_EDITOR
	return 	FileType.Compare(TEXT("mlir"), ESearchCase::IgnoreCase) == 0;
#else
	return false;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREECpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	FString TargetPlatformDisplayName = UE::NNERuntimeIREE::GetTargetPlatformDisplayName(TargetPlatform);
	if (!CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu cannot create the model data with id %s (Filetype: %s) for platform %s"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType, *TargetPlatformDisplayName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	FConfigFile ConfigFile;
	FString ConfigFilePath;
	GetUpdatedPlatformConfig(TargetPlatformDisplayName, ConfigFile, ConfigFilePath);
	if (ConfigFile.Dirty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not find the required settings in config file %s. Please make the file writeable and re-start the editor or manually add the required staging settings or models will not work in packaged builds for platform %s!"), *ConfigFilePath, *TargetPlatformDisplayName);
	}

	FString BuildConfigFileName = FString("IREE_") + NNE_RUNTIME_IREE_PLATFORM_NAME + "_To_" + TargetPlatformDisplayName + ".json";
	UE::NNERuntimeIREECpu::Private::FBuildConfig BuildConfig;
	if (!UE::NNERuntimeIREECpu::Private::LoadBuildConfig(TargetPlatformDisplayName, BuildConfigFileName, BuildConfig))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to find and load the build config file %s"), *BuildConfigFileName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
	FString PluginDir = FPaths::ConvertRelativePathToFull(*IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir());
	FString IntermediateModelDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(BuildConfig.TargetPlatformName, FileIdString)));
	FString ObjectPath = FPaths::Combine(IntermediateModelDir, FileIdString + ".o");
	FString AdditionalOutputPath = FPaths::Combine(IntermediateModelDir, FileIdString + ".vmfb");
	FString InputPath = FPaths::Combine(IntermediateModelDir, FileIdString + "." + FileType);
	FString SharedLibName = FileIdString + "." + BuildConfig.SharedLibraryExtension;
	FString SharedLibPath = FPaths::Combine(IntermediateModelDir, SharedLibName);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*IntermediateModelDir);
	PlatformFile.CreateDirectoryTree(*IntermediateModelDir);
	FFileHelper::SaveArrayToFile(FileData, *InputPath);

	FString ModelCompilationCommand = BuildConfig.ModelCompilationCommand.Replace(*FString("${PLUGIN_DIR}"), *PluginDir);
	FString ModelCompilationArguments = BuildConfig.ModelCompilationArguments.Replace(*FString("${OBJECT_PATH}"), *ObjectPath).Replace(*FString("${ADDITIONAL_OUTPUT_PATH}"), *AdditionalOutputPath).Replace(*FString("${INPUT_PATH}"), *InputPath);

	FProcHandle ModelCompilationProcHandle = FPlatformProcess::CreateProc(*ModelCompilationCommand, *ModelCompilationArguments, false, true, true, nullptr, 0, nullptr, nullptr, nullptr);
	FPlatformProcess::WaitForProc(ModelCompilationProcHandle);

	if ((BuildConfig.ModelCompilationArguments.Contains(FString("${ADDITIONAL_OUTPUT_PATH}")) && !PlatformFile.FileExists(*AdditionalOutputPath)) ||
		(BuildConfig.ModelCompilationArguments.Contains(FString("${OBJECT_PATH}")) && !PlatformFile.FileExists(*ObjectPath)))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to compile the model \"%s\" using the command:"), *InputPath);
		UE_LOG(LogTemp, Warning, TEXT("\"%s\" %s"), *ModelCompilationCommand, *ModelCompilationArguments);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	if (BuildConfig.SharedLibraryCompilationCommand.Len() > 0)
	{
		FString SharedLibraryCompilationCommand = BuildConfig.SharedLibraryCompilationCommand;
		FString SharedLibraryCompilationArguments = BuildConfig.SharedLibraryCompilationArguments;
#ifdef NNE_RUNTIME_IREE_WIN_TOOLCHAIN_PATH
		SharedLibraryCompilationCommand = SharedLibraryCompilationCommand.Replace(*FString("${WIN_TOOLCHAIN_PATH}"), *FString(NNE_RUNTIME_IREE_WIN_TOOLCHAIN_PATH));
		SharedLibraryCompilationArguments = SharedLibraryCompilationArguments.Replace(*FString("${WIN_TOOLCHAIN_PATH}"), *FString(NNE_RUNTIME_IREE_WIN_TOOLCHAIN_PATH));
#endif
#ifdef NNE_RUNTIME_IREE_WIN_SDK_LIB_PATH
		SharedLibraryCompilationArguments = SharedLibraryCompilationArguments.Replace(*FString("${WIN_SDK_LIB_PATH}"), *FString(NNE_RUNTIME_IREE_WIN_SDK_LIB_PATH));
#endif
#ifdef NNE_RUNTIME_IREE_LINUX_COMPILER
		SharedLibraryCompilationCommand = SharedLibraryCompilationCommand.Replace(*FString("${LINUX_COMPILER}"), *FString(NNE_RUNTIME_IREE_LINUX_COMPILER));
#endif
		FString FinalSharedLibraryCompilationArguments = SharedLibraryCompilationArguments.Replace(*FString("${OBJECT_PATH}"), *ObjectPath).Replace(*FString("${SHARED_LIB_PATH}"), *SharedLibPath);
		FProcHandle SharedLibraryCompilationProcHandle = FPlatformProcess::CreateProc(*SharedLibraryCompilationCommand, *FinalSharedLibraryCompilationArguments, false, true, true, nullptr, 0, nullptr, nullptr, nullptr);
		FPlatformProcess::WaitForProc(SharedLibraryCompilationProcHandle);
		if (!PlatformFile.FileExists(*SharedLibPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to compile the shared library for the object \"%s\" using the command:"), *ObjectPath);
			UE_LOG(LogTemp, Warning, TEXT("\"%s\" %s"), *SharedLibraryCompilationCommand, *FinalSharedLibraryCompilationArguments);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}

		FString CookedModelPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetCookedModelDirPath(TargetPlatformDisplayName), SharedLibName));
		IFileManager::Get().Copy(*CookedModelPath, *SharedLibPath);

		TArray<uint8> SharedLibData;
		if (!FFileHelper::LoadFileToArray(SharedLibData, *CookedModelPath) || SharedLibData.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not read the shared library \"%s\""), *CookedModelPath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		FSharedBuffer SharedBuffer = MakeSharedBufferFromArray(MoveTemp(SharedLibData));
		UE::NNERuntimeIREECpu::Private::PutIntoDDC(FileId, GetRuntimeName(), UE::NNERuntimeIREECpu::Private::GetModelCpuDataIdentifier(FileIdString, TargetPlatformDisplayName), SharedBuffer);
	}

	FString HeaderPath = FPaths::Combine(IntermediateModelDir, FileIdString + ".h");
	if (!PlatformFile.FileExists(*HeaderPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not find the model header \"%s\""), *HeaderPath);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	FString HeaderString;
	if (!FFileHelper::LoadFileToString(HeaderString, *HeaderPath) || HeaderString.Len() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not read the model header \"%s\""), *HeaderPath);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	FString LibraryQueryFunctionName = UE::NNERuntimeIREECpu::Private::GetLibraryEntryPointName(HeaderString);
	if(LibraryQueryFunctionName.Len() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not find the entry point in model header \"%s\""), *HeaderPath);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	TArray<uint8> AdditionalOutputData;
	if (!FFileHelper::LoadFileToArray(AdditionalOutputData, *AdditionalOutputPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not load the model virtual machine buffer \"%s\""), *AdditionalOutputPath);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	UE::NNERuntimeIREE::FModuleMetaData ModuleMetaData;
	if (!UE::NNERuntimeIREECpu::Private::GetModuleMetaData(FileType, FileData, &ModuleMetaData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not get the module meta data"));
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	FString ModuleDataString = ModuleMetaData.ToJson(false);

	TArray<uint8> Header;
	FMemoryWriter Writer(Header);
	Writer << UNNERuntimeIREECpu::GUID;
	Writer << UNNERuntimeIREECpu::Version;
	Writer << FileId;
	Writer << LibraryQueryFunctionName;
	Writer << ModuleDataString;

	uint32 AdditionalDataOffset = 0;
	uint32 HeaderSize = (uint32)Header.Num() + sizeof(AdditionalDataOffset);
	check(MemoryAlignment > 0);
	AdditionalDataOffset = uint32(FMath::CeilToDouble(double(HeaderSize) / double(MemoryAlignment))) * MemoryAlignment;
	check(AdditionalDataOffset > 0);
	check(HeaderSize < AdditionalDataOffset);
	check(AdditionalDataOffset % MemoryAlignment == 0);

	Writer << AdditionalDataOffset;
	check(HeaderSize == (uint32)Header.Num());

	SIZE_T ResultDataSize = AdditionalDataOffset + AdditionalOutputData.Num();
	void* ResultData = FMemory::Malloc(ResultDataSize, MemoryAlignment);
	FMemory::Memcpy(ResultData, (void*)Header.GetData(), Header.Num());
	FMemory::Memcpy((void*)&((uint8*)ResultData)[AdditionalDataOffset], (void*)AdditionalOutputData.GetData(), AdditionalOutputData.Num());
	return MakeShared<UE::NNE::FSharedModelData>(FSharedBuffer::TakeOwnership(ResultData, ResultDataSize, FMemory::Free), MemoryAlignment);
#else
	return TSharedPtr<UE::NNE::FSharedModelData>();
#endif // WITH_EDITOR
}

FString UNNERuntimeIREECpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	FString PlatformDisplayName = UE::NNERuntimeIREE::GetTargetPlatformDisplayName(TargetPlatform);
	return UE::NNERuntimeIREECpu::Private::GetModelCpuDataIdentifier(FileId.ToString(EGuidFormats::Digits), PlatformDisplayName);
}

bool UNNERuntimeIREECpu::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	int32 GuidSize = sizeof(UNNERuntimeIREECpu::GUID);
	int32 VersionSize = sizeof(UNNERuntimeIREECpu::Version);
	if (SharedDataView.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(SharedDataView[0]), &(UNNERuntimeIREECpu::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(SharedDataView[GuidSize]), &(UNNERuntimeIREECpu::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeIREECpu::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);

	if (!CanCreateModelCPU(ModelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());
	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FMemoryReaderView Reader(SharedDataView);
	FGuid DataGuid;
	Reader << DataGuid;
	int32 VersionVersion;
	Reader << VersionVersion;
	FGuid FileId;
	Reader << FileId;
	FString LibraryQueryFunctionName;
	Reader << LibraryQueryFunctionName;
	FString ModuleDataString;
	Reader << ModuleDataString;
	uint32 VmfbDataOffset;
	Reader << VmfbDataOffset;

	UE::NNERuntimeIREE::FModuleMetaData ModuleMetaData;
	if (ModuleDataString.Len() > 0 && !ModuleMetaData.FromJson(ModuleDataString))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to parse the module meta data"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
#if WITH_EDITOR
	FString SharedLibDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(NNE_RUNTIME_IREE_PLATFORM_NAME, FileIdString)));
	FString SharedLibName = FileIdString + "." + NNE_RUNTIME_IREE_PLATFORM_SHARED_LIB_EXTENSION;
#else
	FString SharedLibDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetPackagedModelDirPath(NNE_RUNTIME_IREE_PLATFORM_NAME)));
	FString SharedLibName = FileIdString + "." + NNE_RUNTIME_IREE_PLATFORM_SHARED_LIB_EXTENSION;
#endif // WITH_EDITOR

#if WITH_EDITOR
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString SharedLibPath = FPaths::Combine(SharedLibDirPath, SharedLibName);
	if (!PlatformFile.FileExists(*SharedLibPath))
	{
		FSharedBuffer SharedBuffer = UE::NNERuntimeIREECpu::Private::GetFromDDC(FileId, GetRuntimeName(), UE::NNERuntimeIREECpu::Private::GetModelCpuDataIdentifier(FileIdString, NNE_RUNTIME_IREE_PLATFORM_DISPLAY_NAME));
		if (SharedBuffer.GetSize() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not fetch the shared library %s from DDC"), *SharedLibName);
			return TSharedPtr<UE::NNE::IModelCPU>();
		}
		PlatformFile.CreateDirectoryTree(*SharedLibDirPath);
		FFileHelper::SaveArrayToFile(TConstArrayView<uint8>((uint8*)SharedBuffer.GetData(), SharedBuffer.GetSize()), *SharedLibPath);
	}
#endif // WITH_EDITOR

	TUniquePtr<UE::NNERuntimeIREECpu::FModel> Model = MakeUnique<UE::NNERuntimeIREECpu::FModel>();
	if (!Model->Init(SharedData, VmfbDataOffset, ModuleMetaData, SharedLibDirPath, SharedLibName, LibraryQueryFunctionName))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not initialize the model created from model data with id %s"), *FileIdString);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedDataView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelCPU>(static_cast<UE::NNE::IModelCPU*>(Model.Release()));
}

FString UNNERuntimeIREECpu::GetIntermediateModelDirPath(const FString& PlatformName, const FString& FileIdString)
{
	return FPaths::Combine("Intermediate", "Build", PlatformName, UE_PLUGIN_NAME, FileIdString);
}

FString UNNERuntimeIREECpu::GetCookedModelDirPath(const FString& PlatformName)
{
	return FPaths::Combine("Saved", "Cooked", PlatformName, "Engine", "Plugins", UE_PLUGIN_NAME, "Binaries");
}

FString UNNERuntimeIREECpu::GetPackagedModelDirPath(const FString& PlatformName)
{
	FString PlatformNameShort = PlatformName.Equals("Windows") ? "Win64" : PlatformName;
	return FPaths::Combine("Binaries", PlatformNameShort, UE_PLUGIN_NAME);
}

void UNNERuntimeIREECpu::GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath)
{ 
	FString ConfigFolderPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	ConfigFilePath = FPaths::Combine(ConfigFolderPath, PlatformName, PlatformName + "Game.ini");

	ConfigFile.Read(ConfigFilePath);

	FString CookingPath = FString("/") + GetCookedModelDirPath(PlatformName);
	FString PackagingPath = FString("/") + GetPackagedModelDirPath(PlatformName);

	ConfigFile.AddUniqueToSection(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("+DirectoriesToAlwaysStageAsNonUFS"), FString("(Path=\"..") + CookingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+RemapDirectories"), FString("(From=\"") + FApp::GetProjectName() + CookingPath + FString("\", To=\"") + FApp::GetProjectName() + PackagingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+AllowedDirectories"), FApp::GetProjectName() + PackagingPath);
}

#else // WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREECpu::GetRuntimeName() const { return ""; };

bool UNNERuntimeIREECpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const { return false; };
TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREECpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return TSharedPtr<UE::NNE::FSharedModelData>(); };
FString UNNERuntimeIREECpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return ""; };

bool UNNERuntimeIREECpu::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const { return false; };
TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeIREECpu::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData) { return TSharedPtr<UE::NNE::IModelCPU>(); };

FString UNNERuntimeIREECpu::GetIntermediateModelDirPath(const FString& PlatformName, const FString& FileIdString) { return ""; }
FString UNNERuntimeIREECpu::GetCookedModelDirPath(const FString& PlatformName) { return ""; }
FString UNNERuntimeIREECpu::GetPackagedModelDirPath(const FString& PlatformName) { return ""; }

void UNNERuntimeIREECpu::GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath) { }

#endif // WITH_NNE_RUNTIME_IREE
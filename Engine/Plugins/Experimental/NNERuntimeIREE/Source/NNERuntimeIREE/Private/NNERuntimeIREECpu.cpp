// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREECpu.h"

#ifdef WITH_NNE_RUNTIME_IREE

#if WITH_EDITOR
#include "Containers/StringConv.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"
#include "Misc/FileHelper.h"
#endif // WITH_EDITOR

#include "EngineAnalytics.h"
#include "HAL/Platform.h"
#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "NNERuntimeIREECpuModel.h"
#include "Serialization/MemoryReader.h"

namespace UE::NNERuntimeIREECpu::Private
{
#if WITH_EDITOR
	inline UE::DerivedData::FCacheKey CreateCacheKey(const FString& RequestId)
	{
		return { UE::DerivedData::FCacheBucket(TEXT("NNEModelData")), FIoHash::HashBuffer(MakeMemoryView(StringCast<UTF8CHAR>(*RequestId))) };
	}

	inline void PutIntoDDC(const FString& RequestId, const FSharedBuffer& Data)
	{
		check(!Data.IsNull());
		check(Data.GetSize() > 0);

		TArray<UE::DerivedData::FCachePutValueRequest> Requests;
		Requests.SetNum(1);

		Requests[0].Name = FString("Put-") + RequestId;
		Requests[0].Key = CreateCacheKey(RequestId);
		Requests[0].Value = UE::DerivedData::FValue::Compress(Data);

		UE::DerivedData::FRequestOwner BlockingPutOwner(UE::DerivedData::EPriority::Blocking);
		UE::DerivedData::GetCache().PutValue(Requests, BlockingPutOwner);
		BlockingPutOwner.Wait();
	}

	inline FSharedBuffer GetFromDDC(const FString& RequestId)
	{
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
#endif // WITH_EDITOR

	FString GetModelCpuDataIdentifier(const FString& RuntimeName, const FString& FileIdString, const FString& PlatformName, const FString& Architecture)
	{
		return RuntimeName + "-" + UNNERuntimeIREECpu::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeIREECpu::Version) + "-" + FileIdString + "-" + PlatformName + (!Architecture.IsEmpty() ? ("-" + Architecture) : "");
	}

	bool GetModuleMetaData(const FString& FileType, TConstArrayView<uint8> FileData, UE::NNERuntimeIREE::FModuleMetaData* ModuleMetaData)
	{
		check(ModuleMetaData);
		FString FileDataString = "";
		FileDataString.AppendChars((char*)FileData.GetData(), FileData.Num());
		return ModuleMetaData->ParseFromString(FileDataString);
	}

	FString GetIntermediateModelDirPath(const FString& PlatformName, const FString& ModelName)
	{
		return FPaths::Combine("Intermediate", "Build", PlatformName, UE_PLUGIN_NAME, ModelName);
	}

	FString GetStagedModelDirPath(const FString& PlatformName)
	{
		return FPaths::Combine("Saved", "Cooked", PlatformName, "Engine", "Plugins", UE_PLUGIN_NAME, "Binaries");
	}

	FString GetPackagedModelDirPath(const FString& PlatformName)
	{
		FString PlatformNameShort = PlatformName.Equals("Windows") ? "Win64" : PlatformName;
		return FPaths::Combine("Binaries", PlatformNameShort, UE_PLUGIN_NAME);
	}
} // UE::NNERuntimeIREECpu::Private

FGuid UNNERuntimeIREECpu::GUID = FGuid((int32)'I', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeIREECpu::Version = 0x00000002;

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
	using namespace UE::NNERuntimeIREECpu::Private;

	FString TargetPlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	if (!CanCreateModelData(FileType, FileData, FileId, TargetPlatform))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu cannot create the model data with id %s (Filetype: %s) for platform %s"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType, *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	FConfigFile ConfigFile;
	FString ConfigFilePath;
	GetUpdatedPlatformConfig(TargetPlatformName, ConfigFile, ConfigFilePath);
	if (ConfigFile.Dirty)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not find the required settings in config file %s. Please make the file writeable and re-start the editor or manually add the required staging settings or models will not work in packaged builds for platform %s!"), *ConfigFilePath, *TargetPlatformName);
	}

	TUniquePtr<FNNERuntimeIREECpuCompiler> Compiler = FNNERuntimeIREECpuCompiler::Make(TargetPlatformName);
	if (!Compiler.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to create a compiler to compile for platform %s"), *TargetPlatformName);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	
	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(TargetPlatformName, FileIdString)));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.DeleteDirectoryRecursively(*IntermediateDir);
	PlatformFile.CreateDirectoryTree(*IntermediateDir);

	TArray<FIREECompilerResult> CompilerResults;
	FString StagingDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetStagedModelDirPath(TargetPlatformName)));
	if (!Compiler->CompileMlir(FileData, FileIdString, IntermediateDir, StagingDir, CompilerResults))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to compile model %s"), *FileIdString);
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}

	for (int32 i = 0; i < CompilerResults.Num(); i++)
	{
		TArray<uint8> SharedLibData;
		FString StagedSharedLibPath = FPaths::Combine(StagingDir, CompilerResults[i].RelativeDirPath, CompilerResults[i].SharedLibraryFileName);
		if (!FFileHelper::LoadFileToArray(SharedLibData, *StagedSharedLibPath) || SharedLibData.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not read the shared library \"%s\""), *StagedSharedLibPath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		FSharedBuffer SharedLibBuffer = MakeSharedBufferFromArray(MoveTemp(SharedLibData));
		PutIntoDDC(GetModelCpuDataIdentifier(GetRuntimeName(), FileIdString, TargetPlatformName, CompilerResults[i].Architecture) + "-lib", SharedLibBuffer);

		TArray<uint8> VmfbData;
		FString StagedVmfbPath = FPaths::Combine(StagingDir, CompilerResults[i].RelativeDirPath, CompilerResults[i].VmfbFileName);
		if (!FFileHelper::LoadFileToArray(VmfbData, *StagedVmfbPath) || VmfbData.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not read the vmfb data \"%s\""), *StagedVmfbPath);
			return TSharedPtr<UE::NNE::FSharedModelData>();
		}
		FSharedBuffer VmfbBuffer = MakeSharedBufferFromArray(MoveTemp(VmfbData));
		PutIntoDDC(GetModelCpuDataIdentifier(GetRuntimeName(), FileIdString, TargetPlatformName, CompilerResults[i].Architecture) + "-vmfb", VmfbBuffer);
	}
	
	UE::NNERuntimeIREE::FModuleMetaData ModuleMetaData;
	if (!GetModuleMetaData(FileType, FileData, &ModuleMetaData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not get the module meta data"));
		return TSharedPtr<UE::NNE::FSharedModelData>();
	}
	FString ModuleDataString = ModuleMetaData.ToJson(false);

	TArray<uint8> ResultData;
	FMemoryWriter Writer(ResultData);
	Writer << UNNERuntimeIREECpu::GUID;
	Writer << UNNERuntimeIREECpu::Version;
	Writer << FileId;

	Writer << ModuleDataString;

	int32 NumArchitectures = CompilerResults.Num();
	Writer << NumArchitectures;
	for (int32 i = 0; i < NumArchitectures; i++)
	{
		Writer << CompilerResults[i].Architecture;
		Writer << CompilerResults[i].RelativeDirPath;
		Writer << CompilerResults[i].SharedLibraryFileName;
		Writer << CompilerResults[i].VmfbFileName;
		Writer << CompilerResults[i].SharedLibraryEntryPointName;
	}

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(ResultData)), 0);
#else
	return TSharedPtr<UE::NNE::FSharedModelData>();
#endif // WITH_EDITOR
}

FString UNNERuntimeIREECpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform)
{
	// Leave architecture blank as there is only one model data for all architectures of a given platform
	FString PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : UGameplayStatics::GetPlatformName();
	return UE::NNERuntimeIREECpu::Private::GetModelCpuDataIdentifier(GetRuntimeName(), FileId.ToString(EGuidFormats::Digits), PlatformName, "");
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

	using namespace UE::NNERuntimeIREECpu::Private;

	if (!CanCreateModelCPU(ModelData))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString CurrentArchitecture = "";
#ifdef PLATFORM_CPU_X86_FAMILY
	CurrentArchitecture = "x86_64";
#elif PLATFORM_CPU_ARM_FAMILY
	CurrentArchitecture = "arm64";
#endif

	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());
	TConstArrayView<uint8> SharedDataView = SharedData->GetView();
	FMemoryReaderView Reader(SharedDataView);
	FGuid DataGuid = FGuid();
	Reader << DataGuid;
	int32 VersionVersion = 0;
	Reader << VersionVersion;
	FGuid FileId = FGuid();
	Reader << FileId;

	FString ModuleDataString = "";
	Reader << ModuleDataString;

	int32 NumArchitectures = 0;
	Reader << NumArchitectures;

	bool bFound = false;
	FString Architecture = "";
	FString RelativeDirPath = "";
	FString SharedLibraryFileName = "";
	FString VmfbFileName = "";
	FString SharedLibraryEntryPointName = "";
	for (int32 i = 0; i < NumArchitectures; i++)
	{
		FString TmpArchitecture = "";
		Reader << TmpArchitecture;
		FString TmpRelativeDirPath = "";
		Reader << TmpRelativeDirPath;
		FString TmpSharedLibraryFileName = "";
		Reader << TmpSharedLibraryFileName;
		FString TmpVmfbFileName = "";
		Reader << TmpVmfbFileName;
		FString TmpSharedLibraryEntryPointName = "";
		Reader << TmpSharedLibraryEntryPointName;

		if (TmpArchitecture.IsEmpty() && !bFound)
		{
			Architecture = TmpArchitecture;
			RelativeDirPath = TmpRelativeDirPath;
			SharedLibraryFileName = TmpSharedLibraryFileName;
			VmfbFileName = TmpVmfbFileName;
			SharedLibraryEntryPointName = TmpSharedLibraryEntryPointName;
			bFound = true;
		}
		else if (TmpArchitecture.Equals(CurrentArchitecture))
		{
			Architecture = TmpArchitecture;
			RelativeDirPath = TmpRelativeDirPath;
			SharedLibraryFileName = TmpSharedLibraryFileName;
			VmfbFileName = TmpVmfbFileName;
			SharedLibraryEntryPointName = TmpSharedLibraryEntryPointName;
			bFound = true;
		}
	}
	if (!bFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to find a matching architecture for \'%s\'"), *CurrentArchitecture);
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	UE::NNERuntimeIREE::FModuleMetaData ModuleMetaData;
	if (!ModuleDataString.IsEmpty() && !ModuleMetaData.FromJson(ModuleDataString))
	{
		UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu failed to parse the module meta data"));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	FString FileIdString = FileId.ToString(EGuidFormats::Digits).ToLower();
#if WITH_EDITOR
	FString SharedLibraryDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetIntermediateModelDirPath(UGameplayStatics::GetPlatformName(), FileIdString), RelativeDirPath));
#else
	FString SharedLibraryDirPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), GetPackagedModelDirPath(UGameplayStatics::GetPlatformName()), RelativeDirPath));
#endif // WITH_EDITOR

#if WITH_EDITOR
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString SharedLibraryFilePath = FPaths::Combine(SharedLibraryDirPath, SharedLibraryFileName);
	if (!PlatformFile.FileExists(*SharedLibraryFilePath))
	{
		FSharedBuffer SharedBuffer = GetFromDDC(GetModelCpuDataIdentifier(GetRuntimeName(), FileIdString, UGameplayStatics::GetPlatformName(), Architecture) + "-lib");
		if (SharedBuffer.GetSize() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not fetch the shared library %s from DDC"), *SharedLibraryFileName);
			return TSharedPtr<UE::NNE::IModelCPU>();
		}
		PlatformFile.CreateDirectoryTree(*SharedLibraryDirPath);
		FFileHelper::SaveArrayToFile(TConstArrayView<uint8>((uint8*)SharedBuffer.GetData(), SharedBuffer.GetSize()), *SharedLibraryFilePath);
	}

	FString VmfbFilePath = FPaths::Combine(SharedLibraryDirPath, VmfbFileName);
	if (!PlatformFile.FileExists(*VmfbFilePath))
	{
		FSharedBuffer SharedBuffer = GetFromDDC(GetModelCpuDataIdentifier(GetRuntimeName(), FileIdString, UGameplayStatics::GetPlatformName(), Architecture) + "-vmfb");
		if (SharedBuffer.GetSize() <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNNERuntimeIREECpu could not fetch the vmfb %s from DDC"), *VmfbFileName);
			return TSharedPtr<UE::NNE::IModelCPU>();
		}
		PlatformFile.CreateDirectoryTree(*SharedLibraryDirPath);
		FFileHelper::SaveArrayToFile(TConstArrayView<uint8>((uint8*)SharedBuffer.GetData(), SharedBuffer.GetSize()), *VmfbFilePath);
	}
#endif // WITH_EDITOR

	TUniquePtr<UE::NNERuntimeIREECpu::FModel> Model = MakeUnique<UE::NNERuntimeIREECpu::FModel>();
	if (!Model->Init(SharedLibraryDirPath, SharedLibraryFileName, VmfbFileName, SharedLibraryEntryPointName, ModuleMetaData))
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

void UNNERuntimeIREECpu::GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath)
{ 
	FString ConfigFolderPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
	ConfigFilePath = FPaths::Combine(ConfigFolderPath, PlatformName, PlatformName + "Game.ini");

	ConfigFile.Read(ConfigFilePath);

	FString StagingPath = FString("/") + UE::NNERuntimeIREECpu::Private::GetStagedModelDirPath(PlatformName);
	FString PackagingPath = FString("/") + UE::NNERuntimeIREECpu::Private::GetPackagedModelDirPath(PlatformName);

	ConfigFile.AddUniqueToSection(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("+DirectoriesToAlwaysStageAsNonUFS"), FString("(Path=\"..") + StagingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+RemapDirectories"), FString("(From=\"") + FApp::GetProjectName() + StagingPath + FString("\", To=\"") + FApp::GetProjectName() + PackagingPath + FString("\")"));
	ConfigFile.AddUniqueToSection(TEXT("Staging"), TEXT("+AllowedDirectories"), FApp::GetProjectName() + PackagingPath);
}

#else // WITH_NNE_RUNTIME_IREE

FString UNNERuntimeIREECpu::GetRuntimeName() const { return ""; };

bool UNNERuntimeIREECpu::CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const { return false; };
TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeIREECpu::CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return TSharedPtr<UE::NNE::FSharedModelData>(); };
FString UNNERuntimeIREECpu::GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) { return ""; };

bool UNNERuntimeIREECpu::CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const { return false; };
TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeIREECpu::CreateModelCPU(TObjectPtr<UNNEModelData> ModelData) { return TSharedPtr<UE::NNE::IModelCPU>(); };

void UNNERuntimeIREECpu::GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath) { }

#endif // WITH_NNE_RUNTIME_IREE
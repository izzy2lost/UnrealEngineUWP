// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORT.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeORTModel.h"
#include "NNERuntimeORTUtils.h"

#if PLATFORM_WINDOWS
#include "ID3D12DynamicRHI.h"
#endif // PLATFORM_WINDOWS

#include UE_INLINE_GENERATED_CPP_BY_NAME(NNERuntimeORT)

FGuid UNNERuntimeORTCpu::GUID = FGuid((int32)'O', (int32)'C', (int32)'P', (int32)'U');
int32 UNNERuntimeORTCpu::Version = 0x00000002;

FGuid UNNERuntimeORTDmlEditor::GUID = FGuid((int32)'O', (int32)'D', (int32)'M', (int32)'L');
int32 UNNERuntimeORTDmlEditor::Version = 0x00000002;

FGuid UNNERuntimeORTDml::GUID = UNNERuntimeORTDmlEditor::GUID;
int32 UNNERuntimeORTDml::Version = UNNERuntimeORTDmlEditor::Version;

namespace UE::NNERuntimeORT::Private
{

class NNERuntimeORTDmlImpl : public INNERuntime, public INNERuntimeGPU, public INNERuntimeRDG
{
	using ECanCreateModelCommonStatus = UE::NNE::EResultStatus;

private:
	TSharedPtr<Ort::Env> ORTEnvironment;
	FGuid GUID;
	int32 Version;
	
public:
	NNERuntimeORTDmlImpl(FGuid GUID, int32 Version) : GUID(GUID), Version(Version)
	{

	}

	virtual ~NNERuntimeORTDmlImpl() = default;

	void Init()
	{
		check(!ORTEnvironment.IsValid());
		ORTEnvironment = MakeShared<Ort::Env>();
	}

	virtual FString GetRuntimeName() const override
	{
		return TEXT("NNERuntimeORTDml");
	}

	virtual ECanCreateModelDataStatus CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override
	{
		return (!FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
	}

	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) override
	{
		if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
		{
			UE_LOG(LogNNE, Warning, TEXT("NNERuntimeORTDmlImpl cannot create the model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
			return {};
		}

		FNNEModelRaw InputModel{TArray<uint8>{FileData}, ENNEInferenceFormat::ONNX};
		if (!UE::NNERuntimeORT::Private::OrtHelper::OptimizeModel(InputModel, ENNEInferenceFormat::ONNX))
		{
			return {};
		}

		TArray<uint8> Result;
		FMemoryWriter Writer(Result);
		Writer << UNNERuntimeORTDml::GUID;
		Writer << UNNERuntimeORTDml::Version;
		Writer.Serialize(InputModel.Data.GetData(), InputModel.Data.Num());

		return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
	}

	virtual FString GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override
	{
		return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTDml::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTDml::Version);
	}

	virtual ECanCreateModelGPUStatus CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const override
	{
		return CanCreateModelCommon(ModelData) == ECanCreateModelCommonStatus::Ok ? ECanCreateModelGPUStatus::Ok : ECanCreateModelGPUStatus::Fail;
	}

	virtual TSharedPtr<UE::NNE::IModelGPU> CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) override
	{
#if PLATFORM_WINDOWS
		check(ModelData);
		check(ORTEnvironment.IsValid());

		if (CanCreateModelGPU(ModelData) != ECanCreateModelGPUStatus::Ok)
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeORTDml cannot create a model GPU from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
			return {};
		}

		const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
		check(SharedData.IsValid());

		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
				TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
				TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
				TEXT("ModelDataSize"), SharedData->GetView().Num()
			);
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
		}

		return MakeShared<UE::NNERuntimeORT::Private::FModelORTDmlGPU>(ORTEnvironment, SharedData);
#else // PLATFORM_WINDOWS
		return TSharedPtr<UE::NNE::IModelGPU>();
#endif // PLATFORM_WINDOWS
	}

	virtual ECanCreateModelRDGStatus CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const override{
		return CanCreateModelCommon(ModelData) == ECanCreateModelCommonStatus::Ok ? ECanCreateModelRDGStatus::Ok : ECanCreateModelRDGStatus::Fail;
	}

	virtual TSharedPtr<UE::NNE::IModelRDG> CreateModelRDG(TObjectPtr<UNNEModelData> ModelData) override{
#if PLATFORM_WINDOWS
		check(ModelData);
		check(ORTEnvironment.IsValid());

		if (CanCreateModelRDG(ModelData) != ECanCreateModelRDGStatus::Ok)
		{
			UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeORTDml cannot create a model RDG from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
			return {};
		}

		const TSharedRef<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName()).ToSharedRef();

		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
				TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
				TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
				TEXT("ModelDataSize"), SharedData->GetView().Num()
			);
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
		}

		return MakeShared<UE::NNERuntimeORT::Private::FModelORTDmlRDG>(ORTEnvironment.ToSharedRef(), SharedData);
#else // PLATFORM_MICROSOFT
		return {};
#endif // PLATFORM_MICROSOFT
	}

private:
	ECanCreateModelCommonStatus CanCreateModelCommon(const TObjectPtr<UNNEModelData> ModelData) const
	{
#if PLATFORM_WINDOWS
		check(ModelData != nullptr);

		// In order to use DirectML we need D3D12
		if (!IsRHID3D12())
		{
			return ECanCreateModelCommonStatus::Fail;
		}

		constexpr int32 GuidSize = sizeof(UNNERuntimeORTDml::GUID);
		constexpr int32 VersionSize = sizeof(UNNERuntimeORTDml::Version);
		const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

		if (!SharedData.IsValid())
		{
			return ECanCreateModelCommonStatus::Fail;
		}

		TConstArrayView<uint8> Data = SharedData->GetView();

		if (Data.Num() <= GuidSize + VersionSize)
		{
			return ECanCreateModelCommonStatus::Fail;
		}

		static const FGuid DeprecatedGUID = FGuid((int32)'O', (int32)'G', (int32)'P', (int32)'U');

		bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTDml::GUID), GuidSize) == 0;
		bResult |= FGenericPlatformMemory::Memcmp(&(Data[0]), &(DeprecatedGUID), GuidSize) == 0;
		bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTDml::Version), VersionSize) == 0;

		return bResult ? ECanCreateModelCommonStatus::Ok : ECanCreateModelCommonStatus::Fail;
#else // PLATFORM_MICROSOFT
		return ECanCreateModelCommonStatus::Fail;
#endif // PLATFORM_MICROSOFT
	}
};

} // namespace UE::NNERuntimeORT::Private

UNNERuntimeORTCpu::ECanCreateModelDataStatus UNNERuntimeORTCpu::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return (!FileData.IsEmpty() && FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0) ? ECanCreateModelDataStatus::Ok : ECanCreateModelDataStatus::FailFileIdNotSupported;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTCpu::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	if (CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform) != ECanCreateModelDataStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeORTCpu cannot create the model data with id %s (Filetype: %s)"), *FileId.ToString(EGuidFormats::Digits).ToLower(), *FileType);
		return {};
	}

	FNNEModelRaw InputModel{TArray<uint8>{FileData}, ENNEInferenceFormat::ONNX};
	if (!UE::NNERuntimeORT::Private::OrtHelper::OptimizeModel(InputModel, ENNEInferenceFormat::ORT))
	{
		return {};
	}

	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTCpu::GUID;
	Writer << UNNERuntimeORTCpu::Version;
	Writer.Serialize(InputModel.Data.GetData(), InputModel.Data.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeORTCpu::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTCpu::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTCpu::Version);
}

void UNNERuntimeORTCpu::Init()
{
#if WITH_EDITOR
	check(!ORTEnvironment.IsValid());
	ORTEnvironment = MakeShared<Ort::Env>();
#endif // WITH_EDITOR
}

FString UNNERuntimeORTCpu::GetRuntimeName() const
{
	return TEXT("NNERuntimeORTCpu");
}

UNNERuntimeORTCpu::ECanCreateModelCPUStatus UNNERuntimeORTCpu::CanCreateModelCPU(const TObjectPtr<UNNEModelData> ModelData) const
{
#if WITH_EDITOR
	check(ModelData != nullptr);

	constexpr int32 GuidSize = sizeof(UNNERuntimeORTCpu::GUID);
	constexpr int32 VersionSize = sizeof(UNNERuntimeORTCpu::Version);
	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return ECanCreateModelCPUStatus::Fail;
	}

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTCpu::GUID), GuidSize) == 0;
	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTCpu::Version), VersionSize) == 0;

	return bResult ? ECanCreateModelCPUStatus::Ok : ECanCreateModelCPUStatus::Fail;
#else
	return ECanCreateModelCPUStatus::Fail;
#endif // WITH_EDITOR
}

TSharedPtr<UE::NNE::IModelCPU> UNNERuntimeORTCpu::CreateModelCPU(const TObjectPtr<UNNEModelData> ModelData)
{
#if WITH_EDITOR
	check(ModelData != nullptr);
	check(ORTEnvironment.IsValid());

	if (CanCreateModelCPU(ModelData) != ECanCreateModelCPUStatus::Ok)
	{
		UE_LOG(LogNNE, Warning, TEXT("UNNERuntimeORTCpu cannot create a model from the model data with id %s"), *ModelData->GetFileId().ToString(EGuidFormats::Digits));
		return TSharedPtr<UE::NNE::IModelCPU>();
	}

	const TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());
	check(SharedData.IsValid());

	UE::NNE::IModelCPU* IModel = static_cast<UE::NNE::IModelCPU*>(new UE::NNERuntimeORT::Private::FModelORTCpu(ORTEnvironment, SharedData));
	check(IModel != nullptr);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), SharedData->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelCPU>(IModel);
#else
	return {};
#endif // WITH_EDITOR
}

/*
 * UNNERuntimeORTDmlEditor
 */
UNNERuntimeORTDmlEditor::UNNERuntimeORTDmlEditor()
{
	Impl = MakeUnique<UE::NNERuntimeORT::Private::NNERuntimeORTDmlImpl>(GUID, Version);
}

void UNNERuntimeORTDmlEditor::Init()
{
	Impl->Init();
}

FString UNNERuntimeORTDmlEditor::GetRuntimeName() const
{
	return Impl->GetRuntimeName();
}

UNNERuntimeORTDmlEditor::ECanCreateModelDataStatus UNNERuntimeORTDmlEditor::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return Impl->CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTDmlEditor::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	return Impl->CreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

FString UNNERuntimeORTDmlEditor::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return Impl->GetModelDataIdentifier(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

UNNERuntimeORTDmlEditor::ECanCreateModelGPUStatus UNNERuntimeORTDmlEditor::CanCreateModelGPU(const TObjectPtr<UNNEModelData> ModelData) const
{
	return Impl->CanCreateModelGPU(ModelData);
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeORTDmlEditor::CreateModelGPU(const TObjectPtr<UNNEModelData> ModelData)
{
	return Impl->CreateModelGPU(ModelData);
}

UNNERuntimeORTDmlEditor::ECanCreateModelRDGStatus UNNERuntimeORTDmlEditor::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const
{
	return Impl->CanCreateModelRDG(ModelData);
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeORTDmlEditor::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData)
{
	return Impl->CreateModelRDG(ModelData);
}

/*
 * UNNERuntimeORTDml
 */
UNNERuntimeORTDml::UNNERuntimeORTDml()
{
	Impl = MakeUnique<UE::NNERuntimeORT::Private::NNERuntimeORTDmlImpl>(GUID, Version);
}

void UNNERuntimeORTDml::Init()
{
	Impl->Init();
}

FString UNNERuntimeORTDml::GetRuntimeName() const
{
	return Impl->GetRuntimeName();
}

UNNERuntimeORTDml::ECanCreateModelDataStatus UNNERuntimeORTDml::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return Impl->CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTDml::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	return Impl->CreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

FString UNNERuntimeORTDml::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return Impl->GetModelDataIdentifier(FileType, FileData, AdditionalFileData, FileId, TargetPlatform);
}

UNNERuntimeORTDml::ECanCreateModelRDGStatus UNNERuntimeORTDml::CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const
{
	return Impl->CanCreateModelRDG(ModelData);
}

TSharedPtr<UE::NNE::IModelRDG> UNNERuntimeORTDml::CreateModelRDG(TObjectPtr<UNNEModelData> ModelData)
{
	return Impl->CreateModelRDG(ModelData);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTGpu.h"

#include "EngineAnalytics.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "NNERuntimeORTGpuModel.h"
#include "NNERuntimeORTGpuUtils.h"
#include "NNEUtilsModelOptimizer.h"
#include "NNEAttributeMap.h"
#include "NNEModelData.h"
#include "NNEModelOptimizerInterface.h"
#include "NNEProfilingTimer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NNERuntimeORTGpu)

FGuid UNNERuntimeORTGpuImpl::GUID = FGuid((int32)'O', (int32)'G', (int32)'P', (int32)'U');
int32 UNNERuntimeORTGpuImpl::Version = 0x00000001;

bool UNNERuntimeORTGpuImpl::CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileType.Compare("onnx", ESearchCase::IgnoreCase) == 0;
}

TSharedPtr<UE::NNE::FSharedModelData> UNNERuntimeORTGpuImpl::CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform)
{
	if (!CanCreateModelData(FileType, FileData, AdditionalFileData, FileId, TargetPlatform))
	{
		return {};
	}

	TUniquePtr<UE::NNE::Internal::IModelOptimizer> Optimizer = UE::NNEUtils::Internal::CreateONNXToONNXModelOptimizer();

	FNNEModelRaw InputModel;
	InputModel.Data = FileData;
	InputModel.Format = ENNEInferenceFormat::ONNX;
	FNNEModelRaw OutputModel;
	UE::NNE::Internal::FOptimizerOptionsMap Options;
	if (!Optimizer->Optimize(InputModel, OutputModel, Options))
	{
		return {};
	}

	int32 GuidSize = sizeof(UNNERuntimeORTGpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTGpuImpl::Version);
	TArray<uint8> Result;
	FMemoryWriter Writer(Result);
	Writer << UNNERuntimeORTGpuImpl::GUID;
	Writer << UNNERuntimeORTGpuImpl::Version;
	Writer.Serialize(OutputModel.Data.GetData(), OutputModel.Data.Num());

	return MakeShared<UE::NNE::FSharedModelData>(MakeSharedBufferFromArray(MoveTemp(Result)), 0);
}

FString UNNERuntimeORTGpuImpl::GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const
{
	return FileId.ToString(EGuidFormats::Digits) + "-" + UNNERuntimeORTGpuImpl::GUID.ToString(EGuidFormats::Digits) + "-" + FString::FromInt(UNNERuntimeORTGpuImpl::Version);
}

void UNNERuntimeORTGpuImpl::Init(ENNERuntimeORTGpuProvider InProvider)
{
	check(!ORTEnvironment.IsValid());
	ORTEnvironment = MakeUnique<Ort::Env>();

	Provider = InProvider;
}

FString UNNERuntimeORTGpuImpl::GetRuntimeName() const
{
	switch (Provider)
	{
		case ENNERuntimeORTGpuProvider::Dml:  return TEXT("NNERuntimeORTDml");
		case ENNERuntimeORTGpuProvider::Cuda: return TEXT("NNERuntimeORTCuda");
		default:   return TEXT("NNERuntimeORT_NONE");
	}
}

#if PLATFORM_WINDOWS
bool UNNERuntimeORTGpuImpl::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	check(ModelData != nullptr);

	int32 GuidSize = sizeof(UNNERuntimeORTGpuImpl::GUID);
	int32 VersionSize = sizeof(UNNERuntimeORTGpuImpl::Version);
	TSharedPtr<UE::NNE::FSharedModelData> SharedData = ModelData->GetModelData(GetRuntimeName());

	if (!SharedData.IsValid())
	{
		return false;
	}

	TConstArrayView<uint8> Data = SharedData->GetView();

	if (Data.Num() <= GuidSize + VersionSize)
	{
		return false;
	}

	static const FGuid DeprecatedGUID = FGuid((int32)'O', (int32)'D', (int32)'M', (int32)'L');

	bool bResult = FGenericPlatformMemory::Memcmp(&(Data[0]), &(UNNERuntimeORTGpuImpl::GUID), GuidSize) == 0;
	bResult |= FGenericPlatformMemory::Memcmp(&(Data[0]), &(DeprecatedGUID), GuidSize) == 0;

	bResult &= FGenericPlatformMemory::Memcmp(&(Data[GuidSize]), &(UNNERuntimeORTGpuImpl::Version), VersionSize) == 0;
	return bResult;
}

TSharedPtr<UE::NNE::IModelGPU> UNNERuntimeORTGpuImpl::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	check(ModelData != nullptr);
	check(ORTEnvironment.IsValid());

	if (!CanCreateModelGPU(ModelData))
	{
		return TSharedPtr<UE::NNE::IModelGPU>();
	}

	UE::NNE::IModelGPU* IModel = nullptr;
	TSharedPtr<UE::NNE::FSharedModelData> Data = ModelData->GetModelData(GetRuntimeName());
	check(Data.IsValid());

	switch (Provider)
	{
		case ENNERuntimeORTGpuProvider::Dml:  
			IModel = static_cast<UE::NNE::IModelGPU*>(new UE::NNERuntimeORTGpu::Private::FModelORTDml(ORTEnvironment.Get(), Data));
			break;
		case ENNERuntimeORTGpuProvider::Cuda: 
			IModel = static_cast<UE::NNE::IModelGPU*>(new UE::NNERuntimeORTGpu::Private::FModelORTCuda(ORTEnvironment.Get(), Data));
			break;
		default:
			UE_LOG(LogNNE, Error, TEXT("Failed to create model for ORT GPU runtime, unsupported provider. Runtime will not be functional."));
			return TSharedPtr<UE::NNE::IModelGPU>();
	}

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("HashedRuntimeName"), FMD5::HashAnsiString(*GetRuntimeName()),
			TEXT("ModelDataSize"), Data->GetView().Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.CreateModel"), Attributes);
	}

	return TSharedPtr<UE::NNE::IModelGPU>(IModel);
}

#else // PLATFORM_WINDOWS

bool UNNERuntimeORTGpuImpl::CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const
{
	return false;
}

TUniquePtr<UE::NNE::IModelGPU> UNNERuntimeORTGpuImpl::CreateModelGPU(TObjectPtr<UNNEModelData> ModelData)
{
	return TUniquePtr<UE::NNE::IModelGPU>();
}

#endif // PLATFORM_WINDOWS
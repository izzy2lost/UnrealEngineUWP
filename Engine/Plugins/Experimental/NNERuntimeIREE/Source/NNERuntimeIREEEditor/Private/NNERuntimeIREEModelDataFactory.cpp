// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEModelDataFactory.h"

#include "CoreMinimal.h"
#include "Editor.h"
#include "EngineAnalytics.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "NNEModelData.h"
#include "NNERuntimeIREEMetaData.h"
#include "Serialization/MemoryWriter.h"
#include "Subsystems/ImportSubsystem.h"

UNNERuntimeIREEModelDataFactory::UNNERuntimeIREEModelDataFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	bCreateNew = false;
	bEditorImport = true;
	SupportedClass = UNNEModelData::StaticClass();
	ImportPriority = DefaultImportPriority;
	Formats.Add("mlir;Multi-Level Intermediate Representation Format");
}

UObject* UNNERuntimeIREEModelDataFactory::FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR * Type, const uint8 *& Buffer, const uint8 * BufferEnd, FFeedbackContext* Warn)
{
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, Class, InParent, Name, Type);

	if (!Type || !Buffer || !BufferEnd || BufferEnd - Buffer <= 0)
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
		return nullptr;
	}

	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd - Buffer);
	FString FileDataString = "";
	FileDataString.AppendChars((char*)BufferView.GetData(), BufferView.Num());

	UNNERuntimeIREEModuleMetaData* ModuleMetaData = NewObject<UNNERuntimeIREEModuleMetaData>();
	if (!ModuleMetaData->ParseFromString(FileDataString) || ModuleMetaData->FunctionMetaData.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UNNERuntimeIREEModelDataFactory failed to parse the models meta data"));
		return nullptr;
	}

	TArray<uint8> MetaDataByteArray;
	FMemoryWriter Writer(MetaDataByteArray);
	ModuleMetaData->Serialize(Writer);

	TMap<FString, TConstArrayView<uint8>> AdditionalFileData;
	AdditionalFileData.Add("IREEModuleMetaData", MetaDataByteArray);

	UNNEModelData* ModelData = NewObject<UNNEModelData>(InParent, Class, Name, Flags);
	ModelData->Init(Type, BufferView, AdditionalFileData);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, ModelData);

	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes = MakeAnalyticsEventAttributeArray(
			TEXT("PlatformName"), UGameplayStatics::GetPlatformName(),
			TEXT("FactoryName"), TEXT("UNNERuntimeIREEModelDataFactory"),
			TEXT("ModelFileSize"), BufferView.Num()
		);
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("NeuralNetworkEngine.FactoryCreateBinary"), Attributes);
	}

	return ModelData;
}

bool UNNERuntimeIREEModelDataFactory::FactoryCanImport(const FString & Filename)
{
	return Filename.EndsWith(FString("mlir"));
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEModelDataFactory.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorFramework/AssetImportData.h"
#include "EngineAnalytics.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "NNE.h"
#include "NNEModelData.h"
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

	UNNEModelData* ModelData = NewObject<UNNEModelData>(InParent, Class, Name, Flags);
	TConstArrayView<uint8> BufferView = MakeArrayView(Buffer, BufferEnd-Buffer);
	ModelData->Init(Type, BufferView);

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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionStaticLightingBuilder.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Algo/ForEach.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"

#include "EngineUtils.h"
#include "EngineModule.h"
#include "SourceControlHelpers.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

#include "Editor.h"

#include "WorldPartition/WorldPartitionHelpers.h"

#include "LightingBuildOptions.h"

#include "HAL/IConsoleManager.h"
#include "Engine/MapBuildDataRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionStaticLightingBuilder, All, All);

UWorldPartitionStaticLightingBuilder::UWorldPartitionStaticLightingBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)	
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	SourceControlHelper = nullptr;
	
	bBuildVLMOnly = FParse::Param(FCommandLine::Get(), TEXT("BuildVLM"));

	QualityLevel = ELightingBuildQuality::Quality_Preview;

	BuildOptions |= FParse::Param(FCommandLine::Get(), TEXT("Submit")) ? EWPStaticLightingBuildStep::WPSL_Submit : EWPStaticLightingBuildStep::None;
	BuildOptions |= bBuildVLMOnly ?  (EWPStaticLightingBuildStep::WPSL_Build): EWPStaticLightingBuildStep::None;
	

	// Default behavior without any option is to build VLM only (will change when we add Lightmaps support)
	if (BuildOptions == EWPStaticLightingBuildStep::None)
	{
		BuildOptions = EWPStaticLightingBuildStep::WPSL_Build; 
		bBuildVLMOnly = true;
	}

	// Parse quality level and limit to valid values
	int32 QualityLevelVal;
	FParse::Value(FCommandLine::Get(), TEXT("QualityLevel="), QualityLevelVal);
	QualityLevelVal = FMath::Clamp<int32>(QualityLevelVal, Quality_Preview, Quality_Production);
	QualityLevel = (ELightingBuildQuality)QualityLevelVal;
}

bool UWorldPartitionStaticLightingBuilder::RequiresCommandletRendering() const
{
	// The Lightmass export process uses the renderer to generate some data so we need rendering
	return true; 
}

bool UWorldPartitionStaticLightingBuilder::ShouldRunStep(const EWPStaticLightingBuildStep BuildStep) const
{
	return (BuildOptions & BuildStep) == BuildStep;
}

UWorldPartitionBuilder::ELoadingMode UWorldPartitionStaticLightingBuilder::GetLoadingMode() const 
{
	check(bBuildVLMOnly);
	return ELoadingMode::EntireWorld;
}

bool UWorldPartitionStaticLightingBuilder::ValidateParams() const
{
	return true;
}

bool UWorldPartitionStaticLightingBuilder::PreWorldInitialization(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	return ValidateParams();	
}

bool UWorldPartitionStaticLightingBuilder::PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{			
	return true;
}


bool UWorldPartitionStaticLightingBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	WorldPartition = World->GetWorldPartition();
	check(WorldPartition);

	bool bRet = true;

	SourceControlHelper = MakeUnique<FSourceControlHelper>(PackageHelper, ModifiedFiles);

	if (ShouldRunStep(EWPStaticLightingBuildStep::WPSL_Build))
	{
		bRet = RunForVLM(World, InCellInfo, PackageHelper);		
	}

	if (bRet && ShouldRunStep(EWPStaticLightingBuildStep::WPSL_Submit))
	{
		bRet = Submit(World, PackageHelper);
	}

	SourceControlHelper.Reset();

	return bRet;
}

bool UWorldPartitionStaticLightingBuilder::Submit(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{	
	// Wait for pending async file writes before submitting
	UPackage::WaitForAsyncFileWrites();

	const FString ChangeDescription = FString::Printf(TEXT("Rebuilt static lighting for %s"), *World->GetPackage()->GetName());
	return OnFilesModified(ModifiedFiles.GetAllFiles(), ChangeDescription);
}

bool UWorldPartitionStaticLightingBuilder::RunForVLM(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	bool bRet = true;
	check (bBuildVLMOnly);

	UE_LOG(LogWorldPartitionStaticLightingBuilder, Verbose, TEXT("Building Volumetric Lightmaps for %s"), *World->GetName());
	 
	// Invoke static lighting computation
	FLightingBuildOptions LightingOptions;
	LightingOptions.QualityLevel =  QualityLevel;

	bool bLightingBuildFailed = false;

	auto BuildFailedDelegate = [&bLightingBuildFailed, &World]() {
		// WP-TODO!! better error message
		UE_LOG(LogWorldPartitionStaticLightingBuilder, Error, TEXT("[REPORT] Failed building lighting for %s"), *World->GetName());
		bLightingBuildFailed = true;
	};

	FDelegateHandle BuildFailedDelegateHandle = FEditorDelegates::OnLightingBuildFailed.AddLambda(BuildFailedDelegate);
	
	GEditor->BuildLighting(LightingOptions);
	while (GEditor->IsLightingBuildCurrentlyRunning())
	{
		GEditor->UpdateBuildLighting();
	}
		
	if (!bLightingBuildFailed)
	{
		// Save the AMapBuildData actors + MapBuildData we just updated
		TArray<UPackage*> PackagesToSave;

		PackagesToSave.Add(World->PersistentLevel->MapBuildData->GetPackage());
	
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
	}
	else
	{
		bRet = false;
	}

	FEditorDelegates::OnLightingBuildFailed.Remove(BuildFailedDelegateHandle);
		
	return bRet;
}

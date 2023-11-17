// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MapTestSpawner.h"

#if ENABLE_MAPSPAWNER_TEST
#include "Commands/TestCommands.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "LevelEditorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"

namespace {

static const FString TempMapDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("CQTestMapTemp"));

/**
 * Generates a unique random 8 character map name.
 */
FString GenerateUniqueMapName()
{
	FString UniqueMapName = FGuid::NewGuid().ToString();
	UniqueMapName.LeftInline(8);

	return UniqueMapName;
}

/**
 * Cleans up all created resources.
 */
void CleanupTempResources()
{
	bool bDirectoryMustExist = true;
	bool bRemoveRecursively = true;
	bool bWasDeleted = IFileManager::Get().DeleteDirectory(*TempMapDirectory, bDirectoryMustExist, bRemoveRecursively);
	check(bWasDeleted);
}

} //anonymous

TUniquePtr<FMapTestSpawner> FMapTestSpawner::CreateFromTempLevel(FTestCommandBuilder& InCommandBuilder)
{
	FString MapName = GenerateUniqueMapName();
	FString MapPath = FPaths::Combine(TempMapDirectory, MapName);
	FString NewLevelPackage = FPackageName::FilenameToLongPackageName(MapPath);

	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	bool bWasTempLevelCreated = LevelEditorSubsystem->NewLevel(NewLevelPackage);
	check(bWasTempLevelCreated);

	TUniquePtr<FMapTestSpawner> Spawner = MakeUnique<FMapTestSpawner>(TempMapDirectory, MapName);
	InCommandBuilder.OnTearDown([&]() {
		// Create a new map to free up the reference to the map used during testing before cleaning up all temporary resources
		FAutomationEditorCommonUtils::CreateNewMap();
		CleanupTempResources();
	});
	return MoveTemp(Spawner);
}

void FMapTestSpawner::AddWaitUntilLoadedCommand(FAutomationTestBase* TestRunner)
{
	check(PieWorld == nullptr);

	const FString FileName = FString::Printf(TEXT("%s.%s"), *MapName, *MapName);
	const FString Path = FPaths::Combine(MapDirectory, FileName);
	bool bOpened = AutomationOpenMap(Path);
	check(bOpened);

	ADD_LATENT_AUTOMATION_COMMAND(FWaitUntil(*TestRunner, [&]() -> bool {
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			if (((Context.WorldType == EWorldType::PIE) || (Context.WorldType == EWorldType::Game)) && (Context.World() != nullptr))
			{
				PieWorld = Context.World();
				return true;
			}
		}

		return false;
	}));
}

UWorld* FMapTestSpawner::CreateWorld()
{
	checkf(PieWorld, TEXT("Must call AddWaitUntilLoadedCommand in BEFORE_TEST"));
	return PieWorld;
}

APawn* FMapTestSpawner::FindFirstPlayerPawn()
{
	return GetWorld().GetFirstPlayerController()->GetPawn();
}

#endif // ENABLE_MAPSPAWNER_TEST
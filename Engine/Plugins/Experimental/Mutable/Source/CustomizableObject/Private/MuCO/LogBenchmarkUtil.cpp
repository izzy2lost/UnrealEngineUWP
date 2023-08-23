// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/LogBenchmarkUtil.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMesh.h"
#include "TextureResource.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableInstancePrivateData.h"
#include "MuCO/CustomizableSkeletalComponent.h"


extern ENGINE_API UEngine* GEngine;

#if WITH_EDITOR
extern UNREALED_API UEditorEngine* GEditor;
#endif

DEFINE_STAT(STAT_MutableNumSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumCachedSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumAllocatedSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumInstancesLOD0);
DEFINE_STAT(STAT_MutableNumInstancesLOD1);
DEFINE_STAT(STAT_MutableNumInstancesLOD2);
DEFINE_STAT(STAT_MutableNumTextures);
DEFINE_STAT(STAT_MutableNumCachedTextures);
DEFINE_STAT(STAT_MutableNumAllocatedTextures);
DEFINE_STAT(STAT_MutableTextureResourceMemory);
DEFINE_STAT(STAT_MutableTextureGeneratedMemory);
DEFINE_STAT(STAT_MutableTextureCacheMemory);
DEFINE_STAT(STAT_MutablePendingInstanceUpdates);
DEFINE_STAT(STAT_MutableAbandonedInstanceUpdates);
DEFINE_STAT(STAT_MutableInstanceBuildTime);
DEFINE_STAT(STAT_MutableInstanceBuildTimeAvrg);

bool LogBenchmarkUtil::bLoggingActive = false;


void LogBenchmarkUtil::StartLogging()
{
	bLoggingActive = true;
}

void LogBenchmarkUtil::ShutdownAndSaveResults()
{
	if (bLoggingActive)
	{
		FString SaveDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) + "/Logs";
		FString FileName = FString("BenchmarkResult.txt");
		FString AbsoluteFilePath = SaveDirectory + "/" + FileName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		if (PlatformFile.CreateDirectoryTree(*SaveDirectory))
		{
			FString CurrentTime = FDateTime::Now().ToString();
			FString DateStamp = LINE_TERMINATOR + FString("[" + CurrentTime + "]LogBenchmark: (string) ") + FString("benchmark_results_location : ") + SaveDirectory;
			FFileHelper::SaveStringToFile(DateStamp, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi);

			FString KeyName;
			int32 MaxValue;
			float AverageValue;

			// Iterate over the integer stats and log them to a file
			for (TPair<FName, IntStat> Ist : GetInstance().IntStats)
			{
				CurrentTime = FDateTime::Now().ToString();

				KeyName = FString(Ist.Key.ToString());
				MaxValue = Ist.Value.Max;
				AverageValue = (long double)Ist.Value.Total / (long double)Ist.Value.TotalCalls;

				FString TextToSave = LINE_TERMINATOR;
				TextToSave += FString("[" + CurrentTime + "]");
				TextToSave += FString("LogBenchmark: (int) peak_");
				TextToSave += FString::Printf(TEXT("%s : %lld"), *KeyName, MaxValue);
				TextToSave += LINE_TERMINATOR;
				TextToSave += FString("[" + CurrentTime + "]");
				TextToSave += FString("LogBenchmark: (float) average_");
				TextToSave += FString::Printf(TEXT("%s : %f"), *KeyName, AverageValue);

				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
			}

			// Iterate over the Float stats and log them to a file
			for (TPair<FName, FloatStat> dst : GetInstance().FloatStats)
			{
				CurrentTime = FDateTime::Now().ToString();

				KeyName = FString(dst.Key.ToString());
				MaxValue = dst.Value.Max;
				AverageValue = dst.Value.Total / dst.Value.TotalCalls;

				FString TextToSave = LINE_TERMINATOR;
				TextToSave += FString("[" + CurrentTime + "]");
				TextToSave += FString("LogBenchmark: (int) peak_");
				TextToSave += FString::Printf(TEXT("%s : %lld"), *KeyName, MaxValue);
				TextToSave += LINE_TERMINATOR;
				TextToSave += FString("[" + CurrentTime + "]");
				TextToSave += FString("LogBenchmark: (float) average_");
				TextToSave += FString::Printf(TEXT("%s : %f"), *KeyName, AverageValue);

				FFileHelper::SaveStringToFile(TextToSave, *AbsoluteFilePath, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
			}
		}

		bLoggingActive = false;
	}
}

void LogBenchmarkUtil::UpdateStat(const FName& Stat, int32 Value)
{
	if (bLoggingActive)
	{
		if (GetInstance().IntStats.Contains(Stat))
		{
			GetInstance().IntStats[Stat] += Value;
		}
		else
		{
			GetInstance().IntStats.Add(Stat) += Value;
		}
	}
}

void LogBenchmarkUtil::UpdateStat(const FName & Stat, double Value)
{
	UpdateStat(Stat, (long double)Value);
}

void LogBenchmarkUtil::UpdateStat(const FName& Stat, long double Value)
{
	if (bLoggingActive)
	{
		if (GetInstance().FloatStats.Contains(Stat))
		{
			GetInstance().FloatStats[Stat] += Value;
		}
		else
		{
			GetInstance().FloatStats.Add(Stat) += Value;
		}
	}
}

bool LogBenchmarkUtil::IsLoggingActive()
{
	return bLoggingActive;
}


void LogBenchmarkUtil::UpdateStats(FMutableStats& StatsToUpdate, const TArray< TObjectPtr<UTexture2D> >& ProtectedCachedTextures)
{
	if (LogBenchmarkUtil::IsLoggingActive())
	{
		uint64 SizeCache = 0;

		for (const UTexture2D* CachedTextures : ProtectedCachedTextures)
		{
			if (CachedTextures)
			{
				SizeCache += CachedTextures->CalcTextureMemorySizeEnum(TMC_AllMips);
			}
		}

		SET_DWORD_STAT(STAT_MutableTextureCacheMemory, SizeCache / 1024.f);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bLogEnabled = true;
#else
	bool bLogEnabled = LogBenchmarkUtil::IsLoggingActive();
#endif
	if (bLogEnabled)
	{
		const int32& MutablePendingInstanceWorkCount = StatsToUpdate.MutablePendingInstanceWorkCount;
		StatsToUpdate.CountAllocatedSkeletalMesh = 0;

		int32 CountLOD0 = 0;
		int32 CountLOD1 = 0;
		int32 CountLOD2 = 0;
		int32 CountTotal = 0;

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				++CountTotal;

				for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++ComponentIndex)
				{
					if (CustomizableObjectInstance->SkeletalMeshes[ComponentIndex] && CustomizableObjectInstance->SkeletalMeshes[ComponentIndex]->GetResourceForRendering())
					{
						StatsToUpdate.CountAllocatedSkeletalMesh++;

						if (CustomizableObjectInstance->GetCurrentMinLOD() < 1)
						{
							++CountLOD0;
						}
						else if (CustomizableObjectInstance->GetCurrentMaxLOD() < 2)
						{
							++CountLOD1;
						}
						else
						{
							++CountLOD2;
						}
					}
				}
			}
		}

		StatsToUpdate.NumInstances = CountLOD0 + CountLOD1 + CountLOD2;
		StatsToUpdate.TotalInstances = CountTotal;
		StatsToUpdate.NumPendingInstances = MutablePendingInstanceWorkCount;
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutablePendingInstanceWorkCount);

		uint64 Size = 0;
		uint32 CountAllocated = 0;
		for (TWeakObjectPtr<UTexture2D>& Tracker : StatsToUpdate.TextureTrackerArray)
		{
			if (Tracker.IsValid() && Tracker->GetResource())
			{
				CountAllocated++;

				if (Tracker->GetResource()->TextureRHI)
				{
					Size += Tracker->CalcTextureMemorySizeEnum(TMC_AllMips);
				}
			}
		}

		StatsToUpdate.TextureMemoryUsed = int64_t(Size / 1024);

		uint64 SizeGenerated = 0;
		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate() && CustomizableObjectInstance->HasAnySkeletalMesh())
			{
				bool bHasResourceForRendering = false;
				for (int32 MeshIndex = 0; !bHasResourceForRendering && MeshIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++MeshIndex)
				{
					bHasResourceForRendering = CustomizableObjectInstance->GetSkeletalMesh(MeshIndex) && CustomizableObjectInstance->GetSkeletalMesh(MeshIndex)->GetResourceForRendering();
				}

				if (bHasResourceForRendering)
				{
					for (const FGeneratedTexture& GeneratedTextures : CustomizableObjectInstance->GetPrivate()->GeneratedTextures)
					{
						if (GeneratedTextures.Texture)
						{
							check(GeneratedTextures.Texture != nullptr);
							SizeGenerated += GeneratedTextures.Texture->CalcTextureMemorySizeEnum(TMC_AllMips);
						}
					}
				}
			}
		}

		if (LogBenchmarkUtil::IsLoggingActive())
		{
			LogBenchmarkUtil::UpdateStat("customizable_objects", (int32)StatsToUpdate.CountAllocatedSkeletalMesh);
			LogBenchmarkUtil::UpdateStat("pending_instance_updates", MutablePendingInstanceWorkCount);
			LogBenchmarkUtil::UpdateStat("allocated_textures", (int32)CountAllocated);
			LogBenchmarkUtil::UpdateStat("texture_resource_memory", (long double)Size / 1048576.0L);
			LogBenchmarkUtil::UpdateStat("texture_generated_memory", (long double)SizeGenerated / 1048576.0L);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD0, CountLOD0);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD1, CountLOD1);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD2, CountLOD2);
		SET_DWORD_STAT(STAT_MutableNumAllocatedSkeletalMeshes, StatsToUpdate.CountAllocatedSkeletalMesh);
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutablePendingInstanceWorkCount);
		SET_DWORD_STAT(STAT_MutableNumAllocatedTextures, CountAllocated);
		SET_DWORD_STAT(STAT_MutableTextureResourceMemory, Size / 1024.f);
		SET_DWORD_STAT(STAT_MutableTextureGeneratedMemory, SizeGenerated / 1024.f);
#endif

#if WITH_EDITORONLY_DATA
		if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
		{
			if (GEngine)
			{
				bool bFoundPlayer = false;
				int32 MsgIndex = 15820; // Arbitrary big value to prevent collisions with other on-screen messages

				for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
				{
					AActor* ParentActor = CustomizableSkeletalComponent->GetAttachmentRootActor();
					UCustomizableObjectInstance* Instance = CustomizableSkeletalComponent->CustomizableObjectInstance;

					APawn* PlayerPawn = nullptr;
					if (UWorld* World = CustomizableSkeletalComponent->GetWorld())
					{
						PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
					}

					if (ParentActor && (ParentActor == PlayerPawn) && Instance)
					{
						bFoundPlayer = true;

						FString TagString;
						const FGameplayTagContainer& Tags = Instance->GetAnimationGameplayTags();

						for (const FGameplayTag& Tag : Tags)
						{
							TagString += !TagString.IsEmpty() ? FString(TEXT(", ")) : FString();
							TagString += Tag.ToString();
						}

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, TEXT("Animation tags: ") + TagString);

						check(Instance->GetPrivate() != nullptr);
						FCustomizableInstanceComponentData* ComponentData = Instance->GetPrivate()->GetComponentData(CustomizableSkeletalComponent->ComponentIndex);

						if (ComponentData)
						{
							for (TPair<FName, TSoftClassPtr<UAnimInstance>>& Entry : ComponentData->AnimSlotToBP)
							{
								FString AnimBPSlot;

								AnimBPSlot += Entry.Key.ToString() + FString("-") + Entry.Value.GetAssetName();
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, AnimBPSlot);
							}
						}

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, TEXT("Slots-AnimBP: "));

						if (ComponentData)
						{
							if (ComponentData->MeshPartPaths.IsEmpty())
							{
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta,
									TEXT("No meshes found. In order to see the meshes compile the pawn's root CustomizableObject after the 'mutable.EnableMutableAnimInfoDebugging 1' command has been run."));
							}

							for (const FString& MeshPath : ComponentData->MeshPartPaths)
							{
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta, MeshPath);
							}
						}

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta, TEXT("Meshes: "));

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Cyan,
							TEXT("Player Pawn Mutable Mesh/Animation info for component ") + FString::Printf(TEXT("%d"),
								CustomizableSkeletalComponent->ComponentIndex));
					}
				}

				if (!bFoundPlayer)
				{
					GEngine->AddOnScreenDebugMessage(MsgIndex, .0f, FColor::Yellow, TEXT("Mutable Animation info: N/A"));
				}
			}
		}
#endif
	}
}


void LogBenchmarkUtil::UpdateBuildTimeStats(FMutableStats& StatsToUpdate, double StartUpdateTime)
{
	if (LogBenchmarkUtil::IsLoggingActive())
	{
		double DeltaSeconds = FPlatformTime::Seconds() - StartUpdateTime;
		int32 DeltaMs = int32(DeltaSeconds * 1000);

		LogBenchmarkUtil::UpdateStat("customizable_instance_build_time", DeltaMs);
		StatsToUpdate.TotalBuildMs += DeltaMs;
		StatsToUpdate.TotalBuiltInstances++;
		SET_DWORD_STAT(STAT_MutableInstanceBuildTime, DeltaMs);
		SET_DWORD_STAT(STAT_MutableInstanceBuildTimeAvrg, StatsToUpdate.TotalBuildMs / StatsToUpdate.TotalBuiltInstances);
	}
}

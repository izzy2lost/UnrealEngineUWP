// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Stats/Stats.h"

// Mutable stats
DECLARE_STATS_GROUP(TEXT("Mutable"), STATGROUP_Mutable, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Created Mutable Skeletal Meshes"), STAT_MutableNumSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Cached Mutable Skeletal Meshes"), STAT_MutableNumCachedSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Allocated Mutable Skeletal Meshes"), STAT_MutableNumAllocatedSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 0"), STAT_MutableNumInstancesLOD0, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 1"), STAT_MutableNumInstancesLOD1, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 2 or more"), STAT_MutableNumInstancesLOD2, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Created Mutable Textures"), STAT_MutableNumTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Cached Mutable Textures"), STAT_MutableNumCachedTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Allocated Mutable Textures"), STAT_MutableNumAllocatedTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Resource Memory"), STAT_MutableTextureResourceMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Generated Memory"), STAT_MutableTextureGeneratedMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Locked Memory"), STAT_MutableTextureCacheMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Pending Instance Updates"), STAT_MutablePendingInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Abandoned Instance Updates"), STAT_MutableAbandonedInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Last Instance Build Time"), STAT_MutableInstanceBuildTime, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Avrg Instance Build Time"), STAT_MutableInstanceBuildTimeAvrg, STATGROUP_Mutable, );

struct FMutableStats;

class LogBenchmarkUtil
{
public:
	static void StartLogging();
	static void ShutdownAndSaveResults();

	//! Update the stats logged in unreal's stats system.
	static void UpdateStats(FMutableStats& StatsToUpdate, const TArray< TObjectPtr<class UTexture2D> >& ProtectedCachedTextures);

	static void UpdateBuildTimeStats(FMutableStats& StatsToUpdate, double StartUpdateTime);

	static void UpdateStat(const FName& Stat, int32 Value);
	static void UpdateStat(const FName& Stat, double Value);
	static void UpdateStat(const FName& Stat, long double Value);

	static bool IsLoggingActive();

private:
	struct IntStat
	{
		int32 Total;
		int32 Max;
		int32 TotalCalls;

		IntStat& operator+=(const int32& Rhs)
		{
			Total += Rhs;
			Max = FGenericPlatformMath::Max(Max, Rhs);
			TotalCalls++;
			return *this;
		}
	};

	struct FloatStat
	{
		long double Total;
		long double Max;
		int32 TotalCalls;

		FloatStat& operator+=(const long double& Rhs)
		{
			Total += Rhs;
			Max = FGenericPlatformMath::Max(Max, Rhs);
			TotalCalls++;
			return *this;
		}
	};

	static bool bLoggingActive;
	TMap<FName, IntStat> IntStats;
	TMap<FName, FloatStat> FloatStats;

	static LogBenchmarkUtil &GetInstance()
	{
		static LogBenchmarkUtil Instance;
		return Instance;
	}

	LogBenchmarkUtil() = default;
};

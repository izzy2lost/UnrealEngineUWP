// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshDrawCommandStats.h"
#include "InstanceCulling/InstanceCullingContext.h"
#include "RenderGraph.h"
#include "RendererOnScreenNotification.h"

#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RendererModule.h"

#if MESH_DRAW_COMMAND_STAT_COLLECTION

FMeshDrawCommandStatsManager* FMeshDrawCommandStatsManager::Instance = nullptr;

void FMeshDrawCommandStatsManager::CreateInstance()
{
	check(Instance == nullptr);
	Instance = new FMeshDrawCommandStatsManager();
}

DECLARE_STATS_GROUP(TEXT("MeshDrawCommandStats"), STATGROUP_Culling, STATCAT_Advanced);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total Rendered Triangles"), STAT_Culling_TotalNumTriangles, STATGROUP_Culling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Rendered Instances"), STAT_Culling_TotalNumInstances, STATGROUP_Culling);
DECLARE_DWORD_COUNTER_STAT(TEXT("InstanceCulling Indirect Rendered Triangles"), STAT_Culling_InstanceCullingIndirectNumTriangles, STATGROUP_Culling);
DECLARE_DWORD_COUNTER_STAT(TEXT("InstanceCulling Indirect Rendered Instances"), STAT_Culling_InstanceCullingIndirectNumInstances, STATGROUP_Culling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Custom Indirect Rendered Triangles"), STAT_Culling_CustomIndirectNumTriangles, STATGROUP_Culling);
DECLARE_DWORD_COUNTER_STAT(TEXT("Custom Indirect Rendered Instances"), STAT_Culling_CustomIndirectNumInstances, STATGROUP_Culling);

static TAutoConsoleVariable<int32> CVarShowMeshDrawCommandStats(
	TEXT("r.MeshDrawCommands.Stats"),
	0,
	TEXT("Show on screen mesh draw command stats per stat category.\n")
	TEXT("The stats are accumulated across passes and are post-culling.\n")
	TEXT("Use stat culling to see global culling stats.\n"),
	ECVF_RenderThreadSafe
);

FMeshDrawCommandStatsManager::FFrameData::~FFrameData()
{
	// Collect set of unique readback buffers for deletion (can be shared between MDCs and passes)
	TSet<FRHIGPUBufferReadback*> ReadbackBuffers;
	for (FMeshDrawCommandPassStats* PassStats : PassData)
	{
		if (PassStats->InstanceCullingGPUBufferReadback)
		{
			ReadbackBuffers.Add(PassStats->InstanceCullingGPUBufferReadback);
			PassStats->InstanceCullingGPUBufferReadback = nullptr;
		}
		delete PassStats;
	}
	for (auto Iter = CustomIndirectArgsBufferResults.CreateIterator(); Iter; ++Iter)
	{
		FIndirectArgsBufferResult& CustomArgsBufferResult = Iter.Value();
		if (CustomArgsBufferResult.GPUBufferReadback)
		{
			ReadbackBuffers.Add(CustomArgsBufferResult.GPUBufferReadback);
			CustomArgsBufferResult.GPUBufferReadback = nullptr;
		}
	}
	CustomIndirectArgsBufferResults.Empty();

	// delete all unique readback buffers
	for (FRHIGPUBufferReadback* ReadbackBuffer : ReadbackBuffers)
	{
		delete ReadbackBuffer;
	}
}

void FMeshDrawCommandStatsManager::FFrameData::Validate() const
{
	bool bHasIndirectArgs = false;

	// make sure that each pass which has indirect draws also has a gpu readback buffer to resolve the final used instance count
	for (const FMeshDrawCommandPassStats* PassStats : PassData)
	{
		if (PassStats->bBuildRenderingCommandsCalled)
		{
			bool bUsesInstantCullingIndirectBuffer = false;
			for (const FVisibleMeshDrawCommandStatsData& DrawData : PassStats->DrawData)
			{
				if (DrawData.UseInstantCullingIndirectBuffer > 0)
				{
					bUsesInstantCullingIndirectBuffer = true;
					bHasIndirectArgs = true;
				}

				if (DrawData.CustomIndirectArgsBuffer)
				{
					check(PassStats->CustomIndirectArgsBuffers.Contains(DrawData.CustomIndirectArgsBuffer));
					bHasIndirectArgs = true;
				}
			}

			// either we don't use draw indirect or we don't have a readback buffer
			check(!bUsesInstantCullingIndirectBuffer || PassStats->InstanceCullingGPUBufferReadback != nullptr);
		}
	}

	// Make sure readback has been requested
	check(!bHasIndirectArgs || bIndirectArgReadbackRequested);
}

/**
 * Make sure all GPU readback requests are finished before marking frame as complete
 */
bool FMeshDrawCommandStatsManager::FFrameData::IsCompleted()
{
	for (auto Iter = CustomIndirectArgsBufferResults.CreateIterator(); Iter; ++Iter)
	{
		if (!Iter.Value().GPUBufferReadback->IsReady())
		{
			return false;
		}
	}

	for (FMeshDrawCommandPassStats* PassStats : PassData)
	{
		if (PassStats->InstanceCullingGPUBufferReadback && !PassStats->InstanceCullingGPUBufferReadback->IsReady())
		{
			return false;
		}
	}
	return true;
}

FMeshDrawCommandStatsManager::FMeshDrawCommandStatsManager()
{
	// Tick on and of RT frame
	FCoreDelegates::OnEndFrameRT.AddRaw(this, &FMeshDrawCommandStatsManager::Update);

	// Is it fine to keep the screen message delegate always registered even if we are not showing anything?
	ScreenMessageDelegate = FRendererOnScreenNotification::Get().AddLambda([this](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
		{	
			const bool bShowStats = CVarShowMeshDrawCommandStats->GetInt() == 0 ? false : true;
			if (bShowStats)
			{
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(FString::Printf(TEXT("ResourceType Triangle Count:"), Stats.TotalTriangles / 1000)));
				for (auto Iter = Stats.CategoryStats.CreateConstIterator(); Iter; ++Iter)
				{
					OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(FString::Printf(TEXT("\t%5dK - %s"), Iter->PrimitiveCount / 1000, *(Iter->Category.ToString()))));
				}
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(FString::Printf(TEXT("\t%5dK - TOTAL"), Stats.TotalTriangles / 1000)));
			}
		});
}

FMeshDrawCommandPassStats* FMeshDrawCommandStatsManager::CreatePassStats(FName PassName)
{
	if (!bCollectStats)
	{
		return nullptr;
	}

	FScopeLock ScopeLock(&FrameDataCS);

	FFrameData* FrameData = GetOrAddFrameData();
	FMeshDrawCommandPassStats* PassStats = new FMeshDrawCommandPassStats(PassName);
	FrameData->PassData.Add(PassStats);
	return PassStats;
}

FRHIGPUBufferReadback* FMeshDrawCommandStatsManager::QueueDrawRDGIndirectArgsReadback(FRDGBuilder& GraphBuilder, FRDGBuffer* DrawIndirectArgsRDG)
{
	// TODO: pool the readback buffers
	FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("InstanceCulling.StatsReadbackQuery"));
	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("ReadbackIndirectArgs"), DrawIndirectArgsRDG,
		[GPUBufferReadback, DrawIndirectArgsRDG](FRHICommandList& RHICmdList)
		{
			GPUBufferReadback->EnqueueCopy(RHICmdList, DrawIndirectArgsRDG->GetRHI(), 0u);
		});
	return GPUBufferReadback;
}

void FMeshDrawCommandStatsManager::QueueCustomDrawIndirectArgsReadback(FRHICommandListImmediate& CommandList)
{
	if (!bCollectStats)
	{
		return;
	}

	FScopeLock ScopeLock(&FrameDataCS);

	FFrameData* FrameData = GetOrAddFrameData();
	FrameData->bIndirectArgReadbackRequested = true;

	// Collect set of all unique custom indirect arg buffers
	TSet<FRHIBuffer*> CustomIndirectArgsBuffers;
	for (FMeshDrawCommandPassStats* PassStats : FrameData->PassData)
	{
		CustomIndirectArgsBuffers.Append(PassStats->CustomIndirectArgsBuffers);
	}

	for (FRHIBuffer* CustomIndirectArgsBuffer : CustomIndirectArgsBuffers)
	{
		FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("CustomIndirectArgs.StatsReadbackQuery"));
		GPUBufferReadback->EnqueueCopy(CommandList, CustomIndirectArgsBuffer, 0u);

		FIndirectArgsBufferResult IndirectArgsBufferResult;
		IndirectArgsBufferResult.GPUBufferReadback = GPUBufferReadback;
		FrameData->CustomIndirectArgsBufferResults.Add(CustomIndirectArgsBuffer, IndirectArgsBufferResult);
	}
}

void FMeshDrawCommandStatsManager::Update()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMeshDrawCommandStatsManager::Update);

	++CurrentFrameNumber;

	FScopeLock ScopeLock(&FrameDataCS);

	bool bHasProcessedFrame = false;

	FMeshDrawCommandStatsComponentDataManager const* ComponentDataManager = FMeshDrawCommandStatsComponentDataManager::Get();

	// TODO: might be more than one from a given frame. E.g., if it was using a scene capture, need to filter out those, or perhaps record them as a group actually.
	for (int32 Index = Frames.Num() - 1; Index >= 0; --Index)
	{
		FFrameData* FrameData = Frames[Index];
		if (FrameData->IsCompleted())
		{
			if (!bHasProcessedFrame)
			{
				bHasProcessedFrame = true;	
				
				// TODO: offload processing to async task to offload the rendering thread and time the FrameDataCS lock is taken

				Stats.Reset();

				// Get custom indirect args data
				for (auto Iter = FrameData->CustomIndirectArgsBufferResults.CreateIterator(); Iter; ++Iter)
				{
					FIndirectArgsBufferResult& CustomArgsBufferResult = Iter.Value();
					CustomArgsBufferResult.DrawIndexedIndirectParameters = reinterpret_cast<const FRHIDrawIndexedIndirectParameters*>(CustomArgsBufferResult.GPUBufferReadback->Lock(CustomArgsBufferResult.GPUBufferReadback->GetGPUSizeBytes()));
				}

				TMap<FName, uint64> CategoryStats;
				for (FMeshDrawCommandPassStats* PassStats : FrameData->PassData)
				{
					// make sure the pass was kicked
					if (!PassStats->bBuildRenderingCommandsCalled)
					{
						continue;
					}

					const uint8* InstanceCullingReadBackData = PassStats->InstanceCullingGPUBufferReadback ? reinterpret_cast<const uint8*>(PassStats->InstanceCullingGPUBufferReadback->Lock(PassStats->DrawData.Num())) : nullptr;
					const FRHIDrawIndexedIndirectParameters* IndirectArgsPtr = reinterpret_cast<const FRHIDrawIndexedIndirectParameters*>(InstanceCullingReadBackData);
					
					for (int32 CmdIndex = 0; CmdIndex < PassStats->DrawData.Num(); ++CmdIndex)
					{
						FVisibleMeshDrawCommandStatsData& DrawData = PassStats->DrawData[CmdIndex];
						int32 IndirectCommandIndex = DrawData.IndirectArgsOffset / (FInstanceCullingContext::IndirectArgsNumWords * sizeof(uint32));
						if (DrawData.CustomIndirectArgsBuffer)
						{
							check(DrawData.PrimitiveCount == 0);

							FIndirectArgsBufferResult* IndirectArgsBufferResult = FrameData->CustomIndirectArgsBufferResults.Find(DrawData.CustomIndirectArgsBuffer);
							check(IndirectArgsBufferResult);
							if (IndirectArgsBufferResult)
							{
								const FRHIDrawIndexedIndirectParameters& IndirectArgs = IndirectArgsBufferResult->DrawIndexedIndirectParameters[IndirectCommandIndex];
								DrawData.PrimitiveCount = IndirectArgs.IndexCountPerInstance / 3; //< Assume triangles here for now - primitive count is empty so can't be used
								DrawData.VisibleInstanceCount = IndirectArgs.InstanceCount;
								DrawData.TotalInstanceCount = FMath::Max(DrawData.TotalInstanceCount, DrawData.VisibleInstanceCount);

								Stats.CustomIndirectInstances += DrawData.VisibleInstanceCount;
								Stats.CustomIndirectTriangles += DrawData.VisibleInstanceCount * DrawData.PrimitiveCount;
							}
						}
						else if (DrawData.UseInstantCullingIndirectBuffer > 0 && InstanceCullingReadBackData)
						{
							const FRHIDrawIndexedIndirectParameters& IndirectArgs = IndirectArgsPtr[PassStats->IndirectArgParameterOffset + IndirectCommandIndex];
							check(DrawData.PrimitiveCount == IndirectArgs.IndexCountPerInstance / 3);
							DrawData.VisibleInstanceCount = IndirectArgs.InstanceCount;
							check(DrawData.VisibleInstanceCount <= DrawData.TotalInstanceCount);
							Stats.InstanceCullingIndirectInstances += DrawData.VisibleInstanceCount;
							Stats.InstanceCullingIndirectTriangles += DrawData.VisibleInstanceCount * DrawData.PrimitiveCount;
						}

						Stats.TotalInstances += DrawData.VisibleInstanceCount;
						Stats.TotalTriangles += DrawData.VisibleInstanceCount * DrawData.PrimitiveCount;

						FMeshDrawCommandStatsComponentData ComponentData = ComponentDataManager->GetComponentData(DrawData.StatsData.ComponentDataID);
						static FName NAME_Unknown("Unknown Category");
						FName StatsCategory = ComponentData.StatsCategory.IsNone() ? NAME_Unknown : ComponentData.StatsCategory;
						uint64& TotalCount = CategoryStats.FindOrAdd(StatsCategory);
						TotalCount += DrawData.VisibleInstanceCount * DrawData.PrimitiveCount;
					}

					if (IndirectArgsPtr)
					{
						PassStats->InstanceCullingGPUBufferReadback->Unlock();
					}
				}

				for (auto Iter = FrameData->CustomIndirectArgsBufferResults.CreateIterator(); Iter; ++Iter)
				{
					FIndirectArgsBufferResult& CustomArgsBufferResult = Iter.Value();
					CustomArgsBufferResult.GPUBufferReadback->Unlock();
					CustomArgsBufferResult.DrawIndexedIndirectParameters = nullptr;
				}

				for (auto Iter = CategoryStats.CreateConstIterator(); Iter; ++Iter)
				{
					Stats.CategoryStats.Add(FStats::FCategoryStats(Iter.Key(), Iter.Value()));
				}
				Algo::Sort(Stats.CategoryStats, [this](FStats::FCategoryStats& LHS, FStats::FCategoryStats& RHS) { return LHS.Category.ToString() < RHS.Category.ToString(); });

				// Got new stats, so can dump them if requested
				static bool bDumpStats = false;
				if (bDumpStats || bRequestDumpStats)
				{
					DumpStats(FrameData);
					bDumpStats = false;
					bRequestDumpStats = false;
				}
			}

			// Could pool the frames for allocation effeciency
			delete FrameData;

			// Ok, since we're interating backwards - must not use RemoveAtSwap because we depend on the order being the most recent last.
			// there may be older frames further up that were not completed last frame, but we want to clear them out now.
			Frames.RemoveAt(Index);
		}
	}

	// We keep and set the value from the previous frame in case there are no readback, this avoids alternating values if for example two queries were consumed in one frame
	// might be able to do this better perhaps.
	SET_DWORD_STAT(STAT_Culling_TotalNumTriangles, Stats.TotalTriangles);
	SET_DWORD_STAT(STAT_Culling_TotalNumInstances, Stats.TotalInstances);
	SET_DWORD_STAT(STAT_Culling_InstanceCullingIndirectNumTriangles, Stats.InstanceCullingIndirectTriangles);
	SET_DWORD_STAT(STAT_Culling_InstanceCullingIndirectNumInstances, Stats.InstanceCullingIndirectInstances);
	SET_DWORD_STAT(STAT_Culling_CustomIndirectNumTriangles, Stats.CustomIndirectTriangles);
	SET_DWORD_STAT(STAT_Culling_CustomIndirectNumInstances, Stats.CustomIndirectInstances);

	// Collect stats during the next frame (check if STATGROUP_Culling is also visible somehow)
	const bool bShowStats = CVarShowMeshDrawCommandStats->GetInt() == 0 ? false : true;
	bCollectStats = bShowStats || bRequestDumpStats;
}

#if (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

void FMeshDrawCommandStatsManager::DumpStats(FFrameData* FrameData)
{
	const FString Filename = FString::Printf(TEXT("%sMeshDrawCommandStats-%s.csv"), *FPaths::ProfilingDir(), *FDateTime::Now().ToString());
	FArchive* CSVFile = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead);
	if (CSVFile == nullptr)
	{
		return;
	}

	struct FStatEntry
	{
		FName PassName;
		int32 VisibilePrimitiveCount;
		int32 VisibleInstance;		
		FName Category;
		FName ComponentType;
		FName ResourceName;
		int32 LODIndex;
		int32 SegmentIndex;
		FString MaterialName;
		int32 PrimitiveCount;
		int32 TotalInstanceCount;
		int32 TotalTriangleCount;
	};
	TArray<FStatEntry> StatEntries;

	FMeshDrawCommandStatsComponentDataManager const* ComponentDataManager = FMeshDrawCommandStatsComponentDataManager::Get();
	for (FMeshDrawCommandPassStats* PassStats : FrameData->PassData)
	{
		for (FVisibleMeshDrawCommandStatsData& DrawData : PassStats->DrawData)
		{
			if (DrawData.VisibleInstanceCount > 0)
			{
				FStatEntry& StatEntry = StatEntries.Add_GetRef(FStatEntry());
				StatEntry.PassName = PassStats->PassName;
				StatEntry.VisibilePrimitiveCount = DrawData.VisibleInstanceCount * DrawData.PrimitiveCount;
				StatEntry.VisibleInstance = DrawData.VisibleInstanceCount;
				StatEntry.LODIndex = DrawData.StatsData.LODIndex;
				StatEntry.SegmentIndex = DrawData.StatsData.SegmentIndex;
				StatEntry.PrimitiveCount = DrawData.PrimitiveCount;
				StatEntry.TotalInstanceCount = DrawData.TotalInstanceCount;
				StatEntry.TotalTriangleCount = DrawData.TotalInstanceCount * DrawData.PrimitiveCount;
				StatEntry.ResourceName = DrawData.ResourceName;
				StatEntry.MaterialName = DrawData.MaterialName;

				FMeshDrawCommandStatsComponentData ComponentData = ComponentDataManager->GetComponentData(DrawData.StatsData.ComponentDataID);
				StatEntry.Category = ComponentData.StatsCategory;
				StatEntry.ComponentType = ComponentData.ComponentType;
			}
		}
	}

	Algo::Sort(StatEntries, [this](FStatEntry& LHS, FStatEntry& RHS)
		{
			// first by pass
			if (LHS.PassName != RHS.PassName)
			{
				return LHS.PassName.ToString() < RHS.PassName.ToString();
			}

			// then by visible primitive count
			return LHS.VisibilePrimitiveCount > RHS.VisibilePrimitiveCount;
		});

	const TCHAR* Header = TEXT("Pass,VisiblePrimitiveCount,VisibleInstances,Category,ComponentType,ResourceName,LODIndex,SegmentIndex,MaterialName,PrimitiveCount,TotalInstanceCount,TotalTriangleCount\n");
	CSVFile->Serialize(TCHAR_TO_ANSI(Header), FPlatformString::Strlen(Header));

	TCHAR PassNameBuffer[FName::StringBufferSize];
	TCHAR ResourceNameBuffer[FName::StringBufferSize];
	TCHAR ComponentTypeNameBuffer[FName::StringBufferSize];
	TCHAR CategoryBuffer[FName::StringBufferSize];	

	for (FStatEntry& StatEntry : StatEntries)
	{
		StatEntry.PassName.ToString(PassNameBuffer);
		StatEntry.Category.ToString(CategoryBuffer);
		StatEntry.ComponentType.ToString(ComponentTypeNameBuffer);
		StatEntry.ResourceName.ToString(ResourceNameBuffer);

		FString Row = FString::Printf(TEXT("%s,%d,%d,%s,%s,%s,%d,%d,%s,%d,%d,%d\n"),
			PassNameBuffer,
			StatEntry.VisibilePrimitiveCount,
			StatEntry.VisibleInstance,
			CategoryBuffer,
			ComponentTypeNameBuffer,
			ResourceNameBuffer,
			StatEntry.LODIndex,
			StatEntry.SegmentIndex,
			*StatEntry.MaterialName,
			StatEntry.PrimitiveCount,
			StatEntry.TotalInstanceCount,
			StatEntry.TotalTriangleCount);
		CSVFile->Serialize(TCHAR_TO_ANSI(*Row), Row.Len());
	}

	delete CSVFile;
	CSVFile = nullptr;
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDumpMeshDrawCommandFrameStatsCmd(
	TEXT("DumpMeshDrawCommandFrameStats"),
	TEXT("Dumps the draw stat of the MeshDrawCommand passes of the last rendered frame\n"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic([](const TArray<FString>& Args, UWorld*, FOutputDevice& OutputDevice)
{
	if (FMeshDrawCommandStatsManager* Instance = FMeshDrawCommandStatsManager::Get())
	{
		Instance->RequestDumpStats();
	}
}));

#else

void FMeshDrawCommandStatsManager::DumpStats(FFrameData* FrameData)
{
}

#endif // (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

#endif  // MESH_DRAW_COMMAND_STAT_COLLECTION
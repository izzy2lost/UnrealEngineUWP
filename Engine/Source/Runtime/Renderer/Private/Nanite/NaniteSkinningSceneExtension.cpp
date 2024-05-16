// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteSkinningSceneExtension.h"
#include "ScenePrivate.h"
#include "RenderUtils.h"
#include "SkeletalRenderPublic.h"

static TAutoConsoleVariable<int32> CVarNaniteTransformDataBufferMinSizeBytes(
	TEXT("r.Nanite.SkinningBuffers.TransformDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite bone transform data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNanitePrimitiveSkinningDataBufferMinSizeBytes(
	TEXT("r.Nanite.SkinningBuffers.HeaderDataMinSizeBytes"),
	4 * 1024,
	TEXT("The smallest size (in bytes) of the Nanite per-primitive skinning header data buffer."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteTransformBufferAsyncUpdates(
	TEXT("r.Nanite.SkinningBuffers.AsyncUpdates"),
	true,
	TEXT("When non-zero, Nanite transform data buffer updates are updated asynchronously."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteTransformBufferForceFullUpload = 0;
static FAutoConsoleVariableRef CVarNaniteTransformBufferForceFullUpload(
	TEXT("r.Nanite.SkinningBuffers.ForceFullUpload"),
	GNaniteTransformBufferForceFullUpload,
	TEXT("0: Do not force a full upload.\n")
	TEXT("1: Force one full upload on the next update.\n")
	TEXT("2: Force a full upload every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarNaniteTransformBufferDefrag(
	TEXT("r.Nanite.SkinningBuffers.Defrag"),
	true,
	TEXT("Whether or not to allow defragmentation of the Nanite skinning buffers."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteTransformBufferForceDefrag = 0;
static FAutoConsoleVariableRef CVarNaniteTransformBufferDefragForce(
	TEXT("r.Nanite.SkinningBuffers.Defrag.Force"),
	GNaniteTransformBufferForceDefrag,
	TEXT("0: Do not force a full defrag.\n")
	TEXT("1: Force one full defrag on the next update.\n")
	TEXT("2: Force a full defrag every frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteTransformBufferDefragLowWaterMark(
	TEXT("r.Nanite.SkinningBuffers.Defrag.LowWaterMark"),
	0.375f,
	TEXT("Ratio of used to allocated memory at which to decide to defrag the Nanite skinning buffers."),
	ECVF_RenderThreadSafe
);

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteSkinningParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, SkinningHeaders)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneHierarchy)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneObjectSpace)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, BoneTransforms)
END_SHADER_PARAMETER_STRUCT()

DECLARE_SCENE_UB_STRUCT(FNaniteSkinningParameters, NaniteSkinning, RENDERER_API)

// TODO: Nanite-Skinning [Need to safely populate UpdateList - for now we can defrag and full re-upload to GPU scene every frame]
#define NANITE_SKINNING_WIP 1

namespace Nanite
{

static void GetDefaultSkinningParameters(FNaniteSkinningParameters& OutParameters, FRDGBuilder& GraphBuilder)
{
	auto DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u));
	OutParameters.SkinningHeaders	= DefaultBuffer;
	OutParameters.BoneHierarchy		= DefaultBuffer;
	OutParameters.BoneObjectSpace	= DefaultBuffer;
	OutParameters.BoneTransforms	= DefaultBuffer;
}

IMPLEMENT_SCENE_EXTENSION(FSkinningSceneExtension);

bool FSkinningSceneExtension::ShouldCreateExtension(FScene& InScene)
{
	return NaniteSkinnedMeshesSupported() && DoesRuntimeSupportNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()), true, true);
}

void FSkinningSceneExtension::InitExtension(FScene& InScene)
{
	Scene = &InScene;

	// Determine if we want to be initially enabled or disabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()));
	SetEnabled(bNaniteEnabled);
}

ISceneExtensionUpdater* FSkinningSceneExtension::CreateUpdater()
{
	return new FUpdater(*this);
}

ISceneExtensionRenderer* FSkinningSceneExtension::CreateRenderer()
{
	// We only need to create renderers when we're enabled
	if (!IsEnabled())
	{
		return nullptr;
	}

	return new FRenderer(*this);
}

void FSkinningSceneExtension::SetEnabled(bool bEnabled)
{
	if (bEnabled != IsEnabled())
	{
		if (bEnabled)
		{
			Buffers = MakeUnique<FBuffers>();
		}
		else
		{
			Buffers = nullptr;
			HierarchyAllocator.Reset();
			TransformAllocator.Reset();
			HeaderData.Reset();
		}
	}
}

void FSkinningSceneExtension::FinishSkinningBufferUpload(
	FRDGBuilder& GraphBuilder,
	FNaniteSkinningParameters* OutParams
)
{
	if (!IsEnabled())
	{
		return;
	}

	FRDGBufferRef HeaderBuffer = nullptr;
	FRDGBufferRef BoneHierarchyBuffer = nullptr;
	FRDGBufferRef BoneObjectSpaceBuffer = nullptr;
	FRDGBufferRef TransformBuffer = nullptr;

	const uint32 MinHeaderDataSize = (HeaderData.GetMaxIndex() + 1);
	const uint32 MinTransformDataSize = TransformAllocator.GetMaxSize();
	const uint32 MinHierarchyDataSize = HierarchyAllocator.GetMaxSize();

	if (Uploader.IsValid())
	{
		// Sync on upload tasks
		UE::Tasks::Wait(
			MakeArrayView(
				{
					TaskHandles[UploadHeaderDataTask],
					TaskHandles[UploadHierarchyDataTask],
					TaskHandles[UploadTransformDataTask]
				}
			)
		);

		HeaderBuffer = Uploader->HeaderDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->HeaderDataBuffer,
			MinHeaderDataSize
		);

		BoneHierarchyBuffer = Uploader->BoneHierarchyUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->BoneHierarchyBuffer,
			MinHierarchyDataSize
		);

		BoneObjectSpaceBuffer = Uploader->BoneObjectSpaceUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->BoneObjectSpaceBuffer,
			MinHierarchyDataSize
		);

		TransformBuffer = Uploader->TransformDataUploader.ResizeAndUploadTo(
			GraphBuilder,
			Buffers->TransformDataBuffer,
			MinTransformDataSize
		);

		Uploader = nullptr;
	}
	else
	{
		HeaderBuffer			= Buffers->HeaderDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinHeaderDataSize);
		BoneHierarchyBuffer		= Buffers->BoneHierarchyBuffer.ResizeBufferIfNeeded(GraphBuilder, MinHierarchyDataSize);
		BoneObjectSpaceBuffer	= Buffers->BoneObjectSpaceBuffer.ResizeBufferIfNeeded(GraphBuilder, MinHierarchyDataSize);
		TransformBuffer			= Buffers->TransformDataBuffer.ResizeBufferIfNeeded(GraphBuilder, MinTransformDataSize);
	}

	if (OutParams != nullptr)
	{
		OutParams->SkinningHeaders	= GraphBuilder.CreateSRV(HeaderBuffer);
		OutParams->BoneHierarchy	= GraphBuilder.CreateSRV(BoneHierarchyBuffer);
		OutParams->BoneObjectSpace	= GraphBuilder.CreateSRV(BoneObjectSpaceBuffer);
		OutParams->BoneTransforms	= GraphBuilder.CreateSRV(TransformBuffer);
	}
}

bool FSkinningSceneExtension::ProcessBufferDefragmentation()
{
	// Consolidate spans
	HierarchyAllocator.Consolidate();
	TransformAllocator.Consolidate();

	// Decide to defragment the buffer when the used size dips below a certain multiple of the max used size.
	// Since the buffer allocates in powers of two, we pick the mid point between 1/4 and 1/2 in hopes to prevent
	// thrashing when usage is close to a power of 2.
	//
	// NOTES:
	//	* We only currently use the state of the transform buffer's fragmentation to decide to defrag all buffers
	//	* Rather than trying to minimize number of moves/uploads, we just realloc and re-upload everything. This
	//	  could be implemented in a more efficient manner if the current method proves expensive.

	const bool bAllowDefrag = CVarNaniteTransformBufferDefrag.GetValueOnRenderThread();
	static const int32 MinTransformBufferCount = CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnRenderThread() / sizeof(FMatrix3x4);
	const float LowWaterMarkRatio = CVarNaniteTransformBufferDefragLowWaterMark.GetValueOnRenderThread();
	const int32 EffectiveMaxSize = FMath::RoundUpToPowerOfTwo(TransformAllocator.GetMaxSize());
	const int32 LowWaterMark = uint32(EffectiveMaxSize * LowWaterMarkRatio);
	const int32 UsedSize = TransformAllocator.GetSparselyAllocatedSize();
	
	if (!bAllowDefrag)
	{
		return false;
	}

	// Check to force a defrag
#if NANITE_SKINNING_WIP
	const bool bForceDefrag = true;
#else
	const bool bForceDefrag = GNaniteTransformBufferForceDefrag != 0;
	if (GNaniteTransformBufferForceDefrag == 1)
	{
		GNaniteTransformBufferForceDefrag = 0;
	}
#endif
	
	if (!bForceDefrag && (EffectiveMaxSize <= MinTransformBufferCount || UsedSize > LowWaterMark))
	{
		// No need to defragment
		return false;
	}

	HierarchyAllocator.Reset();
	TransformAllocator.Reset();

	for (auto& Data : HeaderData)
	{
		if (Data.TransformBufferOffset != INDEX_NONE)
		{
			Data.TransformBufferOffset = INDEX_NONE;
			Data.TransformBufferCount = 0;
		}

		if (Data.HierarchyBufferOffset != INDEX_NONE)
		{
			Data.HierarchyBufferOffset = INDEX_NONE;
			Data.HierarchyBufferCount = 0;
		}
	}

	return true;
}

FSkinningSceneExtension::FBuffers::FBuffers()
: HeaderDataBuffer(CVarNanitePrimitiveSkinningDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.SkinningHeaders"))
, BoneHierarchyBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneHierarchy"))
, BoneObjectSpaceBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneObjectSpace"))
, TransformDataBuffer(CVarNaniteTransformDataBufferMinSizeBytes.GetValueOnAnyThread() >> 2u, TEXT("Nanite.BoneTransforms"))
{
}

FSkinningSceneExtension::FUpdater::FUpdater(FSkinningSceneExtension& InSceneData)
: SceneData(&InSceneData)
, bEnableAsync(CVarNaniteTransformBufferAsyncUpdates.GetValueOnRenderThread())
{
}

void FSkinningSceneExtension::FUpdater::End()
{
	// Ensure these tasks finish before we fall out of scope.
	// NOTE: This should be unnecessary if the updater shares the graph builder's lifetime but we don't enforce that
	SceneData->SyncAllTasks();
}

void FSkinningSceneExtension::FUpdater::PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet)
{
	// If there was a pending upload from a prior update (due to the buffer never being used), finish the upload now.
	// This keeps the upload entries from growing unbounded and prevents any undefined behavior caused by any
	// updates that overlap primitives.
	SceneData->FinishSkinningBufferUpload(GraphBuilder);

	// Update whether or not we are enabled based on in Nanite is enabled
	const bool bNaniteEnabled = UseNanite(GetFeatureLevelShaderPlatform(SceneData->Scene->GetFeatureLevel()));
	SceneData->SetEnabled(bNaniteEnabled);

	if (!SceneData->IsEnabled())
	{
		return;
	}

	SceneData->TaskHandles[FreeBufferSpaceTask] = GraphBuilder.AddSetupTask(
		[this, RemovedList = ChangeSet.RemovedPrimitiveIds]
		{
			// Remove and free transform data for removed primitives
			// NOTE: Using the ID list instead of the primitive list since we're in an async task
			for (const auto& PersistentIndex : RemovedList)
			{
				if (SceneData->HeaderData.IsValidIndex(PersistentIndex.Index))
				{
					FSkinningSceneExtension::FHeaderData& Data = SceneData->HeaderData[PersistentIndex.Index];

					if (Data.HierarchyBufferOffset != INDEX_NONE)
					{
						SceneData->HierarchyAllocator.Free(Data.HierarchyBufferOffset, Data.HierarchyBufferCount);
					}

					if (Data.TransformBufferOffset != INDEX_NONE)
					{
						SceneData->TransformAllocator.Free(Data.TransformBufferOffset, Data.TransformBufferCount);
					}
		
					SceneData->HeaderData.RemoveAt(PersistentIndex.Index);
				}
			}

			// Check to force a full upload by CVar
			// NOTE: Doesn't currently discern which scene to affect
		#if NANITE_SKINNING_WIP
			bForceFullUpload = true;
		#else
			bForceFullUpload = GNaniteTransformBufferForceFullUpload != 0;
			if (GNaniteTransformBufferForceFullUpload == 1)
			{
				GNaniteTransformBufferForceFullUpload = 0;
			}
		#endif

			bDefragging = SceneData->ProcessBufferDefragmentation();
			bForceFullUpload |= bDefragging;
		},
		UE::Tasks::ETaskPriority::Normal,
		bEnableAsync
	);
}

void FSkinningSceneExtension::FUpdater::PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
{
	if (!SceneData->IsEnabled())
	{
		return;
	}

	// Cache the updated PrimitiveSceneInfos (this is safe as long as we only access it in updater funcs and RDG setup tasks)
	AddedList = ChangeSet.AddedPrimitiveSceneInfos;

	// Kick off a task to initialize added transform ranges
	if (AddedList.Num() > 0)
	{
		SceneData->TaskHandles[InitHeaderDataTask] = GraphBuilder.AddSetupTask(
			[this]
			{
				// Skip any non-Nanite primitives, or rigid Nanite primitives
				for (auto PrimitiveSceneInfo : AddedList)
				{
					if (!PrimitiveSceneInfo->Proxy->IsNaniteMesh())
					{
						continue;
					}

					auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(PrimitiveSceneInfo->Proxy);
					if (!NaniteProxy->IsSkinnedMesh())
					{
						continue;
					}

					auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(NaniteProxy);
					
					const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
					
					FHeaderData NewHeader;
					NewHeader.PrimitiveSceneInfo = PrimitiveSceneInfo;
					NewHeader.MaxTransformCount = SkinnedProxy->GetMaxBoneTransformCount();
					NewHeader.MaxInfluenceCount = SkinnedProxy->GetMaxBoneInfluenceCount();
					NewHeader.UniqueAnimationCount = SkinnedProxy->GetUniqueAnimationCount();
					NewHeader.bHasScale = SkinnedProxy->HasScale();

					SceneData->HeaderData.EmplaceAt(PersistentIndex, NewHeader);
	
					if (!bForceFullUpload)
					{
						DirtyPrimitiveList.Add(PersistentIndex);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[FreeBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);
	}

	FinalizeSkinningUploads(GraphBuilder);
}

void FSkinningSceneExtension::FUpdater::RequestSkinningUpload(FPrimitiveSceneInfo* Primitive)
{
	check(Primitive->Proxy->IsNaniteMesh());
	check(static_cast<Nanite::FSceneProxyBase*>(Primitive->Proxy)->IsSkinnedMesh());
	UpdateList.Add(Primitive);
}

void FSkinningSceneExtension::FUpdater::FinalizeSkinningUploads(FRDGBuilder& GraphBuilder)
{
	if (SceneData->IsEnabled())
	{
		// TODO: Nanite-Skinning: Not thread safe
		//UpdateList.Reset();

		//for (const FHeaderData& Data : SceneData->HeaderData)
		//{
		//	UpdateList.Add(Data.PrimitiveSceneInfo);
		//}

		// Gets the information needed from the primitive for skinning and allocates the appropriate space in the buffer
		// for the primitive's bone transforms
		auto AllocSpaceForPrimitive = [this](FHeaderData& Data)
		{
			auto* NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Data.PrimitiveSceneInfo->Proxy);
			check(NaniteProxy->IsSkinnedMesh());

			auto* SkinnedProxy = static_cast<Nanite::FSkinnedSceneProxy*>(NaniteProxy);

			Data.MaxTransformCount		= SkinnedProxy->GetMaxBoneTransformCount();
			Data.MaxInfluenceCount		= SkinnedProxy->GetMaxBoneInfluenceCount();
			Data.UniqueAnimationCount	= SkinnedProxy->GetUniqueAnimationCount();

			bool bRequireUpload = false;

			const uint32 HierarchyNeededSize = Data.MaxTransformCount;
			if (HierarchyNeededSize != Data.HierarchyBufferCount)
			{
				if (Data.HierarchyBufferCount > 0)
				{
					SceneData->HierarchyAllocator.Free(Data.HierarchyBufferOffset, Data.HierarchyBufferCount);
				}

				Data.HierarchyBufferOffset = HierarchyNeededSize > 0 ? SceneData->HierarchyAllocator.Allocate(HierarchyNeededSize) : INDEX_NONE;
				Data.HierarchyBufferCount = HierarchyNeededSize;

				if (!bForceFullUpload)
				{
					bRequireUpload = true;
				}
			}

			const uint32 TransformNeededSize = Data.UniqueAnimationCount * Data.MaxTransformCount * 2u; // Current and Previous
			if (bRequireUpload || (TransformNeededSize != Data.TransformBufferCount))
			{
				if (Data.TransformBufferCount > 0)
				{
					SceneData->TransformAllocator.Free(Data.TransformBufferOffset, Data.TransformBufferCount);
				}

				Data.TransformBufferOffset = TransformNeededSize > 0 ? SceneData->TransformAllocator.Allocate(TransformNeededSize) : INDEX_NONE;
				Data.TransformBufferCount = TransformNeededSize;

				if (!bForceFullUpload)
				{
					bRequireUpload = true;
				}
			}

			if (bRequireUpload)
			{
				DirtyPrimitiveList.Add(Data.PrimitiveSceneInfo->GetPersistentIndex().Index);
			}
		};

		// Kick off the allocate task (synced just prior to header uploads)
		SceneData->TaskHandles[AllocBufferSpaceTask] = GraphBuilder.AddSetupTask(
			[this, AllocSpaceForPrimitive]
			{
				if (bDefragging)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						AllocSpaceForPrimitive(Data);
					}
				}
				else
				{
					// Only check to reallocate space for primitives that have requested an update
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 Index = PrimitiveSceneInfo->GetPersistentIndex().Index;
						if (SceneData->HeaderData.IsValidIndex(Index))
						{
							AllocSpaceForPrimitive(SceneData->HeaderData[Index]);
						}
					}
				}

				// Only create a new uploader here if one of the two dependent upload tasks will use it
				if (bForceFullUpload || DirtyPrimitiveList.Num() > 0 || UpdateList.Num() > 0)
				{
					SceneData->Uploader = MakeUnique<FUploader>();
				}
			},
			MakeArrayView(
				{
					SceneData->TaskHandles[FreeBufferSpaceTask],
					SceneData->TaskHandles[InitHeaderDataTask]
				}
			),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		auto UploadHeaderData = [this](const FHeaderData& Data)
		{
			const int32 PersistentIndex = Data.PrimitiveSceneInfo->GetPersistentIndex().Index;

			// Catch when/if no transform buffer data is allocated for a primitive we're tracking.
			// This should be indicative of a bug.
			ensure(Data.HierarchyBufferCount != INDEX_NONE && Data.TransformBufferCount != INDEX_NONE);

			check(SceneData->Uploader.IsValid()); // Sanity check
			SceneData->Uploader->HeaderDataUploader.Add(Data.Pack(), PersistentIndex);
		};

		// Kick off the header data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadHeaderDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadHeaderData]
			{
				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadHeaderData(Data);
					}
				}
				else
				{
					// Sort the array so we can skip duplicate entries
					DirtyPrimitiveList.Sort();
					int32 LastPersistentIndex = INDEX_NONE;
					for (auto PersistentIndex : DirtyPrimitiveList)
					{
						if (PersistentIndex != LastPersistentIndex &&
							SceneData->HeaderData.IsValidIndex(PersistentIndex))
						{
							UploadHeaderData(SceneData->HeaderData[PersistentIndex]);
						}
						LastPersistentIndex = PersistentIndex;
					}
				}
			},
			MakeArrayView(
				{
					SceneData->TaskHandles[AllocBufferSpaceTask]
				}
			),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		auto UploadHierarchyData = [this](const FHeaderData& Data)
		{
			auto SkinnedProxy = static_cast<const Nanite::FSkinnedSceneProxy*>(Data.PrimitiveSceneInfo->Proxy);
			const TArray<uint32>& BoneHierarchy = SkinnedProxy->GetBoneHierarchy();
			const TArray<FMatrix3x4>& BoneObjectSpace = SkinnedProxy->GetBoneObjectSpace();

			check(BoneHierarchy.Num() == Data.MaxTransformCount);
			check(BoneObjectSpace.Num() == Data.MaxTransformCount);
			check(SceneData->Uploader.IsValid());

			// Bone Hierarchy
			{
				auto UploadData = SceneData->Uploader->BoneHierarchyUploader.AddMultiple_GetRef(
					Data.HierarchyBufferOffset,
					Data.HierarchyBufferCount
				);

				uint32* DstBoneHierarchyPtr = UploadData.GetData();
				for (int32 BoneIndex = 0; BoneIndex < Data.MaxTransformCount; ++BoneIndex)
				{
					DstBoneHierarchyPtr[BoneIndex] = BoneHierarchy[BoneIndex];
				}
			}

			// Bone Object Space
			{
				auto UploadData = SceneData->Uploader->BoneObjectSpaceUploader.AddMultiple_GetRef(
					Data.HierarchyBufferOffset,
					Data.HierarchyBufferCount
				);

				FMatrix3x4* DstBoneObjectSpacePtr = UploadData.GetData();
				for (int32 BoneIndex = 0; BoneIndex < Data.MaxTransformCount; ++BoneIndex)
				{
					DstBoneObjectSpacePtr[BoneIndex] = BoneObjectSpace[BoneIndex];
				}
			}
		};

		auto UploadTransformData = [this](const FHeaderData& Data)
		{
			auto SkinnedProxy = static_cast<const Nanite::FSkinnedSceneProxy*>(Data.PrimitiveSceneInfo->Proxy);

			check(SceneData->Uploader.IsValid());
			auto UploadData = SceneData->Uploader->TransformDataUploader.AddMultiple_GetRef(
				Data.TransformBufferOffset,
				Data.TransformBufferCount
			);

			// Fetch bone transforms from Nanite mesh object and upload to GPU (3x4 transposed)
			const TArray<FMatrix3x4>* SrcCurrentBoneTransforms = SkinnedProxy->GetMeshObject()->GetCurrentBoneTransforms();
			check(SrcCurrentBoneTransforms);

			const TArray<FMatrix3x4>* SrcPreviousBoneTransforms = SkinnedProxy->GetMeshObject()->GetPreviousBoneTransforms();
			check(SrcPreviousBoneTransforms);

			check(Data.UniqueAnimationCount * Data.MaxTransformCount * 2u == Data.TransformBufferCount);
			check(uint32(SrcCurrentBoneTransforms->Num() + SrcPreviousBoneTransforms->Num()) <= Data.TransformBufferCount);

			FMatrix3x4*  DstCurrentBoneTransformsPtr = UploadData.GetData();
			FMatrix3x4* DstPreviousBoneTransformsPtr = DstCurrentBoneTransformsPtr + Data.MaxTransformCount;

			const FMatrix3x4*  SrcCurrentBoneTransformsPtr =  SrcCurrentBoneTransforms->GetData();
			const FMatrix3x4* SrcPreviousBoneTransformsPtr = SrcPreviousBoneTransforms->GetData();

			const uint32 StridedPtrStep = Data.MaxTransformCount * 2u;

			for (int32 UniqueAnimation = 0; UniqueAnimation < Data.UniqueAnimationCount; ++UniqueAnimation)
			{
				FMemory::Memcpy( DstCurrentBoneTransformsPtr,  SrcCurrentBoneTransformsPtr, sizeof(FMatrix3x4) * Data.MaxTransformCount);
				FMemory::Memcpy(DstPreviousBoneTransformsPtr, SrcPreviousBoneTransformsPtr, sizeof(FMatrix3x4) * Data.MaxTransformCount);

				 DstCurrentBoneTransformsPtr += StridedPtrStep;
				DstPreviousBoneTransformsPtr += StridedPtrStep;

				 SrcCurrentBoneTransformsPtr += Data.MaxTransformCount;
				SrcPreviousBoneTransformsPtr += Data.MaxTransformCount;
			}
		};

		// Kick off the hierarchy data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadHierarchyDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadHierarchyData]
			{
				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadHierarchyData(Data);
					}
				}
				else
				{
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
						UploadHierarchyData(SceneData->HeaderData[PersistentIndex]);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[AllocBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		// Kick off the transform data upload task (synced when accessing the buffer)
		SceneData->TaskHandles[UploadTransformDataTask] = GraphBuilder.AddSetupTask(
			[this, UploadTransformData]
			{
				if (bForceFullUpload)
				{
					for (auto& Data : SceneData->HeaderData)
					{
						UploadTransformData(Data);
					}
				}
				else
				{
					for (auto PrimitiveSceneInfo : UpdateList)
					{
						const int32 PersistentIndex = PrimitiveSceneInfo->GetPersistentIndex().Index;
						UploadTransformData(SceneData->HeaderData[PersistentIndex]);
					}
				}
			},
			MakeArrayView({ SceneData->TaskHandles[AllocBufferSpaceTask] }),
			UE::Tasks::ETaskPriority::Normal,
			bEnableAsync
		);

		if (!bEnableAsync)
		{
			// If disabling async, just finish the upload immediately
			SceneData->FinishSkinningBufferUpload(GraphBuilder);
		}
	}
}

void FSkinningSceneExtension::FRenderer::UpdateSceneUniformBuffer(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer& SceneUniformBuffer
)
{
	check(SceneData->IsEnabled());

	FNaniteSkinningParameters Parameters;
	SceneData->FinishSkinningBufferUpload(GraphBuilder, &Parameters);
	SceneUniformBuffer.Set(SceneUB::NaniteSkinning, Parameters);
}

void FSkinningSceneExtension::GetSkinnedPrimitives(TArray<FPrimitiveSceneInfo*>& OutPrimitives) const
{
	OutPrimitives.Reset();

	if (!IsEnabled())
	{
		return;
	}

	OutPrimitives.Reserve(HeaderData.Num());

	for (typename TSparseArray<FHeaderData>::TConstIterator It(HeaderData); It; ++It)
	{
		const FHeaderData& Header = *It;
		OutPrimitives.Add(Header.PrimitiveSceneInfo);
	}
}

} // Nanite

IMPLEMENT_SCENE_UB_STRUCT(FNaniteSkinningParameters, NaniteSkinning, Nanite::GetDefaultSkinningParameters);

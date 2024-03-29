// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "Containers/Map.h"
#include "SceneExtensions.h"
#include "NaniteDefinitions.h"
#include "RendererPrivateUtils.h"
#include "Matrix3x4.h"

class FNaniteSkinningParameters;

namespace Nanite
{

class FSkinnedSceneProxy;
class FSkinningSceneExtension : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(FSkinningSceneExtension);

public:
	class FUpdater : public ISceneExtensionUpdater
	{
		DECLARE_SCENE_EXTENSION_UPDATER(FUpdater, FSkinningSceneExtension);

	public:
		FUpdater(FSkinningSceneExtension& InSceneData);

		virtual void End();
		virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet) override;
		virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) override;
		
		void RequestSkinningUpload(FPrimitiveSceneInfo* Primitive);
		void FinalizeSkinningUploads(FRDGBuilder& GraphBuilder);

	private:
		FSkinningSceneExtension* SceneData = nullptr;
		TConstArrayView<FPrimitiveSceneInfo*> AddedList;
		TArray<FPrimitiveSceneInfo*> UpdateList;
		TArray<int32, FSceneRenderingArrayAllocator> DirtyPrimitiveList;
		const bool bEnableAsync = true;
		bool bForceFullUpload = false;
		bool bDefragging = false;
	};

	class FRenderer : public ISceneExtensionRenderer
	{
		DECLARE_SCENE_EXTENSION_RENDERER(FRenderer, FSkinningSceneExtension);
	
	public:
		FRenderer(FSkinningSceneExtension& InSceneData) : SceneData(&InSceneData) {}
		virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& Buffer) override;

	private:
		FSkinningSceneExtension* SceneData = nullptr;
	};

	friend class FUpdater;

	static bool ShouldCreateExtension(FScene& InScene);

	virtual void InitExtension(FScene& InScene) override;

	virtual ISceneExtensionUpdater* CreateUpdater() override;
	virtual ISceneExtensionRenderer* CreateRenderer() override;

private:
	enum ETask : uint32
	{
		FreeBufferSpaceTask,
		InitPrimitiveDataTask,
		AllocTransformBufferTask,
		UploadPrimitiveDataTask,
		UploadTransformDataTask,

		NumTasks
	};

	struct FPackedPrimitiveData
	{
		uint32 TransformBufferOffset;
		uint32 MaxTransformCount : 16;
		uint32 MaxInfluenceCount : 16;
	};

	struct FPrimitiveData
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = nullptr;
		uint32 TransformBufferOffset = INDEX_NONE;
		uint32 TransformBufferCount = 0;
		uint16 MaxTransformCount = 0;
		uint16 MaxInfluenceCount = 0;

		FPackedPrimitiveData Pack() const
		{
			FPackedPrimitiveData Output;
			Output.TransformBufferOffset = TransformBufferOffset;
			Output.MaxTransformCount = MaxTransformCount;
			Output.MaxInfluenceCount = MaxInfluenceCount;
			return Output;
		}
	};

	class FBuffers
	{
	public:
		FBuffers();

		TPersistentByteAddressBuffer<FPackedPrimitiveData> PrimitiveDataBuffer;
		TPersistentByteAddressBuffer<FMatrix3x4> TransformDataBuffer;
	};
	
	class FUploader
	{
	public:
		TByteAddressBufferScatterUploader<FPackedPrimitiveData> PrimitiveDataUploader;
		TByteAddressBufferScatterUploader<FMatrix3x4> TransformDataUploader;
	};
	
	bool IsEnabled() const { return Buffers.IsValid(); }
	void SetEnabled(bool bEnabled);
	void SyncAllTasks() const { UE::Tasks::Wait(TaskHandles); }

	void FinishSkinningBufferUpload(
		FRDGBuilder& GraphBuilder,
		FNaniteSkinningParameters* OutParams = nullptr
	);

	bool ProcessBufferDefragmentation();

	FScene* Scene = nullptr;
	FSpanAllocator TransformAllocator;
	TSparseArray<FPrimitiveData> PrimitiveData;
	TUniquePtr<FBuffers> Buffers;
	TUniquePtr<FUploader> Uploader;
	TStaticArray<UE::Tasks::FTask, NumTasks> TaskHandles;
};

} // namespace Nanite

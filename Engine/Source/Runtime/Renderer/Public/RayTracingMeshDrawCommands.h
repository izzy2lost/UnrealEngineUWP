// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessor.h
=============================================================================*/

#pragma once

#include "MeshPassProcessor.h"

class FRayTracingMeshCommand
{
public:
	FMeshDrawShaderBindings ShaderBindings;
	FRHIRayTracingShader* MaterialShader = nullptr;

	uint32 MaterialShaderIndex = UINT_MAX;
	uint32 GeometrySegmentIndex = UINT_MAX;
	uint8 InstanceMask = 0xFF;

	bool bCastRayTracedShadows = true;
	bool bOpaque = true;
	bool bAlphaMasked = false;
	bool bDecal = false;
	bool bIsSky = false;
	bool bIsTranslucent = false;
	bool bTwoSided = false;
	bool bReverseCulling = false;

	RENDERER_API void SetRayTracingShaderBindingsForHitGroup(
		FRayTracingLocalShaderBindingWriter* BindingWriter,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FRHIUniformBuffer* SceneUniformBuffer,
		FRHIUniformBuffer* NaniteUniformBuffer,
		uint32 RecordIndex,
		const FRHIRayTracingGeometry* RayTracingGeometry,
		uint32 SegmentIndex,
		uint32 HitGroupIndexInPipeline) const;

	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and GlobalSegmentIndex instead of InstanceIndex")
	RENDERER_API void SetRayTracingShaderBindingsForHitGroup(
		FRayTracingLocalShaderBindingWriter* BindingWriter,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FRHIUniformBuffer* SceneUniformBuffer,
		FRHIUniformBuffer* NaniteUniformBuffer,
		uint32 InstanceIndex,
		uint32 SegmentIndex,
		uint32 HitGroupIndexInPipeline,
		uint32 ShaderSlot) const;

	/** Sets ray hit group shaders on the mesh command and allocates room for the shader bindings. */
	RENDERER_API void SetShader(const TShaderRef<FShader>& Shader);

	UE_DEPRECATED(5.4, "Use SetShader")
	RENDERER_API void SetShaders(const FMeshProcessorShaders& Shaders);

	RENDERER_API bool IsUsingNaniteRayTracing() const;
private:
	FShaderUniformBufferParameter ViewUniformBufferParameter;
	FShaderUniformBufferParameter SceneUniformBufferParameter;
	FShaderUniformBufferParameter NaniteUniformBufferParameter;
};

class FVisibleRayTracingMeshCommand
{
public:
	FVisibleRayTracingMeshCommand(const FRayTracingMeshCommand* InRayTracingMeshCommand, const FRHIRayTracingGeometry* InRayTracingGeometry, uint32 InGlobalSegmentIndex, bool bInHidden = false)
		: RayTracingMeshCommand(InRayTracingMeshCommand)
		, RayTracingGeometry(InRayTracingGeometry)
		, GlobalSegmentIndex(InGlobalSegmentIndex)
		, InstanceIndex(INDEX_NONE)
		, bHidden(bInHidden)
	{
		check(RayTracingGeometry != nullptr);
		check(GlobalSegmentIndex != INDEX_NONE);
	}

	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and GlobalSegmentIndex instead of InstanceIndex")
	FVisibleRayTracingMeshCommand(const FRayTracingMeshCommand* InRayTracingMeshCommand, uint32 InInstanceIndex, bool bInHidden = false)
		: RayTracingMeshCommand(InRayTracingMeshCommand)
		, RayTracingGeometry(nullptr)
		, GlobalSegmentIndex(INDEX_NONE)
		, InstanceIndex(InInstanceIndex)
		, bHidden(bInHidden)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		check(InstanceIndex != INDEX_NONE);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	const FRayTracingMeshCommand* RayTracingMeshCommand;
	const FRHIRayTracingGeometry* RayTracingGeometry;
	uint32 GlobalSegmentIndex;
	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and GlobalSegmentIndex instead of InstanceIndex")
	uint32 InstanceIndex;
	bool bHidden;
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FVisibleRayTracingMeshCommand(const FVisibleRayTracingMeshCommand&) = default;
	FVisibleRayTracingMeshCommand& operator=(const FVisibleRayTracingMeshCommand&) = default;
	FVisibleRayTracingMeshCommand(FVisibleRayTracingMeshCommand&&) = default;
	FVisibleRayTracingMeshCommand& operator=(FVisibleRayTracingMeshCommand&&) = default;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

template <>
struct TUseBitwiseSwap<FVisibleRayTracingMeshCommand>
{
	// Prevent Memcpy call overhead during FVisibleRayTracingMeshCommand sorting
	enum { Value = false };
};

typedef TArray<FVisibleRayTracingMeshCommand> FRayTracingMeshCommandOneFrameArray;

class FRayTracingMeshCommandContext
{
public:

	virtual ~FRayTracingMeshCommandContext() {}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) = 0;

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) = 0;
};

using FTempRayTracingMeshCommandStorage = TArray<FRayTracingMeshCommand>;

using FCachedRayTracingMeshCommandStorage = TSparseArray<FRayTracingMeshCommand>;

using FDynamicRayTracingMeshCommandStorage = TChunkedArray<FRayTracingMeshCommand>;

template<class T>
class FCachedRayTracingMeshCommandContext : public FRayTracingMeshCommandContext
{
public:
	FCachedRayTracingMeshCommandContext(T& InDrawListStorage) : DrawListStorage(InDrawListStorage) {}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) override final
	{
		CommandIndex = DrawListStorage.Add(Initializer);
		return DrawListStorage[CommandIndex];
	}

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) override final {}

	int32 CommandIndex = -1;

private:
	T& DrawListStorage;
};

class FDynamicRayTracingMeshCommandContext : public FRayTracingMeshCommandContext
{
public:
	FDynamicRayTracingMeshCommandContext
	(
		FDynamicRayTracingMeshCommandStorage& InDynamicCommandStorage,
		FRayTracingMeshCommandOneFrameArray& InVisibleCommands,
		const FRHIRayTracingGeometry* InRayTracingGeometry,
		uint32 InGeometrySegmentIndex,
		uint32 InGlobalSegmentIndex,
		uint32 InDecalGlobalSegmentIndex = INDEX_NONE
	) :
		DynamicCommandStorage(InDynamicCommandStorage),
		VisibleCommands(InVisibleCommands),
		RayTracingGeometry(InRayTracingGeometry),
		GeometrySegmentIndex(InGeometrySegmentIndex),
		GlobalSegmentIndex(InGlobalSegmentIndex),
		DecalGlobalSegmentIndex(InDecalGlobalSegmentIndex),
		RayTracingInstanceIndex(INDEX_NONE),
		RayTracingDecalInstanceIndex(INDEX_NONE)
	{}

	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and GlobalGeometrySegmentIndex and DecalGlobalGeometrySegmentIndex instead")
	FDynamicRayTracingMeshCommandContext
	(
		FDynamicRayTracingMeshCommandStorage& InDynamicCommandStorage,
		FRayTracingMeshCommandOneFrameArray& InVisibleCommands,
		uint32 InGeometrySegmentIndex,
		uint32 InRayTracingInstanceIndex,
		uint32 InRayTracingDecalInstanceIndex = INDEX_NONE
	) :
		DynamicCommandStorage(InDynamicCommandStorage),
		VisibleCommands(InVisibleCommands),
		RayTracingGeometry(nullptr),
		GeometrySegmentIndex(InGeometrySegmentIndex),
		GlobalSegmentIndex(INDEX_NONE),
		DecalGlobalSegmentIndex(INDEX_NONE),
		RayTracingInstanceIndex(InRayTracingInstanceIndex),
		RayTracingDecalInstanceIndex(InRayTracingDecalInstanceIndex)
	{}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) override final
	{
		const int32 Index = DynamicCommandStorage.AddElement(Initializer);
		FRayTracingMeshCommand& NewCommand = DynamicCommandStorage[Index];
		NewCommand.GeometrySegmentIndex = GeometrySegmentIndex;
		return NewCommand;
	}

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) override final
	{
		if (GlobalSegmentIndex != INDEX_NONE)
		{
			const bool bHidden = RayTracingMeshCommand.bDecal;
			FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&RayTracingMeshCommand, RayTracingGeometry, GlobalSegmentIndex + GeometrySegmentIndex, bHidden);
			VisibleCommands.Add(NewVisibleMeshCommand);
		}

		if (DecalGlobalSegmentIndex != INDEX_NONE)
		{
			const bool bHidden = !RayTracingMeshCommand.bDecal;
			FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&RayTracingMeshCommand, RayTracingGeometry, DecalGlobalSegmentIndex + GeometrySegmentIndex, bHidden);
			VisibleCommands.Add(NewVisibleMeshCommand);
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if(RayTracingInstanceIndex != INDEX_NONE)
		{
			const bool bHidden = RayTracingMeshCommand.bDecal;
			FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&RayTracingMeshCommand, RayTracingInstanceIndex, bHidden);
			VisibleCommands.Add(NewVisibleMeshCommand);
		}

		if (RayTracingDecalInstanceIndex != INDEX_NONE)
		{
			const bool bHidden = !RayTracingMeshCommand.bDecal;
			FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&RayTracingMeshCommand, RayTracingDecalInstanceIndex, bHidden);
			VisibleCommands.Add(NewVisibleMeshCommand);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

private:
	FDynamicRayTracingMeshCommandStorage& DynamicCommandStorage;
	FRayTracingMeshCommandOneFrameArray& VisibleCommands;

	const FRHIRayTracingGeometry* RayTracingGeometry;
	uint32 GeometrySegmentIndex;
	uint32 GlobalSegmentIndex;
	uint32 DecalGlobalSegmentIndex;

	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and GlobalSegmentIndex instead of RayTracingInstanceIndex")
	uint32 RayTracingInstanceIndex;
	UE_DEPRECATED(5.5, "Provide RayTracingGeometry and DecalGlobalGeometrySegmentIndex instead of RayTracingDecalInstanceIndex")
	uint32 RayTracingDecalInstanceIndex;
};

class FRayTracingShaderCommand
{
public:
	FMeshDrawShaderBindings ShaderBindings;
	FRHIRayTracingShader* Shader = nullptr;

	uint32 ShaderIndex = UINT_MAX;
	uint32 SlotInScene = UINT_MAX;

	RENDERER_API void SetRayTracingShaderBindings(
		FRayTracingLocalShaderBindingWriter* BindingWriter,
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
		FRHIUniformBuffer* SceneUniformBuffer,
		FRHIUniformBuffer* NaniteUniformBuffer,
		uint32 ShaderIndexInPipeline,
		uint32 ShaderSlot) const;

	/** Sets ray tracing shader on the command and allocates room for the shader bindings. */
	RENDERER_API void SetShader(const TShaderRef<FShader>& Shader);

private:
	FShaderUniformBufferParameter ViewUniformBufferParameter;
	FShaderUniformBufferParameter SceneUniformBufferParameter;
	FShaderUniformBufferParameter NaniteUniformBufferParameter;
};

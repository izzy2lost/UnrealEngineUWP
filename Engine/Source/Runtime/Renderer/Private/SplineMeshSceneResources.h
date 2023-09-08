// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "SceneUniformBuffer.h"
#include "RHIShaderPlatform.h"
#include "RendererInterface.h"
#include "Containers/Map.h"

class FRDGBuilder;
class FScene;
class FSceneUniformBuffer;
class FScenePreUpdateChangeSet;
class FScenePostUpdateChangeSet;
class FPrimitiveSceneInfo;
struct IPooledRenderTarget;

/**
 *	FSplineMeshSceneResources
 *
 *	This class manages a texture that is used to bake down spline position values at a fixed number of texels
 *	to lower the cost of sampling these splines when deforming spline mesh vertices. Each spline in the scene is
 *	allocated an Nx1 region of the scene-wide 2D texture, and the system performs minimal updates to the texture
 *	on demand as spline mesh scene proxies are created or request updates due to changes to their parameters at run time.
 *
 *	Notes about texture allocation:
 *	- The scene spline mesh texture is allocated in square pow2 tiles of size SPLINE_MESH_TEXEL_WIDTH^2.
 *	- Tiles are encoded in Morton order so that the texture can be resized as the upper bound grows or shrinks
 *	  without changing the assigned texture coordinate of registered spline meshes.
 *	- Defragmentation of the texture will occur if the texture could be 1/4 the size of the current texture
 *	  size when tightly allocated.
 */
class FSplineMeshSceneResources
{
public:
	FSplineMeshSceneResources(FScene& InScene);

	void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet);
	void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet);
	void Update(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms);

	uint32 NumRegisteredPrimitives() const { return RegisteredPrimitives.Num(); }

private:
	struct FPrimitiveSlot
	{
		uint32 FirstSplineIndex = INDEX_NONE;
		uint32 NumSplines = 0;
	};
	using FPrimitiveSlotMap = TMap<const FPrimitiveSceneInfo*, FPrimitiveSlot>;

	void AddUpdatePass(
		FRDGBuilder& GraphBuilder,
		FRDGTextureRef PosTexture,
		FRDGTextureRef RotTexture,
		FSceneUniformBuffer& SceneUniforms,
		FVector2f Extent,
		FVector2f InvExtent,
		bool bFullUpdate,
		bool bForceUpdate);

	void Register(const FPrimitiveSceneInfo& PrimitiveSceneInfo);
	void Unregister(const FPrimitiveSceneInfo& PrimitiveSceneInfo);
	void AllocTextureSpace(const FPrimitiveSceneInfo& PrimitiveSceneInfo, uint32 NumSplines, FPrimitiveSlot& OutSlot);
	static uint32 GetNumSplines(const FPrimitiveSceneInfo& SceneInfo);
	void AssignCoordinates(const FPrimitiveSceneInfo& SceneInfo, const FPrimitiveSlot& Slot);
	template<typename TSplineMeshSceneProxy>
	void AssignCoordinates(TSplineMeshSceneProxy* SceneProxy, const FPrimitiveSlot& Slot);
	void RequestUpdate(const FPrimitiveSlot& Slot);
	void DefragTexture();
	FRDGBufferSRVRef GetInstanceIdLookupSRV(FRDGBuilder& GraphBuilder, bool bForceUpdate);

private:
	FScene& Scene;
	FPrimitiveSlotMap RegisteredPrimitives;
	TArray<uint32> RegisteredInstanceIds;
	TArray<uint32> UpdateRequests;
	FSpanAllocator SlotAllocator;
	TRefCountPtr<IPooledRenderTarget> SavedPosTexture;
	TRefCountPtr<IPooledRenderTarget> SavedRotTexture;
	TRefCountPtr<FRDGPooledBuffer> SavedIdLookup;
	bool bInstanceLookupDirty = true;
	bool bOverflowError = false;
};

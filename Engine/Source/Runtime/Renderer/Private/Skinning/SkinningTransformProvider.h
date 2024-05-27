// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpanAllocator.h"
#include "Containers/Map.h"
#include "SceneExtensions.h"
#include "RendererPrivateUtils.h"
#include "Matrix3x4.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"

class FSkinningTransformProvider : public ISceneExtension
{
	DECLARE_SCENE_EXTENSION(FSkinningTransformProvider);

public:
	typedef FGuid FProviderId;

	struct FProviderContext
	{
		FProviderContext(
			const TConstArrayView<FPrimitiveSceneInfo*> InPrimitives,
			const TConstArrayView<FUintVector2> InPrimitiveIndices,
			FRDGBuilder& InGraphBuilder,
			FRDGBufferRef InTransformBuffer
		)
		: Primitives(InPrimitives)
		, PrimitiveIndices(InPrimitiveIndices)
		, GraphBuilder(InGraphBuilder)
		, TransformBuffer(InTransformBuffer)
		{
		}

		const TConstArrayView<FPrimitiveSceneInfo*> Primitives;
		const TConstArrayView<FUintVector2> PrimitiveIndices;

		FRDGBuilder& GraphBuilder;
		FRDGBufferRef TransformBuffer;
	};

	DECLARE_DELEGATE_OneParam(FOnProvideTransforms, FProviderContext&);

public:
	static bool ShouldCreateExtension(FScene& InScene);

	virtual void InitExtension(FScene& InScene) override;

	RENDERER_API FProviderId RegisterProvider(const FOnProvideTransforms& Delegate);
	RENDERER_API void UnregisterProvider(const FProviderId& Id);

	void Broadcast(FProviderContext& Context);

	inline bool HasProviders() const
	{
		return !Providers.IsEmpty();
	}

private:
	struct FTransformProvider
	{
		FProviderId Id;
		FOnProvideTransforms Delegate;
	};

	TArray<FTransformProvider> Providers;

	const FScene* Scene = nullptr;
};

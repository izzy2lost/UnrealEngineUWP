// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkinningTransformProvider.h"
#include "ScenePrivate.h"
#include "RenderUtils.h"
#include "SkeletalRenderPublic.h"

IMPLEMENT_SCENE_EXTENSION(FSkinningTransformProvider);

bool FSkinningTransformProvider::ShouldCreateExtension(FScene& InScene)
{
	return NaniteSkinnedMeshesSupported() && DoesRuntimeSupportNanite(GetFeatureLevelShaderPlatform(InScene.GetFeatureLevel()), true, true);
}

void FSkinningTransformProvider::InitExtension(FScene& InScene)
{
	Scene = &InScene;
}

FSkinningTransformProvider::FProviderId FSkinningTransformProvider::RegisterProvider(const FOnProvideTransforms& Delegate)
{
	FTransformProvider& Provider = Providers.Emplace_GetRef();
	Provider.Id = FGuid::NewGuid();
	Provider.Delegate = Delegate;
	return Provider.Id;
}

void FSkinningTransformProvider::UnregisterProvider(const FSkinningTransformProvider::FProviderId& Id)
{
	for (int32 ProviderIndex = 0; ProviderIndex < Providers.Num(); ++ProviderIndex)
	{
		const FTransformProvider& Provider = Providers[ProviderIndex];
		if (Provider.Id == Id)
		{
			Providers.RemoveAtSwap(ProviderIndex);
			return;
		}
	}

	checkNoEntry(); // No provider found with this id - error!
}

void FSkinningTransformProvider::Broadcast(FProviderContext& Context)
{
	for (const FTransformProvider& Provider : Providers)
	{
		if (Context.PrimitiveIndices.Num() > 0)
		{
			Provider.Delegate.ExecuteIfBound(Context);
		}
	}
}

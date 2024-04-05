// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIResources.h"
#include "RHITextureReference.h"

struct FRHIResourceCollectionMember
{
	enum class EType : uint8
	{
		Texture,
		TextureReference,
		ShaderResourceView,
		UnorderedAccessView,
		Sampler,
	};

	FRHIResourceCollectionMember() = default;
	FRHIResourceCollectionMember(EType InType, FRHIResource* InResource)
		: Resource(InResource)
		, Type(InType)
	{
	}
	FRHIResourceCollectionMember(FRHITexture* InTexture)
		: FRHIResourceCollectionMember(FRHIResourceCollectionMember::EType::Texture, InTexture)
	{
	}
	FRHIResourceCollectionMember(FRHITextureReference* InTextureReference)
		: FRHIResourceCollectionMember(FRHIResourceCollectionMember::EType::TextureReference, InTextureReference)
	{
	}
	FRHIResourceCollectionMember(FRHIShaderResourceView* InView)
		: FRHIResourceCollectionMember(FRHIResourceCollectionMember::EType::ShaderResourceView, InView)
	{
	}
	FRHIResourceCollectionMember(FRHIUnorderedAccessView* InUAV)
		: FRHIResourceCollectionMember(FRHIResourceCollectionMember::EType::UnorderedAccessView, InUAV)
	{
	}
	FRHIResourceCollectionMember(FRHISamplerState* InSamplerState)
		: FRHIResourceCollectionMember(FRHIResourceCollectionMember::EType::Sampler, InSamplerState)
	{
	}

	FRHIResource* Resource = nullptr;
	EType         Type = EType::Texture;
};

class FRHIResourceCollection : public FRHIResource
{
public:
	FRHIResourceCollection(TConstArrayView<FRHIResourceCollectionMember> InMembers)
		: FRHIResource(RRT_ResourceCollection)
	{
		Resources.Reserve(InMembers.Num());
		for (const FRHIResourceCollectionMember& Member : InMembers)
		{
			Resources.Emplace(Member.Resource);
		}
	}

	virtual FRHIDescriptorHandle GetBindlessHandle() const
	{
		return FRHIDescriptorHandle();
	}

	TArray<TRefCountPtr<FRHIResource>> Resources;
};

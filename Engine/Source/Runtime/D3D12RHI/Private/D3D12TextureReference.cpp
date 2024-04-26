// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12TextureReference.h"

FD3D12RHITextureReference::FD3D12RHITextureReference(FD3D12Device* InDevice, FD3D12Texture* InReferencedTexture)
	: FD3D12DeviceChild(InDevice)
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	, FRHITextureReference(InReferencedTexture, InDevice->GetBindlessDescriptorManager().AllocateResourceHandle())
#else
	, FRHITextureReference(InReferencedTexture)
#endif
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		InReferencedTexture->AddRenameListener(this);

		InDevice->GetBindlessDescriptorManager().InitializeDescriptor(BindlessHandle, InReferencedTexture->GetShaderResourceView());
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
}

FD3D12RHITextureReference::~FD3D12RHITextureReference()
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		FD3D12DynamicRHI::ResourceCast(GetReferencedTexture())->RemoveRenameListener(this);

		GetParentDevice()->GetBindlessDescriptorManager().DeferredFreeFromDestructor(BindlessHandle);
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
}

void FD3D12RHITextureReference::SwitchToNewTexture(FD3D12ContextArray const& Contexts, FD3D12Texture* InNewTexture)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		FD3D12Texture* NewTexture = InNewTexture ? InNewTexture : FD3D12DynamicRHI::ResourceCast(FRHITextureReference::GetDefaultTexture());

		FD3D12Texture* CurrentTexture = FD3D12DynamicRHI::ResourceCast(GetReferencedTexture());

		if (CurrentTexture != NewTexture)
		{
			if (CurrentTexture)
			{
				CurrentTexture->RemoveRenameListener(this);
			}

			NewTexture->AddRenameListener(this);

			GetParentDevice()->GetBindlessDescriptorManager().UpdateDescriptor(Contexts, BindlessHandle, NewTexture->GetShaderResourceView());
		}
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

	SetReferencedTexture(InNewTexture);
}

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
void FD3D12RHITextureReference::ResourceRenamed(FD3D12ContextArray const& Contexts, FD3D12BaseShaderResource* InRenamedResource, FD3D12ResourceLocation* InNewResourceLocation)
{
	if (ensure(BindlessHandle.IsValid()))
	{
		FD3D12Texture* RenamedTexture = static_cast<FD3D12Texture*>(InRenamedResource);
		checkSlow(RenamedTexture == ReferencedTexture);

		GetParentDevice()->GetBindlessDescriptorManager().UpdateDescriptor(Contexts, BindlessHandle, RenamedTexture->GetShaderResourceView());
	}
}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

FTextureReferenceRHIRef FD3D12DynamicRHI::RHICreateTextureReference(FRHICommandListBase& RHICmdList, FRHITexture* InReferencedTexture)
{
	FRHITexture* ReferencedTexture = InReferencedTexture ? InReferencedTexture : FRHITextureReference::GetDefaultTexture();

	FD3D12Adapter* Adapter = &GetAdapter();
	return Adapter->CreateLinkedObject<FD3D12RHITextureReference>(FRHIGPUMask::All(), [ReferencedTexture](FD3D12Device* Device)
	{
		return new FD3D12RHITextureReference(Device, ResourceCast(ReferencedTexture, Device->GetGPUIndex()));
	});
}

void FD3D12DynamicRHI::RHIUpdateTextureReference(FRHICommandListBase& RHICmdList, FRHITextureReference* TextureRef, FRHITexture* InNewTexture)
{
	// Workaround for a crash bug where FRHITextureReferences are deleted before this command is executed on the RHI thread.
	// Take a reference on the FRHITextureReference object to keep it alive.
	// @todo dev-pr - This should be refactored out when we eventually remove FRHITextureReference.
	TRefCountPtr<FRHITextureReference> Ref = TextureRef;

	RHICmdList.EnqueueLambdaMultiPipe(GetEnabledRHIPipelines(), TEXT("FD3D12DynamicRHI::RHIUpdateTextureReference"),
	[
		TextureRef = MoveTemp(Ref),
		NewTexture = InNewTexture ? InNewTexture : FRHITextureReference::GetDefaultTexture()
	](FD3D12ContextArray const& Contexts)
	{
		for (TD3D12DualLinkedObjectIterator<FD3D12RHITextureReference, FD3D12Texture> It(ResourceCast(TextureRef.GetReference()), ResourceCast(NewTexture)); It; ++It)
		{
			It.GetFirst()->SwitchToNewTexture(Contexts, It.GetSecond());
		}
	});

	RHICmdList.RHIThreadFence(true);
}

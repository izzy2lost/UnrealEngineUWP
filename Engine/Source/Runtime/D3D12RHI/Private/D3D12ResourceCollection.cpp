// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12ResourceCollection.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

#include "D3D12RHIPrivate.h"
#include "D3D12CommandContext.h"
#include "D3D12TextureReference.h"

FD3D12ResourceCollection::FD3D12ResourceCollection(FD3D12Device* InParent, FRHICommandListBase& RHICmdList, FD3D12Buffer* InBuffer, TConstArrayView<FRHIResourceCollectionMember> InMembers)
	: FRHIResourceCollection(InMembers)
	, FD3D12DeviceChild(InParent)
	, Buffer(InBuffer->GetLinkedObject(InParent->GetGPUIndex()))
{
	const uint32 GpuIndex = InParent->GetGPUIndex();

	TArray<FRHIDescriptorHandle> Handles;
	Handles.Reserve(InMembers.Num());

	for (const FRHIResourceCollectionMember& Member : InMembers)
	{
		switch (Member.Type)
		{
		case FRHIResourceCollectionMember::EType::Texture:
		{
			if (FRHITextureReference* TextureReferenceRHI = static_cast<FRHITexture*>(Member.Resource)->GetTextureReference())
			{
				FD3D12RHITextureReference* TextureReference = FD3D12CommandContext::RetrieveObject<FD3D12RHITextureReference>(TextureReferenceRHI, GpuIndex);
				Handles.Emplace(TextureReference->GetDefaultBindlessHandle());
				AllTextureReferences.Emplace(TextureReference);
			}
			else
			{
				FD3D12Texture* Texture = FD3D12CommandContext::RetrieveTexture(static_cast<FRHITexture*>(Member.Resource), GpuIndex);
				Handles.Emplace(Texture->GetDefaultBindlessHandle());
				AllSrvs.Emplace(Texture->GetShaderResourceView());
			}
		}
		break;
		case FRHIResourceCollectionMember::EType::TextureReference:
		{
			FD3D12RHITextureReference* TextureReference = FD3D12CommandContext::RetrieveObject<FD3D12RHITextureReference>(Member.Resource, GpuIndex);
			Handles.Emplace(TextureReference->GetDefaultBindlessHandle());
			AllTextureReferences.Emplace(TextureReference);
		}
		break;
		case FRHIResourceCollectionMember::EType::ShaderResourceView:
		{
			FD3D12ShaderResourceView_RHI* ShaderResourceView = FD3D12CommandContext::RetrieveObject<FD3D12ShaderResourceView_RHI>(Member.Resource, GpuIndex);
			Handles.Emplace(ShaderResourceView->GetBindlessHandle());
			AllSrvs.Emplace(ShaderResourceView);
		}
		break;
		}
	}

	constexpr D3D12_RESOURCE_STATES States = D3D12_RESOURCE_STATE_GENERIC_READ;
	const TArray<uint32> CollectionMemory = UE::RHICore::CreateResourceCollectionArray<FRHIDescriptorHandle>(Handles);
	InBuffer->UploadResourceData(RHICmdList, FRHIGPUMask::FromIndex(GpuIndex), States, CollectionMemory.GetData(), CollectionMemory.GetTypeSize() * CollectionMemory.Num());

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.FirstElement = InBuffer->ResourceLocation.GetOffsetFromBaseOfResource() / 4;
	SRVDesc.Buffer.NumElements = UE::RHICore::CalculateResourceCollectionMemorySize(InMembers) / 4;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

	BufferSRV = MakeShared<FD3D12ShaderResourceView>(InParent);
	BufferSRV->CreateView(InBuffer, SRVDesc, FD3D12ShaderResourceView::EFlags::None);
}

FD3D12ResourceCollection::~FD3D12ResourceCollection() = default;

FRHIDescriptorHandle FD3D12ResourceCollection::GetBindlessHandle() const
{
	return BufferSRV->GetBindlessHandle();
}

static FD3D12Buffer* CreateCollectionBuffer(FD3D12DynamicRHI& RHI, FRHICommandListBase& RHICmdList, TConstArrayView<FRHIResourceCollectionMember> InMembers)
{
	const size_t BufferSize = UE::RHICore::CalculateResourceCollectionMemorySize(InMembers);
	FRHIBufferDesc BufferDesc(BufferSize, 4, EBufferUsageFlags::Static | EBufferUsageFlags::ByteAddressBuffer);

	FRHIResourceCreateInfo CreateInfo(TEXT("ResourceCollection"));
	return RHI.CreateD3D12Buffer(&RHICmdList, BufferDesc, ERHIAccess::SRVMask, CreateInfo, nullptr, true);
}

FRHIResourceCollectionRef FD3D12DynamicRHI::RHICreateResourceCollection(FRHICommandListBase& RHICmdList, TConstArrayView<FRHIResourceCollectionMember> InMembers)
{
	FD3D12Buffer* Buffer = CreateCollectionBuffer(*this, RHICmdList, InMembers);

	FRHIViewDesc::FBufferSRV::FInitializer ViewDesc = FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw);
	FShaderResourceViewRHIRef ShaderResourceView = RHICmdList.CreateShaderResourceView(Buffer, ViewDesc);

	return GetAdapter().CreateLinkedObject<FD3D12ResourceCollection>(FRHIGPUMask::All(), [&RHICmdList, Buffer, InMembers](FD3D12Device* Device)
	{
		return new FD3D12ResourceCollection(Device, RHICmdList, Buffer, InMembers);
	});
}

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

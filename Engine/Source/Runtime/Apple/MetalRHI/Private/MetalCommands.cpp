// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommands.cpp: Metal RHI commands implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "MetalRHIContext.h"
#include "GlobalShader.h"
#include "OneColorShader.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "SceneUtils.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "StaticBoundShaderState.h"
#include "EngineGlobals.h"
#include "PipelineStateCache.h"
#include "RHICoreShader.h"
#include "RHIShaderParametersShared.h"
#include "RHIUtilities.h"
#include "MetalResourceCollection.h"

static const bool GUsesInvertedZ = true;

/** Vertex declaration for just one FVector4 position. */
class FVector4VertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, sizeof(FVector4f)));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
static TGlobalResource<FVector4VertexDeclaration> FVector4VertexDeclaration;

MTL::PrimitiveType TranslatePrimitiveType(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:	return MTL::PrimitiveTypeTriangle;
		case PT_TriangleStrip:	return MTL::PrimitiveTypeTriangleStrip;
		case PT_LineList:		return MTL::PrimitiveTypeLine;
		case PT_PointList:		return MTL::PrimitiveTypePoint;
		default:
			METAL_FATAL_ERROR(TEXT("Unsupported primitive type %d"), (int32)PrimitiveType);
			return MTL::PrimitiveTypeTriangle;
	}
}

static FORCEINLINE EMetalShaderStages GetShaderStage(FRHIGraphicsShader* ShaderRHI)
{
	EMetalShaderStages Stage = EMetalShaderStages::Num;
	switch (ShaderRHI->GetFrequency())
	{
	case SF_Vertex:		Stage = EMetalShaderStages::Vertex; break;
	case SF_Pixel:		Stage = EMetalShaderStages::Pixel; break;
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    case SF_Geometry:   Stage = EMetalShaderStages::Geometry; break;
#endif
#if PLATFORM_SUPPORTS_MESH_SHADERS
    case SF_Mesh:               Stage = EMetalShaderStages::Mesh; break;
    case SF_Amplification:      Stage = EMetalShaderStages::Amplification; break;
#endif
	default:
		checkf(0, TEXT("FRHIShader Type %d is invalid or unsupported!"), (int32)ShaderRHI->GetFrequency());
		NOT_SUPPORTED("RHIShaderStage");
		break;
	}

	return Stage;
}

void FMetalRHICommandContext::RHISetStreamSource(uint32 StreamIndex, FRHIBuffer* VertexBufferRHI,uint32 Offset)
{
    MTL_SCOPED_AUTORELEASE_POOL;;
    
    FMetalRHIBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
    
    FMetalBufferPtr TheBuffer = nullptr;
    if(VertexBuffer && !VertexBuffer->Data)
    {
        TheBuffer = VertexBuffer->GetCurrentBuffer();
    }
    
    Context->GetCurrentState().SetVertexStream(StreamIndex, VertexBuffer ? TheBuffer : nullptr, VertexBuffer ? VertexBuffer->Data : nullptr, Offset, VertexBuffer ? VertexBuffer->GetSize() : 0);
}

static void SetUniformBufferInternal(FMetalStateCache& StateCache, FMetalShaderData* ShaderData, EMetalShaderStages Stage, uint32 BufferIndex, FRHIUniformBuffer* UBRHI)
{
    StateCache.BindUniformBuffer(Stage, BufferIndex, UBRHI);
        
    FMetalShaderBindings& Bindings = ShaderData->Bindings;
    if ((Bindings.ConstantBuffers) & (1 << BufferIndex))
    {
        FMetalUniformBuffer* UB = ResourceCast(UBRHI);
        UB->PrepareToBind();
#if METAL_USE_METAL_SHADER_CONVERTER
        if (IsMetalBindlessEnabled())
        {
            StateCache.IRBindUniformBuffer(Stage, BufferIndex, UB);
        }
        else
#endif
        {
            FMetalBufferPtr Buf = FMetalBufferPtr(new FMetalBuffer(UB->Backing));
            StateCache.SetShaderBuffer(Stage, Buf, nullptr, UB->Offset, UB->GetSize(), BufferIndex, MTL::ResourceUsageRead);
        }
    }
}

inline FMetalShaderData* GetShaderData(FRHIShader* InShaderRHI, EMetalShaderStages Stage)
{
    switch (Stage)
    {
    case EMetalShaderStages::Vertex:        return ResourceCast(static_cast<FRHIVertexShader*>(InShaderRHI));
#if PLATFORM_SUPPORTS_MESH_SHADERS
    case EMetalShaderStages::Mesh:          return ResourceCast(static_cast<FRHIMeshShader*>(InShaderRHI));
    case EMetalShaderStages::Amplification: return ResourceCast(static_cast<FRHIAmplificationShader*>(InShaderRHI));
#endif
    case EMetalShaderStages::Pixel:         return ResourceCast(static_cast<FRHIPixelShader*>(InShaderRHI));
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
    case EMetalShaderStages::Geometry:      return ResourceCast(static_cast<FRHIGeometryShader*>(InShaderRHI));
#endif
    case EMetalShaderStages::Compute:       return ResourceCast(static_cast<FRHIComputeShader*>(InShaderRHI));
            
    default:
        checkf(0, TEXT("FRHIShader Type %d is invalid or unsupported!"), (int32)InShaderRHI->GetFrequency());
        NOT_SUPPORTED("RHIShaderStage");
    }
    return nullptr;
}

static void BindUniformBuffer(FMetalStateCache& StateCache, FRHIShader* Shader, EMetalShaderStages Stage, uint32 BufferIndex, FRHIUniformBuffer* InBuffer)
{
    if (FMetalShaderData* ShaderData = GetShaderData(Shader, Stage))
    {
        SetUniformBufferInternal(StateCache, ShaderData, Stage, BufferIndex, InBuffer);
    }
}

static void ApplyStaticUniformBuffersOnContext(FMetalRHICommandContext& Context, FRHIShader* Shader, FMetalShaderData* ShaderData)
{
    if (Shader)
    {
        MTL_SCOPED_AUTORELEASE_POOL;

        FMetalStateCache& StateCache = Context.GetInternalContext().GetCurrentState();
        const EMetalShaderStages Stage = GetMetalShaderFrequency(Shader->GetFrequency());

        UE::RHICore::ApplyStaticUniformBuffers(
            Shader,
            Context.GetStaticUniformBuffers(),
            [&StateCache, ShaderData, Stage](int32 BufferIndex, FRHIUniformBuffer* Buffer)
            {
                SetUniformBufferInternal(StateCache, ShaderData, Stage, BufferIndex, ResourceCast(Buffer));
            }
        );
    }
}

template <typename TRHIShader>
static void ApplyStaticUniformBuffersOnContext(FMetalRHICommandContext& Context, TRefCountPtr<TRHIShader>& Shader)
{
    if (IsValidRef(Shader))
    {
        ApplyStaticUniformBuffersOnContext(Context, Shader, static_cast<FMetalShaderData*>(Shader.GetReference()));
    }
}

void FMetalRHICommandContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
    MTL_SCOPED_AUTORELEASE_POOL;
	
	FMetalComputeShader* ComputeShader = ResourceCast(ComputePipelineState->GetComputeShader());
	
	// cache this for Dispatch
	// sets this compute shader pipeline as the current (this resets all state, so we need to set all resources after calling this)
	Context->GetCurrentState().SetComputeShader(ComputeShader);

    ApplyStaticUniformBuffersOnContext(*this, ComputeShader, static_cast<FMetalShaderData*>(ComputeShader));
}

void FMetalRHICommandContext::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	ThreadGroupCountX = FMath::Max(ThreadGroupCountX, 1u);
	ThreadGroupCountY = FMath::Max(ThreadGroupCountY, 1u);
	ThreadGroupCountZ = FMath::Max(ThreadGroupCountZ, 1u);
	
	Context->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FMetalRHICommandContext::RHIDispatchIndirectComputeShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		FMetalRHIBuffer* VertexBuffer = ResourceCast(ArgumentBufferRHI);
		
		Context->DispatchIndirect(VertexBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDispatchIndirectComputeShader");
	}
}

void FMetalRHICommandContext::RHISetViewport(float MinX, float MinY,float MinZ, float MaxX, float MaxY,float MaxZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;

	MTL::Viewport Viewport;
	Viewport.originX = MinX;
	Viewport.originY = MinY;
	Viewport.width = MaxX - MinX;
	Viewport.height = MaxY - MinY;
	Viewport.znear = MinZ;
	Viewport.zfar = MaxZ;
	
	Context->GetCurrentState().SetViewport(Viewport);
}

void FMetalRHICommandContext::RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports))
	{
        MTL_SCOPED_AUTORELEASE_POOL;
        
		MTL::Viewport Viewport[2];
		
		Viewport[0].originX = LeftMinX;
		Viewport[0].originY = LeftMinY;
		Viewport[0].width = LeftMaxX - LeftMinX;
		Viewport[0].height = LeftMaxY - LeftMinY;
		Viewport[0].znear = MinZ;
		Viewport[0].zfar = MaxZ;
		
		Viewport[1].originX = RightMinX;
		Viewport[1].originY = RightMinY;
		Viewport[1].width = RightMaxX - RightMinX;
		Viewport[1].height = RightMaxY - RightMinY;
		Viewport[1].znear = MinZ;
		Viewport[1].zfar = MaxZ;
		
		Context->GetCurrentState().SetViewports(Viewport, 2);
	}
	else
	{
		NOT_SUPPORTED("RHISetStereoViewport");
	}
}

void FMetalRHICommandContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{ 
	NOT_SUPPORTED("RHISetMultipleViewports");
}

void FMetalRHICommandContext::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	MTL::ScissorRect Scissor;
	Scissor.x = MinX;
	Scissor.y = MinY;
	Scissor.width = MaxX - MinX;
	Scissor.height = MaxY - MinY;

	// metal doesn't support 0 sized scissor rect
	if (bEnable == false || Scissor.width == 0 || Scissor.height == 0)
	{
		MTL::Viewport const& Viewport = Context->GetCurrentState().GetViewport(0);
		CGSize FBSize = Context->GetCurrentState().GetFrameBufferSize();
		
		Scissor.x = Viewport.originX;
		Scissor.y = Viewport.originY;
		Scissor.width = (Viewport.originX + Viewport.width <= FBSize.width) ? Viewport.width : FBSize.width - Viewport.originX;
		Scissor.height = (Viewport.originY + Viewport.height <= FBSize.height) ? Viewport.height : FBSize.height - Viewport.originY;
	}
	Context->GetCurrentState().SetScissorRect(bEnable, Scissor);
}

void FMetalRHICommandContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    FMetalGraphicsPipelineState* PipelineState = ResourceCast(GraphicsState);
    if (SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelResetOnBind && Context->GetCurrentState().GetGraphicsPSO() != PipelineState)
    {
        Context->GetCurrentRenderPass().GetCurrentCommandEncoder().ResetLive();
    }
    Context->GetCurrentState().SetGraphicsPipelineState(PipelineState);

    RHISetStencilRef(StencilRef);
    RHISetBlendFactor(FLinearColor(1.0f, 1.0f, 1.0f));

    if (bApplyAdditionalState)
    {
#if PLATFORM_SUPPORTS_MESH_SHADERS
        ApplyStaticUniformBuffersOnContext(*this, PipelineState->MeshShader);
        ApplyStaticUniformBuffersOnContext(*this, PipelineState->AmplificationShader);
#endif
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
        ApplyStaticUniformBuffersOnContext(*this, PipelineState->GeometryShader);
#endif
        ApplyStaticUniformBuffersOnContext(*this, PipelineState->VertexShader);
        ApplyStaticUniformBuffersOnContext(*this, PipelineState->PixelShader);
    }
}

void FMetalRHICommandContext::RHISetStaticUniformBuffers(const FUniformBufferStaticBindings& InUniformBuffers)
{
	FMemory::Memzero(GlobalUniformBuffers.GetData(), GlobalUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));

	for (int32 Index = 0; Index < InUniformBuffers.GetUniformBufferCount(); ++Index)
	{
		GlobalUniformBuffers[InUniformBuffers.GetSlot(Index)] = InUniformBuffers.GetUniformBuffer(Index);
	}
}

struct FMetalShaderBinder
{
    FMetalDeviceContext& Context;
    FMetalStateCache& StateCache;
    const EMetalShaderStages Stage;
    FMetalShaderParameterCache& ShaderParameters;
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    const bool bBindlessResources;
    const bool bBindlessSamplers;
#endif
    
    FMetalShaderBinder(FMetalDeviceContext& InContext, EShaderFrequency ShaderFrequency)
    : Context(InContext)
    , StateCache(InContext.GetCurrentState())
    , Stage(GetMetalShaderFrequency(ShaderFrequency))
    , ShaderParameters(StateCache.GetShaderParameters(Stage))
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    , bBindlessResources(IsMetalBindlessEnabled())
    , bBindlessSamplers(IsMetalBindlessEnabled())
#endif
    {
    }
    
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    void SetBindlessHandle(const FRHIDescriptorHandle& Handle, uint32 Offset)
    {
        if (Handle.IsValid())
        {
            const uint32 BindlessIndex = Handle.GetIndex();
            Context.GetCurrentState().GetShaderParameters(Stage).Set(0, Offset, 4, &BindlessIndex);
        }
    }
#endif

    void SetUAV(FRHIUnorderedAccessView* InUnorderedAccessView, uint32 Index, bool bClearResources = false)
    {
        FMetalUnorderedAccessView* UAV = ResourceCast(InUnorderedAccessView);
        StateCache.SetShaderUnorderedAccessView(Stage, Index, UAV);
    }

    void SetSRV(FRHIShaderResourceView* InShaderResourceView, uint32 Index)
    {
        FMetalShaderResourceView* SRV = ResourceCast(InShaderResourceView);
        StateCache.SetShaderResourceView(Stage, Index, SRV);
    }

    void SetTexture(FRHITexture* InTexture, uint32 Index)
    {
        if (FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(InTexture))
        {
            if (Surface->Texture || !EnumHasAnyFlags(Surface->GetDesc().Flags, ETextureCreateFlags::Presentable))
            {
                StateCache.SetShaderTexture(Stage, Surface->Texture.get(), Index, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
            }
            else
            {
                MTLTexturePtr Tex = Surface->GetCurrentTexture();
                StateCache.SetShaderTexture(Stage, Tex.get(), Index, (MTL::ResourceUsage)(MTL::ResourceUsageRead|MTL::ResourceUsageSample));
            }
        }
        else
        {
            StateCache.SetShaderTexture(Stage, nullptr, Index, MTL::ResourceUsage(0));
        }
    }

    void SetSampler(FRHISamplerState* InSampler, uint32 Index)
    {
        FMetalSamplerState* Sampler = ResourceCast(InSampler);
        StateCache.SetShaderSamplerState(Stage, Sampler, Index);
    }

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	void SetResourceCollection(FRHIResourceCollection* ResourceCollection, uint32 Index)
	{
		FMetalResourceCollection* MetalResourceCollection = ResourceCast(ResourceCollection);
		SetSRV(MetalResourceCollection->GetShaderResourceView(), Index);
	}
#endif
};

static void SetShaderParametersOnContext(
      FMetalDeviceContext& Context
    , FRHIShader* Shader
    , EShaderFrequency ShaderFrequency
    , TArrayView<const uint8> InParametersData
    , TArrayView<const FRHIShaderParameter> InParameters
    , TArrayView<const FRHIShaderParameterResource> InResourceParameters
    , TArrayView<const FRHIShaderParameterResource> InBindlessParameters)
{
    MTL_SCOPED_AUTORELEASE_POOL;

    FMetalShaderBinder Binder(Context, ShaderFrequency);
    
    for (const FRHIShaderParameter& Parameter : InParameters)
    {
        Binder.ShaderParameters.Set(Parameter.BufferIndex, Parameter.BaseIndex, Parameter.ByteSize, &InParametersData[Parameter.ByteOffset]);
    }

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    for (const FRHIShaderParameterResource& Parameter : InBindlessParameters)
    {
        const FRHIDescriptorHandle Handle = UE::RHICore::GetBindlessParameterHandle(Parameter);
        if (Handle.IsValid())
        {
            checkf(Handle.IsValid(), TEXT("Metal resource did not provide a valid descriptor handle. Please validate that all Metal types can provide this or that the resource is still valid."));
            Binder.SetBindlessHandle(Handle, Parameter.Index);
        }
    }
#endif

    for (const FRHIShaderParameterResource& Parameter : InResourceParameters)
    {
        if (Parameter.Type == FRHIShaderParameterResource::EType::UnorderedAccessView)
        {
            if (ShaderFrequency == SF_Pixel || ShaderFrequency == SF_Compute)
            {
                Binder.SetUAV(static_cast<FRHIUnorderedAccessView*>(Parameter.Resource), Parameter.Index, true);
            }
            else
            {
                checkf(false, TEXT("TShaderRHI Can't have compute shader to be set. UAVs are not supported on vertex, tessellation and geometry shaders."));
            }
        }
    }

    for (const FRHIShaderParameterResource& Parameter : InResourceParameters)
    {
        switch (Parameter.Type)
        {
        case FRHIShaderParameterResource::EType::Texture:
            Binder.SetTexture(static_cast<FRHITexture*>(Parameter.Resource), Parameter.Index);
            break;
        case FRHIShaderParameterResource::EType::ResourceView:
            Binder.SetSRV(static_cast<FRHIShaderResourceView*>(Parameter.Resource), Parameter.Index);
            break;
        case FRHIShaderParameterResource::EType::UnorderedAccessView:
            break;
        case FRHIShaderParameterResource::EType::Sampler:
            Binder.SetSampler(static_cast<FRHISamplerState*>(Parameter.Resource), Parameter.Index);
            break;
        case FRHIShaderParameterResource::EType::UniformBuffer:
            BindUniformBuffer(Binder.StateCache, Shader, Binder.Stage, Parameter.Index, static_cast<FRHIUniformBuffer*>(Parameter.Resource));
            break;
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
        case FRHIShaderParameterResource::EType::ResourceCollection:
            Binder.SetResourceCollection(static_cast<FRHIResourceCollection*>(Parameter.Resource), Parameter.Index);
            break;
#endif
        default:
            checkf(false, TEXT("Unhandled resource type?"));
            break;
        }
    }
    
    
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    if(IsMetalBindlessEnabled())
    {
        Binder.StateCache.IRForwardBindlessParameters(Binder.Stage, InResourceParameters);
        Binder.StateCache.IRForwardBindlessParameters(Binder.Stage, InBindlessParameters);
    }
#endif
}

void FMetalRHICommandContext::RHISetShaderParameters(FRHIGraphicsShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
    const EShaderFrequency ShaderFrequency = Shader->GetFrequency();
    if (IsValidGraphicsFrequency(ShaderFrequency))
    {
        SetShaderParametersOnContext(
            *Context
            , Shader
            , ShaderFrequency
            , InParametersData
            , InParameters
            , InResourceParameters
            , InBindlessParameters
        );
    }
    else
    {
        checkf(0, TEXT("Unsupported FRHIGraphicsShader Type '%s'!"), GetShaderFrequencyString(ShaderFrequency, false));
    }
}

void FMetalRHICommandContext::RHISetShaderParameters(FRHIComputeShader* Shader, TConstArrayView<uint8> InParametersData, TConstArrayView<FRHIShaderParameter> InParameters, TConstArrayView<FRHIShaderParameterResource> InResourceParameters, TConstArrayView<FRHIShaderParameterResource> InBindlessParameters)
{
    SetShaderParametersOnContext(
        *Context
        , Shader
        , SF_Compute
        , InParametersData
        , InParameters
        , InResourceParameters
        , InBindlessParameters
    );
}

void FMetalRHICommandContext::RHISetStencilRef(uint32 StencilRef)
{
	Context->GetCurrentState().SetStencilRef(StencilRef);
}

void FMetalRHICommandContext::RHISetBlendFactor(const FLinearColor& BlendFactor)
{
	Context->GetCurrentState().SetBlendFactor(BlendFactor);
}

void FMetalRHICommandContext::SetRenderTargets(uint32 NumSimultaneousRenderTargets, const FRHIRenderTargetView* NewRenderTargets,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	FMetalContext* Manager = Context;
	FRHIDepthRenderTargetView DepthView;
	if (NewDepthStencilTargetRHI)
	{
		DepthView = *NewDepthStencilTargetRHI;
	}
	else
	{
		DepthView = FRHIDepthRenderTargetView(nullptr, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction);
	}

	FRHISetRenderTargetsInfo Info(NumSimultaneousRenderTargets, NewRenderTargets, DepthView);
	SetRenderTargetsAndClear(Info);
}

void FMetalRHICommandContext::SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
    MTL_SCOPED_AUTORELEASE_POOL;
		
	FRHIRenderPassInfo PassInfo;
	bool bHasTarget = (RenderTargetsInfo.DepthStencilRenderTarget.Texture != nullptr);
	FMetalContext* Manager = Context;
	
	for (uint32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; i++)
	{
		if (RenderTargetsInfo.ColorRenderTarget[i].Texture)
		{
			PassInfo.ColorRenderTargets[i].RenderTarget = RenderTargetsInfo.ColorRenderTarget[i].Texture;
			PassInfo.ColorRenderTargets[i].ArraySlice = RenderTargetsInfo.ColorRenderTarget[i].ArraySliceIndex;
			PassInfo.ColorRenderTargets[i].MipIndex = RenderTargetsInfo.ColorRenderTarget[i].MipIndex;
			PassInfo.ColorRenderTargets[i].Action = MakeRenderTargetActions(RenderTargetsInfo.ColorRenderTarget[i].LoadAction, RenderTargetsInfo.ColorRenderTarget[i].StoreAction);
		bHasTarget = (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr);
		}
	}
		
	if (RenderTargetsInfo.DepthStencilRenderTarget.Texture)
	{
		PassInfo.DepthStencilRenderTarget.DepthStencilTarget = RenderTargetsInfo.DepthStencilRenderTarget.Texture;
		PassInfo.DepthStencilRenderTarget.ExclusiveDepthStencil = RenderTargetsInfo.DepthStencilRenderTarget.GetDepthStencilAccess();
		PassInfo.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(RenderTargetsInfo.DepthStencilRenderTarget.DepthLoadAction, RenderTargetsInfo.DepthStencilRenderTarget.DepthStoreAction), MakeRenderTargetActions(RenderTargetsInfo.DepthStencilRenderTarget.StencilLoadAction, RenderTargetsInfo.DepthStencilRenderTarget.GetStencilStoreAction()));
	}
		
	PassInfo.NumOcclusionQueries = UINT16_MAX;
	PassInfo.bOcclusionQueries = true;

	// Ignore any attempt to "clear" the render-targets as that is senseless with the way MetalRHI has to try and coalesce passes.
	if (bHasTarget)
	{
		Manager->SetRenderPassInfo(PassInfo);

		// Set the viewport to the full size of render target 0.
		if (RenderTargetsInfo.ColorRenderTarget[0].Texture)
		{
			const FRHIRenderTargetView& RenderTargetView = RenderTargetsInfo.ColorRenderTarget[0];
			FMetalSurface* RenderTarget = GetMetalSurfaceFromRHITexture(RenderTargetView.Texture);

			uint32 Width = FMath::Max((uint32)(RenderTarget->Texture->width() >> RenderTargetView.MipIndex), (uint32)1);
			uint32 Height = FMath::Max((uint32)(RenderTarget->Texture->height() >> RenderTargetView.MipIndex), (uint32)1);

			RHISetViewport(0, 0, 0.0f, Width, Height, 1.0f);
		}
	}
}


void FMetalRHICommandContext::RHIDrawPrimitive(uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
	EPrimitiveType PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	
	NumInstances = FMath::Max(NumInstances,1u);
	
	RHI_DRAW_CALL_STATS(PrimitiveType,NumInstances*NumPrimitives);

	// how many verts to render
	uint32 NumVertices = GetVertexCountForPrimitiveCount(NumPrimitives, PrimitiveType);
	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	
	Context->DrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FMetalRHICommandContext::RHIDrawPrimitiveIndirect(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
    if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
    {
        SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
        EPrimitiveType PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
        
        
        RHI_DRAW_CALL_STATS(PrimitiveType,1);
        FMetalRHIBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
        
        Context->DrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
    }
    else
    {
        NOT_SUPPORTED("RHIDrawPrimitiveIndirect");
    }
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
void FMetalRHICommandContext::RHIDispatchMeshShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    MTL_SCOPED_AUTORELEASE_POOL;

#if METAL_USE_METAL_SHADER_CONVERTER
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	Context->DispatchMeshShader(PrimitiveType, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
#else
	NOT_SUPPORTED("RHIDispatchMeshShader");
#endif
}

void FMetalRHICommandContext::RHIDispatchIndirectMeshShader(FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;

#if METAL_USE_METAL_SHADER_CONVERTER
	uint32 PrimitiveType = Context->GetCurrentState().GetPrimitiveType();

	FMetalRHIBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);
	Context->DispatchIndirectMeshShader(PrimitiveType, ArgumentBuffer, ArgumentOffset);
#else
	NOT_SUPPORTED("RHIDispatchIndirectMeshShader");
#endif
}
#endif

void FMetalRHICommandContext::RHIDrawIndexedPrimitive(FRHIBuffer* IndexBufferRHI, int32 BaseVertexIndex, uint32 FirstInstance,
	uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
	//checkf(NumInstances == 1, TEXT("Currently only 1 instance is supported"));
	checkf(GRHISupportsBaseVertexIndex || BaseVertexIndex == 0, TEXT("BaseVertexIndex must be 0, see GRHISupportsBaseVertexIndex"));
	checkf(GRHISupportsFirstInstance || FirstInstance == 0, TEXT("FirstInstance must be 0, see GRHISupportsFirstInstance"));
	EPrimitiveType PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
	
		
	RHI_DRAW_CALL_STATS(PrimitiveType,FMath::Max(NumInstances,1u)*NumPrimitives);

	FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	Context->DrawIndexedPrimitive(IndexBuffer->GetCurrentBuffer(), IndexBuffer->GetStride(), IndexBuffer->GetIndexType(), PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FMetalRHICommandContext::RHIDrawIndexedIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 /*NumInstances*/)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
		EPrimitiveType PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
		

		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalRHIBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);
		
		Context->DrawIndexedIndirect(IndexBuffer, PrimitiveType, ArgumentsBuffer, DrawArgumentsIndex);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedIndirect");
	}
}

void FMetalRHICommandContext::RHIDrawIndexedPrimitiveIndirect(FRHIBuffer* IndexBufferRHI, FRHIBuffer* ArgumentBufferRHI, uint32 ArgumentOffset)
{
    MTL_SCOPED_AUTORELEASE_POOL;
    
	if (GetMetalDeviceContext().SupportsFeature(EMetalFeaturesIndirectBuffer))
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalDrawCallTime);
		
		EPrimitiveType PrimitiveType = Context->GetCurrentState().GetPrimitiveType();
		

		RHI_DRAW_CALL_STATS(PrimitiveType,1);
		FMetalRHIBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		FMetalRHIBuffer* ArgumentsBuffer = ResourceCast(ArgumentBufferRHI);
		
		Context->DrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}
	else
	{
		NOT_SUPPORTED("RHIDrawIndexedPrimitiveIndirect");
	}
}

void FMetalRHICommandContext::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{
	NOT_SUPPORTED("RHIClearMRT");
}

void FMetalRHICommandContext::RHISetDepthBounds(float MinDepth, float MaxDepth)
{
	METAL_IGNORED(FMetalRHICommandContextSetDepthBounds);
}

void FMetalRHICommandContext::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	Context->GetCurrentState().DiscardRenderTargets(Depth, Stencil, ColorBitMask);
}

#if PLATFORM_USES_FIXED_RHI_CLASS
#define INTERNAL_DECORATOR(Method) ((FMetalRHICommandContext&)CmdList.GetContext()).FMetalRHICommandContext::Method
#include "RHICommandListCommandExecutes.inl"
#endif

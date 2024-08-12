// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SPostBufferUpdate.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"

// Exclude SlateRHIRenderer related includes & implementations from server builds since the module is not a dependency / will not link for UMG on the server
#if !UE_SERVER
#include "FX/SlateFXSubsystem.h"
#include "FX/SlateRHIPostBufferProcessor.h"
#include "Interfaces/SlateRHIRenderingPolicyInterface.h"
#include "Rendering/DrawElements.h"
#include "Rendering/ElementBatcher.h"
#include "Math/MathFwd.h"
#include "RenderUtils.h"
#include "RHICommandList.h"
#include "RHIFwd.h"
#include "RHIResources.h"
#include "RHIUtilities.h"
#include "SlateRHIRendererSettings.h"

/**
 * Custom Slate drawer to update slate post buffer
 */
class FPostBufferUpdater : public ICustomSlateElement
{
public:

	//~ Begin ICustomSlateElement interface
	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override;
	virtual void PostCustomElementAdded(class FSlateElementBatcher& ElementBatcher) const override;
	//~ End ICustomSlateElement interface

public:

	/** True if we should perform the default post buffer update, used to set related state on ElementBatcher at Gamethread element batch time */
	bool bPerformDefaultPostBufferUpdate = true;

	/** True if buffers to update has been initialized. Used to ensure 'BuffersToUpdate_Renderthread' is only set once at initialization. */
	bool bBuffersToUpdateInitialized = false;

	/** 
	 * Buffers that we should update, all of these buffers will be affected by 'bPerformDefaultPostBufferUpdate' if disabled 
	 * This value is read by the Renderthread, so all non-initialization updates must be done via a render command.
	 * See 'FSlatePostBufferBlurProxy::OnUpdateValuesRenderThread' as an example.
	 * 
	 * Additionally, note that this value should be masked against the currently enabled buffers in 'USlateRHIRendererSettings'
	 */
	ESlatePostRT BuffersToUpdate_Renderthread = ESlatePostRT::None;

	/** Proxies used to update a post processor within a frame */
	TMap<ESlatePostRT, TSharedPtr<FSlatePostProcessorUpdaterProxy>> ProcessorUpdaters;
};

/////////////////////////////////////////////////////
// FPostBufferUpdater

void FPostBufferUpdater::Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs)
{
	for (ESlatePostRT SlatePostBufferBit : MakeFlagsRange(Inputs.UsedSlatePostBuffers & BuffersToUpdate_Renderthread))
	{
		UTextureRenderTarget2D* SlatePostBuffer = Cast<UTextureRenderTarget2D>(USlateRHIRendererSettings::Get()->TryGetPostBufferRT(SlatePostBufferBit));
		if (!SlatePostBuffer)
		{
			continue;
		}

		// Provided output texture is actually the input into our custom post process texture.
		const FScreenPassTexture InputTexture(Inputs.OutputTexture, Inputs.SceneViewRect);

		// Write to our custom post process texture.
		const FScreenPassTexture OutputTexture(RegisterExternalTexture(GraphBuilder, SlatePostBuffer->TextureReference.TextureReferenceRHI, TEXT("SlatePostProcessTexture")));

		if (TSharedPtr<FSlateRHIPostBufferProcessorProxy> PostProcessorProxy = USlateFXSubsystem::GetPostProcessorProxy(SlatePostBufferBit))
		{
			if (TSharedPtr<FSlatePostProcessorUpdaterProxy>* ProcessorUpdaterItr = ProcessorUpdaters.Find(SlatePostBufferBit))
			{
				if (TSharedPtr<FSlatePostProcessorUpdaterProxy> ProcessorUpdater = *ProcessorUpdaterItr)
				{
					ProcessorUpdater->UpdateProcessor_RenderThread(PostProcessorProxy);

					if (ProcessorUpdater->bSkipBufferUpdate)
					{
						return;
					}
				}
			}

			PostProcessorProxy->PostProcess_Renderthread(GraphBuilder, InputTexture, OutputTexture);
		}
		else
		{
			AddDrawTexturePass(GraphBuilder, FScreenPassViewInfo(), InputTexture, OutputTexture);
		}
	}
}

void FPostBufferUpdater::PostCustomElementAdded(FSlateElementBatcher& ElementBatcher) const
{
	ESlatePostRT PrevResourceUpdatingPostBuffers = ElementBatcher.GetResourceUpdatingPostBuffers();
	ElementBatcher.SetResourceUpdatingPostBuffers(PrevResourceUpdatingPostBuffers | BuffersToUpdate_Renderthread);

	if (!bPerformDefaultPostBufferUpdate)
	{
		ESlatePostRT PrevSkipDefaultUpdatePostBuffers = ElementBatcher.GetSkipDefaultUpdatePostBuffers();
		ElementBatcher.SetSkipDefaultUpdatePostBuffers(PrevSkipDefaultUpdatePostBuffers | BuffersToUpdate_Renderthread);
	}

	// Give proxies a chance to update their renderthread values.
	for (ESlatePostRT SlatePostBufferBit : MakeFlagsRange(BuffersToUpdate_Renderthread))
	{
		if (!USlateRHIRendererSettings::Get()->GetSlatePostSetting(SlatePostBufferBit).bEnabled)
		{
			continue;
		}

		if (TSharedPtr<FSlateRHIPostBufferProcessorProxy> PostProcessorProxy = USlateFXSubsystem::GetPostProcessorProxy(SlatePostBufferBit))
		{
			PostProcessorProxy->OnUpdateValuesRenderThread();
		}
	}
}

#endif // !UE_SERVER

/////////////////////////////////////////////////////
// SPostBufferUpdate

SLATE_IMPLEMENT_WIDGET(SPostBufferUpdate)
void SPostBufferUpdate::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
}

SPostBufferUpdate::SPostBufferUpdate()
	: bPerformDefaultPostBufferUpdate(true)
	, BuffersToUpdate({})
	, PostBufferUpdater(nullptr)
{
}

void SPostBufferUpdate::Construct(const FArguments& InArgs)
{
#if !UE_SERVER
	bPerformDefaultPostBufferUpdate = InArgs._bPerformDefaultPostBufferUpdate;

	BuffersToUpdate = {};

	PostBufferUpdater = MakeShared<FPostBufferUpdater>();
	if (PostBufferUpdater)
	{
		PostBufferUpdater->bPerformDefaultPostBufferUpdate = bPerformDefaultPostBufferUpdate;
	}
#endif // !UE_SERVER
}

void SPostBufferUpdate::SetPerformDefaultPostBufferUpdate(bool bInPerformDefaultPostBufferUpdate)
{
#if !UE_SERVER
	bPerformDefaultPostBufferUpdate = bInPerformDefaultPostBufferUpdate;

	if (PostBufferUpdater)
	{
		PostBufferUpdater->bPerformDefaultPostBufferUpdate = bPerformDefaultPostBufferUpdate;
	}
#endif // !UE_SERVER
}

bool SPostBufferUpdate::GetPerformDefaultPostBufferUpdate() const
{
	return bPerformDefaultPostBufferUpdate;
}

void SPostBufferUpdate::SetBuffersToUpdate(const TArrayView<ESlatePostRT> InBuffersToUpdate)
{
#if !UE_SERVER
	BuffersToUpdate = InBuffersToUpdate;

	if (PostBufferUpdater && !PostBufferUpdater->bBuffersToUpdateInitialized)
	{
		PostBufferUpdater->BuffersToUpdate_Renderthread = ESlatePostRT::None;
		for (ESlatePostRT BufferToUpdate : BuffersToUpdate)
		{
			if (USlateRHIRendererSettings::Get()->GetSlatePostSetting(BufferToUpdate).bEnabled)
			{
				PostBufferUpdater->BuffersToUpdate_Renderthread |= BufferToUpdate;
			}
		}

		PostBufferUpdater->bBuffersToUpdateInitialized = true;
	}
#endif // !UE_SERVER
}

void SPostBufferUpdate::SetProcessorUpdaters(TMap<ESlatePostRT, TSharedPtr<FSlatePostProcessorUpdaterProxy>> InProcessorUpdaters)
{
#if !UE_SERVER
	if (PostBufferUpdater)
	{
		PostBufferUpdater->ProcessorUpdaters = InProcessorUpdaters;
	}
#endif // !UE_SERVER
}

const TArrayView<const ESlatePostRT> SPostBufferUpdate::GetBuffersToUpdate() const
{
	return MakeArrayView(BuffersToUpdate);
}

UMG_API void SPostBufferUpdate::ReleasePostBufferUpdater()
{
#if !UE_SERVER
	// Copy the pointer onto a lambda to defer the final deletion to after any pending uses on the renderthread
	TSharedPtr<FPostBufferUpdater> ReleaseMe = PostBufferUpdater;
	ENQUEUE_RENDER_COMMAND(ReleaseCommand)([ReleaseMe](FRHICommandList& RHICmdList) mutable
	{
		ReleaseMe.Reset();
	});

	PostBufferUpdater.Reset();
#endif // !UE_SERVER
}

int32 SPostBufferUpdate::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
#if !UE_SERVER
	FSlateRect RenderBoundingRect = AllottedGeometry.GetRenderBoundingRect();
	FPaintGeometry PaintGeometry(RenderBoundingRect.GetTopLeft(), RenderBoundingRect.GetSize(), AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());
	FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, PostBufferUpdater);
#endif // !UE_SERVER

	// Increment LayerId to ensure items afterwards are not processed
	return ++LayerId;
}

FVector2D SPostBufferUpdate::ComputeDesiredSize( float ) const
{
	return FVector2D::ZeroVector;
}


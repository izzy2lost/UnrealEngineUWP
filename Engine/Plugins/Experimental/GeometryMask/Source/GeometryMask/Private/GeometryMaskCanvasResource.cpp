// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskCanvasResource.h"

#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GeometryMaskModule.h"
#include "GeometryMaskSettings.h"
#include "GeometryMaskTypes.h"
#include "SceneView.h"
#include "Shaders/GeometryMaskPostProcess.h"
#include "Shaders/GeometryMaskPostProcess_Blur.h"
#include "Shaders/GeometryMaskPostProcess_DistanceField.h"
#include "TextureResource.h"
#include "UObject/Package.h"

namespace UE::GeometryMask::Private
{
	void OverscanProjectionMatrix(FMatrix& InOutMatrix, const FIntPoint& InSize, const int32 InPadding)
	{
		const FVector2f Multiplier(
			static_cast<float>(InSize.X) / static_cast<float>(InSize.X + InPadding),
			static_cast<float>(InSize.Y) / static_cast<float>(InSize.Y + InPadding));

		// Original Calc - we simply scale each
		// [0][0] = MultFOVX / FMath::Tan(HalfFOVX)
		// [1][1] = MultFOVY / FMath::Tan(HalfFOVY)
		
		InOutMatrix.M[0][0] *= Multiplier.X;
		InOutMatrix.M[1][1] *= Multiplier.Y;
	}
}

UGeometryMaskCanvasResource::UGeometryMaskCanvasResource()
{
	// @note: We omit the Alpha channel as it's not uniformly supported
	DependentCanvasNames = {
		{ EGeometryMaskColorChannel::Red, NAME_None },
		{ EGeometryMaskColorChannel::Green, NAME_None },
		{ EGeometryMaskColorChannel::Blue, NAME_None }
	};

	FGeometryMaskPostProcessParameters_Blur PostProcessParameters_Blur;
	PostProcessParameters_Blur.PerChannelBlurStrength = { 16, 16, 16, 16};
	PostProcess_Blur = MakeShared<FGeometryMaskPostProcess_Blur>(PostProcessParameters_Blur);

	FGeometryMaskPostProcessParameters_DistanceField PostProcessParameters_DistanceField;
	PostProcessParameters_DistanceField.bPerChannelCalculateDF.SetRange(0, 4, true);
	PostProcess_DistanceField = MakeShared<FGeometryMaskPostProcess_DistanceField>(PostProcessParameters_DistanceField);
}

const EGeometryMaskColorChannel UGeometryMaskCanvasResource::GetNextAvailableColorChannel() const
{
	for (const TPair<EGeometryMaskColorChannel, FName>& ColorChannelCanvas : DependentCanvasNames)
	{
		if (ColorChannelCanvas.Value.IsNone())
		{
			return ColorChannelCanvas.Key;
		}
	}

	return EGeometryMaskColorChannel::None;
}

bool UGeometryMaskCanvasResource::Checkout(
	const EGeometryMaskColorChannel InColorChannel
	, const FName InRequestingCanvasName)
{
	if (!ensure(InColorChannel != EGeometryMaskColorChannel::Num && InColorChannel != EGeometryMaskColorChannel::None))
	{
		return false;
	}

	if (!ensure(DependentCanvasNames[InColorChannel].IsNone()))
	{
		return false;
	}

	DependentCanvasNames[InColorChannel] = InRequestingCanvasName;
	return true;
}

bool UGeometryMaskCanvasResource::Checkin(const FName InRequestingCanvasName)
{
	for (TPair<EGeometryMaskColorChannel, FName>& ColorChannelCanvas : DependentCanvasNames)
	{
		if (ColorChannelCanvas.Value.IsEqual(InRequestingCanvasName))
		{
			// Effectively free this ColorChannel and make available for Checkout
			ColorChannelCanvas.Value = NAME_None;
			return true;
		}
	}

	// The requesting canvas name wasn't present in this Resource
	return false;
}

void UGeometryMaskCanvasResource::UpdateViewportSize()
{
	if (ViewportSize.Size() > 0
		&& RenderTargetTexture)
	{
		const float SizeMultiplier = GetDefault<UGeometryMaskSettings>()->GetDefaultResolutionMultiplier();
		
		int32 SizeX = ViewportSize.X * SizeMultiplier;
		int32 SizeY = ViewportSize.Y * SizeMultiplier;

		const int32 Padding = GetViewportPadding();
		SizeX += Padding * SizeMultiplier;
		SizeY += Padding * SizeMultiplier;

		if (const float RatioX = static_cast<float>(SizeX) / MaxTextureSize;
			RatioX > 1.0)
		{
			// Width too big, cap to max and reduce height proportionally
			SizeX = MaxTextureSize;
			SizeY /= RatioX;
		}

		if (const float RatioY = static_cast<float>(SizeY) / MaxTextureSize;
			RatioY > 1.0)
		{
			// Height too big, cap to max and reduce width proportionally
			SizeY = MaxTextureSize;
			SizeX /= RatioY;
		}

		// Update RT size to viewport size
		RenderTargetTexture->ResizeTarget(SizeX, SizeY);
	}
}

void UGeometryMaskCanvasResource::UpdateRenderParameters(
	const EGeometryMaskColorChannel InColorChannel,
	const bool bInApplyBlur,
	const double InBlurStrength,
	bool bInApplyFeather,
	int32 InOuterFeatherRadius,
	int32 InInnerFeatherRadius)
{
	const int32 ChannelIdx = static_cast<int32>(InColorChannel);

	auto LogOutOfBounds = [InColorChannel](){
		const FStringView ChannelStringView = UE::GeometryMask::ChannelToString(InColorChannel);
		TCHAR* ChannelString = nullptr;
		ChannelStringView.CopyString(ChannelString, ChannelStringView.Len());
		
		UE_LOG(LogGeometryMask, Error, TEXT("ColorChannel wasn't valid for setting shader parameters (was '%s', expected R, G, B or A"), ChannelString);
	};

	FGeometryMaskPostProcessParameters_Blur BlurParameters = PostProcess_Blur->GetParameters();
	{
		if (BlurParameters.bPerChannelApplyBlur.IsValidIndex(ChannelIdx))
		{
			BlurParameters.bPerChannelApplyBlur[ChannelIdx] = bInApplyBlur && InBlurStrength > 0.0;
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		if (BlurParameters.PerChannelBlurStrength.IsValidIndex(ChannelIdx))
		{
			BlurParameters.PerChannelBlurStrength[ChannelIdx] = InBlurStrength;	
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		PostProcess_Blur->SetParameters(BlurParameters);
	}

	FGeometryMaskPostProcessParameters_DistanceField DFParameters = PostProcess_DistanceField->GetParameters();
	{
		if (DFParameters.bPerChannelCalculateDF.IsValidIndex(ChannelIdx))
		{
			DFParameters.bPerChannelCalculateDF[ChannelIdx] = bInApplyFeather && (InOuterFeatherRadius + InInnerFeatherRadius) > 0;
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		if (DFParameters.PerChannelRadius.IsValidIndex(ChannelIdx))
		{
			DFParameters.PerChannelRadius[ChannelIdx] = FMath::Max(InOuterFeatherRadius, InInnerFeatherRadius);	
		}
		else
		{
			LogOutOfBounds();
			return;
		}

		PostProcess_DistanceField->SetParameters(DFParameters);
	}

	// Effects may have changed viewport padding
	UpdateViewportSize();

	bApplyBlur = BlurParameters.bPerChannelApplyBlur.CountSetBits(0) > 0; // if any channels have blur
	bApplyDF = DFParameters.bPerChannelCalculateDF.CountSetBits(0) > 0; // if any channels have DF
}

void UGeometryMaskCanvasResource::SetViewportSize(const FIntPoint& InViewportSize)
{
	if (InViewportSize.Size() > 0
		&& ViewportSize != InViewportSize)
	{
		ViewportSize = InViewportSize;
		UpdateViewportSize();		
	}
}

int32 UGeometryMaskCanvasResource::GetViewportPadding() const
{
	int32 MaxFeatherRadius = 0;
	{
		const FGeometryMaskPostProcessParameters_DistanceField& DistanceFieldParameters = PostProcess_DistanceField->GetParameters();
		for (int32 ChannelIdx = 0; ChannelIdx < MaxNumChannels; ++ChannelIdx)
		{
			if (DistanceFieldParameters.bPerChannelCalculateDF[ChannelIdx])
			{
				MaxFeatherRadius = FMath::Max(MaxFeatherRadius, DistanceFieldParameters.PerChannelRadius[ChannelIdx]);
			}
		}
	}

	int32 MaxBlurRadius = 0;
	{
		const FGeometryMaskPostProcessParameters_Blur& BlurParameters = PostProcess_Blur->GetParameters();
		for (int32 ChannelIdx = 0; ChannelIdx < MaxNumChannels; ++ChannelIdx)
		{
			if (BlurParameters.bPerChannelApplyBlur[ChannelIdx])
			{
				MaxBlurRadius = FMath::Max(MaxBlurRadius, UE::GeometryMask::Internal::ComputeEffectiveKernelSize(BlurParameters.PerChannelBlurStrength[ChannelIdx]));
			}
		}
	}

	
	return FMath::Max(MaxFeatherRadius, MaxBlurRadius);
}

UTextureRenderTarget2D* UGeometryMaskCanvasResource::GetRenderTargetTexture()
{
	if (!RenderTargetTexture)
	{
		const FName ObjectName = MakeUniqueObjectName(this, UTextureRenderTarget2D::StaticClass(), FName(FString::Printf(TEXT("GeometryMaskCanvasResource_RenderTarget"))));
		RenderTargetTexture = NewObject<UTextureRenderTarget2D>(this, ObjectName);
		RenderTargetTexture->bForceLinearGamma = false;
		RenderTargetTexture->InitAutoFormat(ViewportSize.X, ViewportSize.Y);
	}

	return RenderTargetTexture;
}

void UGeometryMaskCanvasResource::Update(
	UWorld* InWorld,
	FSceneView& InView)
{
	Draw(InWorld, InView);
}

void UGeometryMaskCanvasResource::Draw(UWorld* InWorld, FSceneView& InView)
{
	if (!InWorld)
	{
		return;
	}

	if (UTextureRenderTarget2D* Texture = GetRenderTargetTexture())
	{
		if (!CanvasObject)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage());
		}

		// Begin
		{
			SetViewportSize(InView.UnconstrainedViewRect.Size());
			FMatrix ProjectionMatrix = InView.ViewMatrices.GetProjectionMatrix();
			UE::GeometryMask::Private::OverscanProjectionMatrix(ProjectionMatrix, ViewportSize, GetViewportPadding());

			CachedViewProjectionMatrix = InView.ViewMatrices.GetViewMatrix() * ProjectionMatrix;
			CachedViewProjectionMatrix.M[2][2] = UE_KINDA_SMALL_NUMBER; // Prevents div by zero later

			UpdateViewportSize();
			
			InWorld->FlushDeferredParameterCollectionInstanceUpdates();

			FTextureRenderTargetResource* RenderTargetResource = Texture->GameThread_GetRenderTargetResource();
			FCanvas* NewCanvas = new FCanvas(
				RenderTargetResource,
				nullptr,
				InWorld,
				InWorld->GetFeatureLevel(),
				// Draw immediately so that interleaved SetVectorParameter (etc) function calls work as expected
				FCanvas::CDM_ImmediateDrawing);

			CanvasObject->Init(Texture->SizeX, Texture->SizeY, &InView, NewCanvas);
			CanvasObject->Update();
			CanvasObject->SetView(&InView);
			CanvasObject->Canvas->Clear(GetRenderTargetTexture()->ClearColor);
		}

		// Contents
		{
			// Store current transform
			const FMatrix CanvasMatrix = CanvasObject->Canvas->GetBottomTransform();

			// Set to World->Viewport transform
			CanvasObject->Canvas->SetBaseTransform(CachedViewProjectionMatrix);
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskCanvasResource::Draw::OnDrawToCanvas);
				
				OnDrawToCanvas().Broadcast(CanvasObject->Canvas);
			}

			// Restore original transform
			CanvasObject->Canvas->SetBaseTransform(CanvasMatrix);
		}
		
		// End
		{
			if (CanvasObject && CanvasObject->Canvas)
			{
				CanvasObject->Canvas->Flush_GameThread();
				
				// Post Process
				if (bApplyDF || bApplyBlur)
				{
					FRenderTarget* RenderTarget = CanvasObject->Canvas->GetRenderTarget();
					if (bApplyDF)
					{
						PostProcess_DistanceField->Execute(RenderTarget);	
					}

					if (bApplyBlur)
					{
						PostProcess_Blur->Execute(RenderTarget);
					}
				}

				delete CanvasObject->Canvas;
				CanvasObject->Canvas = nullptr;
			}
		}
	}
}

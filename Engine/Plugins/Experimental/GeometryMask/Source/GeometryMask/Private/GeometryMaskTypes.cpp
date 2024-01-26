// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskTypes.h"

#include "Engine/Engine.h"
#include "GeometryMaskSubsystem.h"
#include "UObject/UObjectThreadContext.h"

namespace UE::GeometryMask
{
	EGeometryMaskColorChannel VectorToMaskChannel(const FLinearColor& InVector)
	{
		float CurrentMax = std::numeric_limits<float>::min();
		int32 MaxIdx = 0;
		for (int32 ElementIdx = 0; ElementIdx < 4; ++ElementIdx)
		{
			const float CurrentValue = InVector.Component(ElementIdx);			
			if (CurrentValue > CurrentMax)
			{
				CurrentMax = CurrentValue;
				MaxIdx = ElementIdx;
			}
		}
		
		return static_cast<EGeometryMaskColorChannel>(MaxIdx);
	}

	EGeometryMaskColorChannel GetValidMaskChannel(
		EGeometryMaskColorChannel InColorChannel,
		bool bInIncludeAlpha)
	{
		return FMath::Clamp(InColorChannel, EGeometryMaskColorChannel::Red, bInIncludeAlpha ? EGeometryMaskColorChannel::Alpha : EGeometryMaskColorChannel::Blue);
	}

	FStringView ChannelToString(EGeometryMaskColorChannel InColorChannel)
	{
		return MaskChannelEnumToString[FMath::Clamp(InColorChannel, EGeometryMaskColorChannel::Red, EGeometryMaskColorChannel::Num)];
	}
}

UTextureRenderTarget2D* UGeometryMaskCanvasReferenceComponentBase::GetTexture()
{
	if (const UGeometryMaskCanvas* Canvas = CanvasWeak.Get())
	{
		return Canvas->GetTexture();
	}
	
	return nullptr;
}

void UGeometryMaskCanvasReferenceComponentBase::BeginPlay()
{
	Super::BeginPlay();

	TryResolveCanvas();
}

void UGeometryMaskCanvasReferenceComponentBase::PostLoad()
{
	Super::PostLoad();

	if (!IsTemplate())
	{
		TryResolveCanvas();
	}
}

void UGeometryMaskCanvasReferenceComponentBase::OnRegister()
{
	Super::OnRegister();

	if (!IsTemplate())
	{
		TryResolveCanvas();
	}
}

bool UGeometryMaskCanvasReferenceComponentBase::TryResolveNamedCanvas(FName InCanvasName)
{
	auto BroadcastSetCanvas = [this](const UGeometryMaskCanvas* InCanvas)
	{
		ReceiveSetCanvas(InCanvas);
		OnSetCanvasDelegate.Broadcast(InCanvas);
	};

	// Get current canvas
	UGeometryMaskCanvas* Canvas = CanvasWeak.Get();
	
	if (Canvas)
	{
		// Canvas set, no need to resolve it
		if (Canvas->GetFName() == InCanvasName)
		{
			return true;
		}

		// Canvas is not the correct one
		CanvasWeak.Reset();
	}

	if (UGeometryMaskSubsystem* Subsystem = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>())
	{
		Canvas = Subsystem->GetNamedCanvas(InCanvasName);

		CanvasWeak = Canvas;
		
		if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
		{
			if (Canvas)
			{
				BroadcastSetCanvas(Canvas);
			}
		}
	}

	return CanvasWeak.IsValid();
}

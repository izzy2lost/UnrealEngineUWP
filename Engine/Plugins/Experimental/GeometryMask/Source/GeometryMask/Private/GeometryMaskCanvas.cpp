// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskCanvas.h"

#include "Algo/RemoveIf.h"
#include "CanvasTypes.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GeometryMaskCanvasResource.h"
#include "GeometryMaskModule.h"
#include "IGeometryMaskWriteInterface.h"

const FName UGeometryMaskCanvas::ApplyBlurPropertyName = GET_MEMBER_NAME_CHECKED(UGeometryMaskCanvas, bApplyBlur);
const FName UGeometryMaskCanvas::BlurStrengthPropertyName = GET_MEMBER_NAME_CHECKED(UGeometryMaskCanvas, BlurStrength);
const FName UGeometryMaskCanvas::ApplyFeatherPropertyName = GET_MEMBER_NAME_CHECKED(UGeometryMaskCanvas, bApplyFeather);
const FName UGeometryMaskCanvas::OuterFeatherRadiusPropertyName = GET_MEMBER_NAME_CHECKED(UGeometryMaskCanvas, InnerFeatherRadius);
const FName UGeometryMaskCanvas::InnerFeatherRadiusPropertyName = GET_MEMBER_NAME_CHECKED(UGeometryMaskCanvas, OuterFeatherRadius);

void UGeometryMaskCanvas::BeginDestroy()
{
	FreeResource();
	
	UObject::BeginDestroy();
}

#if WITH_EDITOR
void UGeometryMaskCanvas::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	UObject::PostEditChangeProperty(InPropertyChangedEvent);

	static TSet<FName> UpdateRenderParameterProperties =
	{
		ApplyBlurPropertyName,
		BlurStrengthPropertyName,
		ApplyFeatherPropertyName,
		OuterFeatherRadiusPropertyName,
		InnerFeatherRadiusPropertyName
	};

	if (UpdateRenderParameterProperties.Contains(InPropertyChangedEvent.GetPropertyName()))
	{
		UpdateRenderParameters();
	}
}
#endif

const TArray<TWeakInterfacePtr<IGeometryMaskWriteInterface>>& UGeometryMaskCanvas::GetWriters() const
{
	return Writers;
}

void UGeometryMaskCanvas::AddWriter(const TScriptInterface<IGeometryMaskWriteInterface>& InWriter)
{
	if (!ensure(InWriter))
	{
		return;
	}

	RemoveInvalidWriters();

	// Writers is empty, but about to become activated
	if (Writers.IsEmpty())
	{
		OnActivated().ExecuteIfBound();
	}

	Writers.Add(InWriter.GetObject());
}

void UGeometryMaskCanvas::AddWriters(const TArray<TScriptInterface<IGeometryMaskWriteInterface>>& InWriters)
{
	if (InWriters.IsEmpty())
	{
		return;
	}

	RemoveInvalidWriters();
	for (const TScriptInterface<IGeometryMaskWriteInterface>& Writer : InWriters)
	{
		AddWriter(Writer);
	}
}

void UGeometryMaskCanvas::RemoveWriter(const TScriptInterface<IGeometryMaskWriteInterface>& InWriter)
{
	if (!ensure(InWriter))
	{
		return;
	}

	RemoveInvalidWriters();
	Writers.Remove(InWriter.GetObject());
}

UCanvasRenderTarget2D* UGeometryMaskCanvas::GetTexture() const
{
	if (CanvasResource)
	{
		return CanvasResource->GetRenderTargetTexture();
	}

	return nullptr;
}

bool UGeometryMaskCanvas::IsBlurApplied() const
{
	return bApplyBlur;
}

void UGeometryMaskCanvas::SetApplyBlur(const bool bInValue)
{
	if (bInValue != bApplyBlur)
	{
		bApplyBlur = bInValue;
		UpdateRenderParameters();
	}
}

double UGeometryMaskCanvas::GetBlurStrength() const
{
	return BlurStrength;
}

void UGeometryMaskCanvas::SetBlurStrength(const double InValue)
{
	if (!FMath::IsNearlyEqual(InValue, BlurStrength))
	{
		BlurStrength = InValue;
		UpdateRenderParameters();
	}
}

bool UGeometryMaskCanvas::IsFeatherApplied() const
{
	return bApplyFeather;
}

void UGeometryMaskCanvas::SetApplyFeather(const bool bInValue)
{
	if (bInValue != bApplyFeather)
	{
		bApplyFeather = bInValue;
		UpdateRenderParameters();
	}
}

int32 UGeometryMaskCanvas::GetOuterFeatherRadius() const
{
	return OuterFeatherRadius;
}

void UGeometryMaskCanvas::SetOuterFeatherRadius(const int32 InValue)
{
	if (InValue != OuterFeatherRadius)
	{
		OuterFeatherRadius = InValue;
		UpdateRenderParameters();
	}
}

int32 UGeometryMaskCanvas::GetInnerFeatherRadius() const
{
	return InnerFeatherRadius;
}

void UGeometryMaskCanvas::SetInnerFeatherRadius(const int32 InValue)
{
	if (InValue != InnerFeatherRadius)
	{
		InnerFeatherRadius = InValue;
		UpdateRenderParameters();
	}
}

EGeometryMaskColorChannel UGeometryMaskCanvas::GetColorChannel() const
{
	return ColorChannel;
}

void UGeometryMaskCanvas::RemoveInvalidWriters()
{
	if (Writers.IsEmpty())
	{
		return;
	}

	Writers.SetNum(Algo::RemoveIf(Writers, [](const TWeakInterfacePtr<IGeometryMaskWriteInterface>& InWriter)
	{
		return !InWriter.IsValid() || !IsValid(InWriter.GetObject());
	}));

	if (Writers.IsEmpty())
	{
		// No writers after cleanup, flag as deactivated
		OnDeactivated().ExecuteIfBound();
	}
}

void UGeometryMaskCanvas::OnDrawToCanvas(FCanvas* InCanvas)
{
	// Sort so subtract happens after additive, etc.
	SortWriters();

	for (const TWeakInterfacePtr<IGeometryMaskWriteInterface>& Writer : Writers)
	{
		if (Writer.IsValid())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UGeometryMaskCanvas::OnDrawToCanvas);
			DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UGeometryMaskCanvas::OnDrawToCanvas"), STAT_GeometryMask_Update, STATGROUP_GeometryMask);
			Writer->DrawToCanvas(InCanvas);	
		}
	}
}

void UGeometryMaskCanvas::UpdateRenderParameters()
{
	if (CanvasResource)
	{
		CanvasResource->UpdateRenderParameters(ColorChannel, bApplyBlur, BlurStrength, bApplyFeather, OuterFeatherRadius, InnerFeatherRadius);
	}
}

void UGeometryMaskCanvas::Update(
	UWorld* InWorld,
	FSceneView& InView)
{
	RemoveInvalidWriters();
}

void UGeometryMaskCanvas::AssignResource(
	UGeometryMaskCanvasResource* InResource,
	const EGeometryMaskColorChannel InColorChannel)
{
	// Cleanup existing resource, if assigned
	FreeResource();

	CanvasResource = InResource;
	ColorChannel = InColorChannel;

	CanvasResource->OnDrawToCanvas().AddUObject(this, &UGeometryMaskCanvas::OnDrawToCanvas);

	UpdateRenderParameters();
}

void UGeometryMaskCanvas::FreeResource()
{
	if (CanvasResource)
	{
		CanvasResource->OnDrawToCanvas().RemoveAll(this);
		ColorChannel = EGeometryMaskColorChannel::None;
	
		// Free up the resource
		CanvasResource->Checkin(CanvasName);
		CanvasResource = nullptr;
	}
}

FName UGeometryMaskCanvas::GetApplyBlurPropertyName()
{
	return ApplyBlurPropertyName;
}

FName UGeometryMaskCanvas::GetBlurStrengthPropertyName()
{
	return BlurStrengthPropertyName;
}

FName UGeometryMaskCanvas::GetApplyFeatherPropertyName()
{
	return ApplyFeatherPropertyName;
}

FName UGeometryMaskCanvas::GetOuterFeatherRadiusPropertyName()
{
	return OuterFeatherRadiusPropertyName;
}

FName UGeometryMaskCanvas::GetInnerFeatherRadiusPropertyName()
{
	return InnerFeatherRadiusPropertyName;
}

void UGeometryMaskCanvas::SortWriters()
{
	Algo::SortBy(Writers, [](const TWeakInterfacePtr<IGeometryMaskWriteInterface>& InWriter)
	{
		if (const IGeometryMaskWriteInterface* Writer = InWriter.Get())
		{
			return Writer->GetParameters().OperationType;
		}

		return EGeometryMaskCompositeOperation::Num;
	});
}

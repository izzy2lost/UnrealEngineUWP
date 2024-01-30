// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/AvaViewportBackplateVisualizer.h"
#include "AvaViewportPostProcessManager.h"
#include "AvaViewportSettings.h"
#include "AvaViewportUtils.h"
#include "AvaVisibleArea.h"
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"
#include "Viewport/Interaction/AvaViewportPostProcessInfo.h"
#include "Viewport/Interaction/IAvaViewportDataProvider.h"
#include "Viewport/Interaction/IAvaViewportDataProxy.h"
#include "ViewportClient/IAvaViewportClient.h"

#define LOCTEXT_NAMESPACE "AvaViewportBackplateVisualizer"

namespace UE::AvalancheViewport::Private
{
	const FString BackplateReferencerName = FString(TEXT("AvaViewportBackplateVisualizer"));
	const FName TextureObjectName = FName(TEXT("TextureObject"));
	const FName TextureOffsetName = FName(TEXT("TextureOffset"));
	const FName TextureScaleName = FName(TEXT("TextureScale"));
}

FAvaViewportBackplateVisualizer::FAvaViewportBackplateVisualizer(TSharedRef<IAvaViewportClient> InAvaViewportClient)
	: FAvaViewportPostProcessVisualizer(InAvaViewportClient)
{
	bRequiresTonemapperSetting = true;

	TextureOffset = FVector::ZeroVector;
	TextureScale = FVector::ZeroVector;

	const UAvaViewportSettings* ViewportSettings = GetDefault<UAvaViewportSettings>();

	if (!ViewportSettings)
	{
		return;
	}

	UMaterial* BackplateMaterial = ViewportSettings->ViewportBackplateMaterial.LoadSynchronous();

	if (!BackplateMaterial)
	{
		return;
	}

	PostProcessBaseMaterial = BackplateMaterial;
	PostProcessMaterial = UMaterialInstanceDynamic::Create(BackplateMaterial, GetTransientPackage());
}

UTexture* FAvaViewportBackplateVisualizer::GetTexture() const
{
	return Texture;
}

void FAvaViewportBackplateVisualizer::SetTexture(UTexture* InTexture)
{
	if (Texture == InTexture)
	{
		return;
	}

	SetTextureInternal(InTexture);

	UpdatePostProcessInfo();
	UpdatePostProcessMaterial();
}

void FAvaViewportBackplateVisualizer::AddReferencedObjects(FReferenceCollector& InCollector)
{
	Super::AddReferencedObjects(InCollector);

	if (Texture)
	{
		InCollector.AddReferencedObject(Texture);
	}
}

FString FAvaViewportBackplateVisualizer::GetReferencerName() const
{
	return UE::AvalancheViewport::Private::BackplateReferencerName;
}

void FAvaViewportBackplateVisualizer::UpdateForViewport(const FAvaVisibleArea& InVisibleArea, const FVector2f& InWidgetSize, 
	const FVector2f& InCameraOffset)
{
	if (FMath::IsNearlyZero(PostProcessOpacity) || !Texture || !PostProcessMaterial)
	{
		return;
	}

	if (!InVisibleArea.IsValid())
	{
		return;
	}

	if (!FAvaViewportUtils::IsValidViewportSize(InWidgetSize))
	{
		return;
	}

	const FVector2f ImageSize = {Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight()};

	if (!FAvaViewportUtils::IsValidViewportSize(ImageSize))
	{
		return;
	}

	using namespace UE::AvalancheViewport::Private;

	const float ImageAspectRatio = ImageSize.X / ImageSize.Y;
	const float WidgetAspectRatio = InWidgetSize.X / InWidgetSize.Y;
	const float ViewportAspectRatio = InVisibleArea.AbsoluteSize.X / InVisibleArea.AbsoluteSize.Y;
	const FVector2f WidgetBasedScale = InVisibleArea.AbsoluteSize / InWidgetSize;

	const FVector2f Scale = WidgetBasedScale / InVisibleArea.GetVisibleAreaFraction();

	if (!FMath::IsNearlyEqual(TextureScale.X, Scale.X)
		|| !FMath::IsNearlyEqual(TextureScale.Y, Scale.Y))
	{
		TextureScale.X = Scale.X;
		TextureScale.Y = Scale.Y;
		TextureOffset.Z = 0.f;
		PostProcessMaterial->SetVectorParameterValue(TextureScaleName, TextureScale);
	}

	FVector2f Offset = ((InWidgetSize - InVisibleArea.AbsoluteSize) * 0.5f)
		+ InCameraOffset / InVisibleArea.GetVisibleAreaFraction();

	if (!FMath::IsNearlyEqual(WidgetAspectRatio, ViewportAspectRatio))
	{
		if (WidgetAspectRatio > ViewportAspectRatio)
		{
			Offset.X -= (Scale.Y - 1) * InVisibleArea.AbsoluteSize.X * 0.5f;
			Offset.Y -= (Scale.Y - 1) * InVisibleArea.AbsoluteSize.Y * 0.5f;
		}
		else
		{
			Offset.X -= (Scale.X - 1) * InVisibleArea.AbsoluteSize.X * 0.5f;
			Offset.Y -= (Scale.X - 1) * InVisibleArea.AbsoluteSize.Y * 0.5f;
		}
	}		

	if (!FMath::IsNearlyEqual(TextureOffset.X, Offset.X)
		|| !FMath::IsNearlyEqual(TextureOffset.Y, Offset.Y))
	{
		TextureOffset.X = Offset.X;
		TextureOffset.Y = Offset.Y;
		TextureOffset.Z = 0.f;
		PostProcessMaterial->SetVectorParameterValue(TextureOffsetName, TextureOffset);
	}
}

void FAvaViewportBackplateVisualizer::LoadPostProcessInfo(const FAvaViewportPostProcessInfo& InPostProcessInfo)
{
	Super::LoadPostProcessInfo(InPostProcessInfo);

	SetTextureInternal(InPostProcessInfo.Texture.LoadSynchronous());
}

void FAvaViewportBackplateVisualizer::UpdatePostProcessInfo(FAvaViewportPostProcessInfo& InPostProcessInfo) const
{
	Super::UpdatePostProcessInfo(InPostProcessInfo);

	InPostProcessInfo.Texture = Texture;
}

void FAvaViewportBackplateVisualizer::UpdatePostProcessMaterial()
{
	if (!PostProcessMaterial)
	{
		return;
	}

	Super::UpdatePostProcessMaterial();

	using namespace UE::AvalancheViewport::Private;

	PostProcessMaterial->SetTextureParameterValue(TextureObjectName, Texture);
}

bool FAvaViewportBackplateVisualizer::SetupPostProcessSettings(FPostProcessSettings& InPostProcessSettings) const
{
	if (!IsValid(Texture))
	{
		return false;
	}

	return FAvaViewportPostProcessVisualizer::SetupPostProcessSettings(InPostProcessSettings);
}

void FAvaViewportBackplateVisualizer::SetTextureInternal(UTexture* InTexture)
{
	if (!IsValid(InTexture))
	{
		Texture = nullptr;
	}
	else
	{
		Texture = InTexture;
	}
}

#undef LOCTEXT_NAMESPACE

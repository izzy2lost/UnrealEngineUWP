// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMRenderTargetRenderer.h"

#include "Components/MaterialValues/DMMaterialValueRenderTarget.h"
#include "Misc/CoreDelegates.h"
#include "Templates/SubclassOf.h"

UDMRenderTargetRenderer* UDMRenderTargetRenderer::CreateRenderTargetRenderer(TSubclassOf<UDMRenderTargetRenderer> InRendererClass, 
	UDMMaterialValueRenderTarget* InRenderTargetValue)
{
	UClass* Class = InRendererClass.Get();

	check(Class);
	check(!Class->HasAnyClassFlags(UE::DynamicMaterial::InvalidClassFlags));

	check(InRenderTargetValue);

	UDMRenderTargetRenderer* Renderer = NewObject<UDMRenderTargetRenderer>(InRenderTargetValue, Class, NAME_None, RF_Transactional);
	InRenderTargetValue->SetRenderer(Renderer);

	return Renderer;
}

UDMMaterialValueRenderTarget* UDMRenderTargetRenderer::GetRenderTargetValue() const
{
	return Cast<UDMMaterialValueRenderTarget>(GetOuterSafe());
}

void UDMRenderTargetRenderer::UpdateRenderTarget()
{
	if (EndOfFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(EndOfFrameDelegateHandle);
		EndOfFrameDelegateHandle.Reset();
	}

	if (bUpdating)
	{
		return;
	}

	TGuardValue<bool> Guard(bUpdating, true);

	UpdateRenderTarget_Internal();
}

void UDMRenderTargetRenderer::AsyncUpdateRenderTarget()
{
	if (bUpdating)
	{
		return;
	}

	if (!EndOfFrameDelegateHandle.IsValid())
	{
		EndOfFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UDMRenderTargetRenderer::UpdateRenderTarget);
	}
}

void UDMRenderTargetRenderer::FlushUpdateRenderTarget()
{
	if (EndOfFrameDelegateHandle.IsValid())
	{
		UpdateRenderTarget();
	}
}

void UDMRenderTargetRenderer::PostLoad()
{
	Super::PostLoad();

	if (UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue())
	{
		RenderTargetValue->EnsureRenderTarget();
	}
}

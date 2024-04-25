// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/RenderTargetRenderers/DMRenderTargetWidgetRenderer.h"
#include "Components/MaterialValues/DMMaterialValueRenderTarget.h"
#include "Components/Widget.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"

#define LOCWidget_NAMESPACE "DMRenderTargetWidgetRenderer"

UDMRenderTargetWidgetRenderer::UDMRenderTargetWidgetRenderer()
{
#if WITH_EDITOR
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMRenderTargetWidgetRenderer, WidgetClass));
#endif

	WidgetRenderer = MakeShared<FWidgetRenderer>(/* Gamma correction */ false);
	WidgetRenderer->SetIsPrepassNeeded(true);
	WidgetRenderer->SetShouldClearTarget(true);
}

void UDMRenderTargetWidgetRenderer::SetWidgetClass(TSubclassOf<UWidget> InWidgetClass)
{
	if (InWidgetClass == WidgetClass)
	{
		return;
	}

	WidgetClass = InWidgetClass;
	CreateWidgetInstance();
	AsyncUpdateRenderTarget();
}

#if WITH_EDITOR
TSharedPtr<FJsonValue> UDMRenderTargetWidgetRenderer::JsonSerialize() const
{
	return FDMJsonUtils::Serialize(WidgetClass.Get());
}

bool UDMRenderTargetWidgetRenderer::JsonDeserialize(const TSharedPtr<FJsonValue>& InJsonValue)
{
	TSubclassOf<UWidget> WidgetClassJson;

	if (FDMJsonUtils::Deserialize(InJsonValue, WidgetClassJson))
	{
		SetWidgetClass(WidgetClassJson);
		return true;
	}

	return false;
}

void UDMRenderTargetWidgetRenderer::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMRenderTargetWidgetRenderer, WidgetClass))
	{
		CreateWidgetInstance();
		AsyncUpdateRenderTarget();
	}
}
#endif

void UDMRenderTargetWidgetRenderer::CreateWidgetInstance()
{
	UClass* WidgetClassLocal = WidgetClass.Get();

	if (!WidgetClassLocal)
	{
		return;
	}

	WidgetInstance = NewObject<UWidget>(this, WidgetClassLocal, NAME_None, RF_Transient);
}

void UDMRenderTargetWidgetRenderer::UpdateRenderTarget_Internal()
{
	Super::UpdateRenderTarget_Internal();

	UDMMaterialValueRenderTarget* RenderTargetValue = GetRenderTargetValue();

	if (!RenderTargetValue)
	{
		return;
	}

	RenderTargetValue->EnsureRenderTarget(/* Async */ false);

	UTextureRenderTarget2D* RenderTarget = RenderTargetValue->GetRenderTarget();

	if (!RenderTarget)
	{
		return;
	}

	if (!WidgetInstance)
	{
		CreateWidgetInstance();

		if (!WidgetInstance)
		{
			return;
		}
	}

	WidgetRenderer->DrawWidget(
		RenderTarget,
		WidgetInstance->TakeWidget(),
		{(double)RenderTarget->SizeX, (double)RenderTarget->SizeX},
		/* Delta Time */ 0.f
	);
}

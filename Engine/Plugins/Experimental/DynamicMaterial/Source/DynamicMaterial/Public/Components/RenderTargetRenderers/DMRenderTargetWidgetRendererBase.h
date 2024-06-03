// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMRenderTargetRenderer.h"
#include "DMRenderTargetWidgetRendererBase.generated.h"

class FWidgetRenderer;
class SWidget;
class UWidget;

UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Render Target Widget Renderer"))
class DYNAMICMATERIAL_API UDMRenderTargetWidgetRendererBase : public UDMRenderTargetRenderer
{
	GENERATED_BODY()

public:
	UDMRenderTargetWidgetRendererBase();

protected:
	TSharedPtr<SWidget> Widget;
	TSharedPtr<FWidgetRenderer> WidgetRenderer;

	virtual void CreateWidgetInstance() PURE_VIRTUAL(UDMRenderTargetUMGWidgetRenderer::CreateWidgetInstance);

	//~ Begin UDMRenderTargetRenderer
	virtual void UpdateRenderTarget_Internal() override;
	//~ End UDMRenderTargetRenderer
};

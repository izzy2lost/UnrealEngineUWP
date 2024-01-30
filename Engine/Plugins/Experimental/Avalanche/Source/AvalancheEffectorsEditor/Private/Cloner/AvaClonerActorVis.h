// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaVisBase.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AAvaClonerActor;
class UAvaClonerComponent;

struct HAvaClonerActorSpacingHitProxy : HAvaHitProxy
{
	DECLARE_HIT_PROXY();

	HAvaClonerActorSpacingHitProxy(const UActorComponent* InComponent, EAvaClonerAxis InAxis)
		: HAvaHitProxy(InComponent)
		, Axis(InAxis)
	{}

	EAvaClonerAxis Axis = EAvaClonerAxis::Custom;
};

/** Custom visualization for cloner actor to handle spacing in various layouts */
class FAvaClonerActorVisualizer : public FAvaVisualizerBase
{
public:
	typedef FAvaVisualizerBase Super;
	typedef UAvaClonerComponent MeshType;

	FAvaClonerActorVisualizer();
	
	//~ Begin FAvaVisualizerBase
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TMap<UObject*, TArray<FProperty*>> GatherEditableProperties(UObject* InObject) const override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* InVisProxy, const FViewportClick& InClick) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const override;
	virtual bool GetWidgetMode(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode& OutMode) const override;
	virtual bool GetWidgetAxisList(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const override;
	virtual bool GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient, UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const override;
	virtual bool ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy) override;
	virtual bool IsEditing() const override;
	virtual void EndEditing() override;
	virtual void StoreInitialValues() override;
	virtual FBox GetComponentBounds(const UActorComponent* InComponent) const override;
	virtual bool HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation, const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale) override;
	virtual void DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex) override;
	virtual void DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex) override;
	//~ End FAvaVisualizerBase
	
	AAvaClonerActor* GetClonerActor() const
	{
		return ClonerActorWeak.Get();
	}

protected:
	FVector GetHandleSpacingLocation(const AAvaClonerActor* InClonerActor, EAvaClonerAxis InAxis) const;
	void DrawSpacingButton(const AAvaClonerActor* InClonerActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, EAvaClonerAxis InAxis, FLinearColor InColor) const;

	void OnPropertyModified(UObject* InPropertyObject, FName InPropertyName, EPropertyChangeType::Type InType = EPropertyChangeType::Interactive);
	
	TWeakObjectPtr<AAvaClonerActor> ClonerActorWeak = nullptr;
	FVector InitialSpacing = FVector::ZeroVector;
	bool bEditingSpacing = false;
	EAvaClonerAxis EditingAxis = EAvaClonerAxis::X;
};

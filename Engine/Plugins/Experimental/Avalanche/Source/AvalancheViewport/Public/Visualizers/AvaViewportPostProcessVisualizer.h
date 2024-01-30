// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "EditorUndoClient.h"
#include "Math/MathFwd.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"

class FSceneView;
class IAvaViewportClient;
class UMaterial;
class UMaterialInstanceDynamic;
struct FAvaViewportPostProcessInfo;
struct FAvaVisibleArea;
struct FPostProcessSettings;

class AVALANCHEVIEWPORT_API FAvaViewportPostProcessVisualizer : public FGCObject, public FEditorUndoClient, public IAvaTypeCastable
{
public:
	UE_AVA_INHERITS(FAvaViewportPostProcessVisualizer, IAvaTypeCastable)

	FAvaViewportPostProcessVisualizer(TSharedRef<IAvaViewportClient> InAvaViewportClient);
	virtual ~FAvaViewportPostProcessVisualizer() override;

	TSharedPtr<IAvaViewportClient> GetAvaViewportClient() const;

	float GetPostProcessOpacity() const { return PostProcessOpacity; }
	void SetPostProcessOpacity(float InOpacity);

	FAvaViewportPostProcessInfo* GetPostProcessInfo() const;

	void LoadPostProcessInfo();

	virtual bool CanActivate(bool bInSilent) const;
	virtual void OnActivate();
	virtual void OnDeactivate() {}

	virtual void UpdateForViewport(const FAvaVisibleArea& InVisibleArea, const FVector2f& InWidgetSize, const FVector2f& InCameraOffset) {}
	void ApplyToSceneView(FSceneView* InSceneView) const;

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	//~ End FGCObject

	// FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// ~FEditorUndoClient Interface

protected:
	TWeakPtr<IAvaViewportClient> AvaViewportClientWeak;

	TObjectPtr<UMaterial> PostProcessBaseMaterial;

	TObjectPtr<UMaterialInstanceDynamic> PostProcessMaterial;

	float PostProcessOpacity;

	bool bRequiresTonemapperSetting;

	void SetPostProcessOpacityInternal(float InOpacity);

	void UpdatePostProcessInfo();

	virtual void LoadPostProcessInfo(const FAvaViewportPostProcessInfo& InPostProcessInfo);
	virtual void UpdatePostProcessInfo(FAvaViewportPostProcessInfo& InPostProcessInfo) const;

	virtual void UpdatePostProcessMaterial();

	virtual bool SetupPostProcessSettings(FPostProcessSettings& InPostProcessSettings) const;
};

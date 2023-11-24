// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"

namespace EAnimationMode { enum Type : int; }

class FCanvas;
class FMaterialRenderProxy;
class FPreviewScene;
class FPrimitiveDrawInterface;
class FReferenceCollector;
class FSceneView;
class FViewport;
class ICustomizableObjectInstanceEditor;
class UAnimationAsset;
class UCustomizableObject;
class UCustomizableObjectNodeMeshClipMorph;
class UCustomizableObjectNodeMeshClipWithMesh;
class UCustomizableObjectNodeProjectorConstant;
class UCustomizableObjectNodeProjectorParameter;
class UDebugSkelMeshComponent;
class UMaterial;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UStaticMeshComponent;
struct FInputKeyEventArgs;


DECLARE_DELEGATE_RetVal(FVector, FWidgetLocationDelegate);
DECLARE_DELEGATE_OneParam(FOnWidgetLocationChangedDelegate, const FVector&)

DECLARE_DELEGATE_RetVal(FVector, FWidgetDirectionDelegate);
DECLARE_DELEGATE_OneParam(FOnWidgetDirectionChangedDelegate, const FVector&)

DECLARE_DELEGATE_RetVal(FVector, FWidgetUpDelegate);
DECLARE_DELEGATE_OneParam(FOnWidgetUpChangedDelegate, const FVector&)

DECLARE_DELEGATE_RetVal(FVector, FWidgetScaleDelegate);
DECLARE_DELEGATE_OneParam(FOnWidgetScaleChangedDelegate, const FVector&)

DECLARE_DELEGATE_RetVal(float, FWidgetAngleDelegate);

DECLARE_DELEGATE_RetVal(ECustomizableObjectProjectorType, FProjectorTypeDelegate);

DECLARE_DELEGATE_RetVal(FColor, FWidgetColorDelegate);

DECLARE_DELEGATE(FWidgetTrackingStartedDelegate);


extern void RemoveRestrictedChars(FString& String);


enum class EWidgetType : uint8
{
	Hidden,
	Projector,
	ClipMorph,
	ClipMesh,
	Light,
};


/** Viewport which shows the scene with the generated Instance. */
class FCustomizableObjectEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FCustomizableObjectEditorViewportClient>
{
public:
	FCustomizableObjectEditorViewportClient(TWeakPtr<ICustomizableObjectInstanceEditor> InCustomizableObjectEditor, FPreviewScene* InPreviewScene);
	~FCustomizableObjectEditorViewportClient();

	/** persona config options **/
	class UPersonaOptions* ConfigOption;

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputWidgetDelta(FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStarted(const FInputEventState& InInputState, bool bIsDragging, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override;
	virtual void SetViewportType(ELevelViewportType InViewportType) override;
	// End of FEditorViewportClient

	void UpdateCameraSetup();
	void UpdateFloor();

	/** */
	void SetPreviewComponent(UStaticMeshComponent* InPreviewStaticMeshComponent);
	void SetPreviewComponents(const TArray<UDebugSkelMeshComponent*>& InPreviewSkeletalMeshComponents);

	/** Sets an informative message in the viewport warning the user that the CustomizableObject has no reference mesh */
	void SetReferenceMeshMissingWarningMessage(bool bVisible);

	/**
	 *	Draws the UV overlay for the current LOD.
	 *
	 *	@param	InViewport					The viewport to draw to
	 *	@param	InCanvas					The canvas to draw to
	 *	@param	InTextYPos					The Y-position to begin drawing the UV-Overlay at (due to text displayed above it).
	 */
	void DrawUVs(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos, const FString& MaterialName);

	// Callback for baking the current instance. If nullptr is passed, the instance in the editor will be taken.
	void BakeInstance();
	void BakeInstance(class UCustomizableObjectInstance* Instance);
	
	// Returns false if the resource has already been duplicated, otherwise, returns true and an unique ResourceName.
	bool GetUniqueResourceName(UObject* InResource, FString& InOutResourceName, TArray<UObject*>& InCachedObjects, TArray<FString>& InCachedObjectNames);
	
	// If baking files that already exist, ask the user for permission to overwirte them
	bool ManageBakingAction(const FString& Path, const FString& Name);

	// Callback to show / hide instance geometry data
	void StateChangeShowGeometryData();

	/** Callback for toggling the UV overlay show flag. */
	void SetDrawUVOverlay();
	
	/** Callback for checking the UV overlay show flag. */
	bool IsSetDrawUVOverlayChecked() const;

	// Set the material index whose UVs will be drawn.
	void SetDrawUVOverlayMaterial(const FString& MaterialName, FString UVChannel);

	/** Callback for toggling the grid show flag. */
	void SetShowGrid();
	
	/** Callback for checking the grid show flag. */
	bool IsSetShowGridChecked() const;

	/** Callback for toggling the sky show flag. */
	void SetShowSky();

	/** Callback for checking the sky show flag. */
	bool IsSetShowSkyChecked() const;

	/** Callback for toggling the bounds show flag. */
	void SetShowBounds();
		
	/** Returns the desired target of the camera */
	FSphere GetCameraTarget();

	/** Point the camera to the Skeletal Mesh Components. */
	void ResetCamera();
	
	/* Returns the floor height offset */	
	float GetFloorOffset() const;

	/* Sets the floor height offset, saves it to config and invalidates the viewport so it shows up immediately */
	void SetFloorOffset(float NewValue);
	
	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void ShowGizmoClipMorph(UCustomizableObjectNodeMeshClipMorph& ClipPlainNode);

	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void HideGizmoClipMorph();
	
	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void ShowGizmoClipMesh(UCustomizableObjectNodeMeshClipWithMesh& ClipMeshNode, UStaticMesh& ClipMesh);

	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void HideGizmoClipMesh();
	
	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void ShowGizmoProjector(
		const FWidgetLocationDelegate& WidgetLocationDelegate, const FOnWidgetLocationChangedDelegate& OnWidgetLocationChangedDelegate,
		const FWidgetDirectionDelegate& WidgetDirectionDelegate, const FOnWidgetDirectionChangedDelegate& OnWidgetDirectionChangedDelegate,
		const FWidgetUpDelegate& WidgetUpDelegate, const FOnWidgetUpChangedDelegate& OnWidgetUpChangedDelegate,
		const FWidgetScaleDelegate& WidgetScaleDelegate, const FOnWidgetScaleChangedDelegate& OnWidgetScaleChangedDelegate,
		const FWidgetAngleDelegate& WidgetAngleDelegate,
		const FProjectorTypeDelegate& ProjectorTypeDelegate,
		const FWidgetColorDelegate& WidgetColorDelegate,
		const FWidgetTrackingStartedDelegate& WidgetTrackingStartedDelegate);

	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void HideGizmoProjector();
	
	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void ShowGizmoLight(ULightComponent& Light);

	/** Do not call directly. Use ICustomizableObjectEditor functions instead. */
	void HideGizmoLight();
	
	void SetVisibilityForWireframeMode(bool bIsWireframeMode);

	void SetAnimation(UAnimationAsset* Animation, EAnimationMode::Type AnimationType);

	/** Set again the animation saved in AnimationBeingPlayed (if any) */
	void ReSetAnimation();

	/** Add Light Component to the scene. */
	void AddLightToScene(ULightComponent* AddedLight);

	/** Remove Light component from the scene. */
	void RemoveLightFromScene(ULightComponent* RemovedLight);

	/** Remove all Light components from the scene. */
	void RemoveAllLightsFromScene();

	/** Setter of CustomizableObject */
	void SetCustomizableObject(UCustomizableObject* CustomizableObjectParameter);

	/** Helper method to draw shadowed string on viewport */
	void DrawShadowedString(FCanvas* Canvas, float StartX, float StartY, const FLinearColor& Color, float TextScale, FString String);

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectEditorViewportClient");
	}
	// End of FSerializableObject interface

	/** Setter of AssetRegistryLoaded */
	void SetAssetRegistryLoaded(bool Value);

	/** Show pero LOD geometric information of the instance */
	void ShowInstanceGeometryInformation(FCanvas* InCanvas);

	/** Sets up the ShowFlag according to the current preview scene profile */
	void SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags);

	/** Delegate for preview profile is changed (used for updating show flags) */
	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	/** Debug draw a partial cylinder, given by MaxAngle in [0, 2PI] */
	void DrawCylinderArc(FPrimitiveDrawInterface* PDI, const FMatrix& CylToWorld, const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, float Radius, float HalfHeight, uint32 Sides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, FColor Color, float MaxAngle);

	/** Getter of viewport's floor visibility */
	bool GetFloorVisibility();

	/** Setter of viewport's floor visibility */
	void SetFloorVisibility(bool Value);

	/** Getter of viewport's grid visibility */
	bool GetGridVisibility();

	/** Getter of viewport's environment visibility */
	bool GetEnvironmentMeshVisibility();

	/** Setter of viewport's environment visibility */
	void SetEnvironmentMeshVisibility(uint32 Value);

	/** Returns camera mode */
	bool IsOrbitalCameraActive() const; 

	/** Sets camera mode */
	void SetCameraMode(bool Value);

	/** Sets the skeletal mesh bones visibility */
	void SetShowBones();

	/** Returns true if bones are visible in viewport */
	bool IsShowingBones() const;

	const TArray<ULightComponent*>& GetLightComponents() const;
	
private:
	/** Draws Mesh Bones in foreground (From: FAnimationViewportClient) */
	void DrawMeshBones(UDebugSkelMeshComponent* MeshComponent, FPrimitiveDrawInterface* PDI);

	void SetWidgetType(EWidgetType Type);
	
	/** Component for the static/skeletal mesh. */
	TWeakObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	TArray<TWeakObjectPtr<UDebugSkelMeshComponent>> SkeletalMeshComponents;

	/** True if the widget is being dragged. */
	bool bManipulating = false;

	/** Pointer back to the editor tool that owns us */
	TWeakPtr<ICustomizableObjectInstanceEditor> CustomizableObjectEditorPtr;

	/** Flags for various options in the editor. */
	bool bDrawUVs;
	bool bDrawSky;

	// Material index to draw in the uv preview.
	FString MaterialToDrawInUVs;
	int32 MaterialToDrawInUVsLOD;
	int32 MaterialToDrawInUVsIndex;
	int32 UVChannelToDrawInUVs;
	int32 MaterialToDrawInUVsComponent;

	bool bReferenceMeshMissingWarningMessageVisible;

	UCustomizableObjectNodeMeshClipMorph* ClipMorphNode;
	TObjectPtr<UMaterial> ClipMorphMaterial;
	bool bClipMorphLocalStartOffset;
	FVector ClipMorphOrigin;
	FVector ClipMorphOffset;
	FVector ClipMorphLocalOffset;
	FVector ClipMorphNormal;
	FVector ClipMorphXAxis;
	FVector ClipMorphYAxis;
	float MorphLength;
	FBoxSphereBounds MorphBounds;
	UCustomizableObjectNodeMeshClipWithMesh* ClipMeshNode;
	UStaticMeshComponent* ClipMeshComp;
	float Radius1;
	float Radius2;
	float RotationAngle;

	FSphere BoundSphere;

	/** Light being edited */
	ULightComponent* SelectedLightComponent;

	/** Spawned light components. */
	TArray<ULightComponent*> LightComponents;
	
	/** To know if an animation ia being played by the Customizable Object and restre it after compilation */
	bool IsPlayingAnimation;

	/** Animation being played by the Customizable Object, if any */
	TObjectPtr<UAnimationAsset> AnimationBeingPlayed;

	/** Customizable object being used */
	UCustomizableObject* CustomizableObject;

	/** Flag to control whether to show / hide the instance geometry information data */
	bool StateChangeShowGeometryDataFlag;

	/** To know if the user has given permission to overwrite files when baking if already present */
	bool BakingOverwritePermission;

	/** Flag to know when the asset registry initial loading has completed, value set by the Customizable Object / Customizable Object Instance editor */
	bool AssetRegistryLoaded;

	/** To know if the orbital camera is being used or not */
	bool bActivateOrbitalCamera;

	/** Material for cylinder arc solid render */
	TObjectPtr<UMaterialInterface> TransparentPlaneMaterialXY;

	/** bool to return the Camera mode to Orbital when changing the Camera view to Perspective */
	bool bSetOrbitalOnPerspectiveMode;

	/** Flag to control the bones visibility in the viewport */
	bool bShowBones;

	// Temp Instance used in the bake process if a new instance is needed because mutable texture streaming is enabled so the viewport 
	// instance does not have the high quality mips in the texture's platform data
	TObjectPtr<UCustomizableObjectInstance> BakeTempInstance = nullptr;

	// The following delegates currently are only used by the EWidgetType::Projector
	FWidgetLocationDelegate WidgetLocationDelegate;
	FOnWidgetLocationChangedDelegate OnWidgetLocationChangedDelegate;

	FWidgetLocationDelegate WidgetDirectionDelegate;
	FOnWidgetLocationChangedDelegate OnWidgetDirectionChangedDelegate;

	FWidgetLocationDelegate WidgetUpDelegate;
	FOnWidgetLocationChangedDelegate OnWidgetUpChangedDelegate;

	FWidgetLocationDelegate WidgetScaleDelegate;
	FOnWidgetLocationChangedDelegate OnWidgetScaleChangedDelegate;

	FWidgetAngleDelegate WidgetAngleDelegate; 

	FProjectorTypeDelegate ProjectorTypeDelegate;

	FWidgetColorDelegate WidgetColorDelegate;

	FWidgetTrackingStartedDelegate WidgetTrackingStartedDelegate;
	
	EWidgetType WidgetType = EWidgetType::Hidden;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdTypes.h"
#endif

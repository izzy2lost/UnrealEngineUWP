// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorViewportClient.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "AssetViewerSettings.h"
#include "CanvasTypes.h"
#include "Components/SphereReflectionCaptureComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentBrowserModule.h"
#include "DynamicMeshBuilder.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Editor/UnrealEdTypes.h"
#include "EditorModeManager.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "InputKeyEventArgs.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuCOE/UnrealBakeHelpers.h"
#include "MuCOE/CustomizableObjectPreviewScene.h"
#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuT/UnrealPixelFormatOverride.h"
#include "ObjectTools.h"
#include "Preferences/PersonaOptions.h"
#include "ScopedTransaction.h"
#include "SkeletalDebugRendering.h"
#include "UnrealWidget.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Materials/MaterialInstanceConstant.h"

class FMaterialRenderProxy;
class UFont;
class UMaterialExpression;
class UTextureMipDataProviderFactory;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor" 


FCustomizableObjectEditorViewportClient::FCustomizableObjectEditorViewportClient(TWeakPtr<ICustomizableObjectInstanceEditor> InCustomizableObjectEditor, FPreviewScene* InPreviewScene)
	: FEditorViewportClient(&GLevelEditorModeTools(), InPreviewScene)
	, CustomizableObjectEditorPtr(InCustomizableObjectEditor)
	, CustomizableObject(nullptr)
	, BakingOverwritePermission(false)
{
	// load config
	ConfigOption = UPersonaOptions::StaticClass()->GetDefaultObject<UPersonaOptions>();
	check (ConfigOption);

	bUsingOrbitCamera = true;

	bDrawUVs = false;
	Widget->SetDefaultVisibility(false);
	bCameraLock = true;
	bDrawSky = true;

	bActivateOrbitalCamera = true;
	bSetOrbitalOnPerspectiveMode = true;

	const int32 CameraSpeed = 3;
	SetCameraSpeedSetting(CameraSpeed);

	bShowBones = false;

	MaterialToDrawInUVs = 0;
	MaterialToDrawInUVsLOD = 0;
	MaterialToDrawInUVsIndex = 0;
	UVChannelToDrawInUVs = 0;
	MaterialToDrawInUVsComponent = 0;


	bReferenceMeshMissingWarningMessageVisible = false;

	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(160, 160, 160);
	DrawHelper.GridColorMajor = FColor(144, 144, 144);
	DrawHelper.GridColorMinor = FColor(128, 128, 128);
	DrawHelper.PerspectiveGridSize = 2048.0f;
	DrawHelper.NumCells = DrawHelper.PerspectiveGridSize / (32);
	SetShowGrid();

	SetViewMode(VMI_Lit);

	EngineShowFlags.SetSeparateTranslucency(true);
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetCompositeEditorPrimitives(true);

	EngineShowFlags.ScreenSpaceReflections = 1;
	EngineShowFlags.AmbientOcclusion = 1;
	EngineShowFlags.Grid = ConfigOption->bShowGrid;

	OverrideNearClipPlane(1.0f);

	SetPreviewComponent(nullptr);

	// add capture component for reflection
	USphereReflectionCaptureComponent* CaptureComponent = NewObject<USphereReflectionCaptureComponent>();

	const FTransform CaptureTransform(FRotator(0, 0, 0), FVector(0.f, 0.f, 100.f), FVector(1.f));
	PreviewScene->AddComponent(CaptureComponent, CaptureTransform);
	CaptureComponent->UpdateReflectionCaptureContents(PreviewScene->GetWorld());

	// now add the ClipMorph plane
	ClipMorphNode = nullptr;
	bClipMorphLocalStartOffset = true;
	ClipMorphMaterial = LoadObject<UMaterial>(NULL, TEXT("Material'/Engine/EditorMaterials/LevelGridMaterial.LevelGridMaterial'"), NULL, LOAD_None, NULL);
	check(ClipMorphMaterial);

	// clip mesh preview
	ClipMeshNode = nullptr;
	ClipMeshComp = NewObject<UStaticMeshComponent>();
	PreviewScene->AddComponent(ClipMeshComp, FTransform());
	ClipMeshComp->SetVisibility(false);

	BoundSphere.W = 100.f;

	const float FOVMin = 5.f;
	const float FOVMax = 170.f;
	ViewFOV = FMath::Clamp<float>(53.43f, FOVMin, FOVMax);

	SetRealtime(true);
	if (GEditor->PlayWorld)
	{
		AddRealtimeOverride(false, LOCTEXT("RealtimeOverrideMessage_InstanceViewport", "Instance Viewport")); // We are PIE, don't start in realtime mode
	}

	IsPlayingAnimation = false;
	AnimationBeingPlayed = nullptr;

	// Lighting 
	SelectedLightComponent = nullptr;
	
	StateChangeShowGeometryDataFlag = false;

	// Register delegate to update the show flags when the post processing is turned on or off
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &FCustomizableObjectEditorViewportClient::OnAssetViewerSettingsChanged);
	// Set correct flags according to current profile settings
	SetAdvancedShowFlagsForScene(UAssetViewerSettings::Get()->Profiles[GetMutableDefault<UEditorPerProjectUserSettings>()->AssetViewerProfileIndex].bPostProcessingEnabled);

	// Set profile so changes in scene lighting affect and match this editor too
	UEditorPerProjectUserSettings* PerProjectSettings = GetMutableDefault<UEditorPerProjectUserSettings>();
	UAssetViewerSettings* DefaultSettings = UAssetViewerSettings::Get();
	PerProjectSettings->AssetViewerProfileIndex = DefaultSettings->Profiles.IsValidIndex(PerProjectSettings->AssetViewerProfileIndex) ? PerProjectSettings->AssetViewerProfileIndex : 0;
	int32 ProfileIndex = PerProjectSettings->AssetViewerProfileIndex;
	FAdvancedPreviewScene* PreviewSceneCasted = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	PreviewSceneCasted->SetProfileIndex(ProfileIndex);

	TransparentPlaneMaterialXY = (UMaterial*)StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial.WidgetVertexColorMaterial"), NULL, LOAD_None, NULL);
}


void FCustomizableObjectEditorViewportClient::UpdateCameraSetup()
{
	static FRotator CustomOrbitRotation(-33.75, -135, 0);
	if ( (SkeletalMeshComponents.Num() && SkeletalMeshComponents[0].IsValid() && UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponents[0]))
		||
		(StaticMeshComponent.IsValid() && StaticMeshComponent->GetStaticMesh()) )
	{
		BoundSphere = GetCameraTarget();
		FVector CustomOrbitZoom(0, BoundSphere.W / (75.0f * (float)PI / 360.0f), 0);
		FVector CustomOrbitLookAt = BoundSphere.Center;

		SetCameraSetup(CustomOrbitLookAt, CustomOrbitRotation, CustomOrbitZoom, CustomOrbitLookAt, GetViewLocation(), GetViewRotation() );

		UpdateFloor();

		EnableCameraLock(bActivateOrbitalCamera);
		FBox Box( BoundSphere.Center - FVector(BoundSphere.W) / 2.0f, BoundSphere.Center + FVector(BoundSphere.W) / 2.0f );
		FocusViewportOnBox( Box, false );
	}
}


void FCustomizableObjectEditorViewportClient::UpdateFloor()
{
	// Move the floor to the bottom of the bounding box of the mesh, rather than on the origin
	bool bFoundSkelMesh = false;

	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent.IsValid())
		{
			SkeletalMeshComponent->bComponentUseFixedSkelBounds = true;
			SkeletalMeshComponent->UpdateBounds();

			bFoundSkelMesh = true;
		}
	}

	// TODO: Optimize
	if (bFoundSkelMesh)
	{

	}
	else if (StaticMeshComponent.IsValid())
	{
		StaticMeshComponent->UpdateBounds();
	}

	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	if (AdvancedScene != nullptr)
	{
		const UStaticMeshComponent* FloorMeshComponent = AdvancedScene->GetFloorMeshComponent();
		if (FloorMeshComponent != nullptr)
		{
			UStaticMeshComponent* FloorMeshComponentCasted = const_cast<UStaticMeshComponent*>(FloorMeshComponent);
			FloorMeshComponentCasted->SetWorldLocation(FVector(0.0f, 0.0f, -1.0f/* Does not seem to work Bottom.Z*/));
		}
	}
}


FCustomizableObjectEditorViewportClient::~FCustomizableObjectEditorViewportClient()
{
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().RemoveAll(this);
}


void FCustomizableObjectEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	UpdateFloor();
}


void DrawEllipse(FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FLinearColor& Color, float Radius1, float Radius2, int32 NumSides, uint8 DepthPriority, float Thickness, float DepthBias, bool bScreenSpace)
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius1;

	for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
	{
		const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) * Radius1 + Y * FMath::Sin(AngleDelta * (SideIndex + 1)) * Radius2);
		PDI->DrawLine(LastVertex, Vertex, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
		LastVertex = Vertex;
	}
}


void FCustomizableObjectEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	
	switch (WidgetType)
	{
	case EWidgetType::Light:
		{
			check(SelectedLightComponent);

			if (USpotLightComponent* SpotLightComp = Cast<USpotLightComponent>(SelectedLightComponent))
			{
				FTransform TransformNoScale = SpotLightComp->GetComponentToWorld();
				TransformNoScale.RemoveScaling();

				// Draw point light source shape
				DrawWireCapsule(PDI, TransformNoScale.GetTranslation(), -TransformNoScale.GetUnitAxis(EAxis::Z), TransformNoScale.GetUnitAxis(EAxis::Y), TransformNoScale.GetUnitAxis(EAxis::X),
					FColor(231, 239, 0, 255), SpotLightComp->SourceRadius, 0.5f * SpotLightComp->SourceLength + SpotLightComp->SourceRadius, 25, SDPG_World);

				// Draw outer light cone
				DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->OuterConeAngle, 32, 8, 10, FColor(200, 255, 255), SDPG_World);

				// Draw inner light cone (if non zero)
				if (SpotLightComp->InnerConeAngle > UE_KINDA_SMALL_NUMBER)
				{
					DrawWireSphereCappedCone(PDI, TransformNoScale, SpotLightComp->AttenuationRadius, SpotLightComp->InnerConeAngle, 32, 8, 10, FColor(150, 200, 255), SDPG_World);
				}
			}
			else if (UPointLightComponent* PointLightComp = Cast<UPointLightComponent>(SelectedLightComponent))
			{
				FTransform LightTM = PointLightComp->GetComponentToWorld();

				// Draw light radius
				DrawWireSphereAutoSides(PDI, FTransform(LightTM.GetTranslation()), FColor(200, 255, 255), PointLightComp->AttenuationRadius, SDPG_World);

				// Draw point light source shape
				DrawWireCapsule(PDI, LightTM.GetTranslation(), -LightTM.GetUnitAxis(EAxis::Z), LightTM.GetUnitAxis(EAxis::Y), LightTM.GetUnitAxis(EAxis::X),
					FColor(231, 239, 0, 255), PointLightComp->SourceRadius, 0.5f * PointLightComp->SourceLength + PointLightComp->SourceRadius, 25, SDPG_World);
			}
			
			break;
		}
	case EWidgetType::ClipMorph:
		{
			float MaxSphereRadius = 0.f;

			for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
			{
				if (SkeletalMeshComponent.IsValid())
				{
					MaxSphereRadius = FMath::Max(MaxSphereRadius, SkeletalMeshComponent->Bounds.SphereRadius);
				}
			}

			if (MaxSphereRadius <= 0.f)
			{
				MaxSphereRadius = 1.f;
			}

			float PlaneRadius1 = MaxSphereRadius * 0.1f;
			float PlaneRadius2 = PlaneRadius1 * 0.5f;

			FMatrix PlaneMatrix = FMatrix(ClipMorphNormal, ClipMorphYAxis, ClipMorphXAxis, ClipMorphOrigin + ClipMorphOffset);

			// Start Plane
			DrawDirectionalArrow(PDI, PlaneMatrix, FColor::Red, MorphLength, MorphLength * 0.1f, 0, 0.1f);
			DrawBox(PDI, PlaneMatrix, FVector(0.01f, PlaneRadius1, PlaneRadius1), ClipMorphMaterial->GetRenderProxy(), 0);

			// End Plane + Ellipse
			PlaneMatrix.SetOrigin(ClipMorphOrigin + ClipMorphOffset + ClipMorphNormal * MorphLength);
			DrawBox(PDI, PlaneMatrix, FVector(0.01f, PlaneRadius2, PlaneRadius2), ClipMorphMaterial->GetRenderProxy(), 0);
			DrawEllipse(PDI, ClipMorphOrigin + ClipMorphOffset + ClipMorphNormal * MorphLength, ClipMorphXAxis, ClipMorphYAxis, FColor::Red, Radius1, Radius2, 15, 1, 0.f, 0, false);
			
			break;
		}

	case EWidgetType::Projector:
		{
			const FColor Color = WidgetColorDelegate.IsBound() ?
			WidgetColorDelegate.Execute() :
			FColor::Green;

			const ECustomizableObjectProjectorType ProjectorType = ProjectorTypeDelegate.IsBound() ?
				ProjectorTypeDelegate.Execute() :
				ECustomizableObjectProjectorType::Planar;

			const FVector WidgetScale = WidgetScaleDelegate.IsBound() ?
				WidgetScaleDelegate.Execute() :
				FVector::OneVector;

			const float CylindricalAngle = WidgetAngleDelegate.IsBound() ? 
				FMath::DegreesToRadians<float>(WidgetAngleDelegate.Execute()) :
				0.0f;

			const FVector CorrectedWidgetScale = FVector(WidgetScale.Z, WidgetScale.X, WidgetScale.Y);

			switch (ProjectorType)
			{
				case ECustomizableObjectProjectorType::Planar:
				{
					FVector Min = FVector(0.f, -0.5f, -0.5f);
					FVector Max = FVector(1.0f, 0.5f, 0.5f);
					FBox Box = FBox(Min * CorrectedWidgetScale, Max * CorrectedWidgetScale);
					FMatrix Mat = GetWidgetCoordSystem();
					Mat.SetOrigin(GetWidgetLocation());
					DrawWireBox(PDI, Mat, Box, Color, 1, 0.f);
					break;
				}
				case ECustomizableObjectProjectorType::Cylindrical:
				{
					// Draw the cylinder
					FMatrix Mat = GetWidgetCoordSystem();
					FVector Location = GetWidgetLocation();
					Mat.SetOrigin(Location);
					FVector TransformedX = Mat.TransformVector(FVector(1, 0, 0));
					FVector TransformedY = Mat.TransformVector(FVector(0, 1, 0));
					FVector TransformedZ = Mat.TransformVector(FVector(0, 0, 1));

					FVector Min = FVector(0.f, -0.5f, -0.5f);
					FVector Max = FVector(1.0f, 0.5f, 0.5f);
					FBox Box = FBox(Min * CorrectedWidgetScale, Max * CorrectedWidgetScale);
					FVector BoxExtent = Box.GetExtent();
					float CylinderHalfHeight = BoxExtent.X;
					//float CylinderRadius = (BoxExtent.Y + BoxExtent.Z) * 0.5f;
					float CylinderRadius = FMath::Abs(BoxExtent.Y);

					DrawWireCylinder(PDI, Location + TransformedX * CylinderHalfHeight, TransformedY, TransformedZ, TransformedX, Color, CylinderRadius, CylinderHalfHeight, 16, SDPG_World, 0.1f, 0, false);

					// Draw the arcs: the locations are Location with an offset towards the local forward direction
					FVector Location0 = Location - TransformedX * CylinderHalfHeight * 0.8f + TransformedX * CylinderHalfHeight;
					FVector Location1 = Location + TransformedX * CylinderHalfHeight * 0.8f + TransformedX * CylinderHalfHeight;
					FMatrix Mat0 = Mat;
					FMatrix Mat1 = Mat;
					Mat0.SetOrigin(Location0);
					Mat1.SetOrigin(Location1);
					DrawCylinderArc(PDI, Mat0, FVector(0.0f, 0.0f, 0.0f), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0),  CylinderRadius, CylinderHalfHeight * 0.1f, 16, TransparentPlaneMaterialXY->GetRenderProxy(), SDPG_World, FColor(255, 85, 0, 192), CylindricalAngle);
					DrawCylinderArc(PDI, Mat1, FVector(0.0f, 0.0f, 0.0f), FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), CylinderRadius, CylinderHalfHeight * 0.1f, 16, TransparentPlaneMaterialXY->GetRenderProxy(), SDPG_World, FColor(255, 85, 0, 192), CylindricalAngle);
					break;
				}
				case ECustomizableObjectProjectorType::Wrapping:
		        {
		            FVector Min = FVector(0.f, -0.5f, -0.5f);
		            FVector Max = FVector(1.0f, 0.5f, 0.5f);
		            FBox Box = FBox(Min * CorrectedWidgetScale, Max * CorrectedWidgetScale);
		            FMatrix Mat = GetWidgetCoordSystem();
		            Mat.SetOrigin(GetWidgetLocation());
		            DrawWireBox(PDI, Mat, Box, Color, 1, 0.f);
					break;
				}
				default:
				{
					check(false);
					break;
				}
			}
			break;
		}

	case EWidgetType::ClipMesh:
	case EWidgetType::Hidden:
		break;

	default:
		unimplemented(); // Case not implemented	
	}
	

	if (bShowBones)
	{
		for (TWeakObjectPtr<UDebugSkelMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (SkeletalMeshComponent.IsValid())
			{
				DrawMeshBones(SkeletalMeshComponent.Get(), PDI);
			}
		}
	}
}


void FCustomizableObjectEditorViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	// Defensive check to avoid unreal crashing inside render if the mesh is degenereated
	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent.IsValid() && UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent) && UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)->GetLODInfoArray().Num() == 0)
		{
			SkeletalMeshComponent->SetSkeletalMesh(nullptr);
		}
	}

	FEditorViewportClient::Draw(InViewport, Canvas);

	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( InViewport, GetScene(), EngineShowFlags ));

	if(bDrawUVs)
	{
		constexpr int32 YPos = 24;
		DrawUVs(InViewport, Canvas, YPos, MaterialToDrawInUVs);
	}

	if (bReferenceMeshMissingWarningMessageVisible)
	{
		Canvas->DrawShadowedString(
			6,
			2,
			*NSLOCTEXT("CustomizableObjectEditor", "NoReferenceMeshMutable", "Warning! No reference mesh is set in the Object Properties tab.").ToString(),
			GEngine->GetSmallFont(),
			FLinearColor::Red
			);
	}

	if (StateChangeShowGeometryDataFlag)
	{
		ShowInstanceGeometryInformation(Canvas);
	}
}

namespace
{
	template<typename Real>
	FVector2D ClampUVRange(Real U, Real V)
	{
		return FVector2D( FMath::Wrap(U, Real(0), Real(1)), FMath::Wrap(V, Real(0), Real(1)) );
	}
}

void FCustomizableObjectEditorViewportClient::DrawUVs(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos, const FString& MaterialName )
{
	//use the overriden LOD level
	// TODO
	const uint32 LODLevel = MaterialToDrawInUVsLOD; //FMath::Clamp(StaticMeshComponent->ForcedLodModel - 1, 0, StaticMesh->RenderData->LODResources.Num() - 1);

	// TODO
	int32 UVChannel = UVChannelToDrawInUVs; //StaticMeshEditorPtr.Pin()->GetCurrentUVChannel();

	const uint32 ComponentIndex = MaterialToDrawInUVsComponent;

	//draw a string showing what UV channel and LOD is being displayed
	InCanvas->DrawShadowedString( 
		6,
		InTextYPos,
		*FText::Format( NSLOCTEXT("CustomizableObjectEditor", "UVOverlay_F", "Showing UV channel {0} for LOD {1}"), FText::AsNumber(UVChannel), FText::AsNumber(LODLevel) ).ToString(),
		GEngine->GetSmallFont(),
		FLinearColor::White
		);
	InTextYPos += 18;

	//calculate scaling
	const uint32 BorderWidth = 5;
	const uint32 MinY = InTextYPos + BorderWidth;
	const uint32 MinX = BorderWidth;
	const FVector2D UVBoxOrigin(MinX, MinY);
	const FVector2D BoxOrigin( MinX - 1, MinY - 1 );
	const uint32 UVBoxScale = FMath::Min(InViewport->GetSizeXY().X - MinX, InViewport->GetSizeXY().Y - MinY) - BorderWidth;
	const uint32 BoxSize = UVBoxScale + 2;
	const FVector2D Box[ 4 ] = {
		BoxOrigin,									// topleft
		BoxOrigin + FVector2D( BoxSize, 0 ),		// topright
		BoxOrigin + FVector2D( BoxSize, BoxSize ),	// bottomright
		BoxOrigin + FVector2D( 0, BoxSize ),		// bottomleft
	};

	const FVector Color(1.0f, 1.0f, 1.0f);

	//draw texture border
	FLinearColor BorderColor = FLinearColor::White;
	FBatchedElements* BatchedElements = InCanvas->GetBatchedElements(FCanvas::ET_Line);
	FHitProxyId HitProxyId = InCanvas->GetHitProxyId();

	// Reserve line vertices (4 border lines, then up to the maximum number of graph lines)
	BatchedElements->AddReserveLines( 4 );

	// Left
	BatchedElements->AddLine( FVector( Box[ 0 ], 0.0f ), FVector( Box[ 1 ], 0.0f ), BorderColor, HitProxyId );
	BatchedElements->AddLine( FVector( Box[ 1 ], 0.0f ), FVector( Box[ 2 ], 0.0f ), BorderColor, HitProxyId );
	BatchedElements->AddLine( FVector( Box[ 2 ], 0.0f ), FVector( Box[ 3 ], 0.0f ), BorderColor, HitProxyId );
	BatchedElements->AddLine( FVector( Box[ 3 ], 0.0f ), FVector( Box[ 0 ], 0.0f ), BorderColor, HitProxyId );

	if ( StaticMeshComponent.IsValid() && StaticMeshComponent->GetStaticMesh() && StaticMeshComponent->GetStaticMesh()->GetRenderData() )
	{
		FStaticMeshLODResources* RenderData = &StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[LODLevel];
		
		if( RenderData && ( ( uint32 )UVChannel < RenderData->VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() ) )
		{
			//draw triangles
			FIndexArrayView Indices = RenderData->IndexBuffer.GetArrayView();
			uint32 NumIndices = Indices.Num();
		
			BatchedElements->AddReserveLines( NumIndices );

			for (uint32 i = 0; i < NumIndices - 2; i += 3)
			{
				FVector2D UV1( RenderData->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( Indices[ i + 0 ], UVChannel ) );
				FVector2D UV2( RenderData->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( Indices[ i + 1 ], UVChannel ) );
				FVector2D UV3( RenderData->VertexBuffers.StaticMeshVertexBuffer.GetVertexUV( Indices[ i + 2 ], UVChannel ) );
	
				// Draw lines in black unless the UVs are outside of the 0.0 - 1.0 range.  For out-of-bounds
				// UVs, we'll draw the line segment in red
				
				// If we are supporting a version lower than LWC get the right real type. 
				using Vector2DRealType = TDecay<decltype( DeclVal<FVector2D>().X )>::Type; 

				constexpr Vector2DRealType Epsilon = static_cast<Vector2DRealType>(1e-4);
				constexpr Vector2DRealType One     = static_cast<Vector2DRealType>(1);
				constexpr Vector2DRealType Zero    = static_cast<Vector2DRealType>(0);

				UV1 = ClampUVRange(UV1.X, UV1.Y) * UVBoxScale + UVBoxOrigin;
				UV2 = ClampUVRange(UV2.X, UV2.Y) * UVBoxScale + UVBoxOrigin;
				UV3 = ClampUVRange(UV3.X, UV3.Y) * UVBoxScale + UVBoxOrigin;

				BatchedElements->AddLine(FVector(UV1, Zero), FVector(UV2, Zero), BorderColor, HitProxyId);
				BatchedElements->AddLine(FVector(UV2, Zero), FVector(UV3, Zero), BorderColor, HitProxyId);
				BatchedElements->AddLine(FVector(UV3, Zero), FVector(UV1, Zero), BorderColor, HitProxyId);
			}
		}
	}

	else if (SkeletalMeshComponents.Num())
	{
		int32 CurrentComponentIndex = 0;

		for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
		{
			if (!SkeletalMeshComponent.IsValid() || !UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent) || CurrentComponentIndex != ComponentIndex)
			{
				CurrentComponentIndex++;
				continue;
			}

			bool bFoundMaterial = false;

			const FSkeletalMeshRenderData* MeshRes = UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)->GetResourceForRendering();
			if (UVChannel < (int32)MeshRes->LODRenderData[LODLevel].GetNumTexCoords())
			{
				// Find material index from name
				const FSkeletalMeshLODRenderData& lodModel = MeshRes->LODRenderData[LODLevel];
				int MaterialIndex = 0;
				int MaterialIndexCount = 0;
				{
					for (int s = 0; s < lodModel.RenderSections.Num(); ++s)
					{
						int SectionMaterial = lodModel.RenderSections[s].MaterialIndex;
						UMaterialInterface* Material = SkeletalMeshComponent->GetMaterial(SectionMaterial);

						if (!Material)
						{
							continue;
						}

						const UMaterial* BaseMaterial = Material->GetBaseMaterial();
						if (BaseMaterial && BaseMaterial->GetName() == MaterialName)
						{
							MaterialIndex = s;

							if (MaterialIndexCount == MaterialToDrawInUVsIndex)
							{
								bFoundMaterial = true;
								break;
							}
							MaterialIndexCount++;
						}
					}
				}

				if (!bFoundMaterial)
				{
					continue;
				}

				const FStaticMeshVertexBuffer& Vertices = lodModel.StaticVertexBuffers.StaticMeshVertexBuffer;

				TArray<uint32> Indices;
				lodModel.MultiSizeIndexContainer.GetIndexBuffer(Indices);

				uint32 NumTriangles = lodModel.RenderSections[MaterialIndex].NumTriangles;
				int IndexIndex = lodModel.RenderSections[MaterialIndex].BaseIndex;

				BatchedElements->AddReserveLines(NumTriangles * 3);

				for (uint32 FaceIndex = 0
					; FaceIndex < NumTriangles
					; ++FaceIndex, IndexIndex += 3)
				{
					FVector2D UV1(Vertices.GetVertexUV(Indices[IndexIndex + 0], UVChannel));
					FVector2D UV2(Vertices.GetVertexUV(Indices[IndexIndex + 1], UVChannel));
					FVector2D UV3(Vertices.GetVertexUV(Indices[IndexIndex + 2], UVChannel));

					// Draw lines in black unless the UVs are outside of the 0.0 - 1.0 range.  For out-of-bounds
					// UVs, we'll draw the line segment in red

					// If we are supporting a version lower than LWC get the right real type. 
					using Vector2DRealType = TDecay<decltype(DeclVal<FVector2D>().X)>::Type;

					constexpr Vector2DRealType Epsilon = static_cast<Vector2DRealType>(1e-4);
					constexpr Vector2DRealType One = static_cast<Vector2DRealType>(1);
					constexpr Vector2DRealType Zero = static_cast<Vector2DRealType>(0);

					UV1 = ClampUVRange(UV1.X, UV1.Y) * UVBoxScale + UVBoxOrigin;
					UV2 = ClampUVRange(UV2.X, UV2.Y) * UVBoxScale + UVBoxOrigin;
					UV3 = ClampUVRange(UV3.X, UV3.Y) * UVBoxScale + UVBoxOrigin;

					BatchedElements->AddLine( FVector(UV1, Zero), FVector(UV2, Zero), BorderColor, HitProxyId );
					BatchedElements->AddLine( FVector(UV2, Zero), FVector(UV3, Zero), BorderColor, HitProxyId );
					BatchedElements->AddLine( FVector(UV3, Zero), FVector(UV1, Zero), BorderColor, HitProxyId );
				}
			}

			if (bFoundMaterial)
			{
				break;
			}
		}
	}
}


float FCustomizableObjectEditorViewportClient::GetFloorOffset() const
{
	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	if (AdvancedScene != nullptr)
	{
		const UStaticMeshComponent* FloorMeshComponent = AdvancedScene->GetFloorMeshComponent();
		if (FloorMeshComponent != nullptr)
		{
			return FloorMeshComponent->GetComponentLocation().Z;
		}
	}

	return 0.0f;
}


void FCustomizableObjectEditorViewportClient::ShowGizmoClipMorph(UCustomizableObjectNodeMeshClipMorph& NodeMeshClipMorph)
{
	SetWidgetType(EWidgetType::ClipMorph);

	if (ClipMorphNode != &NodeMeshClipMorph || NodeMeshClipMorph.bUpdateViewportWidget)
	{	
		NodeMeshClipMorph.bUpdateViewportWidget = false;

		bClipMorphLocalStartOffset = NodeMeshClipMorph.bLocalStartOffset;
		MorphLength = NodeMeshClipMorph.B;
		Radius1 = NodeMeshClipMorph.Radius;
		Radius2 = NodeMeshClipMorph.Radius2;
		RotationAngle = NodeMeshClipMorph.RotationAngle;
		ClipMorphOrigin = NodeMeshClipMorph.Origin;
		ClipMorphLocalOffset = NodeMeshClipMorph.StartOffset;

		NodeMeshClipMorph.FindLocalAxes(ClipMorphXAxis, ClipMorphYAxis, ClipMorphNormal);

		if (bClipMorphLocalStartOffset)
		{
			ClipMorphOffset = ClipMorphLocalOffset.X * ClipMorphXAxis
				+ ClipMorphLocalOffset.Y * ClipMorphYAxis
				+ ClipMorphLocalOffset.Z * ClipMorphNormal;
		}
		else
		{
			ClipMorphOffset = ClipMorphLocalOffset;
		}
	}

	ClipMorphNode = &NodeMeshClipMorph;
}


void FCustomizableObjectEditorViewportClient::HideGizmoClipMorph()
{
	if (WidgetType == EWidgetType::ClipMorph)
	{
		SetWidgetType(EWidgetType::Hidden);
	}
}


void FCustomizableObjectEditorViewportClient::ShowGizmoClipMesh(UCustomizableObjectNodeMeshClipWithMesh& InClipMeshNode, UStaticMesh& ClipMesh)
{
	SetWidgetType(EWidgetType::ClipMesh);

	ClipMeshNode = &InClipMeshNode;

	ClipMeshComp->SetStaticMesh(&ClipMesh);
	ClipMeshComp->SetVisibility(true);
	ClipMeshComp->SetWorldTransform(InClipMeshNode.Transform);
}


void FCustomizableObjectEditorViewportClient::HideGizmoClipMesh()
{
	if (WidgetType == EWidgetType::ClipMesh)
	{
		SetWidgetType(EWidgetType::Hidden);
	}	
}


void FCustomizableObjectEditorViewportClient::ShowGizmoProjector(
	const FWidgetLocationDelegate& InWidgetLocationDelegate,
	const FOnWidgetLocationChangedDelegate& InOnWidgetLocationChangedDelegate,
	const FWidgetDirectionDelegate& InWidgetDirectionDelegate,
	const FOnWidgetDirectionChangedDelegate& InOnWidgetDirectionChangedDelegate,
	const FWidgetUpDelegate& InWidgetUpDelegate, const FOnWidgetUpChangedDelegate& InOnWidgetUpChangedDelegate,
	const FWidgetScaleDelegate& InWidgetScaleDelegate, const FOnWidgetScaleChangedDelegate& InOnWidgetScaleChangedDelegate,
	const FWidgetAngleDelegate& InWidgetAngleDelegate, const FProjectorTypeDelegate& InProjectorTypeDelegate,
	const FWidgetColorDelegate& InWidgetColorDelegate,
	const FWidgetTrackingStartedDelegate& InWidgetTrackingStartedDelegate)
{
	SetWidgetType(EWidgetType::Projector);

	WidgetLocationDelegate = InWidgetLocationDelegate;
	OnWidgetLocationChangedDelegate = InOnWidgetLocationChangedDelegate;
	WidgetDirectionDelegate = InWidgetDirectionDelegate;
	OnWidgetDirectionChangedDelegate = InOnWidgetDirectionChangedDelegate;
	WidgetUpDelegate = InWidgetUpDelegate;
	OnWidgetUpChangedDelegate = InOnWidgetUpChangedDelegate;
	WidgetScaleDelegate = InWidgetScaleDelegate;
	OnWidgetScaleChangedDelegate = InOnWidgetScaleChangedDelegate;
	WidgetAngleDelegate = InWidgetAngleDelegate;
	ProjectorTypeDelegate = InProjectorTypeDelegate;
	WidgetColorDelegate = InWidgetColorDelegate;
	WidgetTrackingStartedDelegate = InWidgetTrackingStartedDelegate;
}


void FCustomizableObjectEditorViewportClient::HideGizmoProjector()
{
	if (WidgetType == EWidgetType::Projector)
	{
		SetWidgetType(EWidgetType::Hidden);
	}
}


void FCustomizableObjectEditorViewportClient::ShowGizmoLight(ULightComponent& Light)
{
	SelectedLightComponent = &Light;
	
	SetWidgetType(EWidgetType::Light);
}


void FCustomizableObjectEditorViewportClient::HideGizmoLight()
{
	if (WidgetType == EWidgetType::Light)
	{
		SetWidgetType(EWidgetType::Hidden);
	}
}


void FCustomizableObjectEditorViewportClient::SetVisibilityForWireframeMode(bool bIsWireframeMode)
{
	EngineShowFlags.SetDirectLighting(!bIsWireframeMode && bDrawSky);
}


FSphere FCustomizableObjectEditorViewportClient::GetCameraTarget()
{
	bool bFoundTarget = false;
	FSphere Sphere(FVector(0,0,0), 100.0f); // default

	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent.IsValid())
		{
			FTransform ComponentToWorld = SkeletalMeshComponent->GetComponentTransform();
			SkeletalMeshComponent.Get()->CalcBounds(ComponentToWorld);

			FBoxSphereBounds Bounds = SkeletalMeshComponent.Get()->CalcBounds(FTransform::Identity);

			if (!bFoundTarget)
			{
				Sphere = Bounds.GetSphere();
			}
			else
			{
				Sphere += Bounds.GetSphere();
			}

			bFoundTarget = true;
		}
	}

	if(!bFoundTarget && StaticMeshComponent.IsValid())
	{
		FTransform ComponentToWorld = StaticMeshComponent->GetComponentTransform();
		StaticMeshComponent.Get()->CalcBounds(ComponentToWorld);

		if( !bFoundTarget )
		{
			FBoxSphereBounds Bounds = StaticMeshComponent.Get()->CalcBounds(FTransform::Identity);
			Sphere = Bounds.GetSphere();
		}
	}

	return Sphere;
}


FLinearColor FCustomizableObjectEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FColor(55, 55, 55);

	return BackgroundColor;
}


void FCustomizableObjectEditorViewportClient::SetPreviewComponent(UStaticMeshComponent* InStaticMeshComponent)
{
	StaticMeshComponent = InStaticMeshComponent;
	SkeletalMeshComponents.Reset();

	if (StaticMeshComponent.IsValid() && StaticMeshComponent->GetStaticMesh())
	{
		SetViewLocation( -FVector(0, StaticMeshComponent->GetStaticMesh()->GetBounds().SphereRadius / (75.0f * (float)PI / 360.0f), 0) );
		SetViewRotation( FRotator(0, 90.f, 0) );
		//LockLocation = FVector(0,StaticMeshComponent->StaticMesh->ThumbnailDistance,0);
		//LockRot = StaticMeshComponent->StaticMesh->ThumbnailAngle;
	}

	UpdateCameraSetup();
}


void FCustomizableObjectEditorViewportClient::SetPreviewComponents(const TArray<UDebugSkelMeshComponent*>& InSkeletalMeshComponents)
{
	SkeletalMeshComponents.Reset(InSkeletalMeshComponents.Num());
	
	for (UDebugSkelMeshComponent* SkeletalMeshComponent : InSkeletalMeshComponents)
	{
		SkeletalMeshComponents.Add(SkeletalMeshComponent);

	}

	StaticMeshComponent = nullptr;
}

void FCustomizableObjectEditorViewportClient::ResetCamera()
{
	float MaxSphereRadius = 0.0f;
	for (const TWeakObjectPtr<UDebugSkelMeshComponent>& SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent))
		{
			MaxSphereRadius = FMath::Max(MaxSphereRadius, UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)->GetBounds().SphereRadius);
		}
	}
	
	SetViewLocation(-FVector(0, MaxSphereRadius / (75.0f * (float)PI / 360.0f), 0));
	SetViewRotation(FRotator(0, 90.f, 0));

	UpdateCameraSetup();
}


void FCustomizableObjectEditorViewportClient::SetReferenceMeshMissingWarningMessage(bool bVisible)
{
	bReferenceMeshMissingWarningMessageVisible = bVisible;
}


void FCustomizableObjectEditorViewportClient::SetDrawUVOverlay()
{
	bDrawUVs = !bDrawUVs;
	Invalidate();
}


void FCustomizableObjectEditorViewportClient::SetDrawUVOverlayMaterial(const FString& MaterialName, FString UVChannel)
{
	// Get LOD Index
	FString NameWithLOD, ComponentString;
	MaterialName.Split(FString("_Component_"), &NameWithLOD, &ComponentString);
	check(ComponentString.IsNumeric());
	MaterialToDrawInUVsComponent = FCString::Atoi(*ComponentString);

	FString Name, LODIndex;
	bool bSplit = NameWithLOD.Split(FString(" LOD_"), &Name, &LODIndex);
	check(bSplit && LODIndex.IsNumeric());

	MaterialToDrawInUVsLOD = FCString::Atoi(*LODIndex);
	UVChannelToDrawInUVs = FCString::Atoi(*UVChannel);

	// Get Material Index, added if the name of the material already exists within the skeletal mesh.
	FString DuplicatedMaterialIndex;
	bSplit = Name.Split(FString("__"), &MaterialToDrawInUVs, &DuplicatedMaterialIndex);

	if (bSplit && DuplicatedMaterialIndex.IsNumeric())
	{
		MaterialToDrawInUVsIndex = FCString::Atoi(*DuplicatedMaterialIndex);
	}
	else
	{
		MaterialToDrawInUVs = Name;
		MaterialToDrawInUVsIndex = 0;
	}

	Invalidate();
}

bool FCustomizableObjectEditorViewportClient::IsSetDrawUVOverlayChecked() const
{
	return bDrawUVs;
}

void FCustomizableObjectEditorViewportClient::SetShowGrid()
{
	DrawHelper.bDrawGrid = !DrawHelper.bDrawGrid;

	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	if (AdvancedScene != nullptr)
	{
		AdvancedScene->SetFloorVisibility(DrawHelper.bDrawGrid,true);
	}

	EngineShowFlags.Grid = DrawHelper.bDrawGrid;

	Invalidate();
}

bool FCustomizableObjectEditorViewportClient::IsSetShowGridChecked() const
{
	return DrawHelper.bDrawGrid;
}

void FCustomizableObjectEditorViewportClient::SetShowSky()
{
	bDrawSky = !bDrawSky;
	FAdvancedPreviewScene* PreviewSceneCasted = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	PreviewSceneCasted->SetEnvironmentVisibility(bDrawSky, true);
	Invalidate();
}

bool FCustomizableObjectEditorViewportClient::IsSetShowSkyChecked() const
{
	return bDrawSky;
}

void FCustomizableObjectEditorViewportClient::SetShowBounds()
{
	EngineShowFlags.Bounds = 1 - EngineShowFlags.Bounds;
	Invalidate();
}


bool FCustomizableObjectEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	const bool bMouseButtonDown = EventArgs.Viewport->KeyState(EKeys::LeftMouseButton) || EventArgs.Viewport->KeyState(EKeys::MiddleMouseButton) || EventArgs.Viewport->KeyState(EKeys::RightMouseButton);

	if (EventArgs.Event == IE_Pressed && !bMouseButtonDown)
	{
		if (EventArgs.Key == EKeys::F)
		{
			UpdateCameraSetup();
			return true;
		}
		else if (WidgetType != EWidgetType::Hidden) // Do not change the type when hidden.
		{
			if (EventArgs.Key == EKeys::W)
			{
				SetWidgetMode(UE::Widget::WM_Translate);
				return true;
			}
			else if (EventArgs.Key == EKeys::E)
			{
				SetWidgetMode(UE::Widget::WM_Rotate);
				return true;
			}
			else if (EventArgs.Key == EKeys::R)
			{
				SetWidgetMode(UE::Widget::WM_Scale);
				return true;
			}	
		}
		else if (EventArgs.Key == EKeys::Q) // Not sure why, pressing Q the super class hides the widget.
		{
			SetWidgetType(EWidgetType::Hidden);
			return true;
		}
	}

	// Pass keys to standard controls, if we didn't consume input
	return FEditorViewportClient::InputKey(EventArgs);
}


bool FCustomizableObjectEditorViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	if (CurrentAxis == EAxisList::None)
	{
		return false;
	}

	const UE::Widget::EWidgetMode WidgetMode = GetWidgetMode();
	
	switch (WidgetType)
	{
	case EWidgetType::Projector:
		{
			if (WidgetLocationDelegate.IsBound() && OnWidgetLocationChangedDelegate.IsBound())
			{
				if (Drag != FVector::ZeroVector)
				{
					OnWidgetLocationChangedDelegate.Execute(WidgetLocationDelegate.Execute() + Drag);				
				}
			}

			if (WidgetDirectionDelegate.IsBound() && OnWidgetDirectionChangedDelegate.IsBound())
			{
				const FVector WidgetDirection = WidgetDirectionDelegate.Execute();
				const FVector NewWidgetDirection = Rot.RotateVector(WidgetDirection);

				if (WidgetDirection != NewWidgetDirection)
				{
					OnWidgetDirectionChangedDelegate.Execute(NewWidgetDirection);				
				}
			}

			if (WidgetUpDelegate.IsBound() && OnWidgetUpChangedDelegate.IsBound())
			{
				const FVector WidgetUp = WidgetUpDelegate.Execute();
				const FVector NewWidgetUp = Rot.RotateVector(WidgetUp);

				if (WidgetUp != NewWidgetUp)
				{
					OnWidgetUpChangedDelegate.Execute(NewWidgetUp);				
				}
			}

			if (WidgetScaleDelegate.IsBound() && OnWidgetScaleChangedDelegate.IsBound())
			{
				const FVector CorrectedScale(Scale.Y, Scale.Z, Scale.X);
				if (CorrectedScale != FVector::ZeroVector)
				{
					OnWidgetScaleChangedDelegate.Execute(WidgetScaleDelegate.Execute() + CorrectedScale);
				}
			}
		
			return true;
		}
		
	case EWidgetType::ClipMorph:
		{
			if (WidgetMode == UE::Widget::WM_Translate)
			{
				if (CurrentAxis == EAxisList::Screen) // true when selecting the widget center
				{
					CurrentAxis = EAxisList::XYZ;
				}
				
				if (CurrentAxis & EAxisList::Z)
				{
					const float dragZ = bClipMorphLocalStartOffset ? FVector::DotProduct(Drag, ClipMorphNormal) : Drag.Z;
					ClipMorphLocalOffset.Z += dragZ;
					ClipMorphOffset += (bClipMorphLocalStartOffset) ? dragZ * ClipMorphNormal : FVector(0,0,dragZ);
				}

				if(CurrentAxis & EAxisList::X)
				{
					const float dragX = bClipMorphLocalStartOffset ? FVector::DotProduct(Drag, ClipMorphXAxis) : Drag.X;
					ClipMorphLocalOffset.X += dragX;
					ClipMorphOffset += (bClipMorphLocalStartOffset) ? dragX * ClipMorphXAxis : FVector(dragX, 0, 0);
				}

				if (CurrentAxis & EAxisList::Y)
				{
					const float dragY = bClipMorphLocalStartOffset ? FVector::DotProduct(Drag, ClipMorphYAxis) : Drag.Y;
					ClipMorphLocalOffset.Y += dragY;
					ClipMorphOffset += (bClipMorphLocalStartOffset) ? dragY * ClipMorphYAxis : FVector(0, dragY, 0);
				}
				
				ClipMorphNode->StartOffset = ClipMorphLocalOffset;
			}
			else if (WidgetMode == UE::Widget::WM_Rotate)
			{
				bool bClipMorphViewPortRotation = false;

				if (CurrentAxis == EAxisList::X)
				{
					bClipMorphViewPortRotation = true;
					float Angle = ClipMorphNode->bInvertNormal ? Rot.GetComponentForAxis(EAxis::X) : -Rot.GetComponentForAxis(EAxis::X);
					ClipMorphNormal = ClipMorphNormal.RotateAngleAxis(Angle, ClipMorphXAxis);
				}
				else if (CurrentAxis == EAxisList::Y)
				{
					bClipMorphViewPortRotation = true;
					float Angle = Rot.GetComponentForAxis(EAxis::Y);
					ClipMorphNormal = ClipMorphNormal.RotateAngleAxis(Angle, ClipMorphYAxis);
				}

				if (bClipMorphViewPortRotation)
				{
					ClipMorphNormal.Normalize();
					ClipMorphNode->Normal = ClipMorphNormal;
					ClipMorphNode->FindLocalAxes(ClipMorphXAxis, ClipMorphYAxis, ClipMorphNormal);

					if (bClipMorphLocalStartOffset)
					{
						ClipMorphLocalOffset.Z = FVector::DotProduct(ClipMorphOffset, ClipMorphNormal);
						ClipMorphLocalOffset.Y = FVector::DotProduct(ClipMorphOffset, ClipMorphYAxis);
						ClipMorphLocalOffset.X = FVector::DotProduct(ClipMorphOffset, ClipMorphXAxis);
					}

					ClipMorphNode->StartOffset = ClipMorphLocalOffset;
				}
			}

			return true;
		}
	case EWidgetType::ClipMesh:
		{
			if (WidgetMode == UE::Widget::WM_Translate)
			{
				ClipMeshNode->Transform.AddToTranslation(Drag);
			}
			else if (WidgetMode == UE::Widget::WM_Rotate)
			{
				ClipMeshNode->Transform.ConcatenateRotation(Rot.Quaternion());
			}
			if (WidgetMode == UE::Widget::WM_Scale)
			{
				ClipMeshNode->Transform.SetScale3D(ClipMeshNode->Transform.GetScale3D() + Scale);
			}

			ClipMeshComp->SetWorldTransform(ClipMeshNode->Transform);

			return true;
		}
		
	case EWidgetType::Light:
		{
			if (WidgetMode == UE::Widget::WM_Translate)
			{
				SelectedLightComponent->AddWorldOffset(Drag);
				SelectedLightComponent->MarkForNeededEndOfFrameRecreate();
			}
			else if (WidgetMode == UE::Widget::WM_Rotate)
			{
				SelectedLightComponent->AddWorldRotation(Rot.Quaternion());
				SelectedLightComponent->MarkForNeededEndOfFrameRecreate();
			}

			return true;
		}
		
	case EWidgetType::Hidden:
		{
			return false;
		}
		
	default:
		{
			unimplemented()
			return false;
		}
	}	
}


void FCustomizableObjectEditorViewportClient::TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge)
{
	if (!bIsDraggingWidget || !InInputState.IsLeftMouseButtonPressed() || (Widget->GetCurrentAxis() & EAxisList::All) == 0)
	{
		return;
	}
	
	(void)HandleBeginTransform();
}


void FCustomizableObjectEditorViewportClient::TrackingStopped()
{
	(void)HandleEndTransform();
}

bool FCustomizableObjectEditorViewportClient::BeginTransform(const FGizmoState& InState)
{
	return HandleBeginTransform();
}

bool FCustomizableObjectEditorViewportClient::EndTransform(const FGizmoState& InState)
{
	return HandleEndTransform();
}

bool FCustomizableObjectEditorViewportClient::HandleBeginTransform()
{
	switch (WidgetType)
	{
	case EWidgetType::Projector:
		{
			bManipulating = true;

			const UE::Widget::EWidgetMode WidgetMode = GetWidgetMode();
				
			if (WidgetMode == UE::Widget::WM_Translate)
			{
				GEditor->BeginTransaction(LOCTEXT("CustomizableObjectEditor_TranslateProjector", "Translate Projector"));
			}
			else if (WidgetMode == UE::Widget::WM_Rotate)
			{
				GEditor->BeginTransaction(LOCTEXT("CustomizableObjectEditor_RotateProjector", "Rotate Projector"));
			}
			else if (WidgetMode == UE::Widget::WM_Scale)
			{
				GEditor->BeginTransaction(LOCTEXT("CustomizableObjectEditor_ScaleProjector", "Scale Projector"));
			}

			(void)WidgetTrackingStartedDelegate.ExecuteIfBound();
			return true;
		}

	// The following cases are missing Undo/Redo functionality MTBL-391.
	case EWidgetType::ClipMorph:
	case EWidgetType::ClipMesh:
	case EWidgetType::Light:
	case EWidgetType::Hidden:
		return true;
		break;
	default:
		unimplemented();
	}
	return false;
}

bool FCustomizableObjectEditorViewportClient::HandleEndTransform()
{
	switch (WidgetType)
	{
	case EWidgetType::Projector:
		if (bManipulating)
		{
			bManipulating = false;
			GEditor->EndTransaction();
			return true;
		}

	case EWidgetType::Hidden:
	case EWidgetType::ClipMorph:
	case EWidgetType::ClipMesh:
	case EWidgetType::Light:
		return true;
		break;
	default:
		unimplemented();
	}
	return false;
}

FVector FCustomizableObjectEditorViewportClient::GetWidgetLocation() const
{
	switch (WidgetType)
	{
	case EWidgetType::Projector:
		return WidgetLocationDelegate.IsBound() ?WidgetLocationDelegate.Execute() : FVector::ZeroVector;
		
	case EWidgetType::ClipMorph:
		return ClipMorphOrigin + ClipMorphOffset;

	case EWidgetType::ClipMesh:
		return ClipMeshNode->Transform.GetTranslation();

	case EWidgetType::Light:
		return SelectedLightComponent->GetComponentLocation();

	case EWidgetType::Hidden:
		return FVector::ZeroVector;

	default:
		unimplemented()
		return FVector::ZeroVector;
	}
}


FMatrix FCustomizableObjectEditorViewportClient::GetWidgetCoordSystem() const
{
	switch (WidgetType)
	{
	case EWidgetType::Projector:
		{
			const FVector WidgetDirection = WidgetDirectionDelegate.IsBound() ?
				WidgetDirectionDelegate.Execute() :
				FVector::ForwardVector;

			const FVector WidgetUp = WidgetUpDelegate.IsBound() ?
				WidgetUpDelegate.Execute() :
				FVector::UpVector;
	
			const FVector YVector = FVector::CrossProduct(WidgetDirection, WidgetUp);
			return FMatrix(WidgetDirection, YVector, WidgetUp, FVector::ZeroVector);
		}		
	case EWidgetType::ClipMorph:
		{
			if (bClipMorphLocalStartOffset)
			{
				return FMatrix(-ClipMorphXAxis, -ClipMorphYAxis, -ClipMorphNormal, FVector::ZeroVector);
			}
			else
			{			
				return FMatrix(FVector(1, 0, 0), FVector(0,1,0), FVector(0,0,1), FVector::ZeroVector);
			}
		}
		
	case EWidgetType::ClipMesh:
		{
			return ClipMeshNode->Transform.ToMatrixNoScale().RemoveTranslation();			
		}
		
	case EWidgetType::Light:
		{
			FMatrix Rotation = SelectedLightComponent->GetComponentTransform().ToMatrixNoScale();
			Rotation.SetOrigin(FVector::ZeroVector);
			return Rotation;
		}
		
	case EWidgetType::Hidden:
		{
			return FMatrix::Identity;
		}

	default:
		{
			unimplemented()
			return FMatrix::Identity;
		}
	}
}


ECoordSystem FCustomizableObjectEditorViewportClient::GetWidgetCoordSystemSpace() const
{
	return ModeTools->GetCoordSystem();
}


void FCustomizableObjectEditorViewportClient::SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem)
{
	ModeTools->SetCoordSystem(NewCoordSystem);
	Invalidate();
}

void FCustomizableObjectEditorViewportClient::SetViewportType(ELevelViewportType InViewportType)
{
	// Getting camera mode on perspective view
	if (ViewportType == ELevelViewportType::LVT_Perspective)
	{
		bSetOrbitalOnPerspectiveMode = bActivateOrbitalCamera;
	}

	// Set Camera mode
	if (InViewportType == ELevelViewportType::LVT_Perspective || ViewportType == ELevelViewportType::LVT_Perspective)
	{
		if (InViewportType == ELevelViewportType::LVT_Perspective)
		{
			SetCameraMode(bSetOrbitalOnPerspectiveMode);
		}
		else
		{
			SetCameraMode(false);
		}
	}

	// Set Camera view
	FEditorViewportClient::SetViewportType(InViewportType);
}



bool FCustomizableObjectEditorViewportClient::CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const
{
	return true;
}


void FCustomizableObjectEditorViewportClient::SetAnimation(UAnimationAsset* Animation, EAnimationMode::Type AnimationType)
{
	bool bFoundComponent = false;

	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent.IsValid() && Animation != nullptr
			&& UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent) != nullptr
			&& UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)->GetSkeleton() == Animation->GetSkeleton()
			)
		{
			SetRealtime(true);
			IsPlayingAnimation = true;
			AnimationBeingPlayed = Animation;

			if (UPoseAsset* PoseAsset = Cast<UPoseAsset>(Animation))
			{
				SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				SkeletalMeshComponent->InitAnim(false);
				SkeletalMeshComponent->SetAnimation(PoseAsset);

				UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(SkeletalMeshComponent->GetAnimInstance());
				if (SingleNodeInstance)
				{
					TArray<FName> ArrayPoseNames = PoseAsset->GetPoseFNames();
					for (int32 i = 0; i < ArrayPoseNames.Num(); ++i)
					{
						SingleNodeInstance->SetPreviewCurveOverride(ArrayPoseNames[i], 1.0f, false);
					}
				}
			}
			else
			{
				SkeletalMeshComponent->SetAnimationMode(AnimationType);
				SkeletalMeshComponent->PlayAnimation(Animation, true);
				SkeletalMeshComponent->SetPlayRate(1.f);
			}

			bFoundComponent = true;
		}
	}
	
	if(!bFoundComponent)
	{
		IsPlayingAnimation = false;
		AnimationBeingPlayed = nullptr;
	}
}


void FCustomizableObjectEditorViewportClient::ReSetAnimation()
{
	if ((IsPlayingAnimation == true) && (AnimationBeingPlayed != nullptr))
	{
		if (Cast<UPoseAsset>(AnimationBeingPlayed))
		{
			SetAnimation(AnimationBeingPlayed, EAnimationMode::AnimationBlueprint);
		}
		else
		{
			SetAnimation(AnimationBeingPlayed, EAnimationMode::AnimationSingleNode);
		}
	}
}


void FCustomizableObjectEditorViewportClient::AddLightToScene(ULightComponent* AddedLight)
{
	if (!AddedLight)
	{
		return;
	}

	LightComponents.Add(AddedLight);
	PreviewScene->AddComponent(AddedLight, AddedLight->GetComponentTransform());
}


void FCustomizableObjectEditorViewportClient::RemoveLightFromScene(ULightComponent* RemovedLight)
{
	if (!RemovedLight)
	{
		return;
	}

	LightComponents.Remove(RemovedLight);
	PreviewScene->RemoveComponent(RemovedLight);
}


void FCustomizableObjectEditorViewportClient::RemoveAllLightsFromScene()
{
	for (ULightComponent* Light : LightComponents)
	{
		PreviewScene->RemoveComponent(Light);
	}

	LightComponents.Empty();
}


void FCustomizableObjectEditorViewportClient::SetFloorOffset(float NewValue)
{
	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		USkeletalMesh* Mesh = SkeletalMeshComponent.IsValid() ? Cast<USkeletalMesh>(UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)) : nullptr;

		if (Mesh)
		{
			// This value is saved in a UPROPERTY for the mesh, so changes are transactional
			FScopedTransaction Transaction(LOCTEXT("SetFloorOffset", "Set Floor Offset"));
			Mesh->Modify();

			Mesh->SetFloorOffset(NewValue);
			UpdateCameraSetup(); // This does the actual moving of the floor mesh
			Invalidate();
		}
	}
}


//-------------------------------------------------------------------------------------------------

class SMutableSelectFolderDlg : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMutableSelectFolderDlg)
	{
	}

	SLATE_ARGUMENT(FText, DefaultAssetPath)
	SLATE_ARGUMENT(FText, DefaultFileName)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

public:
	/** Displays the dialog in a blocking fashion */
	EAppReturnType::Type ShowModal();

	/** Gets the resulting asset path */
	FString GetAssetPath();

	/** FileName getter */
	FString GetFileName();

	bool GetExportAllResources();
	bool GetGenerateConstantMaterialInstances();

protected:
	void OnPathChange(const FString& NewPath);
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	void OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo);
	void OnBoolParameterChanged(ECheckBoxState InCheckboxState);
	void OnConstantMaterialInstancesBoolParameterChanged(ECheckBoxState InCheckboxState);

	EAppReturnType::Type UserResponse = EAppReturnType::Cancel; 
	FText AssetPath;
	FText FileName;
	bool bExportAllResources = false;
	bool bGenerateConstantMaterialInstances = false;
};


//-------------------------------------------------------------------------------------------------

void RemoveRestrictedChars(FString& String)
{
	// Remove restricted chars, according to FPaths::ValidatePath, RestrictedChars = "/?:&\\*\"<>|%#@^ ";

	String = String.Replace(TEXT("/"), TEXT(""));
	String = String.Replace(TEXT("?"), TEXT(""));
	String = String.Replace(TEXT(":"), TEXT(""));
	String = String.Replace(TEXT("&"), TEXT(""));
	String = String.Replace(TEXT("\\"), TEXT(""));
	String = String.Replace(TEXT("*"), TEXT(""));
	String = String.Replace(TEXT("\""), TEXT(""));
	String = String.Replace(TEXT("<"), TEXT(""));
	String = String.Replace(TEXT(">"), TEXT(""));
	String = String.Replace(TEXT("|"), TEXT(""));
	String = String.Replace(TEXT("%"), TEXT(""));
	String = String.Replace(TEXT("#"), TEXT(""));
	String = String.Replace(TEXT("@"), TEXT(""));
	String = String.Replace(TEXT("^"), TEXT(""));
	String = String.Replace(TEXT(" "), TEXT(""));
}


//-------------------------------------------------------------------------------------------------
void FCustomizableObjectEditorViewportClient::BakeInstance()
{
	BakeInstance(nullptr);
}


//-------------------------------------------------------------------------------------------------
void FCustomizableObjectEditorViewportClient::BakeInstance(UCustomizableObjectInstance* InInstance)
{
	if (CustomizableObject->GetPrivate()->Status.Get() == FCustomizableObjectStatus::EState::Loading)
	{
		FNotificationInfo Info(NSLOCTEXT("CustomizableObjectEditor", "CustomizableObjectCompileTryLater", "Please wait unitl Customizable Object is loaded"));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	UCustomizableObjectInstance* Instance = InInstance ? InInstance : CustomizableObjectEditorPtr.Pin()->GetPreviewInstance();

	UCustomizableObject* CustomizableObjectFromInstance = Instance ? Instance->GetCustomizableObject() : nullptr;

	if (!CustomizableObjectFromInstance || CustomizableObjectFromInstance->IsLocked())
	{
		FNotificationInfo Info(NSLOCTEXT("CustomizableObjectEditor", "CustomizableObjectCompilingTryLater", "Please wait until the Customizable Object is compiled"));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	bool bHasSkeletalMesh = false;
	int32 NumComponents = Instance->SkeletalMeshes.Num();

	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		if (Instance->SkeletalMeshes.IsValidIndex(ComponentIndex) && Instance->SkeletalMeshes[ComponentIndex])
		{
			bHasSkeletalMesh = true;
		}
	}
		
	if (!bHasSkeletalMesh)
	{
		return;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	check(System);

	if (!InInstance)
	{
		// The instance in the editor viewport does not have high quality mips in the platform data because streaming is enabled.
		// Disable streaming and retry with a newly generated temp instance.
		System->SetProgressiveMipStreamingEnabled(false);
		// Disable requested LOD generation as it will prevent the new instance from having all the LODs
		System->SetOnlyGenerateRequestedLODsEnabled(false);
		// Force high quality texture compression for this instance
		PrepareUnrealCompression();
		System->SetImagePixelFormatOverride(UnrealPixelFormatFunc);

		BakeTempInstance = Instance->Clone();
		BakeTempInstance->SkeletalMeshes.Empty();
		BakeTempInstance->UpdatedNativeDelegate.AddSP(this, &FCustomizableObjectEditorViewportClient::BakeInstance);
		BakeTempInstance->UpdateSkeletalMeshAsync(true, true);

		return;
	}

	// Warn if the CO looks like it wasn't compiled with high-quality texture compression.
	// \TODO: This is a weak test, we should store the setting with the compiled data and check that instead.
	UCustomizableObject* CO = Instance->GetCustomizableObject();
	if (!CO)
	{
		// Something is very wrong
		return;
	}

	if (CO->CompileOptions.TextureCompression!=ECustomizableObjectTextureCompression::HighQuality)
	{
		FNotificationInfo Info(NSLOCTEXT("CustomizableObjectEditor", "CustomizableObjectBakeLowQuality", "The Customizable Object wasn't compiled with high quality textures. For the best baking results, change the Texture Compression setting and recompile it."));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 6.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	FString ObjectName = CO->GetName();
	FText DefaultFileName = FText::Format(LOCTEXT("DefaultFileNameForBakeInstance", "{0}"), FText::AsCultureInvariant(ObjectName));
	bool bExportAllResources = false;
	bool bGenerateConstantMaterialInstances = false;

	TSharedRef<SMutableSelectFolderDlg> FolderDlg =
		SNew(SMutableSelectFolderDlg)
		.DefaultAssetPath(FText())
		.DefaultFileName(DefaultFileName);

	if (FolderDlg->ShowModal() != EAppReturnType::Cancel)
	{
		// Make sure we can create the asset without conflicts
		//IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		//if (!AssetTools.CanCreateAsset(ObjName, PkgName, LOCTEXT("BakeMutableInstance", "Baking a Mutable instance")))
		//{
		//	return;
		//}

		FString FileName = FolderDlg->GetFileName();
		ObjectName = FileName;

		TCHAR InvalidCharacter = '0';
		FString InvalidCharacters = FPaths::GetInvalidFileSystemChars();

		for (int32 i = 0; i < InvalidCharacters.Len(); ++i)
		{
			TCHAR Char = InvalidCharacters[i];
			FString SearchedChar = FString::Chr(Char);
			if (ObjectName.Contains(SearchedChar))
			{
				InvalidCharacter = InvalidCharacters[i];
				break;
			}
		}

		bExportAllResources = FolderDlg->GetExportAllResources();
		bGenerateConstantMaterialInstances = FolderDlg->GetGenerateConstantMaterialInstances();

		BakingOverwritePermission = false;
		FString CustomObjectPath = CO->GetPathName();
		FString AssetPath = FolderDlg->GetAssetPath();
		FString FullAssetPath = AssetPath + FString("/") + ObjectName + FString(".") + ObjectName;

		if (InvalidCharacter != '0')
		{
			FText ErrorString = FText::FromString(FString::Chr(InvalidCharacter));
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("FCustomizableObjectEditorViewportClient_BakeInstance_InvalidCharacter", "The selected contains an invalid character ({0})."), ErrorString));
		}
		else if (CustomObjectPath == FullAssetPath)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FCustomizableObjectEditorViewportClient_BakeInstance_OverwriteCO", "The selected path would overwrite the instance's parent Customizable Object."));
		}
		else
		{
			TArray<UPackage*> PackagesToSave;

			for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
			{
				USkeletalMesh* Mesh = Instance->SkeletalMeshes.IsValidIndex(ComponentIndex) && Instance->SkeletalMeshes[ComponentIndex] ?
					Cast<USkeletalMesh>(Instance->SkeletalMeshes[ComponentIndex]) : nullptr;
				
				if (!Mesh)
				{
					continue;
				}

				if (NumComponents > 1)
				{
					ObjectName = FileName + "_Component_" + FString::FromInt(ComponentIndex);
				}

				TMap<UObject*, UObject*> ReplacementMap;
				TArray<FString> ArrayCachedElement;
				TArray<UObject*> ArrayCachedObject;

				if (bExportAllResources)
				{
					UMaterialInstance* Inst;
					UMaterial* Material;
					UTexture* Texture;
					FString MaterialName;
					FString ResourceName;
					FString PackageName;
					UObject* DuplicatedObject;
					TArray<TMap<int, UTexture*>> TextureReplacementMaps;

					// Duplicate Mutable generated textures
					for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
					{
						UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
						Material = Interface->GetMaterial();
						MaterialName = Material ? Material->GetName() : "Material";
						Inst = Cast<UMaterialInstance>(Mesh->GetMaterials()[m].MaterialInterface);

						TMap<int, UTexture*> ReplacementTextures;
						TextureReplacementMaps.Add(ReplacementTextures);

						// The material will only have Mutable generated textures if it's actually a UMaterialInstance
						if (Material != nullptr && Inst != nullptr)
						{
							TArray<FName> ParameterNames = FUnrealBakeHelpers::GetTextureParameterNames(Material);

							for (int32 i = 0; i < ParameterNames.Num(); i++)
							{
								if (Inst->GetTextureParameterValue(ParameterNames[i], Texture))
								{
									UTexture2D* SrcTex = Cast<UTexture2D>(Texture);
									if (!SrcTex) continue;

									bool bIsMutableTexture = false;

									for (UAssetUserData* UserData : *SrcTex->GetAssetUserDataArray())
									{
										UTextureMipDataProviderFactory* CustomMipDataProviderFactory = Cast<UMutableTextureMipDataProviderFactory>(UserData);
										if (CustomMipDataProviderFactory)
										{
											bIsMutableTexture = true;
										}
									}

									if ((SrcTex->GetPlatformData() != nullptr) &&
										(SrcTex->GetPlatformData()->Mips.Num() > 0) &&
										bIsMutableTexture)
									{
										FString ParameterSanitized = ParameterNames[i].GetPlainNameString();
										RemoveRestrictedChars(ParameterSanitized);
										ResourceName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

										if (!GetUniqueResourceName(SrcTex, ResourceName, ArrayCachedObject, ArrayCachedElement))
										{
											continue;
										}

										if (!ManageBakingAction(AssetPath, ResourceName))
										{
											return;
										}

										// Recover original name of the texture parameter value, now substituted by the generated Mutable texture
										UTexture* OriginalTexture = nullptr;
										UMaterialInstanceDynamic* InstDynamic = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterials()[m].MaterialInterface);
										if (InstDynamic != nullptr)
										{
											InstDynamic->Parent->GetTextureParameterValue(FName(*ParameterNames[i].GetPlainNameString()), OriginalTexture);
										}
										else
										{
											UMaterialInstanceConstant* InstConstant = Cast<UMaterialInstanceConstant>(Mesh->GetMaterials()[m].MaterialInterface);

											if (InstConstant != nullptr)
											{
												InstConstant->Parent->GetTextureParameterValue(FName(*ParameterNames[i].GetPlainNameString()), OriginalTexture);
											}
										}

										PackageName = FolderDlg->GetAssetPath() + FString("/") + ResourceName;
										TMap<UObject*, UObject*> FakeReplacementMap;
										UTexture2D* DupTex = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SrcTex, ResourceName, PackageName, OriginalTexture, true, FakeReplacementMap, BakingOverwritePermission);
										ArrayCachedElement.Add(ResourceName);
										ArrayCachedObject.Add(DupTex);
										PackagesToSave.Add(DupTex->GetPackage());

										if (OriginalTexture != nullptr)
										{
											TextureReplacementMaps[m].Add(i, DupTex);
										}
									}
								}
							}
						}
					}

					// Duplicate non-Mutable material textures
					for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
					{
						UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
						Material = Interface->GetMaterial();
						MaterialName = Material ? Material->GetName() : "Material";

						if (Material != nullptr)
						{
							TArray<FName> ParameterNames = FUnrealBakeHelpers::GetTextureParameterNames(Material);

							for (int32 i = 0; i < ParameterNames.Num(); i++)
							{
								TArray<FMaterialParameterInfo> InfoArray;
								TArray<FGuid> GuidArray;
								Material->GetAllTextureParameterInfo(InfoArray, GuidArray);
								
								if (Material->GetTextureParameterValue(InfoArray[i], Texture))
								{
									FString ParameterSanitized = ParameterNames[i].GetPlainNameString();
									RemoveRestrictedChars(ParameterSanitized);
									ResourceName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

									if (ArrayCachedElement.Find(ResourceName) == INDEX_NONE)
									{
										if (!ManageBakingAction(AssetPath, ResourceName))
										{
											return;
										}

										PackageName = FolderDlg->GetAssetPath() + FString("/") + ResourceName;
										TMap<UObject*, UObject*> FakeReplacementMap;
										DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Texture, ResourceName, PackageName, true, FakeReplacementMap, BakingOverwritePermission, false);
										ArrayCachedElement.Add(ResourceName);
										ArrayCachedObject.Add(DuplicatedObject);
										PackagesToSave.Add(DuplicatedObject->GetPackage());

										UTexture* DupTexture = Cast<UTexture>(DuplicatedObject);
										TextureReplacementMaps[m].Add(i, DupTexture);
									}
								}
							}
						}
					}


					// Duplicate the materials used by each material instance so that the replacement map has proper information 
					// when duplicating the material instances
					for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
					{
						UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
						Material = Interface ? Interface->GetMaterial() : nullptr;

						if (Material)
						{
							ResourceName = ObjectName + "_Material_" + Material->GetName();

							if (!GetUniqueResourceName(Material, ResourceName, ArrayCachedObject, ArrayCachedElement))
							{
								continue;
							}

							if (!ManageBakingAction(AssetPath, ResourceName))
							{
								return;
							}

							PackageName = FolderDlg->GetAssetPath() + FString("/") + ResourceName;
							TMap<UObject*, UObject*> FakeReplacementMap;
							DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Material, ResourceName, PackageName, 
								false, FakeReplacementMap, BakingOverwritePermission, bGenerateConstantMaterialInstances);
							ArrayCachedElement.Add(ResourceName);
							ArrayCachedObject.Add(DuplicatedObject);
							ReplacementMap.Add(Interface, DuplicatedObject);
							PackagesToSave.Add(DuplicatedObject->GetPackage());

							FUnrealBakeHelpers::CopyAllMaterialParameters(DuplicatedObject, Interface, TextureReplacementMaps[m]);
						}
					}
				}
				else
				{
					// Duplicate the material instances
					for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetMaterials().Num(); ++MaterialIndex)
					{
						UMaterialInterface* Interface = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
						UMaterial* ParentMaterial = Interface->GetMaterial();
						FString MaterialName = ParentMaterial ? ParentMaterial->GetName() : "Material";

						// Material
						FString MatObjName = ObjectName + "_" + MaterialName;

						if (!GetUniqueResourceName(Interface, MatObjName, ArrayCachedObject, ArrayCachedElement))
						{
							continue;
						}

						if (!ManageBakingAction(AssetPath, MatObjName))
						{
							return;
						}

						FString MatPkgName = FolderDlg->GetAssetPath() + FString("/") + MatObjName;
						UObject* DupMat = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Interface, MatObjName, 
							MatPkgName, false, ReplacementMap, BakingOverwritePermission, bGenerateConstantMaterialInstances);
						ArrayCachedObject.Add(DupMat);
						ArrayCachedElement.Add(MatObjName);
						PackagesToSave.Add(DupMat->GetPackage());

						UMaterialInstance* Inst = Cast<UMaterialInstance>(Interface);

						// Only need to duplicate the generate textures if the original material is a dynamic instance
						// If the material has Mutable textures, then it will be a dynamic material instance for sure
						if (Inst)
						{
							// Duplicate generated textures
							UMaterialInstanceDynamic* InstDynamic = Cast<UMaterialInstanceDynamic>(DupMat);
							UMaterialInstanceConstant* InstConstant = Cast<UMaterialInstanceConstant>(DupMat);

							if (InstDynamic || InstConstant)
							{
								for (int32 TextureIndex = 0; TextureIndex < Inst->TextureParameterValues.Num(); ++TextureIndex)
								{
									if (Inst->TextureParameterValues[TextureIndex].ParameterValue)
									{
										if (Inst->TextureParameterValues[TextureIndex].ParameterValue->HasAnyFlags(RF_Transient))
										{
											UTexture2D* SrcTex = Cast<UTexture2D>(Inst->TextureParameterValues[TextureIndex].ParameterValue);

											if (SrcTex)
											{
												FString ParameterSanitized = Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name.ToString();
												RemoveRestrictedChars(ParameterSanitized);

												FString TexObjName = ObjectName + "_" + MaterialName + "_" + ParameterSanitized;

												if (!GetUniqueResourceName(SrcTex, TexObjName, ArrayCachedObject, ArrayCachedElement))
												{
													UTexture* PrevTexture = Cast<UTexture>(ArrayCachedObject[ArrayCachedElement.Find(TexObjName)]);

													if (InstDynamic)
													{
														InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
													}
													else if (InstConstant)
													{
														InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
													}
													
													continue;
												}

												if (!ManageBakingAction(AssetPath, TexObjName))
												{
													return;
												}

												FString TexPkgName = FolderDlg->GetAssetPath() + FString("/") + TexObjName;
												TMap<UObject*, UObject*> FakeReplacementMap;
												UTexture2D* DupTex = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SrcTex, TexObjName, TexPkgName, nullptr, false, FakeReplacementMap, BakingOverwritePermission);
												ArrayCachedObject.Add(DupTex);
												ArrayCachedElement.Add(TexObjName);
												PackagesToSave.Add(DupTex->GetPackage());

												if (InstDynamic)
												{
													InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
												}
												else if(InstConstant)
												{
													InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
												}
											}
											else
											{
												UE_LOG(LogMutable, Error, TEXT("A Mutable texture that is not a Texture2D has been found while baking a CustomizableObjectInstance."));
											}
										}
										else
										{
											// If it's not transient it's not a mutable texture, it's a pass-through texture
											// Just set the original texture
											if (InstDynamic)
											{
												InstDynamic->SetTextureParameterValue(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, Inst->TextureParameterValues[TextureIndex].ParameterValue);
											}
											else if (InstConstant)
											{
												InstConstant->SetTextureParameterValueEditorOnly(Inst->TextureParameterValues[TextureIndex].ParameterInfo.Name, Inst->TextureParameterValues[TextureIndex].ParameterValue);
											}
										}
									}
								}
							}
						}
					}
				}
				
				// Skeletal Mesh's Skeleton
				if (Mesh->GetSkeleton())
				{
					const bool bTransient = Mesh->GetSkeleton()->GetPackage() == GetTransientPackage();

					// Don't duplicate if not transient or export all assets.
					if (bTransient || bExportAllResources)
					{
						FString SkeletonName = ObjectName + "_Skeleton";
						if (!ManageBakingAction(AssetPath, SkeletonName))
						{
							return;
						}

						FString SkeletonPkgName = FolderDlg->GetAssetPath() + FString("/") + SkeletonName;
						UObject* DuplicatedSkeleton = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Mesh->GetSkeleton(), SkeletonName, 
							SkeletonPkgName, false, ReplacementMap, BakingOverwritePermission, false);

						ArrayCachedObject.Add(DuplicatedSkeleton);
						PackagesToSave.Add(DuplicatedSkeleton->GetPackage());
						ReplacementMap.Add(Mesh->GetSkeleton(), DuplicatedSkeleton);
					}
				}

				// Skeletal Mesh
				if (!ManageBakingAction(AssetPath, ObjectName))
				{
					return;
				}

				FString PkgName = FolderDlg->GetAssetPath() + FString("/") + ObjectName;
				UObject* DupObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Mesh, ObjectName, PkgName, 
					false, ReplacementMap, BakingOverwritePermission, false);
				ArrayCachedObject.Add(DupObject);
				PackagesToSave.Add(DupObject->GetPackage());

				Mesh->Build();

				USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(DupObject);
				if (SkeletalMesh)
				{
					SkeletalMesh->GetLODInfoArray() = Mesh->GetLODInfoArray();

					SkeletalMesh->GetImportedModel()->SkeletalMeshModelGUID = FGuid::NewGuid();

					// Duplicate AssetUserData
					{
						const TArray<UAssetUserData*>* AssetUserDataArray = Mesh->GetAssetUserDataArray();
						for (const UAssetUserData* AssetUserData : *AssetUserDataArray)
						{
							if (AssetUserData)
							{
								// Duplicate to change ownership
								UAssetUserData* NewAssetUserData = Cast<UAssetUserData>(StaticDuplicateObject(AssetUserData, SkeletalMesh));
								SkeletalMesh->AddAssetUserData(NewAssetUserData);
							}
						}
					}

					// Generate render data
					SkeletalMesh->Build();
				}

				// Remove duplicated UObjects from Root (previously added to avoid objects from beeing GC in the middle of the bake process)
				for (UObject* Obj : ArrayCachedObject)
				{
					Obj->RemoveFromRoot();
				}
			}

			if (PackagesToSave.Num())
			{
				FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, true);
			}
		}
	}

	if (InInstance)
	{
		// Reenable Mutable texture streaming and requested LOD generation as they had been disabled to bake the textures
		System->SetProgressiveMipStreamingEnabled(true);
		System->SetOnlyGenerateRequestedLODsEnabled(true);
		System->SetImagePixelFormatOverride(nullptr);
		BakeTempInstance = nullptr;
	}
}


bool FCustomizableObjectEditorViewportClient::GetUniqueResourceName(UObject* Resource, FString& ResourceName, TArray<UObject*>& InCachedResources, TArray<FString>& InCachedResourceNames)
{
	int32 FindResult = InCachedResourceNames.Find(ResourceName);
	if (FindResult != INDEX_NONE)
	{
		if (Resource == InCachedResources[FindResult])
		{
			return false;
		}

		uint32 Count = 0;
		while (FindResult != INDEX_NONE)
		{
			FindResult = InCachedResourceNames.Find(ResourceName + "_" + FString::FromInt(Count));
			Count++;
		}

		ResourceName += "_" + FString::FromInt(--Count);
	}

	return true;
}


bool FCustomizableObjectEditorViewportClient::ManageBakingAction(const FString& Path, const FString& ObjName)
{
	FString PackagePath = Path + "/" + ObjName;
	UPackage* ExistingPackage = FindPackage(NULL, *PackagePath);

	if (!ExistingPackage)
	{
		FString PackageFilePath = PackagePath + "." + ObjName;

		FString PackageFileName;
		if (FPackageName::DoesPackageExist(PackageFilePath, &PackageFileName))
		{
			ExistingPackage = LoadPackage(nullptr, *PackageFileName, LOAD_EditorOnly);
		}
		else
		{
			// if package does not exists
			BakingOverwritePermission = false;
			return true;
		}
	}

	if (ExistingPackage)
	{
		// Checking if the asset is open in an editor
		TArray<IAssetEditorInstance*> ObjectEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAssetAndSubObjects(ExistingPackage);
		if (ObjectEditors.Num())
		{
			for (IAssetEditorInstance* ObjectEditorInstance : ObjectEditors)
			{
				// Close the editors that contains this asset
				if (!ObjectEditorInstance->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed))
				{
					FText Caption = LOCTEXT("OpenExisitngFile", "Open File");
					FText Message = FText::Format(LOCTEXT("CantCloseAsset", "This Obejct \"{0}\" is open in an editor and can't be closed automatically. Please close the editor and try to bake it again"), FText::FromString(ObjName));

					FMessageDialog::Open(EAppMsgType::Ok, Message, Caption);

					return false;
				}
			}
		}

		if (!BakingOverwritePermission)
		{
			FText Caption = LOCTEXT("Already existing baked files", "Already existing baked files");
			FText Message = FText::Format(LOCTEXT("OverwriteBakedInstance", "Instance baked files already exist in selected destination \"{0}\", this action will overwrite them."), FText::AsCultureInvariant(Path));
			
			if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, Caption) == EAppReturnType::Cancel)
			{
				return false;
			}

			BakingOverwritePermission = true;
		}

		UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), ExistingPackage, *ObjName);
		if (ExistingObject)
		{
			ExistingPackage->FullyLoad();

			TArray<UObject*> ObjectsToDelete;
			ObjectsToDelete.Add(ExistingObject);

			// Delete objects in the package with the same name as the one we want to create
			const uint32 NumObjectsDeleted = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);

			return NumObjectsDeleted == ObjectsToDelete.Num();
		}
	}

	return true;
}


void FCustomizableObjectEditorViewportClient::StateChangeShowGeometryData()
{
	StateChangeShowGeometryDataFlag = !StateChangeShowGeometryDataFlag;
	Invalidate();
}


void FCustomizableObjectEditorViewportClient::ShowInstanceGeometryInformation(FCanvas* InCanvas)
{
	float YOffset = 50.0f;
	int32 ComponentIndex = 0;

	// Show total number of triangles and vertices
	for (TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent.IsValid() && UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent))
		{
			const FSkeletalMeshRenderData* MeshRes = UE_MUTABLE_GETSKINNEDASSET(SkeletalMeshComponent)->GetResourceForRendering();
			int32 NumTriangles;
			int32 NumVertices;
			int32 NumLODLevel = MeshRes->LODRenderData.Num();

			for (int32 i = 0; i < NumLODLevel; ++i)
			{
				NumTriangles = 0;
				NumVertices = 0;
				const FSkeletalMeshLODRenderData& lodModel = MeshRes->LODRenderData[i];
				for (int32 j = 0; j < lodModel.RenderSections.Num(); ++j)
				{
					NumTriangles += lodModel.RenderSections[j].NumTriangles;
					NumVertices += lodModel.RenderSections[j].NumVertices;
				}

				//draw a string showing what UV channel and LOD is being displayed
				InCanvas->DrawShadowedString(
					6.0f,
					YOffset,
					*FText::Format(NSLOCTEXT("CustomizableObjectEditor", "ComponentGeometryReport", "Component {3} LOD {0} has {1} vertices and {2} triangles"),
						FText::AsNumber(i), FText::AsNumber(NumVertices), FText::AsNumber(NumTriangles), FText::AsNumber(ComponentIndex)).ToString(),
					GEngine->GetSmallFont(),
					FLinearColor::White
				);

				YOffset += 20.0f;
			}
		}

		YOffset += 40.0f;
		ComponentIndex++;
	}
}


void FCustomizableObjectEditorViewportClient::SetCustomizableObject(UCustomizableObject* CustomizableObjectParameter)
{
	CustomizableObject = CustomizableObjectParameter;
}


void FCustomizableObjectEditorViewportClient::DrawShadowedString(FCanvas* Canvas, float StartX, float StartY, const FLinearColor& Color, float TextScale, FString String)
{
	UFont* StatFont = nullptr;

	if (TextScale > 2.0f)
	{
		StatFont = GEngine->GetLargeFont();
	}
	else if (TextScale > 1.0f)
	{
		StatFont = GEngine->GetMediumFont();
	}
	else
	{
		StatFont = GEngine->GetSmallFont();
	}

	Canvas->DrawShadowedString(StartX, StartY, *String, StatFont, Color);
}


void FCustomizableObjectEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ClipMorphMaterial);
	Collector.AddReferencedObject(TransparentPlaneMaterialXY);
	Collector.AddReferencedObject(AnimationBeingPlayed);

	if (BakeTempInstance)
	{
		Collector.AddReferencedObject(BakeTempInstance);
	}
}


void FCustomizableObjectEditorViewportClient::SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags)
{
	if (bAdvancedShowFlags)
	{
		EngineShowFlags.EnableAdvancedFeatures();
	}
	else
	{
		EngineShowFlags.DisableAdvancedFeatures();
	}
}


void FCustomizableObjectEditorViewportClient::OnAssetViewerSettingsChanged(const FName& InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bPostProcessingEnabled) || InPropertyName == NAME_None)
	{
		UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		int32 ProfileIndex = GetMutableDefault<UEditorPerProjectUserSettings>()->AssetViewerProfileIndex;
		if (Settings->Profiles.IsValidIndex(ProfileIndex))
		{
			SetAdvancedShowFlagsForScene(Settings->Profiles[ProfileIndex].bPostProcessingEnabled);
		}
	}
}


void FCustomizableObjectEditorViewportClient::DrawCylinderArc(FPrimitiveDrawInterface* PDI, const FMatrix& CylToWorld, const FVector& Base, const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, float Radius, float HalfHeight, uint32 Sides, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriority, FColor Color, float MaxAngle)
{
	TArray<FDynamicMeshVertex> MeshVerts;
	TArray<uint32> MeshIndices;

	const float	AngleDelta = MaxAngle / (Sides - 1);
	const float Offset = 0.5f * MaxAngle;

	FVector2f TC = FVector2f(0.0f, 0.0f);
	float TCStep = 1.0f / (Sides - 1);

	FVector TopOffset = HalfHeight * ZAxis;
	int32 BaseVertIndex = MeshVerts.Num();

	//Compute vertices for base circle.
	for (uint32 SideIndex = 0; SideIndex < Sides; SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * FMath::Cos(AngleDelta * SideIndex - Offset) + YAxis * FMath::Sin(AngleDelta * SideIndex - Offset)) * Radius;
		FVector Normal = Vertex - Base;
		Normal.Normalize();

		FDynamicMeshVertex MeshVertex;
		MeshVertex.Position = FVector3f(Vertex - TopOffset);
		MeshVertex.TextureCoordinate[0] = TC;
		MeshVertex.SetTangents((FVector3f)-ZAxis, FVector3f((-ZAxis) ^ Normal), (FVector3f)Normal);
		MeshVertex.Color = Color;
		MeshVerts.Add(MeshVertex); //Add bottom vertex

		TC.X += TCStep;
	}

	TC = FVector2f(0.0f, 1.0f);

	//Compute vertices for the top circle
	for (uint32 SideIndex = 0; SideIndex < Sides; SideIndex++)
	{
		const FVector Vertex = Base + (XAxis * FMath::Cos(AngleDelta * SideIndex - Offset) + YAxis * FMath::Sin(AngleDelta * SideIndex - Offset)) * Radius;
		FVector Normal = Vertex - Base;
		Normal.Normalize();

		FDynamicMeshVertex MeshVertex;
		MeshVertex.Position = FVector3f(Vertex + TopOffset);	// LWC_TODO: Precision Loss
		MeshVertex.TextureCoordinate[0] = TC;
		MeshVertex.SetTangents((FVector3f)-ZAxis, FVector3f((-ZAxis) ^ Normal), (FVector3f)Normal);
		MeshVertex.Color = Color;
		MeshVerts.Add(MeshVertex); //Add top vertex

		TC.X += TCStep;
	}

	//Add sides.
	for (uint32 SideIndex = 0; SideIndex < (Sides - 1); SideIndex++)
	{
		int32 V0 = BaseVertIndex + SideIndex;
		int32 V1 = BaseVertIndex + ((SideIndex + 1) % Sides);
		int32 V2 = V0 + Sides;
		int32 V3 = V1 + Sides;

		MeshIndices.Add(V0);
		MeshIndices.Add(V2);
		MeshIndices.Add(V1);

		MeshIndices.Add(V2);
		MeshIndices.Add(V3);
		MeshIndices.Add(V1);
	}

	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());
	MeshBuilder.AddVertices(MeshVerts);
	MeshBuilder.AddTriangles(MeshIndices);

	MeshBuilder.Draw(PDI, CylToWorld, MaterialRenderProxy, DepthPriority, 0.f);
}


bool FCustomizableObjectEditorViewportClient::GetFloorVisibility()
{
	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	if (AdvancedScene != nullptr)
	{
		const UStaticMeshComponent* FloorMeshComponent = AdvancedScene->GetFloorMeshComponent();
		if (FloorMeshComponent != nullptr)
		{
			return FloorMeshComponent->IsVisible();
		}
	}

	return false;
}


void FCustomizableObjectEditorViewportClient::SetFloorVisibility(bool Value)
{
	FAdvancedPreviewScene* AdvancedScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);
	if (AdvancedScene != nullptr)
	{
		AdvancedScene->SetFloorVisibility(Value);
	}
}


bool FCustomizableObjectEditorViewportClient::GetGridVisibility()
{
	return DrawHelper.bDrawGrid;
}


bool FCustomizableObjectEditorViewportClient::GetEnvironmentMeshVisibility()
{
	FCustomizableObjectPreviewScene* CustomizableObjectPreviewScene = static_cast<FCustomizableObjectPreviewScene*>(PreviewScene);
	if (CustomizableObjectPreviewScene != nullptr)
	{
		return CustomizableObjectPreviewScene->GetSkyComponent()->IsVisible();
	}

	return false;
}


void FCustomizableObjectEditorViewportClient::SetEnvironmentMeshVisibility(uint32 Value)
{
	FCustomizableObjectPreviewScene* CustomizableObjectPreviewScene = static_cast<FCustomizableObjectPreviewScene*>(PreviewScene);
	if (CustomizableObjectPreviewScene != nullptr)
	{
		CustomizableObjectPreviewScene->GetSkyComponent()->SetVisibility(Value == 1, true);
	}

	Invalidate();
}

bool FCustomizableObjectEditorViewportClient::IsOrbitalCameraActive() const
{
	return bActivateOrbitalCamera;
}

void FCustomizableObjectEditorViewportClient::SetCameraMode(bool Value)
{
	bActivateOrbitalCamera = Value;
	UpdateCameraSetup();
}


void FCustomizableObjectEditorViewportClient::SetShowBones()
{
	bShowBones = !bShowBones;
}


bool FCustomizableObjectEditorViewportClient::IsShowingBones() const
{
	return bShowBones;
}


const TArray<ULightComponent*>& FCustomizableObjectEditorViewportClient::GetLightComponents() const
{
	return LightComponents;
}


void FCustomizableObjectEditorViewportClient::DrawMeshBones(UDebugSkelMeshComponent* MeshComponent, FPrimitiveDrawInterface* PDI)
{
	if (!MeshComponent ||
		!MeshComponent->GetSkeletalMeshAsset() ||
		MeshComponent->GetNumDrawTransform() == 0 ||
		MeshComponent->SkeletonDrawMode == ESkeletonDrawMode::Hidden)
	{
		return;
	}

	TArray<FTransform> WorldTransforms;
	WorldTransforms.AddUninitialized(MeshComponent->GetNumDrawTransform());

	TArray<FLinearColor> BoneColors;
	BoneColors.AddUninitialized(MeshComponent->GetNumDrawTransform());

	const FLinearColor BoneColor = GetDefault<UPersonaOptions>()->DefaultBoneColor;
	const FLinearColor VirtualBoneColor = GetDefault<UPersonaOptions>()->VirtualBoneColor;
	const TArray<FBoneIndexType>& DrawBoneIndices = MeshComponent->GetDrawBoneIndices();
	
	for (int32 Index = 0; Index < DrawBoneIndices.Num(); ++Index)
	{
		const int32 BoneIndex = DrawBoneIndices[Index];
		WorldTransforms[BoneIndex] = MeshComponent->GetDrawTransform(BoneIndex) * MeshComponent->GetComponentTransform();
		BoneColors[BoneIndex] = BoneColor;
	}

	// color virtual bones
	for (int16 VirtualBoneIndex : MeshComponent->GetReferenceSkeleton().GetRequiredVirtualBones())
	{
		BoneColors[VirtualBoneIndex] = VirtualBoneColor;
	}

	constexpr bool bForceDraw = false;

	// don't allow selection if the skeleton draw mode is greyed out
	//const bool bAddHitProxy = MeshComponent->SkeletonDrawMode != ESkeletonDrawMode::GreyedOut;

	FSkelDebugDrawConfig DrawConfig;
	DrawConfig.BoneDrawMode = EBoneDrawMode::All;
	DrawConfig.BoneDrawSize = 1.0f;
	DrawConfig.bAddHitProxy = false;
	DrawConfig.bForceDraw = bForceDraw;
	DrawConfig.DefaultBoneColor = GetMutableDefault<UPersonaOptions>()->DefaultBoneColor;
	DrawConfig.AffectedBoneColor = GetMutableDefault<UPersonaOptions>()->AffectedBoneColor;
	DrawConfig.SelectedBoneColor = GetMutableDefault<UPersonaOptions>()->SelectedBoneColor;
	DrawConfig.ParentOfSelectedBoneColor = GetMutableDefault<UPersonaOptions>()->ParentOfSelectedBoneColor;

	//No user interaction right now
	TArray<TRefCountPtr<HHitProxy>> HitProxies;

	SkeletalDebugRendering::DrawBones(
		PDI,
		MeshComponent->GetComponentLocation(),
		DrawBoneIndices,
		MeshComponent->GetReferenceSkeleton(),
		WorldTransforms,
		MeshComponent->BonesOfInterest,
		BoneColors,
		HitProxies,
		DrawConfig
	);
}


void FCustomizableObjectEditorViewportClient::SetWidgetType(EWidgetType Type)
{
	WidgetType = Type;
	
	SetWidgetMode(UE::Widget::WM_Translate);
	Widget->SetDefaultVisibility(Type != EWidgetType::Hidden);
}	


/////////////////////////////////////////////////
// select folder dialog \todo: move to its own file
//////////////////////////////////////////////////
void SMutableSelectFolderDlg::Construct(const FArguments& InArgs)
{
	AssetPath = FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()));
	FileName = InArgs._DefaultFileName;

	bExportAllResources = false;

	if (AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SMutableSelectFolderDlg::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("SMutableSelectFolderDlg_Title", "Select target folder for baked resources"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		//.SizingRule( ESizingRule::Autosized )
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot() // Add user input block
		.Padding(2)
		[
			SNew(SBorder)
			.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectPath", "Select Path"))
		.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
		]

	+ SVerticalBox::Slot()
		.FillHeight(1)
		.Padding(3)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FileName", "File Name"))
			.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14))
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SEditableTextBox)
			.Text(InArgs._DefaultFileName)
			.OnTextCommitted(this, &SMutableSelectFolderDlg::OnNameChange)
			.MinDesiredWidth(250)
		]

		]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ExportAllUsedResources", "Export all used resources  "))
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12))
				.ToolTipText(LOCTEXT("Export all used Resources", "All the resources used by the object will be baked/stored in the target folder. Otherwise, only the assets that Mutable modifies will be baked/stored."))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("ExportAllResources", "Export all resources"))
				.HAlign(HAlign_Right)
				.IsChecked(bExportAllResources ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SMutableSelectFolderDlg::OnBoolParameterChanged)
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GenerateConstantMaterialInstances", "Generate Constant Material Instances  "))
				.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12))
				.ToolTipText(LOCTEXT("Generate Constant Material Instances", "All the material instances in the baked skeletal meshes will be constant instead of dynamic. They cannot be changed at runtime but they are lighter and required for UEFN."))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("GenerateConstantMaterialInstances_Checkbox", "Generate Constant Material Instances"))
				.HAlign(HAlign_Right)
				.IsChecked(bGenerateConstantMaterialInstances ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &SMutableSelectFolderDlg::OnConstantMaterialInstancesBoolParameterChanged)
			]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(5)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.SlotPadding"))
		.MinDesiredSlotWidth(UE_MUTABLE_GET_FLOAT("StandardDialog.MinDesiredSlotWidth"))
		.MinDesiredSlotHeight(UE_MUTABLE_GET_FLOAT("StandardDialog.MinDesiredSlotHeight"))
		+ SUniformGridPanel::Slot(0, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.ContentPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.ContentPadding"))
		.Text(LOCTEXT("OK", "OK"))
		.OnClicked(this, &SMutableSelectFolderDlg::OnButtonClick, EAppReturnType::Ok)
		]
	+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.ContentPadding(UE_MUTABLE_GET_MARGIN("StandardDialog.ContentPadding"))
		.Text(LOCTEXT("Cancel", "Cancel"))
		.OnClicked(this, &SMutableSelectFolderDlg::OnButtonClick, EAppReturnType::Cancel)
		]
		]
		]);
}

void SMutableSelectFolderDlg::OnPathChange(const FString& NewPath)
{
	AssetPath = FText::FromString(NewPath);
}

FReply SMutableSelectFolderDlg::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}


void SMutableSelectFolderDlg::OnNameChange(const FText& NewName, ETextCommit::Type CommitInfo)
{
	FileName = NewName;
}


void SMutableSelectFolderDlg::OnBoolParameterChanged(ECheckBoxState InCheckboxState)
{
	bExportAllResources = InCheckboxState == ECheckBoxState::Checked;
}


void SMutableSelectFolderDlg::OnConstantMaterialInstancesBoolParameterChanged(ECheckBoxState InCheckboxState)
{
	bGenerateConstantMaterialInstances = InCheckboxState == ECheckBoxState::Checked;
}


EAppReturnType::Type SMutableSelectFolderDlg::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SMutableSelectFolderDlg::GetAssetPath()
{
	return AssetPath.ToString();
}


FString SMutableSelectFolderDlg::GetFileName()
{
	return FileName.ToString();
}


bool SMutableSelectFolderDlg::GetExportAllResources()
{
	return bExportAllResources;
}


bool SMutableSelectFolderDlg::GetGenerateConstantMaterialInstances()
{
	return bGenerateConstantMaterialInstances;
}

#undef LOCTEXT_NAMESPACE 

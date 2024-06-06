// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShallowWaterRiverActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "WaterBodyActor.h"
#include "WaterSplineComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "ShallowWaterCommon.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Math/Float16Color.h"

UShallowWaterRiverComponent::UShallowWaterRiverComponent(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	PrimaryComponentTick.bCanEverTick = true;	

#if WITH_EDITORONLY_DATA
	bTickInEditor = true;
#endif // WITH_EDITORONLY_DATA

	bIsInitialized = false;
	bTickInitialize = false;

	ResolutionMaxAxis = 512;
	SourceSize = 1000;
	//NiagaraRiverSimulation = LoadObject<UNiagaraSystem>(nullptr, TEXT("/WaterAdvanced/Niagara/Systems/Grid2D_SW_River.Grid2D_SW_River"));
}

void UShallowWaterRiverComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	bIsInitialized = false;
	bTickInitialize = false;

	Rebuild();
#endif
}

void UShallowWaterRiverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
#if WITH_EDITOR
	// lots of tick ordering issues, so we try to initialize on the first tick too
	if (!bIsInitialized && !bTickInitialize)
	{
		bTickInitialize = true;
		Rebuild();
	}

#endif

	// #todo(dmp): this is all temporary
// set the sim texture on each
	/*
	for (TObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();

		if (CurrWaterBodyComponent->ShallowWaterSimulationGrid.ArrayValues.Num() == 0 && PreviewBakedSim)
		{
			CurrWaterBodyComponent->SetShallowWaterSimulationTexture(ShallowWaterSimArrayValues,
				FIntVector2(BakedWaterSurfaceTexture->GetSizeX(), BakedWaterSurfaceTexture->GetSizeY()), SystemPos, WorldGridSize);
		}		
	}
	*/
}

void UShallowWaterRiverComponent::OnUnregister()
{
	Super::OnUnregister();

	//if (RiverSimSystem && !RiverSimSystem->IsBeingDestroyed())
	//{
	//	RiverSimSystem->SetActive(false);
	//	RiverSimSystem->DestroyComponent();
	//	RiverSimSystem = nullptr;
	//}
}

#if WITH_EDITOR

void UShallowWaterRiverComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}
			
	// this should go before rebuild not after...something is wrong
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UShallowWaterRiverComponent, PreviewBakedSim) && RiverSimSystem->IsActive())
	{
		RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), PreviewBakedSim);
	}
	else
	{
		Rebuild();
	}
}

void UShallowWaterRiverComponent::Rebuild()
{	
	if (RiverSimSystem != nullptr)
	{
		RiverSimSystem->SetActive(false);
		RiverSimSystem->DestroyComponent();
		RiverSimSystem = nullptr;		
	}
	
	if (NiagaraRiverSimulation == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - null Niagara system asset"));
	}

	AllWaterBodies.Empty();

	// collect all the water bodies	
	if (SourceRiverWaterBody != nullptr)
	{
		AllWaterBodies.Add(SourceRiverWaterBody);
	}
	else	
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - No source water body specified"));
		return;
	}
	
	if (SinkRiverWaterBody != nullptr)
	{
		AllWaterBodies.Add(SinkRiverWaterBody);
	}	

	for (TObjectPtr<AWaterBody > CurrWaterBody : AdditonalRiverWaterBodies)
	{
		AllWaterBodies.Add(CurrWaterBody);
	}

	if (AllWaterBodies.Num() == 0)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - No water bodies specified"));
		return;
	}

	// accumulate bounding box for river water bodies
	FBoxSphereBounds::Builder CombinedWorldBoundsBuilder;
	for (TObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		if (CurrWaterBody != nullptr)
		{
			// accumulate bounds
			FBoxSphereBounds WorldBounds;
			CurrWaterBody->GetActorBounds(false, WorldBounds.Origin, WorldBounds.BoxExtent);

			CombinedWorldBoundsBuilder += WorldBounds;
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - skipping null water body actor found"));
			continue;
		}
	}
	FBoxSphereBounds CombinedBounds(CombinedWorldBoundsBuilder);

	if (CombinedBounds.BoxExtent.Length() < SMALL_NUMBER)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - river bodies have zero bounds"));
		return;
	}

	FVector SourcePos(0, 0, 0);
	float SourceWidth = 1;
	float SourceDepth = 1;
	FVector SourceDir(1, 0, 0);	

	// Get source
	if (!QueryWaterAtSplinePoint(SourceRiverWaterBody, 0, SourcePos, SourceDir, SourceWidth, SourceDepth))
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - water source query failed"));
		return;
	}

	FVector SinkPos(0, 0, 0);
	float SinkWidth = 1;
	float SinkDepth = 1;
	FVector SinkDir(1, 0, 0);

	// Get Sink		
	TObjectPtr<AWaterBody> SinkToUse = SinkRiverWaterBody;

	// if no sink specified, use source
	if (SinkToUse == nullptr)
	{
		SinkToUse = SourceRiverWaterBody;
	}
	
	if (!QueryWaterAtSplinePoint(SinkToUse, -1, SinkPos, SinkDir, SinkWidth, SinkDepth))
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - water sink query failed"));
		return;
	}

	SystemPos = CombinedBounds.Origin - FVector(0, 0, CombinedBounds.BoxExtent.Z);
	
	RiverSimSystem = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
	RiverSimSystem->bUseAttachParentBound = false;
	RiverSimSystem->SetWorldLocation(SystemPos);

	if (GetWorld() && GetWorld()->bIsWorldInitialized)
	{
		if (!RiverSimSystem->IsRegistered())
		{
			RiverSimSystem->RegisterComponentWithWorld(GetWorld());
		}

		RiverSimSystem->SetVisibleFlag(true);
		RiverSimSystem->SetAsset(NiagaraRiverSimulation);
							
		// convert to raw ptr array for function library
		TArray<AActor*> BottomContourActorsRawPtr;
		for (TObjectPtr<AActor> CurrActor : BottomContourActors)
		{
			AActor* CurrActorRawPtr = CurrActor.Get();
			BottomContourActorsRawPtr.Add(CurrActorRawPtr);
		}

		FName DIName = "User.BottomCapture";

		UNiagaraFunctionLibrary::SetSceneCapture2DDataInterfaceManagedMode(RiverSimSystem, DIName,
			ESceneCaptureSource::SCS_SceneDepth,
			FIntPoint(ResolutionMaxAxis, ResolutionMaxAxis),
			ETextureRenderTargetFormat::RTF_R16f,
			ECameraProjectionMode::Orthographic,
			90.0f,
			FMath::Max(WorldGridSize.X, WorldGridSize.Y),
			true,
			false,
			BottomContourActorsRawPtr);

	
		// accumulate bounding box for river water bodies
		FBoxSphereBounds::Builder BottomContourCombinedWorldBoundsBuilder;
		for (TObjectPtr<AActor> BottomContourActor : BottomContourActors)
		{
			if (BottomContourActor != nullptr)
			{
				// accumulate bounds
				FBoxSphereBounds WorldBounds;
				BottomContourActor->GetActorBounds(false, WorldBounds.Origin, WorldBounds.BoxExtent);

				BottomContourCombinedWorldBoundsBuilder += WorldBounds;
			}
			else
			{
				UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - skipping null bottom contour boundary actor found"));
				continue;
			}
		}
		FBoxSphereBounds CombinedBottomContourBounds(BottomContourCombinedWorldBoundsBuilder);
				
		RiverSimSystem->ReinitializeSystem();

		RiverSimSystem->SetVariableFloat(FName("CaptureOffset"), BottomContourCaptureOffset + CombinedBottomContourBounds.Origin.Z + CombinedBottomContourBounds.BoxExtent.Z);
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - World not initialized"));
		return;
	}
	

	if (RiverSimSystem == nullptr)
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::Rebuild() - Cannot spawn river system"));
		return;
	}

	RiverSimSystem->Activate();

	WorldGridSize = 2.0f * FVector2D(CombinedBounds.BoxExtent.X, CombinedBounds.BoxExtent.Y);
	RiverSimSystem->SetVariableVec2(FName("WorldGridSize"), WorldGridSize);
	RiverSimSystem->SetVariableInt(FName("ResolutionMaxAxis"), ResolutionMaxAxis);

	// pad out source's box height a so it intersects the sim plane.  This value doesn't matter much so we hardcode it
	float Overshoot = 1000.f;
	float FinalSourceHeight = 2. * CombinedBounds.BoxExtent.Z + Overshoot;

	RiverSimSystem->SetVariablePosition(FName("SourcePos"), SourcePos - FVector(0, 0, .5 * FinalSourceHeight) + FVector(SourceDir.X, SourceDir.Y, 0) * .5 * SourceSize);
	RiverSimSystem->SetVariableVec3(FName("SourceSize"), FVector(SourceWidth, SourceSize, FinalSourceHeight));
	RiverSimSystem->SetVariableFloat(FName("SourceAngle"), PI / 2.f + FMath::Acos(SourceDir.Dot(FVector(1,0,0))));
	
	// height of the sink box doesn't matter
	float SinkBoxHeight = 10000000;
	RiverSimSystem->SetVariablePosition(FName("SinkPos"), SinkPos);
	RiverSimSystem->SetVariableVec3(FName("SinkSize"), FVector(SinkWidth, SourceSize, SinkBoxHeight));
	RiverSimSystem->SetVariableFloat(FName("SinkAngle"), PI / 2.f + FMath::Acos(SinkDir.Dot(FVector(1, 0, 0))));

	RiverSimSystem->SetVariableFloat(FName("SimSpeed"), SimSpeed);
	RiverSimSystem->SetVariableInt(FName("NumSteps"), NumSteps);	
	
	BakedWaterSurfaceRT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	BakedWaterSurfaceRT->InitAutoFormat(1, 1);
	RiverSimSystem->SetVariableTextureRenderTarget(FName("SimGridRT"), BakedWaterSurfaceRT);
	RiverSimSystem->SetVariableBool(FName("ReadCachedSim"), PreviewBakedSim);

	if (BakedWaterSurfaceTexture != nullptr)
	{
		RiverSimSystem->SetVariableTexture(FName("BakedSimTexture"), BakedWaterSurfaceTexture);
	}

	bIsInitialized = true;
}

void UShallowWaterRiverComponent::Bake()
{	
	EObjectFlags TextureObjectFlags = EObjectFlags::RF_Public;

	BakedWaterSurfaceTexture = BakedWaterSurfaceRT->ConstructTexture2D(this, "BakedRiverTexture", TextureObjectFlags);

	RiverSimSystem->SetVariableTexture(FName("BakedSimTexture"), BakedWaterSurfaceTexture);

	// #todo(dmp): would love to do one more update here to run the pressure solver to get a pressure grid instead of velocity

	// Readback to get the river texture values as an array
	TArray<FFloat16Color> TmpShallowWaterSimArrayValues;
	BakedWaterSurfaceRT->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(TmpShallowWaterSimArrayValues);

	ShallowWaterSimArrayValues.Empty();
	ShallowWaterSimArrayValues.AddZeroed(TmpShallowWaterSimArrayValues.Num());

	// #todo(dmp): no real need to cast all of the values to floats, but seems more conveinent for down the road compute 
	int Index = 0;
	for (FFloat16Color Val : TmpShallowWaterSimArrayValues)
	{
		FVector4 FloatVal;
		FloatVal.X = Val.R;
		FloatVal.Y = Val.G;
		FloatVal.Z = Val.B;
		FloatVal.W = Val.A;

		ShallowWaterSimArrayValues[Index++] = FloatVal;
	}

	// #todo(dmp): this copies the array to each water body that uses it- it'd be better if it were referenced and owned by the
	// river actor instead.  Perhaps the PT version copies it, but that is different...
	// set the sim texture on each water body that is in the simulated river.  
	for (TObjectPtr<AWaterBody > CurrWaterBody : AllWaterBodies)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = CurrWaterBody->GetWaterBodyComponent();

		// #todo(dmp): reenable
		// CurrWaterBodyComponent->SetShallowWaterSimulationTexture(ShallowWaterSimArrayValues, 
		//	FIntVector2(BakedWaterSurfaceRT->SizeX, BakedWaterSurfaceRT->SizeY), SystemPos, WorldGridSize);
	}
}

void UShallowWaterRiverComponent::OnWaterInfoTextureCreated(const UTextureRenderTarget2D* InWaterInfoTexture)
{
	if (InWaterInfoTexture == nullptr)
	{
		ensureMsgf(false, TEXT("UShallowWaterRiverComponent::OnWaterInfoTextureCreated was called with NULL WaterInfoTexture"));
		return;
	}

	WaterInfoTexture = InWaterInfoTexture;
	if (RiverSimSystem)
	{
		RiverSimSystem->SetVariableTexture(FName("WaterInfoTexture"), Cast<UTexture>(const_cast<UTextureRenderTarget2D*>(WaterInfoTexture.Get())));
	}
	else
	{
		ensureMsgf(false, TEXT("UShallowWaterRiverComponent::OnWaterInfoTextureCreated No river system to set water info on"));
	}
}

bool UShallowWaterRiverComponent::QueryWaterAtSplinePoint(TObjectPtr<AWaterBody> WaterBody, int SplinePoint, FVector& OutPos, FVector& OutTangent, float& OutWidth, float& OutDepth)
{	
	if (WaterBody != nullptr)
	{
		TObjectPtr<UWaterBodyComponent> CurrWaterBodyComponent = WaterBody->GetWaterBodyComponent();

		UWaterSplineComponent* CurrSpline = WaterBody->GetWaterSpline();
		
		if (CurrSpline != nullptr)
		{
			// -1 means last spline point
			if (SplinePoint == -1)
			{
				SplinePoint = CurrSpline->GetNumberOfSplinePoints() - 1;
			}

			UWaterSplineMetadata* Metadata = WaterBody->GetWaterSplineMetadata();

			if (Metadata != nullptr)
			{
				OutPos = CurrSpline->SplineCurves.Position.Points[SplinePoint].OutVal;
				OutPos = WaterBody->GetActorTransform().TransformPosition(OutPos);

				OutWidth = Metadata->RiverWidth.Points[SplinePoint].OutVal;
				OutDepth = Metadata->Depth.Points[SplinePoint].OutVal;

				OutTangent = CurrSpline->SplineCurves.Position.Points[SplinePoint].LeaveTangent;
				OutTangent.Normalize();
			}
			else
			{
				UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water spline metadata is null"));
				return false;
			}
		}
		else
		{
			UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water spline component is null"));
			return false;
		}
	}
	else
	{
		UE_LOG(LogShallowWater, Warning, TEXT("UShallowWaterRiverComponent::QueryWaterAtSplinePoint() - Water actor is null"));
		return false;
	}

	return true;
}
#endif

AShallowWaterRiver::AShallowWaterRiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShallowWaterRiverComponent = CreateDefaultSubobject<UShallowWaterRiverComponent>(TEXT("ShallowWaterRiverComponent"));
	RootComponent = ShallowWaterRiverComponent;

	PrimaryActorTick.bCanEverTick = true;
	SetHidden(false);
}


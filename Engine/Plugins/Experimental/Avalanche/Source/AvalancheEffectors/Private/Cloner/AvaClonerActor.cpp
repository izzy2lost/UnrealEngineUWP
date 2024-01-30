// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/AvaClonerActor.h"

#include "Cloner/AvaClonerComponent.h"
#include "Cloner/Layouts/AvaClonerCircleLayout.h"
#include "Cloner/Layouts/AvaClonerCylinderLayout.h"
#include "Cloner/Layouts/AvaClonerGridLayout.h"
#include "Cloner/Layouts/AvaClonerHoneycombLayout.h"
#include "Cloner/Layouts/AvaClonerLineLayout.h"
#include "Cloner/Layouts/AvaClonerMeshLayout.h"
#include "Cloner/Layouts/AvaClonerSphereUniformLayout.h"
#include "Cloner/Layouts/AvaClonerSplineLayout.h"
#include "Effector/AvaEffectorActor.h"
#include "Engine/StaticMeshActor.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "Subsystems/AvaClonerSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Selection.h"
#include "Settings/LevelEditorViewportSettings.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAvaClonerActor, Log, All);

// Sets default values
AAvaClonerActor::AAvaClonerActor()
{
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused   = true;

	ClonerComponent = CreateDefaultSubobject<UAvaClonerComponent>(TEXT("AvaClonerComponent"));
	SetRootComponent(ClonerComponent);

	// Default Scale Curve
	LifetimeScaleCurve.AddKey(0, 1.f);
	LifetimeScaleCurve.AddKey(1, 0.f);

	if (!IsTemplate())
	{
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->GetSelectedActors()->SelectionChangedEvent.AddUObject(this, &AAvaClonerActor::OnEditorSelectionChanged);
		}
#endif

		ClonerComponent->TransformUpdated.AddUObject(this, &AAvaClonerActor::OnClonerTransformed);
		ClonerComponent->OnClonerMeshUpdated.AddUObject(this, &AAvaClonerActor::OnClonerMeshUpdated);

		const TArray<FString> LayoutNames = GetClonerLayoutNames();

		// Apply default layout
		LayoutName = !LayoutNames.IsEmpty() ? FName(LayoutNames[0]) : NAME_None;

#if WITH_EDITOR
		if (const UAvaClonerSubsystem* ClonerSubsystem = UAvaClonerSubsystem::Get())
		{
			UAvaClonerSubsystem::OnCVarChangedDelegate.AddUObject(this, &AAvaClonerActor::OnCVarChanged);
			bReduceMotionGhosting = ClonerSubsystem->IsNoFlickerEnabled();
		}
#endif
	}
}

#if WITH_EDITOR
FString AAvaClonerActor::GetDefaultActorLabel() const
{
	return DefaultLabel;
}

const TAvaPropertyChangeDispatcher<AAvaClonerActor> AAvaClonerActor::PropertyChangeDispatcher =
{
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bEnabled), &AAvaClonerActor::OnEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, Seed), &AAvaClonerActor::OnSeedChanged },
	/** Layout */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, LayoutName), &AAvaClonerActor::OnLayoutNameChanged },
	/** Advanced */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bVisualizerSpriteVisible), &AAvaClonerActor::OnVisualizerSpriteVisibleChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bReduceMotionGhosting), &AAvaClonerActor::OnReduceMotionGhostingChanged },
	/** Spawn */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, SpawnLoopMode), &AAvaClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, SpawnLoopInterval), &AAvaClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, SpawnLoopIterations), &AAvaClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, SpawnBehaviorMode), &AAvaClonerActor::OnSpawnOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, SpawnRate), &AAvaClonerActor::OnSpawnOptionsChanged },
	/** Lifetime */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bLifetimeEnabled), &AAvaClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, LifetimeMin), &AAvaClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, LifetimeMax), &AAvaClonerActor::OnLifetimeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bLifetimeScaleEnabled), &AAvaClonerActor::OnLifetimeOptionsChanged },
	/** Renderer */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, MeshRenderMode), &AAvaClonerActor::OnMeshRenderModeChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, MeshFacingMode), &AAvaClonerActor::OnMeshRendererOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bMeshCastShadows), &AAvaClonerActor::OnMeshRendererOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, DefaultMeshes), &AAvaClonerActor::OnDefaultMeshesChanged },
	/** Progress */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bInvertProgress), &AAvaClonerActor::OnProgressChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, Progress), &AAvaClonerActor::OnProgressChanged },
	/** Step */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bDeltaStepEnabled), &AAvaClonerActor::OnDeltaStepChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, DeltaStepRotation), &AAvaClonerActor::OnDeltaStepChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, DeltaStepScale), &AAvaClonerActor::OnDeltaStepChanged },
	/** Range */
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bRangeEnabled), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeOffsetMin), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeOffsetMax), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeRotationMin), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeRotationMax), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeScaleMin), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeScaleMax), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, bRangeScaleUniform), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeScaleUniformMin), &AAvaClonerActor::OnRangeOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaClonerActor, RangeScaleUniformMax), &AAvaClonerActor::OnRangeOptionsChanged },
};

void AAvaClonerActor::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}
#endif

void AAvaClonerActor::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Register new type def for niagara

		constexpr ENiagaraTypeRegistryFlags MeshFlags =
			ENiagaraTypeRegistryFlags::AllowAnyVariable |
			ENiagaraTypeRegistryFlags::AllowParameter;

		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshRenderMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerGridConstraint>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerPlane>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerAxis>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerEasing>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshAsset>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshSampleData>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerEffectorType>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerTextureSampleChannel>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerCompareMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerEffectorMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerSpawnLoopMode>()), MeshFlags);
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(StaticEnum<EAvaClonerSpawnBehaviorMode>()), MeshFlags);
	}
}

void AAvaClonerActor::PostLoad()
{
	Super::PostLoad();

	// Migrate old properties to new layout system
	if (!bDeprecatedPropertiesMigrated)
	{
		// Grid options
		if (UAvaClonerGridLayout* GridLayout = FindOrAddLayout<UAvaClonerGridLayout>())
		{
			GridLayout->SetCountX(GridOptions_DEPRECATED.Count.X);
			GridLayout->SetCountY(GridOptions_DEPRECATED.Count.Y);
			GridLayout->SetCountZ(GridOptions_DEPRECATED.Count.Z);
			GridLayout->SetSpacingX(GridOptions_DEPRECATED.Spacing.X);
			GridLayout->SetSpacingY(GridOptions_DEPRECATED.Spacing.Y);
			GridLayout->SetSpacingZ(GridOptions_DEPRECATED.Spacing.Z);
			GridLayout->SetConstraint(GridOptions_DEPRECATED.Constraint);
			GridLayout->SetInvertConstraint(GridOptions_DEPRECATED.bInvertConstraint);
			GridLayout->SetSphereConstraint(GridOptions_DEPRECATED.SphereConstraint);
			GridLayout->SetCylinderConstraint(GridOptions_DEPRECATED.CylinderConstraint);
			GridLayout->SetTextureConstraint(GridOptions_DEPRECATED.TextureConstraint);
		}

		// Line options
		if (UAvaClonerLineLayout* LineLayout = FindOrAddLayout<UAvaClonerLineLayout>())
		{
			LineLayout->SetCount(LineOptions_DEPRECATED.Count);
			LineLayout->SetSpacing(LineOptions_DEPRECATED.Spacing);
			LineLayout->SetAxis(LineOptions_DEPRECATED.Axis);
			LineLayout->SetDirection(LineOptions_DEPRECATED.Direction);
			LineLayout->SetRotation(LineOptions_DEPRECATED.Rotation);
		}

		// Circle options
		if (UAvaClonerCircleLayout* CircleLayout = FindOrAddLayout<UAvaClonerCircleLayout>())
		{
			CircleLayout->SetCount(CircleOptions_DEPRECATED.Count);
			CircleLayout->SetRadius(CircleOptions_DEPRECATED.Radius);
			CircleLayout->SetAngleStart(CircleOptions_DEPRECATED.AngleStart);
			CircleLayout->SetAngleRatio(CircleOptions_DEPRECATED.AngleRatio);
			CircleLayout->SetOrientMesh(CircleOptions_DEPRECATED.bOrientMesh);
			CircleLayout->SetPlane(CircleOptions_DEPRECATED.Plane);
			CircleLayout->SetRotation(CircleOptions_DEPRECATED.Rotation);
			CircleLayout->SetScale(CircleOptions_DEPRECATED.Scale);
		}

		// Cylinder options
		if (UAvaClonerCylinderLayout* CylinderLayout = FindOrAddLayout<UAvaClonerCylinderLayout>())
		{
			CylinderLayout->SetBaseCount(CylinderOptions_DEPRECATED.BaseCount);
			CylinderLayout->SetHeightCount(CylinderOptions_DEPRECATED.HeightCount);
			CylinderLayout->SetHeight(CylinderOptions_DEPRECATED.Height);
			CylinderLayout->SetRadius(CylinderOptions_DEPRECATED.Radius);
			CylinderLayout->SetAngleStart(CylinderOptions_DEPRECATED.AngleStart);
			CylinderLayout->SetAngleRatio(CylinderOptions_DEPRECATED.AngleRatio);
			CylinderLayout->SetOrientMesh(CylinderOptions_DEPRECATED.bOrientMesh);
			CylinderLayout->SetPlane(CylinderOptions_DEPRECATED.Plane);
			CylinderLayout->SetRotation(CylinderOptions_DEPRECATED.Rotation);
			CylinderLayout->SetScale(CylinderOptions_DEPRECATED.Scale);
		}

		// Sphere options
		if (UAvaClonerSphereUniformLayout* SphereLayout = FindOrAddLayout<UAvaClonerSphereUniformLayout>())
		{
			SphereLayout->SetCount(SphereOptions_DEPRECATED.Count);
			SphereLayout->SetRadius(SphereOptions_DEPRECATED.Radius);
			SphereLayout->SetRatio(SphereOptions_DEPRECATED.Ratio);
			SphereLayout->SetOrientMesh(SphereOptions_DEPRECATED.bOrientMesh);
			SphereLayout->SetRotation(SphereOptions_DEPRECATED.Rotation);
			SphereLayout->SetScale(SphereOptions_DEPRECATED.Scale);
		}

		// Honeycomb options
		if (UAvaClonerHoneycombLayout* HoneycombLayout = FindOrAddLayout<UAvaClonerHoneycombLayout>())
		{
			HoneycombLayout->SetPlane(HoneycombOptions_DEPRECATED.Plane);
			HoneycombLayout->SetWidthCount(HoneycombOptions_DEPRECATED.WidthCount);
			HoneycombLayout->SetHeightCount(HoneycombOptions_DEPRECATED.HeightCount);
			HoneycombLayout->SetWidthOffset(HoneycombOptions_DEPRECATED.WidthOffset);
			HoneycombLayout->SetHeightOffset(HoneycombOptions_DEPRECATED.HeightOffset);
			HoneycombLayout->SetHeightSpacing(HoneycombOptions_DEPRECATED.HeightSpacing);
			HoneycombLayout->SetWidthSpacing(HoneycombOptions_DEPRECATED.WidthSpacing);
		}

		// Sample mesh options
		if (UAvaClonerMeshLayout* MeshLayout = FindOrAddLayout<UAvaClonerMeshLayout>())
		{
			MeshLayout->SetCount(SampleMeshOptions_DEPRECATED.Count);
			MeshLayout->SetAsset(SampleMeshOptions_DEPRECATED.Asset);
			MeshLayout->SetSampleData(SampleMeshOptions_DEPRECATED.SampleData);
			MeshLayout->SetSampleActorWeak(SampleMeshOptions_DEPRECATED.SampleActor);
		}

		// Sample spline options
		if (UAvaClonerSplineLayout* SplineLayout = FindOrAddLayout<UAvaClonerSplineLayout>())
		{
			SplineLayout->SetCount(SampleSplineOptions_DEPRECATED.Count);
			SplineLayout->SetSplineActorWeak(SampleSplineOptions_DEPRECATED.SplineActor);
		}

		// Set previous layout
		if (const UAvaClonerSubsystem* Subsystem = UAvaClonerSubsystem::Get())
		{
			const int32 LayoutIdx = static_cast<int32>(Layout_DEPRECATED);
			SetLayoutName(Subsystem->GetLayoutNames()[LayoutIdx]);
		}

		bDeprecatedPropertiesMigrated = true;

		UE_LOG(LogAvaClonerActor, Log, TEXT("%s : Cloner deprecated properties migrated"), *GetActorNameOrLabel());
	}

	for (TMap<FName, TObjectPtr<UAvaClonerLayoutBase>>::TIterator It(LayoutInstances); It; ++It)
	{
		if (!It->Value)
		{
			It.RemoveCurrent();
			continue;
		}

		// Update layout name if changed
		const FName NewLayoutName = It->Value->GetLayoutName();

		if (It->Key != NewLayoutName)
		{
			LayoutInstances.Add(NewLayoutName, It->Value);
			It.RemoveCurrent();

			UE_LOG(LogAvaClonerActor, Log, TEXT("%s : Cloner layout name changed %s"), *GetActorNameOrLabel(), *NewLayoutName.ToString());
		}
	}
}

void AAvaClonerActor::PostActorCreated()
{
	Super::PostActorCreated();

#if WITH_EDITOR
	SpawnDefaultActorAttached();
#endif
}

void AAvaClonerActor::Tick(float InDeltaTime)
{
	Super::Tick(InDeltaTime);

	if (!bClonerInitialized)
	{
		InitializeCloner();
	}

	if (!bEnabled)
	{
		return;
	}

	TreeUpdateDeltaTime += InDeltaTime;
	if (TreeUpdateDeltaTime >= TreeUpdateInterval)
	{
		TreeUpdateDeltaTime -= TreeUpdateInterval;

		if (ClonerComponent)
		{
			ClonerComponent->UpdateClonerAttachmentTree();
			ClonerComponent->UpdateClonerRenderState();
		}
	}

	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;
		RequestClonerUpdate(true);
	}
}

void AAvaClonerActor::UpdateLayoutOptions()
{
	OnMeshRenderModeChanged();
	OnSeedChanged();

	// update layouts options
	if (UAvaClonerLayoutBase* CurrentLayout = ClonerComponent->GetClonerActiveLayout())
	{
		CurrentLayout->UpdateLayoutParameters();
	}

	OnMeshRendererOptionsChanged();
	OnDefaultMeshesChanged();
	OnDeltaStepChanged();
	OnRangeOptionsChanged();
	OnSpawnOptionsChanged();
	OnLifetimeOptionsChanged();
}

void AAvaClonerActor::UpdateClonerEffectors()
{
	Effectors.RemoveAll([](const TWeakObjectPtr<AAvaEffectorActor>& InEffector)
	{
		return !InEffector.IsValid();
	});

	if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		// Effectors could be registered before system is loaded
		const int32 EffectorCount = GetEffectorCount();

		if (LayoutSystem->GetDataInterfaces().Num() != EffectorCount)
		{
			LayoutSystem->GetDataInterfaces().Resize(EffectorCount);

			RequestClonerUpdate();
		}
	}

	ForEachEffector([this](AAvaEffectorActor* InEffector, int32 InIdx)
	{
		InEffector->OnClonerUpdated(this, InIdx);
		return true;
	});
}

void AAvaClonerActor::SetTreeUpdateInterval(float InInterval)
{
	if (InInterval == TreeUpdateInterval)
	{
		return;
	}

	TreeUpdateInterval = InInterval;
}

void AAvaClonerActor::SetMeshRenderMode(EAvaClonerMeshRenderMode InMode)
{
	if (InMode == MeshRenderMode)
	{
		return;
	}

	MeshRenderMode = InMode;
	OnMeshRenderModeChanged();
}

void AAvaClonerActor::SetMeshFacingMode(ENiagaraMeshFacingMode InMode)
{
	if (MeshFacingMode == InMode)
	{
		return;
	}

	MeshFacingMode = InMode;
	OnMeshRendererOptionsChanged();
}

void AAvaClonerActor::SetMeshCastShadows(bool InbCastShadows)
{
	if (bMeshCastShadows == InbCastShadows)
	{
		return;
	}

	bMeshCastShadows = InbCastShadows;
	OnMeshRendererOptionsChanged();
}

void AAvaClonerActor::SetDefaultMeshes(const TArray<TObjectPtr<UStaticMesh>>& InMeshes)
{
	DefaultMeshes = InMeshes;
	OnDefaultMeshesChanged();
}

void AAvaClonerActor::BP_SetDefaultMeshes(const TArray<UStaticMesh*>& InMeshes)
{
	DefaultMeshes.Empty(InMeshes.Num());
	Algo::Transform(InMeshes, DefaultMeshes, [](UStaticMesh* InMesh)->TObjectPtr<UStaticMesh>
	{
		return InMesh;
	});
	OnDefaultMeshesChanged();
}

TArray<UStaticMesh*> AAvaClonerActor::BP_GetDefaultMeshes() const
{
	TArray<UStaticMesh*> Meshes;
	Meshes.Reserve(DefaultMeshes.Num());
	Algo::Transform(DefaultMeshes, Meshes, [](const TObjectPtr<UStaticMesh>& InMesh)->UStaticMesh*
	{
		return InMesh;
	});
	return Meshes;
}

void AAvaClonerActor::SetSeed(int32 InSeed)
{
	if (InSeed == Seed)
	{
		return;
	}

	Seed = InSeed;
	OnSeedChanged();
}

void AAvaClonerActor::SetDeltaStepEnabled(bool bInEnabled)
{
	if (bDeltaStepEnabled == bInEnabled)
	{
		return;
	}

	bDeltaStepEnabled = bInEnabled;
	OnDeltaStepChanged();
}

void AAvaClonerActor::SetDeltaStepRotation(const FRotator& InRotation)
{
	if (InRotation == DeltaStepRotation)
	{
		return;
	}

	DeltaStepRotation = InRotation;
	OnDeltaStepChanged();
}

void AAvaClonerActor::SetDeltaStepScale(const FVector& InScale)
{
	if (InScale == DeltaStepScale)
	{
		return;
	}

	DeltaStepScale = InScale;
	OnDeltaStepChanged();
}

void AAvaClonerActor::SetEnabled(bool bInEnable)
{
	if (bInEnable == bEnabled)
	{
		return;
	}

	bEnabled = bInEnable;
	OnEnabledChanged();
}

void AAvaClonerActor::SetInvertProgress(bool bInInvertProgress)
{
	if (bInvertProgress == bInInvertProgress)
	{
		return;
	}

	bInvertProgress = bInInvertProgress;
	OnProgressChanged();
}

void AAvaClonerActor::SetProgress(float InProgress)
{
	if (Progress == InProgress)
	{
		return;
	}

	if (InProgress < 0.f || InProgress > 1.f)
	{
		return;
	}

	Progress = InProgress;
	OnProgressChanged();
}

void AAvaClonerActor::SetLayoutName(FName InLayoutName)
{
	if (LayoutName == InLayoutName)
	{
		return;
	}

	const TArray<FString> LayoutNames = GetClonerLayoutNames();
	if (!LayoutNames.Contains(InLayoutName))
	{
		return;
	}

	LayoutName = InLayoutName;
	OnLayoutNameChanged();
}

void AAvaClonerActor::SetRangeEnabled(bool bInRangeEnabled)
{
	if (bRangeEnabled == bInRangeEnabled)
	{
		return;
	}

	bRangeEnabled = bInRangeEnabled;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeOffsetMin(const FVector& InRangeOffsetMin)
{
	if (RangeOffsetMin == InRangeOffsetMin)
	{
		return;
	}

	RangeOffsetMin = InRangeOffsetMin;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeOffsetMax(const FVector& InRangeOffsetMax)
{
	if (RangeOffsetMax == InRangeOffsetMax)
	{
		return;
	}

	RangeOffsetMax = InRangeOffsetMax;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeRotationMin(const FRotator& InRangeRotationMin)
{
	if (RangeRotationMin == InRangeRotationMin)
	{
		return;
	}

	RangeRotationMin = InRangeRotationMin;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeRotationMax(const FRotator& InRangeRotationMax)
{
	if (RangeRotationMax == InRangeRotationMax)
	{
		return;
	}

	RangeRotationMax = InRangeRotationMax;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeScaleUniform(bool bInRangeScaleUniform)
{
	if (bRangeScaleUniform == bInRangeScaleUniform)
	{
		return;
	}

	bRangeScaleUniform = bInRangeScaleUniform;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeScaleMin(const FVector& InRangeScaleMin)
{
	if (RangeScaleMin == InRangeScaleMin)
	{
		return;
	}

	RangeScaleMin = InRangeScaleMin;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeScaleMax(const FVector& InRangeScaleMax)
{
	if (RangeScaleMax == InRangeScaleMax)
	{
		return;
	}

	RangeScaleMax = InRangeScaleMax;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeScaleUniformMin(float InRangeScaleUniformMin)
{
	if (RangeScaleUniformMin == InRangeScaleUniformMin)
	{
		return;
	}

	RangeScaleUniformMin = InRangeScaleUniformMin;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::SetRangeScaleUniformMax(float InRangeScaleUniformMax)
{
	if (RangeScaleUniformMax == InRangeScaleUniformMax)
	{
		return;
	}

	RangeScaleUniformMax = InRangeScaleUniformMax;
	OnRangeOptionsChanged();
}

void AAvaClonerActor::OnMeshRenderModeChanged()
{
	if (!bEnabled)
	{
		return;
	}

	if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		FNiagaraUserRedirectionParameterStore& ExposedParameters = LayoutSystem->GetSystem()->GetExposedParameters();

		static const FNiagaraVariable MeshModeVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerMeshRenderMode>()), TEXT("MeshRenderMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(MeshRenderMode), MeshModeVar);

		RequestClonerUpdate();
	}
}

void AAvaClonerActor::OnDeltaStepChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	if (bDeltaStepEnabled)
	{
		ClonerComponent->SetVectorParameter(TEXT("DeltaStepRotation"), FVector(DeltaStepRotation.Roll, DeltaStepRotation.Pitch, DeltaStepRotation.Yaw));

		ClonerComponent->SetVectorParameter(TEXT("DeltaStepScale"), DeltaStepScale);
	}
	else
	{
		ClonerComponent->SetVectorParameter(TEXT("DeltaStepRotation"), FVector::ZeroVector);

		ClonerComponent->SetVectorParameter(TEXT("DeltaStepScale"), FVector::ZeroVector);
	}

	RequestClonerUpdate();
}

void AAvaClonerActor::OnSeedChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	ClonerComponent->SetRandomSeedOffset(Seed);

	RequestClonerUpdate();
}

void AAvaClonerActor::OnProgressChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	ClonerComponent->SetFloatParameter(TEXT("ParticleProgress"), Progress * (bInvertProgress ? -1 : 1));
}

void AAvaClonerActor::SetSpawnLoopMode(EAvaClonerSpawnLoopMode InMode)
{
	if (SpawnLoopMode == InMode)
	{
		return;
	}

	SpawnLoopMode = InMode;
	OnSpawnOptionsChanged();
}

void AAvaClonerActor::SetSpawnLoopIterations(int32 InIterations)
{
	if (SpawnLoopIterations == InIterations)
	{
		return;
	}

	if (InIterations < 1)
	{
		return;
	}

	SpawnLoopIterations = InIterations;
	OnSpawnOptionsChanged();
}

void AAvaClonerActor::SetSpawnLoopInterval(float InInterval)
{
	if (SpawnLoopInterval == InInterval)
	{
		return;
	}

	if (InInterval < 0.f)
	{
		return;
	}

	SpawnLoopInterval = InInterval;
	OnSpawnOptionsChanged();
}

void AAvaClonerActor::SetSpawnBehaviorMode(EAvaClonerSpawnBehaviorMode InMode)
{
	if (SpawnBehaviorMode == InMode)
	{
		return;
	}

	SpawnBehaviorMode = InMode;
	OnSpawnOptionsChanged();
}

void AAvaClonerActor::SetSpawnRate(float InRate)
{
	if (SpawnRate == InRate)
	{
		return;
	}

	if (InRate < 0)
	{
		return;
	}

	SpawnRate = InRate;
	OnSpawnOptionsChanged();
}

void AAvaClonerActor::SetLifetimeEnabled(bool bInEnabled)
{
	if (bLifetimeEnabled == bInEnabled)
	{
		return;
	}

	bLifetimeEnabled = bInEnabled;
	OnLifetimeOptionsChanged();
}

void AAvaClonerActor::SetLifetimeMin(float InMin)
{
	if (LifetimeMin == InMin)
	{
		return;
	}

	if (InMin < 0)
	{
		return;
	}

	LifetimeMin = InMin;
	OnLifetimeOptionsChanged();
}

void AAvaClonerActor::SetLifetimeMax(float InMax)
{
	if (LifetimeMax == InMax)
	{
		return;
	}

	if (InMax < 0)
	{
		return;
	}

	LifetimeMax = InMax;
	OnLifetimeOptionsChanged();
}

void AAvaClonerActor::SetLifetimeScaleEnabled(bool bInEnabled)
{
	if (bLifetimeScaleEnabled == bInEnabled)
	{
		return;
	}

	bLifetimeScaleEnabled = bInEnabled;
	OnLifetimeOptionsChanged();
}

void AAvaClonerActor::SetLifetimeScaleCurve(const FRichCurve& InCurve)
{
	LifetimeScaleCurve = InCurve;
	OnLifetimeOptionsChanged();
}

UAvaClonerLayoutBase* AAvaClonerActor::GetActiveLayout() const
{
	if (ClonerComponent)
	{
		return ClonerComponent->GetClonerActiveLayout();
	}

	return nullptr;
}

const FAvaClonerEffectorDataInterfaces* AAvaClonerActor::GetEffectorDataInterfaces() const
{
	if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		return &LayoutSystem->GetDataInterfaces();
	}

	return nullptr;
}

int32 AAvaClonerActor::GetMeshCount() const
{
	if (ClonerComponent)
	{
		if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
		{
			return LayoutSystem->GetMeshRenderer()->Meshes.Num();
		}
	}
	return 0;
}

int32 AAvaClonerActor::RegisterEffector(AAvaEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return INDEX_NONE;
	}

	int32 Index = Effectors.Find(InEffector);

	if (Index != INDEX_NONE)
	{
		return Index;
	}

	Index = Effectors.Add(InEffector);

	if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		const int32 EffectorCount = GetEffectorCount();
		LayoutSystem->GetDataInterfaces().Resize(EffectorCount);
	}

	InEffector->OnClonerLinked(this, Index);

	constexpr bool bImmediateUpdate = true;
	RequestClonerUpdate(bImmediateUpdate);

	return Index;
}

bool AAvaClonerActor::UnregisterEffector(AAvaEffectorActor* InEffector)
{
	if (!InEffector)
	{
		return false;
	}

	const int32 OldIndex = Effectors.Find(InEffector);

	if (OldIndex == INDEX_NONE)
	{
		return false;
	}

	Effectors.RemoveAt(OldIndex);

	if (const UAvaClonerLayoutBase* LayoutSystem = GetActiveLayout())
	{
		LayoutSystem->GetDataInterfaces().Remove(OldIndex);
	}

	InEffector->OnClonerUnlinked(this, OldIndex);

	constexpr bool bImmediateUpdate = true;
	RequestClonerUpdate(bImmediateUpdate);

	return true;
}

bool AAvaClonerActor::IsEffectorRegistered(const AAvaEffectorActor* InEffector) const
{
	return InEffector && Effectors.Contains(InEffector);
}

int32 AAvaClonerActor::GetEffectorIndex(AAvaEffectorActor* InEffector) const
{
	return InEffector ? Effectors.Find(InEffector) : INDEX_NONE;
}

int32 AAvaClonerActor::GetEffectorCount() const
{
	return Effectors.Num();
}

void AAvaClonerActor::ForEachEffector(TFunctionRef<bool(AAvaEffectorActor*, int32)> InFunction)
{
	for (int32 Idx = 0; Idx < Effectors.Num(); Idx++)
	{
		AAvaEffectorActor* Effector = Effectors[Idx].Get();
		if (!Effector)
		{
			continue;
		}

		if (!InFunction(Effector, Idx))
		{
			return;
		}
	}
}

void AAvaClonerActor::OnEnabledChanged()
{
	if (bEnabled)
	{
		OnLayoutNameChanged();
	}
	else
	{
		ClonerComponent->DeactivateImmediate();
		ClonerComponent->SetAsset(nullptr);
	}
}

void AAvaClonerActor::OnLayoutNameChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	UAvaClonerLayoutBase* NewActiveLayout = FindOrAddLayout(LayoutName);

	// Apply layout
	if (ClonerComponent->SetClonerActiveLayout(NewActiveLayout))
	{
		ActiveLayout = ClonerComponent->GetClonerActiveLayout();
		OnClonerSystemChanged();
	}
}

void AAvaClonerActor::OnSpawnOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	if (const UAvaClonerLayoutBase* ActiveSystem = GetActiveLayout())
	{
		FNiagaraUserRedirectionParameterStore& ExposedParameters = ActiveSystem->GetSystem()->GetExposedParameters();

		ClonerComponent->SetFloatParameter(TEXT("SpawnLoopInterval"), SpawnLoopInterval);

		ClonerComponent->SetIntParameter(TEXT("SpawnLoopIterations"), SpawnLoopIterations);

		ClonerComponent->SetFloatParameter(TEXT("SpawnRate"), SpawnRate);

		const FNiagaraVariable SpawnBehaviorModeVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerSpawnBehaviorMode>()), TEXT("SpawnBehaviorMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnBehaviorMode), SpawnBehaviorModeVar);

		const FNiagaraVariable SpawnLoopModeVar(FNiagaraTypeDefinition(StaticEnum<EAvaClonerSpawnLoopMode>()), TEXT("SpawnLoopMode"));
		ExposedParameters.SetParameterValue<int32>(static_cast<int32>(SpawnLoopMode), SpawnLoopModeVar);

		RequestClonerUpdate();
	}
}

void AAvaClonerActor::OnLifetimeScaleCurveChanged()
{
	if (const UNiagaraDataInterfaceCurve* LifetimeCurve = LifetimeScaleCurveDIWeak.Get())
	{
		LifetimeScaleCurve = LifetimeCurve->Curve;
		OnLifetimeOptionsChanged();
	}
}

void AAvaClonerActor::OnLifetimeOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	LifetimeMin = FMath::Max(0, LifetimeMin);
	LifetimeMax = FMath::Max(LifetimeMin, LifetimeMax);

	ClonerComponent->SetBoolParameter(TEXT("LifetimeEnabled"), bLifetimeEnabled);

	ClonerComponent->SetFloatParameter(TEXT("LifetimeMin"), LifetimeMin);

	ClonerComponent->SetFloatParameter(TEXT("LifetimeMax"), LifetimeMax);

	ClonerComponent->SetBoolParameter(TEXT("LifetimeScaleEnabled"), bLifetimeEnabled && bLifetimeScaleEnabled);

	if (const UAvaClonerLayoutBase* ActiveSystem = GetActiveLayout())
	{
		const FNiagaraUserRedirectionParameterStore& ExposedParameters = ActiveSystem->GetSystem()->GetExposedParameters();

		static const FNiagaraVariable LifetimeScaleCurveVar(FNiagaraTypeDefinition(UNiagaraDataInterfaceCurve::StaticClass()), TEXT("LifetimeScaleCurve"));

#if WITH_EDITOR
		if (UNiagaraDataInterfaceCurve* LifetimeCurve = LifetimeScaleCurveDIWeak.Get())
		{
			LifetimeCurve->OnChanged().RemoveAll(this);
		}
#endif

		if (UNiagaraDataInterfaceCurve* LifetimeScaleCurveDI = Cast<UNiagaraDataInterfaceCurve>(ExposedParameters.GetDataInterface(LifetimeScaleCurveVar)))
		{
			LifetimeScaleCurveDIWeak = LifetimeScaleCurveDI;
			LifetimeScaleCurveDI->Curve = LifetimeScaleCurve;

#if WITH_EDITOR
			LifetimeScaleCurveDI->UpdateLUT();
			LifetimeScaleCurveDI->OnChanged().AddUObject(this, &AAvaClonerActor::OnLifetimeScaleCurveChanged);
#endif
		}
	}

	RequestClonerUpdate();
}

void AAvaClonerActor::OnClonerTransformed(USceneComponent*, EUpdateTransformFlags, ETeleportType)
{
	UpdateClonerEffectors();
}

void AAvaClonerActor::OnClonerMeshUpdated()
{
	RequestClonerUpdate();
}

void AAvaClonerActor::OnClonerSystemChanged()
{
	UpdateLayoutOptions();
	UpdateClonerEffectors();
}

void AAvaClonerActor::RequestClonerUpdate(bool bInImmediate)
{
	if (!bEnabled || !ClonerComponent)
	{
		return;
	}

	if (bInImmediate)
	{
		bNeedsRefresh = false;
		ClonerComponent->RefreshUserParameters();
	}
	else
	{
		bNeedsRefresh = true;
	}
}

void AAvaClonerActor::OnRangeOptionsChanged()
{
	if (!ClonerComponent || !bEnabled)
	{
		return;
	}

	RangeScaleUniformMin = FMath::Clamp(RangeScaleUniformMin, UE_KINDA_SMALL_NUMBER, RangeScaleUniformMax);
	RangeScaleUniformMax = FMath::Max3(RangeScaleUniformMin, RangeScaleUniformMax, UE_KINDA_SMALL_NUMBER);

	RangeScaleMin.X = FMath::Clamp(RangeScaleMin.X, UE_KINDA_SMALL_NUMBER, RangeScaleMax.X);
	RangeScaleMin.Y = FMath::Clamp(RangeScaleMin.Y, UE_KINDA_SMALL_NUMBER, RangeScaleMax.Y);
	RangeScaleMin.Z = FMath::Clamp(RangeScaleMin.Z, UE_KINDA_SMALL_NUMBER, RangeScaleMax.Z);

	RangeScaleMax.X = FMath::Max3<double>(RangeScaleMin.X, RangeScaleMax.X, UE_KINDA_SMALL_NUMBER);
	RangeScaleMax.Y = FMath::Max3<double>(RangeScaleMin.Y, RangeScaleMax.Y, UE_KINDA_SMALL_NUMBER);
	RangeScaleMax.Z = FMath::Max3<double>(RangeScaleMin.Z, RangeScaleMax.Z, UE_KINDA_SMALL_NUMBER);

	ClonerComponent->SetBoolParameter(TEXT("RangeEnabled"), bRangeEnabled);

	ClonerComponent->SetVectorParameter(TEXT("RangeOffsetMin"), RangeOffsetMin);

	ClonerComponent->SetVectorParameter(TEXT("RangeOffsetMax"), RangeOffsetMax);

	ClonerComponent->SetVariableQuat(TEXT("RangeRotationMin"), RangeRotationMin.Quaternion());

	ClonerComponent->SetVariableQuat(TEXT("RangeRotationMax"), RangeRotationMax.Quaternion());

	ClonerComponent->SetBoolParameter(TEXT("RangeScaleUniform"), bRangeScaleUniform);

	ClonerComponent->SetVectorParameter(TEXT("RangeScaleMin"), RangeScaleMin);

	ClonerComponent->SetVectorParameter(TEXT("RangeScaleMax"), RangeScaleMax);

	ClonerComponent->SetFloatParameter(TEXT("RangeScaleUniformMin"), RangeScaleUniformMin);

	ClonerComponent->SetFloatParameter(TEXT("RangeScaleUniformMax"), RangeScaleUniformMax);

	RequestClonerUpdate();
}

bool AAvaClonerActor::IsClonerValid() const
{
	return IsValid(ClonerComponent.Get()) && IsValid(ClonerComponent->GetAsset());
}

void AAvaClonerActor::OnMeshRendererOptionsChanged()
{
	const UAvaClonerLayoutBase* CurrentLayout = ClonerComponent->GetClonerActiveLayout();

	if (!CurrentLayout)
	{
		return;
	}

	UNiagaraMeshRendererProperties* MeshRenderer = CurrentLayout->GetMeshRenderer();

	if (!MeshRenderer)
	{
		return;
	}

	MeshRenderer->FacingMode = MeshFacingMode;
	MeshRenderer->bCastShadows = bMeshCastShadows;

	RequestClonerUpdate();
}

void AAvaClonerActor::OnDefaultMeshesChanged()
{
	// Will force an update
	TreeUpdateDeltaTime = TreeUpdateInterval;
}

#if WITH_EDITOR
void AAvaClonerActor::OnReduceMotionGhostingChanged()
{
	if (UAvaClonerSubsystem* ClonerSubsystem = UAvaClonerSubsystem::Get())
	{
		if (bReduceMotionGhosting)
		{
			ClonerSubsystem->EnableNoFlicker();
		}
		else
		{
			ClonerSubsystem->DisableNoFlicker();
		}
	}
}

void AAvaClonerActor::OnCVarChanged()
{
	if (const UAvaClonerSubsystem* ClonerSubsystem = UAvaClonerSubsystem::Get())
	{
		bReduceMotionGhosting = ClonerSubsystem->IsNoFlickerEnabled();
	}
}

void AAvaClonerActor::ForceUpdateCloner()
{
	if (ClonerComponent)
	{
		constexpr bool bReset = true;
		ClonerComponent->UpdateClonerAttachmentTree(bReset);
		TreeUpdateDeltaTime = TreeUpdateInterval;
	}
}

void AAvaClonerActor::SpawnLinkedEffector()
{
	UWorld* ClonerWorld = GetWorld();
	if (!ClonerWorld)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.bTemporaryEditorActor = false;

	const FVector ClonerLocation = GetActorLocation();
	const FRotator ClonerRotation = GetActorRotation();

	AAvaEffectorActor* EffectorActor = ClonerWorld->SpawnActor<AAvaEffectorActor>(AAvaEffectorActor::StaticClass(), ClonerLocation, ClonerRotation, Params);

	if (!EffectorActor)
	{
		return;
	}

	EffectorActor->LinkCloner(this);
	FActorLabelUtilities::RenameExistingActor(EffectorActor, EffectorActor->GetDefaultActorLabel(), true);

	// Set default offset for visual feedback
	EffectorActor->SetOffset(FVector(0, 0, 100));
}

void AAvaClonerActor::SpawnDefaultActorAttached()
{
	// Only spawn if world is valid and not a preview actor
	UWorld* World = GetWorld();
	if (!World || bIsEditorPreviewActor)
	{
		return;
	}

	// Find or load cube mesh
	constexpr const TCHAR* DefaultStaticMeshPath = TEXT("/Script/Engine.StaticMesh'/Engine/BasicShapes/Cube.Cube'");
	UStaticMesh* DefaultStaticMesh = LoadObject<UStaticMesh>(nullptr, DefaultStaticMeshPath);

	if (!DefaultStaticMesh)
	{
		return;
	}

	// Spawn attached actor with same flags as this actor
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.ObjectFlags = GetFlags();
	SpawnParameters.bTemporaryEditorActor = false;

	if (AStaticMeshActor* DefaultActorAttached = World->SpawnActor<AStaticMeshActor>(GetActorLocation(), GetActorRotation(), SpawnParameters))
	{
		UStaticMeshComponent* StaticMeshComponent = DefaultActorAttached->GetStaticMeshComponent();
		StaticMeshComponent->SetStaticMesh(DefaultStaticMesh);

		DefaultActorAttached->SetMobility(EComponentMobility::Movable);
		DefaultActorAttached->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

		FActorLabelUtilities::SetActorLabelUnique(DefaultActorAttached, TEXT("DefaultCube"));
	}
}

void AAvaClonerActor::OnEditorSelectionChanged(UObject* InSelection)
{
	if (const USelection* ActorSelection = Cast<USelection>(InSelection))
	{
		if (ULevelEditorViewportSettings* ViewportSettings = GetMutableDefault<ULevelEditorViewportSettings>())
		{
			if (ActorSelection->Num() == 1 && ActorSelection->GetSelectedObject(0) == this)
			{
				UseSelectionOutline = ViewportSettings->bUseSelectionOutline;
				ViewportSettings->bUseSelectionOutline = false;
			}
			else if (UseSelectionOutline.IsSet())
			{
				ViewportSettings->bUseSelectionOutline = UseSelectionOutline.GetValue();
				UseSelectionOutline.Reset();
			}
		}
	}
}
#endif

void AAvaClonerActor::OnVisualizerSpriteVisibleChanged()
{
#if WITH_EDITOR
	UE::ClonerEffector::SetBillboardComponentSprite(this, TEXT("/Script/Engine.Texture2D'/Avalanche/ClonerResources/Textures/T_ClonerIcon.T_ClonerIcon'"));
	UE::ClonerEffector::SetBillboardComponentVisibility(this, bVisualizerSpriteVisible);
#endif
}

TArray<FString> AAvaClonerActor::GetClonerLayoutNames() const
{
	TArray<FString> LayoutNamesStrings;

	if (const UAvaClonerSubsystem* Subsystem = UAvaClonerSubsystem::Get())
	{
		const TArray<FName>& LayoutNames = Subsystem->GetLayoutNames();
		LayoutNamesStrings.Reserve(LayoutNames.Num());

		Algo::Transform(LayoutNames, LayoutNamesStrings, [](const FName& InName)
		{
			return InName.ToString();
		});
	}

	return LayoutNamesStrings;
}

UAvaClonerLayoutBase* AAvaClonerActor::FindOrAddLayout(TSubclassOf<UAvaClonerLayoutBase> InClass)
{
	const UAvaClonerSubsystem* Subsystem = UAvaClonerSubsystem::Get();

	if (!Subsystem)
	{
		return nullptr;
	}

	const FName ClassLayoutName = Subsystem->FindLayoutName(InClass);

	if (ClassLayoutName.IsNone())
	{
		return nullptr;
	}

	return FindOrAddLayout(ClassLayoutName);
}

UAvaClonerLayoutBase* AAvaClonerActor::FindOrAddLayout(FName InLayoutName)
{
	UAvaClonerSubsystem* Subsystem = UAvaClonerSubsystem::Get();
	if (!Subsystem)
	{
		return nullptr;
	}

	// Check cached layout instances
	UAvaClonerLayoutBase* NewActiveLayout = nullptr;
	if (TObjectPtr<UAvaClonerLayoutBase> const* LayoutInstance = LayoutInstances.Find(InLayoutName))
	{
		NewActiveLayout = *LayoutInstance;
	}

	// Create new layout instance and cache it
	if (!NewActiveLayout)
	{
		NewActiveLayout = Subsystem->CreateNewLayout(InLayoutName, this);
		LayoutInstances.Add(InLayoutName, NewActiveLayout);
	}

	return NewActiveLayout;
}

void AAvaClonerActor::InitializeCloner()
{
	if (bClonerInitialized)
	{
		return;
	}

	OnVisualizerSpriteVisibleChanged();

#if WITH_EDITOR
	OnReduceMotionGhostingChanged();
#endif

	// For new cloner instances no need to migrate anything
	bDeprecatedPropertiesMigrated = true;

	bClonerInitialized = true;

	OnLayoutNameChanged();
}

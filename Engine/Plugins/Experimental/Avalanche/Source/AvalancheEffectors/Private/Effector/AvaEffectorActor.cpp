// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/AvaEffectorActor.h"

#include "Cloner/AvaClonerActor.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Effector/AvaEffectorComponent.h"
#include "Math/Vector.h"
#include "Subsystems/AvaEffectorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvalancheEffector, Log, All);

AAvaEffectorActor::AAvaEffectorActor()
{
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneComponent = CreateDefaultSubobject<UAvaEffectorComponent>(TEXT("AvaEffectorComponent"));
#if WITH_EDITORONLY_DATA
	SceneComponent->bVisualizeComponent = true;
#endif
	SetRootComponent(SceneComponent);

	// Sphere 
	InnerSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AvaInnerSphereComponent"));;
	InnerSphereComponent->ShapeColor = FColor::Blue;
	InnerSphereComponent->SetLineThickness(VisualizerThickness);
	InnerSphereComponent->SetSphereRadius(InnerRadius);
	InnerSphereComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerSphereComponent->SetIsVisualizationComponent(true);
#endif
	InnerSphereComponent->bIsEditorOnly = false;
	InnerSphereComponent->SetupAttachment(SceneComponent);

	OuterSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("AvaOuterSphereComponent"));
	OuterSphereComponent->ShapeColor = FColor::Red;
	OuterSphereComponent->SetLineThickness(VisualizerThickness);
	OuterSphereComponent->SetSphereRadius(OuterRadius);
	OuterSphereComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterSphereComponent->SetIsVisualizationComponent(true);
#endif
	OuterSphereComponent->bIsEditorOnly = false;
	OuterSphereComponent->SetupAttachment(SceneComponent);

	// Box
	InnerBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaInnerBoxComponent"));
	InnerBoxComponent->ShapeColor = FColor::Blue;
	InnerBoxComponent->SetLineThickness(VisualizerThickness);
	InnerBoxComponent->SetBoxExtent(InnerExtent);
	InnerBoxComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerBoxComponent->SetIsVisualizationComponent(true);
#endif
	InnerBoxComponent->bIsEditorOnly = false;
	InnerBoxComponent->SetupAttachment(SceneComponent);

	OuterBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaOuterBoxComponent"));
	OuterBoxComponent->ShapeColor = FColor::Red;
	OuterBoxComponent->SetLineThickness(VisualizerThickness);
	OuterBoxComponent->SetBoxExtent(OuterExtent);
	OuterBoxComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterBoxComponent->SetIsVisualizationComponent(true);
#endif
	OuterBoxComponent->bIsEditorOnly = false;
	OuterBoxComponent->SetupAttachment(SceneComponent);

	// Plane
	InnerPlaneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaInnerPlaneComponent"));
	InnerPlaneComponent->ShapeColor = FColor::Blue;
	InnerPlaneComponent->SetLineThickness(VisualizerThickness);
	InnerPlaneComponent->SetBoxExtent(InnerExtent);
	InnerPlaneComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerPlaneComponent->SetIsVisualizationComponent(true);
#endif
	InnerPlaneComponent->bIsEditorOnly = false;
	InnerPlaneComponent->SetupAttachment(SceneComponent);

	OuterPlaneComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("AvaOuterPlaneComponent"));
	OuterPlaneComponent->ShapeColor = FColor::Red;
	OuterPlaneComponent->SetLineThickness(VisualizerThickness);
	OuterPlaneComponent->SetBoxExtent(OuterExtent);
	OuterPlaneComponent->SetHiddenInGame(true);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterPlaneComponent->SetIsVisualizationComponent(true);
#endif
	OuterPlaneComponent->bIsEditorOnly = false;
	OuterPlaneComponent->SetupAttachment(SceneComponent);

	if (!IsTemplate())
	{
		SceneComponent->TransformUpdated.AddUObject(this, &AAvaEffectorActor::OnEffectorTransformed);

		RegisterToChannel();
	}
}

#if WITH_EDITOR
FString AAvaEffectorActor::GetDefaultActorLabel() const
{
	return DefaultLabel;
}

TAvaPropertyChangeDispatcher<AAvaEffectorActor> AAvaEffectorActor::PropertyChangeDispatcher =
{
	/** Effector */
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bEnabled), &AAvaEffectorActor::OnEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Magnitude), &AAvaEffectorActor::OnMagnitudeChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Cloners), &AAvaEffectorActor::OnClonersChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, VisualizerThickness), &AAvaEffectorActor::OnVisualizerThicknessChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bVisualizerSpriteVisible), &AAvaEffectorActor::OnVisualizerSpriteVisibleChanged },
	/** Type */
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Type), &AAvaEffectorActor::OnTypeChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Easing), &AAvaEffectorActor::OnEasingChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OuterRadius), &AAvaEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, InnerRadius), &AAvaEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, InnerExtent), &AAvaEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OuterExtent), &AAvaEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, PlaneSpacing), &AAvaEffectorActor::OnPlaneChanged },
	/** Mode */
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Mode), &AAvaEffectorActor::OnModeChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Offset), &AAvaEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Rotation), &AAvaEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Scale), &AAvaEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, TargetActorWeak), &AAvaEffectorActor::OnTargetActorChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, LocationStrength), &AAvaEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, RotationStrength), &AAvaEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, ScaleStrength), &AAvaEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Pan), &AAvaEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, Frequency), &AAvaEffectorActor::OnNoiseFieldOptionsChanged },
	/** Force */
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bOrientationForceEnabled), &AAvaEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OrientationForceRate), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OrientationForceMin), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OrientationForceMax), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bVortexForceEnabled), &AAvaEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, VortexForceAmount), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, VortexForceAxis), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bCurlNoiseForceEnabled), &AAvaEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, CurlNoiseForceStrength), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, CurlNoiseForceFrequency), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bAttractionForceEnabled), &AAvaEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, AttractionForceStrength), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, AttractionForceFalloff), &AAvaEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, bGravityForceEnabled), &AAvaEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, GravityForceAcceleration), &AAvaEffectorActor::OnForceOptionsChanged },
};

void AAvaEffectorActor::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}

void AAvaEffectorActor::PostEditImport()
{
	Super::PostEditImport();

	RegisterToCloners();
}

void AAvaEffectorActor::PostEditUndo()
{
	Super::PostEditUndo();

	RegisterToChannel();

	OnClonersChanged();

	// Undo causes cloner to blackout, so refresh state
	ForEachCloner([](AAvaClonerActor* InCloner, int32 InIndex)
	{
		InCloner->ForceUpdateCloner();
		return true;
	});
}
#endif

void AAvaEffectorActor::Destroyed()
{
	Super::Destroyed();

	// Remove this effector from the effector channel
	if (UAvaEffectorSubsystem* EffectorSubsystem = UAvaEffectorSubsystem::Get(GetWorld()))
	{
		EffectorSubsystem->UnregisterChannelEffector(this);
	}

	// Remove this effector from the cloners it is linked to
	ForEachCloner([this](AAvaClonerActor* InCloner, int32 InIndex)
	{
		InCloner->UnregisterEffector(this);
		return true;
	});
}

void AAvaEffectorActor::PostActorCreated()
{
	Super::PostActorCreated();

	OnEffectorChanged();
}

void AAvaEffectorActor::PostLoad()
{
	Super::PostLoad();

	RegisterToCloners();
}

void AAvaEffectorActor::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	RegisterToCloners();
}

void AAvaEffectorActor::LinkCloner(AAvaClonerActor* InCloner)
{
	if (!InCloner || Cloners.Contains(InCloner))
	{
		return;
	}

	Cloners.Add(InCloner);
	OnClonersChanged();
}

void AAvaEffectorActor::UnlinkCloner(AAvaClonerActor* InCloner)
{
	if (!InCloner || !Cloners.Contains(InCloner))
	{
		return;
	}

	Cloners.Remove(InCloner);
	OnClonersChanged();
}

void AAvaEffectorActor::OnEffectorIdentifierChanged()
{
	int32 Identifier = ChannelData.GetIdentifier();

	if (Identifier == INDEX_NONE)
	{
		UE_LOG(LogAvalancheEffector, Log, TEXT("%s : Effector channel identifier is invalid"), *GetActorNameOrLabel());
		return;
	}

	// Update effector identifier in linked cloners
	ForEachCloner([Identifier](AAvaClonerActor* InCloner, int32 InEffectorIndex)
	{
		if (const FAvaClonerEffectorDataInterfaces* DataInterfaces = InCloner->GetEffectorDataInterfaces())
		{
			UNiagaraDataInterfaceArrayInt32* EffectorIndexDI = DataInterfaces->GetIndexArray();

			// This effector has a specific index in the cloner
			if (EffectorIndexDI->GetArrayReference().IsValidIndex(InEffectorIndex))
			{
				const int32 PrevIdentifier = EffectorIndexDI->GetArrayReference()[InEffectorIndex];

				// Only update if identifier is different
				if (PrevIdentifier != Identifier)
				{
					EffectorIndexDI->GetArrayReference()[InEffectorIndex] = Identifier;

					constexpr bool bImmediateUpdate = true;
					InCloner->RequestClonerUpdate(bImmediateUpdate);
				}
			}
		}

		return true;
	});
}

void AAvaEffectorActor::SetType(EAvaClonerEffectorType InType)
{
	if (InType == Type)
	{
		return;
	}

	Type = InType;
	OnTypeChanged();
}

void AAvaEffectorActor::SetEasing(EAvaClonerEasing InEasing)
{
	if (InEasing == Easing)
	{
		return;
	}

	Easing = InEasing;
	OnEasingChanged();
}

void AAvaEffectorActor::SetMagnitude(float InMagnitude)
{
	if (FMath::IsNearlyEqual(InMagnitude, Magnitude))
	{
		return;
	}

	if (InMagnitude < 0.f || InMagnitude > 1.f)
	{
		return;
	}

	Magnitude = InMagnitude;
	OnMagnitudeChanged();
}

void AAvaEffectorActor::SetOuterRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, OuterRadius))
	{
		return;
	}

	if (InRadius < 0.f || InRadius < InnerRadius)
	{
		return;
	}

	OuterRadius = InRadius;
	OnSphereChanged();
}

void AAvaEffectorActor::SetInnerRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, InnerRadius))
	{
		return;
	}

	if (InRadius < 0.f || InRadius > OuterRadius)
	{
		return;
	}

	InnerRadius = InRadius;
	OnSphereChanged();
}

void AAvaEffectorActor::SetInnerExtent(const FVector& InExtent)
{
	if (InExtent.Equals(InnerExtent))
	{
		return;
	}

	if (InExtent.X < 0 || InExtent.Y < 0 || InExtent.Z < 0)
	{
		return;
	}

	InnerExtent = InExtent;
	OnBoxChanged();
}

void AAvaEffectorActor::SetOuterExtent(const FVector& InExtent)
{
	if (InExtent.Equals(OuterExtent))
	{
		return;
	}

	if (InExtent.X < 0 || InExtent.Y < 0 || InExtent.Z < 0)
	{
		return;
	}

	OuterExtent = InExtent;
	OnBoxChanged();
}

void AAvaEffectorActor::SetPlaneSpacing(float InSpacing)
{
	if (FMath::IsNearlyEqual(InSpacing, PlaneSpacing))
	{
		return;
	}

	if (InSpacing < 0.f)
	{
		return;
	}

	PlaneSpacing = InSpacing;
	OnPlaneChanged();
}

void AAvaEffectorActor::SetOffset(const FVector& InOffset)
{
	if (InOffset.Equals(Offset))
	{
		return;
	}

	Offset = InOffset;
	OnTransformOptionsChanged();
}

void AAvaEffectorActor::SetRotation(const FRotator& InRotation)
{
	if (InRotation.Equals(Rotation))
	{
		return;
	}

	constexpr float MinRotation = -180.f;
	if (InRotation.Pitch < MinRotation || InRotation.Roll < MinRotation || InRotation.Yaw < MinRotation)
	{
		return;
	}

	constexpr float MaxRotation = 180.f;
	if (InRotation.Pitch > MaxRotation || InRotation.Roll > MaxRotation || InRotation.Yaw > MaxRotation)
	{
		return;
	}

	Rotation = InRotation;
	OnTransformOptionsChanged();
}

void AAvaEffectorActor::SetScale(const FVector& InScale)
{
	if (InScale.Equals(Scale))
	{
		return;
	}

	if (InScale.X < 0.f || InScale.Y < 0.f || InScale.Z < 0.f)
	{
		return;
	}

	Scale = InScale;
	OnTransformOptionsChanged();
}

void AAvaEffectorActor::SetEnabled(bool bInEnable)
{
	if (bInEnable == bEnabled)
	{
		return;
	}

	bEnabled = bInEnable;
	OnEnabledChanged();
}

void AAvaEffectorActor::SetVisualizerThickness(float InThickness)
{
	if (FMath::IsNearlyEqual(VisualizerThickness, InThickness))
	{
		return;
	}

	if (VisualizerThickness < 0.1f || VisualizerThickness > 10.f)
	{
		return;
	}

	VisualizerThickness = InThickness;
	OnVisualizerThicknessChanged();
}

void AAvaEffectorActor::SetMode(EAvaClonerEffectorMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void AAvaEffectorActor::SetTargetActor(AActor* InTargetActor)
{
	if (InTargetActor == TargetActorWeak.Get())
	{
		return;
	}

	TargetActorWeak = InTargetActor;
	OnTargetActorChanged();
}

void AAvaEffectorActor::SetAttractionForceStrength(float InForceStrength)
{
	if (FMath::IsNearlyEqual(AttractionForceStrength, InForceStrength))
	{
		return;
	}

	AttractionForceStrength = InForceStrength;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetAttractionForceFalloff(float InForceFalloff)
{
	if (FMath::IsNearlyEqual(AttractionForceFalloff, InForceFalloff))
	{
		return;
	}

	AttractionForceFalloff = InForceFalloff;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetGravityForceEnabled(bool bInForceEnabled)
{
	if (bGravityForceEnabled == bInForceEnabled)
	{
		return;
	}

	bGravityForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void AAvaEffectorActor::SetGravityForceAcceleration(const FVector& InAcceleration)
{
	if (GravityForceAcceleration.Equals(InAcceleration))
	{
		return;
	}

	GravityForceAcceleration = InAcceleration;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetLocationStrength(const FVector& InStrength)
{
	if (LocationStrength.Equals(InStrength))
	{
		return;
	}

	LocationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::SetRotationStrength(const FRotator& InStrength)
{
	if (RotationStrength.Equals(InStrength))
	{
		return;
	}

	RotationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::SetScaleStrength(const FVector& InStrength)
{
	if (ScaleStrength.Equals(InStrength))
	{
		return;
	}

	ScaleStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::SetPan(const FVector& InPan)
{
	if (Pan.Equals(InPan))
	{
		return;
	}

	Pan = InPan;
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::SetFrequency(float InFrequency)
{
	if (FMath::IsNearlyEqual(Frequency, InFrequency))
	{
		return;
	}

	if (InFrequency < 0.f)
	{
		return;
	}

	Frequency = InFrequency;
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::SetOrientationForceEnabled(bool bInForceEnabled)
{
	if (bOrientationForceEnabled == bInForceEnabled)
	{
		return;
	}

	bOrientationForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void AAvaEffectorActor::SetOrientationForceRate(float InForceOrientationRate)
{
	if (FMath::IsNearlyEqual(OrientationForceRate, InForceOrientationRate))
	{
		return;
	}

	OrientationForceRate = InForceOrientationRate;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetOrientationForceMin(const FVector& InForceOrientationMin)
{
	if (OrientationForceMin.Equals(InForceOrientationMin))
	{
		return;
	}

	OrientationForceMin = InForceOrientationMin;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetOrientationForceMax(const FVector& InForceOrientationMax)
{
	if (OrientationForceMax.Equals(InForceOrientationMax))
	{
		return;
	}

	OrientationForceMax = InForceOrientationMax;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetVortexForceEnabled(bool bInForceEnabled)
{
	if (bVortexForceEnabled == bInForceEnabled)
	{
		return;
	}

	bVortexForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void AAvaEffectorActor::SetVortexForceAmount(float InForceVortexAmount)
{
	if (FMath::IsNearlyEqual(VortexForceAmount, InForceVortexAmount))
	{
		return;
	}

	VortexForceAmount = InForceVortexAmount;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetVortexForceAxis(const FVector& InForceVortexAxis)
{
	if (VortexForceAxis.Equals(InForceVortexAxis))
	{
		return;
	}

	VortexForceAxis = InForceVortexAxis;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetCurlNoiseForceEnabled(bool bInForceEnabled)
{
	if (bCurlNoiseForceEnabled == bInForceEnabled)
	{
		return;
	}

	bCurlNoiseForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void AAvaEffectorActor::SetCurlNoiseForceStrength(float InForceCurlNoiseStrength)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceStrength, InForceCurlNoiseStrength))
	{
		return;
	}

	CurlNoiseForceStrength = InForceCurlNoiseStrength;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetCurlNoiseForceFrequency(float InForceCurlNoiseFrequency)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceFrequency, InForceCurlNoiseFrequency))
	{
		return;
	}

	CurlNoiseForceFrequency = InForceCurlNoiseFrequency;
	OnForceOptionsChanged();
}

void AAvaEffectorActor::SetAttractionForceEnabled(bool bInForceEnabled)
{
	if (bAttractionForceEnabled == bInForceEnabled)
	{
		return;
	}

	bAttractionForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

#if WITH_EDITOR
void AAvaEffectorActor::SetVisualizerSpriteVisible(bool bInVisible)
{
	if (bVisualizerSpriteVisible == bInVisible)
	{
		return;
	}

	bVisualizerSpriteVisible = bInVisible;
	OnVisualizerSpriteVisibleChanged();
}
#endif

void AAvaEffectorActor::ForEachCloner(TFunctionRef<bool(AAvaClonerActor*, int32)> InFunction, bool bSkipIdxCheck)
{
	for (TSet<TWeakObjectPtr<AAvaClonerActor>>::TIterator It(InternalCloners); It; ++It)
	{
		AAvaClonerActor* Cloner = It->Get();
		if (!Cloner)
		{
			continue;
		}

		const int32 Idx = Cloner->GetEffectorIndex(this);
		if (!bSkipIdxCheck && Idx == INDEX_NONE)
		{
			continue;
		}

		if (!InFunction(Cloner, Idx))
		{
			return;
		}
	}
}

void AAvaEffectorActor::OnEffectorSubsystemInitialized(const UWorld* World)
{
	if (GetWorld() == World)
	{
		RegisterToChannel();
	}
}

void AAvaEffectorActor::RegisterToChannel()
{
	UAvaEffectorSubsystem::OnSubsystemInitializedDelegate.RemoveAll(this);

	// Register this effector to the effector channel
	if (UAvaEffectorSubsystem* EffectorSubsystem = UAvaEffectorSubsystem::Get(GetWorld()))
	{
		EffectorSubsystem->RegisterChannelEffector(this);
	}
	else
	{
		UAvaEffectorSubsystem::OnSubsystemInitializedDelegate.AddUObject(this, &AAvaEffectorActor::OnEffectorSubsystemInitialized);
	}
}

void AAvaEffectorActor::OnEffectorTransformed(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport)
{
	OnTransformChanged();
	// Update when scaled or rotated
	OnSphereChanged();
	OnBoxChanged();
	OnPlaneChanged();
	// update if self
	const AActor* InternalTargetActor = InternalTargetActorWeak.Get();
	if (InternalTargetActor == this)
	{
		OnTargetOptionsChanged();
	}
}

void AAvaEffectorActor::OnEnabledChanged()
{
	if (bEnabled)
	{
		OnEffectorChanged();
	}
	else // Disabled
	{
		OnEffectorDisabled();
		// Hide visualization components
		OnTypeChanged();
		// Editor
		OnVisualizerThicknessChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void AAvaEffectorActor::OnEffectorDisabled()
{
	OnMagnitudeChanged();
}

void AAvaEffectorActor::OnClonersChanged()
{
	TSet<TWeakObjectPtr<AAvaClonerActor>> AddedCloners = Cloners.Difference(InternalCloners);
	TSet<TWeakObjectPtr<AAvaClonerActor>> RemovedCloners = InternalCloners.Difference(Cloners);

	// Unlink removed cloners
	for (const TWeakObjectPtr<AAvaClonerActor>& RemovedClonerWeak : RemovedCloners)
	{
		if (AAvaClonerActor* RemovedCloner = RemovedClonerWeak.Get())
		{
			RemovedCloner->UnregisterEffector(this);
		}
	}

	// Link added cloners
	for (const TWeakObjectPtr<AAvaClonerActor>& AddedClonerWeak : AddedCloners)
	{
		if (AAvaClonerActor* AddedCloner = AddedClonerWeak.Get())
		{
			AddedCloner->RegisterEffector(this);
		}
	}
}

void AAvaEffectorActor::OnModeChanged()
{
	ChannelData.Mode = Mode;

	OnTransformOptionsChanged();
	OnTargetActorChanged();
	OnNoiseFieldOptionsChanged();
}

void AAvaEffectorActor::OnTypeChanged()
{
	// Sphere
	InnerSphereComponent->SetVisibility(Type == EAvaClonerEffectorType::Sphere);
	OuterSphereComponent->SetVisibility(Type == EAvaClonerEffectorType::Sphere);
	// Box
	InnerBoxComponent->SetVisibility(Type == EAvaClonerEffectorType::Box);
	OuterBoxComponent->SetVisibility(Type == EAvaClonerEffectorType::Box);
	// Plane
	InnerPlaneComponent->SetVisibility(Type == EAvaClonerEffectorType::Plane);
	OuterPlaneComponent->SetVisibility(Type == EAvaClonerEffectorType::Plane);

	ChannelData.Type = Type;

	// Update type data
	OnSphereChanged();
	OnBoxChanged();
	OnPlaneChanged();
}

void AAvaEffectorActor::RefreshClonerParameters(AAvaClonerActor* InCloner, bool bInForce) const
{
	if (InCloner && (bEnabled || bInForce))
	{
		InCloner->RequestClonerUpdate();
	}
}

void AAvaEffectorActor::RefreshClonersParameters(bool bInForce)
{
	ForEachCloner([this, bInForce](AAvaClonerActor* InCloner, int32 InIndex)
	{
		RefreshClonerParameters(InCloner, bInForce);
		return true;
	});
}

void AAvaEffectorActor::RegisterToCloners()
{
	for (TSet<TWeakObjectPtr<AAvaClonerActor>>::TIterator It = InternalCloners.CreateIterator(); It; ++It)
	{
		AAvaClonerActor* Cloner = It->Get();

		if (!Cloner)
		{
			It.RemoveCurrent();
			continue;
		}

		Cloner->RegisterEffector(this);
	}

	OnEffectorChanged();
}

void AAvaEffectorActor::OnEffectorChanged()
{
	if (!bEnabled)
	{
		OnEnabledChanged();
	}
	else
	{
		OnModeChanged();
		OnTypeChanged();
		OnEasingChanged();
		OnTransformChanged();
		OnMagnitudeChanged();
		OnForceOptionsChanged();
		// Editor
		OnVisualizerThicknessChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void AAvaEffectorActor::OnClonerLinked(AAvaClonerActor* InCloner, int32 InEffectorIdx)
{
	if (!InCloner)
	{
		return;
	}

	InternalCloners.Add(InCloner);

	OnEffectorIdentifierChanged();

	UE_LOG(LogAvalancheEffector, Log, TEXT("%s : Effector linked to Cloner %s"), *GetActorNameOrLabel(), *InCloner->GetActorNameOrLabel());
}

void AAvaEffectorActor::OnClonerUnlinked(AAvaClonerActor* InCloner, int32 InEffectorIdx)
{
	if (!InCloner)
	{
		return;
	}

	InternalCloners.Remove(InCloner);

	UE_LOG(LogAvalancheEffector, Log, TEXT("%s : Effector unlinked from cloner %s"), *GetActorNameOrLabel(), *InCloner->GetActorNameOrLabel());
}

void AAvaEffectorActor::OnEasingChanged()
{
	ChannelData.Easing = Easing;
}

void AAvaEffectorActor::OnTransformChanged()
{
	ChannelData.Location = GetActorLocation();
	ChannelData.Rotation = GetActorRotation().Quaternion();
	ChannelData.Scale = GetActorScale();
}

void AAvaEffectorActor::OnBoxChanged()
{
	InnerExtent = ClampVector(InnerExtent, FVector::ZeroVector, OuterExtent);
	// Max()
	OuterExtent = OuterExtent.ComponentMax(InnerExtent);

	InnerBoxComponent->SetBoxExtent(InnerExtent);
	OuterBoxComponent->SetBoxExtent(OuterExtent);

	if (Type != EAvaClonerEffectorType::Box)
	{
		return;
	}

	ChannelData.InnerExtent = GetInnerExtent();
	ChannelData.OuterExtent = GetOuterExtent();
}

void AAvaEffectorActor::OnSphereChanged()
{
	InnerRadius = FMath::Clamp(InnerRadius, 0.f, OuterRadius);
	OuterRadius = FMath::Max(OuterRadius, InnerRadius);

	InnerSphereComponent->SetSphereRadius(InnerRadius);
	OuterSphereComponent->SetSphereRadius(OuterRadius);

	if (Type != EAvaClonerEffectorType::Sphere)
	{
		return;
	}

	ChannelData.InnerExtent = FVector(GetInnerRadius());
	ChannelData.OuterExtent = FVector(GetOuterRadius());
}

void AAvaEffectorActor::OnPlaneChanged()
{
	static const FVector PlaneAxis = -FVector::LeftVector;
	const FVector InnerPlane = PlaneAxis * FVector(-PlaneSpacing/2);
	const FVector OuterPlane = PlaneAxis * FVector(PlaneSpacing/2);

	static const FVector InnerSize(100, 0, 100);
	InnerPlaneComponent->SetRelativeLocation(InnerPlane);
	InnerPlaneComponent->SetBoxExtent(InnerSize);

	static const FVector OuterSize(200, 0, 200);
	OuterPlaneComponent->SetRelativeLocation(OuterPlane);
	OuterPlaneComponent->SetBoxExtent(OuterSize);

	if (Type != EAvaClonerEffectorType::Plane)
	{
		return;
	}

	ChannelData.InnerExtent = PlaneAxis;
	ChannelData.OuterExtent = FVector(PlaneSpacing);
}

void AAvaEffectorActor::OnMagnitudeChanged()
{
	const float EffectorMagnitude = bEnabled ? GetMagnitude() : 0.f;
	ChannelData.Magnitude = EffectorMagnitude;
}

void AAvaEffectorActor::OnTransformOptionsChanged()
{
	if (Mode != EAvaClonerEffectorMode::Default)
	{
		return;
	}

	FVector ScaleDelta = GetScale();
	ScaleDelta.X = FMath::Max(ScaleDelta.X, UE_KINDA_SMALL_NUMBER);
	ScaleDelta.Y = FMath::Max(ScaleDelta.Y, UE_KINDA_SMALL_NUMBER);
	ScaleDelta.Z = FMath::Max(ScaleDelta.Z, UE_KINDA_SMALL_NUMBER);

	ChannelData.LocationDelta = GetOffset();
	ChannelData.RotationDelta = GetRotation().Quaternion();
	ChannelData.ScaleDelta = ScaleDelta;
}

void AAvaEffectorActor::OnTargetActorChanged()
{
	AActor* TargetActor = TargetActorWeak.Get();
	AActor* const InternalTargetActor = InternalTargetActorWeak.Get();
	if (TargetActor && TargetActor == InternalTargetActor)
	{
		OnTargetOptionsChanged();
		return;
	}

	// unbind, except if it is self
	if (InternalTargetActor && InternalTargetActor->GetRootComponent())
	{
		if (InternalTargetActor != this)
		{
			InternalTargetActor->OnDestroyed.RemoveAll(this);
			InternalTargetActor->GetRootComponent()->TransformUpdated.RemoveAll(this);
			InternalTargetActorWeak.Reset();
		}
	}

	// set self by default if invalid
	if (!TargetActor || !TargetActor->GetRootComponent())
	{
		TargetActor = this;
	}

	// bind to transform event, do not bind to self since we already do that
	if (TargetActor != this)
	{
		TargetActor->GetRootComponent()->TransformUpdated.RemoveAll(this);
		TargetActor->GetRootComponent()->TransformUpdated.AddUObject(this, &AAvaEffectorActor::OnTargetActorTransformChanged);
		TargetActor->OnDestroyed.RemoveAll(this);
		TargetActor->OnDestroyed.AddUniqueDynamic(this, &AAvaEffectorActor::OnTargetActorDestroyed);
	}

	TargetActorWeak = TargetActor;
	InternalTargetActorWeak = TargetActor;
	OnTargetOptionsChanged();
}

void AAvaEffectorActor::OnTargetActorDestroyed(AActor* InActor)
{
	const AActor* TargetActor = TargetActorWeak.Get();
	if (TargetActor == InActor)
	{
		TargetActorWeak = this;
		OnTargetActorChanged();
	}
}

void AAvaEffectorActor::OnNoiseFieldOptionsChanged()
{
	if (Mode != EAvaClonerEffectorMode::NoiseField)
	{
		return;
	}

	ChannelData.LocationDelta = LocationStrength;
	ChannelData.RotationDelta = RotationStrength.Quaternion();
	ChannelData.ScaleDelta = ScaleStrength;
	ChannelData.Frequency = Frequency;
	ChannelData.Pan = Pan;
}

void AAvaEffectorActor::OnTargetActorTransformChanged(USceneComponent*, EUpdateTransformFlags, ETeleportType)
{
	OnTargetOptionsChanged();
}

void AAvaEffectorActor::OnTargetOptionsChanged()
{
	if (Mode != EAvaClonerEffectorMode::Target)
	{
		return;
	}

	const AActor* InternalTargetActor = InternalTargetActorWeak.Get();
	if (!InternalTargetActor)
	{
		return;
	}

	ChannelData.LocationDelta = InternalTargetActor->GetActorLocation();
	ChannelData.RotationDelta = FQuat::Identity;
	ChannelData.ScaleDelta = FVector::OneVector;
}

void AAvaEffectorActor::OnVisualizerThicknessChanged()
{
	VisualizerThickness = FMath::Clamp(VisualizerThickness, 0.1f, 10.f);

	InnerSphereComponent->SetLineThickness(VisualizerThickness);
	OuterSphereComponent->SetLineThickness(VisualizerThickness);
	InnerBoxComponent->SetLineThickness(VisualizerThickness);
	OuterBoxComponent->SetLineThickness(VisualizerThickness);
	InnerPlaneComponent->SetLineThickness(VisualizerThickness);
	OuterPlaneComponent->SetLineThickness(VisualizerThickness);
}

void AAvaEffectorActor::OnVisualizerSpriteVisibleChanged()
{
#if WITH_EDITOR
	UE::ClonerEffector::SetBillboardComponentSprite(this, TEXT("/Script/Engine.Texture2D'/Avalanche/ClonerResources/Textures/T_EffectorIcon.T_EffectorIcon'"));
	UE::ClonerEffector::SetBillboardComponentVisibility(this, bVisualizerSpriteVisible);
#endif
}

void AAvaEffectorActor::OnForceOptionsChanged()
{
	if (bOrientationForceEnabled)
	{
		ChannelData.OrientationForceRate = OrientationForceRate;
		ChannelData.OrientationForceMin = OrientationForceMin;
		ChannelData.OrientationForceMax = OrientationForceMax;
	}
	else
	{
		ChannelData.OrientationForceRate = 0.f;
		ChannelData.OrientationForceMin = FVector::ZeroVector;
		ChannelData.OrientationForceMax = FVector::ZeroVector;
	}

	if (bVortexForceEnabled)
	{
		ChannelData.VortexForceAmount = VortexForceAmount;
		ChannelData.VortexForceAxis = VortexForceAxis;
	}
	else
	{
		ChannelData.VortexForceAmount = 0.f;
		ChannelData.VortexForceAxis = FVector::ZeroVector;
	}

	if (bCurlNoiseForceEnabled)
	{
		ChannelData.CurlNoiseForceStrength = CurlNoiseForceStrength;
		ChannelData.CurlNoiseForceFrequency = CurlNoiseForceFrequency;
	}
	else
	{
		ChannelData.CurlNoiseForceStrength = 0.f;
		ChannelData.CurlNoiseForceFrequency = 0.f;
	}

	if (bAttractionForceEnabled)
	{
		ChannelData.AttractionForceStrength = AttractionForceStrength;
		ChannelData.AttractionForceFalloff = AttractionForceFalloff;
	}
	else
	{
		ChannelData.AttractionForceStrength = 0.f;
		ChannelData.AttractionForceFalloff = 0.f;
	}

	if (bGravityForceEnabled)
	{
		ChannelData.GravityForceAcceleration = GravityForceAcceleration;
	}
	else
	{
		ChannelData.GravityForceAcceleration = FVector::ZeroVector;
	}
}

void AAvaEffectorActor::OnForceEnabledChanged()
{
	// Refresh cloners to reset clones transform
	RefreshClonersParameters();
	OnForceOptionsChanged();
}

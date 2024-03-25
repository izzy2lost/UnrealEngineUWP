// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/CEEffectorActor.h"

#include "Cloner/CEClonerActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Effector/CEEffectorComponent.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/Vector.h"
#include "PropertyBag.h"
#include "Subsystems/CEEffectorSubsystem.h"
#include "UObject/ConstructorHelpers.h"

ACEEffectorActor::FOnEffectorIdentifierChanged ACEEffectorActor::OnEffectorRefreshClonerDelegate;

ACEEffectorActor::ACEEffectorActor()
{
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneComponent = CreateDefaultSubobject<UCEEffectorComponent>(TEXT("AvaEffectorComponent"));
#if WITH_EDITORONLY_DATA
	SceneComponent->bVisualizeComponent = true;
#endif
	SetRootComponent(SceneComponent);

	// Inner Visualizer Component
	InnerVisualizerComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("InnerVisualizerComponent"));
	InnerVisualizerComponent->SetHiddenInGame(true);
	InnerVisualizerComponent->SetTranslucentSortPriority(0);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	InnerVisualizerComponent->SetIsVisualizationComponent(true);
#endif
	InnerVisualizerComponent->bIsEditorOnly = false;
	InnerVisualizerComponent->SetupAttachment(SceneComponent);

	// Outer Visualizer Component
	OuterVisualizerComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("OuterVisualizerComponent"));
	OuterVisualizerComponent->SetHiddenInGame(true);
	OuterVisualizerComponent->SetTranslucentSortPriority(1);
#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	OuterVisualizerComponent->SetIsVisualizationComponent(true);
#endif
	OuterVisualizerComponent->bIsEditorOnly = false;
	OuterVisualizerComponent->SetupAttachment(SceneComponent);

	static const ConstructorHelpers::FObjectFinder<UMaterialInterface> VisualizerMaterial(TEXT("/Script/Engine.Material'/ClonerEffector/Materials/M_EffectorVisualizer.M_EffectorVisualizer'"));

	if (VisualizerMaterial.Succeeded())
	{
		InnerVisualizerMaterial = UMaterialInstanceDynamic::Create(
			VisualizerMaterial.Object,
			this
		);

		InnerVisualizerMaterial->SetVectorParameterValue(VisualizerColorName, FLinearColor::Red);
		InnerVisualizerMaterial->SetScalarParameterValue(VisualizerOpacityName, VisualizerOpacity);

		OuterVisualizerMaterial = UMaterialInstanceDynamic::Create(
			VisualizerMaterial.Object,
			this
		);

		OuterVisualizerMaterial->SetVectorParameterValue(VisualizerColorName, FLinearColor::Blue);
		OuterVisualizerMaterial->SetScalarParameterValue(VisualizerOpacityName, VisualizerOpacity);
	}

	if (!IsTemplate())
	{
		SceneComponent->TransformUpdated.AddUObject(this, &ACEEffectorActor::OnEffectorTransformed);
		UCEEffectorSubsystem::OnSubsystemInitializedDelegate.AddUObject(this, &ACEEffectorActor::OnEffectorSubsystemInitialized);
	}
}

#if WITH_EDITOR
FString ACEEffectorActor::GetDefaultActorLabel() const
{
	return DefaultLabel;
}

TCEPropertyChangeDispatcher<ACEEffectorActor> ACEEffectorActor::PropertyChangeDispatcher =
{
	/** Effector */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bEnabled), &ACEEffectorActor::OnEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Magnitude), &ACEEffectorActor::OnMagnitudeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VisualizerOpacity), &ACEEffectorActor::OnVisualizerOpacityChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bVisualizerSpriteVisible), &ACEEffectorActor::OnVisualizerSpriteVisibleChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Color), &ACEEffectorActor::OnColorChanged },
	/** Type */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Type), &ACEEffectorActor::OnTypeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bInvertType), &ACEEffectorActor::OnMagnitudeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Easing), &ACEEffectorActor::OnEasingChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterRadius), &ACEEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerRadius), &ACEEffectorActor::OnSphereChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerExtent), &ACEEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterExtent), &ACEEffectorActor::OnBoxChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, PlaneSpacing), &ACEEffectorActor::OnPlaneChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialAngle), &ACEEffectorActor::OnRadialChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMinRadius), &ACEEffectorActor::OnRadialChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMaxRadius), &ACEEffectorActor::OnRadialChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusRadius), &ACEEffectorActor::OnTorusChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusInnerRadius), &ACEEffectorActor::OnTorusChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusOuterRadius), &ACEEffectorActor::OnTorusChanged },
	/** Mode */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Mode), &ACEEffectorActor::OnModeChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Offset), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Rotation), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Scale), &ACEEffectorActor::OnTransformOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TargetActorWeak), &ACEEffectorActor::OnTargetActorChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, LocationStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RotationStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, ScaleStrength), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Pan), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, Frequency), &ACEEffectorActor::OnNoiseFieldOptionsChanged },
	/** Force */
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bOrientationForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceRate), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceMin), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OrientationForceMax), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bVortexForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VortexForceAmount), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, VortexForceAxis), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bCurlNoiseForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, CurlNoiseForceStrength), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, CurlNoiseForceFrequency), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bAttractionForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, AttractionForceStrength), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, AttractionForceFalloff), &ACEEffectorActor::OnForceOptionsChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, bGravityForceEnabled), &ACEEffectorActor::OnForceEnabledChanged },
	{ GET_MEMBER_NAME_CHECKED(ACEEffectorActor, GravityForceAcceleration), &ACEEffectorActor::OnForceOptionsChanged },
};

void ACEEffectorActor::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	PropertyChangeDispatcher.OnPropertyChanged(this, InPropertyChangedEvent);
}

void ACEEffectorActor::PostEditImport()
{
	Super::PostEditImport();

	OnEffectorChanged();
}

void ACEEffectorActor::PostEditUndo()
{
	Super::PostEditUndo();

	OnEffectorChanged();
}
#endif

void ACEEffectorActor::Destroyed()
{
	Super::Destroyed();

	// Remove this effector from the effector channel
	if (UCEEffectorSubsystem* EffectorSubsystem = UCEEffectorSubsystem::Get(GetWorld()))
	{
		EffectorSubsystem->UnregisterChannelEffector(this);
	}
}

void ACEEffectorActor::PostActorCreated()
{
	Super::PostActorCreated();

	OnEffectorChanged();
}

void ACEEffectorActor::PostLoad()
{
	Super::PostLoad();

	OnEffectorChanged();

	if (!InternalCloners.IsEmpty())
	{
		for (const TWeakObjectPtr<ACEClonerActor>& ClonerWeak : InternalCloners)
		{
			if (ACEClonerActor* Cloner = ClonerWeak.Get())
			{
				Cloner->LinkEffector(this);
			}
		}

		InternalCloners.Empty();
	}
}

void ACEEffectorActor::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	OnEffectorChanged();
}

void ACEEffectorActor::RegisterToChannel()
{
	if (IsValid(this)
		&& ChannelData.GetIdentifier() == INDEX_NONE)
	{
		// Register this effector to the effector channel
		if (UCEEffectorSubsystem* EffectorSubsystem = UCEEffectorSubsystem::Get(GetWorld()))
		{
			EffectorSubsystem->RegisterChannelEffector(this);
		}
	}
}

int32 ACEEffectorActor::GetChannelIdentifier() const
{
	return ChannelData.GetIdentifier();
}

void ACEEffectorActor::OnEffectorSubsystemInitialized(const UWorld* InWorld)
{
	if (InWorld && GetWorld() == InWorld)
	{
		OnEffectorChanged();
	}
}

FCEClonerEffectorChannelData& ACEEffectorActor::GetChannelData()
{
	return ChannelData;
}

void ACEEffectorActor::SetType(ECEClonerEffectorType InType)
{
	if (InType == Type)
	{
		return;
	}

	Type = InType;
	OnTypeChanged();
}

void ACEEffectorActor::SetEasing(ECEClonerEasing InEasing)
{
	if (InEasing == Easing)
	{
		return;
	}

	Easing = InEasing;
	OnEasingChanged();
}

void ACEEffectorActor::SetMagnitude(float InMagnitude)
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

void ACEEffectorActor::SetOuterRadius(float InRadius)
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

void ACEEffectorActor::SetInnerRadius(float InRadius)
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

void ACEEffectorActor::SetInnerExtent(const FVector& InExtent)
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

void ACEEffectorActor::SetOuterExtent(const FVector& InExtent)
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

void ACEEffectorActor::SetPlaneSpacing(float InSpacing)
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

void ACEEffectorActor::SetRadialAngle(float InAngle)
{
	InAngle = FMath::Clamp(InAngle, 0, 360);

	if (FMath::IsNearlyEqual(InAngle, RadialAngle))
	{
		return;
	}

	RadialAngle = InAngle;
	OnRadialChanged();
}

void ACEEffectorActor::SetRadialMinRadius(float InRadius)
{
	InRadius = FMath::Max(0, InRadius);

	if (FMath::IsNearlyEqual(InRadius, RadialMinRadius))
	{
		return;
	}

	RadialMinRadius = InRadius;
	OnRadialChanged();
}

void ACEEffectorActor::SetInvertType(bool bInInvert)
{
	if (bInvertType == bInInvert)
	{
		return;
	}

	bInvertType = bInInvert;
	OnMagnitudeChanged();
}

void ACEEffectorActor::SetRadialMaxRadius(float InRadius)
{
	InRadius = FMath::Max(0, InRadius);

	if (FMath::IsNearlyEqual(InRadius, RadialMaxRadius))
	{
		return;
	}

	RadialMaxRadius = InRadius;
	OnRadialChanged();
}

void ACEEffectorActor::SetTorusRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, TorusRadius))
	{
		return;
	}

	TorusRadius = InRadius;
	OnTorusChanged();
}

void ACEEffectorActor::SetTorusInnerRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, TorusInnerRadius))
	{
		return;
	}

	TorusInnerRadius = InRadius;
	OnTorusChanged();
}

void ACEEffectorActor::SetTorusOuterRadius(float InRadius)
{
	if (FMath::IsNearlyEqual(InRadius, TorusOuterRadius))
	{
		return;
	}

	TorusOuterRadius = InRadius;
	OnTorusChanged();
}

void ACEEffectorActor::SetOffset(const FVector& InOffset)
{
	if (InOffset.Equals(Offset))
	{
		return;
	}

	Offset = InOffset;
	OnTransformOptionsChanged();
}

void ACEEffectorActor::SetRotation(const FRotator& InRotation)
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

void ACEEffectorActor::SetScale(const FVector& InScale)
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

void ACEEffectorActor::SetEnabled(bool bInEnable)
{
	if (bInEnable == bEnabled)
	{
		return;
	}

	bEnabled = bInEnable;
	OnEnabledChanged();
}

void ACEEffectorActor::SetVisualizerOpacity(float InOpacity)
{
	if (FMath::IsNearlyEqual(VisualizerOpacity, InOpacity))
	{
		return;
	}

	if (VisualizerOpacity < 0.f || VisualizerOpacity > 1.f)
	{
		return;
	}

	VisualizerOpacity = InOpacity;
	OnVisualizerOpacityChanged();
}

void ACEEffectorActor::SetMode(ECEClonerEffectorMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}

	Mode = InMode;
	OnModeChanged();
}

void ACEEffectorActor::SetTargetActor(AActor* InTargetActor)
{
	if (InTargetActor == TargetActorWeak.Get())
	{
		return;
	}

	TargetActorWeak = InTargetActor;
	OnTargetActorChanged();
}

void ACEEffectorActor::SetTargetActorWeak(const TWeakObjectPtr<AActor>& InTargetActor)
{
	SetTargetActor(InTargetActor.Get());
}

void ACEEffectorActor::SetAttractionForceStrength(float InForceStrength)
{
	if (FMath::IsNearlyEqual(AttractionForceStrength, InForceStrength))
	{
		return;
	}

	AttractionForceStrength = InForceStrength;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetAttractionForceFalloff(float InForceFalloff)
{
	if (FMath::IsNearlyEqual(AttractionForceFalloff, InForceFalloff))
	{
		return;
	}

	AttractionForceFalloff = InForceFalloff;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetGravityForceEnabled(bool bInForceEnabled)
{
	if (bGravityForceEnabled == bInForceEnabled)
	{
		return;
	}

	bGravityForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetGravityForceAcceleration(const FVector& InAcceleration)
{
	if (GravityForceAcceleration.Equals(InAcceleration))
	{
		return;
	}

	GravityForceAcceleration = InAcceleration;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetLocationStrength(const FVector& InStrength)
{
	if (LocationStrength.Equals(InStrength))
	{
		return;
	}

	LocationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetRotationStrength(const FRotator& InStrength)
{
	if (RotationStrength.Equals(InStrength))
	{
		return;
	}

	RotationStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetScaleStrength(const FVector& InStrength)
{
	if (ScaleStrength.Equals(InStrength))
	{
		return;
	}

	ScaleStrength = InStrength;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetPan(const FVector& InPan)
{
	if (Pan.Equals(InPan))
	{
		return;
	}

	Pan = InPan;
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::SetFrequency(float InFrequency)
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

void ACEEffectorActor::SetOrientationForceEnabled(bool bInForceEnabled)
{
	if (bOrientationForceEnabled == bInForceEnabled)
	{
		return;
	}

	bOrientationForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetOrientationForceRate(float InForceOrientationRate)
{
	if (FMath::IsNearlyEqual(OrientationForceRate, InForceOrientationRate))
	{
		return;
	}

	OrientationForceRate = InForceOrientationRate;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetOrientationForceMin(const FVector& InForceOrientationMin)
{
	if (OrientationForceMin.Equals(InForceOrientationMin))
	{
		return;
	}

	OrientationForceMin = InForceOrientationMin;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetOrientationForceMax(const FVector& InForceOrientationMax)
{
	if (OrientationForceMax.Equals(InForceOrientationMax))
	{
		return;
	}

	OrientationForceMax = InForceOrientationMax;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetVortexForceEnabled(bool bInForceEnabled)
{
	if (bVortexForceEnabled == bInForceEnabled)
	{
		return;
	}

	bVortexForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetVortexForceAmount(float InForceVortexAmount)
{
	if (FMath::IsNearlyEqual(VortexForceAmount, InForceVortexAmount))
	{
		return;
	}

	VortexForceAmount = InForceVortexAmount;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetVortexForceAxis(const FVector& InForceVortexAxis)
{
	if (VortexForceAxis.Equals(InForceVortexAxis))
	{
		return;
	}

	VortexForceAxis = InForceVortexAxis;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetCurlNoiseForceEnabled(bool bInForceEnabled)
{
	if (bCurlNoiseForceEnabled == bInForceEnabled)
	{
		return;
	}

	bCurlNoiseForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

void ACEEffectorActor::SetCurlNoiseForceStrength(float InForceCurlNoiseStrength)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceStrength, InForceCurlNoiseStrength))
	{
		return;
	}

	CurlNoiseForceStrength = InForceCurlNoiseStrength;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetCurlNoiseForceFrequency(float InForceCurlNoiseFrequency)
{
	if (FMath::IsNearlyEqual(CurlNoiseForceFrequency, InForceCurlNoiseFrequency))
	{
		return;
	}

	CurlNoiseForceFrequency = InForceCurlNoiseFrequency;
	OnForceOptionsChanged();
}

void ACEEffectorActor::SetAttractionForceEnabled(bool bInForceEnabled)
{
	if (bAttractionForceEnabled == bInForceEnabled)
	{
		return;
	}

	bAttractionForceEnabled = bInForceEnabled;
	OnForceEnabledChanged();
}

#if WITH_EDITOR
void ACEEffectorActor::SetVisualizerSpriteVisible(bool bInVisible)
{
	if (bVisualizerSpriteVisible == bInVisible)
	{
		return;
	}

	bVisualizerSpriteVisible = bInVisible;
	OnVisualizerSpriteVisibleChanged();
}
#endif

void ACEEffectorActor::SetColor(const FLinearColor& InColor)
{
	if (Color == InColor)
	{
		return;
	}

	Color = InColor;
	OnColorChanged();
}

void ACEEffectorActor::OnEffectorTransformed(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport)
{
	OnTransformChanged();

	// Update when scaled or rotated
	UpdateEffectorTypes();

	// update if self
	const AActor* InternalTargetActor = InternalTargetActorWeak.Get();
	if (InternalTargetActor == this)
	{
		OnTargetOptionsChanged();
	}
}

void ACEEffectorActor::OnEnabledChanged()
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
		OnVisualizerOpacityChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void ACEEffectorActor::OnEffectorDisabled()
{
	OnMagnitudeChanged();
}

void ACEEffectorActor::OnModeChanged()
{
	ChannelData.Mode = Mode;

	OnTransformOptionsChanged();
	OnTargetActorChanged();
	OnNoiseFieldOptionsChanged();
}

void ACEEffectorActor::OnTypeChanged()
{
	ChannelData.Type = Type;

	// Update type data
	UpdateEffectorTypes();
}

void ACEEffectorActor::OnEffectorChanged()
{
	RegisterToChannel();

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
		OnColorChanged();
		// Editor
		OnVisualizerOpacityChanged();
		OnVisualizerSpriteVisibleChanged();
	}
}

void ACEEffectorActor::OnEasingChanged()
{
	ChannelData.Easing = Easing;
}

void ACEEffectorActor::OnTransformChanged()
{
	ChannelData.Location = GetActorLocation();
	ChannelData.Rotation = GetActorRotation().Quaternion();
	ChannelData.Scale = GetActorScale();
}

void ACEEffectorActor::OnBoxChanged()
{
	InnerExtent = ClampVector(InnerExtent, FVector::ZeroVector, OuterExtent);
	OuterExtent = OuterExtent.ComponentMax(InnerExtent);

	if (Type != ECEClonerEffectorType::Box)
	{
		return;
	}

	ChannelData.InnerExtent = GetInnerExtent();
	ChannelData.OuterExtent = GetOuterExtent();

	// Update visualizer
	static const FName BoxInnerExtentName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerExtent);
	static const FName BoxOuterExtentName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterExtent);
	TValueOrError<FVector*, EPropertyBagResult> BoxInnerExtentValue = VisualizerData.GetValueStruct<FVector>(BoxInnerExtentName);
	TValueOrError<FVector*, EPropertyBagResult> BoxOuterExtentValue = VisualizerData.GetValueStruct<FVector>(BoxOuterExtentName);

	if (!BoxInnerExtentValue.HasValue()
		|| *BoxInnerExtentValue.GetValue() != InnerExtent)
	{
		UpdateVisualizer(InnerVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(InMesh, PrimitiveOptions, FTransform(FVector(0, 0, -InnerExtent.Z)), InnerExtent.X * 2, InnerExtent.Y * 2, InnerExtent.Z * 2);
		});
	}

	if (!BoxOuterExtentValue.HasValue()
		|| *BoxOuterExtentValue.GetValue() != OuterExtent)
	{
		UpdateVisualizer(OuterVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(InMesh, PrimitiveOptions, FTransform(FVector(0, 0, -OuterExtent.Z)), OuterExtent.X * 2, OuterExtent.Y * 2, OuterExtent.Z * 2);
		});
	}

	VisualizerData.Reset();
	VisualizerData.AddProperty(BoxInnerExtentName, EPropertyBagPropertyType::Struct);
	VisualizerData.AddProperty(BoxOuterExtentName, EPropertyBagPropertyType::Struct);
	VisualizerData.SetValueStruct(BoxInnerExtentName, InnerExtent);
	VisualizerData.SetValueStruct(BoxOuterExtentName, OuterExtent);
}

void ACEEffectorActor::OnSphereChanged()
{
	InnerRadius = FMath::Clamp(InnerRadius, 0.f, OuterRadius);
	OuterRadius = FMath::Max(OuterRadius, InnerRadius);

	if (Type != ECEClonerEffectorType::Sphere)
	{
		return;
	}

	ChannelData.InnerExtent = FVector(GetInnerRadius());
	ChannelData.OuterExtent = FVector(GetOuterRadius());

	// Update visualizer
	static const FName SphereInnerRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerRadius);
	static const FName SphereOuterRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterRadius);
	TValueOrError<float, EPropertyBagResult> SphereInnerRadiusValue = VisualizerData.GetValueFloat(SphereInnerRadiusName);
	TValueOrError<float, EPropertyBagResult> SphereOuterRadiusValue = VisualizerData.GetValueFloat(SphereOuterRadiusName);

	if (!SphereInnerRadiusValue.HasValue()
		|| SphereInnerRadiusValue.GetValue() != InnerRadius)
	{
		UpdateVisualizer(InnerVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(InMesh, PrimitiveOptions, FTransform::Identity, InnerRadius);
		});
	}

	if (!SphereOuterRadiusValue.HasValue()
		|| SphereOuterRadiusValue.GetValue() != OuterRadius)
	{
		UpdateVisualizer(OuterVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(InMesh, PrimitiveOptions, FTransform::Identity, OuterRadius);
		});
	}

	VisualizerData.Reset();
	VisualizerData.AddProperty(SphereInnerRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.AddProperty(SphereOuterRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.SetValueFloat(SphereInnerRadiusName, InnerRadius);
	VisualizerData.SetValueFloat(SphereOuterRadiusName, OuterRadius);
}

void ACEEffectorActor::OnPlaneChanged()
{
	if (Type != ECEClonerEffectorType::Plane)
	{
		return;
	}

	ChannelData.InnerExtent = FVector::LeftVector;
	ChannelData.OuterExtent = FVector(PlaneSpacing);

	// Update visualizer
	static const FName PlaneSpacingName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, PlaneSpacing);
	TValueOrError<float, EPropertyBagResult> PlaneSpacingValue = VisualizerData.GetValueFloat(PlaneSpacingName);

	if (!PlaneSpacingValue.HasValue()
		|| PlaneSpacingValue.GetValue() != PlaneSpacing)
	{
		UpdateVisualizer(InnerVisualizerId, [this](UDynamicMesh* InMesh)
		{
			const FVector InnerPlane = FVector::LeftVector * FVector(-PlaneSpacing/2);
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(InMesh, PrimitiveOptions, FTransform(FRotator(0, 0, 90), InnerPlane), 250, 250);
		});

		UpdateVisualizer(OuterVisualizerId, [this](UDynamicMesh* InMesh)
		{
			const FVector OuterPlane = FVector::LeftVector * FVector(PlaneSpacing/2);
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(InMesh, PrimitiveOptions, FTransform(FRotator(0, 0, 90), OuterPlane), 500, 500);
		});
	}

	VisualizerData.Reset();
	VisualizerData.AddProperty(PlaneSpacingName, EPropertyBagPropertyType::Float);
	VisualizerData.SetValueFloat(PlaneSpacingName, PlaneSpacing);
}

void ACEEffectorActor::OnRadialChanged()
{
	RadialMinRadius = FMath::Min(RadialMinRadius, RadialMaxRadius);
	RadialMaxRadius = FMath::Max(RadialMinRadius, RadialMaxRadius);

	if (Type != ECEClonerEffectorType::Radial)
	{
		return;
	}

	ChannelData.InnerExtent = FVector::LeftVector;
	ChannelData.OuterExtent = FVector(RadialAngle, RadialMinRadius, RadialMaxRadius);

	// Update visualizer
	static const FName RadialAngleName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialAngle);
	static const FName RadialMinRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMinRadius);
	static const FName RadialMaxRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMaxRadius);
	TValueOrError<float, EPropertyBagResult> RadialAngleValue = VisualizerData.GetValueFloat(RadialAngleName);
	TValueOrError<float, EPropertyBagResult> RadialMinRadiusValue = VisualizerData.GetValueFloat(RadialMinRadiusName);
	TValueOrError<float, EPropertyBagResult> RadialMaxRadiusValue = VisualizerData.GetValueFloat(RadialMaxRadiusName);

	if (!RadialMaxRadiusValue.HasValue()
		|| RadialMaxRadiusValue.GetValue() != RadialMaxRadius
		|| !RadialMinRadiusValue.HasValue()
		|| RadialMinRadiusValue.GetValue() != RadialMinRadius
		|| !RadialAngleValue.HasValue()
		|| RadialAngleValue.GetValue() != RadialAngle)
	{
		UpdateVisualizer(InnerVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(InMesh, PrimitiveOptions, FTransform(FRotator(0, -90, 0)), RadialMaxRadius, 16, 0, 0, RadialAngle / 2, RadialMinRadius);
		});

		UpdateVisualizer(OuterVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(InMesh, PrimitiveOptions, FTransform(FRotator(0, -90, 0)), RadialMaxRadius, 16, 0, RadialAngle / 2, RadialAngle, RadialMinRadius);
		});
	}

	VisualizerData.Reset();
	VisualizerData.AddProperty(RadialAngleName, EPropertyBagPropertyType::Float);
	VisualizerData.AddProperty(RadialMinRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.AddProperty(RadialMaxRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.SetValueFloat(RadialAngleName, RadialAngle);
	VisualizerData.SetValueFloat(RadialMinRadiusName, RadialMinRadius);
	VisualizerData.SetValueFloat(RadialMaxRadiusName, RadialMaxRadius);
}

void ACEEffectorActor::OnTorusChanged()
{
	TorusOuterRadius = FMath::Min(TorusRadius, TorusOuterRadius);
	TorusInnerRadius = FMath::Min(TorusInnerRadius, TorusOuterRadius);
	TorusOuterRadius = FMath::Max(TorusInnerRadius, TorusOuterRadius);

	if (Type != ECEClonerEffectorType::Torus)
	{
		return;
	}

	ChannelData.InnerExtent = FVector::ZAxisVector;
	ChannelData.OuterExtent = FVector(TorusInnerRadius, TorusOuterRadius, TorusRadius);

	// Update visualizer
	static const FName TorusRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusRadius);
	static const FName TorusInnerRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusInnerRadius);
	static const FName TorusOuterRadiusName = GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusOuterRadius);
	TValueOrError<float, EPropertyBagResult> TorusRadiusValue = VisualizerData.GetValueFloat(TorusRadiusName);
	TValueOrError<float, EPropertyBagResult> TorusInnerRadiusValue = VisualizerData.GetValueFloat(TorusInnerRadiusName);
	TValueOrError<float, EPropertyBagResult> TorusOuterRadiusValue = VisualizerData.GetValueFloat(TorusOuterRadiusName);

	if (!TorusRadiusValue.HasValue()
		|| TorusRadiusValue.GetValue() != TorusRadius
		|| !TorusInnerRadiusValue.HasValue()
		|| TorusInnerRadiusValue.GetValue() != TorusInnerRadius)
	{
		UpdateVisualizer(InnerVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			constexpr FGeometryScriptRevolveOptions RevolveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(InMesh, PrimitiveOptions, FTransform(FVector(0, 0, -TorusInnerRadius)), RevolveOptions, TorusRadius, TorusInnerRadius);
		});
	}

	if (!TorusRadiusValue.HasValue()
		|| TorusRadiusValue.GetValue() != TorusRadius
		|| !TorusOuterRadiusValue.HasValue()
		|| TorusOuterRadiusValue.GetValue() != TorusOuterRadius)
	{
		UpdateVisualizer(OuterVisualizerId, [this](UDynamicMesh* InMesh)
		{
			constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
			constexpr FGeometryScriptRevolveOptions RevolveOptions;
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(InMesh, PrimitiveOptions, FTransform(FVector(0, 0, -TorusOuterRadius)), RevolveOptions, TorusRadius, TorusOuterRadius);
		});
	}

	VisualizerData.Reset();
	VisualizerData.AddProperty(TorusRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.AddProperty(TorusInnerRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.AddProperty(TorusOuterRadiusName, EPropertyBagPropertyType::Float);
	VisualizerData.SetValueFloat(TorusRadiusName, TorusRadius);
	VisualizerData.SetValueFloat(TorusInnerRadiusName, TorusInnerRadius);
	VisualizerData.SetValueFloat(TorusOuterRadiusName, TorusOuterRadius);
}

void ACEEffectorActor::OnUnboundChanged()
{
	if (Type != ECEClonerEffectorType::Unbound)
	{
		return;
	}

	// Clear visualizers
	UpdateVisualizer(InnerVisualizerId, [](UDynamicMesh* InMesh){});
	UpdateVisualizer(OuterVisualizerId, [](UDynamicMesh* InMesh){});
}

void ACEEffectorActor::OnMagnitudeChanged()
{
	const float EffectorMagnitude = bEnabled ? GetMagnitude() : 0.f;
	ChannelData.Magnitude = bInvertType ? -EffectorMagnitude : EffectorMagnitude;
}

void ACEEffectorActor::OnTransformOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::Default)
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

void ACEEffectorActor::OnTargetActorChanged()
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
		TargetActor->GetRootComponent()->TransformUpdated.AddUObject(this, &ACEEffectorActor::OnTargetActorTransformChanged);
		TargetActor->OnDestroyed.RemoveAll(this);
		TargetActor->OnDestroyed.AddUniqueDynamic(this, &ACEEffectorActor::OnTargetActorDestroyed);
	}

	TargetActorWeak = TargetActor;
	InternalTargetActorWeak = TargetActor;
	OnTargetOptionsChanged();
}

void ACEEffectorActor::OnTargetActorDestroyed(AActor* InActor)
{
	const AActor* TargetActor = TargetActorWeak.Get();
	if (TargetActor == InActor)
	{
		TargetActorWeak = this;
		OnTargetActorChanged();
	}
}

void ACEEffectorActor::UpdateVisualizer(int32 InVisualizerId, TFunction<void(UDynamicMesh*)> InMeshFunction) const
{
	UDynamicMeshComponent* MeshComponent = InVisualizerId == InnerVisualizerId ? InnerVisualizerComponent : OuterVisualizerComponent;

	if (!MeshComponent)
	{
		return;
	}

	UDynamicMesh* DynamicMesh = MeshComponent->GetDynamicMesh();

	DynamicMesh->EditMesh([](FDynamicMesh3& InMesh)
	{
		InMesh.Clear();
	});

	InMeshFunction(DynamicMesh);

	// Apply material
	UMaterialInstanceDynamic* VisualizerMaterial = InVisualizerId == InnerVisualizerId ? InnerVisualizerMaterial : OuterVisualizerMaterial;
	MeshComponent->SetMaterial(0, VisualizerMaterial);
}

void ACEEffectorActor::OnNoiseFieldOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::NoiseField)
	{
		return;
	}

	ChannelData.LocationDelta = LocationStrength;
	ChannelData.RotationDelta = RotationStrength.Quaternion();
	ChannelData.ScaleDelta = ScaleStrength;
	ChannelData.Frequency = Frequency;
	ChannelData.Pan = Pan;
}

void ACEEffectorActor::OnTargetActorTransformChanged(USceneComponent*, EUpdateTransformFlags, ETeleportType)
{
	OnTargetOptionsChanged();
}

void ACEEffectorActor::OnTargetOptionsChanged()
{
	if (Mode != ECEClonerEffectorMode::Target)
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

void ACEEffectorActor::OnVisualizerOpacityChanged()
{
	VisualizerOpacity = FMath::Clamp(VisualizerOpacity, 0.f, 1.f);

	if (InnerVisualizerMaterial)
	{
		InnerVisualizerMaterial->SetScalarParameterValue(VisualizerOpacityName, VisualizerOpacity);
	}

	if (OuterVisualizerMaterial)
	{
		OuterVisualizerMaterial->SetScalarParameterValue(VisualizerOpacityName, VisualizerOpacity);
	}
}

void ACEEffectorActor::OnVisualizerSpriteVisibleChanged()
{
#if WITH_EDITOR
	UE::ClonerEffector::SetBillboardComponentSprite(this, TEXT("/Script/Engine.Texture2D'/ClonerEffector/Textures/T_EffectorIcon.T_EffectorIcon'"));
	UE::ClonerEffector::SetBillboardComponentVisibility(this, bVisualizerSpriteVisible);
#endif
}

void ACEEffectorActor::OnColorChanged()
{
	ChannelData.Color = Color;
}

void ACEEffectorActor::UpdateEffectorTypes()
{
	OnSphereChanged();
	OnBoxChanged();
	OnPlaneChanged();
	OnRadialChanged();
	OnTorusChanged();
	OnUnboundChanged();
}

void ACEEffectorActor::OnForceOptionsChanged()
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

void ACEEffectorActor::OnForceEnabledChanged()
{
	if (bEnabled)
	{
		// Refresh cloners to reset clones transform
		OnEffectorRefreshClonerDelegate.Broadcast(this);
	}

	OnForceOptionsChanged();
}

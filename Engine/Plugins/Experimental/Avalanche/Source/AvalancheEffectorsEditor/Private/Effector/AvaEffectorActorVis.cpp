// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/AvaEffectorActorVis.h"
#include "AvaField.h"
#include "AvaShapeSprites.h"
#include "AvaVisBase.h"
#include "EditorViewportClient.h"
#include "Effector/CEEffectorComponent.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"

IMPLEMENT_HIT_PROXY(HAvaEffectorActorZoneHitProxy, HAvaHitProxy);

#define LOCTEXT_NAMESPACE "AvaEffectorActorVisualizer"

FAvaEffectorActorVisualizer::FAvaEffectorActorVisualizer()
	: FAvaVisualizerBase()
{
	using namespace UE::AvaCore;

	// Sphere
	InnerRadiusProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerRadius));
	OuterRadiusProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterRadius));

	// Box
	InnerExtentProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, InnerExtent));
	OuterExtentProperty  = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, OuterExtent));

	// Plane
	PlaneSpacingProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, PlaneSpacing));

	// Radial
	RadialAngleProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialAngle));
	RadialMinRadiusProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMinRadius));
	RadialMaxRadiusProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, RadialMaxRadius));

	// Torus
	TorusRadiusProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusRadius));
	TorusInnerRadiusProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusInnerRadius));
	TorusOuterRadiusProperty = GetProperty<ACEEffectorActor>(GET_MEMBER_NAME_CHECKED(ACEEffectorActor, TorusOuterRadius));
}

void FAvaEffectorActorVisualizer::StoreInitialValues()
{
	Super::StoreInitialValues();

	const ACEEffectorActor* EffectorActor = EffectorActorWeak.Get();

	if (GetEditedComponent() == nullptr || !EffectorActor)
	{
		return;
	}

	// Sphere
	InitialInnerRadius = EffectorActor->GetInnerRadius();
	InitialOuterRadius = EffectorActor->GetOuterRadius();

	// Box
	InitialInnerExtent = EffectorActor->GetInnerExtent();
	InitialOuterExtent = EffectorActor->GetOuterExtent();

	// Plane
	InitialPlaneSpacing = EffectorActor->GetPlaneSpacing();

	// Radial
	InitialRadialAngle = EffectorActor->GetRadialAngle();
	InitialRadialMinRadius = EffectorActor->GetRadialMinRadius();
	InitialRadialMaxRadius = EffectorActor->GetRadialMaxRadius();

	// Torus
	InitialTorusRadius = EffectorActor->GetTorusRadius();
	InitialTorusInnerRadius = EffectorActor->GetTorusInnerRadius();
	InitialTorusOuterRadius = EffectorActor->GetTorusOuterRadius();
}

FBox FAvaEffectorActorVisualizer::GetComponentBounds(const UActorComponent* InComponent) const
{
	if (const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent))
	{
		if (const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner()))
		{
			const ECEClonerEffectorType Type = EffectorActor->GetType();

			if (Type == ECEClonerEffectorType::Box)
			{
				return FBox(-EffectorActor->GetOuterExtent(), EffectorActor->GetOuterExtent());
			}

			if (Type == ECEClonerEffectorType::Sphere)
			{
				return FBox(-FVector(EffectorActor->GetOuterRadius() / 2), FVector(EffectorActor->GetOuterRadius() / 2));
			}

			if (Type == ECEClonerEffectorType::Plane)
			{
				return FBox(-FVector(EffectorActor->GetPlaneSpacing() / 2), FVector(EffectorActor->GetPlaneSpacing() / 2));
			}

			if (Type == ECEClonerEffectorType::Radial)
			{
				return FBox(-FVector(EffectorActor->GetRadialMaxRadius() / 2), FVector(EffectorActor->GetRadialMaxRadius() / 2));
			}

			if (Type == ECEClonerEffectorType::Torus)
			{
				return FBox(-FVector((EffectorActor->GetTorusRadius() + EffectorActor->GetTorusOuterRadius()) / 2), FVector((EffectorActor->GetTorusRadius() + EffectorActor->GetTorusOuterRadius()) / 2));
			}
		}
	}

	return Super::GetComponentBounds(InComponent);
}

bool FAvaEffectorActorVisualizer::HandleInputDeltaInternal(FEditorViewportClient* InViewportClient, FViewport* InViewport, const FVector& InAccumulatedTranslation, const FRotator& InAccumulatedRotation, const FVector& InAccumulatedScale)
{
	if (!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		return false;
	}

	if (ACEEffectorActor* EffectorActor = EffectorActorWeak.Get())
	{
		const ECEClonerEffectorType Type = EffectorActor->GetType();

		if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
		{
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::XYZ)
			{
				if (Type == ECEClonerEffectorType::Box)
				{
					if (EditingHandleType == HandleTypeInnerZone)
					{
						ModifyProperty(EffectorActor, InnerExtentProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetInnerExtent(InitialInnerExtent + InAccumulatedTranslation);
						});
					}
					else if (EditingHandleType == HandleTypeOuterZone)
					{
						ModifyProperty(EffectorActor, OuterExtentProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetOuterExtent(InitialOuterExtent + InAccumulatedTranslation);
						});
					}

					return true;
				}
			}

			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Y)
			{
				if (Type == ECEClonerEffectorType::Plane)
				{
					if (EditingHandleType == HandleTypeInnerZone || EditingHandleType == HandleTypeOuterZone)
					{
						ModifyProperty(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetPlaneSpacing(InitialPlaneSpacing + InAccumulatedTranslation.Y);
						});
					}

					return true;
				}

				if (Type == ECEClonerEffectorType::Sphere)
				{
					if (EditingHandleType == HandleTypeInnerZone)
					{
						ModifyProperty(EffectorActor, InnerRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetInnerRadius(InitialInnerRadius + InAccumulatedTranslation.Y);
						});
					}
					else if (EditingHandleType == HandleTypeOuterZone)
					{
						ModifyProperty(EffectorActor, OuterRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]
						{
							EffectorActor->SetOuterRadius(InitialOuterRadius + InAccumulatedTranslation.Y);
						});
					}

					return true;
				}

				if (Type == ECEClonerEffectorType::Radial)
				{
					if (EditingHandleType == HandleTypeInnerZone)
					{
						ModifyProperty(EffectorActor, RadialMinRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]
						{
							EffectorActor->SetRadialMinRadius(InitialRadialMinRadius + InAccumulatedTranslation.Y);
						});
					}
					else if (EditingHandleType == HandleTypeOuterZone)
					{
						ModifyProperty(EffectorActor, RadialMaxRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetRadialMaxRadius(InitialRadialMaxRadius + InAccumulatedTranslation.Y);
						});
					}

					return true;
				}

				if (Type == ECEClonerEffectorType::Torus)
				{
					if (EditingHandleType == HandleTypeRadius)
					{
						ModifyProperty(EffectorActor, TorusRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetTorusRadius(InitialTorusRadius + InAccumulatedTranslation.Y);
						});
					}

					return true;
				}
			}

			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Z)
			{
				if (Type == ECEClonerEffectorType::Torus)
				{
					if (EditingHandleType == HandleTypeInnerZone)
					{
						ModifyProperty(EffectorActor, TorusInnerRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetTorusInnerRadius(InitialTorusInnerRadius + InAccumulatedTranslation.Z);
						});
					}
					else if (EditingHandleType == HandleTypeOuterZone)
					{
						ModifyProperty(EffectorActor, TorusOuterRadiusProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedTranslation]()
						{
							EffectorActor->SetTorusOuterRadius(InitialTorusOuterRadius + InAccumulatedTranslation.Z);
						});
					}

					return true;
				}
			}
		}
		else if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Rotate)
		{
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Z)
			{
				if (Type == ECEClonerEffectorType::Radial)
				{
					if (EditingHandleType == HandleTypeAngle)
					{
						ModifyProperty(EffectorActor, RadialAngleProperty, EPropertyChangeType::Interactive, [this, &EffectorActor, &InAccumulatedRotation]()
						{
							EffectorActor->SetRadialAngle(InitialRadialAngle + InAccumulatedRotation.Yaw);
						});
					}

					return true;
				}
			}
		}
	}
	else
	{
		EndEditing();
	}

	return Super::HandleInputDeltaInternal(InViewportClient, InViewport, InAccumulatedTranslation, InAccumulatedRotation, InAccumulatedScale);
}

void FAvaEffectorActorVisualizer::DrawVisualizationEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != ECEClonerEffectorType::Plane)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeInnerZone, FAvaVisualizerBase::Active);
		InOutIconIndex++;
	}

	DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeOuterZone, FAvaVisualizerBase::Active);
	InOutIconIndex++;

	if (EffectorActor->GetType() == ECEClonerEffectorType::Radial)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeAngle, FAvaVisualizerBase::Active);
		InOutIconIndex++;
	}

	if (EffectorActor->GetType() == ECEClonerEffectorType::Torus)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeRadius, FAvaVisualizerBase::Active);
		InOutIconIndex++;
	}
}

void FAvaEffectorActorVisualizer::DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != ECEClonerEffectorType::Plane)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeInnerZone, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}

	DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeOuterZone, FAvaVisualizerBase::Inactive);
	InOutIconIndex++;

	if (EffectorActor->GetType() == ECEClonerEffectorType::Radial)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeAngle, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}

	if (EffectorActor->GetType() == ECEClonerEffectorType::Torus)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, HandleTypeRadius, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
}

FVector FAvaEffectorActorVisualizer::GetHandleZoneLocation(const ACEEffectorActor* InEffectorActor, int32 InHandleType) const
{
	const ECEClonerEffectorType Type = InEffectorActor->GetType();
	const FVector EffectorScale = InEffectorActor->GetActorScale();
	const FRotator EffectorRotation = InEffectorActor->GetActorRotation();
	FVector OutLocation = InEffectorActor->GetActorLocation();

	// To avoid inner/outer handle to be near actor gizmo and hard to select
	constexpr float MinHandleOffset = 50.f;
	constexpr float MaxHandleOffset = 100.f;

	if (Type == ECEClonerEffectorType::Box)
	{
		if (InHandleType == HandleTypeInnerZone)
		{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetInnerExtent()) * EffectorScale;
		}
		else if (InHandleType == HandleTypeOuterZone)
		{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetOuterExtent()) * EffectorScale;
		}
	}
	else if (Type == ECEClonerEffectorType::Plane)
	{
		if (InHandleType == HandleTypeInnerZone
			|| InHandleType == HandleTypeOuterZone)
		{
			const float ComponentScale = (EffectorRotation.RotateVector(-FVector::YAxisVector) * EffectorScale).Length();
			const FVector HandleAxis = EffectorRotation.RotateVector(FVector::YAxisVector);

			OutLocation += HandleAxis * (InEffectorActor->GetPlaneSpacing() / 2) * ComponentScale;
		}
	}
	else if (Type == ECEClonerEffectorType::Sphere)
	{
		const float MinComponentScale = FMath::Min<float>(FMath::Min<float>(EffectorScale.X, EffectorScale.Y), EffectorScale.Z);
		const FVector HandleAxis = EffectorRotation.RotateVector(FVector::YAxisVector);

		if (InHandleType == HandleTypeInnerZone)
		{
			OutLocation += FVector::Max(HandleAxis * InEffectorActor->GetInnerRadius(), HandleAxis * MinHandleOffset) * MinComponentScale;
		}
		else if (InHandleType == HandleTypeOuterZone)
		{
			OutLocation += FVector::Max(HandleAxis * InEffectorActor->GetOuterRadius(), HandleAxis * MaxHandleOffset) * MinComponentScale;
		}
	}
	else if (Type == ECEClonerEffectorType::Radial)
	{
		const float MinComponentScale = FMath::Min<float>(FMath::Min<float>(EffectorScale.X, EffectorScale.Y), EffectorScale.Z);
		const FVector UpHandleAxis = EffectorRotation.RotateVector(FVector::ZAxisVector);
		const FVector RightHandleAxis = EffectorRotation.RotateVector(FVector::YAxisVector);

		if (InHandleType == HandleTypeInnerZone)
		{
			OutLocation += FVector::Max(RightHandleAxis * InEffectorActor->GetRadialMinRadius(), RightHandleAxis * MinHandleOffset) * MinComponentScale;
		}
		else if (InHandleType == HandleTypeOuterZone)
		{
			OutLocation += FVector::Max(RightHandleAxis * InEffectorActor->GetRadialMaxRadius(), RightHandleAxis * MaxHandleOffset) * MinComponentScale;
		}
		else if (InHandleType == HandleTypeAngle)
		{
			OutLocation += UpHandleAxis * MaxHandleOffset * MinComponentScale;
		}
	}
	else if (Type == ECEClonerEffectorType::Torus)
	{
		const float MinComponentScale = FMath::Min<float>(FMath::Min<float>(EffectorScale.X, EffectorScale.Y), EffectorScale.Z);
		const FVector UpHandleAxis = EffectorRotation.RotateVector(FVector::ZAxisVector);
		const FVector RightHandleAxis = EffectorRotation.RotateVector(FVector::YAxisVector);

		if (InHandleType == HandleTypeInnerZone)
		{
			OutLocation += RightHandleAxis * InEffectorActor->GetTorusRadius()
				+ FVector::Max(UpHandleAxis * InEffectorActor->GetTorusInnerRadius(), UpHandleAxis * MinHandleOffset) * MinComponentScale;
		}
		else if (InHandleType == HandleTypeOuterZone)
		{
			OutLocation += RightHandleAxis * InEffectorActor->GetTorusRadius()
				+ FVector::Max(UpHandleAxis * InEffectorActor->GetTorusOuterRadius(), UpHandleAxis * MaxHandleOffset) * MinComponentScale;
		}
		else if (InHandleType == HandleTypeRadius)
		{
			OutLocation += FVector::Max(RightHandleAxis * InEffectorActor->GetTorusRadius(), RightHandleAxis * MinHandleOffset) * MinComponentScale;
		}
	}

	return OutLocation;
}

void FAvaEffectorActorVisualizer::DrawZoneButton(const ACEEffectorActor* InEffectorActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, int32 InHandleType, FLinearColor InColor) const
{
	UTexture2D* ZoneSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(UE::AvaShapes::BevelSprite);

	if (!ZoneSprite || !ZoneSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	IconLocation = GetHandleZoneLocation(InEffectorActor, InHandleType);

	InPDI->SetHitProxy(new HAvaEffectorActorZoneHitProxy(InEffectorActor->GetRootComponent(), InHandleType));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, ZoneSprite->GetResource(), InColor, SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

UActorComponent* FAvaEffectorActorVisualizer::GetEditedComponent() const
{
	return EffectorActorWeak.IsValid() ? EffectorActorWeak->GetRootComponent() : nullptr;
}

TMap<UObject*, TArray<FProperty*>> FAvaEffectorActorVisualizer::GatherEditableProperties(UObject* InObject) const
{
	if (const UCEEffectorComponent* EffectorComponent = Cast<UCEEffectorComponent>(InObject))
	{
		if (ACEEffectorActor* EffectorActor = EffectorComponent->GetOuterACEEffectorActor())
		{
			switch (EffectorActor->GetType())
			{
				case ECEClonerEffectorType::Plane:
					return {{EffectorActor, {PlaneSpacingProperty}}};

				case ECEClonerEffectorType::Box:
					return {{EffectorActor, {InnerExtentProperty, OuterExtentProperty}}};

				case ECEClonerEffectorType::Sphere:
					return {{EffectorActor, {InnerRadiusProperty, OuterRadiusProperty}}};

				case ECEClonerEffectorType::Radial:
					return {{EffectorActor, {RadialAngleProperty, RadialMinRadiusProperty, RadialMaxRadiusProperty}}};

				case ECEClonerEffectorType::Torus:
					return {{EffectorActor, {TorusRadiusProperty, TorusInnerRadiusProperty, TorusOuterRadiusProperty}}};

				default:
					return {};
			}
		}
	}

	return {};
}

bool FAvaEffectorActorVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* InVisProxy, const FViewportClick& InClick)
{
	if (InClick.GetKey() != EKeys::LeftMouseButton)
	{
		EndEditing();
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	UActorComponent* Component = const_cast<UActorComponent*>(InVisProxy->Component.Get());

	if (!Component || !Component->GetOwner()->IsA<ACEEffectorActor>())
	{
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	if (InVisProxy->IsA(HAvaEffectorActorZoneHitProxy::StaticGetType()))
	{
		EndEditing();
		EffectorActorWeak = Cast<ACEEffectorActor>(InVisProxy->Component->GetOwner());
		EditingHandleType = static_cast<HAvaEffectorActorZoneHitProxy*>(InVisProxy)->HandleType;
		StartEditing(InViewportClient, Component);

		return true;
	}

	return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
}

bool FAvaEffectorActorVisualizer::GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const
{
	if (const ACEEffectorActor* EffectorActor = EffectorActorWeak.Get())
	{
		OutLocation = GetHandleZoneLocation(EffectorActor, EditingHandleType);
		return true;
	}

	return Super::GetWidgetLocation(InViewportClient, OutLocation);
}

bool FAvaEffectorActorVisualizer::GetWidgetMode(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode& OutMode) const
{
	if (EditingHandleType == HandleTypeInnerZone
		|| EditingHandleType == HandleTypeOuterZone
		|| EditingHandleType == HandleTypeRadius)
	{
		OutMode = UE::Widget::EWidgetMode::WM_Translate;
		return true;
	}

	if (EditingHandleType == HandleTypeAngle)
	{
		OutMode = UE::Widget::EWidgetMode::WM_Rotate;
		return true;
	}

	return Super::GetWidgetMode(InViewportClient, OutMode);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisList(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (EditingHandleType == HandleTypeInnerZone
		|| EditingHandleType == HandleTypeOuterZone)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Torus)
		{
			OutAxisList = EAxisList::Type::Z;
		}
		else if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::XYZ;
		}
		else
		{
			OutAxisList = EAxisList::Type::Y;
		}

		return true;
	}

	if (EditingHandleType == HandleTypeRadius)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Torus)
		{
			OutAxisList = EAxisList::Type::Y;
		}

		return true;
	}

	if (EditingHandleType == HandleTypeAngle)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Radial)
		{
			OutAxisList = EAxisList::Type::Z;
		}

		return true;
	}

	return Super::GetWidgetAxisList(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (EditingHandleType == HandleTypeInnerZone
		|| EditingHandleType == HandleTypeOuterZone)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Torus)
		{
			OutAxisList = EAxisList::Type::Z;
			return true;
		}

		if (EffectorActorWeak->GetType() != ECEClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::Y;
			return true;
		}
	}

	if (EditingHandleType == HandleTypeRadius)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Torus)
		{
			OutAxisList = EAxisList::Type::Y;
			return true;
		}
	}

	if (EditingHandleType == HandleTypeAngle)
	{
		if (EffectorActorWeak->GetType() == ECEClonerEffectorType::Radial)
		{
			OutAxisList = EAxisList::Type::Z;
		}

		return true;
	}

	return Super::GetWidgetAxisListDragOverride(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaEffectorActorVisualizer::ResetValue(FEditorViewportClient* InViewportClient, HHitProxy* InHitProxy)
{
	if (!InHitProxy->IsA(HAvaEffectorActorZoneHitProxy::StaticGetType()))
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	const HAvaEffectorActorZoneHitProxy* ComponentHitProxy = static_cast<HAvaEffectorActorZoneHitProxy*>(InHitProxy);

	if (!ComponentHitProxy->Component.IsValid() || !ComponentHitProxy->Component->IsA<UCEEffectorComponent>())
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	if (ACEEffectorActor* EffectorActor = Cast<ACEEffectorActor>(ComponentHitProxy->Component->GetOwner()))
	{
		const ECEClonerEffectorType Type = EffectorActor->GetType();
		const int32 HandleType = ComponentHitProxy->HandleType;

		FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));

		if (Type == ECEClonerEffectorType::Box)
		{
			if (HandleType == HandleTypeInnerZone)
			{
				ModifyProperty(EffectorActor, InnerExtentProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetInnerExtent(FVector(50.f));
				});
			}
			else if (HandleType == HandleTypeOuterZone)
			{
				ModifyProperty(EffectorActor, OuterExtentProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetOuterExtent(FVector(200.f));
				});
			}
		}
		else if (Type == ECEClonerEffectorType::Plane)
		{
			if (HandleType == HandleTypeInnerZone
				|| HandleType == HandleTypeOuterZone)
			{
				ModifyProperty(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetPlaneSpacing(200.f);
				});
			}
		}
		else if (Type == ECEClonerEffectorType::Sphere)
		{
			if (HandleType == HandleTypeInnerZone)
			{
				ModifyProperty(EffectorActor, InnerRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetInnerRadius(50.f);
				});
			}
			else if (HandleType == HandleTypeOuterZone)
			{
				ModifyProperty(EffectorActor, OuterRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetOuterRadius(200.f);
				});
			}
		}
		else if (Type == ECEClonerEffectorType::Radial)
		{
			if (HandleType == HandleTypeInnerZone)
			{
				ModifyProperty(EffectorActor, RadialMinRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetRadialMinRadius(0.f);
				});
			}
			else if (HandleType == HandleTypeOuterZone)
			{
				ModifyProperty(EffectorActor, RadialMaxRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetRadialMaxRadius(1000.f);
				});
			}
			else if (HandleType == HandleTypeAngle)
			{
				ModifyProperty(EffectorActor, RadialAngleProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetRadialAngle(180.f);
				});
			}
		}
		else if (Type == ECEClonerEffectorType::Torus)
		{
			if (HandleType == HandleTypeInnerZone)
			{
				ModifyProperty(EffectorActor, TorusInnerRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetTorusInnerRadius(50.f);
				});
			}
			else if (HandleType == HandleTypeOuterZone)
			{
				ModifyProperty(EffectorActor, TorusOuterRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetTorusOuterRadius(200.f);
				});
			}
			else if (HandleType == HandleTypeRadius)
			{
				ModifyProperty(EffectorActor, TorusRadiusProperty, EPropertyChangeType::ValueSet, [&EffectorActor]()
				{
					EffectorActor->SetTorusRadius(250.f);
				});
			}
		}
	}

	return true;
}

bool FAvaEffectorActorVisualizer::IsEditing() const
{
	if (EditingHandleType != INDEX_NONE)
	{
		return true;
	}

	return Super::IsEditing();
}

void FAvaEffectorActorVisualizer::EndEditing()
{
	Super::EndEditing();

	EffectorActorWeak.Reset();
	EditingHandleType = INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE

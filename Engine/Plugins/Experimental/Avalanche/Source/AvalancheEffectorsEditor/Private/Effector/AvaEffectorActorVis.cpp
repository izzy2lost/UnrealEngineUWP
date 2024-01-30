// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effector/AvaEffectorActorVis.h"

#include "AvaField.h"
#include "AvalancheShapesEditorModule.h"
#include "AvaVisBase.h"
#include "EditorViewportClient.h"
#include "Effector/AvaEffectorComponent.h"
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
	InnerRadiusProperty  = GetProperty<AAvaEffectorActor>(GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, InnerRadius));
	OuterRadiusProperty  = GetProperty<AAvaEffectorActor>(GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OuterRadius));
	InnerExtentProperty  = GetProperty<AAvaEffectorActor>(GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, InnerExtent));
	OuterExtentProperty  = GetProperty<AAvaEffectorActor>(GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, OuterExtent));
	PlaneSpacingProperty = GetProperty<AAvaEffectorActor>(GET_MEMBER_NAME_CHECKED(AAvaEffectorActor, PlaneSpacing));
}

void FAvaEffectorActorVisualizer::StoreInitialValues()
{
	Super::StoreInitialValues();
	
	if (GetEditedComponent() == nullptr)
	{
		return;
	}

	InitialInnerRadius = EffectorActorWeak->GetInnerRadius();
	InitialOuterRadius = EffectorActorWeak->GetOuterRadius();
	InitialInnerExtent = EffectorActorWeak->GetInnerExtent();
	InitialOuterExtent = EffectorActorWeak->GetOuterExtent();
	InitialPlaneSpacing = EffectorActorWeak->GetPlaneSpacing();
}

FBox FAvaEffectorActorVisualizer::GetComponentBounds(const UActorComponent* InComponent) const
{
	if (const UAvaEffectorComponent* EffectorComponent = Cast<UAvaEffectorComponent>(InComponent))
	{
		if (const AAvaEffectorActor* EffectorActor = Cast<AAvaEffectorActor>(EffectorComponent->GetOwner()))
		{
			if (EffectorActor->GetType() == EAvaClonerEffectorType::Box)
			{
				return FBox(-EffectorActor->GetOuterExtent(), EffectorActor->GetOuterExtent());
			}
			if (EffectorActor->GetType() == EAvaClonerEffectorType::Sphere)
			{
				return FBox(-FVector(EffectorActor->GetOuterRadius() / 2), FVector(EffectorActor->GetOuterRadius() / 2));
			}
			if (EffectorActor->GetType() == EAvaClonerEffectorType::Plane)
			{
				return FBox(-FVector(EffectorActor->GetPlaneSpacing() / 2), FVector(EffectorActor->GetPlaneSpacing() / 2));
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

	if (AAvaEffectorActor* EffectorActor = EffectorActorWeak.Get())
	{
		if (GetViewportWidgetMode(InViewportClient) == UE::Widget::WM_Translate)
		{
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::XYZ)
			{
				if (EffectorActor->GetType() == EAvaClonerEffectorType::Box)
				{
					if (bEditingInnerZone)
					{
						EffectorActor->SetInnerExtent(InitialInnerExtent + InAccumulatedTranslation);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, InnerExtentProperty, EPropertyChangeType::Interactive);
					}
					else if (bEditingOuterZone)
					{
						EffectorActor->SetOuterExtent(InitialOuterExtent + InAccumulatedTranslation);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, OuterExtentProperty, EPropertyChangeType::Interactive);
					}
					
					return true;
				}
			}
			if (GetViewportWidgetAxisList(InViewportClient) & EAxisList::Y)
			{
				if (EffectorActor->GetType() == EAvaClonerEffectorType::Plane)
				{
					if (bEditingOuterZone || bEditingInnerZone)
					{
						EffectorActor->SetPlaneSpacing(InitialPlaneSpacing + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::Interactive);
					}
					
					return true;
				}
				if (EffectorActor->GetType() == EAvaClonerEffectorType::Sphere)
				{
					if (bEditingInnerZone)
					{
						EffectorActor->SetInnerRadius(InitialInnerRadius + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, InnerRadiusProperty, EPropertyChangeType::Interactive);
					}
					else if (bEditingOuterZone)
					{
						EffectorActor->SetOuterRadius(InitialOuterRadius + InAccumulatedTranslation.Y);
						EffectorActor->Modify();
						bHasBeenModified = true;
						NotifyPropertyModified(EffectorActor, OuterRadiusProperty, EPropertyChangeType::Interactive);
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

	const UAvaEffectorComponent* EffectorComponent = Cast<UAvaEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const AAvaEffectorActor* EffectorActor = Cast<AAvaEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != EAvaClonerEffectorType::Plane)
	{
		if (!bEditingInnerZone)
		{
			DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, true, FAvaVisualizerBase::Inactive);
			InOutIconIndex++;
		}
	}

	if (!bEditingOuterZone)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, false, bEditingOuterZone ? FAvaVisualizerBase::Active : FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
}

void FAvaEffectorActorVisualizer::DrawVisualizationNotEditing(const UActorComponent* InComponent, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32& InOutIconIndex)
{
	Super::DrawVisualizationNotEditing(InComponent, InView, InPDI, InOutIconIndex);

	const UAvaEffectorComponent* EffectorComponent = Cast<UAvaEffectorComponent>(InComponent);

	if (!EffectorComponent)
	{
		return;
	}

	const AAvaEffectorActor* EffectorActor = Cast<AAvaEffectorActor>(EffectorComponent->GetOwner());

	if (!EffectorActor)
	{
		return;
	}

	if (EffectorActor->GetType() != EAvaClonerEffectorType::Plane)
	{
		DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, true, FAvaVisualizerBase::Inactive);
		InOutIconIndex++;
	}
	
	DrawZoneButton(EffectorActor, InView, InPDI, InOutIconIndex, false, FAvaVisualizerBase::Inactive);
	InOutIconIndex++;
}

FVector FAvaEffectorActorVisualizer::GetHandleZoneLocation(const AAvaEffectorActor* InEffectorActor, bool bInInnerSize) const
{
	const FVector EffectorScale = InEffectorActor->GetActorScale();
	const FRotator EffectorRotation = InEffectorActor->GetActorRotation();
	FVector OutLocation = InEffectorActor->GetActorLocation();

	if (InEffectorActor->GetType() == EAvaClonerEffectorType::Box)
	{
		if (bInInnerSize)
		{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetInnerExtent()) * EffectorScale;
		}
		else
			{
			OutLocation += EffectorRotation.RotateVector(InEffectorActor->GetOuterExtent()) * EffectorScale;
		}
	}
	else if (InEffectorActor->GetType() == EAvaClonerEffectorType::Plane)
	{
		const float ComponentScale = (EffectorRotation.RotateVector(-FVector::YAxisVector) * EffectorScale).Length();
		OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * (InEffectorActor->GetPlaneSpacing() / 2) * ComponentScale;
	}
	else if (InEffectorActor->GetType() == EAvaClonerEffectorType::Sphere)
	{
		const float MinComponentScale = FMath::Min<float>(FMath::Min<float>(EffectorScale.X, EffectorScale.Y), EffectorScale.Z);
		if (bInInnerSize)
		{
			OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * InEffectorActor->GetInnerRadius() * MinComponentScale;
		}
		else
		{
			OutLocation += EffectorRotation.RotateVector(FVector::YAxisVector) * InEffectorActor->GetOuterRadius() * MinComponentScale;
		}
	}
	
	return OutLocation;
}

void FAvaEffectorActorVisualizer::DrawZoneButton(const AAvaEffectorActor* InEffectorActor, const FSceneView* InView, FPrimitiveDrawInterface* InPDI, int32 InIconIndex, bool bInInnerZone, FLinearColor InColor) const
{
	UTexture2D* ZoneSprite = IAvalancheComponentVisualizersModule::Get().GetSettings()->GetVisualizerSprite(FAvalancheShapesEditorModule::BevelSprite);

	if (!ZoneSprite || !ZoneSprite->GetResource())
	{
		return;
	}

	FVector IconLocation;
	float IconSize;
	GetIconMetrics(InView, InIconIndex, IconLocation, IconSize);

	IconLocation = GetHandleZoneLocation(InEffectorActor, bInInnerZone);

	InPDI->SetHitProxy(new HAvaEffectorActorZoneHitProxy(InEffectorActor->GetRootComponent(), bInInnerZone));
	InPDI->DrawSprite(IconLocation, IconSize, IconSize, ZoneSprite->GetResource(), InColor, SDPG_Foreground, 0, 0, 0, 0, SE_BLEND_Opaque);
	InPDI->SetHitProxy(nullptr);
}

UActorComponent* FAvaEffectorActorVisualizer::GetEditedComponent() const
{
	return EffectorActorWeak.IsValid() ? EffectorActorWeak->GetRootComponent() : nullptr;
}

TMap<UObject*, TArray<FProperty*>> FAvaEffectorActorVisualizer::GatherEditableProperties(UObject* InObject) const
{
	if (UAvaEffectorComponent* EffectorComponent = Cast<UAvaEffectorComponent>(InObject))
	{
		if (AAvaEffectorActor* EffectorActor = EffectorComponent->GetOuterAAvaEffectorActor())
		{
			switch (EffectorActor->GetType())
			{
				case EAvaClonerEffectorType::Plane:
					return {{EffectorActor, {PlaneSpacingProperty}}};

				case EAvaClonerEffectorType::Box:
					return {{EffectorActor, {InnerExtentProperty, OuterExtentProperty}}};

				case EAvaClonerEffectorType::Sphere:
					return {{EffectorActor, {InnerRadiusProperty, OuterRadiusProperty}}};
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

	if (!Component || !Component->GetOwner()->IsA<AAvaEffectorActor>())
	{
		return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
	}

	if (InVisProxy->IsA(HAvaEffectorActorZoneHitProxy::StaticGetType()))
	{
		EndEditing();
		EffectorActorWeak = Cast<AAvaEffectorActor>(InVisProxy->Component->GetOwner());
		bEditingInnerZone = static_cast<HAvaEffectorActorZoneHitProxy*>(InVisProxy)->bInnerZone;
		bEditingOuterZone = !bEditingInnerZone;
		StartEditing(InViewportClient, Component);
		
		return true;
	}

	return Super::VisProxyHandleClick(InViewportClient, InVisProxy, InClick);
}

bool FAvaEffectorActorVisualizer::GetWidgetLocation(const FEditorViewportClient* InViewportClient, FVector& OutLocation) const
{
	if (EffectorActorWeak.IsValid())
	{
		OutLocation = GetHandleZoneLocation(EffectorActorWeak.Get(), bEditingInnerZone);
		return true;
	}
	
	return Super::GetWidgetLocation(InViewportClient, OutLocation);
}

bool FAvaEffectorActorVisualizer::GetWidgetMode(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode& OutMode) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		OutMode = UE::Widget::EWidgetMode::WM_Translate;
		return true;
	}
	
	return Super::GetWidgetMode(InViewportClient, OutMode);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisList(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		if (EffectorActorWeak->GetType() != EAvaClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::Y;
		}
		else
		{
			OutAxisList = EAxisList::Type::XYZ;
		}
		
		return true;
	}
	
	return Super::GetWidgetAxisList(InViewportClient, InWidgetMode, OutAxisList);
}

bool FAvaEffectorActorVisualizer::GetWidgetAxisListDragOverride(const FEditorViewportClient* InViewportClient,
	UE::Widget::EWidgetMode InWidgetMode, EAxisList::Type& OutAxisList) const
{
	if (bEditingOuterZone || bEditingInnerZone)
	{
		if (EffectorActorWeak->GetType() != EAvaClonerEffectorType::Box)
		{
			OutAxisList = EAxisList::Type::Y;
			return true;
		}
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
	
	if (!ComponentHitProxy->Component.IsValid() || !ComponentHitProxy->Component->IsA<UAvaEffectorComponent>())
	{
		return Super::ResetValue(InViewportClient, InHitProxy);
	}

	if (AAvaEffectorActor* EffectorActor = Cast<AAvaEffectorActor>(ComponentHitProxy->Component->GetOwner()))
	{
		if (EffectorActor->GetType() == EAvaClonerEffectorType::Box)
		{
			if (ComponentHitProxy->bInnerZone)
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetInnerExtent(FVector(50.f));
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, InnerExtentProperty, EPropertyChangeType::ValueSet);
			}
			else
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetOuterExtent(FVector(200.f));
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, OuterExtentProperty, EPropertyChangeType::ValueSet);
			}
		}
		else if (EffectorActor->GetType() == EAvaClonerEffectorType::Plane)
		{
			FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
			EffectorActor->SetFlags(RF_Transactional);
			EffectorActor->SetPlaneSpacing(200.f);
			EffectorActor->Modify();
			NotifyPropertyModified(EffectorActor, PlaneSpacingProperty, EPropertyChangeType::ValueSet);
		}
		else if (EffectorActor->GetType() == EAvaClonerEffectorType::Sphere)
		{
			if (ComponentHitProxy->bInnerZone)
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetInnerRadius(50.f);
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, InnerRadiusProperty, EPropertyChangeType::ValueSet);
			}
			else
			{
				FScopedTransaction Transaction(LOCTEXT("VisualizerResetValue", "Visualizer Reset Value"));
				EffectorActor->SetFlags(RF_Transactional);
				EffectorActor->SetOuterRadius(200.f);
				EffectorActor->Modify();
				NotifyPropertyModified(EffectorActor, OuterRadiusProperty, EPropertyChangeType::ValueSet);
			}
		}
	}
	
	return true;
}

bool FAvaEffectorActorVisualizer::IsEditing() const
{
	if (bEditingInnerZone || bEditingOuterZone)
	{
		return true;
	}
	
	return Super::IsEditing();
}

void FAvaEffectorActorVisualizer::EndEditing()
{
	Super::EndEditing();

	EffectorActorWeak.Reset();
	bEditingInnerZone = false;
	bEditingOuterZone = false;
}

#undef LOCTEXT_NAMESPACE

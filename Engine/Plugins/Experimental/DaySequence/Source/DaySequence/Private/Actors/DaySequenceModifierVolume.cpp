// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/DaySequenceModifierVolume.h"

#include "DaySequenceActor.h"
#include "DaySequenceModifierComponent.h"
#include "DaySequenceSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameDelegates.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ADaySequenceModifierVolume::ADaySequenceModifierVolume(const FObjectInitializer& Init)
: Super(Init)
{
	PrimaryActorTick.bCanEverTick = true;
	
	DaySequenceModifier = CreateDefaultSubobject<UDaySequenceModifierComponent>(TEXT("DaySequenceModifier"));
	DaySequenceModifier->SetupAttachment(RootComponent);

	DefaultBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	DefaultBox->SetupAttachment(DaySequenceModifier);
	DefaultBox->SetLineThickness(10.f);
	DefaultBox->SetBoxExtent(FVector(500.f));
}

void ADaySequenceModifierVolume::TryEnableModifier() const
{	
	if (DaySequenceModifier->IsBlendTargetInAnyVolume())
	{
		DaySequenceModifier->EnableModifier();
	}
	else
	{
		DaySequenceModifier->DisableModifier();
	}
}

void ADaySequenceModifierVolume::SetBlendTarget(AActor* InBlendTarget)
{
	if (!IsValid(InBlendTarget) || InBlendTarget == CurrentBlendTarget)
	{
		return;
	}

	CurrentBlendTarget = InBlendTarget;

	DaySequenceModifier->EnableDistanceVolumeBlends(InBlendTarget);
	DaySequenceModifier->SetCustomVolumeBlendWeight(1.f);

	TryEnableModifier();
}

void ADaySequenceModifierVolume::BeginPlay()
{
	Super::BeginPlay();

	Initialize();
}

void ADaySequenceModifierVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	Initialize();
}

void ADaySequenceModifierVolume::Initialize()
{
	if (IsTemplate())
	{
		return;
	}

	// This actor should only initialize on the client.
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorEnableCollision(false);
		return;
	}

#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::Editor)
	{
		DaySequenceActor = nullptr;
		if (IsValid(DaySequenceModifier))
		{
			DaySequenceModifier->UnbindFromDaySequenceActor();
		}
	}
#endif

	DaySequenceActorSetup();
}

void ADaySequenceModifierVolume::VolumeSetup()
{
	DaySequenceModifier->EmptyVolumeShapeComponents();
	
	AddShapeComponentsToModifier();

	// this is kind of a hack, we are calling this for the side effect of forcing the modifier to recache volume shapes after we just invalidated them.
	DaySequenceModifier->IsBlendTargetInAnyVolume();
	
#if WITH_EDITOR
	const UWorld* World = GetWorld();
	if (World && World->WorldType != EWorldType::Editor)
	{
#endif
	SetupVolumeCallbacks();
#if WITH_EDITOR
	}
#endif
}

void ADaySequenceModifierVolume::AddShapeComponentsToModifier()
{
	FComponentReference DefaultBoxReference;
	DefaultBoxReference.OverrideComponent = DefaultBox;
	DaySequenceModifier->AddVolumeShapeComponent(DefaultBoxReference);


}

void ADaySequenceModifierVolume::SetupVolumeCallbacks()
{
	if (!DaySequenceModifier->GetOnVolumeBlendTargetOverlapBegin().IsBoundToObject(this))
	{
		DaySequenceModifier->GetOnVolumeBlendTargetOverlapBegin().AddWeakLambda(this, [this](AActor*)
		{
			OnOverlapBegin();
		});
	}
	
	if (!DaySequenceModifier->GetOnVolumeBlendTargetOverlapEnd().IsBoundToObject(this))
	{
		DaySequenceModifier->GetOnVolumeBlendTargetOverlapEnd().AddWeakLambda(this, [this](AActor*)
		{
			OnOverlapEnd();
		});
	}
}

void ADaySequenceModifierVolume::OnOverlapBegin() const
{
	DaySequenceModifier->EnableModifier();
}

void ADaySequenceModifierVolume::OnOverlapEnd() const
{
	DaySequenceModifier->DisableModifier();
}

void ADaySequenceModifierVolume::PlayerControllerSetup()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (World && World->WorldType != EWorldType::Editor)
	{
#endif
	SetupBlendTargetCallbacks();
	CachePlayerController();
#if WITH_EDITOR
	}
#endif
}

void ADaySequenceModifierVolume::CachePlayerController()
{
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APlayerController* PlayerController = Iterator->Get())
			{
				if (PlayerController->IsLocalPlayerController())
				{
					CachedPlayerController = PlayerController;
					break;
				}
			}
		}
	}

	// If no local player controller found, queue this function for next tick.
	if (!IsValid(CachedPlayerController))
	{
		QueuePlayerControllerQuery();
	}
	else
	{
		if (AActor* ViewTarget = CachedPlayerController->GetViewTarget())
		{
			SetBlendTarget(ViewTarget);
		}
	}
}

void ADaySequenceModifierVolume::QueuePlayerControllerQuery()
{
	if (!IsValid(this))
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this]()
		{
			CachePlayerController();
		});
	}
}

void ADaySequenceModifierVolume::SetupBlendTargetCallbacks()
{
	if (!ViewTargetChangedHandle.IsValid())
	{
		ViewTargetChangedHandle = FGameDelegates::Get().GetViewTargetChangedDelegate().AddWeakLambda(this, [this](APlayerController* PC, AActor* OldTarget, AActor* NewTarget)
		{
			if (!IsValid(CachedPlayerController))
			{
				QueuePlayerControllerQuery();
				return;
			}

			if (PC != CachedPlayerController)
			{
				return;
			}

			// there is a lot of extra logic we can do here to stop popping (for example, try cast NewTarget to APlayerController which happens when view target unset).
			// for now just set
			SetBlendTarget(NewTarget);
		});
	}
}

void ADaySequenceModifierVolume::DaySequenceActorSetup()
{
	SetupDaySequenceSubsystemCallbacks();
	BindToDaySequenceActor();
}

void ADaySequenceModifierVolume::BindToDaySequenceActor()
{
	bool bNewActorSet = false;
	
	if (const UWorld* World = GetWorld())
	{
		if (const UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			if (ADaySequenceActor* NewActor = DaySequenceSubsystem->GetDaySequenceActor())
			{
				if (NewActor != DaySequenceActor)
				{
					DaySequenceActor = NewActor;
					bNewActorSet = true;
				}
			}
		}
	}
	
	if (bNewActorSet)
	{
		DaySequenceModifier->BindToDaySequenceActor(DaySequenceActor);

		VolumeSetup();
		PlayerControllerSetup();

		OnDaySequenceActorBound(DaySequenceActor);
	}
}

void ADaySequenceModifierVolume::SetupDaySequenceSubsystemCallbacks()
{
	if (const UWorld* World = GetWorld())
	{
		if (UDaySequenceSubsystem* DaySequenceSubsystem = World->GetSubsystem<UDaySequenceSubsystem>())
		{
			// Prevent consecutive calls to this function from adding redundant lambdas to invocation list.
			if (!DaySequenceSubsystem->OnDaySequenceActorSetEvent.IsBoundToObject(this))
			{
				DaySequenceSubsystem->OnDaySequenceActorSetEvent.AddWeakLambda(this, [this](ADaySequenceActor* InActor)
				{
					BindToDaySequenceActor();
				});
			}
		}
	}
}
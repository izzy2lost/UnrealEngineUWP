// Copyright Epic Games, Inc. All Rights Reserved.

#include "DaySequenceModifierComponent.h"

#include "Components/BoxComponent.h"
#include "DaySequence.h"
#include "DaySequenceCollectionAsset.h"
#include "DaySequenceModule.h"
#include "DaySequenceTrack.h"

#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"

#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"

#include "Tracks/MovieSceneSubTrack.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneDoubleTrack.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "Tracks/MovieSceneMaterialTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"

#include "Sections/MovieSceneSubSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneDoubleSection.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieScenePrimitiveMaterialSection.h"

#include "ProfilingDebugging/CsvProfiler.h"

#if ENABLE_DRAW_DEBUG
#endif

#define LOCTEXT_NAMESPACE "DaySequenceModifierComponent"

namespace UE::DaySequence
{
	template<typename TrackType>
	TrackType* CreateOrAddOverrideTrack(UMovieScene* MovieScene, const FGuid& ObjectGuid, FName Name = NAME_None)
	{
		TrackType* Track = MovieScene->FindTrack<TrackType>(ObjectGuid, Name);
		if (!Track)
		{
			// Clear RF_Transactional and set RF_Transient on created tracks and sections
			// to avoid dirtying the package for these procedurally generated sequences.
			// RF_Transactional is explicitly set in UMovieSceneSection/Track::PostInitProperties.
			Track = NewObject<TrackType>(MovieScene, NAME_None, RF_Transient);
			Track->ClearFlags(RF_Transactional);

			UMovieSceneSection* Section = Track->CreateNewSection();
			Section->ClearFlags(RF_Transactional);
			Section->SetFlags(RF_Transient);
			Section->SetRange(TRange<FFrameNumber>::All());

			Track->AddSection(*Section);
			MovieScene->AddGivenTrack(Track, ObjectGuid);
		}

		return Track;
	}

	template<typename TrackType>
	TrackType* CreateOrAddPropertyOverrideTrack(UMovieScene* MovieScene, const FGuid& ObjectGuid, FName InPropertyName)
	{
		TrackType* Track = CreateOrAddOverrideTrack<TrackType>(MovieScene, ObjectGuid, InPropertyName);
		check(Track);
		
		const FString PropertyPath = InPropertyName.ToString();

		// Split the property path to capture the leaf property name and parent struct to conform
		// with Sequencer Editor property name/path and display name conventions:
		//
		// PropertyName = MyProperty
		// PropertyPath = MyPropertyStruct.MyProperty
		// DisplayName = PropertyName (PropertyStruct)
		FName PropertyName;
		FName PropertyParent;
		int32 NamePos = INDEX_NONE;
		if (PropertyPath.FindLastChar('.', NamePos) && NamePos < PropertyPath.Len() - 1)
		{
			PropertyName = FName(FStringView(*PropertyPath + NamePos + 1, PropertyPath.Len() - NamePos - 1));
			PropertyParent = FName(FStringView(*PropertyPath, NamePos));
		}
		else
		{
			PropertyName = *PropertyPath;
		}
		
		Track->SetPropertyNameAndPath(PropertyName, PropertyPath);

#if WITH_EDITOR
		if (NamePos != INDEX_NONE)
		{
			FText DisplayText = FText::Format(LOCTEXT("ModifierPropertyTrackFormat", "{0} ({1})"), FText::FromName(PropertyName), FText::FromName(PropertyParent));
			Track->SetDisplayName(DisplayText);
		}
#endif
		return Track;
	}

	template<typename TrackType, typename SectionType>
	SectionType* CreateOrAddPropertyOverrideSection(UMovieScene* MovieScene, const FGuid& ObjectGuid, FName PropertyName)
	{
		TrackType* Track = CreateOrAddPropertyOverrideTrack<TrackType>(MovieScene, ObjectGuid, PropertyName);
		check(Track);
		return Cast<SectionType>(Track->GetAllSections()[0]);
	}

	template<typename TrackType, typename SectionType>
	SectionType* CreateOrAddOverrideSection(UMovieScene* MovieScene, const FGuid& ObjectGuid)
	{
		TrackType* Track = CreateOrAddOverrideTrack<TrackType>(MovieScene, ObjectGuid);
		check(Track);
		return Cast<SectionType>(Track->GetAllSections()[0]);
	}

	FVector GVolumePreviewLocation = FVector::ZeroVector;
	bool bIsSimulating = false;

	float ComputeBoxSignedDistance(const UBoxComponent* BoxComponent, const FVector& InWorldPosition)
	{
		const FTransform& ComponentTransform = BoxComponent->GetComponentTransform();

		FVector Point = ComponentTransform.InverseTransformPositionNoScale(InWorldPosition);
		FVector Box = BoxComponent->GetUnscaledBoxExtent() * ComponentTransform.GetScale3D();

		FVector Delta = Point.GetAbs() - Box;
		return FVector::Max(Delta, FVector::ZeroVector).Length() + FMath::Min(Delta.GetMax(), 0.0);
	}

	float ComputeSphereSignedDistance(const USphereComponent* SphereComponent, const FVector& InWorldPosition)
	{
		const FTransform& ComponentTransform = SphereComponent->GetComponentTransform();

		FVector Point = ComponentTransform.InverseTransformPositionNoScale(InWorldPosition);
		return Point.Length()-SphereComponent->GetScaledSphereRadius();
	}
	
	float ComputeCapsuleSignedDistance(const UCapsuleComponent* CapsuleComponent, const FVector& InWorldPosition)
	{
		// UCapsuleComponent::GetScaledCapsuleRadius() returns the min scaled X/Y axis for the radius
		// while the actual collision query uses the max scaled X/Y axis for the radius. We use Max here to match the collision.
		const FTransform& ComponentTransform = CapsuleComponent->GetComponentTransform();
		const FVector&    ComponentScale     = ComponentTransform.GetScale3D();

		FVector Point = ComponentTransform.InverseTransformPositionNoScale(InWorldPosition);
		const double CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight_WithoutHemisphere();
		const double CapsuleRadius = CapsuleComponent->GetUnscaledCapsuleRadius() * FMath::Max(ComponentScale.X, ComponentScale.Y);

		Point.Z = FMath::Max(FMath::Abs(Point.Z) - CapsuleHalfHeight, 0.0);
		return Point.Length() - CapsuleRadius;
	}

	float ComputeSignedDistance(const UShapeComponent* ShapeComponent, const FVector& InWorldPosition)
	{
		if (!ShapeComponent)
		{
			return UE_MAX_FLT;
		}
		
		if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(ShapeComponent))
		{
			return ComputeBoxSignedDistance(BoxComponent, InWorldPosition);
		}
		else if (const USphereComponent* SphereComponent = Cast<USphereComponent>(ShapeComponent))
		{
			return ComputeSphereSignedDistance(SphereComponent, InWorldPosition);
		}
		else if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(ShapeComponent))
		{
			return ComputeCapsuleSignedDistance(CapsuleComponent, InWorldPosition);
		}
		
		// @todo: unsupported shape?
		return (InWorldPosition - ShapeComponent->GetComponentLocation()).Length();
	}

	bool TestValidProperty(UObject* Object, FProperty* Property)
	{
		if (!Property)
		{
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Invalid property specified for object %s."), *Object->GetName()), ELogVerbosity::Error);
			return false;
		}

		if (Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			// Emit a warning for deprecated properties but still consider them valid
			FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Depcrecated property specified: %s for object %s."), *Property->GetName(), *Object->GetName()), ELogVerbosity::Warning);
		}

		return true;
	}

} // namespace UE::DaySequence

void UDaySequenceModifierEasingFunction::Initialize(EEasingFunctionType EasingType)
{
	if (UDaySequenceModifierComponent* Outer = Cast<UDaySequenceModifierComponent>(GetOuter()))
	{
		switch (EasingType)
		{
		case EEasingFunctionType::EaseIn:
			EvaluateImpl = [Outer](float)
			{
				return Outer->GetCurrentBlendWeight();
			};
			break;
	
		case EEasingFunctionType::EaseOut:
			EvaluateImpl = [Outer](float)
			{
				return 1.f - Outer->GetCurrentBlendWeight();
			};
			break;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Initialize called without a valid Outer!"));
		EvaluateImpl = [](float){ return 0.f; };
	}
}

float UDaySequenceModifierEasingFunction::Evaluate(float Interp) const
{
	return EvaluateImpl(Interp);
}

UDaySequenceModifierComponent::UDaySequenceModifierComponent(const FObjectInitializer& Init)
	: Super(Init)
{
	bIsComponentEnabled = true;
	bIsEnabled = false;
	bIgnoreBias = false;
	bUseVolume = true;
	bPreview = true;
	bUseCollection = false;
	bCachedExternalShapesInvalid = true;
	Bias = 1000;
	DayNightCycleTime = 12.f;
	DayNightCycle = EDayNightCycleMode::Default;
	BlendMode = EDaySequenceModifierBlendMode::Distance;
	BlendAmount = 100.f;
	CachedBlendFactor = 0.f;
	CustomVolumeBlendWeight = 1.0f;
#if ENABLE_DRAW_DEBUG
	DebugLevel = 0;
	
	// This gets captured by a lambda below so should continue living
	TSharedPtr<TMap<FString, FString>> DebugData = MakeShared<TMap<FString, FString>>();
	DebugEntry = MakeShared<UE::DaySequence::FDaySequenceDebugEntry>(
	[this](){ return ShouldShowDebugInfo(); },
	[this, DebugData]()
	{
		(*DebugData).FindOrAdd("Owner Name") = GetOwner()->GetFName().ToString();
		(*DebugData).FindOrAdd("Component Enabled") = bIsComponentEnabled ? "True" : "False";
		(*DebugData).FindOrAdd("Modifier Enabled") = bIsEnabled ? "True" : "False";
		(*DebugData).FindOrAdd("Blend Weight") = FString::Printf(TEXT("%.5f"), GetCurrentBlendWeight());

		const APlayerController* BlendTarget = ExternalVolumeBlendTarget.Get();
		(*DebugData).FindOrAdd("Blend Target" ) = BlendTarget ? BlendTarget->GetName() : "None";

		return DebugData;
	});
#endif
	
	PrimaryComponentTick.bCanEverTick = false;
	
	EasingFunction = CreateDefaultSubobject<UDaySequenceModifierEasingFunction>("EasingFunction", true);
}

#if WITH_EDITOR

void UDaySequenceModifierComponent::SetVolumePreviewLocation(const FVector& Location)
{
	UE::DaySequence::GVolumePreviewLocation = Location;
}

void UDaySequenceModifierComponent::SetIsSimulating(bool bInIsSimulating)
{
	UE::DaySequence::bIsSimulating = bInIsSimulating;
}

void UDaySequenceModifierComponent::UpdateEditorPreview(float DeltaTime)
{
	using namespace UE::DaySequence;

	if (bIsComponentEnabled && bPreview && bUseVolume && IsRegistered() && !GetWorld()->IsGameWorld())
	{
		if (const float DistanceBlendFactor = GetDistanceBlendFactor(GVolumePreviewLocation); DistanceBlendFactor > UE_SMALL_NUMBER)
		{
			if (!bIsEnabled)
			{
				EnableModifier();
			}
			
			const float BlendFactor = FMath::Min(DistanceBlendFactor, CustomVolumeBlendWeight);
			if (BlendMode != EDaySequenceModifierBlendMode::None && CachedBlendFactor != BlendFactor)
			{
				// If we're using a blend we have to mark active sections as changed
				// in order to force an update in-editor:
				
				if (UMovieSceneSubSection* SubSection = WeakSubSection.Get())
				{
					SubSection->MarkAsChanged();
				}

				for (TWeakObjectPtr<UMovieSceneSubSection> SubSection : SubSections)
				{
					UMovieSceneSubSection* StrongSubSection = SubSection.Get();
					if (StrongSubSection && StrongSubSection->IsActive())
					{
						StrongSubSection->MarkAsChanged();
						break;
					}
				}
			}
			CachedBlendFactor = BlendFactor;
		}
		else
		{
			DisableModifier();
		}
	}
}

TStatId UDaySequenceModifierComponent::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDaySequenceModifierComponent, STATGROUP_Tickables);
}

ETickableTickType UDaySequenceModifierComponent::GetTickableTickType() const
{
	UWorld* World = GetWorld();
	if (World && World->WorldType == EWorldType::Editor)
	{
		return ETickableTickType::Always;
	}
	return ETickableTickType::Never;
}

void UDaySequenceModifierComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDaySequenceModifierComponent, bPreview))
	{
		if (bPreview && !bIsEnabled)
		{
			EnableModifier();
		}
		else if (!bPreview && bIsEnabled)
		{
			DisableModifier();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDaySequenceModifierComponent, bUseVolume))
	{
		if (!bUseVolume)
		{
			if (bPreview && !bIsEnabled)
			{
				EnableModifier();
			}
			else if (!bPreview && bIsEnabled)
			{
				DisableModifier();
			}
		}
	}
}

#endif // WITH_EDITOR

void UDaySequenceModifierComponent::OnRegister()
{
	Super::OnRegister();

	bCachedExternalShapesInvalid = true;
	
	if (!bUseVolume)
	{
		EnableModifier();
	}
}

void UDaySequenceModifierComponent::OnUnregister()
{
	Super::OnUnregister();

	bCachedExternalShapesInvalid = true;
	
	DisableModifier();
	RemoveSubSequenceTrack();
}

void UDaySequenceModifierComponent::DaySequenceUpdate()
{
	CSV_SCOPED_TIMING_STAT(DaySequence, SequencePlayerUpdated);
	
	// Force expensive update
	const float DistanceBlendFactor = UpdateBlendWeight();

	if (bIsComponentEnabled && bUseVolume)
	{
		if (DistanceBlendFactor > UE_SMALL_NUMBER)
		{
			EnableModifier();
		}
		else
		{
			DisableModifier();
		}
	}
}

void UDaySequenceModifierComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDaySequenceModifierComponent::EndPlay(EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	
	RemoveSubSequenceTrack();
}

void UDaySequenceModifierComponent::ResetOverrides()
{
	if (ProceduralDaySequence)
	{
		UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();
		TArray<FMovieSceneBinding> Bindings = MovieScene->GetBindings();

		for (const FMovieSceneBinding& Binding : Bindings)
		{
			FGuid BindingID = Binding.GetObjectGuid();

			ProceduralDaySequence->UnbindPossessableObjects(BindingID);
			MovieScene->RemovePossessable(BindingID);
		}
	}

	if (TargetActor && bUnpauseOnDisable)
	{
		TargetActor->Play();
		bUnpauseOnDisable = false;
	}
}

void UDaySequenceModifierComponent::BindToDaySequenceActor(ADaySequenceActor* DaySequenceActor)
{
	if (TargetActor == DaySequenceActor)
	{
		return;
	}

	bool bWasEnabled = bIsEnabled;
	UnbindFromDaySequenceActor();

	TargetActor = DaySequenceActor;

	if (bWasEnabled)
	{
		EnableModifier();
	}

	if (ensureMsgf(DaySequenceActor, TEXT("BindToDaySequenceActor called with a null Day Sequence Actor.")))
	{
		DaySequenceActor->GetOnPostInitializeDaySequences().AddUObject(this, &UDaySequenceModifierComponent::ReinitializeSubSequence);
		DaySequenceActor->GetOnDaySequenceUpdate().AddUObject(this, &UDaySequenceModifierComponent::DaySequenceUpdate);
#if ENABLE_DRAW_DEBUG
		if (!DaySequenceActor->IsDebugCategoryRegistered(ShowDebug_ModifierCategory))
		{
			DaySequenceActor->RegisterDebugCategory(ShowDebug_ModifierCategory, TargetActor->OnShowDebugInfoDrawFunction);
		}
		
		DaySequenceActor->GetOnDebugLevelChanged().AddUObject(this, &UDaySequenceModifierComponent::OnDebugLevelChanged);
		DaySequenceActor->RegisterDebugEntry(DebugEntry, ShowDebug_ModifierCategory);
#endif
	}
}

void UDaySequenceModifierComponent::UnbindFromDaySequenceActor()
{
	DisableModifier();
	RemoveSubSequenceTrack();

	if (TargetActor)
	{
		TargetActor->GetOnPostInitializeDaySequences().RemoveAll(this);
		TargetActor->GetOnDaySequenceUpdate().RemoveAll(this);
#if ENABLE_DRAW_DEBUG
		TargetActor->GetOnDebugLevelChanged().RemoveAll(this);
		TargetActor->UnregisterDebugEntry(DebugEntry, ShowDebug_ModifierCategory);
#endif
		TargetActor = nullptr;
	}
}

void UDaySequenceModifierComponent::RemoveSubSequenceTrack()
{
	auto RemoveSubTrack = [](const UMovieSceneSubSection* SubSection)
	{
		if (SubSection)
		{
			UMovieSceneTrack* Track = SubSection->GetTypedOuter<UMovieSceneTrack>();
			UMovieScene* MovieScene = Track->GetTypedOuter<UMovieScene>();

			check(Track && MovieScene);

			MovieScene->RemoveTrack(*Track);
			MovieScene->MarkAsChanged();
		}
	};
	
	RemoveSubTrack(WeakSubSection.Get());
	WeakSubSection = nullptr;

	for (const TWeakObjectPtr<UMovieSceneSubSection> SubSection : SubSections)
	{
		RemoveSubTrack(SubSection.Get());
	}
	SubSections.Empty();

#if ENABLE_DRAW_DEBUG
	if (TargetActor)
	{
		for (TSharedPtr<UE::DaySequence::FDaySequenceDebugEntry> Entry : SubSectionDebugEntries)
		{
			TargetActor->UnregisterDebugEntry(Entry, TargetActor->ShowDebug_SubSequenceCategory);
		}
	}
	SubSectionDebugEntries.Empty();
#endif
}

bool UDaySequenceModifierComponent::CanBeEnabled() const
{
	AActor* Actor = TargetActor ? TargetActor : GetOwner();

	if (!bIsComponentEnabled)
	{
		return false;
	}
	
	if (bUseVolume)
	{
		ENetMode NetMode = Actor->GetNetMode();
		return NetMode != NM_DedicatedServer;
	}

	return true;
}

void UDaySequenceModifierComponent::EnableComponent()
{
	if (bIsComponentEnabled)
	{
		return;
	}
	
	bIsComponentEnabled = true;
}

void UDaySequenceModifierComponent::DisableComponent()
{
	if (!bIsComponentEnabled && !bIsEnabled)
	{
		return;
	}
	
	bIsComponentEnabled = false;

	DisableModifier();
	RemoveSubSequenceTrack();
}

void UDaySequenceModifierComponent::EnableModifier()
{
	if (bIsEnabled || !CanBeEnabled())
	{
		return;
	}

	if (!bPreview && GetWorld()->WorldType == EWorldType::Editor)
	{
		return;
	}

	bIsEnabled = true;

	// Will call SetSubTrackMuteState for all living subsections, which checks enable state of modifier and their conditions
	InvalidateMuteStates();

	// In both collection and non collection case this array is populated, so if size is 0 we never initialized or removed subsections
	if (SubSections.Num() == 0)
	{
		ReinitializeSubSequence(nullptr);
	}

	SetInitialTimeOfDay();

	// Force an update if it's not playing so that the effects of this being enabled are seen
	if (TargetActor && !TargetActor->IsPlaying())
	{
		TargetActor->SetTimeOfDay(TargetActor->GetTimeOfDay());
	}

	OnPostEnableModifier.Broadcast();
}

void UDaySequenceModifierComponent::DisableModifier()
{
	if (!bIsEnabled)
	{
		return;
	}

	if (!bPreview && GetWorld()->WorldType == EWorldType::Editor)
	{
		return;
	}
	
	bIsEnabled = false;

	if (TargetActor && !TargetActor->HasAnyFlags(RF_BeginDestroyed))
	{
		// Will call SetSubTrackMuteState for all living subsections, which checks enable state of modifier and their conditions
		InvalidateMuteStates();

		TargetActor->RemoveStaticTimeOfDay();
		
		if (bUnpauseOnDisable)
		{
			TargetActor->Play();
			bUnpauseOnDisable = false;
		}
		// Force an update if it's not playing so that the effects of this being disabled are seen
		else if (!TargetActor->IsPlaying())
		{
			TargetActor->SetTimeOfDay(TargetActor->GetTimeOfDay());
		}
	}

	// Necessary for correctly marking TargetActor as changed on enable.
	CachedBlendFactor = -1.0f;
}

void UDaySequenceModifierComponent::SetInitialTimeOfDay()
{
	if (TargetActor)
	{
		const bool bHasAuthority = TargetActor->HasAuthority();
		const bool bRandomTimeOfDay = DayNightCycle == EDayNightCycleMode::RandomFixedTime || DayNightCycle == EDayNightCycleMode::RandomStartTime;
		const float Time = bRandomTimeOfDay ? FMath::FRand()*TargetActor->GetDayLength() : DayNightCycleTime;

		if (!bHasAuthority && !bUseVolume)
		{
			// Never set initial time of day from non-volume based modifiers if they don't have authority
			// We'll just get the initial time of day from the server replication
			return;
		}

		switch (DayNightCycle)
		{
		case EDayNightCycleMode::FixedTime:
		case EDayNightCycleMode::RandomFixedTime:
			if (!bHasAuthority && bUseVolume)
			{
				// This function assigns a custom time controller so we can override
				// the time regardless of server replication
				TargetActor->SetStaticTimeOfDay(Time);
				break;
			}

			// If we're not overriding the time on a client, we need to make sure the time is replicated correctly
			// AddStaticTimeOfDayOverride should be used where a static time of day needs to be evaluated from the sequence itself (to support h-bias overriding)
			bUnpauseOnDisable = TargetActor->IsPlaying();
			TargetActor->Pause();

			// Intentional fallthrough - set the time and preview time

		case EDayNightCycleMode::StartAtSpecifiedTime:
		case EDayNightCycleMode::RandomStartTime:

			TargetActor->SetTimeOfDay(Time);
#if WITH_EDITOR
			TargetActor->ConditionalSetTimeOfDayPreview(Time);
#endif
			break;

#if WITH_EDITOR
		default:
			TargetActor->SetTimeOfDayPreview(TargetActor->GetTimeOfDayPreview());
#endif
		}
	}
}

void UDaySequenceModifierComponent::ReinitializeSubSequence(ADaySequenceActor::FSubSectionPreserveMap* SectionsToPreserve)
{
	CSV_SCOPED_TIMING_STAT(DaySequence, ReinitializeSubSequence);
	
#if ROOT_SEQUENCE_RECONSTRUCTION_ENABLED
	bool bReinit = true;

	if (SectionsToPreserve)
	{
		// Mark all subsections we have recorded for keep in the root sequence
		// This is a fast path we take only if all of our subsections are in the root sequence
		for (TWeakObjectPtr<UMovieSceneSubSection> SubSection : SubSections)
		{
			if (const UMovieSceneSubSection* StrongSubSection = SubSection.Get())
			{
				if (bool* SectionToPreserveFlag = SectionsToPreserve->Find(StrongSubSection))
				{
					*SectionToPreserveFlag = true;
					bReinit = false;
				}
				else
				{
					// If we have a subsection that is not in the root sequence, break and reinit completely
					bReinit = true;
					break;
				}
			}
		}

		if (bReinit)
		{
			// Mark all sections associated with this modifier for delete before we do a full reinit
			for (TWeakObjectPtr<UMovieSceneSubSection> SubSection : SubSections)
			{
				if (const UMovieSceneSubSection* StrongSubSection = SubSection.Get())
				{
					if (bool* SectionToPreserveFlag = SectionsToPreserve->Find(StrongSubSection))
					{
						*SectionToPreserveFlag = false;
					}
				}
			}
		}
	}

	if (bReinit)
	{
#endif
		RemoveSubSequenceTrack();
		
		if (bUseCollection)
		{
			if (DaySequenceCollection)
			{
				for (const FDaySequenceCollectionEntry& Entry : DaySequenceCollection->DaySequences)
				{
					InitializeDaySequence(Entry);
				}
			}
		}
		else
		{
			// Always create the sub section even if it is nullptr. This means that
			// if the procedural day sequence is created later, we can still add it to the sub section
			UDaySequence* SequenceToUse = UserDaySequence ? UserDaySequence : ProceduralDaySequence;
	
			WeakSubSection = InitializeDaySequence(SequenceToUse);
		}
#if ROOT_SEQUENCE_RECONSTRUCTION_ENABLED
	}
	else
	{
		// If we took the fast path, invalidate all mute states.
		InvalidateMuteStates();
	}
#endif
	
#if ENABLE_DRAW_DEBUG
	if (TargetActor)
	{
		if (!TargetActor->IsDebugCategoryRegistered(TargetActor->ShowDebug_SubSequenceCategory))
		{
			TargetActor->RegisterDebugCategory(TargetActor->ShowDebug_SubSequenceCategory,  TargetActor->OnShowDebugInfoDrawFunction);
		}
	
		for (TSharedPtr<UE::DaySequence::FDaySequenceDebugEntry> Entry : SubSectionDebugEntries)
		{
			TargetActor->RegisterDebugEntry(Entry, TargetActor->ShowDebug_SubSequenceCategory);
		}
	}
#endif
	
	OnPostReinitializeSubSequences.Broadcast();
}

UMovieSceneSubSection* UDaySequenceModifierComponent::InitializeDaySequence(const FDaySequenceCollectionEntry& Entry)
{
	UDaySequence* RootSequence = TargetActor ? TargetActor->GetRootSequence() : nullptr;
    UMovieScene*  MovieScene   = RootSequence ? RootSequence->GetMovieScene() : nullptr;

    if (!MovieScene)
    {
    	return nullptr;
    }
	
	auto CreateSubTrack = [this, MovieScene](UDaySequence* Sequence, int BiasOffset, bool bActivate, bool bBlendHierarchicalBias)
	{
		UDaySequenceTrack*     RootTrack  = MovieScene->AddTrack<UDaySequenceTrack>();
		RootTrack->ClearFlags(RF_Transactional);
		RootTrack->SetFlags(RF_Transient);
        
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(RootTrack->CreateNewSection());
		SubSection->ClearFlags(RF_Transactional);
		// SubSections of DaySequenceTrack will inherit flags from its parent track - RF_Transient in this case.
		SubSection->Parameters.HierarchicalBias = Bias + BiasOffset;
		SubSection->Parameters.Flags            = EMovieSceneSubSectionFlags::OverrideRestoreState
												| (bIgnoreBias ? EMovieSceneSubSectionFlags::IgnoreHierarchicalBias : EMovieSceneSubSectionFlags::None)
												| (bBlendHierarchicalBias ? EMovieSceneSubSectionFlags::BlendHierarchicalBias : EMovieSceneSubSectionFlags::None);

		SubSection->SetSequence(Sequence);
		SubSection->SetRange(MovieScene->GetPlaybackRange());
		SubSection->SetIsActive(bActivate);
		SubSection->SetIsLocked(true);
		
		TargetActor->UpdateSubSectionTimeScale(SubSection);

		RootTrack->AddSection(*SubSection);

		if (bUseVolume && BlendMode != EDaySequenceModifierBlendMode::None && bBlendHierarchicalBias)
		{
			// In the Sequencer Editor, EaseIn pads the Sequence asset name by the EaseIn duration
			// (see SSequencerSection::OnPaint). Since we set the Easing duration to the full section
			// width to facilitate blending, the label is clipped. So we use EaseOut here instead and
			// ensure that the weight is inverted in Evaluate().
			SubSection->Easing.bManualEaseOut = true;
			SubSection->Easing.ManualEaseOutDuration = MovieScene->GetPlaybackRange().Size<FFrameNumber>().Value;

			EasingFunction->Initialize(UDaySequenceModifierEasingFunction::EEasingFunctionType::EaseOut);
			SubSection->Easing.EaseOut = EasingFunction;
		}

#if WITH_EDITOR
		FString Label = GetOwner()->GetActorLabel();
#else
		FString Label = GetOwner()->GetName();
#endif
#if WITH_EDITORONLY_DATA
		RootTrack->DisplayName = FText::Format(LOCTEXT("ModifierTrackFormat", "Modifier ({0})"), FText::FromString(Label));
#endif
        
		SubSection->MarkAsChanged();
		return SubSection;
	};
	
	constexpr bool bActivate = true;
	constexpr bool bBlendHierarchicalBias = true;
	UMovieSceneSubSection* SubSection = CreateSubTrack(Entry.Sequence, Entry.BiasOffset, bActivate, bBlendHierarchicalBias);
	
	if (!SubSections.Contains(SubSection))
	{
		SubSections.Add(SubSection);
	}

	const TFunction<void(void)> SetSubTrackMuteStateConditional = [this, SubSection, Conditions = Entry.Conditions.Conditions]()
	{
		if (!IsValid(this) || !IsValid(SubSection))
		{
			return;
		}

		constexpr bool bInitialMuteState = false;
		const bool bActive = bIsEnabled && !TargetActor->EvaluateSequenceConditions(bInitialMuteState, Conditions);
		if (SubSection->IsActive() != bActive)
		{
			SubSection->MarkAsChanged();
			SubSection->SetIsActive(bActive);
		}
	};

	const TFunction<void(void)> SetSubTrackMuteStateUnconditional = [this, SubSection]()
	{
		if (!IsValid(this) || !IsValid(SubSection))
		{
			return;
		}

		const bool bActive = bIsEnabled;
		if (SubSection->IsActive() != bActive)
		{
			SubSection->MarkAsChanged();
			SubSection->SetIsActive(bActive);
		}
	};

	const TFunction<void(void)>& SetSubTrackMuteState = Entry.Conditions.Conditions.Num() == 0 ? SetSubTrackMuteStateUnconditional : SetSubTrackMuteStateConditional;
	
	// Initialize mute state and set up the condition callbacks to dynamically update mute state.
	SetSubTrackMuteState();
	OnInvalidateMuteStates.AddWeakLambda(SubSection, SetSubTrackMuteState);
	TargetActor->BindToConditionCallbacks(this, Entry.Conditions.Conditions, [this]() { InvalidateMuteStates(); });
	
#if ENABLE_DRAW_DEBUG
	// This gets captured by a lambda below so should continue living
	TSharedPtr<TMap<FString, FString>> DebugData = MakeShared<TMap<FString, FString>>();
	SubSectionDebugEntries.Emplace(MakeShared<UE::DaySequence::FDaySequenceDebugEntry>(
	[this](){ return true; },
	[this, DebugData, SubSection]()
	{
		if (IsValid(SubSection))
		{
			(*DebugData).FindOrAdd("Owner Name") = GetOwner()->GetFName().ToString();
			(*DebugData).FindOrAdd("Sequence Name") = SubSection->GetSequence() ? SubSection->GetSequence()->GetFName().ToString() : "None";
			(*DebugData).FindOrAdd("Mute State") = SubSection->IsActive() ? "Active" : "Muted";
			(*DebugData).FindOrAdd("Hierarchical Bias") = FString::Printf(TEXT("%d"), SubSection->Parameters.HierarchicalBias);
		}
	
		return DebugData;
	}));
#endif

	return SubSection;
}

FGuid UDaySequenceModifierComponent::GetOrCreateProceduralBinding(UObject* Object)
{
	if (!Object)
	{
		FFrame::KismetExecutionMessage(TEXT("Null Object parameter specified."), ELogVerbosity::Error);
		return FGuid();
	}

	USceneComponent* Component = Cast<USceneComponent>(Object);
	AActor*          Actor     = Cast<AActor>(Object);

	// Set up the time of day actor binding if we don't have one already
	if (!TargetActor)
	{
		if (Actor)
		{
			TargetActor = Cast<ADaySequenceActor>(Actor);
		}
		else if (Component)
		{
			TargetActor = Cast<ADaySequenceActor>(Component->GetOwner());
		}
	}

	if (!TargetActor)
	{
		FFrame::KismetExecutionMessage(TEXT("No valid ADaySequenceActor has been set up. Have you called BindToDaySequenceActor yet?"), ELogVerbosity::Error);
		return FGuid();
	}

	if (Component && !Component->IsIn(TargetActor))
	{
		FFrame::KismetExecutionMessage(TEXT("Unable to bind to components that exist outside of the ADaySequenceActor we are tracking."), ELogVerbosity::Error);
		return FGuid();
	}

	if (Actor && Actor != TargetActor)
	{
		FFrame::KismetExecutionMessage(TEXT("Unable to bind to actors that are not the ADaySequenceActor we are tracking."), ELogVerbosity::Error);
		return FGuid();
	}

	if (!ProceduralDaySequence)
	{
		// Name the procedural sequence the same as this component's owner so it shows up in the Sequence
		// with a meaningful name
#if WITH_EDITOR
		FName SequenceName = MakeUniqueObjectName(this, UDaySequence::StaticClass(), *GetOwner()->GetActorLabel());
#else
		FName SequenceName = MakeUniqueObjectName(this, UDaySequence::StaticClass(), GetOwner()->GetFName());
#endif

		ProceduralDaySequence = NewObject<UDaySequence>(this, SequenceName);
		ProceduralDaySequence->Initialize(RF_Transient);
	}

	// If we have a sub section but it has no sequence applied, apply it now.
	// This implies EnableModifier was called before we had any valid sequence data
	UMovieSceneSubSection* SubSection = WeakSubSection.Get();
	if (SubSection && SubSection->GetSequence() == nullptr)
	{
		SubSection->MarkAsChanged();
		SubSection->SetSequence(ProceduralDaySequence);
	}

	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState = MovieSceneHelpers::CreateTransientSharedPlaybackState(TargetActor, ProceduralDaySequence);

	// Find the main binding
	FGuid RootGuid = ProceduralDaySequence->FindBindingFromObject(TargetActor, SharedPlaybackState);
	if (!RootGuid.IsValid())
	{
		FString RootName = TargetActor->GetName();
		FMovieScenePossessable Possessable(RootName, TargetActor->GetClass());
		FMovieSceneBinding     Binding(Possessable.GetGuid(), RootName);

		RootGuid = Possessable.GetGuid();

		// Explicitly invoke MarkAsChanged to ensure proper notification at runtime.
		// The Modify that AddPossessable invokes only works in editor.
		MovieScene->MarkAsChanged();
		MovieScene->AddPossessable(Possessable, Binding);
		ProceduralDaySequence->BindPossessableObject(RootGuid, *TargetActor, TargetActor);
	}

	// If we're trying to animate the actor, just return the root binding
	if (Actor)
	{
		return RootGuid;
	}

	// If we're trying to animate a component within the actor, retrieve or create a child binding for that
	FGuid ComponentGuid = ProceduralDaySequence->FindBindingFromObject(Component, SharedPlaybackState);
	if (!ComponentGuid.IsValid() && Component)
	{
		FString Name = Component->GetName();
		FMovieScenePossessable Possessable(Name, Component->GetClass());
		FMovieSceneBinding     Binding(Possessable.GetGuid(), Name);

		Possessable.SetParent(RootGuid, MovieScene);
		ComponentGuid = Possessable.GetGuid();

		// Explicitly invoke MarkAsChanged to ensure proper notification at runtime.
		// The Modify that AddPossessable invokes only works in editor.
		MovieScene->MarkAsChanged();
		MovieScene->AddPossessable(Possessable, Binding);
		ProceduralDaySequence->BindPossessableObject(ComponentGuid, *Component, TargetActor);
	}

	return ComponentGuid;
}

void UDaySequenceModifierComponent::AddScalarOverride(UObject* Object, FName PropertyName, double Value)
{
	using namespace UE::DaySequence;
	using namespace	UE::MovieScene;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*Object);
	if (!TestValidProperty(Object, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FFloatProperty>())
	{
		UMovieSceneFloatSection* FloatSection = CreateOrAddPropertyOverrideSection<UMovieSceneFloatTrack, UMovieSceneFloatSection>(MovieScene, ObjectGuid, PropertyName);
		FloatSection->GetChannel().SetDefault(static_cast<float>(Value));
	}
	else if (Property->IsA<FDoubleProperty>())
	{
		UMovieSceneDoubleSection* DoubleSection = CreateOrAddPropertyOverrideSection<UMovieSceneDoubleTrack, UMovieSceneDoubleSection>(MovieScene, ObjectGuid, PropertyName);
		DoubleSection->GetChannel().SetDefault(Value);
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Unable to animate a %s property as a scalar."), *Property->GetClass()->GetName()), ELogVerbosity::Error);
	}
}

void UDaySequenceModifierComponent::AddColorOverride(UObject* Object, FName PropertyName, FLinearColor Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*Object);
	if (!TestValidProperty(Object, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FStructProperty>() && 
		(CastField<FStructProperty>(Property)->Struct == TBaseStructure<FLinearColor>::Get() || CastField<FStructProperty>(Property)->Struct == TBaseStructure<FColor>::Get()) )
	{
		UMovieSceneColorSection* ColorSection = CreateOrAddPropertyOverrideSection<UMovieSceneColorTrack, UMovieSceneColorSection>(MovieScene, ObjectGuid, PropertyName);

		ColorSection->GetRedChannel().SetDefault(Value.R);
		ColorSection->GetGreenChannel().SetDefault(Value.G);
		ColorSection->GetBlueChannel().SetDefault(Value.B);
		ColorSection->GetAlphaChannel().SetDefault(Value.A);
	}
}

void UDaySequenceModifierComponent::AddMaterialOverride(UObject* Object, int32 MaterialIndex, UMaterialInterface* Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	UMovieScenePrimitiveMaterialTrack* MaterialTrack = CreateOrAddOverrideTrack<UMovieScenePrimitiveMaterialTrack>(MovieScene, ObjectGuid);
	MaterialTrack->SetMaterialInfo(FComponentMaterialInfo{ FName(), MaterialIndex, EComponentMaterialType::IndexedMaterial });

	UMovieScenePrimitiveMaterialSection* Section = Cast<UMovieScenePrimitiveMaterialSection>(MaterialTrack->GetAllSections()[0]);
	Section->MaterialChannel.SetDefault(Value);
}

void UDaySequenceModifierComponent::AddScalarMaterialParameterOverride(UObject* Object, int32 MaterialIndex, FName ParameterName, float Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track or locate an existing one
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	// Material parameter tracks use the material index as the unique name
	FName IndexAsName(*FString::FromInt(MaterialIndex));
	UMovieSceneComponentMaterialTrack* MaterialTrack = CreateOrAddOverrideTrack<UMovieSceneComponentMaterialTrack>(MovieScene, ObjectGuid, IndexAsName);

	MaterialTrack->SetMaterialInfo(FComponentMaterialInfo{FName(), MaterialIndex, EComponentMaterialType::IndexedMaterial });
	MaterialTrack->AddScalarParameterKey(ParameterName, 0, Value);
}

void UDaySequenceModifierComponent::AddColorMaterialParameterOverride(UObject* Object, int32 MaterialIndex, FName ParameterName, FLinearColor Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track or locate an existing one
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	// Material parameter tracks use the material index as the unique name
	FName IndexAsName(*FString::FromInt(MaterialIndex));
	UMovieSceneComponentMaterialTrack* MaterialTrack = CreateOrAddOverrideTrack<UMovieSceneComponentMaterialTrack>(MovieScene, ObjectGuid, IndexAsName);

	MaterialTrack->SetMaterialInfo(FComponentMaterialInfo{ FName(), MaterialIndex, EComponentMaterialType::IndexedMaterial });
	MaterialTrack->AddColorParameterKey(ParameterName, 0, Value);
}

void UDaySequenceModifierComponent::AddVectorOverride(UObject* Object, FName PropertyName, FVector Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*Object);
	if (!TestValidProperty(Object, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FStructProperty>() && CastField<FStructProperty>(Property)->Struct == TBaseStructure<FVector>::Get())
	{
		UMovieSceneDoubleVectorSection* VectorSection = CreateOrAddPropertyOverrideSection<UMovieSceneDoubleVectorTrack, UMovieSceneDoubleVectorSection>(MovieScene, ObjectGuid, PropertyName);

		VectorSection->SetChannelsUsed(3);

		VectorSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0)->SetDefault(Value.X);
		VectorSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(1)->SetDefault(Value.Y);
		VectorSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(2)->SetDefault(Value.Z);
	}
}

void UDaySequenceModifierComponent::AddTransformOverride(UObject* Object, FTransform Value)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	UMovieScene3DTransformSection* TransformSection = CreateOrAddPropertyOverrideSection<UMovieScene3DTransformTrack, UMovieScene3DTransformSection>(MovieScene, ObjectGuid, "Transform");

	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(0)->SetDefault(Value.GetLocation().X);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(1)->SetDefault(Value.GetLocation().Y);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(2)->SetDefault(Value.GetLocation().Z);

	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(3)->SetDefault(Value.Rotator().Roll);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(4)->SetDefault(Value.Rotator().Pitch);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(5)->SetDefault(Value.Rotator().Yaw);

	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(6)->SetDefault(Value.GetScale3D().X);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(7)->SetDefault(Value.GetScale3D().Y);
	TransformSection->GetChannelProxy().GetChannel<FMovieSceneDoubleChannel>(8)->SetDefault(Value.GetScale3D().Z);
}

void UDaySequenceModifierComponent::AddStaticTimeOfDayOverride(ADaySequenceActor* Actor, float Hours)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Actor);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	UMovieSceneFloatSection* Section = CreateOrAddPropertyOverrideSection<UMovieSceneFloatTrack, UMovieSceneFloatSection>(MovieScene, ObjectGuid, "StaticTimeOfDay");
	Section->GetChannel().SetDefault(Hours);
}

void UDaySequenceModifierComponent::AddBoolOverride(UObject* Object, FName PropertyName, bool bValue)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*Object);
	if (!TestValidProperty(Object, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FBoolProperty>())
	{
		UMovieSceneBoolSection* Section = CreateOrAddPropertyOverrideSection<UMovieSceneBoolTrack, UMovieSceneBoolSection>(MovieScene, ObjectGuid, PropertyName);
		Section->GetChannel().SetDefault(bValue);
	}
}

void UDaySequenceModifierComponent::AddVisibilityOverride(UObject* Object, bool bValue)
{
	using namespace UE::DaySequence;

	FGuid ObjectGuid = GetOrCreateProceduralBinding(Object);
	if (!ObjectGuid.IsValid())
	{
		return;
	}

	check(ProceduralDaySequence);

	// Create the new track
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	static const FName ActorVisibilityTrackName = TEXT("bHidden");
	static const FName ComponentVisibilityTrackName = TEXT("bHiddenInGame");

	const bool bIsComponent = Object->IsA<USceneComponent>();
	const bool bIsActor     = Object->IsA<AActor>();

	if (bIsComponent)
	{
		UMovieSceneBoolSection* VisibilitySection = CreateOrAddPropertyOverrideSection<UMovieSceneVisibilityTrack, UMovieSceneBoolSection>(MovieScene, ObjectGuid, "bHiddenInGame");
		VisibilitySection->GetChannel().SetDefault(bValue);
	}
	else if (bIsActor)
	{
		UMovieSceneBoolSection* VisibilitySection = CreateOrAddPropertyOverrideSection<UMovieSceneVisibilityTrack, UMovieSceneBoolSection>(MovieScene, ObjectGuid, "bHidden");
		VisibilitySection->GetChannel().SetDefault(bValue);
	}
}

void UDaySequenceModifierComponent::SetUserDaySequence(UDaySequence* InDaySequence)
{
	UserDaySequence = InDaySequence;
	ReinitializeSubSequence(nullptr);
}

bool UDaySequenceModifierComponent::GetBlendPosition(FVector& InPosition) const
{
	CSV_SCOPED_TIMING_STAT(DaySequence, GetBlendPosition);
	
#if WITH_EDITOR
	if (const UWorld* World = GetWorld(); World && (!World->IsGameWorld() || UE::DaySequence::bIsSimulating))
	{
		InPosition = UE::DaySequence::GVolumePreviewLocation;
		return true;
	}
	else
#endif
	if (const APlayerController* BlendTarget = ExternalVolumeBlendTarget.Get())
	{
		CSV_SCOPED_TIMING_STAT(DaySequence, GetPlayerViewPoint);
		InPosition = BlendTarget->PlayerCameraManager->GetCameraLocation();
		return true;
	}

	return false;
}

float UDaySequenceModifierComponent::GetDistanceBlendFactorForShape(const UShapeComponent* Shape, const FVector& Position) const
{
	const float Distance = UE::DaySequence::ComputeSignedDistance(Shape, Position);
	return Distance < 0.f ? FMath::Clamp(-Distance / BlendAmount, 0.f, 1.f) : 0.f;
}

float UDaySequenceModifierComponent::GetDistanceBlendFactor(const FVector& Position) const
{
	CSV_SCOPED_TIMING_STAT(DaySequence, GetDistanceBlendFactor);
	
	CachedDistanceBlendFactor = 0.f;

	for (const UShapeComponent* Shape : GetVolumeShapeComponents())
	{
		CachedDistanceBlendFactor = FMath::Max(CachedDistanceBlendFactor, GetDistanceBlendFactorForShape(Shape, Position));
	}

	return CachedDistanceBlendFactor;
}

TArray<UShapeComponent*> UDaySequenceModifierComponent::GetVolumeShapeComponents() const
{
	TArray<UShapeComponent*> ResolvedVolumeShapeComponents;
	ResolvedVolumeShapeComponents.Reserve(VolumeShapeComponents.Num());
	
	if (bCachedExternalShapesInvalid)
	{
		UpdateCachedExternalShapes();
	}
	
#if WITH_EDITOR
	// We don't expect changes to VolumeShapeComponents or CachedExternalShapes during play.
	// This will ensure that CachedExternalShapes remains updated to reflect editor workflows
	// that might invalidate entries, such as deleting a shape component on an external actor.
	bool bRecache = false;
#endif

	// This loop serves two purposes:
	// 1) Go ahead and move from weak object ptr to raw pointer so the caller doesn't have to.
	// 2) Determine if CachedExternalShapes is invalid so we can recache (occurs when reference shape component is deleted).
	for (TWeakObjectPtr<UShapeComponent> Shape : CachedExternalShapes)
	{
		if (UShapeComponent* ValidShape = Shape.Get())
		{
			ResolvedVolumeShapeComponents.Add(ValidShape);
		}
#if WITH_EDITOR
		else
		{
			// Break out out here as we will update the cached shapes and reconstruct ResolvedVolumeShapeComponents below.
			bRecache = true;
			break;
		}
#endif
	}

#if WITH_EDITOR
	// We do this here so that we don't modify CachedExternalShapes while iterating over it.
	// The idea is if we recache immediately before the recursive call, we should not be able to recursively hit this branch.
	if (bRecache)
	{
		checkNoRecursion();
		bCachedExternalShapesInvalid = true;
		UpdateCachedExternalShapes();
		return GetVolumeShapeComponents();
	}
#endif
	
	return ResolvedVolumeShapeComponents;
}

float UDaySequenceModifierComponent::GetCurrentBlendWeight() const
{
	return FMath::Min(CachedDistanceBlendFactor, CustomVolumeBlendWeight);
}

float UDaySequenceModifierComponent::UpdateBlendWeight() const
{
	FVector BlendPosition;
	const bool bHasBlendPosition = GetBlendPosition(BlendPosition);

	const float OldBlendWeight = CachedDistanceBlendFactor;
	const float NewBlendWeight = FMath::Min(bHasBlendPosition ? GetDistanceBlendFactor(BlendPosition) : 1.f, CustomVolumeBlendWeight);

	// todo [nickolas.drake]: Enable blending for paused actors. Need to force set time of day if:
	// 1) We have a blend position
	// 2) The target DSA is valid and not playing
	// 3) Our old blend weight is sufficiently different from our new blend weight
	
	return NewBlendWeight;
}

void UDaySequenceModifierComponent::SetVolumeCollisionEnabled(const ECollisionEnabled::Type InCollisionType) const
{
	for (UShapeComponent* Shape : GetVolumeShapeComponents())
	{
		Shape->SetCollisionEnabled(InCollisionType);
	}
}

void UDaySequenceModifierComponent::EmptyVolumeShapeComponents()
{
	VolumeShapeComponents.Empty();
	bCachedExternalShapesInvalid = true;
}

void UDaySequenceModifierComponent::AddVolumeShapeComponent(const FComponentReference& InShapeReference)
{
	VolumeShapeComponents.Add(InShapeReference);
	bCachedExternalShapesInvalid = true;
}

void UDaySequenceModifierComponent::InvalidateMuteStates() const
{
	OnInvalidateMuteStates.Broadcast();
}

void UDaySequenceModifierComponent::EnableDistanceVolumeBlends(APlayerController* InActor)
{	
	ExternalVolumeBlendTarget = InActor;
}

void UDaySequenceModifierComponent::SetUseVolume(bool bState)
{
	bUseVolume = bState;
}

void UDaySequenceModifierComponent::SetCustomVolumeBlendWeight(float Weight)
{
	CustomVolumeBlendWeight = FMath::Clamp(Weight, 0.f, 1.f);
}

#if ENABLE_DRAW_DEBUG
void UDaySequenceModifierComponent::OnDebugLevelChanged(int32 InDebugLevel)
{
	DebugLevel = InDebugLevel;
}

bool UDaySequenceModifierComponent::ShouldShowDebugInfo() const
{
	if (GetOwner()->HasAuthority())
	{
		return false;
	}
	
	switch (DebugLevel)
	{
	case 0: return false;
	case 1: return bIsEnabled;
	case 2: return bIsComponentEnabled;
	case 3: return true;
	default: return false;
	}
}
#endif

bool UDaySequenceModifierComponent::IsBlendTargetInAnyVolume()
{
	OccupiedVolumes = 0;

	if (FVector Position; GetBlendPosition(Position))
	{
		for (const UShapeComponent* Shape : GetVolumeShapeComponents())
		{
			if (GetDistanceBlendFactorForShape(Shape, Position) > 0.f)
			{
				OccupiedVolumes++;
			}
		}
	}

	return OccupiedVolumes > 0;
}

void UDaySequenceModifierComponent::UpdateCachedExternalShapes() const
{
	check (bCachedExternalShapesInvalid)
	
	CachedExternalShapes.Reset();
	
	for (const FComponentReference& ComponentRef : VolumeShapeComponents)
	{
		if (ComponentRef.PathToComponent.Len() != 0 || ComponentRef.ComponentProperty != NAME_None || !ComponentRef.OverrideComponent.IsExplicitlyNull())
		{
			if (UShapeComponent* ResolvedShape = Cast<UShapeComponent>(ComponentRef.GetComponent(GetOwner())); IsValid(ResolvedShape))
			{
				CachedExternalShapes.Add(ResolvedShape);
			}
		}
	}

	bCachedExternalShapesInvalid = false;
}

#undef LOCTEXT_NAMESPACE

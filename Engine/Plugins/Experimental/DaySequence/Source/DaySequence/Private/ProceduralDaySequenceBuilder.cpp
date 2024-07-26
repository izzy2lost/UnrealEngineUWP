// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProceduralDaySequenceBuilder.h"

#include "DaySequenceTime.h"
#include "MovieSceneCommonHelpers.h"

#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneDoubleSection.h"
#include "Sections/MovieSceneFloatSection.h"

#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneDoubleTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"

#include "MovieScene.h"
#include "DaySequence.h"

#define LOCTEXT_NAMESPACE "ProceduralDaySequenceBuilder"

namespace UE::DaySequence
{
	FFrameNumber GetKeyFrameNumber(float NormalizedTime, const TRange<FFrameNumber>& FrameRange)
	{
		NormalizedTime = FMath::Clamp(NormalizedTime, 0.f, 1.f);
		const unsigned StartFrameNum = FrameRange.GetLowerBoundValue().Value;
		const unsigned EndFrameNum = FrameRange.GetUpperBoundValue().Value;
		const unsigned FrameCount = EndFrameNum - StartFrameNum;
	
		if (FMath::IsNearlyEqual(NormalizedTime, 1.f))
		{
			return FFrameNumber(static_cast<int32>(EndFrameNum - 1));
		}

		return FFrameNumber(static_cast<int32>(NormalizedTime * FrameCount + StartFrameNum));
	}

	bool IsPropertyValid(UObject* Object, FProperty* Property)
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
};

UDaySequence* UProceduralDaySequenceBuilder::Initialize(ADaySequenceActor* InActor, UDaySequence* InitialSequence, bool bClearInitialSequence)
{
	if (!ensureAlways(!TargetActor) || !ensureAlways(InActor))
	{
		return nullptr;
	}

	TargetActor = InActor;
	
	if (InitialSequence)
	{
		ProceduralDaySequence = InitialSequence;

		if (bClearInitialSequence)
		{
			ClearAllKeys();
		}
	}
	else
	{
		const FName SequenceName = MakeUniqueObjectName(InActor, UDaySequence::StaticClass());
		ProceduralDaySequence = NewObject<UDaySequence>(InActor, SequenceName, RF_Transient);
		ProceduralDaySequence->Initialize(RF_Transient);

		const float DaySeconds = TargetActor->GetTimePerCycle() * FDaySequenceTime::SecondsPerHour;
		
		const int32 Duration = ProceduralDaySequence->GetMovieScene()->GetTickResolution().AsFrameNumber(DaySeconds).Value;
		ProceduralDaySequence->GetMovieScene()->SetPlaybackRange(0, Duration);
	}

	return ProceduralDaySequence;
}

bool UProceduralDaySequenceBuilder::IsInitialized() const
{
	return IsValid(TargetActor) && IsValid(ProceduralDaySequence);
}

void UProceduralDaySequenceBuilder::SetActiveBoundObject(UObject* InObject)
{
	if (!IsValid(InObject))
	{
		FFrame::KismetExecutionMessage(TEXT("SetActiveBoundObject called with an invalid object!"), ELogVerbosity::Error);
		return;
	}

	ActiveBoundObject = InObject;

	USceneComponent* Component = Cast<USceneComponent>(InObject);
	AActor*          Actor     = Cast<AActor>(InObject);

	ADaySequenceActor* AssociatedActor = nullptr;
	if (Actor)
	{
		AssociatedActor = Cast<ADaySequenceActor>(Actor);
	}
	else if (Component)
	{
		AssociatedActor = Cast<ADaySequenceActor>(Component->GetOwner());
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("SetActiveBoundObject called with an object that is neither an Actor or a Scene Component!"), ELogVerbosity::Error);
		return;
	}

	ActiveBinding = GetOrCreateProceduralBinding(InObject);
}

void UProceduralDaySequenceBuilder::ClearAllKeys()
{
	if (!ProceduralDaySequence)
	{
		return;
	}
	
	if (UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene())
	{
		for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
		{
			// Inconvenient we have to do this but at least FindBinding is doing a binary search and we do this once per binding.
			if (FMovieSceneBinding* MutableBinding = MovieScene->FindBinding(Binding.GetObjectGuid()))
			{
				// We have to copy the array here because we are mutating the internal array
				for (TArray<UMovieSceneTrack*> Tracks = MutableBinding->GetTracks(); UMovieSceneTrack* Track : Tracks)
				{
					MutableBinding->RemoveTrack(*Track, MovieScene);
				}
			}
		}

		MovieScene->MarkAsChanged();
	}
}

void UProceduralDaySequenceBuilder::AddScalarKey(FName PropertyName, float Key, double Value, ERichCurveInterpMode InterpMode)
{
	AddScalarKey(PropertyName, TPair<float, double>(Key, Value), InterpMode);
}

void UProceduralDaySequenceBuilder::AddScalarKey(FName PropertyName, const TPair<float, double>& KeyValue, ERichCurveInterpMode InterpMode)
{
	AddScalarKeys(PropertyName, TArray {KeyValue}, InterpMode);
}

void UProceduralDaySequenceBuilder::AddScalarKeys(FName PropertyName, const TArray<TPair<float, double>>& KeysAndValues, ERichCurveInterpMode InterpMode)
{
	if (!IsInitialized())
	{
		FFrame::KismetExecutionMessage(TEXT("AddScalarKey(s) called on an uninitialized Procedural Day Sequence Builder!"), ELogVerbosity::Error);
		return;
	}
	
	const UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();
	
	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*ActiveBoundObject);
	if (!UE::DaySequence::IsPropertyValid(ActiveBoundObject, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FFloatProperty>())
	{
		UMovieSceneFloatSection* FloatSection = CreateOrAddPropertyOverrideSection<UMovieSceneFloatTrack, UMovieSceneFloatSection>(PropertyName);
		
		for (const TPair<float, double>& KeyValue : KeysAndValues)
		{
			const FFrameNumber FrameNumber = UE::DaySequence::GetKeyFrameNumber(KeyValue.Key, MovieScene->GetPlaybackRange());

			switch (InterpMode)
			{
			case RCIM_Linear:
				FloatSection->GetChannel().AddLinearKey(FrameNumber, KeyValue.Value);
				break;
				
			case RCIM_Constant:
				FloatSection->GetChannel().AddConstantKey(FrameNumber, KeyValue.Value);
				break;
			
			case RCIM_Cubic:
				FloatSection->GetChannel().AddCubicKey(FrameNumber, KeyValue.Value);
				break;
				
			case RCIM_None:
				break;
			}
		}

		FloatSection->MarkAsChanged();
	}
	else if (Property->IsA<FDoubleProperty>())
	{
		UMovieSceneDoubleSection* DoubleSection = CreateOrAddPropertyOverrideSection<UMovieSceneDoubleTrack, UMovieSceneDoubleSection>(PropertyName);
		
		for (const TPair<float, double>& KeyValue : KeysAndValues)
		{
			const FFrameNumber FrameNumber = UE::DaySequence::GetKeyFrameNumber(KeyValue.Key, MovieScene->GetPlaybackRange());
			
			switch (InterpMode)
			{
			case RCIM_Linear:
				DoubleSection->GetChannel().AddLinearKey(FrameNumber, KeyValue.Value);
				break;
				
			case RCIM_Constant:
				DoubleSection->GetChannel().AddConstantKey(FrameNumber, KeyValue.Value);
				break;
			
			case RCIM_Cubic:
				DoubleSection->GetChannel().AddCubicKey(FrameNumber, KeyValue.Value);
				break;
				
			case RCIM_None:
				break;
			}
		}

		DoubleSection->MarkAsChanged();
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Unable to animate a %s property as a scalar."), *Property->GetClass()->GetName()), ELogVerbosity::Error);
	}
}

void UProceduralDaySequenceBuilder::ClearScalarKeys(FName PropertyName)
{
	if (!IsInitialized())
	{
		FFrame::KismetExecutionMessage(TEXT("ClearScalarKeys called on an uninitialized Procedural Day Sequence Builder!"), ELogVerbosity::Error);
		return;
	}
	
	const UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();
	
	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*ActiveBoundObject);
	if (!UE::DaySequence::IsPropertyValid(ActiveBoundObject, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FFloatProperty>())
	{
		UMovieSceneFloatSection* FloatSection = CreateOrAddPropertyOverrideSection<UMovieSceneFloatTrack, UMovieSceneFloatSection>(PropertyName);
		
		FloatSection->GetChannel().Reset();

		FloatSection->MarkAsChanged();
	}
	else if (Property->IsA<FDoubleProperty>())
	{
		UMovieSceneDoubleSection* DoubleSection = CreateOrAddPropertyOverrideSection<UMovieSceneDoubleTrack, UMovieSceneDoubleSection>(PropertyName);
		
		DoubleSection->GetChannel().Reset();
		
		DoubleSection->MarkAsChanged();
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Property %s is not a scalar."), *Property->GetClass()->GetName()), ELogVerbosity::Error);
	}
}

void UProceduralDaySequenceBuilder::AddBoolKey(FName PropertyName, float Key, bool Value)
{
	AddBoolKey(PropertyName, TPair<float, bool>(Key, Value));
}

void UProceduralDaySequenceBuilder::AddBoolKey(FName PropertyName, const TPair<float, bool>& KeyValue)
{
	AddBoolKeys(PropertyName, TArray {KeyValue});
}

void UProceduralDaySequenceBuilder::AddBoolKeys(FName PropertyName, const TArray<TPair<float, bool>>& KeysAndValues)
{
	if (!IsInitialized())
	{
		FFrame::KismetExecutionMessage(TEXT("AddBoolKey(s) called on an uninitialized Procedural Day Sequence Builder!"), ELogVerbosity::Error);
		return;
	}
	
	const UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();
	
	FTrackInstancePropertyBindings Bindings(PropertyName, PropertyName.ToString());
	FProperty* Property = Bindings.GetProperty(*ActiveBoundObject);
	if (!UE::DaySequence::IsPropertyValid(ActiveBoundObject, Property))
	{
		// Do nothing
	}
	else if (Property->IsA<FBoolProperty>())
	{
		UMovieSceneBoolSection* Section = CreateOrAddPropertyOverrideSection<UMovieSceneBoolTrack, UMovieSceneBoolSection>(PropertyName);
		
		for (const TPair<float, bool>& KeyValue : KeysAndValues)
		{
			const FFrameNumber FrameNumber = UE::DaySequence::GetKeyFrameNumber(KeyValue.Key, MovieScene->GetPlaybackRange());
			
			Section->GetChannel().AddKeys(TArray {FrameNumber}, TArray {KeyValue.Value});
		}

		Section->MarkAsChanged();
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("Unable to animate a %s property as a bool."), *Property->GetClass()->GetName()), ELogVerbosity::Error);
	}
}

void UProceduralDaySequenceBuilder::AddStaticTime(float StaticTime)
{
	if (!IsInitialized())
	{
		FFrame::KismetExecutionMessage(TEXT("AddStaticTime called on an uninitialized Procedural Day Sequence Builder!"), ELogVerbosity::Error);
		return;
	}
	
	UMovieSceneFloatSection* Section = CreateOrAddPropertyOverrideSection<UMovieSceneFloatTrack, UMovieSceneFloatSection>("StaticTimeOfDay");
	Section->GetChannel().SetDefault(StaticTime);
}

FGuid UProceduralDaySequenceBuilder::GetOrCreateProceduralBinding(UObject* Object) const
{
	if (!Object)
	{
		FFrame::KismetExecutionMessage(TEXT("Null Object parameter specified."), ELogVerbosity::Error);
		return FGuid();
	}

	USceneComponent* Component = Cast<USceneComponent>(Object);
	AActor*          Actor     = Cast<AActor>(Object);

	if (!TargetActor)
	{
		FFrame::KismetExecutionMessage(TEXT("No valid ADaySequenceActor set. Have you called SetActiveBoundObject yet?"), ELogVerbosity::Error);
		return FGuid();
	}

	check(ProceduralDaySequence);

	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();

	// Find the main binding
	TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState = MovieSceneHelpers::CreateTransientSharedPlaybackState(TargetActor, ProceduralDaySequence);
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

template<typename TrackType>
TrackType* UProceduralDaySequenceBuilder::CreateOrAddOverrideTrack(FName Name)
{
	UMovieScene* MovieScene = ProceduralDaySequence->GetMovieScene();
	TrackType* Track = MovieScene->FindTrack<TrackType>(ActiveBinding, Name);
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
		MovieScene->AddGivenTrack(Track, ActiveBinding);
	}

	return Track;
}

template<typename TrackType>
TrackType* UProceduralDaySequenceBuilder::CreateOrAddPropertyOverrideTrack(FName InPropertyName)
{
	TrackType* Track = CreateOrAddOverrideTrack<TrackType>(InPropertyName);
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
		FText DisplayText = FText::Format(LOCTEXT("DaySequenceActorPropertyTrackFormat", "{0} ({1})"), FText::FromName(PropertyName), FText::FromName(PropertyParent));
		Track->SetDisplayName(DisplayText);
	}
#endif
	return Track;
}

template<typename TrackType, typename SectionType>
SectionType* UProceduralDaySequenceBuilder::CreateOrAddPropertyOverrideSection(FName PropertyName)
{
	TrackType* Track = CreateOrAddPropertyOverrideTrack<TrackType>(PropertyName);
	check(Track);
	return Cast<SectionType>(Track->GetAllSections()[0]);
}

#undef LOCTEXT_NAMESPACE
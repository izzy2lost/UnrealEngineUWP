// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DaySequence.h"
#include "DaySequenceActor.h"
#include "Curves/RealCurve.h"

#include "ProceduralDaySequenceBuilder.generated.h"

/**
 * A utility class for creating procedural Day Sequences.
 * Before adding any keys, SetActiveBoundObject should be called and provided a Day Sequence Actor or a component owned by a Day Sequence Actor.
 * All time values are currently normalized to the range [0, 1], inclusive on both ends. A time of 1 is handled as a special case and maps to the final frame.
 * This class assumes the target Day Sequence Actor will stay alive and that users will keep the generated sequence alive, it manages no lifetimes.
 */
UCLASS(BlueprintType)
class DAYSEQUENCE_API UProceduralDaySequenceBuilder
	: public UObject
{
	GENERATED_BODY()
	
public:
	/**
	 * Initialize the procedural sequence and set the TargetActor for this builder.
	 * 
	 * @param InActor The target DaySequenceActor that will be animated by the generated sequence.
	 * @param InitialSequence Optional sequence that this builder can operate on instead of allocating a new sequence.
	 * @param bClearInitialSequence If true, calls ClearAllKeys().
	 * @return The sequence which will be modified when calling SetActiveBoundObject and the Add*Key(s) functions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	UDaySequence* Initialize(ADaySequenceActor* InActor, UDaySequence* InitialSequence = nullptr, bool bClearInitialSequence = true);

	/** Returns true Initialize has been called with a valid actor. */
	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	bool IsInitialized() const;
	
	/** Prepare the builder to begin adding keys animating properties on InObject. */
	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void SetActiveBoundObject(UObject* InObject);

	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void ClearAllKeys();
	
	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void AddScalarKey(FName PropertyName, float Key, double Value, ERichCurveInterpMode InterpMode = RCIM_Cubic);
	void AddScalarKey(FName PropertyName, const TPair<float, double>& KeyValue, ERichCurveInterpMode InterpMode = RCIM_Cubic);
	void AddScalarKeys(FName PropertyName, const TArray<TPair<float, double>>& KeysAndValues, ERichCurveInterpMode InterpMode = RCIM_Cubic);
	
	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void ClearScalarKeys(FName PropertyName);

	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void AddBoolKey(FName PropertyName, float Key, bool Value);
	void AddBoolKey(FName PropertyName, const TPair<float, bool>& KeyValue);
	void AddBoolKeys(FName PropertyName, const TArray<TPair<float, bool>>& KeysAndValues);

	UFUNCTION(BlueprintCallable, Category = "Day Sequence")
	void AddStaticTime(float StaticTime);
	
private:
	FGuid GetOrCreateProceduralBinding(UObject* Object) const;

	template<typename TrackType>
	TrackType* CreateOrAddOverrideTrack(FName Name);

	template<typename TrackType>
	TrackType* CreateOrAddPropertyOverrideTrack(FName Name);

	template<typename TrackType, typename SectionType>
	SectionType* CreateOrAddPropertyOverrideSection(FName Name);
	
private:
	/** This is returned immediately upon creation in InitializeSequence. The caller is responsible for holding a reference to prevent GC. */
	TObjectPtr<UDaySequence> ProceduralDaySequence = nullptr;

	TObjectPtr<ADaySequenceActor> TargetActor = nullptr;
	
	TObjectPtr<UObject> ActiveBoundObject = nullptr;
	FGuid ActiveBinding;
};
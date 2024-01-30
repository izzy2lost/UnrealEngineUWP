// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningObservation.h"

#include "Engine/EngineTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "LearningAgentsObservations.generated.h"

class ULearningAgentsObservationSchema;
class ULearningAgentsObservationObject;
struct FLearningAgentsObservationSchemaElement;
struct FLearningAgentsObservationObjectElement;

class USplineComponent;

/** An element of an Observation Schema */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsObservationSchemaElement
{
	GENERATED_BODY()

	UE::Learning::Observation::FSchemaElement SchemaElement;
};

/** An element of an Observation Object */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsObservationObjectElement
{
	GENERATED_BODY()

	UE::Learning::Observation::FObjectElement ObjectElement;
};

/** Comparison operator for Observation Object Elements */
bool operator==(const FLearningAgentsObservationObjectElement& Lhs, const FLearningAgentsObservationObjectElement& Rhs);

/** Hashing operator for Observation Object Elements */
uint32 GetTypeHash(const FLearningAgentsObservationObjectElement& Element);

/** Enum Type representing either observation A or observation B */
UENUM(BlueprintType)
enum class ELearningAgentsEitherObservation : uint8
{
	A,
	B,
};

/** Enum Type representing either a Null observation or some Valid observation */
UENUM(BlueprintType)
enum class ELearningAgentsOptionalObservation : uint8
{
	Null,
	Valid,
};

/**
 * Observation Functions
 * 
 * Convenience functions useful for constructing observations.
 */
UCLASS()
class ULearningAgentsObservationFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Project a transform onto the ground plane, leaving just rotation around the vertical axis */
	UFUNCTION(BlueprintPure, Category = "Learning Agents")
	static FTransform ProjectTransformOntoGroundPlane(const FTransform Transform, const FVector LocalForwardVector = FVector::ForwardVector, const float GroundPlaneHeight = 0.0f);
};

/**
 * Observation Schema
 *
 * This object is used to construct a schema describing some structure of observations.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservationSchema : public UObject
{
	GENERATED_BODY()

public:

	/** Gets the internal observation schema object */
	const UE::Learning::Observation::FSchema& GetObservationSchema() const;

	/**
	 * Validates that the given object matches the schema. Will log errors on objects that don't match.
	 *
	 * @param SchemaElement			Schema Element
	 * @param Object				Observation Object
	 * @param ObjectElement			Observation Object Element
	 * @returns						true if the object matches the schema
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool ValidateObjectMatchesSchema(
		const FLearningAgentsObservationSchemaElement SchemaElement,
		const ULearningAgentsObservationObject* Object,
		const FLearningAgentsObservationObjectElement ObjectElement) const;

public:

	// Basic Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyNullObservation(const FName Name = TEXT("Null"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyContinuousObservation(const int32 Size, const FName Name = TEXT("Continuous"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyExclusiveDiscreteObservation(const int32 Size, const FName Name = TEXT("ExclusiveDiscrete"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyInclusiveDiscreteObservation(const int32 Size, const FName Name = TEXT("InclusiveDiscrete"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyIndexObservation(const int32 Size, const FName Name = TEXT("Index"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyStructObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const FName Name = TEXT("Struct"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyStructObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const FName Name = TEXT("Struct"));
	FLearningAgentsObservationSchemaElement SpecifyStructObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const FName Name = TEXT("Struct"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize = 128, const FName Name = TEXT("ExclusiveUnion"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize = 128, const FName Name = TEXT("ExclusiveUnion"));
	FLearningAgentsObservationSchemaElement SpecifyExclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 EncodingSize = 128, const FName Name = TEXT("ExclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("InclusiveUnion"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("InclusiveUnion"));
	FLearningAgentsObservationSchemaElement SpecifyInclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("InclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyStaticArrayObservation(const FLearningAgentsObservationSchemaElement Element, const int32 Num, const FName Name = TEXT("StaticArray"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifySetObservation(const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("Set"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyPairObservation(const FLearningAgentsObservationSchemaElement Key, const FLearningAgentsObservationSchemaElement Value, const FName Name = TEXT("Pair"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyArrayObservation(const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("Array"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyMapObservation(const FLearningAgentsObservationSchemaElement KeyElement, const FLearningAgentsObservationSchemaElement ValueElement, const int32 MaxNum, const int32 AttentionEncodingSize = 32, const int32 AttentionHeadNum = 4, const int32 ValueEncodingSize = 32, const FName Name = TEXT("Map"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyEnumObservation(const UEnum* Enum, const FName Name = TEXT("Enum"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyBitmaskObservation(const UEnum* Enum, const FName Name = TEXT("Bitmask"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyOptionalObservation(const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize = 128, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyEitherObservation(const FLearningAgentsObservationSchemaElement A, const FLearningAgentsObservationSchemaElement B, const int32 EncodingSize = 128, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyEncodingObservation(const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize = 128, const FName Name = TEXT("Encoding"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyBoolObservation(const FName Name = TEXT("Bool"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyFloatObservation(const FName Name = TEXT("Float"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyLocationObservation(const FName Name = TEXT("Location"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyRotationObservation(const FName Name = TEXT("Rotation"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyScaleObservation(const FName Name = TEXT("Scale"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyTransformObservation(const FName Name = TEXT("Transform"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyAngleObservation(const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyVelocityObservation(const FName Name = TEXT("Velocity"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyDirectionObservation(const FName Name = TEXT("Direction"));

	// Spline Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyLocationAlongSplineObservation(const FName Name = TEXT("LocationAlongSpline"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyProportionAlongSplineObservation(const FName Name = TEXT("ProportionAlongSpline"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyDirectionAlongSplineObservation(const FName Name = TEXT("DirectionAlongSpline"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyPropertiesAlongSplineObservation(const FName Name = TEXT("PropertiesAlongSpline"));

	// Ray Cast Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyProportionAlongRayObservation(const FName Name = TEXT("ProportionAlongRay"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationSchemaElement SpecifyProportionAlongRaysObservation(const int32 Num, const FName Name = TEXT("ProportionAlongRays"));

private:

	UE::Learning::Observation::FSchema ObservationSchema;
};

UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservationVisualLoggerObject : public UObject
{
	GENERATED_BODY()
};

/**
 * Observation Object
 *
 * This object is used to construct or get the values of observations.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsObservationObject : public UObject
{
	GENERATED_BODY()

public:

	/** Gets the internal observation object */
	const UE::Learning::Observation::FObject& GetObservationObject() const;

	/** Gets the internal observation object */
	UE::Learning::Observation::FObject& GetObservationObject();

public:

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	void LogObservation(const FLearningAgentsObservationObjectElement Element);

public:

	// Basic Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeNullObservation(const FName Name = TEXT("Null"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeContinuousObservation(const TArray<float>& Values, const FName Name = TEXT("Continuous"));
	FLearningAgentsObservationObjectElement MakeContinuousObservationFromArrayView(const TArrayView<const float> Values, const FName Name = TEXT("Continuous"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeExclusiveDiscreteObservation(const int32 DiscreteIndex, const int32 Size, const FName Name = TEXT("ExclusiveDiscrete"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeInclusiveDiscreteObservation(const TArray<int32>& DiscreteIndices, const int32 Size, const FName Name = TEXT("InclusiveDiscrete"));
	FLearningAgentsObservationObjectElement MakeInclusiveDiscreteObservationFromArrayView(const TArrayView<const int32> DiscreteIndices, const int32 Size, const FName Name = TEXT("InclusiveDiscrete"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeIndexObservation(const int32 Index, const int32 Size, const FName Name = TEXT("Index"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeStructObservation(const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("Struct"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeStructObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("Struct"));
	FLearningAgentsObservationObjectElement MakeStructObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name = TEXT("Struct"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeExclusiveUnionObservation(const FName ElementName, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ExclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeInclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("InclusiveUnion"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeInclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("InclusiveUnion"));
	FLearningAgentsObservationObjectElement MakeInclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name = TEXT("InclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeStaticArrayObservation(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("StaticArray"));
	FLearningAgentsObservationObjectElement MakeStaticArrayObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name = TEXT("StaticArray"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeSetObservation(const TSet<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("Set"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeSetObservationFromArray(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("Set"));
	FLearningAgentsObservationObjectElement MakeSetObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name = TEXT("Set"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakePairObservation(const FLearningAgentsObservationObjectElement Key, const FLearningAgentsObservationObjectElement Value, const FName Name = TEXT("Pair"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeArrayObservation(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name = TEXT("Array"));
	FLearningAgentsObservationObjectElement MakeArrayObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name = TEXT("Array"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeMapObservation(const TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& Map, const FName Name = TEXT("Map"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeMapObservationFromArrays(const TArray<FLearningAgentsObservationObjectElement>& Keys, const TArray<FLearningAgentsObservationObjectElement>& Values, const FName Name = TEXT("Map"));
	FLearningAgentsObservationObjectElement MakeMapObservationFromArrayViews(const TArrayView<const FLearningAgentsObservationObjectElement> Keys, const TArrayView<const FLearningAgentsObservationObjectElement> Values, const FName Name = TEXT("Map"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeEnumObservation(const UEnum* Enum, const uint8 EnumValue, const FName Name = TEXT("Enum"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeBitmaskObservation(const UEnum* Enum, const int32 BitmaskValue, const FName Name = TEXT("Bitmask"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeOptionalObservation(const FLearningAgentsObservationObjectElement Element, const ELearningAgentsOptionalObservation Option, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeOptionalNullObservation(const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeOptionalValidObservation(const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeEitherObservation(const FLearningAgentsObservationObjectElement Element, const ELearningAgentsEitherObservation Either, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta=(DisplayName="Make Either A Observation"))
	FLearningAgentsObservationObjectElement MakeEitherAObservation(const FLearningAgentsObservationObjectElement A, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DisplayName = "Make Either B Observation"))
	FLearningAgentsObservationObjectElement MakeEitherBObservation(const FLearningAgentsObservationObjectElement B, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeEncodingObservation(const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Encoding"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeBoolObservation(const bool bValue, const FName Name = TEXT("Bool"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeFloatObservation(const float Value, const float FloatScale = 1.0f, const FName Name = TEXT("Float"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	FLearningAgentsObservationObjectElement MakeLocationObservation(
		const FVector Location,
		const FTransform RelativeTransform = FTransform(),
		const float LocationScale = 100.0f,
		const FName Name = TEXT("Location"),
		const bool bVisualLoggerEnabled = false,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeRotationObservation(const FRotator Rotation, const FRotator RelativeRotation = FRotator::ZeroRotator, const FName Name = TEXT("Rotation"));
	FLearningAgentsObservationObjectElement MakeRotationObservationFromQuat(const FQuat Rotation, const FQuat RelativeRotation = FQuat::Identity, const FName Name = TEXT("Rotation"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeScaleObservation(const FVector Scale, const FVector RelativeScale = FVector(1,1,1), const FName Name = TEXT("Scale"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeTransformObservation(const FTransform Transform, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Transform"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeAngleObservation(const float Angle, const float RelativeAngle = 0.0f, const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeAngleObservationRadians(const float Angle, const float RelativeAngle = 0.0f, const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeVelocityObservation(const FVector Velocity, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Name = TEXT("Velocity"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 3))
	FLearningAgentsObservationObjectElement MakeDirectionObservation(
		const FVector Direction,
		const FTransform RelativeTransform = FTransform(),
		const FName Name = TEXT("Direction"),
		const bool bVisualLoggerEnabled = false,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	// Spline Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 5))
	FLearningAgentsObservationObjectElement MakeLocationAlongSplineObservation(
		const USplineComponent* SplineComponent,
		const float DistanceAlongSpline,
		const FTransform RelativeTransform = FTransform(),
		const float LocationScale = 100.0f,
		const FName Name = TEXT("LocationAlongSpline"),
		const bool bVisualLoggerEnabled = false,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeProportionAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FName Name = TEXT("ProportionAlongSpline"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AdvancedDisplay = 4))
	FLearningAgentsObservationObjectElement MakeDirectionAlongSplineObservation(
		const USplineComponent* SplineComponent,
		const float DistanceAlongSpline,
		const FTransform RelativeTransform = FTransform(),
		const FName Name = TEXT("DirectionAlongSpline"),
		const bool bVisualLoggerEnabled = false,
		const FVector VisualLoggerLocation = FVector::ZeroVector,
		const FLinearColor VisualLoggerColor = FLinearColor::Red);

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakePropertiesAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("PropertiesAlongSpline"));

	// Ray Cast Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeProportionAlongRayObservation(const FVector RayStart, const FVector RayEnd, const FTransform RayTransform = FTransform(), const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic, const FName Name = TEXT("ProportionAlongRay"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsObservationObjectElement MakeProportionAlongRaysObservation(const TArray<FVector>& RayStarts, const TArray<FVector>& RayEnds, const FTransform RayTransform, const ECollisionChannel CollisionChannel = ECollisionChannel::ECC_WorldStatic, const FName Name = TEXT("ProportionAlongRays"));

public:

	// Basic Observations

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetNullObservation(const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Null")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetContinuousObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Continuous")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetContinuousObservation(TArray<float>& OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Continuous")) const;
	UPARAM(DisplayName = "Success") bool GetContinuousObservationToArrayView(TArrayView<float> OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Continuous")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetExclusiveDiscreteObservation(int32& OutIndex, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ExclusiveDiscrete")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveDiscrete")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteObservation(TArray<int32>& OutIndices, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveDiscrete")) const;
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteObservationToArrayView(TArrayView<int32> OutIndices, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveDiscrete")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetIndexObservation(int32& OutIndex, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Index")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Struct")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Struct")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Struct")) const;
	UPARAM(DisplayName = "Success") bool GetStructObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Struct")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetExclusiveUnionObservation(FName& OutElementName, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ExclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStaticArrayObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("StaticArray")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStaticArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("StaticArray")) const;
	UPARAM(DisplayName = "Success") bool GetStaticArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("StaticArray")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetSetObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Set")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetSetObservation(TSet<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Set")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetSetObservationToArray(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Set")) const;
	UPARAM(DisplayName = "Success") bool GetSetObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Set")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetPairObservation(FLearningAgentsObservationObjectElement& OutKey, FLearningAgentsObservationObjectElement& OutValue, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Pair")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetArrayObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Array")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Array")) const;
	UPARAM(DisplayName = "Success") bool GetArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Array")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetMapObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Map")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetMapObservation(TMap<FLearningAgentsObservationObjectElement,FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Map")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetMapObservationToArrays(TArray<FLearningAgentsObservationObjectElement>& OutKeys, TArray<FLearningAgentsObservationObjectElement>& OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Map")) const;
	UPARAM(DisplayName = "Success") bool GetMapObservationToArrayViews(TArrayView<FLearningAgentsObservationObjectElement> OutKeys, TArrayView<FLearningAgentsObservationObjectElement> OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Map")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetEnumObservation(uint8& OutEnumValue, const UEnum* Enum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Enum")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetBitmaskObservation(int32& OutBitmaskValue, const UEnum* Enum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Bitmask")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (ExpandEnumAsExecs = "OutOption"))
	UPARAM(DisplayName = "Success") bool GetOptionalObservation(ELearningAgentsOptionalObservation& OutOption, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Optional")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (ExpandEnumAsExecs = "OutEither"))
	UPARAM(DisplayName = "Success") bool GetEitherObservation(ELearningAgentsEitherObservation& OutEither, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Either")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetEncodingObservation(FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Encoding")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetBoolObservation(bool& bOutValue, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("Bool")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetFloatObservation(float& OutValue, const FLearningAgentsObservationObjectElement Element, const float FloatScale = 1.0f, const FName Name = TEXT("Float")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetLocationObservation(FVector& OutLocation, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Location")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetRotationObservation(FRotator& OutRotation, const FLearningAgentsObservationObjectElement Element, const FRotator RelativeRotation = FRotator::ZeroRotator, const FName Name = TEXT("Rotation")) const;
	UPARAM(DisplayName = "Success") bool GetRotationObservationAsQuat(FQuat& OutRotation, const FLearningAgentsObservationObjectElement Element, const FQuat RelativeRotation = FQuat::Identity, const FName Name = TEXT("Rotation")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetScaleObservation(FVector& OutScale, const FLearningAgentsObservationObjectElement Element, const FVector RelativeScale = FVector(1, 1, 1), const FName Name = TEXT("Scale")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetTransformObservation(FTransform& OutTransform, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Transform")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetAngleObservation(float& OutAngle, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle = 0.0f, const FName Name = TEXT("Angle")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetAngleObservationRadians(float& OutAngle, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle = 0.0f, const FName Name = TEXT("Angle")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetVelocityObservation(FVector& OutVelocity, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Name = TEXT("Velocity")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetDirectionObservation(FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const FName Name = TEXT("Direction")) const;

	// Spline Observations

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetLocationAlongSplineObservation(FVector& OutLocation, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("LocationAlongSpline"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetProportionAlongSplineObservation(bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ProportionAlongSpline"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetDirectionAlongSplineObservation(FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const FName Name = TEXT("DirectionAlongSpline"));

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetPropertiesAlongSplineObservation(FVector& OutLocation, bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("PropertiesAlongSpline"));

	// Ray Cast Observations

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetProportionAlongRayObservation(float& OutProportion, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ProportionAlongRay"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetProportionAlongRaysObservationNum(int32& OutProportionNum, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ProportionAlongRays"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetProportionAlongRaysObservation(TArray<float>& OutProportions, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ProportionAlongRays"));
	UPARAM(DisplayName = "Success") bool GetProportionAlongRaysObservationToArrayView(TArrayView<float> OutProportions, const FLearningAgentsObservationObjectElement Element, const FName Name = TEXT("ProportionAlongRays"));


private:

	UE::Learning::Observation::FObject ObservationObject;

	const ULearningAgentsObservationVisualLoggerObject* GetOrAddVisualLoggerObject(const FName Name);

	UPROPERTY()
	TMap<FName, TObjectPtr<const ULearningAgentsObservationVisualLoggerObject>> VisualLoggerObjects;
};

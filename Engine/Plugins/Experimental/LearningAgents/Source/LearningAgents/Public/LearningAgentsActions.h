// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"
#include "LearningAction.h"

#include "LearningAgentsActions.generated.h"

class ULearningAgentsActionSchema;
class ULearningAgentsActionObject;
struct FLearningAgentsActionSchemaElement;
struct FLearningAgentsActionObjectElement;

/** An element of an Action Schema */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsActionSchemaElement
{
	GENERATED_BODY()

	UE::Learning::Action::FSchemaElement SchemaElement;
};

/** An element of an Action Object */
USTRUCT(BlueprintType)
struct LEARNINGAGENTS_API FLearningAgentsActionObjectElement
{
	GENERATED_BODY()

	UE::Learning::Action::FObjectElement ObjectElement;
};

/** Comparison operator for Action Object Elements */
bool operator==(const FLearningAgentsActionObjectElement& Lhs, const FLearningAgentsActionObjectElement& Rhs);

/** Hashing operator for Action Object Elements */
uint32 GetTypeHash(const FLearningAgentsActionObjectElement& Element);

/** Enum Type representing either action A or action B */
UENUM(BlueprintType)
enum class ELearningAgentsEitherAction : uint8
{
	A,
	B,
};

/** Enum Type representing either a Null action or some Valid action */
UENUM(BlueprintType)
enum class ELearningAgentsOptionalAction : uint8
{
	Null,
	Valid,
};

/**
 * Action Schema
 * 
 * This object is used to construct a schema describing some structure of actions.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsActionSchema : public UObject
{
	GENERATED_BODY()

public:

	/** Gets the internal action schema object */
	const UE::Learning::Action::FSchema& GetActionSchema() const;

	/**
	 * Validates that the given object matches the schema. Will log errors on objects that don't match.
	 * 
	 * @param SchemaElement			Schema Element
	 * @param Object				Action Object
	 * @param ObjectElement			Action Object Element
	 * @returns						true if the object matches the schema
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool ValidateObjectMatchesSchema(
		const FLearningAgentsActionSchemaElement SchemaElement,
		const ULearningAgentsActionObject* Object,
		const FLearningAgentsActionObjectElement ObjectElement) const;

public:

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyNullAction(const FName Name = TEXT("Null"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyContinuousAction(const int32 Size, const FName Name = TEXT("Continuous"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta=(AutoCreateRefTerm="PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyExclusiveDiscreteAction(const int32 Size, const TArray<float>& PriorProbabilities, const FName Name = TEXT("DiscreteExclusive"));
	FLearningAgentsActionSchemaElement SpecifyExclusiveDiscreteActionFromArrayView(const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("DiscreteExclusive"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyInclusiveDiscreteAction(const int32 Size, const TArray<float>& PriorProbabilities, const FName Name = TEXT("DiscreteInclusive"));
	FLearningAgentsActionSchemaElement SpecifyInclusiveDiscreteActionFromArrayView(const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("DiscreteInclusive"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyStructAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const FName Name = TEXT("Struct"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyStructActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const FName Name = TEXT("Struct"));
	FLearningAgentsActionSchemaElement SpecifyStructActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const FName Name = TEXT("Struct"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyExclusiveUnionAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Name = TEXT("ExclusiveUnion"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyExclusiveUnionActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Name = TEXT("ExclusiveUnion"));
	FLearningAgentsActionSchemaElement SpecifyExclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("ExclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyInclusiveUnionAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Name = TEXT("InclusiveUnion"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyInclusiveUnionActionFromArrays(const TArray<FName> ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Name = TEXT("InclusiveUnion"));
	FLearningAgentsActionSchemaElement SpecifyInclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("InclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyStaticArrayAction(const FLearningAgentsActionSchemaElement Element, const int32 Num, const FName Name = TEXT("StaticArray"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyPairAction(const FLearningAgentsActionSchemaElement Key, const FLearningAgentsActionSchemaElement Value, const FName Name = TEXT("Pair"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyEnumAction(const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Name = TEXT("Enum"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyEnumActionFromArray(const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Name = TEXT("Enum"));
	FLearningAgentsActionSchemaElement SpecifyEnumActionFromArrayView(const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("Enum"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyBitmaskAction(const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Name = TEXT("Bitmask"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AutoCreateRefTerm = "PriorProbabilities"))
	FLearningAgentsActionSchemaElement SpecifyBitmaskActionFromArray(const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Name = TEXT("Bitmask"));
	FLearningAgentsActionSchemaElement SpecifyBitmaskActionFromArrayView(const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Name = TEXT("Bitmask"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyOptionalAction(const FLearningAgentsActionSchemaElement Element, const float PriorProbability = 0.5f, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyEitherAction(const FLearningAgentsActionSchemaElement A, const FLearningAgentsActionSchemaElement B, const float PriorProbabilityOfA = 0.5f, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyEncodingAction(const FLearningAgentsActionSchemaElement Element, const int32 EncodingSize = 128, const FName Name = TEXT("Encoding"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyBoolAction(const float PriorProbability = 0.5f, const FName Name = TEXT("Bool"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyFloatAction(const FName Name = TEXT("Float"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyLocationAction(const FName Name = TEXT("Location"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyRotationAction(const FName Name = TEXT("Rotation"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyScaleAction(const FName Name = TEXT("Scale"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyTransformAction(const FName Name = TEXT("Transform"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyAngleAction(const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifyVelocityAction(const FName Name = TEXT("Velocity"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionSchemaElement SpecifySpeedAction(const FName Name = TEXT("Speed"));

private:

	UE::Learning::Action::FSchema ActionSchema;
};

/**
 * Action Object
 *
 * This object is used to construct or get the values of actions.
 */
UCLASS(BlueprintType)
class LEARNINGAGENTS_API ULearningAgentsActionObject : public UObject
{
	GENERATED_BODY()

public:

	/** Gets the internal action object */
	const UE::Learning::Action::FObject& GetActionObject() const;

	/** Gets the internal action object */
	UE::Learning::Action::FObject& GetActionObject();

public:

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	void LogAction(const FLearningAgentsActionObjectElement Element);

public:

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeNullAction(const FName Name = TEXT("Null"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeContinuousAction(const TArray<float>& Values, const FName Name = TEXT("Continuous"));
	FLearningAgentsActionObjectElement MakeContinuousActionFromArrayView(const TArrayView<const float> Values, const FName Name = TEXT("Continuous"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeExclusiveDiscreteAction(const int32 Index, const FName Name = TEXT("DiscreteExclusive"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeInclusiveDiscreteAction(const TArray<int32>& Indices, const FName Name = TEXT("DiscreteInclusive"));
	FLearningAgentsActionObjectElement MakeInclusiveDiscreteActionFromArrayView(const TArrayView<const int32> Indices, const FName Name = TEXT("DiscreteInclusive"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeStructAction(const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Name = TEXT("Struct"));
	
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeStructActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name = TEXT("Struct"));
	FLearningAgentsActionObjectElement MakeStructActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name = TEXT("Struct"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeExclusiveUnionAction(const FName ElementName, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("ExclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeInclusiveUnionAction(const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Name = TEXT("InclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeInclusiveUnionActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name = TEXT("InclusiveUnion"));
	FLearningAgentsActionObjectElement MakeInclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name = TEXT("InclusiveUnion"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeStaticArrayAction(const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name = TEXT("StaticArray"));
	FLearningAgentsActionObjectElement MakeStaticArrayActionFromArrayView(const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name = TEXT("StaticArray"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakePairAction(const FLearningAgentsActionObjectElement Key, const FLearningAgentsActionObjectElement Value, const FName Name = TEXT("Pair"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeEnumAction(const UEnum* Enum, const uint8 EnumValue, const FName Name = TEXT("Enum"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeBitmaskAction(const UEnum* Enum, const int32 BitmaskValue, const FName Name = TEXT("Bitmask"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeOptionalAction(const FLearningAgentsActionObjectElement Element, const ELearningAgentsOptionalAction Option, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeOptionalNullAction(const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeOptionalValidAction(const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Optional"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeEitherAction(const FLearningAgentsActionObjectElement Element, const ELearningAgentsEitherAction Either, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DisplayName = "Make Either A Action"))
	FLearningAgentsActionObjectElement MakeEitherAAction(const FLearningAgentsActionObjectElement A, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DisplayName = "Make Either B Action"))
	FLearningAgentsActionObjectElement MakeEitherBAction(const FLearningAgentsActionObjectElement B, const FName Name = TEXT("Either"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeEncodingAction(const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Encoding"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeBoolAction(const bool bValue, const FName Name = TEXT("Bool"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeFloatAction(const float Value, const float FloatScale = 1.0f, const FName Name = TEXT("Float"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeLocationAction(const FVector Location, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Location"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeRotationAction(const FRotator Rotation, const FRotator RelativeRotation = FRotator::ZeroRotator, const float RotationScale = 90.0f, const FName Name = TEXT("Rotation"));
	FLearningAgentsActionObjectElement MakeRotationActionFromQuat(const FQuat Rotation, const FQuat RelativeRotation = FQuat::Identity, const float RotationScale = 90.0f, const FName Name = TEXT("Rotation"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeScaleAction(const FVector Scale, const FVector RelativeScale = FVector(1,1,1), const FName Name = TEXT("Scale"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeTransformAction(const FTransform Transform, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Transform"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeAngleAction(const float Angle, const float RelativeAngle = 0.0f, const float AngleScale = 90.0f, const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeAngleActionRadians(const float Angle, const float RelativeAngle = 0.0f, const float AngleScale = 1.57079632679f, const FName Name = TEXT("Angle"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeVelocityAction(const FVector Velocity, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Name = TEXT("Velocity"));

	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	FLearningAgentsActionObjectElement MakeSpeedAction(const float Speed, const float SpeedScale = 200.0f, const FName Name = TEXT("Speed"));

public:

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetNullAction(const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Null")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetContinuousActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Continuous")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetContinuousAction(TArray<float>& OutValues, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Continuous")) const;
	UPARAM(DisplayName = "Success") bool GetContinuousActionToArrayView(TArrayView<float> OutValues, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Continuous")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetExclusiveDiscreteAction(int32& OutIndex, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("DiscreteExclusive")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("DiscreteInclusive")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteAction(TArray<int32>& OutIndices, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("DiscreteInclusive")) const;
	UPARAM(DisplayName = "Success") bool GetInclusiveDiscreteActionToArrayView(TArrayView<int32> OutIndices, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("DiscreteInclusive")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Struct")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Struct")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStructActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Struct")) const;
	UPARAM(DisplayName = "Success") bool GetStructActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Struct")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetExclusiveUnionAction(FName& OutElementName, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("ExclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;
	UPARAM(DisplayName = "Success") bool GetInclusiveUnionActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("InclusiveUnion")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStaticArrayActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("StaticArray")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetStaticArrayAction(TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("StaticArray")) const;
	UPARAM(DisplayName = "Success") bool GetStaticArrayActionToArrayView(TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("StaticArray")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetPairAction(FLearningAgentsActionObjectElement& OutKey, FLearningAgentsActionObjectElement& OutValue, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Pair")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetEnumAction(uint8& OutEnumValue, const UEnum* Enum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Enum")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetBitmaskAction(int32& OutBitmaskValue, const UEnum* Enum, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Bitmask")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (ExpandEnumAsExecs = "OutOption"))
	UPARAM(DisplayName = "Success") bool GetOptionalAction(ELearningAgentsOptionalAction& OutOption, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Optional")) const;
	
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (ExpandEnumAsExecs = "OutEither"))
	UPARAM(DisplayName = "Success") bool GetEitherAction(ELearningAgentsEitherAction& OutEither, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Either")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetEncodingAction(FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Encoding")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetBoolAction(bool& bOutValue, const FLearningAgentsActionObjectElement Element, const FName Name = TEXT("Bool")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetFloatAction(float& OutValue, const FLearningAgentsActionObjectElement Element, const float FloatScale = 1.0f, const FName Name = TEXT("Float")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetLocationAction(FVector& OutLocation, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const FName Name = TEXT("Location")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetRotationAction(FRotator& OutRotation, const FLearningAgentsActionObjectElement Element, const FRotator RelativeRotation = FRotator::ZeroRotator, const float RotationScale = 90.0f, const FName Name = TEXT("Rotation")) const;
	UPARAM(DisplayName = "Success") bool GetRotationActionAsQuat(FQuat& OutRotation, const FLearningAgentsActionObjectElement Element, const FQuat RelativeRotation = FQuat::Identity, const float RotationScale = 90.0f, const FName Name = TEXT("Rotation")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetScaleAction(FVector& OutScale, const FLearningAgentsActionObjectElement Element, const FVector RelativeScale = FVector(1,1,1), const float Scale = 1.0f, const FName Name = TEXT("Scale")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetTransformAction(FTransform& OutTransform, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform = FTransform(), const float LocationScale = 100.0f, const float RotationScale = 1.0f, const float ScaleScale = 1.0f, const FName Name = TEXT("Transform")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetAngleAction(float& OutAngle, const FLearningAgentsActionObjectElement Element, const float RelativeAngle = 0.0f, const float AngleScale = 90.0f, const FName Name = TEXT("Angle")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetAngleActionRadians(float& OutAngle, const FLearningAgentsActionObjectElement Element, const float RelativeAngle = 0.0f, const float AngleScale = 1.57079632679f, const FName Name = TEXT("Angle")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetVelocityAction(FVector& OutVelocity, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform = FTransform(), const float VelocityScale = 200.0f, const FName Name = TEXT("Velocity")) const;

	UFUNCTION(BlueprintPure = false, Category = "LearningAgents")
	UPARAM(DisplayName = "Success") bool GetSpeedAction(float& OutSpeed, const FLearningAgentsActionObjectElement Element, const float SpeedScale = 200.0f, const FName Name = TEXT("Speed")) const;

private:

	UE::Learning::Action::FObject ActionObject;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsObservations.h"

#include "LearningArray.h"
#include "LearningLog.h"

#include "Containers/StaticArray.h"

#include "Runtime/Engine/Public/DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"

bool operator==(const FLearningAgentsObservationObjectElement& Lhs, const FLearningAgentsObservationObjectElement& Rhs)
{
	return Lhs.ObjectElement.Index == Rhs.ObjectElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsObservationObjectElement& Element)
{
	return (uint32)Element.ObjectElement.Index;
}

const UE::Learning::Observation::FSchema& ULearningAgentsObservationSchema::GetObservationSchema() const
{
	return ObservationSchema;
}

namespace UE::Learning::Agents::Observation::Private
{
	static inline bool ContainsDuplicates(const TArrayView<const int32> Indices)
	{
		TSet<int32, DefaultKeyFuncs<int32>, TInlineSetAllocator<32>> IndicesSet;
		IndicesSet.Append(Indices);
		return Indices.Num() != IndicesSet.Num();
	}

	static inline bool ContainsDuplicates(const TArrayView<const FName> ElementNames)
	{
		TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<32>> ElementNameSet;
		ElementNameSet.Append(ElementNames);
		return ElementNames.Num() != ElementNameSet.Num();
	}

	static inline const TCHAR* GetObservationTypeString(const Learning::Observation::EType ObservationType)
	{
		switch (ObservationType)
		{
		case  Learning::Observation::EType::Null: return TEXT("Null");
		case  Learning::Observation::EType::Continuous: return TEXT("Continuous");
		case  Learning::Observation::EType::And: return TEXT("Struct");
		case  Learning::Observation::EType::OrExclusive: return TEXT("ExclusiveUnion");
		case  Learning::Observation::EType::OrInclusive: return TEXT("InclusiveUnion");
		case  Learning::Observation::EType::Array: return TEXT("StaticArray");
		case  Learning::Observation::EType::Set: return TEXT("Set");
		case  Learning::Observation::EType::Encoding: return TEXT("Encoding");
		default:
			UE_LEARNING_NOT_IMPLEMENTED();
			return TEXT("Unimplemented");
		}
	}

	static bool ValidateObjectMatchesSchema(
		const Learning::Observation::FSchema& Schema,
		const Learning::Observation::FSchemaElement SchemaElement,
		const Learning::Observation::FObject& Object,
		const Learning::Observation::FObjectElement ObjectElement,
		const FString& ObjectName)
	{
		// Check Elements are Valid

		if (!Schema.IsValid(SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Schema Element."), *ObjectName);
			return false;
		}

		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object Element."), *ObjectName);
			return false;
		}

		// Check Names Match

		const FName ObservationSchemaElementName = Schema.GetName(SchemaElement);
		const FName ObservationObjectElementName = Object.GetName(ObjectElement);

		if (ObservationSchemaElementName != ObservationObjectElementName)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match Schema. Expected '%s', got '%s'."),
				*ObjectName, *ObservationSchemaElementName.ToString(), *ObservationObjectElementName.ToString());
		}

		// Check Types Match

		const Learning::Observation::EType ObservationSchemaElementType = Schema.GetType(SchemaElement);
		const Learning::Observation::EType ObservationObjectElementType = Object.GetType(ObjectElement);

		if (ObservationSchemaElementType != ObservationObjectElementType)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match Schema. Expected type '%s', got type '%s'."),
				*ObjectName,
				*ObservationSchemaElementName.ToString(),
				GetObservationTypeString(ObservationSchemaElementType),
				GetObservationTypeString(ObservationObjectElementType));
			return false;
		}

		// Type Specific Checks

		switch (ObservationSchemaElementType)
		{
		case Learning::Observation::EType::Null: return true;
		
		case Learning::Observation::EType::Continuous:
		{
			const int32 SchemaElementSize = Schema.GetContinuous(SchemaElement).Num;
			const int32 ObjectElementSize = Object.GetContinuous(ObjectElement).Values.Num();

			if (SchemaElementSize != ObjectElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					SchemaElementSize,
					ObjectElementSize);
				return false;
			}

			return true;
		}

		case Learning::Observation::EType::And:
		{
			const Learning::Observation::FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const Learning::Observation::FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());
			UE_LEARNING_CHECK(ObjectParameters.Elements.Num() == ObjectParameters.ElementNames.Num());

			if (SchemaParameters.Elements.Num() != ObjectParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' number of sub-elements does not match Schema. Expected '%i', got '%i'."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' does not include '%s' observation required by Schema."),
						*ObjectName,
						*ObservationSchemaElementName.ToString(),
						*SchemaParameters.ElementNames[SchemaElementIdx].ToString());
					return false;
				}

				if (!ValidateObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIdx],
					ObjectName))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::OrExclusive:
		{
			const Learning::Observation::FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const Learning::Observation::FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());

			const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);

			if (SchemaSubElementIdx == INDEX_NONE)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' Schema does not include '%s' observation."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					*ObjectParameters.ElementName.ToString());
				return false;
			}

			return ValidateObjectMatchesSchema(
				Schema,
				SchemaParameters.Elements[SchemaSubElementIdx],
				Object,
				ObjectParameters.Element,
				ObjectName);
		}

		case Learning::Observation::EType::OrInclusive:
		{
			const Learning::Observation::FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const Learning::Observation::FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' too many sub-observations provided. Expected at most '%i', got '%i'."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ObjectSubElementIdx = 0; ObjectSubElementIdx < ObjectParameters.Elements.Num(); ObjectSubElementIdx++)
			{
				const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectSubElementIdx]);

				if (SchemaSubElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' Schema does not include '%s' observation."),
						*ObjectName,
						*ObservationSchemaElementName.ToString(),
						*ObjectParameters.ElementNames[ObjectSubElementIdx].ToString());
					return false;
				}

				if (!ValidateObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaSubElementIdx],
					Object,
					ObjectParameters.Elements[ObjectSubElementIdx],
					ObjectName))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Array:
		{
			const Learning::Observation::FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const Learning::Observation::FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);

			if (ObjectParameters.Elements.Num() != SchemaParameters.Num)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' array incorrect size. Expected '%i' elements, got '%i'."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					SchemaParameters.Num,
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx],
					ObjectName))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Set:
		{
			const Learning::Observation::FSchemaSetParameters SchemaParameters = Schema.GetSet(SchemaElement);
			const Learning::Observation::FObjectSetParameters ObjectParameters = Object.GetSet(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.MaxNum)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' set too large. Expected at most '%i' elements, got '%i'."),
					*ObjectName,
					*ObservationSchemaElementName.ToString(),
					SchemaParameters.MaxNum,
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx],
					ObjectName))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Encoding:
		{
			const Learning::Observation::FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const Learning::Observation::FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			return ValidateObjectMatchesSchema(
				Schema,
				SchemaParameters.Element,
				Object,
				ObjectParameters.Element,
				ObjectName);
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return true;
		}
		}
	}

	static void LogObservation(
		const UE::Learning::Observation::FObject& Object, 
		const UE::Learning::Observation::FObjectElement ObjectElement,
		const FString& Indentation,
		const FString& Prefix,
		const FString& ObjectName)
	{
		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object Element."), *ObjectName);
			return;
		}

		const UE::Learning::Observation::EType Type = Object.GetType(ObjectElement);
		const FName Name = Object.GetName(ObjectElement);

		switch (Type)
		{
		case UE::Learning::Observation::EType::Null:
		{
			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			return;
		}

		case UE::Learning::Observation::EType::Continuous:
		{
			const UE::Learning::Observation::FObjectContinuousParameters Parameters = Object.GetContinuous(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type), *UE::Learning::Array::FormatFloat(Parameters.Values));
			return;
		}

		case UE::Learning::Observation::EType::And:
		{
			const UE::Learning::Observation::FObjectAndParameters Parameters = Object.GetAnd(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()), ObjectName);
			}

			return;
		}

		case UE::Learning::Observation::EType::OrExclusive:
		{
			const UE::Learning::Observation::FObjectOrExclusiveParameters Parameters = Object.GetOrExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			LogObservation(Object, Parameters.Element, *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementName.ToString()), ObjectName);

			return;
		}

		case UE::Learning::Observation::EType::OrInclusive:
		{
			const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = Object.GetOrInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()), ObjectName);
			}

			return;
		}

		case UE::Learning::Observation::EType::Array:
		{
			const UE::Learning::Observation::FObjectArrayParameters Parameters = Object.GetArray(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx), ObjectName);
			}

			return;
		}

		case UE::Learning::Observation::EType::Set:
		{
			const UE::Learning::Observation::FObjectSetParameters Parameters = Object.GetSet(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx), ObjectName);
			}

			return;
		}

		case UE::Learning::Observation::EType::Encoding:
		{
			const UE::Learning::Observation::FObjectEncodingParameters Parameters = Object.GetEncoding(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetObservationTypeString(Type));
			LogObservation(Object, Parameters.Element, *(Indentation + TEXT("    ")), TEXT("|"), ObjectName);

			return;
		}
		}
	}

	static inline FVector VectorLogSafe(const FVector V, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector(
			FMath::Loge(FMath::Max(V.X, Epsilon)),
			FMath::Loge(FMath::Max(V.Y, Epsilon)),
			FMath::Loge(FMath::Max(V.Z, Epsilon)));
	}

	static inline FVector VectorExp(const FVector V)
	{
		return FVector(
			FMath::Exp(V.X),
			FMath::Exp(V.Y),
			FMath::Exp(V.Z));
	}
}

FTransform ULearningAgentsObservationFunctions::ProjectTransformOntoGroundPlane(const FTransform Transform, const FVector LocalForwardVector, const float GroundPlaneHeight)
{
	FVector Position = Transform.GetLocation();
	Position.Z = GroundPlaneHeight;

	const FVector Direction = (FVector(1.0f, 1.0f, 0.0f) * Transform.TransformVectorNoScale(LocalForwardVector)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	return FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector, Direction), Position, Transform.GetScale3D());
}

bool ULearningAgentsObservationSchema::ValidateObjectMatchesSchema(
	const FLearningAgentsObservationSchemaElement SchemaElement,
	const ULearningAgentsObservationObject* Object,
	const FLearningAgentsObservationObjectElement ObjectElement) const
{
	return UE::Learning::Agents::Observation::Private::ValidateObjectMatchesSchema(
		ObservationSchema,
		SchemaElement.SchemaElement,
		Object->GetObservationObject(),
		ObjectElement.ObjectElement,
		GetName());
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyNullObservation(const FName Name)
{
	return { ObservationSchema.CreateNull(Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyContinuousObservation(const int32 Size, const FName Name)
{
	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Continuous Observation Size '%i'."), *GetName(), Size);
		return FLearningAgentsObservationSchemaElement();
	}
	
	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Continuous Observation."), *GetName());
	}

	return { ObservationSchema.CreateContinuous({ Size }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyExclusiveDiscreteObservation(const int32 Size, const FName Name)
{
	return SpecifyContinuousObservation(Size, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyInclusiveDiscreteObservation(const int32 Size, const FName Name)
{
	return SpecifyContinuousObservation(Size, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyIndexObservation(const int32 Size, const FName Name)
{
	return SpecifyContinuousObservation(Size, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyStructObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Struct Observation."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(Elements.Num());
	SubElementNames.Empty(Elements.Num());
	SubElements.Empty(Elements.Num());

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyStructObservationFromArrayViews(SortedSubElementNames, SortedSubElements, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyStructObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const FName Name)
{
	return SpecifyStructObservationFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyStructObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Struct Observation."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { ObservationSchema.CreateAnd({ ElementNames, SubElements }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyExclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize, const FName Name)
{
	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation EncodingSize '%i'."), *GetName(), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Exclusive Union Observation."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(Elements.Num());
	SubElementNames.Empty(Elements.Num());
	SubElements.Empty(Elements.Num());

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyExclusiveUnionObservationFromArrayViews(SortedSubElementNames, SortedSubElements, EncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyExclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize, const FName Name)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(ElementNames, Elements, EncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyExclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 EncodingSize, const FName Name)
{
	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation EncodingSize '%i'."), *GetName(), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Exclusive Union Observation."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { ObservationSchema.CreateOrExclusive({ ElementNames, SubElements, EncodingSize }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyInclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i."), *GetName(), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Inclusive Union Observation."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyInclusiveUnionObservationFromArrayViews(SortedSubElementNames, SortedSubElements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyInclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	return SpecifyInclusiveUnionObservationFromArrayViews(ElementNames, Elements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyInclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i."), *GetName(), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Inclusive Union Observation."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { ObservationSchema.CreateOrInclusive({ ElementNames, SubElements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyStaticArrayObservation(const FLearningAgentsObservationSchemaElement Element, const int32 Num, const FName Name)
{
	if (Num < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Array Num %i."), *GetName(), Num);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Num == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Static Array Observation."), *GetName());
	}

	if (!ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	return { ObservationSchema.CreateArray({ Element.SchemaElement, Num }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifySetObservation(const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	if (MaxNum < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Set MaxNum %i."), *GetName(), MaxNum);
		return FLearningAgentsObservationSchemaElement();
	}

	if (MaxNum == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Set Observation."), *GetName());
	}

	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i."), *GetName(), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (!ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	return { ObservationSchema.CreateSet({ Element.SchemaElement, MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyPairObservation(const FLearningAgentsObservationSchemaElement Element0, const FLearningAgentsObservationSchemaElement Element1, const FName Name)
{
	return SpecifyStructObservationFromArrayViews({ TEXT("Key"), TEXT("Value") }, { Element0, Element1 }, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyArrayObservation(const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	return SpecifySetObservation(SpecifyPairObservation(SpecifyIndexObservation(MaxNum), Element), MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyMapObservation(const FLearningAgentsObservationSchemaElement KeyElement, const FLearningAgentsObservationSchemaElement ValueElement, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Name)
{
	return SpecifySetObservation(SpecifyPairObservation(KeyElement, ValueElement), MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyEnumObservation(const UEnum* Enum, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	return SpecifyContinuousObservation(Enum->NumEnums() - 1, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyBitmaskObservation(const UEnum* Enum, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		return FLearningAgentsObservationSchemaElement();
	}

	return SpecifyContinuousObservation(Enum->NumEnums() - 1, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyOptionalObservation(const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize, const FName Name)
{
	return SpecifyExclusiveUnionObservationFromArrayViews({ TEXT("Null"), TEXT("Valid") }, { SpecifyNullObservation(), Element }, EncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyEitherObservation(const FLearningAgentsObservationSchemaElement A, const FLearningAgentsObservationSchemaElement B, const int32 EncodingSize, const FName Name)
{
	return SpecifyExclusiveUnionObservationFromArrayViews({ TEXT("A"), TEXT("B") }, { A, B }, EncodingSize, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyEncodingObservation(const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize, const FName Name)
{
	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation EncodingSize '%i'."), *GetName(), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (!ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return FLearningAgentsObservationSchemaElement();
	}

	return { ObservationSchema.CreateEncoding({ Element.SchemaElement, EncodingSize }, Name) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyBoolObservation(const FName Name)
{
	return SpecifyContinuousObservation(1, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyFloatObservation(const FName Name)
{
	return SpecifyContinuousObservation(1, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyLocationObservation(const FName Name)
{
	return SpecifyContinuousObservation(3, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyRotationObservation(const FName Name)
{
	return SpecifyContinuousObservation(6, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyScaleObservation(const FName Name)
{
	return SpecifyContinuousObservation(3, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyTransformObservation(const FName Name)
{
	return SpecifyStructObservationFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			SpecifyLocationObservation(),
			SpecifyRotationObservation(),
			SpecifyScaleObservation()
		}, 
		Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyAngleObservation(const FName Name)
{
	return SpecifyContinuousObservation(2, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyVelocityObservation(const FName Name)
{
	return SpecifyContinuousObservation(3, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyDirectionObservation(const FName Name)
{
	return SpecifyContinuousObservation(3, Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyLocationAlongSplineObservation(const FName Name)
{
	return SpecifyLocationObservation(Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyProportionAlongSplineObservation(const FName Name)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(
		{
			TEXT("Angle"),
			TEXT("Proportion")
		},
		{
			SpecifyAngleObservation(),
			SpecifyFloatObservation()
		},
		8,
		Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyDirectionAlongSplineObservation(const FName Name)
{
	return SpecifyDirectionObservation(Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyPropertiesAlongSplineObservation(const FName Name)
{
	return SpecifyStructObservationFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Proportion"),
			TEXT("Direction")
		},
		{
			SpecifyLocationAlongSplineObservation(),
			SpecifyProportionAlongSplineObservation(),
			SpecifyDirectionAlongSplineObservation(),
		},
		Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyProportionAlongRayObservation(const FName Name)
{
	return SpecifyFloatObservation(Name);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservationSchema::SpecifyProportionAlongRaysObservation(const int32 Num, const FName Name)
{
	return SpecifyStaticArrayObservation(SpecifyProportionAlongRayObservation(), Num, Name);
}

const UE::Learning::Observation::FObject& ULearningAgentsObservationObject::GetObservationObject() const
{
	return ObservationObject;
}

UE::Learning::Observation::FObject& ULearningAgentsObservationObject::GetObservationObject()
{
	return ObservationObject;
}

void ULearningAgentsObservationObject::LogObservation(const FLearningAgentsObservationObjectElement Element)
{
	UE::Learning::Agents::Observation::Private::LogObservation(GetObservationObject(), Element.ObjectElement, TEXT(""), TEXT(""), GetName());
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeNullObservation(const FName Name)
{
	return { ObservationObject.CreateNull(Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeContinuousObservation(const TArray<float>& Values, const FName Name)
{
	return MakeContinuousObservationFromArrayView(Values, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeContinuousObservationFromArrayView(const TArrayView<const float> Values, const FName Name)
{
	if (Values.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Continuous Observation."), *GetName());
	}

	return { ObservationObject.CreateContinuous({ Values }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeExclusiveDiscreteObservation(const int32 DiscreteIndex, const int32 Size, const FName Name)
{
	if (DiscreteIndex < 0 || DiscreteIndex >= Size)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Discrete index out of range: Got %i, expected <= %i."), *GetName(), DiscreteIndex, Size);
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> Values;
	Values.Init(0.0f, Size);
	Values[DiscreteIndex] = 1.0f;
	return MakeContinuousObservationFromArrayView(Values, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeInclusiveDiscreteObservation(const TArray<int32>& DiscreteIndices, const int32 Size, const FName Name)
{
	return MakeInclusiveDiscreteObservationFromArrayView(DiscreteIndices, Size, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeInclusiveDiscreteObservationFromArrayView(const TArrayView<const int32> DiscreteIndices, const int32 Size, const FName Name)
{
	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(DiscreteIndices))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Indices contain duplicates."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> Values;
	Values.Init(0.0f, Size);

	for (int32 Idx = 0; Idx < DiscreteIndices.Num(); Idx++)
	{
		if (DiscreteIndices[Idx] < 0 || DiscreteIndices[Idx] >= Size)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Discrete index out of range: Got %i, expected <= %i."), *GetName(), DiscreteIndices[Idx], Size);
			return FLearningAgentsObservationObjectElement();
		}

		Values[DiscreteIndices[Idx]] = 1.0f;
	}

	return MakeContinuousObservationFromArrayView(Values, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeIndexObservation(const int32 Index, const int32 Size, const FName Name)
{
	if (Index < 0 || Index >= Size)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Discrete index out of range: Got %i, expected <= %i."), *GetName(), Index, Size);
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> Values;
	Values.Init(0.0f, Size);

	for (int32 Idx = 0; Idx < Index; Idx++)
	{
		Values[Idx] = 1.0f;
	}

	return MakeContinuousObservationFromArrayView(Values, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeStructObservation(const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Struct Observation."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsObservationObjectElement>& Element : Elements)
	{
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
	}

	return MakeStructObservationFromArrayViews(SubElementNames, SubElements);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeStructObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	return MakeStructObservationFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeStructObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Struct Observation."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationObjectElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ObservationObject.CreateAnd({ ElementNames, SubElements }, Name)};
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeExclusiveUnionObservation(const FName ElementName, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	return { ObservationObject.CreateOrExclusive({ ElementName, Element.ObjectElement }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeInclusiveUnionObservation(const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsObservationObjectElement>& Element : Elements)
	{
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
	}

	return MakeInclusiveUnionObservationFromArrayViews(SubElementNames, SubElements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeInclusiveUnionObservationFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	return MakeInclusiveUnionObservationFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeInclusiveUnionObservationFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name)
{
	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationObjectElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ObservationObject.CreateOrInclusive({ ElementNames, SubElements }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeStaticArrayObservation(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	return MakeStaticArrayObservationFromArrayView(Elements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeStaticArrayObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Static Array Observation."), *GetName());
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ObservationObject.CreateArray({ SubElements }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeSetObservation(const TSet<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ObservationObject.CreateSet({ SubElements }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeSetObservationFromArray(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	return MakeSetObservationFromArrayView(Elements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeSetObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name)
{
	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ObservationObject.CreateSet({ SubElements }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakePairObservation(const FLearningAgentsObservationObjectElement Key, const FLearningAgentsObservationObjectElement Value, const FName Name)
{
	return MakeStructObservationFromArrayViews({ TEXT("Key"), TEXT("Value") }, { Key, Value }, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeArrayObservation(const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Name)
{
	return MakeArrayObservationFromArrayView(Elements, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeArrayObservationFromArrayView(const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Name)
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (int32 ElementIdx = 0; ElementIdx < Elements.Num(); ElementIdx++)
	{
		const FLearningAgentsObservationObjectElement Element = Elements[ElementIdx];

		if (!ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(MakeIndexObservation(ElementIdx, Elements.Num()), Element));
	}

	return MakeSetObservationFromArrayView(SubElements);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeMapObservation(const TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& Map, const FName Name)
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Map.Num());

	for (TPair<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement> Item : Map)
	{
		if (!ObservationObject.IsValid(Item.Key.ObjectElement) || !ObservationObject.IsValid(Item.Value.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(Item.Key, Item.Value));
	}

	return MakeSetObservationFromArrayView(SubElements);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeMapObservationFromArrays(const TArray<FLearningAgentsObservationObjectElement>& Keys, const TArray<FLearningAgentsObservationObjectElement>& Values, const FName Name)
{
	return MakeMapObservationFromArrayViews(Keys, Values, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeMapObservationFromArrayViews(const TArrayView<const FLearningAgentsObservationObjectElement> Keys, const TArrayView<const FLearningAgentsObservationObjectElement> Values, const FName Name)
{
	if (Keys.Num() != Values.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of keys (%i) must match number of values (%i)."), *GetName(), Keys.Num(), Values.Num());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Keys.Num());

	for (int32 ElementIdx = 0; ElementIdx < Keys.Num(); ElementIdx++)
	{
		if(!ObservationObject.IsValid(Keys[ElementIdx].ObjectElement) || !ObservationObject.IsValid(Values[ElementIdx].ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(Keys[ElementIdx], Values[ElementIdx]));
	}

	return MakeSetObservationFromArrayView(SubElements);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeEnumObservation(const UEnum* Enum, const uint8 EnumValue, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	const int32 EnumValueIndex = Enum->GetIndexByValue(EnumValue);

	if (EnumValueIndex == INDEX_NONE || EnumValueIndex < 0 || EnumValueIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: EnumValue %i not valid for Enum '%s'."), *GetName(), EnumValue , *Enum->GetName());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, Enum->NumEnums() - 1);
	OneHot[EnumValueIndex] = 1.0f;

	return MakeContinuousObservationFromArrayView(OneHot, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeBitmaskObservation(const UEnum* Enum, const int32 BitmaskValue, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, Enum->NumEnums() - 1);

	for (int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; EnumIdx++)
	{
		if (BitmaskValue & (1 << EnumIdx))
		{
			OneHot[EnumIdx] = 1.0f;
		}
	}

	return MakeContinuousObservationFromArrayView(OneHot, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeOptionalObservation(const FLearningAgentsObservationObjectElement Element, const ELearningAgentsOptionalObservation Option, const FName Name)
{
	return MakeExclusiveUnionObservation(
		Option == ELearningAgentsOptionalObservation::Null ? TEXT("Null") : TEXT("Valid"),
		Option == ELearningAgentsOptionalObservation::Null ? MakeNullObservation() : Element,
		Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeOptionalNullObservation(const FName Name)
{
	return MakeExclusiveUnionObservation(TEXT("Null"), MakeNullObservation(), Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeOptionalValidObservation(const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	return MakeExclusiveUnionObservation(TEXT("Valid"), Element, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeEitherObservation(const FLearningAgentsObservationObjectElement Element, const ELearningAgentsEitherObservation Either, const FName Name)
{
	return MakeExclusiveUnionObservation(Either == ELearningAgentsEitherObservation::A ? TEXT("A") : TEXT("B"), Element, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeEitherAObservation(const FLearningAgentsObservationObjectElement A, const FName Name)
{
	return MakeExclusiveUnionObservation(TEXT("A"), A, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeEitherBObservation(const FLearningAgentsObservationObjectElement B, const FName Name)
{
	return MakeExclusiveUnionObservation(TEXT("B"), B, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeEncodingObservation(const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return FLearningAgentsObservationObjectElement();
	}

	return { ObservationObject.CreateEncoding({ Element.ObjectElement }, Name) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeBoolObservation(const bool bValue, const FName Name)
{
	return MakeContinuousObservationFromArrayView({ bValue ? 1.0f : -1.0f }, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeFloatObservation(const float Value, const float FloatScale, const FName Name)
{
	return MakeContinuousObservationFromArrayView({ Value / FMath::Max(FloatScale, UE_SMALL_NUMBER) }, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeLocationObservation(const FVector Location, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	const FVector LocalLocation = RelativeTransform.InverseTransformPosition(Location);

	return MakeContinuousObservationFromArrayView({
		(float)LocalLocation.X / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		(float)LocalLocation.Y / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		(float)LocalLocation.Z / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeRotationObservation(const FRotator Rotation, const FRotator RelativeRotation, const FName Name)
{
	return MakeRotationObservationFromQuat(FQuat::MakeFromRotator(Rotation), FQuat::MakeFromRotator(RelativeRotation), Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeRotationObservationFromQuat(const FQuat Rotation, const FQuat RelativeRotation, const FName Name)
{
	const FQuat LocalRotation = RelativeRotation.Inverse() * Rotation;
	const FVector LocalAxisForward = LocalRotation.GetForwardVector();
	const FVector LocalAxisRight = LocalRotation.GetRightVector();

	return MakeContinuousObservationFromArrayView({
		(float)LocalAxisForward.X,
		(float)LocalAxisForward.Y,
		(float)LocalAxisForward.Z,
		(float)LocalAxisRight.X,
		(float)LocalAxisRight.Y,
		(float)LocalAxisRight.Z,
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeScaleObservation(const FVector Scale, const FVector RelativeScale, const FName Name)
{
	const FVector LocalLogScale = 
		UE::Learning::Agents::Observation::Private::VectorLogSafe(Scale) - 
		UE::Learning::Agents::Observation::Private::VectorLogSafe(RelativeScale);

	return MakeContinuousObservationFromArrayView({
		(float)LocalLogScale.X,
		(float)LocalLogScale.Y,
		(float)LocalLogScale.Z,
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeTransformObservation(const FTransform Transform, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	const FTransform LocalTransform = Transform * RelativeTransform.Inverse();
	
	return MakeStructObservationFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			MakeLocationObservation(LocalTransform.GetLocation(), FTransform::Identity, LocationScale),
			MakeRotationObservationFromQuat(LocalTransform.GetRotation(), FQuat::Identity),
			MakeScaleObservation(LocalTransform.GetScale3D(), FVector::OneVector)
		},
		Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeAngleObservation(const float Angle, const float RelativeAngle, const FName Name)
{
	return MakeAngleObservationRadians(FMath::DegreesToRadians(Angle), FMath::DegreesToRadians(RelativeAngle), Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeAngleObservationRadians(const float Angle, const float RelativeAngle, const FName Name)
{
	const float LocalAngle = FMath::FindDeltaAngleRadians(RelativeAngle, Angle);

	return MakeContinuousObservationFromArrayView({
		(float)FMath::Sin(Angle),
		(float)FMath::Cos(Angle),
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeVelocityObservation(const FVector Velocity, const FTransform RelativeTransform, const float VelocityScale, const FName Name)
{
	const FVector LocalVelocity = RelativeTransform.InverseTransformVectorNoScale(Velocity);

	return MakeContinuousObservationFromArrayView({
		(float)LocalVelocity.X / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		(float)LocalVelocity.Y / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		(float)LocalVelocity.Z / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeDirectionObservation(const FVector Direction, const FTransform RelativeTransform, const FName Name)
{
	const FVector LocalDirection = RelativeTransform.InverseTransformVectorNoScale(Direction).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	return MakeContinuousObservationFromArrayView({
		(float)LocalDirection.X,
		(float)LocalDirection.Y,
		(float)LocalDirection.Z,
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeLocationAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeLocationAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	return MakeLocationObservation(SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World), RelativeTransform, LocationScale, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeProportionAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FName Name)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeProportionAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (SplineComponent->IsClosedLoop())
	{
		const float TotalDistance = SplineComponent->GetSplineLength();
		const float WrapDistance = FMath::Wrap(DistanceAlongSpline, 0.0f, TotalDistance);
		const float Proportion = WrapDistance / FMath::Max(TotalDistance, UE_SMALL_NUMBER);
		const float Angle = FMath::Wrap(UE_TWO_PI * Proportion, -UE_PI, UE_PI);
		return MakeExclusiveUnionObservation(TEXT("Angle"), MakeAngleObservation(Angle), Name);
	}
	else
	{
		const float Proportion = FMath::Clamp(DistanceAlongSpline / FMath::Max(SplineComponent->GetSplineLength(), UE_SMALL_NUMBER), 0.0f, 1.0f);
		return MakeExclusiveUnionObservation(TEXT("Proportion"), MakeFloatObservation(Proportion), Name);
	}
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeDirectionAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FTransform RelativeTransform, const FName Name)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeDirectionAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	return MakeDirectionObservation(SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World), RelativeTransform, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakePropertiesAlongSplineObservation(const USplineComponent* SplineComponent, const float DistanceAlongSpline, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakePropertiesAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	return MakeStructObservationFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Proportion"),
			TEXT("Direction")
		},
		{
			MakeLocationAlongSplineObservation(SplineComponent, DistanceAlongSpline, RelativeTransform, LocationScale),
			MakeProportionAlongSplineObservation(SplineComponent, DistanceAlongSpline),
			MakeDirectionAlongSplineObservation(SplineComponent, DistanceAlongSpline, RelativeTransform),
		}, Name);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeProportionAlongRayObservation(const FVector RayStart, const FVector RayEnd, const FTransform RayTransform, const ECollisionChannel CollisionChannel, const FName Name)
{
	const FVector RayStartWorld = RayTransform.TransformPosition(RayStart);
	const FVector RayEndWorld = RayTransform.TransformPosition(RayEnd);
	const float RayDistance = FVector::Distance(RayStartWorld, RayEndWorld);


	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);

	FHitResult TraceHit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(TraceHit, RayStartWorld, RayEndWorld, ObjectQueryParams);

	if (bHit)
	{
		//DrawDebugLine(GetWorld(), RayStartWorld, RayStartWorld + TraceHit.Time * (RayEndWorld - RayStartWorld), FColor::Red, false, -1.0f, 0, 5.0f);
		//DrawDebugLine(GetWorld(), RayStartWorld + TraceHit.Time * (RayEndWorld - RayStartWorld), RayEndWorld, FColor::Blue, false, -1.0f, 0, 5.0f);
		return MakeFloatObservation(1.0f - TraceHit.Time, 1.0f, Name);
	}
	else
	{
		//DrawDebugLine(GetWorld(), RayStartWorld, RayEndWorld, FColor::Red, false, -1.0f, 0, 5.0f);
		return MakeFloatObservation(0.0f, 1.0f, Name);
	}
}

FLearningAgentsObservationObjectElement ULearningAgentsObservationObject::MakeProportionAlongRaysObservation(const TArray<FVector>& RayStarts, const TArray<FVector>& RayEnds, const FTransform RayTransform, const ECollisionChannel CollisionChannel, const FName Name)
{
	if (RayStarts.Num() != RayEnds.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeProportionAlongRaysObservation: Different number of RayStarts and RayEnds (%i vs %i)."), RayStarts.Num(), RayEnds.Num());
		return FLearningAgentsObservationObjectElement();
	}

	const int32 RayNum = RayStarts.Num();

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<32>> RayElements;
	RayElements.Reserve(RayNum);

	for (int32 RayIdx = 0; RayIdx < RayNum; RayIdx++)
	{
		RayElements.Add(MakeProportionAlongRayObservation(RayStarts[RayIdx], RayEnds[RayIdx], RayTransform, CollisionChannel));
	}

	return MakeStaticArrayObservationFromArrayView(RayElements, Name);
}

bool ULearningAgentsObservationObject::GetNullObservation(const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Null)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Null));
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetContinuousObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Continuous));
		OutNum = 0;
		return false;
	}

	OutNum = ObservationObject.GetContinuous(Element.ObjectElement).Values.Num();
	return true;
}

bool ULearningAgentsObservationObject::GetContinuousObservation(TArray<float>& OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutValueNum = 0;
	if (!GetContinuousObservationNum(OutValueNum, Element, Name))
	{
		OutValues.Empty();
		return false;
	}

	OutValues.SetNumUninitialized(OutValueNum);

	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetContinuousObservationToArrayView(TArrayView<float> OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Continuous));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	const TArrayView<const float> Values = ObservationObject.GetContinuous(Element.ObjectElement).Values;

	if (Values.Num() != OutValues.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' values but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			Values.Num(), OutValues.Num());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	UE::Learning::Array::Copy<1, float>(OutValues, Values);
	return true;
}

bool ULearningAgentsObservationObject::GetExclusiveDiscreteObservation(int32& OutIndex, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Element, Name))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	OutIndex = INDEX_NONE;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx])
		{
			OutIndex = Idx;
			break;
		}
	}

	return OutIndex != INDEX_NONE;
}

bool ULearningAgentsObservationObject::GetInclusiveDiscreteObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Element, Name))
	{
		OutNum = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		OutNum = 0;
		return false;
	}

	OutNum = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { OutNum++; }
	}

	return true;
}

bool ULearningAgentsObservationObject::GetInclusiveDiscreteObservation(TArray<int32>& OutIndices, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutIndicesNum;
	if (!GetInclusiveDiscreteObservationNum(OutIndicesNum, Element, Name))
	{
		OutIndices.Empty();
		return false;
	}

	OutIndices.SetNumUninitialized(OutIndicesNum);
	return GetInclusiveDiscreteObservationToArrayView(OutIndices, Element, Name);
}

bool ULearningAgentsObservationObject::GetInclusiveDiscreteObservationToArrayView(TArrayView<int32> OutIndices, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Element, Name))
	{
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		return false;
	}

	int32 IndicesNum = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { IndicesNum++; }
	}

	if (IndicesNum != OutIndices.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' indices but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			IndicesNum, OutIndices.Num());
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	int32 Offset = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { OutIndices[Offset] = Idx; Offset++; }
	}

	return true;
}

bool ULearningAgentsObservationObject::GetIndexObservation(int32& OutIndex, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Element, Name))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	OutIndex = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { OutIndex = Idx; }
	}

	return true;
}

bool ULearningAgentsObservationObject::GetStructObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::And));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Observation::FObjectAndParameters Parameters = ObservationObject.GetAnd(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsObservationObject::GetStructObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStructObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetStructObservationToArrayViews(SubElementNames, SubElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsObservationObject::GetStructObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStructObservationNum(OutElementNum, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetStructObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::And));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectAndParameters Parameters = ObservationObject.GetAnd(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Getting zero-sized And Observation."), *GetName());
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservationObject::GetExclusiveUnionObservation(FName& OutElementName, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutElementName = NAME_None;
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrExclusive));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	const UE::Learning::Observation::FObjectOrExclusiveParameters Parameters = ObservationObject.GetOrExclusive(Element.ObjectElement);
	OutElementName = Parameters.ElementName;
	OutElement = { Parameters.Element };
	return true;
}

bool ULearningAgentsObservationObject::GetInclusiveUnionObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrInclusive));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = ObservationObject.GetOrInclusive(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsObservationObject::GetInclusiveUnionObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionObservationToArrayViews(SubElementNames, SubElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsObservationObject::GetInclusiveUnionObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionObservationNum(OutElementNum, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionObservationToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetInclusiveUnionObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrInclusive));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = ObservationObject.GetOrInclusive(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservationObject::GetStaticArrayObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Array));
		OutNum = 0;
		return false;
	}

	OutNum = ObservationObject.GetArray(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsObservationObject::GetStaticArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStaticArrayObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStaticArrayObservationToArrayView(OutElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetStaticArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Array));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectArrayParameters Parameters = ObservationObject.GetArray(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Getting zero-sized Static Array Observation."), *GetName());
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservationObject::GetSetObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		OutNum = 0;
		return false;
	}

	OutNum = ObservationObject.GetSet(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsObservationObject::GetSetObservation(TSet<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetSetObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutElements.Empty();
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		OutElements.Empty();
		return false;
	}

	const UE::Learning::Observation::FObjectSetParameters Parameters = ObservationObject.GetSet(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(Parameters.Elements.Num());
	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			OutElements.Empty();
			return false;
		}

		OutElements.Add({ Parameters.Elements[ElementIdx] });
	}

	return true;
}

bool ULearningAgentsObservationObject::GetSetObservationToArray(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetSetObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetSetObservationToArrayView(OutElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetSetObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectSetParameters Parameters = ObservationObject.GetSet(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservationObject::GetPairObservation(FLearningAgentsObservationObjectElement& OutKey, FLearningAgentsObservationObjectElement& OutValue, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	TStaticArray<FName, 2> OutElementNames;
	TStaticArray<FLearningAgentsObservationObjectElement, 2> OutElements;
	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutKey = FLearningAgentsObservationObjectElement();
		OutValue = FLearningAgentsObservationObjectElement();
		return false;
	}

	OutKey = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Key"))];
	OutValue = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Value"))];
	return true;
}

bool ULearningAgentsObservationObject::GetArrayObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	return GetSetObservationNum(OutNum, Element, Name);
}

bool ULearningAgentsObservationObject::GetArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetArrayObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetArrayObservationToArrayView(OutElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutElements.Num());
	if (!GetSetObservationToArrayView(Pairs, Element, Name))
	{
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 PairIdx = 0; PairIdx < Pairs.Num(); PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Pairs[PairIdx], Key, Value))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		int32 Index = INDEX_NONE;
		if (!GetIndexObservation(Index, Key))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElements[Index] = Value;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetMapObservationNum(int32& OutNum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	return GetSetObservationNum(OutNum, Element, Name);
}

bool ULearningAgentsObservationObject::GetMapObservation(TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& OutElements, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetMapObservationNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutElementNum);
	if (!GetSetObservationToArrayView(Pairs, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 PairIdx = 0; PairIdx < OutElementNum; PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Pairs[PairIdx], Key, Value))
		{
			OutElements.Empty();
			return false;
		}

		OutElements.Add(Key, Value);
	}

	return true;
}

bool ULearningAgentsObservationObject::GetMapObservationToArrays(TArray<FLearningAgentsObservationObjectElement>& OutKeys, TArray<FLearningAgentsObservationObjectElement>& OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetMapObservationNum(OutElementNum, Element, Name))
	{
		OutKeys.Empty();
		OutValues.Empty();
		return false;
	}

	OutKeys.SetNumUninitialized(OutElementNum);
	OutValues.SetNumUninitialized(OutElementNum);
	if (!GetMapObservationToArrayViews(OutKeys, OutValues, Element, Name))
	{
		OutKeys.Empty();
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetMapObservationToArrayViews(TArrayView<FLearningAgentsObservationObjectElement> OutKeys, TArrayView<FLearningAgentsObservationObjectElement> OutValues, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutKeys.Num());
	if (!GetSetObservationToArrayView(Pairs, Element, Name))
	{
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutKeys, FLearningAgentsObservationObjectElement());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutValues, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 PairIdx = 0; PairIdx < Pairs.Num(); PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Pairs[PairIdx], Key, Value))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutKeys, FLearningAgentsObservationObjectElement());
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutValues, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutKeys[PairIdx] = Key;
		OutValues[PairIdx] = Value;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetEnumObservation(uint8& OutEnumValue, const UEnum* Enum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		OutEnumValue = 0;
		return false;
	}

	int32 EnumValueNum;
	if (!GetContinuousObservationNum(EnumValueNum, Element, Name))
	{
		OutEnumValue = 0;
		return false;
	}
	
	if (EnumValueNum != Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values for Enum '%s'. Expected %i, got %i."), *GetName(), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutEnumValue = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(EnumValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		OutEnumValue = 0;
		return false;
	}

	int32 EnumValueIndex = INDEX_NONE;
	for (int32 EnumIdx = 0; EnumIdx < EnumValueNum; EnumIdx++)
	{
		if (OneHot[EnumIdx])
		{
			EnumValueIndex = EnumIdx;
			break;
		}
	}

	if (EnumValueIndex == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Index not found."), *GetName());
		OutEnumValue = 0;
		return false;
	}

	const int32 EnumValue = Enum->GetValueByIndex(EnumValueIndex);

	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum Value not found for index %i."), *GetName(), EnumValueIndex);
		OutEnumValue = 0;
		return false;
	}

	OutEnumValue = (uint8)EnumValue;
	return true;
}

bool ULearningAgentsObservationObject::GetBitmaskObservation(int32& OutBitmaskValue, const UEnum* Enum, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		OutBitmaskValue = 0;
		return false;
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		OutBitmaskValue = 0;
		return false;
	}

	int32 EnumValueNum;
	if (!GetContinuousObservationNum(EnumValueNum, Element, Name))
	{
		OutBitmaskValue = 0;
		return false;
	}

	if (EnumValueNum != Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values for Enum '%s'. Expected %i, got %i."), *GetName(), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutBitmaskValue = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, EnumValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Element, Name))
	{
		OutBitmaskValue = 0;
		return false;
	}

	OutBitmaskValue = 0;
	for (int32 OneHotIdx = 0; OneHotIdx < EnumValueNum; OneHotIdx++)
	{
		if (OneHot[OneHotIdx])
		{
			OutBitmaskValue |= (1 << OneHotIdx);
		}
	}
	return true;
}


bool ULearningAgentsObservationObject::GetOptionalObservation(ELearningAgentsOptionalObservation& OutOption, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionObservation(OutName, OutElement, Element, Name))
	{
		OutOption = ELearningAgentsOptionalObservation::Null;
		return false;
	}

	OutOption = OutName == TEXT("Null") ? ELearningAgentsOptionalObservation::Null : ELearningAgentsOptionalObservation::Valid;
	return true;
}

bool ULearningAgentsObservationObject::GetEitherObservation(ELearningAgentsEitherObservation& OutEither, FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionObservation(OutName, OutElement, Element, Name))
	{
		OutEither = ELearningAgentsEitherObservation::A;
		return false;
	}

	OutEither = OutName == TEXT("A") ? ELearningAgentsEitherObservation::A : ELearningAgentsEitherObservation::B;
	return true;
}

bool ULearningAgentsObservationObject::GetEncodingObservation(FLearningAgentsObservationObjectElement& OutElement, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	if (!ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Observation Object."), *GetName());
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (ObservationObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Observation name does not match. Observation is '%s' but asked for '%s'."), *GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Encoding)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*GetName(),
			*ObservationObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Encoding));
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	OutElement = { ObservationObject.GetEncoding(Element.ObjectElement).Element };

	return true;
}

bool ULearningAgentsObservationObject::GetBoolObservation(bool& bOutValue, const FLearningAgentsObservationObjectElement Element, const FName Name) const
{
	float OutValue = 0.0f;
	if (!GetFloatObservation(OutValue, Element, 1.0f, Name))
	{
		bOutValue = false;
		return false;
	}

	bOutValue = OutValue >= 0.0f;
	return true;
}

bool ULearningAgentsObservationObject::GetFloatObservation(float& OutValue, const FLearningAgentsObservationObjectElement Element, const float FloatScale, const FName Name) const
{
	float OutValuesData;
	if (!GetContinuousObservationToArrayView(MakeArrayView(&OutValuesData, 1), Element, Name))
	{
		OutValue = 0.0f;
		return false;
	}

	OutValue = OutValuesData * FloatScale;
	return true;
}

bool ULearningAgentsObservationObject::GetLocationObservation(FVector& OutLocation, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	OutLocation = RelativeTransform.TransformPosition(LocationScale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservationObject::GetRotationObservation(FRotator& OutRotation, const FLearningAgentsObservationObjectElement Element, const FRotator RelativeRotation, const FName Name) const
{
	FQuat OutRotationQuat;
	if (!GetRotationObservationAsQuat(OutRotationQuat, Element, FQuat::MakeFromRotator(RelativeRotation), Name))
	{
		OutRotation = FRotator::ZeroRotator;
		return false;
	}

	OutRotation = OutRotationQuat.Rotator();
	return true;
}

bool ULearningAgentsObservationObject::GetRotationObservationAsQuat(FQuat& OutRotation, const FLearningAgentsObservationObjectElement Element, const FQuat RelativeRotation, const FName Name) const
{
	TStaticArray<float, 6> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutRotation = FQuat::Identity;
		return false;
	}

	const FVector LocalAxisForward = FVector(OutValues[0], OutValues[1], OutValues[2]);
	const FVector LocalAxisRight = FVector(OutValues[3], OutValues[4], OutValues[5]);
	const FVector AxisUp = LocalAxisForward.Cross(LocalAxisRight).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector AxisRight = AxisUp.Cross(LocalAxisForward).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	const FVector AxisForward = LocalAxisForward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.SetAxis(0, AxisForward);
	RotationMatrix.SetAxis(1, AxisRight);
	RotationMatrix.SetAxis(2, AxisUp);

	OutRotation = RelativeRotation * RotationMatrix.ToQuat();
	return true;
}

bool ULearningAgentsObservationObject::GetScaleObservation(FVector& OutScale, const FLearningAgentsObservationObjectElement Element, const FVector RelativeScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutScale = FVector::OneVector;
		return false;
	}

	OutScale = RelativeScale * UE::Learning::Agents::Observation::Private::VectorExp(FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservationObject::GetTransformObservation(FTransform& OutTransform, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Name) const
{
	TStaticArray<FName, 3> OutElementNames;
	TStaticArray<FLearningAgentsObservationObjectElement, 3> OutElements;
	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 LocationElement = MakeArrayView(OutElementNames).Find(TEXT("Location"));
	FVector OutLocation;
	if (LocationElement == INDEX_NONE || !GetLocationObservation(OutLocation, OutElements[LocationElement], RelativeTransform, LocationScale))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 RotationElement = MakeArrayView(OutElementNames).Find(TEXT("Rotation"));
	FQuat OutRotation;
	if (RotationElement == INDEX_NONE || !GetRotationObservationAsQuat(OutRotation, OutElements[RotationElement], RelativeTransform.GetRotation()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 ScaleElement = MakeArrayView(OutElementNames).Find(TEXT("Scale"));
	FVector OutScale;
	if (ScaleElement == INDEX_NONE || !GetScaleObservation(OutScale, OutElements[ScaleElement], RelativeTransform.GetScale3D()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	OutTransform = FTransform(OutRotation, OutLocation, OutScale);
	return true;
}

bool ULearningAgentsObservationObject::GetAngleObservationRadians(float& OutAngle, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle, const FName Name) const
{
	TStaticArray<float, 2> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutAngle = 0.0f;
		return false;
	}

	OutAngle = RelativeAngle + FMath::Atan2(OutValues[0], OutValues[1]);
	return true;
}


bool ULearningAgentsObservationObject::GetAngleObservation(float& OutAngle, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle, const FName Name) const
{
	if (!GetAngleObservationRadians(OutAngle, Element, FMath::DegreesToRadians(RelativeAngle), Name))
	{
		return false;
	}

	OutAngle = FMath::RadiansToDegrees(OutAngle);
	return true;
}

bool ULearningAgentsObservationObject::GetVelocityObservation(FVector& OutVelocity, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float VelocityScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutVelocity = FVector::ZeroVector;
		return false;
	}

	OutVelocity = RelativeTransform.TransformVectorNoScale(VelocityScale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservationObject::GetDirectionObservation(FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Element, Name))
	{
		OutDirection = FVector::ForwardVector;
		return false;
	}

	OutDirection = RelativeTransform.TransformVectorNoScale(FVector(OutValues[0], OutValues[1], OutValues[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	return true;
}

bool ULearningAgentsObservationObject::GetLocationAlongSplineObservation(FVector& OutLocation, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	return GetLocationObservation(OutLocation, Element, RelativeTransform, LocationScale, Name);
}

bool ULearningAgentsObservationObject::GetProportionAlongSplineObservation(bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	FName SubName;
	FLearningAgentsObservationObjectElement SubElement;
	if (!GetExclusiveUnionObservation(SubName, SubElement, Element, Name))
	{
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0;
		return false;
	}

	if (SubName == TEXT("Angle"))
	{
		bOutIsClosedLoop = true;
		OutPropotion = 0.0f;
		return GetAngleObservation(OutAngle, SubElement);
	}
	else
	{
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		return GetFloatObservation(OutPropotion, SubElement);
	}
}

bool ULearningAgentsObservationObject::GetDirectionAlongSplineObservation(FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const FName Name)
{
	return GetDirectionObservation(OutDirection, Element, RelativeTransform, Name);
}

bool ULearningAgentsObservationObject::GetPropertiesAlongSplineObservation(FVector& OutLocation, bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, FVector& OutDirection, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	TStaticArray<FName, 3> OutElementNames;
	TStaticArray<FLearningAgentsObservationObjectElement, 3> OutElements;
	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutLocation = FVector::ZeroVector;
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0f;
		OutDirection = FVector::ForwardVector;
		return false;
	}

	const int32 LocationElement = MakeArrayView(OutElementNames).Find(TEXT("Location"));
	if (LocationElement == INDEX_NONE || !GetLocationAlongSplineObservation(OutLocation, OutElements[LocationElement], RelativeTransform, LocationScale))
	{
		OutLocation = FVector::ZeroVector;
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0f;
		OutDirection = FVector::ForwardVector;
		return false;
	}

	const int32 ProportionElement = MakeArrayView(OutElementNames).Find(TEXT("Proportion"));
	if (ProportionElement == INDEX_NONE || !GetProportionAlongSplineObservation(bOutIsClosedLoop, OutAngle, OutPropotion, OutElements[ProportionElement]))
	{
		OutLocation = FVector::ZeroVector;
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0f;
		OutDirection = FVector::ForwardVector;
		return false;
	}

	const int32 DirectionElement = MakeArrayView(OutElementNames).Find(TEXT("Direction"));
	if (DirectionElement == INDEX_NONE || !GetDirectionAlongSplineObservation(OutDirection, OutElements[ProportionElement], RelativeTransform))
	{
		OutLocation = FVector::ZeroVector;
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0f;
		OutDirection = FVector::ForwardVector;
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetProportionAlongRayObservation(float& OutProportion, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	if (!GetFloatObservation(OutProportion, Element, 1.0f, Name))
	{
		OutProportion = 0.0f;
		return false;
	}

	OutProportion = 1.0f - OutProportion;
	return true;
}

bool ULearningAgentsObservationObject::GetProportionAlongRaysObservationNum(int32& OutProportionNum, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	return GetStaticArrayObservationNum(OutProportionNum, Element, Name);
}

bool ULearningAgentsObservationObject::GetProportionAlongRaysObservation(TArray<float>& OutProportions, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	int32 ProportionNum;
	if (!GetProportionAlongRaysObservationNum(ProportionNum, Element, Name))
	{
		OutProportions.Empty();
		return false;
	}

	OutProportions.SetNumUninitialized(ProportionNum);
	if (!GetProportionAlongRaysObservationToArrayView(OutProportions, Element, Name))
	{
		OutProportions.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservationObject::GetProportionAlongRaysObservationToArrayView(TArrayView<float> OutProportions, const FLearningAgentsObservationObjectElement Element, const FName Name)
{
	int32 ProportionNum;
	if (!GetStaticArrayObservationNum(ProportionNum, Element, Name))
	{
		UE::Learning::Array::Zero<1, float>(OutProportions);
		return false;
	}

	if (ProportionNum != OutProportions.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*GetName(), *ObservationObject.GetName(Element.ObjectElement).ToString(),
			ProportionNum, OutProportions.Num());
		UE::Learning::Array::Zero<1, float>(OutProportions);
		return false;
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<32>> SubElements;
	SubElements.SetNumUninitialized(ProportionNum);
	if (!GetStaticArrayObservationToArrayView(SubElements, Element, Name))
	{
		UE::Learning::Array::Zero<1, float>(OutProportions);
		return false;
	}

	for (int32 SubElementIdx = 0; SubElementIdx < ProportionNum; SubElementIdx++)
	{
		if (!GetProportionAlongRayObservation(OutProportions[SubElementIdx], SubElements[SubElementIdx]))
		{
			UE::Learning::Array::Zero<1, float>(OutProportions);
			return false;
		}
	}

	return true;
}
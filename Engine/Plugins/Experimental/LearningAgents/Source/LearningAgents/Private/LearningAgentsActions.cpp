// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsActions.h"

#include "LearningArray.h"
#include "LearningLog.h"

#include "Containers/StaticArray.h"

bool operator==(const FLearningAgentsActionObjectElement& Lhs, const FLearningAgentsActionObjectElement& Rhs)
{
	return Lhs.ObjectElement.Index == Rhs.ObjectElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsActionObjectElement& Element)
{
	return (uint32)Element.ObjectElement.Index;
}

const UE::Learning::Action::FSchema& ULearningAgentsActionSchema::GetActionSchema() const
{
	return ActionSchema;
}

namespace UE::Learning::Agents::Action::Private
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

	static inline const TCHAR* GetActionTypeString(const Learning::Action::EType ActionType)
	{
		switch (ActionType)
		{
		case Learning::Action::EType::Null: return TEXT("Null");
		case Learning::Action::EType::Continuous: return TEXT("Continuous");
		case Learning::Action::EType::DiscreteExclusive: return TEXT("DiscreteExclusive");
		case Learning::Action::EType::DiscreteInclusive: return TEXT("DiscreteInclusive");
		case Learning::Action::EType::And: return TEXT("Struct");
		case Learning::Action::EType::OrExclusive: return TEXT("ExclusiveUnion");
		case Learning::Action::EType::OrInclusive: return TEXT("InclusiveUnion");
		case Learning::Action::EType::Array: return TEXT("Array");
		case Learning::Action::EType::Encoding: return TEXT("Encoding");
		default:
			UE_LEARNING_NOT_IMPLEMENTED();
			return TEXT("Unimplemented");
		}
	}

	static bool ValidateObjectMatchesSchema(
		const Learning::Action::FSchema& Schema,
		const Learning::Action::FSchemaElement SchemaElement,
		const Learning::Action::FObject& Object,
		const Learning::Action::FObjectElement ObjectElement,
		const FString& ObjectName)
	{
		// Check Elements are Valid

		if (!Schema.IsValid(SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Schema Element."), *ObjectName);
			return false;
		}

		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object Element."), *ObjectName);
			return false;
		}

		// Check Names Match

		const FName ActionSchemaElementName = Schema.GetName(SchemaElement);
		const FName ActionObjectElementName = Object.GetName(ObjectElement);

		if (ActionSchemaElementName != ActionObjectElementName)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match Schema. Expected '%s', got '%s'."),
				*ObjectName, *ActionSchemaElementName.ToString(), *ActionObjectElementName.ToString());
		}

		// Check Types Match

		const Learning::Action::EType ActionSchemaElementType = Schema.GetType(SchemaElement);
		const Learning::Action::EType ActionObjectElementType = Object.GetType(ObjectElement);

		if (ActionSchemaElementType != ActionObjectElementType)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match Schema. Expected type '%s', got type '%s'."),
				*ObjectName,
				*ActionSchemaElementName.ToString(),
				GetActionTypeString(ActionSchemaElementType),
				GetActionTypeString(ActionObjectElementType));
			return false;
		}

		// Type Specific Checks

		switch (ActionSchemaElementType)
		{
		case Learning::Action::EType::Null: return true;

		case Learning::Action::EType::Continuous:
		{
			const int32 SchemaElementSize = Schema.GetContinuous(SchemaElement).Num;
			const int32 ObjectElementSize = Object.GetContinuous(ObjectElement).Values.Num();

			if (SchemaElementSize != ObjectElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
					SchemaElementSize,
					ObjectElementSize);
				return false;
			}

			return true;
		}

		case Learning::Action::EType::DiscreteExclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteExclusive(SchemaElement).Num;
			const int32 ObjectElementIndex = Object.GetDiscreteExclusive(ObjectElement).DiscreteIndex;

			if (ObjectElementIndex < 0 || ObjectElementIndex >= SchemaElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' index out of range for Schema. Expected '<%i', got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
					SchemaElementSize,
					ObjectElementIndex);
				return false;
			}

			return true;
		}

		case Learning::Action::EType::DiscreteInclusive:
		{
			const int32 SchemaElementSize = Schema.GetDiscreteInclusive(SchemaElement).Num;
			const TArrayView<const int32> ObjectElementIndices = Object.GetDiscreteInclusive(ObjectElement).DiscreteIndices;

			if (ObjectElementIndices.Num() > SchemaElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' too many indices provided. Expected at most '%i', got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
					SchemaElementSize,
					ObjectElementIndices.Num());
				return false;
			}

			for (int32 SubElementIdx = 0; SubElementIdx < ObjectElementIndices.Num(); SubElementIdx++)
			{
				if (ObjectElementIndices[SubElementIdx] < 0 || ObjectElementIndices[SubElementIdx] >= SchemaElementSize)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' index out of range for Schema. Expected '<%i', got '%i'."),
						*ObjectName,
						*ActionSchemaElementName.ToString(),
						SchemaElementSize,
						ObjectElementIndices[SubElementIdx]);
					return false;
				}
			}

			return true;
		}

		case Learning::Action::EType::And:
		{
			const Learning::Action::FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const Learning::Action::FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());
			UE_LEARNING_CHECK(ObjectParameters.Elements.Num() == ObjectParameters.ElementNames.Num());

			if (SchemaParameters.Elements.Num() != ObjectParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' number of sub-elements does not match Schema. Expected '%i', got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' does not include '%s' action required by Schema."),
						*ObjectName,
						*ActionSchemaElementName.ToString(),
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

		case Learning::Action::EType::OrExclusive:
		{
			const Learning::Action::FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const Learning::Action::FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());

			const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);

			if (SchemaSubElementIdx == INDEX_NONE)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' Schema does not include '%s' action."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
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

		case Learning::Action::EType::OrInclusive:
		{
			const Learning::Action::FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const Learning::Action::FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' too many sub-actions provided. Expected at most '%i', got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ObjectSubElementIdx = 0; ObjectSubElementIdx < ObjectParameters.Elements.Num(); ObjectSubElementIdx++)
			{
				const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectSubElementIdx]);

				if (SchemaSubElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' Schema does not include '%s' action."),
						*ObjectName,
						*ActionSchemaElementName.ToString(),
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

		case Learning::Action::EType::Array:
		{
			const Learning::Action::FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const Learning::Action::FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);

			if (ObjectParameters.Elements.Num() != SchemaParameters.Num)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' array incorrect size. Expected '%i' elements, got '%i'."),
					*ObjectName,
					*ActionSchemaElementName.ToString(),
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

		case Learning::Action::EType::Encoding:
		{
			const Learning::Action::FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const Learning::Action::FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

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

	static void LogAction(
		const UE::Learning::Action::FObject& Object,
		const UE::Learning::Action::FObjectElement ObjectElement,
		const FString& Indentation,
		const FString& Prefix,
		const FString& ObjectName)
	{
		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object Element."), *ObjectName);
			return;
		}

		const UE::Learning::Action::EType Type = Object.GetType(ObjectElement);
		const FName Name = Object.GetName(ObjectElement);

		switch (Type)
		{
		case UE::Learning::Action::EType::Null:
		{
			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			return;
		}

		case UE::Learning::Action::EType::Continuous:
		{
			const UE::Learning::Action::FObjectContinuousParameters Parameters = Object.GetContinuous(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type), *UE::Learning::Array::FormatFloat(Parameters.Values));
			return;
		}

		case UE::Learning::Action::EType::DiscreteExclusive:
		{
			const UE::Learning::Action::FObjectDiscreteExclusiveParameters Parameters = Object.GetDiscreteExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %i"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type), Parameters.DiscreteIndex);
			return;
		}

		case UE::Learning::Action::EType::DiscreteInclusive:
		{
			const UE::Learning::Action::FObjectDiscreteInclusiveParameters Parameters = Object.GetDiscreteInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type), *UE::Learning::Array::FormatInt32(Parameters.DiscreteIndices));
			return;
		}

		case UE::Learning::Action::EType::And:
		{
			const UE::Learning::Action::FObjectAndParameters Parameters = Object.GetAnd(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()), ObjectName);
			}

			return;
		}

		case UE::Learning::Action::EType::OrExclusive:
		{
			const UE::Learning::Action::FObjectOrExclusiveParameters Parameters = Object.GetOrExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			LogAction(Object, Parameters.Element, *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementName.ToString()), ObjectName);

			return;
		}

		case UE::Learning::Action::EType::OrInclusive:
		{
			const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = Object.GetOrInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()), ObjectName);
			}

			return;
		}

		case UE::Learning::Action::EType::Array:
		{
			const UE::Learning::Action::FObjectArrayParameters Parameters = Object.GetArray(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogAction(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx), ObjectName);
			}

			return;
		}

		case UE::Learning::Action::EType::Encoding:
		{
			const UE::Learning::Action::FObjectEncodingParameters Parameters = Object.GetEncoding(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Name.ToString(), GetActionTypeString(Type));
			LogAction(Object, Parameters.Element, *(Indentation + TEXT("    ")), TEXT("|"), ObjectName);

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

	static inline void NormalizeProbabilitiesExclusive(TArrayView<float> PriorProbabilities, const FString& ObjectName)
	{
		float Total = 0.0f;
		for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
		{
			if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Invalid Prior Probability Given (%f), must be in range 0 to 1."), *ObjectName, PriorProbabilities[Idx]);
			}

			PriorProbabilities[Idx] = FMath::Clamp(PriorProbabilities[Idx], 0.0f, 1.0f);
			Total += PriorProbabilities[Idx];
		}

		if (PriorProbabilities.Num() > 0 && FMath::Abs(Total) < UE_SMALL_NUMBER)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Prior Probabilities are too small. Should sum to 1."), *ObjectName);

			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				PriorProbabilities[Idx] = 1.0f / PriorProbabilities.Num();
			}
		}
		else
		{
			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				PriorProbabilities[Idx] /= Total;
			}
		}
	}

	static inline void NormalizeProbabilitiesInclusive(TArrayView<float> PriorProbabilities, const FString& ObjectName)
	{
		for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
		{
			if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Invalid Prior Probability Given (%f), must be in range 0 to 1."), *ObjectName, PriorProbabilities[Idx]);
			}

			PriorProbabilities[Idx] = FMath::Clamp(PriorProbabilities[Idx], 0.0f, 1.0f);
		}
	}
}

bool ULearningAgentsActionSchema::ValidateObjectMatchesSchema(
	const FLearningAgentsActionSchemaElement SchemaElement,
	const ULearningAgentsActionObject* Object,
	const FLearningAgentsActionObjectElement ObjectElement) const
{
	return UE::Learning::Agents::Action::Private::ValidateObjectMatchesSchema(
		ActionSchema,
		SchemaElement.SchemaElement,
		Object->GetActionObject(),
		ObjectElement.ObjectElement,
		GetName());
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyNullAction(const FName Name)
{
	return { ActionSchema.CreateNull(Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyContinuousAction(const int32 Size, const FName Name)
{
	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Continuous Action Size '%i'."), *GetName(), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Continuous Action."), *GetName());
	}

	return { ActionSchema.CreateContinuous({ Size }, Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyExclusiveDiscreteAction(const int32 Size, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyExclusiveDiscreteActionFromArrayView(Size, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyExclusiveDiscreteActionFromArrayView(const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid DiscreteExclusive Action Size '%i'."), *GetName(), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Exclusive Discrete Action."), *GetName());
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Size, Size);
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesExclusive(NormalizedPriorProbabilities, GetName());

	return { ActionSchema.CreateDiscreteExclusive({ Size, MakeArrayView(NormalizedPriorProbabilities) }, Name)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyInclusiveDiscreteAction(const int32 Size, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyInclusiveDiscreteActionFromArrayView(Size, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyInclusiveDiscreteActionFromArrayView(const int32 Size, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid DiscreteInclusive Action Size '%i'."), *GetName(), Size);
		return FLearningAgentsActionSchemaElement();
	}

	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Inclusive Discrete Action."), *GetName());
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(0.5f, Size);
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesInclusive(NormalizedPriorProbabilities, GetName());

	return { ActionSchema.CreateDiscreteInclusive({ Size, MakeArrayView(NormalizedPriorProbabilities) }, Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyStructAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Struct Action."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
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
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyStructActionFromArrayViews(SortedSubElementNames, SortedSubElements, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyStructActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const FName Name)
{
	return SpecifyStructActionFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyStructActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Struct Action."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { ActionSchema.CreateAnd({ ElementNames, SubElements }, Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyExclusiveUnionAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Exclusive Union Action."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	TArray<float, TInlineAllocator<16>> SubElementPriorProbabilities;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);
	SubElementPriorProbabilities.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		const float* PriorProb = PriorProbabilities.Find(Element.Key);
		SubElementPriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / SubElementNum);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	TArray<float, TInlineAllocator<16>> SortedSubElementPriorProbabilities;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	SortedSubElementPriorProbabilities.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
		SortedSubElementPriorProbabilities[Idx] = SubElementPriorProbabilities[SubElementIndices[Idx]];
	}

	return SpecifyExclusiveUnionActionFromArrayViews(SortedSubElementNames, SortedSubElements, SortedSubElementPriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyExclusiveUnionActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyExclusiveUnionActionFromArrayViews(ElementNames, Elements, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyExclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Exclusive Union Action."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(1.0f / Elements.Num(), Elements.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesExclusive(NormalizedPriorProbabilities, GetName());

	return { ActionSchema.CreateOrExclusive({ ElementNames, SubElements, MakeArrayView(NormalizedPriorProbabilities) }, Name)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyInclusiveUnionAction(const TMap<FName, FLearningAgentsActionSchemaElement>& Elements, const TMap<FName, float>& PriorProbabilities, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Inclusive Union Action."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SubElements;
	TArray<float, TInlineAllocator<16>> SubElementPriorProbabilities;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);
	SubElementPriorProbabilities.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsActionSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		const float* PriorProb = PriorProbabilities.Find(Element.Key);
		SubElementPriorProbabilities.Add(PriorProb ? *PriorProb : 1.0f / SubElementNum);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsActionSchemaElement, TInlineAllocator<16>> SortedSubElements;
	TArray<float, TInlineAllocator<16>> SortedSubElementPriorProbabilities;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	SortedSubElementPriorProbabilities.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
		SortedSubElementPriorProbabilities[Idx] = SubElementPriorProbabilities[SubElementIndices[Idx]];
	}

	return SpecifyInclusiveUnionActionFromArrayViews(SortedSubElementNames, SortedSubElements, SortedSubElementPriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyInclusiveUnionActionFromArrays(const TArray<FName> ElementNames, const TArray<FLearningAgentsActionSchemaElement>& Elements, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyInclusiveUnionActionFromArrayViews(ElementNames, Elements, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyInclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionSchemaElement> Elements, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Inclusive Union Action."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionSchemaElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	TArray<UE::Learning::Action::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionSchemaElement& Element : Elements)
	{
		if (!ActionSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	TArray<float, TInlineAllocator<16>> NormalizedPriorProbabilities;
	NormalizedPriorProbabilities.Init(0.5f, Elements.Num());
	for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
	{
		NormalizedPriorProbabilities[Idx] = PriorProbabilities[Idx];
	}
	UE::Learning::Agents::Action::Private::NormalizeProbabilitiesInclusive(NormalizedPriorProbabilities, GetName());

	return { ActionSchema.CreateOrInclusive({ ElementNames, SubElements, MakeArrayView(NormalizedPriorProbabilities) }, Name)};
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyStaticArrayAction(const FLearningAgentsActionSchemaElement Element, const int32 Num, const FName Name)
{
	if (Num < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Static Array Num %i."), *GetName(), Num);
		return FLearningAgentsActionSchemaElement();
	}

	if (Num == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Specifying zero-sized Static Array Action."), *GetName());
	}

	if (!ActionSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	return { ActionSchema.CreateArray({ Element.SchemaElement, Num }, Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyPairAction(const FLearningAgentsActionSchemaElement Key, const FLearningAgentsActionSchemaElement Value, const FName Name)
{
	return SpecifyStructActionFromArrayViews({ TEXT("Key"), TEXT("Value") }, { Key, Value }, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyEnumAction(const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	TArray<float, TInlineAllocator<16>> PriorProbabilitiesArray;
	PriorProbabilitiesArray.Init(1.0f / (Enum->NumEnums() - 1), Enum->NumEnums() - 1);
	for (const TPair<uint8, float> Prior : PriorProbabilities)
	{
		const int32 EnumIndex = Enum->GetIndexByValue(Prior.Key);
		if (EnumIndex != INDEX_NONE)
		{
			PriorProbabilitiesArray[EnumIndex] = Prior.Value;
		}
	}

	return SpecifyEnumActionFromArrayView(Enum, PriorProbabilitiesArray, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyEnumActionFromArray(const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyEnumActionFromArrayView(Enum, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyEnumActionFromArrayView(const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	return SpecifyExclusiveDiscreteActionFromArrayView(Enum->NumEnums() - 1, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyBitmaskAction(const UEnum* Enum, const TMap<uint8, float>& PriorProbabilities, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		return FLearningAgentsActionSchemaElement();
	}

	TArray<float, TInlineAllocator<16>> PriorProbabilitiesArray;
	PriorProbabilitiesArray.Init(0.5f, Enum->NumEnums() - 1);
	for (const TPair<uint8, float> Prior : PriorProbabilities)
	{
		const int32 EnumIndex = Enum->GetIndexByValue(Prior.Key);
		if (EnumIndex != INDEX_NONE)
		{
			PriorProbabilitiesArray[EnumIndex] = Prior.Value;
		}
	}

	return SpecifyBitmaskActionFromArrayView(Enum, PriorProbabilitiesArray, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyBitmaskActionFromArray(const UEnum* Enum, const TArray<float>& PriorProbabilities, const FName Name)
{
	return SpecifyBitmaskActionFromArrayView(Enum, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyBitmaskActionFromArrayView(const UEnum* Enum, const TArrayView<const float> PriorProbabilities, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		return FLearningAgentsActionSchemaElement();
	}

	return SpecifyInclusiveDiscreteActionFromArrayView(Enum->NumEnums() - 1, PriorProbabilities, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyOptionalAction(const FLearningAgentsActionSchemaElement Element, const float PriorProbability, const FName Name)
{
	return SpecifyExclusiveUnionActionFromArrayViews({ TEXT("Null"), TEXT("Valid") }, { SpecifyNullAction(), Element }, { 1.0f - PriorProbability, PriorProbability }, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyEitherAction(const FLearningAgentsActionSchemaElement A, const FLearningAgentsActionSchemaElement B, const float PriorProbabilityOfA, const FName Name)
{
	return SpecifyExclusiveUnionActionFromArrayViews({ TEXT("A"), TEXT("B") }, { A, B }, { 1.0f - PriorProbabilityOfA, PriorProbabilityOfA }, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyEncodingAction(const FLearningAgentsActionSchemaElement Element, const int32 EncodingSize, const FName Name)
{
	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action EncodingSize '%i'."), *GetName(), EncodingSize);
		return FLearningAgentsActionSchemaElement();
	}

	if (!ActionSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		return FLearningAgentsActionSchemaElement();
	}

	return { ActionSchema.CreateEncoding({ Element.SchemaElement, EncodingSize }, Name) };
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyBoolAction(const float PriorProbability, const FName Name)
{
	return SpecifyExclusiveDiscreteActionFromArrayView(2, { 1.0f - PriorProbability, PriorProbability }, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyFloatAction(const FName Name)
{
	return SpecifyContinuousAction(1, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyLocationAction(const FName Name)
{
	return SpecifyContinuousAction(3, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyRotationAction(const FName Name)
{
	return SpecifyContinuousAction(3, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyScaleAction(const FName Name)
{
	return SpecifyContinuousAction(3, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyTransformAction(const FName Name)
{
	return SpecifyStructActionFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			SpecifyLocationAction(),
			SpecifyRotationAction(),
			SpecifyScaleAction()
		}, 
		Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyAngleAction(const FName Name)
{
	return SpecifyContinuousAction(1, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifyVelocityAction(const FName Name)
{
	return SpecifyContinuousAction(3, Name);
}

FLearningAgentsActionSchemaElement ULearningAgentsActionSchema::SpecifySpeedAction(const FName Name)
{
	return SpecifyContinuousAction(1, Name);
}

const UE::Learning::Action::FObject& ULearningAgentsActionObject::GetActionObject() const
{
	return ActionObject;
}

UE::Learning::Action::FObject& ULearningAgentsActionObject::GetActionObject()
{
	return ActionObject;
}

void ULearningAgentsActionObject::LogAction(const FLearningAgentsActionObjectElement Element)
{
	UE::Learning::Agents::Action::Private::LogAction(GetActionObject(), Element.ObjectElement, TEXT(""), TEXT(""), GetName());
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeNullAction(const FName Name)
{
	return { ActionObject.CreateNull(Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeContinuousAction(const TArray<float>& Values, const FName Name)
{
	return MakeContinuousActionFromArrayView(Values, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeContinuousActionFromArrayView(const TArrayView<const float> Values, const FName Name)
{
	if (Values.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Continuous Action."), *GetName());
	}

	return { ActionObject.CreateContinuous({ Values }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeExclusiveDiscreteAction(const int32 Index, const FName Name)
{
	if (Index < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Index %i."), *GetName(), Index);
		return FLearningAgentsActionObjectElement();
	}

	return { ActionObject.CreateDiscreteExclusive({ Index }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeInclusiveDiscreteAction(const TArray<int32>& Indices, const FName Name)
{
	return MakeInclusiveDiscreteActionFromArrayView(Indices, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeInclusiveDiscreteActionFromArrayView(const TArrayView<const int32> Indices, const FName Name)
{
	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(Indices))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Indices contain duplicates."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	const int32 IndexNum = Indices.Num();

	for (int32 IndexIdx = 0; IndexIdx < IndexNum; IndexIdx++)
	{
		if (Indices[IndexIdx] < 0)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Index %i."), *GetName(), Indices[IndexIdx]);
			return FLearningAgentsActionObjectElement();
		}
	}

	return { ActionObject.CreateDiscreteInclusive({ Indices }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeStructAction(const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Struct Action."), *GetName());
	}

	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionObjectElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeStructActionFromArrayViews(SubElementNames, SubElements);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeStructActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name)
{
	return MakeStructActionFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeStructActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Struct Action."), *GetName());
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ActionObject.CreateAnd({ ElementNames, SubElements }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeExclusiveUnionAction(const FName ElementName, const FLearningAgentsActionObjectElement Element, const FName Name)
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	return { ActionObject.CreateOrExclusive({ ElementName, Element.ObjectElement }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeInclusiveUnionAction(const TMap<FName, FLearningAgentsActionObjectElement>& Elements, const FName Name)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	SubElements.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsActionObjectElement>& Element : Elements)
	{
		SubElements.Add(Element.Value);
		SubElementNames.Add(Element.Key);
	}

	return MakeInclusiveUnionActionFromArrayViews(SubElementNames, SubElements, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeInclusiveUnionActionFromArrays(const TArray<FName>& ElementNames, const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name)
{
	return MakeInclusiveUnionActionFromArrayViews(ElementNames, Elements, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeInclusiveUnionActionFromArrayViews(const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name)
{
	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Number of elements (%i) must match number of names (%i)."), *GetName(), Elements.Num(), ElementNames.Num());
		return FLearningAgentsActionObjectElement();
	}

	if (UE::Learning::Agents::Action::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Element Names contain duplicates."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ActionObject.CreateOrInclusive({ ElementNames, SubElements }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeStaticArrayAction(const TArray<FLearningAgentsActionObjectElement>& Elements, const FName Name)
{
	return MakeStaticArrayActionFromArrayView(Elements, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeStaticArrayActionFromArrayView(const TArrayView<const FLearningAgentsActionObjectElement> Elements, const FName Name)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Creating zero-sized Static Array Action."), *GetName());
	}

	TArray<UE::Learning::Action::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsActionObjectElement& Element : Elements)
	{
		if (!ActionObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			return FLearningAgentsActionObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { ActionObject.CreateArray({ SubElements }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakePairAction(const FLearningAgentsActionObjectElement Key, const FLearningAgentsActionObjectElement Value, const FName Name)
{
	return MakeStructActionFromArrayViews({ TEXT("Key"), TEXT("Value") }, { Key, Value }, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeEnumAction(const UEnum* Enum, const uint8 EnumValue, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	const int32 EnumValueIndex = Enum->GetIndexByValue(EnumValue);

	if (EnumValueIndex == INDEX_NONE || EnumValueIndex < 0 || EnumValueIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: EnumValue %i not valid for Enum '%s'."), *GetName(), EnumValue , *Enum->GetName());
		return FLearningAgentsActionObjectElement();
	}

	return MakeExclusiveDiscreteAction(EnumValueIndex, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeBitmaskAction(const UEnum* Enum, const int32 BitmaskValue, const FName Name)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values in Enum to use as Bitmask (%i)."), *GetName(), Enum->NumEnums() - 1);
		return FLearningAgentsActionObjectElement();
	}

	TArray<int32, TInlineAllocator<32>> BitmaskIndices;
	BitmaskIndices.Empty(Enum->NumEnums() - 1);

	for (int32 BitmaskIdx = 0; BitmaskIdx < Enum->NumEnums() - 1; BitmaskIdx++)
	{
		if (BitmaskValue & (1 << BitmaskIdx))
		{
			BitmaskIndices.Add(BitmaskIdx);
		}
	}

	return MakeInclusiveDiscreteActionFromArrayView(BitmaskIndices, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeOptionalAction(const FLearningAgentsActionObjectElement Element, const ELearningAgentsOptionalAction Option, const FName Name)
{
	return MakeExclusiveUnionAction(
		Option == ELearningAgentsOptionalAction::Null ? TEXT("Null") : TEXT("Valid"),
		Option == ELearningAgentsOptionalAction::Null ? MakeNullAction() : Element,
		Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeOptionalNullAction(const FName Name)
{
	return MakeExclusiveUnionAction(TEXT("Null"), MakeNullAction(), Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeOptionalValidAction(const FLearningAgentsActionObjectElement Element, const FName Name)
{
	return MakeExclusiveUnionAction(TEXT("Valid"), Element, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeEitherAction(const FLearningAgentsActionObjectElement Element, const ELearningAgentsEitherAction Either, const FName Name)
{
	return MakeExclusiveUnionAction(Either == ELearningAgentsEitherAction::A ? TEXT("A") : TEXT("B"), Element, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeEitherAAction(const FLearningAgentsActionObjectElement A, const FName Name)
{
	return MakeExclusiveUnionAction(TEXT("A"), A, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeEitherBAction(const FLearningAgentsActionObjectElement B, const FName Name)
{
	return MakeExclusiveUnionAction(TEXT("B"), B, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeEncodingAction(const FLearningAgentsActionObjectElement Element, const FName Name)
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		return FLearningAgentsActionObjectElement();
	}

	return { ActionObject.CreateEncoding({ Element.ObjectElement }, Name) };
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeBoolAction(const bool bValue, const FName Name)
{
	return MakeExclusiveDiscreteAction(bValue ? 1 : 0, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeFloatAction(const float Value, const float FloatScale, const FName Name)
{
	return MakeContinuousActionFromArrayView({ Value / FMath::Max(FloatScale, UE_SMALL_NUMBER) }, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeLocationAction(const FVector Location, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	const FVector LocalLocation = RelativeTransform.InverseTransformPosition(Location);

	return MakeContinuousActionFromArrayView({
		(float)LocalLocation.X / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		(float)LocalLocation.Y / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		(float)LocalLocation.Z / FMath::Max(LocationScale, UE_SMALL_NUMBER) }, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeRotationAction(const FRotator Rotation, const FRotator RelativeRotation, const float RotationScale, const FName Name)
{
	return MakeRotationActionFromQuat(FQuat::MakeFromRotator(Rotation), FQuat::MakeFromRotator(RelativeRotation), RotationScale, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeRotationActionFromQuat(const FQuat Rotation, const FQuat RelativeRotation, const float RotationScale, const FName Name)
{
	FQuat LocalRotation = RelativeRotation.Inverse() * Rotation;
	LocalRotation.EnforceShortestArcWith(FQuat::Identity);
	const FVector RotationVector = LocalRotation.ToRotationVector();

	return MakeContinuousActionFromArrayView({
		(float)RotationVector.X / FMath::Max(FMath::DegreesToRadians(RotationScale), UE_SMALL_NUMBER),
		(float)RotationVector.Y / FMath::Max(FMath::DegreesToRadians(RotationScale), UE_SMALL_NUMBER),
		(float)RotationVector.Z / FMath::Max(FMath::DegreesToRadians(RotationScale), UE_SMALL_NUMBER),
		}, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeScaleAction(const FVector Scale, const FVector RelativeScale, const FName Name)
{
	const FVector LocalLogScale =
		UE::Learning::Agents::Action::Private::VectorLogSafe(Scale) -
		UE::Learning::Agents::Action::Private::VectorLogSafe(RelativeScale);

	return MakeContinuousActionFromArrayView({
		(float)LocalLogScale.X,
		(float)LocalLogScale.Y,
		(float)LocalLogScale.Z,
		}, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeTransformAction(const FTransform Transform, const FTransform RelativeTransform, const float LocationScale, const FName Name)
{
	const FTransform LocalTransform = Transform * RelativeTransform.Inverse();

	return MakeStructActionFromArrayViews(
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			MakeLocationAction(LocalTransform.GetLocation(), FTransform::Identity, LocationScale),
			MakeRotationActionFromQuat(LocalTransform.GetRotation(), FQuat::Identity),
			MakeScaleAction(LocalTransform.GetScale3D(), FVector::OneVector)
		},
		Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeAngleAction(const float Angle, const float RelativeAngle, const float AngleScale, const FName Name)
{
	return MakeAngleActionRadians(FMath::DegreesToRadians(Angle), FMath::DegreesToRadians(RelativeAngle), FMath::DegreesToRadians(AngleScale), Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeAngleActionRadians(const float Angle, const float RelativeAngle, const float AngleScale, const FName Name)
{
	const float LocalAngle = FMath::FindDeltaAngleRadians(RelativeAngle, Angle);
	return MakeContinuousActionFromArrayView({ LocalAngle / FMath::Max(AngleScale, UE_SMALL_NUMBER) }, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeVelocityAction(const FVector Velocity, const FTransform RelativeTransform, const float VelocityScale, const FName Name)
{
	const FVector LocalVelocity = RelativeTransform.InverseTransformVectorNoScale(Velocity);

	return MakeContinuousActionFromArrayView({
		(float)LocalVelocity.X / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		(float)LocalVelocity.Y / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		(float)LocalVelocity.Z / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		}, Name);
}

FLearningAgentsActionObjectElement ULearningAgentsActionObject::MakeSpeedAction(const float Speed, const float SpeedScale, const FName Name)
{
	return MakeContinuousActionFromArrayView({ Speed / FMath::Max(SpeedScale, UE_SMALL_NUMBER) });
}

bool ULearningAgentsActionObject::GetNullAction(const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Null)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Null));
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetContinuousActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Continuous));
		OutNum = 0;
		return false;
	}

	OutNum = ActionObject.GetContinuous(Element.ObjectElement).Values.Num();
	return true;
}

bool ULearningAgentsActionObject::GetContinuousAction(TArray<float>& OutValues, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutValueNum = 0;
	if (!GetContinuousActionNum(OutValueNum, Element, Name))
	{
		OutValues.Empty();
		return false;
	}

	OutValues.SetNumUninitialized(OutValueNum);

	if (!GetContinuousActionToArrayView(OutValues, Element, Name))
	{
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetContinuousActionToArrayView(TArrayView<float> OutValues, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Continuous));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	const TArrayView<const float> Values = ActionObject.GetContinuous(Element.ObjectElement).Values;

	if (Values.Num() != OutValues.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match. Action is '%i' values but asked for '%i'."),
			*GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(),
			Values.Num(), OutValues.Num());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	UE::Learning::Array::Copy<1, float>(OutValues, Values);
	return true;
}

bool ULearningAgentsActionObject::GetExclusiveDiscreteAction(int32& OutIndex, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutIndex = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteExclusive));
		OutIndex = 0;
		return false;
	}

	OutIndex = ActionObject.GetDiscreteExclusive(Element.ObjectElement).DiscreteIndex;
	return true;
}

bool ULearningAgentsActionObject::GetInclusiveDiscreteActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteInclusive));
		OutNum = 0;
		return false;
	}

	OutNum = ActionObject.GetDiscreteInclusive(Element.ObjectElement).DiscreteIndices.Num();
	return true;
}

bool ULearningAgentsActionObject::GetInclusiveDiscreteAction(TArray<int32>& OutIndices, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutIndexNum = 0;
	if (!GetInclusiveDiscreteActionNum(OutIndexNum, Element, Name))
	{
		OutIndices.Empty();
		return false;
	}

	OutIndices.SetNumUninitialized(OutIndexNum);

	if (!GetInclusiveDiscreteActionToArrayView(OutIndices, Element, Name))
	{
		OutIndices.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetInclusiveDiscreteActionToArrayView(TArrayView<int32> OutIndices, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::DiscreteInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::DiscreteInclusive));
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	const TArrayView<const int32> Indices = ActionObject.GetDiscreteInclusive(Element.ObjectElement).DiscreteIndices;

	if (Indices.Num() != OutIndices.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(),
			Indices.Num(), OutIndices.Num());
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	UE::Learning::Array::Copy<1, int32>(OutIndices, Indices);
	return true;
}

bool ULearningAgentsActionObject::GetStructActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::And));
		OutNum = 0;
		return false;
	}
	
	const UE::Learning::Action::FObjectAndParameters Parameters = ActionObject.GetAnd(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsActionObject::GetStructAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStructActionNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetStructActionToArrayViews(SubElementNames, SubElements, Element, Name))
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

bool ULearningAgentsActionObject::GetStructActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStructActionNum(OutElementNum, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetStructActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::And));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const UE::Learning::Action::FObjectAndParameters Parameters = ActionObject.GetAnd(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Getting zero-sized And Action."), *GetName());
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ActionObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActionObject::GetExclusiveUnionAction(FName& OutElementName, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutElementName = NAME_None;
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrExclusive));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	const UE::Learning::Action::FObjectOrExclusiveParameters Parameters = ActionObject.GetOrExclusive(Element.ObjectElement);

	OutElementName = Parameters.ElementName;
	OutElement = { Parameters.Element };
	return true;
}

bool ULearningAgentsActionObject::GetInclusiveUnionActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrInclusive));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = ActionObject.GetOrInclusive(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsActionObject::GetInclusiveUnionAction(TMap<FName, FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionActionNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsActionObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionActionToArrayViews(SubElementNames, SubElements, Element, Name))
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

bool ULearningAgentsActionObject::GetInclusiveUnionActionToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionActionNum(OutElementNum, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionActionToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetInclusiveUnionActionToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::OrInclusive));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const UE::Learning::Action::FObjectOrInclusiveParameters Parameters = ActionObject.GetOrInclusive(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!ActionObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActionObject::GetStaticArrayActionNum(int32& OutNum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutNum = 0;
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Array));
		OutNum = 0;
		return false;
	}

	OutNum = ActionObject.GetArray(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsActionObject::GetStaticArrayAction(TArray<FLearningAgentsActionObjectElement>& OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutElementNum = 0;
	if (!GetStaticArrayActionNum(OutElementNum, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStaticArrayActionToArrayView(OutElements, Element, Name))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsActionObject::GetStaticArrayActionToArrayView(TArrayView<FLearningAgentsActionObjectElement> OutElements, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."), 
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Array));
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	const TArrayView<const UE::Learning::Action::FObjectElement> SubElements = ActionObject.GetArray(Element.ObjectElement).Elements;

	if (SubElements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Getting zero-sized Array Action."), *GetName());
	}

	if (SubElements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' size does not match. Action is '%i' elements but asked for '%i'."),
			*GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(),
			SubElements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < SubElements.Num(); ElementIdx++)
	{
		if (!ActionObject.IsValid(SubElements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
			UE::Learning::Array::Set<1, FLearningAgentsActionObjectElement>(OutElements, FLearningAgentsActionObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { SubElements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsActionObject::GetPairAction(FLearningAgentsActionObjectElement& OutKey, FLearningAgentsActionObjectElement& OutValue, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	TStaticArray<FName, 2> OutElementNames;
	TStaticArray<FLearningAgentsActionObjectElement, 2> OutElements;
	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutKey = FLearningAgentsActionObjectElement();
		OutValue = FLearningAgentsActionObjectElement();
		return false;
	}

	OutKey = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Key"))];
	OutValue = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Value"))];
	return true;
}

bool ULearningAgentsActionObject::GetEnumAction(uint8& OutEnumValue, const UEnum* Enum, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum is nullptr."), *GetName());
		OutEnumValue = 0;
		return false;
	}

	int32 OutIndex = 0;
	if (!GetExclusiveDiscreteAction(OutIndex, Element, Name))
	{
		OutEnumValue = 0;
		return false;
	}

	if (OutIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: EnumValue out of range for Enum '%s'. Expected %i or less, got %i."), *GetName(), *Enum->GetName(), Enum->NumEnums() - 1, OutIndex);
		OutEnumValue = 0;
		return false;
	}

	const int32 EnumValue = Enum->GetValueByIndex(OutIndex);

	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Enum Value not found for index %i."), *GetName(), OutIndex);
		OutEnumValue = 0;
		return false;
	}

	OutEnumValue = (uint8)EnumValue;
	return true;
}

bool ULearningAgentsActionObject::GetBitmaskAction(int32& OutBitmaskValue, const UEnum* Enum, const FLearningAgentsActionObjectElement Element, const FName Name) const
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
	if (!GetInclusiveDiscreteActionNum(EnumValueNum, Element, Name))
	{
		OutBitmaskValue = 0;
		return false;
	}

	if (EnumValueNum > Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Too many values for Enum '%s'. Expected %i or less, got %i."), *GetName(), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutBitmaskValue = 0;
		return false;
	}

	TArray<int32, TInlineAllocator<32>> OutIndices;
	OutIndices.SetNumUninitialized(EnumValueNum);
	if (!GetInclusiveDiscreteActionToArrayView(OutIndices, Element, Name))
	{
		OutBitmaskValue = 0;
		return false;
	}

	OutBitmaskValue = 0;
	for (const int32 OutIndex : OutIndices)
	{
		OutBitmaskValue |= (1 << OutIndex);
	}
	return true;
}

bool ULearningAgentsActionObject::GetOptionalAction(ELearningAgentsOptionalAction& OutOption, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionAction(OutName, OutElement, Element, Name))
	{
		OutOption = ELearningAgentsOptionalAction::Null;
		return false;
	}

	OutOption = OutName == TEXT("Null") ? ELearningAgentsOptionalAction::Null : ELearningAgentsOptionalAction::Valid;
	return true;
}

bool ULearningAgentsActionObject::GetEitherAction(ELearningAgentsEitherAction& OutEither, FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionAction(OutName, OutElement, Element, Name))
	{
		OutEither = ELearningAgentsEitherAction::A;
		return false;
	}

	OutEither = OutName == TEXT("A") ? ELearningAgentsEitherAction::A : ELearningAgentsEitherAction::B;
	return true;
}

bool ULearningAgentsActionObject::GetEncodingAction(FLearningAgentsActionObjectElement& OutElement, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	if (!ActionObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid Action Object."), *GetName());
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	if (ActionObject.GetName(Element.ObjectElement) != Name)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Action name does not match. Action is '%s' but asked for '%s'."), *GetName(), *ActionObject.GetName(Element.ObjectElement).ToString(), *Name.ToString());
	}

	if (ActionObject.GetType(Element.ObjectElement) != UE::Learning::Action::EType::Encoding)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action '%s' type does not match. Action is '%s' but asked for '%s'."),
			*GetName(),
			*ActionObject.GetName(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Action::Private::GetActionTypeString(ActionObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Action::Private::GetActionTypeString(UE::Learning::Action::EType::Encoding));
		OutElement = FLearningAgentsActionObjectElement();
		return false;
	}

	OutElement = { ActionObject.GetEncoding(Element.ObjectElement).Element };

	return true;
}

bool ULearningAgentsActionObject::GetBoolAction(bool& bOutValue, const FLearningAgentsActionObjectElement Element, const FName Name) const
{
	int32 OutIndex = 0;
	if (!GetExclusiveDiscreteAction(OutIndex, Element, Name))
	{
		bOutValue = false;
		return false;
	}

	bOutValue = OutIndex == 1;
	return true;
}

bool ULearningAgentsActionObject::GetFloatAction(float& OutValue, const FLearningAgentsActionObjectElement Element, const float FloatScale, const FName Name) const
{
	float OutValuesData;
	if (!GetContinuousActionToArrayView(MakeArrayView(&OutValuesData, 1), Element, Name))
	{
		OutValue = 0.0f;
		return false;
	}

	OutValue = OutValuesData * FloatScale;
	return true;
}

bool ULearningAgentsActionObject::GetLocationAction(FVector& OutLocation, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Element, Name))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	const FVector LocalLocation = LocationScale * FVector(OutValues[0], OutValues[1], OutValues[2]);
	OutLocation = RelativeTransform.TransformPosition(LocalLocation);
	return true;
}

bool ULearningAgentsActionObject::GetRotationAction(FRotator& OutRotation, const FLearningAgentsActionObjectElement Element, const FRotator RelativeRotation, const float RotationScale, const FName Name) const
{
	FQuat OutRotationQuat;
	if (!GetRotationActionAsQuat(OutRotationQuat, Element, FQuat::MakeFromRotator(RelativeRotation), RotationScale, Name))
	{
		OutRotation = FRotator::ZeroRotator;
		return false;
	}

	OutRotation = OutRotationQuat.Rotator();
	return true;
}

bool ULearningAgentsActionObject::GetRotationActionAsQuat(FQuat& OutRotation, const FLearningAgentsActionObjectElement Element, const FQuat RelativeRotation, const float RotationScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Element, Name))
	{
		OutRotation = FQuat::Identity;
		return false;
	}

	const FVector LocalRotationVector = FMath::DegreesToRadians(RotationScale) * FVector(OutValues[0], OutValues[1], OutValues[2]);
	OutRotation = RelativeRotation * FQuat::MakeFromRotationVector(LocalRotationVector);
	return true;
}

bool ULearningAgentsActionObject::GetScaleAction(FVector& OutScale, const FLearningAgentsActionObjectElement Element, const FVector RelativeScale, const float Scale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Element, Name))
	{
		OutScale = FVector::OneVector;
		return false;
	}

	const FVector LocalScaleVector = UE::Learning::Agents::Action::Private::VectorExp(Scale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	OutScale = RelativeScale * LocalScaleVector;
	return true;
}

bool ULearningAgentsActionObject::GetTransformAction(FTransform& OutTransform, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const float RotationScale, const float ScaleScale, const FName Name) const
{
	TStaticArray<FName, 3> OutElementNames;
	TStaticArray<FLearningAgentsActionObjectElement, 3> OutElements;
	if (!GetStructActionToArrayViews(OutElementNames, OutElements, Element, Name))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FVector OutLocation;
	if (!GetLocationAction(OutLocation, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Location"))], RelativeTransform, LocationScale))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FQuat OutRotation;
	if (!GetRotationActionAsQuat(OutRotation, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Rotation"))], RelativeTransform.GetRotation(), RotationScale))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	FVector OutScale;
	if (!GetScaleAction(OutScale, OutElements[MakeArrayView(OutElementNames).Find(TEXT("Scale"))], RelativeTransform.GetScale3D(), ScaleScale))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	OutTransform = FTransform(OutRotation, OutLocation, OutScale);
	return true;
}

bool ULearningAgentsActionObject::GetAngleAction(float& OutAngle, const FLearningAgentsActionObjectElement Element, const float RelativeAngle, const float AngleScale, const FName Name) const
{
	if (GetAngleActionRadians(OutAngle, Element, FMath::DegreesToRadians(RelativeAngle), FMath::DegreesToRadians(AngleScale), Name))
	{
		OutAngle = FMath::RadiansToDegrees(OutAngle);
		return true;
	}
	else
	{
		OutAngle = 0.0f;
		return false;
	}
}

bool ULearningAgentsActionObject::GetAngleActionRadians(float& OutAngle, const FLearningAgentsActionObjectElement Element, const float RelativeAngle, const float AngleScale, const FName Name) const
{
	if (!GetContinuousActionToArrayView(MakeArrayView(&OutAngle, 1), Element, Name))
	{
		OutAngle = 0.0f;
		return false;
	}

	OutAngle = RelativeAngle + AngleScale * OutAngle;
	return true;
}

bool ULearningAgentsActionObject::GetVelocityAction(FVector& OutVelocity, const FLearningAgentsActionObjectElement Element, const FTransform RelativeTransform, const float VelocityScale, const FName Name) const
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousActionToArrayView(OutValues, Element, Name))
	{
		OutVelocity = FVector::OneVector;
		return false;
	}

	OutVelocity = RelativeTransform.TransformVector(VelocityScale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsActionObject::GetSpeedAction(float& OutSpeed, const FLearningAgentsActionObjectElement Element, const float SpeedScale, const FName Name) const
{
	if (!GetContinuousActionToArrayView(MakeArrayView(&OutSpeed, 1), Element, Name))
	{
		OutSpeed = 0.0f;
		return false;
	}

	OutSpeed = SpeedScale * OutSpeed;
	return true;
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorTextResolver.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Text3DComponent.h"

void UPropertyAnimatorTextResolver::SetUnit(EPropertyAnimatorTextResolverRangeUnit InUnit)
{
	Unit = InUnit;
}

void UPropertyAnimatorTextResolver::SetStart(float InRangeStart)
{
	Start = FMath::Clamp(InRangeStart, 0, 100);
}

void UPropertyAnimatorTextResolver::SetEnd(float InRangeEnd)
{
	End = FMath::Clamp(InRangeEnd, 0, 100);
}

void UPropertyAnimatorTextResolver::SetOffset(float InRangeOffset)
{
	Offset = InRangeOffset;
}

void UPropertyAnimatorTextResolver::SetCharacterStartIndex(int32 InRangeStart)
{
	CharacterStartIndex = FMath::Max(InRangeStart, 0);
}

void UPropertyAnimatorTextResolver::SetCharacterEndIndex(int32 InRangeEnd)
{
	CharacterEndIndex = FMath::Max(InRangeEnd, 0);
}

void UPropertyAnimatorTextResolver::SetCharacterOffsetIndex(int32 InRangeOffset)
{
	CharacterOffsetIndex = InRangeOffset;
}

void UPropertyAnimatorTextResolver::SetWordStartIndex(int32 InRangeStart)
{
	WordStartIndex = FMath::Max(InRangeStart, 0);
}

void UPropertyAnimatorTextResolver::SetWordEndIndex(int32 InRangeEnd)
{
	WordEndIndex = FMath::Max(InRangeEnd, 0);
}

void UPropertyAnimatorTextResolver::SetWordOffsetIndex(int32 InRangeOffset)
{
	WordOffsetIndex = InRangeOffset;
}

void UPropertyAnimatorTextResolver::SetDirection(EPropertyAnimatorTextResolverRangeDirection InDirection)
{
	Direction = InDirection;
}

void UPropertyAnimatorTextResolver::GetResolvableProperties(const FPropertyAnimatorCoreData& InParentProperty, TSet<FPropertyAnimatorCoreData>& OutProperties)
{
	const AActor* Actor = InParentProperty.GetOwningActor();
	if (!Actor || InParentProperty.IsResolved())
	{
		return;
	}

	UText3DComponent* TextComponent = Actor->FindComponentByClass<UText3DComponent>();
	if (!TextComponent)
	{
		return;
	}

	USceneComponent* TextRootComponent = TextComponent->GetChildComponent(1);
	if (!TextRootComponent)
	{
		return;
	}

	FProperty* RelativeLocation = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeLocation"));
	FPropertyAnimatorCoreData RelativeLocationProperty(TextRootComponent, RelativeLocation, nullptr, GetClass());
	OutProperties.Add(RelativeLocationProperty);

	FProperty* RelativeRotation = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeRotation"));
	FPropertyAnimatorCoreData RelativeRotationProperty(TextRootComponent, RelativeRotation, nullptr, GetClass());
	OutProperties.Add(RelativeRotationProperty);

	FProperty* RelativeScale = FindFProperty<FProperty>(TextRootComponent->GetClass(), TEXT("RelativeScale3D"));
	FPropertyAnimatorCoreData RelativeScaleProperty(TextRootComponent, RelativeScale, nullptr, GetClass());
	OutProperties.Add(RelativeScaleProperty);
}

void UPropertyAnimatorTextResolver::ResolveProperties(const FPropertyAnimatorCoreData& InTemplateProperty, TArray<FPropertyAnimatorCoreData>& OutProperties, bool bInForEvaluation)
{
	if (!InTemplateProperty.IsResolvable())
	{
		return;
	}

	const USceneComponent* TextRootComponent = Cast<USceneComponent>(InTemplateProperty.GetOwningComponent());
	if (!TextRootComponent)
	{
		return;
	}

	const TArray<FProperty*> ChainProperties = InTemplateProperty.GetChainProperties();

	// Gather each character in the text
	for (int32 ComponentIndex = 0; ComponentIndex < TextRootComponent->GetNumChildrenComponents(); ComponentIndex++)
	{
		USceneComponent* CharacterKerningComponent = TextRootComponent->GetChildComponent(ComponentIndex);

		if (!CharacterKerningComponent)
		{
			continue;
		}

		FPropertyAnimatorCoreData CharacterProperty(CharacterKerningComponent, ChainProperties);
		OutProperties.Add(CharacterProperty);
	}

	if (!bInForEvaluation || OutProperties.IsEmpty())
	{
		return;
	}

	const int32 MaxIndex = OutProperties.Num();
	int32 BeginIndex = 0;
	int32 EndIndex = 0;

	switch (Unit)
	{
		case EPropertyAnimatorTextResolverRangeUnit::Percentage:
		{
			float StartPercentage = Start / 100.f;
			float EndPercentage = End / 100.f;
			float OffsetPercentage = Offset / 100.f;

			if (Direction == EPropertyAnimatorTextResolverRangeDirection::RightToLeft)
			{
				const float Temp = 1.f - StartPercentage;
				StartPercentage = 1.f - EndPercentage;
				EndPercentage = Temp;
				OffsetPercentage *= -1;
			}
			else if (Direction == EPropertyAnimatorTextResolverRangeDirection::FromCenter)
			{
				constexpr float MidPercentage = 0.5f;
				const float Expansion = EndPercentage / 2.f;
				StartPercentage = MidPercentage - Expansion;
				EndPercentage = MidPercentage + Expansion;
			}

			BeginIndex = StartPercentage * MaxIndex + OffsetPercentage * MaxIndex;
			EndIndex = EndPercentage * MaxIndex + OffsetPercentage * MaxIndex;
		}
		break;

		case EPropertyAnimatorTextResolverRangeUnit::Character:
		{
			int32 CharacterStart = CharacterStartIndex;
			int32 CharacterEnd = CharacterEndIndex;
			int32 CharacterOffset = CharacterOffsetIndex;

			if (Direction == EPropertyAnimatorTextResolverRangeDirection::RightToLeft)
			{
				const int32 Temp = MaxIndex - CharacterStart;
				CharacterStart = MaxIndex - CharacterEnd;
				CharacterEnd = Temp;
				CharacterOffset *= -1;
			}
			else if (Direction == EPropertyAnimatorTextResolverRangeDirection::FromCenter)
			{
				const int32 CharacterMid = MaxIndex / 2;
				const int32 Expansion = CharacterEnd / 2;
				CharacterStart = CharacterMid - Expansion;
				CharacterEnd = CharacterMid + Expansion;
			}

			BeginIndex = CharacterStart + CharacterOffset;
			EndIndex = CharacterEnd + CharacterOffset;
		}
		break;

		case EPropertyAnimatorTextResolverRangeUnit::Word:
		{
			if (const UText3DComponent* TextComponent = TextRootComponent->GetTypedOuter<UText3DComponent>())
			{
				const FText3DStatistics& TextStats = TextComponent->GetStatistics();

				if (TextStats.Words.IsEmpty())
				{
					break;
				}

				const int32 WordCount = TextStats.Words.Num();
				int32 WordStart = WordStartIndex;
				int32 WordEnd = WordEndIndex;
				int32 WordOffset = WordOffsetIndex;

				if (Direction == EPropertyAnimatorTextResolverRangeDirection::RightToLeft)
				{
					const int32 Temp = WordCount - WordStart;
					WordStart = WordCount - WordEnd;
					WordEnd = Temp;
					WordOffset *= -1;
				}
				else if (Direction == EPropertyAnimatorTextResolverRangeDirection::FromCenter)
				{
					const int32 WordMid = FMath::CeilToInt(WordCount / 2.f);
					const int32 Expansion = FMath::CeilToInt(WordEnd / 2.f);
					WordStart = WordMid - Expansion;
					WordEnd = WordMid + Expansion;
				}

				if (WordStart != WordEnd)
				{
					WordStart += WordOffset;
					WordEnd += WordOffset - 1;

					if (TextStats.Words.IsValidIndex(WordStart))
					{
						BeginIndex = TextStats.Words[WordStart].RenderRange.BeginIndex;
					}

					if (TextStats.Words.IsValidIndex(WordEnd))
					{
						EndIndex = TextStats.Words[WordEnd].RenderRange.EndIndex;
					}
					else if (WordEnd >= WordCount && WordStart < WordCount)
					{
						EndIndex = TextStats.Words.Last().RenderRange.EndIndex;
					}
				}
			}
		}
		break;
	}

	if (EndIndex < 0 || BeginIndex > EndIndex || BeginIndex == EndIndex || BeginIndex > MaxIndex)
	{
		OutProperties.Empty();
		return;
	}

	// Remove at the end
	if (EndIndex < MaxIndex)
	{
		OutProperties.RemoveAt(EndIndex, MaxIndex - EndIndex);
	}

	// Remove at the start
	if (BeginIndex > 0 && BeginIndex <= MaxIndex)
	{
		OutProperties.RemoveAt(0, BeginIndex);
	}
}

bool UPropertyAnimatorTextResolver::ImportPreset(const UPropertyAnimatorCorePresetBase* InPreset, const TSharedRef<FJsonValue>& InValue)
{
	const TSharedPtr<FJsonObject>* JsonResolverObjectPtr;

	if (Super::ImportPreset(InPreset, InValue) && InValue->TryGetObject(JsonResolverObjectPtr))
	{
		const TSharedPtr<FJsonObject> JsonResolverObject = *JsonResolverObjectPtr;

		uint8 JsonUnit = static_cast<uint8>(Unit);
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Unit), JsonUnit);
		SetUnit(static_cast<EPropertyAnimatorTextResolverRangeUnit>(JsonUnit));

		float JsonStart = Start;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Start), JsonStart);
		SetStart(JsonStart);

		float JsonEnd = End;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, End), JsonEnd);
		SetEnd(JsonEnd);

		float JsonOffset = Offset;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Offset), JsonOffset);
		SetOffset(JsonOffset);

		int32 JsonCharStartIndex = CharacterStartIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterStartIndex), JsonCharStartIndex);
		SetCharacterStartIndex(JsonCharStartIndex);

		int32 JsonCharEndIndex = CharacterEndIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterEndIndex), JsonCharEndIndex);
		SetCharacterEndIndex(JsonCharEndIndex);

		int32 JsonCharOffsetIndex = CharacterOffsetIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterOffsetIndex), JsonCharOffsetIndex);
		SetCharacterOffsetIndex(JsonCharOffsetIndex);

		int32 JsonWordStartIndex = WordStartIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordStartIndex), JsonWordStartIndex);
		SetWordStartIndex(JsonWordStartIndex);

		int32 JsonWordEndIndex = WordEndIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordEndIndex), JsonWordEndIndex);
		SetWordEndIndex(JsonWordEndIndex);

		int32 JsonWordOffsetIndex = WordOffsetIndex;
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordOffsetIndex), JsonWordOffsetIndex);
		SetWordOffsetIndex(JsonWordOffsetIndex);

		uint8 JsonDirection = static_cast<uint8>(Direction);
		JsonResolverObject->TryGetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Direction), JsonDirection);
		SetDirection(static_cast<EPropertyAnimatorTextResolverRangeDirection>(JsonWordOffsetIndex));

		return true;
	}

	return false;
}

bool UPropertyAnimatorTextResolver::ExportPreset(const UPropertyAnimatorCorePresetBase* InPreset, TSharedPtr<FJsonValue>& OutValue)
{
	const TSharedPtr<FJsonObject>* JsonResolverObjectPtr;

	if (Super::ExportPreset(InPreset, OutValue) && OutValue->TryGetObject(JsonResolverObjectPtr))
	{
		const TSharedPtr<FJsonObject> JsonResolverObject = *JsonResolverObjectPtr;

		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Unit), static_cast<uint8>(Unit));
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Start), Start);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, End), End);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Offset), Offset);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterStartIndex), CharacterStartIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterEndIndex), CharacterEndIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, CharacterOffsetIndex), CharacterOffsetIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordStartIndex), WordStartIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordEndIndex), WordEndIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, WordOffsetIndex), WordOffsetIndex);
		JsonResolverObject->SetNumberField(GET_MEMBER_NAME_STRING_CHECKED(UPropertyAnimatorTextResolver, Direction), static_cast<uint8>(Direction));

		return true;
	}

	return false;
}

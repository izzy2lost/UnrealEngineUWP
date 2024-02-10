// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Attributes/AvaSceneAttribute.h"
#include "Containers/Array.h"
#include "UObject/Object.h"
#include "AvaSceneSettings.generated.h"

/** Object containing information about its Scene */
UCLASS(MinimalAPI)
class UAvaSceneSettings : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Iterate each valid Scene Attribute of the given Type
	 * The callable should return true to continue iteration, and false to stop it
	 */
	template<typename InAttributeType>
	void ForEachSceneAttributeOfType(TFunctionRef<bool(const InAttributeType&)> InCallable) const
	{
		for (const UAvaSceneAttribute* SceneAttribute : SceneAttributes)
		{
			if (const InAttributeType* CastedAttribute = Cast<InAttributeType>(SceneAttribute))
			{
				if (!InCallable(*CastedAttribute))
				{
					break;
				}
			}
		}
	}

private:
	UPROPERTY(EditAnywhere, Instanced, Category="Attributes",  meta=(ShowOnlyInnerProperties))
	TArray<TObjectPtr<UAvaSceneAttribute>> SceneAttributes;
};

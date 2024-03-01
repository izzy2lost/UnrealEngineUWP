// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaAttribute.h"
#include "Containers/Array.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "AvaSceneSettings.generated.h"

/** Object containing information about its Scene */
UCLASS(MinimalAPI)
class UAvaSceneSettings : public UObject
{
	GENERATED_BODY()

public:
	static FName GetSceneAttributesPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UAvaSceneSettings, SceneAttributes);
	}

	static FName GetSceneRigPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UAvaSceneSettings, SceneRig);
	}

	/**
	 * Iterate each valid Scene Attribute of the given Type
	 * The callable should return true to continue iteration, and false to stop it
	 */
	template<typename InAttributeType>
	void ForEachSceneAttributeOfType(TFunctionRef<bool(const InAttributeType&)> InCallable) const
	{
		for (const UAvaAttribute* SceneAttribute : SceneAttributes)
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

	FSoftObjectPath GetSceneRig() const
	{
		return SceneRig;
	}

	void SetSceneRig(const FSoftObjectPath& InSceneRig)
	{
		SceneRig = InSceneRig;
	}


private:
	UPROPERTY(EditAnywhere, Instanced, Category="Scene Attributes")
	TArray<TObjectPtr<UAvaAttribute>> SceneAttributes;

	UPROPERTY()
	FSoftObjectPath SceneRig;
};

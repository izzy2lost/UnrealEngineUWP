// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothAssetInteractor.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothAssetInteractor)

void UChaosClothAssetInteractor::SetProperties(const TArray<TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade>>& InCollectionPropertyFacades)
{
	CollectionPropertyFacades.Reset(InCollectionPropertyFacades.Num());
	for (const TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade>& InFacade : InCollectionPropertyFacades)
	{
		CollectionPropertyFacades.Emplace(InFacade);
	}
}

void UChaosClothAssetInteractor::ResetProperties()
{
	CollectionPropertyFacades.Reset();
}

TArray<FString> UChaosClothAssetInteractor::GetAllProperties(int32 LODIndex) const
{
	TArray<FString> Keys;
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				if (Keys.IsEmpty())
				{
					// This is the first non-empty LOD. We can add all keys without worrying about uniqueness.
					Keys.Reserve(PropertyFacade->Num());
					for (int32 KeyIndex = 0; KeyIndex < PropertyFacade->Num(); ++KeyIndex)
					{
						Keys.Add(PropertyFacade->GetKey(KeyIndex));
					}
				}
				else
				{
					for (int32 KeyIndex = 0; KeyIndex < PropertyFacade->Num(); ++KeyIndex)
					{
						Keys.AddUnique(PropertyFacade->GetKey(KeyIndex));
					}
				}
			}
		}
	}
	else
	{
		if (CollectionPropertyFacades.IsValidIndex(LODIndex))
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
			{
				Keys.Reserve(PropertyFacade->Num());
				for (int32 KeyIndex = 0; KeyIndex < PropertyFacade->Num(); ++KeyIndex)
				{
					Keys.Add(PropertyFacade->GetKey(KeyIndex));
				}
			}
		}
	}
	return Keys;
}

float UChaosClothAssetInteractor::GetFloatValue(const FString& PropertyName, int32 LODIndex, float DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return PropertyFacade->GetValue<float>(PropertyName, DefaultValue);
		}
	}
	return DefaultValue;
}

float UChaosClothAssetInteractor::GetLowFloatValue(const FString& PropertyName, int32 LODIndex, float DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return PropertyFacade->GetLowValue<float>(PropertyName, DefaultValue);
		}
	}
	return DefaultValue;
}

float UChaosClothAssetInteractor::GetHighFloatValue(const FString& PropertyName, int32 LODIndex, float DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return PropertyFacade->GetHighValue<float>(PropertyName, DefaultValue);
		}
	}
	return DefaultValue;
}

FVector2D UChaosClothAssetInteractor::GetWeightedFloatValue(const FString& PropertyName, int32 LODIndex, FVector2D DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return FVector2D(PropertyFacade->GetWeightedFloatValue(PropertyName, FVector2f(DefaultValue)));
		}
	}
	return DefaultValue;
}

int32 UChaosClothAssetInteractor::GetIntValue(const FString& PropertyName, int32 LODIndex, int32 DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return PropertyFacade->GetValue<int32>(PropertyName, DefaultValue);
		}
	}
	return DefaultValue;
}

FVector UChaosClothAssetInteractor::GetVectorValue(const FString& PropertyName, int32 LODIndex, FVector DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return FVector(PropertyFacade->GetValue<FVector3f>(PropertyName, FVector3f(DefaultValue)));
		}
	}
	return DefaultValue;
}

FString UChaosClothAssetInteractor::GetStringValue(const FString& PropertyName, int32 LODIndex, const FString& DefaultValue) const
{
	if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			return PropertyFacade->GetStringValue(PropertyName, DefaultValue);
		}
	}
	return DefaultValue;
}

void UChaosClothAssetInteractor::SetFloatValue(const FString& PropertyName, int32 LODIndex, float Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetValue(PropertyName, Value);
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetValue(PropertyName, Value);
		}
	}
}

void UChaosClothAssetInteractor::SetLowFloatValue(const FString& PropertyName, int32 LODIndex, float Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetLowValue(PropertyName, Value);
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetLowValue(PropertyName, Value);
		}
	}
}

void UChaosClothAssetInteractor::SetHighFloatValue(const FString& PropertyName, int32 LODIndex, float Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetHighValue(PropertyName, Value);
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetHighValue(PropertyName, Value);
		}
	}
}

void UChaosClothAssetInteractor::SetWeightedFloatValue(const FString& PropertyName, int32 LODIndex, FVector2D Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetWeightedFloatValue(PropertyName, FVector2f(Value));
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetWeightedFloatValue(PropertyName, FVector2f(Value));
		}
	}
}

void UChaosClothAssetInteractor::SetIntValue(const FString& PropertyName, int32 LODIndex, int32 Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetValue(PropertyName, Value);
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetValue(PropertyName, Value);
		}
	}
}

void UChaosClothAssetInteractor::SetVectorValue(const FString& PropertyName, int32 LODIndex, FVector Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetValue(PropertyName, FVector3f(Value));
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetValue(PropertyName, FVector3f(Value));
		}
	}
}

void UChaosClothAssetInteractor::SetStringValue(const FString& PropertyName, int32 LODIndex, const FString& Value)
{
	if (LODIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < CollectionPropertyFacades.Num(); ++Index)
		{
			if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[Index].Pin())
			{
				PropertyFacade->SetStringValue(PropertyName, Value);
			}
		}
	}
	else if (CollectionPropertyFacades.IsValidIndex(LODIndex))
	{
		if (TSharedPtr<::Chaos::Softs::FCollectionPropertyFacade> PropertyFacade = CollectionPropertyFacades[LODIndex].Pin())
		{
			PropertyFacade->SetStringValue(PropertyName, Value);
		}
	}
}

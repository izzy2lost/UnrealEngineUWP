// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraCalibrationUtilsPrivate.h"

#include "CalibrationPointComponent.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace UE::CameraCalibration::Private
{
	inline static const TMap<EArucoDictionary, FString> ArucoDictionaries =
	{
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_4X4_1000, TEXT("DICT_4X4_1000")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_4X4_250, TEXT("DICT_4X4_250")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_4X4_100, TEXT("DICT_4X4_100")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_4X4_50, TEXT("DICT_4X4_50")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_5X5_1000, TEXT("DICT_5X5_1000")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_5X5_250, TEXT("DICT_5X5_250")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_5X5_100, TEXT("DICT_5X5_100")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_5X5_50, TEXT("DICT_5X5_50")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_6X6_1000, TEXT("DICT_6X6_1000")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_6X6_250, TEXT("DICT_6X6_250")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_6X6_100, TEXT("DICT_6X6_100")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_6X6_50, TEXT("DICT_6X6_50")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_7X7_1000, TEXT("DICT_7X7_1000")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_7X7_250, TEXT("DICT_7X7_250")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_7X7_100, TEXT("DICT_7X7_100")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_7X7_50, TEXT("DICT_7X7_50")),
		TPair<EArucoDictionary, FString>(EArucoDictionary::DICT_ARUCO_ORIGINAL, TEXT("DICT_ARUCO_ORIGINAL"))
	};

	EArucoDictionary GetArucoDictionaryFromName(FString Name)
	{
		TSet<EArucoDictionary> Keys;
		ArucoDictionaries.GetKeys(Keys);

		for (EArucoDictionary Key : Keys)
		{
			FString DictionaryName = GetArucoDictionaryName(Key);
			if (Name.StartsWith(DictionaryName))
			{
				return Key;
			}
		}

		return EArucoDictionary::None;
	}

	FString GetArucoDictionaryName(EArucoDictionary Dictionary)
	{
		const FString* DictionaryName = ArucoDictionaries.Find(Dictionary);

		if (DictionaryName)
		{
			FString FoundDictionaryName = *DictionaryName;
			return FoundDictionaryName;
		}

		return TEXT("");
	}

	EArucoDictionary GetArucoDictionaryForCalibrator(AActor* CalibratorActor)
	{
		if (!CalibratorActor)
		{
			return EArucoDictionary::None;
		}

		// Find all calibration components belonging to the input calibrator actor
		constexpr uint32 NumInlineAllocations = 32;
		TArray<UCalibrationPointComponent*, TInlineAllocator<NumInlineAllocations>> CalibrationComponents;
		CalibratorActor->GetComponents(CalibrationComponents);

		EArucoDictionary Dictionary = EArucoDictionary::None;
		for (const UCalibrationPointComponent* Component : CalibrationComponents)
		{
			if (!Component)
			{
				continue;
			}

			// Look up the dictionary enum value based on the name of the component (only works if it is prefixed with a dictionary name)
			Dictionary = GetArucoDictionaryFromName(Component->GetName());

			if (Dictionary != EArucoDictionary::None)
			{
				break;
			}

			// Look up the dictionary enum value based on the name of each of the component's subpoints (only works if it is prefixed with a dictionary name)
			for (const TPair<FString, FVector>& SubPoint : Component->SubPoints)
			{
				Dictionary = GetArucoDictionaryFromName(SubPoint.Key);

				if (Dictionary != EArucoDictionary::None)
				{
					break;
				}
			}
		}

		return Dictionary;
	}

	void ClearTexture(UTexture2D* Texture, FColor ClearColor)
	{
		TArray<FColor> Pixels;
		Pixels.Init(ClearColor, Texture->GetSizeX() * Texture->GetSizeY());

		void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);

		FMemory::Memcpy(TextureData, Pixels.GetData(), Pixels.Num() * sizeof(FColor));

		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
	}

	void SetTextureData(UTexture2D* Texture, const TArray<FColor>& PixelData)
	{
		void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);

		FMemory::Memcpy(TextureData, PixelData.GetData(), PixelData.Num() * sizeof(FColor));

		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
	}
}
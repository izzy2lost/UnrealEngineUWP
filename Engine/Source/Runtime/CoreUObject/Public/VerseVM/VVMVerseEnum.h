// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VVMVerseEnum.generated.h"

class UEnumCookedMetaData;

namespace uLang
{
class CEnumeration;
}

UENUM()
enum class EVerseEnumFlags : uint32
{
	None = 0x00000000u,
	NativeBound = 0x00000001u
};
ENUM_CLASS_FLAGS(EVerseEnumFlags)

UCLASS()
class COREUOBJECT_API UVerseEnum : public UEnum
{
	GENERATED_BODY()
public:
	UPROPERTY()
	EVerseEnumFlags VerseEnumFlags;

	void Initialize(TArray<TPair<FName, int64>>&& InNames, UEnum::ECppForm InCppForm)
	{
		SetEnums(InNames, InCppForm);
	}

	// UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UEnumCookedMetaData> CachedCookedMetaDataPtr;
#endif // WITH_EDITORONLY_DATA
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBlueprintVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FAvalancheBlueprintVersion::GUID(0xA248F02D, 0x294D4EA5, 0x99C9A713, 0x55277CD7);
FCustomVersionRegistration GRegisterAvalancheBlueprintVersion(FAvalancheBlueprintVersion::GUID, FAvalancheBlueprintVersion::LatestVersion,
	TEXT("AvalancheBlueprint"));

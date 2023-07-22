// Copyright Epic Games, Inc. All Rights Reserved.
#include "UObject/FortniteShaderworkObjectVersion.h"

TMap<FGuid, FGuid> FFortniteShaderworkObjectVersion::GetSystemGuids()
{
	TMap<FGuid, FGuid> SystemGuids;
	const FDevSystemGuids& DevGuids = FDevSystemGuids::Get();

	SystemGuids.Add(DevGuids.NANITE_DERIVEDDATA_VER, FGuid("3287E9A2EFCE4CB79A1FDC0E9B1CBC36"));
	SystemGuids.Add(DevGuids.STATICMESH_DERIVEDDATA_VER, FGuid("EB4D2761094040DD889CC1BE3D24E1B3"));
	SystemGuids.Add(DevGuids.MATERIALSHADERMAP_DERIVEDDATA_VER, FGuid("0820372747734716A3BAA8C0F6A40ABA"));
	return SystemGuids;
}

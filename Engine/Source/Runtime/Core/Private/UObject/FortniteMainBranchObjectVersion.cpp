// Copyright Epic Games, Inc. All Rights Reserved.
#include "UObject/FortniteMainBranchObjectVersion.h"

TMap<FGuid, FGuid> FFortniteMainBranchObjectVersion::GetSystemGuids()
{
	TMap<FGuid, FGuid> SystemGuids;
	const FDevSystemGuids& DevGuids = FDevSystemGuids::Get();

	SystemGuids.Add(DevGuids.GLOBALSHADERMAP_DERIVEDDATA_VER, FGuid("E1B72EA109BB4C2A995F0F65E5BD353D"));
	SystemGuids.Add(DevGuids.LANDSCAPE_MOBILE_COOK_VERSION, FGuid("32D02EF867C74B71A0D4E0FA41392732"));
	SystemGuids.Add(DevGuids.MATERIALSHADERMAP_DERIVEDDATA_VER, FGuid("0E6202C151DE402CA451802671AE9347"));
	SystemGuids.Add(DevGuids.NANITE_DERIVEDDATA_VER, FGuid("DB3ECDFAF27A4344ACB6B1967C21148D"));
	SystemGuids.Add(DevGuids.NIAGARASHADERMAP_DERIVEDDATA_VER, FGuid("C417A50271B3427D98785388266E3B66"));
	SystemGuids.Add(DevGuids.Niagara_LatestScriptCompileVersion, FGuid("F8D995224324470E97ED718D33939A0E"));
	SystemGuids.Add(DevGuids.SkeletalMeshDerivedDataVersion, FGuid("B9AEAA3EE7AC4FCFB3860E8F3E06DD75"));
	SystemGuids.Add(DevGuids.STATICMESH_DERIVEDDATA_VER, FGuid("1C2EBE3310464FD7995ADE1D6AB92BDC"));
	SystemGuids.Add(DevGuids.MaterialTranslationDDCVersion, FGuid("0F9C4453CDDC4B8BA1527D4942381ECA"));
	return SystemGuids;
}

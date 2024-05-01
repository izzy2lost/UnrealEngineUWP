// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGPartitionActorDesc.h"

#if WITH_EDITOR
#include "UObject/FortniteMainBranchObjectVersion.h"

void FPCGPartitionActorDesc::Init(const AActor* InActor)
{
	FPartitionActorDesc::Init(InActor);
}

void FPCGPartitionActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	FPartitionActorDesc::Serialize(Ar);

	if (Ar.IsLoading() && Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::PCGPartitionActorDesc)
	{
		// Try to get Grid Size and Coords from names with following pattern "PCGPartitionActor_{GridSize}_{X}_{Y}_UAID"
		// 
		// Naming was broken in 5.4 where "_{Z}" part of the SpawnParams.Name was converted into the FName Number part
		// in combination with the SpawnParams.NameMode being FActorSpawnParameters::ESpawnActorNameMode::Requested which ends up using FActorSpawnUtils::GetBaseName
		// which calls FName::GetPlainNameString() which removes the Number part (treats _{Z} as the FName number)
		//
		// The label was properly set so if we are dealing with a _UAID_ name then use the label to parse the GridSize/X/Y/Z
		const FString ActorNameStr = GetActorName().ToString();
		if (ActorNameStr.Contains("_UAID_"))
		{
			const FString ActorLabelStr = GetActorLabel().ToString();
			TArray<FString> Tokens;
			ActorLabelStr.ParseIntoArray(Tokens, TEXT("_"));
			if (Tokens[0] == TEXT("PCGPartitionActor") && Tokens.Num() == 5)
			{
				TTypeFromString<uint32>::FromString(GridSize, *Tokens[1]);
				TTypeFromString<int64>::FromString(GridIndexX, *Tokens[2]);
				TTypeFromString<int64>::FromString(GridIndexY, *Tokens[3]);
				TTypeFromString<int64>::FromString(GridIndexZ, *Tokens[4]);
				
				// Try to fixup the GridGuid in the PCGSubsystem using the PCGWorldActor data
				bRequiresGuidFixup = true;
			}
			else
			{
				bInvalid = true;
			}
		}
	}
}

#endif // WITH_EDITOR

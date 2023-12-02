// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithMeshUObject.h"

void FDatasmithMeshSourceModel::SerializeBulkData(FArchive& Ar, UObject* Owner)
{
	RawMeshBulkData.Serialize( Ar, Owner );
}

void UDatasmithMesh::Serialize(FArchive& Ar)
{
	Ar.SetUEVer(FPackageFileVersion::CreateUE4Version(VER_UE4_AUTOMATIC_VERSION));
	Super::Serialize( Ar );

	for ( FDatasmithMeshSourceModel& SourceModel : SourceModels )
	{
		SourceModel.SerializeBulkData( Ar, this );
	}
}

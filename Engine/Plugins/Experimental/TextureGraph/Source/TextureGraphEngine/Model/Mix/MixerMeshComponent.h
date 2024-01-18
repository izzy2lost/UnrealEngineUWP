// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Model/ModelObject.h"
#include "3D/RenderMesh.h"
#include "MixerMeshComponent.generated.h"

class RenderMesh;
typedef std::shared_ptr<RenderMesh>		RenderMeshPtr;

class MeshAsset;
typedef std::shared_ptr<MeshAsset>		MeshAssetPtr;

UCLASS(Blueprintable, BlueprintType)
class TEXTUREGRAPHENGINE_API UMixerMeshComponent : public UModelObject
{
	GENERATED_BODY()

private:
	RenderMeshPtr					MeshObj;					/// Mesh extracted from the asset pack

public:
	
	void							SetMesh(RenderMeshPtr mesh);

protected:
	UPROPERTY()
	int32							MeshType;

	UPROPERTY()
	float							GroundSizeIndex;

	UPROPERTY()
	FString							CustomMeshPath;

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	FVector3f						CustomMeshScale;

public:
	FORCEINLINE int32				GetMeshType() const { return MeshType; }

	FORCEINLINE RenderMeshPtr		GetMesh() const { return MeshObj; }
};

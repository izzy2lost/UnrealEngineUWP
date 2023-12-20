// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h" 
#include "MixInterface.h"
#include "MixerMeshComponent.h"

#include <memory>
#include <vector> 

#include "Mix.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMix, All, All);

class UMixerProject;
class UMixParameters;

class RenderMesh;
typedef std::shared_ptr<RenderMesh>	RenderMeshPtr;

UCLASS(Blueprintable, BlueprintType)
class MIXERENGINE_API UMix : public UMixInterface
{
	GENERATED_BODY()

public:
	

private:
	static UMix*						GNullMix;					/// This is a generic mix that is used in various places and rendering actions that
																	/// often require a Mix object. This is initialised in the first call to the public 
																	/// static function below. 
	static const FString				GRootSectionName;

public:
	static UMix*						NullMix();

protected:	
	UPROPERTY()
	TObjectPtr<UMixerMeshComponent>		MeshComponent;

public:
	virtual								~UMix() override;
	
	virtual void						Update(MixUpdateCyclePtr Cycle) override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Inline Functions
	//////////////////////////////////////////////////////////////////////////
	virtual UMixerMeshComponent*		GetMeshComponent() const override { return MeshComponent; }
};

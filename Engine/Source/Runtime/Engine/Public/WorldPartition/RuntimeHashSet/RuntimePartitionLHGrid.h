// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionLHGrid.generated.h"

UCLASS()
class URuntimePartitionLHGrid : public URuntimePartition
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~ Begin UObject Interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.

	//~ Begin URuntimePartition interface
	virtual void SetDefaultValues() override;
	virtual bool SupportsHLODs() const override;
	virtual bool IsValidGrid(FName GridName) const override;
	virtual bool GenerateStreaming(const TArray<const IStreamingGenerationContext::FActorSetInstance*>& ActorSetInstances, TArray<FCellDesc>& OutRuntimeCellDescs) override;
	//~ End URuntimePartition interface
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	uint32 CellSize;
#endif
};
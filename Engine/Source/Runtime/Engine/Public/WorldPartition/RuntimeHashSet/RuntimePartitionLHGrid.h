// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartition/RuntimeHashSet/RuntimePartition.h"
#include "RuntimePartitionLHGrid.generated.h"

UCLASS()
class URuntimePartitionLHGrid : public URuntimePartition
{
	GENERATED_BODY()

	friend class UWorldPartitionRuntimeHashSet;

public:
#if WITH_EDITOR
	//~ Begin UObject Interface.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.

	//~ Begin URuntimePartition interface
	virtual bool SupportsHLODs() const override { return true; }
	virtual void InitHLODRuntimePartitionFrom(const URuntimePartition* InRuntimePartition, int32 InHLODIndex);
	virtual void SetDefaultValues() override;
	virtual bool IsValidPartitionTokens(const TArray<FName>& InPartitionTokens) const override;
	virtual bool GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingResult& OutResult) override;
	//~ End URuntimePartition interface
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = RuntimeSettings)
	uint32 CellSize;
#endif
};
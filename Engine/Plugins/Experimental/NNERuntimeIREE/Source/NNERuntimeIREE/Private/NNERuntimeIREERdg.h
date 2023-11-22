// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeRDG.h"

#include "NNERuntimeIREERdg.generated.h"

UCLASS()
class UNNERuntimeIREERdg : public UObject, public INNERuntime, public INNERuntimeRDG
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	UNNERuntimeIREERdg() {};
	virtual ~UNNERuntimeIREERdg() = default;

	//~ Begin INNERuntime Interface
	virtual FString GetRuntimeName() const override;
	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	//~ End INNERuntime Interface

	//~ Begin INNERuntimeRdg Interface
	virtual bool CanCreateModelRDG(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TSharedPtr<UE::NNE::IModelRDG> CreateModelRDG(TObjectPtr<UNNEModelData> ModelData) override;
	//~ End INNERuntimeRdg Interface

	bool IsAvailable() const;
};
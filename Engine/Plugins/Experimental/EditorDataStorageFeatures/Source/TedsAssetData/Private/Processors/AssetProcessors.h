// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"

#include "AssetProcessors.generated.h"

class ITypedElementDataStorageInterface;

UCLASS()
class UTedsAssetDataFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTedsAssetDataFactory() override = default;

	virtual void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void PreRegister(ITypedElementDataStorageInterface& DataStorage) override;
	virtual void PreShutdown(ITypedElementDataStorageInterface& DataStorage) override;

protected:

	void OnSetFolderColor(const FString& Path, ITypedElementDataStorageInterface* DataStorage);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSCCProcessors.generated.h"

UCLASS()
class UTypedElementSCCFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementSCCFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) const override;
	
};

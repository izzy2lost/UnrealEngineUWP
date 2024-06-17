// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Interfaces/DataflowPhysicsObject.h"
#include "DataflowCollisionObject.generated.h"

/**
 * Dataflow collision object proxy (PT)
 */
USTRUCT()
struct DATAFLOWSIMULATION_API FDataflowCollisionObjectProxy : public FDataflowPhysicsObjectProxy
{
	GENERATED_BODY()
	
	FDataflowCollisionObjectProxy();
	virtual ~FDataflowCollisionObjectProxy() override = default;

	/** Get the proxy script struct */
	virtual const UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
};

UINTERFACE()
class DATAFLOWSIMULATION_API UDataflowCollisionObjectInterface : public UDataflowPhysicsObjectInterface
{
	GENERATED_BODY()
};

/**
 * Dataflow collision object interface to send/receive datas (GT <-> PT)
 */
class DATAFLOWSIMULATION_API IDataflowCollisionObjectInterface : public IDataflowPhysicsObjectInterface
{
	GENERATED_BODY()
public:
	IDataflowCollisionObjectInterface();

	/** Get the simulation type */
	virtual FString GetSimulationType() const override
	{
		return FDataflowCollisionObjectProxy::StaticStruct()->GetName();
	}
};

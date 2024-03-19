// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"

#include "DataflowConnection.generated.h"

class FProperty;
struct FDataflowNode;


namespace Dataflow
{

	template<class T> inline DATAFLOWCORE_API FName GraphConnectionTypeName();
	template<class T> inline DATAFLOWCORE_API T DeepCopy(const T&);

	struct FPin
	{
		enum class EDirection : uint8 {
			NONE = 0,
			INPUT,
			OUTPUT
		};
		EDirection Direction;
		FName Type;
		FName Name;
	};
}

//
// Input Output Base
//
USTRUCT()
struct FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

protected:
	Dataflow::FPin::EDirection Direction;
	FName Type;
	FName Name;
	FDataflowNode* OwningNode = nullptr;
	const FProperty* Property = nullptr;
	FGuid  Guid;

	friend struct FDataflowNode;

protected:
	DATAFLOWCORE_API bool IsOwningNodeEnabled() const;
	DATAFLOWCORE_API FGuid GetOwningNodeGuid() const;
	DATAFLOWCORE_API uint32 GetOwningNodeValueHash() const;

public:
	FDataflowConnection() {};
	DATAFLOWCORE_API FDataflowConnection(Dataflow::FPin::EDirection Direction, FName InType, FName InName, FDataflowNode* OwningNode = nullptr, const FProperty* InProperty = nullptr, FGuid InGuid = FGuid::NewGuid());
	virtual ~FDataflowConnection() {};

	FDataflowNode* GetOwningNode() { return OwningNode; }
	const FDataflowNode* GetOwningNode() const { return OwningNode; }

	const FProperty* GetProperty() const { return Property; }

	Dataflow::FPin::EDirection GetDirection() const { return Direction; }
	DATAFLOWCORE_API uint32 GetOffset() const;

	FName GetType() const { return Type; }

	FGuid GetGuid() const { return Guid; }
	void SetGuid(FGuid InGuid) { Guid = InGuid; }

	FName GetName() const { return Name; }
	void SetName(FName InName) { Name = InName; }

	void* RealAddress() const { ensure(OwningNode);  return (void*)((size_t)OwningNode + (size_t)GetOffset()); };
	Dataflow::FContextCacheKey CacheKey() const { return GetTypeHash(Guid); };

	virtual bool AddConnection(FDataflowConnection* In) { return false; };
	virtual bool RemoveConnection(FDataflowConnection* In) { return false; }

	template<class T>
	bool IsA(const T* InVar) const
	{
		return (size_t)OwningNode + (size_t)GetOffset() == (size_t)InVar;
	}

	virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) {};
};

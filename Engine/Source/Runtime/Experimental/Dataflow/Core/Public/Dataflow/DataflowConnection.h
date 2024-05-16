// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowTypePolicy.h"
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
		bool bHidden = false;
	};

	struct FConnectionParameters
	{
		FConnectionParameters(FName InType = FName(""), FName InName = FName(""), FDataflowNode* InOwner = nullptr, const FProperty* InProperty = nullptr, uint32 InOffset = INDEX_NONE, FGuid InGuid = FGuid::NewGuid())
			: Type(InType)
			, Name(InName)
			, Owner(InOwner)
			, Property(InProperty)
			, Offset(InOffset)
			, Guid(InGuid)
		{}

		FName Type;
		FName Name;
		FDataflowNode* Owner = nullptr;
		const FProperty* Property = nullptr;
		uint32 Offset = INDEX_NONE;
		FGuid Guid;
	};

	class FGraph;
}

//
// Input Output Base
//
USTRUCT()
struct FDataflowConnection
{
	GENERATED_USTRUCT_BODY()

protected:
	FName Type;
	FName Name;
	FDataflowNode* OwningNode = nullptr;
	const FProperty* Property = nullptr;
	FGuid  Guid;
	IDataflowTypePolicy* TypePolicy = nullptr;
	uint32 Offset;
	Dataflow::FPin::EDirection Direction;
	bool bIsAnyType:1 = false;
	bool bHasConcreteType : 1 = false;
	bool bCanHidePin:1 = false;
	bool bPinIsHidden:1 = false;

	friend struct FDataflowNode;
	friend class Dataflow::FGraph;

protected:
	DATAFLOWCORE_API bool IsOwningNodeEnabled() const;

	/** this should only be used for serialization */
	DATAFLOWCORE_API void SetAsAnyType(bool bAnyType, const FName& ConcreteType);

	/** this should only be used for serialization - for support of old simple TArray types*/
	DATAFLOWCORE_API void ForceSimpleType(FName InType);
	DATAFLOWCORE_API void FixAndPropagateType();
	DATAFLOWCORE_API virtual void FixAndPropagateType(FName InType) { ensure(false); }

	/** 
	* Set the concrete type of an anytype connection 
	* @return true if the typewas effectivelly changed
	*/
	DATAFLOWCORE_API bool SetConcreteType(FName InType);

public:
	FDataflowConnection() {};
	UE_DEPRECATED(5.5, "Deprecated constructor : use FConnectionParameters to pass parameters")
	DATAFLOWCORE_API FDataflowConnection(Dataflow::FPin::EDirection Direction, FName InType, FName InName, FDataflowNode* OwningNode = nullptr, const FProperty* InProperty = nullptr, FGuid InGuid = FGuid::NewGuid());
	DATAFLOWCORE_API FDataflowConnection(Dataflow::FPin::EDirection Direction, const Dataflow::FConnectionParameters& Params);
	virtual ~FDataflowConnection() {};

	FDataflowNode* GetOwningNode() { return OwningNode; }
	const FDataflowNode* GetOwningNode() const { return OwningNode; }

	DATAFLOWCORE_API FGuid GetOwningNodeGuid() const;
	DATAFLOWCORE_API uint32 GetOwningNodeValueHash() const;

	const FProperty* GetProperty() const { return Property; }

	Dataflow::FPin::EDirection GetDirection() const { return Direction; }
	uint32 GetOffset() const { return Offset; }

	FName GetType() const { return Type; }

	FGuid GetGuid() const { return Guid; }
	void SetGuid(FGuid InGuid) { Guid = InGuid; }

	FName GetName() const { return Name; }
	void SetName(FName InName) { Name = InName; }

	void* RealAddress() const { ensure(OwningNode);  return (void*)((size_t)OwningNode + (size_t)GetOffset()); };
	Dataflow::FContextCacheKey CacheKey() const { return GetTypeHash(Guid); };

	virtual bool AddConnection(FDataflowConnection* In) { return false; };
	virtual bool RemoveConnection(FDataflowConnection* In) { return false; }

	DATAFLOWCORE_API bool IsAnyType() const { return bIsAnyType; }
	DATAFLOWCORE_API static bool IsAnyType(const FName& InType);
	DATAFLOWCORE_API bool HasConcreteType() const { return bHasConcreteType; }
	DATAFLOWCORE_API void SetTypePolicy(IDataflowTypePolicy* InTypePolicy);
	DATAFLOWCORE_API bool SupportsType(FName InType) const;

	template<class T>
	bool IsA(const T* InVar) const
	{
		return (size_t)OwningNode + (size_t)GetOffset() == (size_t)InVar;
	}

	virtual void Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp = Dataflow::FTimestamp::Current()) {};

	bool GetCanHidePin() const { return bCanHidePin; }
	bool GetPinIsHidden() const { return bCanHidePin && bPinIsHidden; }
	void SetCanHidePin(bool bInCanHidePin) { bCanHidePin = bInCanHidePin; }
	void SetPinIsHidden(bool bInPinIsHidden) { bPinIsHidden = bInPinIsHidden; }

private:
	void InitFromType();
};

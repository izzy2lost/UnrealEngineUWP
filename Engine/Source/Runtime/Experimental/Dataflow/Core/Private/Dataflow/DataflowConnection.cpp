// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConnection.h"

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowConnection)

FDataflowConnection::FDataflowConnection(Dataflow::FPin::EDirection InDirection, FName InType, FName InName, FDataflowNode* InOwningNode, const FProperty* InProperty, FGuid InGuid)
	: Direction(InDirection)
	, Type(InType)
	, Name(InName)
	, OwningNode(InOwningNode)
	, Property(InProperty)
	, Guid(InGuid)
{
	bIsAnyType = FDataflowConnection::IsAnyType(Type);
}

uint32 FDataflowConnection::GetOffset() const
{
	if (ensure(OwningNode))
	{
		return OwningNode->GetPropertyOffset(Name);
	}
	return INDEX_NONE;
}


bool FDataflowConnection::IsOwningNodeEnabled() const
{
	return (OwningNode && OwningNode->bActive);
}

FGuid FDataflowConnection::GetOwningNodeGuid() const
{
	return OwningNode ? OwningNode->GetGuid() : FGuid();
}

uint32 FDataflowConnection::GetOwningNodeValueHash() const
{
	return OwningNode ? OwningNode->GetValueHash() : 0;
}

bool FDataflowConnection::IsAnyType(const FName& InType)
{
	return (InType == FDataflowAnyType::TypeName);
}

void FDataflowConnection::SetAsAnyType(bool bAnyType, const FName& ConcreteType)
{
	bIsAnyType = bAnyType;
	if (bIsAnyType)
	{
		Type = ConcreteType;
	}
}

void FDataflowConnection::SetConcreteType(FName InType)
{
	// Can only change from AnyType to a concrete type
	if (ensure(IsAnyType() && !IsAnyType(InType)))
	{
		Type = InType;
	}
}

void FDataflowConnection::ForceSimpleType(FName InType)
{
	check(Type.ToString().StartsWith(InType.ToString()));
	Type = InType;
}

void FDataflowConnection::FixAndPropagateType()
{
	FString ExtendedType;
	const FString CPPType = Property->GetCPPType(&ExtendedType);
	FName FixedType(CPPType + ExtendedType);

	FixAndPropagateType(FixedType);
}
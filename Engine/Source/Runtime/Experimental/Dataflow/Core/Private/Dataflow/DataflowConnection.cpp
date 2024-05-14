// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowConnection.h"

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowConnection)

FDataflowConnection::FDataflowConnection(Dataflow::FPin::EDirection InDirection, FName InType, FName InName, FDataflowNode* InOwningNode, const FProperty* InProperty, FGuid InGuid)
	: Type(InType)
	, Name(InName)
	, OwningNode(InOwningNode)
	, Property(InProperty)
	, Guid(InGuid)
	, Direction(InDirection)
{
	InitFromType();
}

FDataflowConnection::FDataflowConnection(Dataflow::FPin::EDirection InDirection, const Dataflow::FConnectionParameters& Params)
	: Type(Params.Type)
	, Name(Params.Name)
	, OwningNode(Params.Owner)
	, Property(Params.Property)
	, Guid(Params.Guid)
	, Direction(InDirection)
{
	InitFromType();
}

void FDataflowConnection::InitFromType()
{
	bIsAnyType = false;
	bHasConcreteType = true;
	if (Property && Property->GetClass()->IsChildOf(FStructProperty::StaticClass()))
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct->IsChildOf<FDataflowAnyType>())
			{
				Type = FDataflowAnyType::TypeName;
				bIsAnyType = true;
				bHasConcreteType = false;
			}
		}
	}
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
		bHasConcreteType = !IsAnyType(ConcreteType);
	}
}

bool FDataflowConnection::SupportsType(FName InType) const
{
	if (IsAnyType(InType))
	{
		return false;
	}
	// resort to policy only if the concrete type is not defined ( case of anytype connection )
	if (bIsAnyType && !bHasConcreteType)
	{
		return TypePolicy ? TypePolicy->SupportsType(InType) : true;
	}
	// todo : in the future we could also check for pointer compatibility
	return (InType == GetType());
}

bool FDataflowConnection::SetConcreteType(FName InType)
{
	// Can only change from AnyType to a concrete type
	if (Type != InType && ensure(!bHasConcreteType))
	{
		if (ensure(SupportsType(InType)))
		{
			Type = InType;
			bHasConcreteType = true;
			return true;
		}
	}
	return false;
}

void FDataflowConnection::SetTypePolicy(IDataflowTypePolicy* InTypePolicy)
{
	// for now only allow setting it once
	if (ensure(TypePolicy == nullptr))
	{
		TypePolicy = InTypePolicy;
	}
}

void FDataflowConnection::ForceSimpleType(FName InType)
{
	check(Type.ToString().StartsWith(InType.ToString()));
	Type = InType;
	bHasConcreteType = true;
}

void FDataflowConnection::FixAndPropagateType()
{
	FString ExtendedType;
	const FString CPPType = Property->GetCPPType(&ExtendedType);
	FName FixedType(CPPType + ExtendedType);

	FixAndPropagateType(FixedType);
}
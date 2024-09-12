// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Iris/ReplicationSystem/NetSubObjectFactory.h"

#if UE_WITH_IRIS

#include "GameFramework/Actor.h"

#include "HAL/UnrealMemory.h"

#include "Iris/Core/IrisLog.h"

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"

#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/ObjectNetSerializer.h"

#include "UObject/Package.h"

#include "Net/DataBunch.h"

#endif // UE_WITH_IRIS

#if UE_WITH_IRIS

TUniquePtr<UE::Net::FNetObjectCreationHeader> UNetSubObjectFactory::CreateAndFillHeader(UE::Net::FNetRefHandle Handle)
{
	using namespace UE::Net;

	TUniquePtr<FNetObjectCreationHeader> Header(new FNetSubObjectCreationHeader);
	FNetSubObjectCreationHeader* SubObjectHeader = static_cast<FNetSubObjectCreationHeader*>(Header.Get());

	UObject* SubObject = Bridge->GetReplicatedObject(Handle);
	if (!SubObject)
	{
		ensureMsgf(SubObject, TEXT("UNetSubObjectFactory::CreateAndFillHeader could not find object tied to handle: %s"), *Bridge->PrintObjectFromNetRefHandle(Handle));
		return nullptr;
	}

	FNetObjectReference ObjectRef = Bridge->GetOrCreateObjectReference(SubObject);

	SubObjectHeader->bIsDynamic = ObjectRef.GetRefHandle().IsDynamic();
	SubObjectHeader->bIsNameStableForNetworking = SubObject->IsNameStableForNetworking();
	SubObjectHeader->ObjectReference = ObjectRef;

	if (!SubObjectHeader->bIsDynamic || SubObjectHeader->bIsNameStableForNetworking)
	{
		// No more information needed since we don't need to spawn the object on the remote.
		return Header;
	}

	check(SubObject->NeedsLoadForClient() ); // We have no business sending this unless the client can load
	check(SubObject->GetClass()->NeedsLoadForClient());	// We have no business sending this unless the client can load

	SubObjectHeader->ObjectClassReference = Bridge->GetOrCreateObjectReference(SubObject->GetClass());

	// Find the right Outer
	UObject* OuterObject = SubObject->GetOuter();	
	if (OuterObject == GetTransientPackage())
	{
		SubObjectHeader->bOuterIsTransientLevel = true;
	}
	else 
	{
		FNetRefHandle RootObjectHandle = Bridge->GetRootObjectOfSubObject(Handle);
		UObject* RootObject = Bridge->GetReplicatedObject(RootObjectHandle);
		check(RootObject);

		if (OuterObject == RootObject)
		{
			SubObjectHeader->bOuterIsRootObject = true;
		}
		else
		{
			SubObjectHeader->OuterReference = Bridge->GetOrCreateObjectReference(OuterObject);

			if (!SubObjectHeader->OuterReference.IsValid())
			{
				UE_LOG(LogIris, Error, TEXT("UNetSubObjectFactory::CreateAndFillHeader %s could not create NetReference to Outer %s"), *Bridge->PrintObjectFromNetRefHandle(Handle), *GetNameSafe(OuterObject));
			}
		}
	}

	return Header;
}

TUniquePtr<UE::Net::FNetObjectCreationHeader> UNetSubObjectFactory::CreateAndDeserializeHeader(const UE::Net::FCreationHeaderContext& Context)
{
	using namespace UE::Net;

	TUniquePtr<FNetSubObjectCreationHeader> Header(new FNetSubObjectCreationHeader);
	Header->Deserialize(Context);
	return Header;
}


UNetSubObjectFactory::FInstantiateResult UNetSubObjectFactory::InstantiateReplicatedObjectFromHeader(const FInstantiateContext& Context, const UE::Net::FNetObjectCreationHeader* Header)
{
	using namespace UE::Net;

	const FNetSubObjectCreationHeader* SubObjectHeader = static_cast<const FNetSubObjectCreationHeader*>(Header);

	if (!SubObjectHeader->bIsDynamic || SubObjectHeader->bIsNameStableForNetworking)
	{
		// Resolve by finding object relative to owner. We do not allow this object to be destroyed.
		UObject* SubObject = Bridge->ResolveObjectReference(SubObjectHeader->ObjectReference, Context.ResolveContext);

		if (!SubObject)
		{
			if (!SubObjectHeader->bIsDynamic)
			{
				UE_LOG(LogIris, Error, TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader %s: Failed to find static object reference for static SubObject: %s, Owner: %s, RootObject: %s"), *Context.Handle.ToString(), *Bridge->DescribeObjectReference(SubObjectHeader->ObjectReference, Context.ResolveContext), *Bridge->PrintObjectFromNetRefHandle(Context.RootObjectOfSubObject), *GetPathNameSafe(Bridge->GetReplicatedObject(Context.RootObjectOfSubObject)));
			}
			else if (SubObjectHeader->bIsNameStableForNetworking)
			{
				UE_LOG(LogIris, Error, TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader %s: Failed to find stable name reference of dynamic SubObject: %s, Owner: %s, RootObject: %s"), *Context.Handle.ToString(), *Bridge->DescribeObjectReference(SubObjectHeader->ObjectReference, Context.ResolveContext), *Bridge->PrintObjectFromNetRefHandle(Context.RootObjectOfSubObject), *GetPathNameSafe(Bridge->GetReplicatedObject(Context.RootObjectOfSubObject)));
			}

			return FInstantiateResult();
		}

		UE_LOG(LogIris, Verbose, TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader %s: Found static SubObject using path %s"), *Context.Handle.ToString(), ToCStr(SubObject->GetPathName()));

		FInstantiateResult Result { .Instance = SubObject };
		return Result;
	}

	// For dynamic objects we have to spawn them
		
	UObject* RootObject = Bridge->GetReplicatedObject(Context.RootObjectOfSubObject);
	AActor* RootActor = CastChecked<AActor>(RootObject);
			
	// Find the proper Outer
	UObject* OuterObject = nullptr;
	if (SubObjectHeader->bOuterIsTransientLevel)
	{
		OuterObject = GetTransientPackage();
	}
	else if (SubObjectHeader->bOuterIsRootObject)
	{
		OuterObject = RootActor;
	}				
	else
	{
		OuterObject = Bridge->ResolveObjectReference(SubObjectHeader->OuterReference, Context.ResolveContext);

		if (!OuterObject)
		{
			UE_LOG(LogIris, Error, TEXT("BeginInstantiateFromRemote Failed to find Outer %s for dynamic subobject %s"), *Bridge->DescribeObjectReference(SubObjectHeader->OuterReference, Context.ResolveContext), *Bridge->DescribeObjectReference(SubObjectHeader->ObjectReference, Context.ResolveContext))

			// Fallback to the rootobject instead
			OuterObject = RootActor;
		}
	}

	// We need to spawn the subobject
	UObject* SubObjClassObj = Bridge->ResolveObjectReference(SubObjectHeader->ObjectClassReference, Context.ResolveContext);
	UClass * SubObjClass = Cast<UClass>(SubObjClassObj);

	// Try to spawn SubObject
	UObject* SubObj = NewObject<UObject>(OuterObject, SubObjClass);

	// Sanity check some things
	checkf(SubObj != nullptr, TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader: Subobject is NULL after instantiating. Class: %s, Outer %s, Actor %s"), *GetNameSafe(SubObjClass), *GetNameSafe(OuterObject), *GetNameSafe(RootActor));
	checkf(!OuterObject || SubObj->IsIn(OuterObject), TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader: Subobject is not in Outer. SubObject: %s, Outer %s, Actor %s"), *SubObj->GetName(), *GetNameSafe(OuterObject), *GetNameSafe(RootActor));
	checkf(Cast<AActor>(SubObj) == nullptr, TEXT("UNetSubObjectFactory::InstantiateNetObjectFromHeader: Subobject is an Actor. SubObject: %s, Outer %s, Actor %s"), *SubObj->GetName(), *GetNameSafe(OuterObject), *GetNameSafe(RootActor));

	FInstantiateResult Result { .Instance = SubObj };

	// We must defer call OnSubObjectCreatedFromReplication after the state has been applied to the owning actor in order to behave like old system.
	Result.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::ShouldCallSubObjectCreatedFromReplication;

	// Created objects may be destroyed.
	Result.Flags |= EReplicationBridgeCreateNetRefHandleResultFlags::AllowDestroyInstanceFromRemote;
			
	return Result;
}

bool UNetSubObjectFactory::SerializeHeader(const UE::Net::FCreationHeaderContext& Context, const UE::Net::FNetObjectCreationHeader* Header)
{
	using namespace UE::Net;
	const FNetSubObjectCreationHeader* SubObjectHeader = static_cast<const FNetSubObjectCreationHeader*>(Header);
	return SubObjectHeader->Serialize(Context);
}

//------------------------------------------------------------------------

namespace UE::Net
{

FString FNetSubObjectCreationHeader::ToString() const
{
	return FString::Printf(TEXT("FNetSubObjectCreationHeader (ProtocolId:0x%x):\n\t"
								"ObjectReference=%s\n\t"
								"ObjectClassReference=%s\n\t"
								"OuterReference=%s\n\t"
								"bIsDynamic=%u\n\t"
								"bIsNameStableForNetworking=%u\n\t"
								"bUsePersistenLevel=%u\n\t"
								"bOuterIsTransientLevel=%u\n\t"
								"bOuterIsRootObject=%u\n\t"),
								GetProtocolId(),
								*ObjectReference.ToString(),
								*ObjectClassReference.ToString(),
								*OuterReference.ToString(),
								bIsDynamic,
								bIsNameStableForNetworking,
								bUsePersistentLevel,
								bOuterIsTransientLevel,
								bOuterIsRootObject);

}

bool FNetSubObjectCreationHeader::Serialize(const FCreationHeaderContext& Context) const
{
	FNetBitStreamWriter* Writer = Context.Serialization.GetBitStreamWriter();

	// Write required references to either find or instantiate the subobject
	if (Writer->WriteBool(bIsDynamic))
	{
		if (Writer->WriteBool(bIsNameStableForNetworking))
		{
			WriteFullNetObjectReference(Context.Serialization, ObjectReference);
		}
		else
		{
			WriteFullNetObjectReference(Context.Serialization, ObjectClassReference);

			if (!Writer->WriteBool(bOuterIsTransientLevel))
			{
				if (!Writer->WriteBool(bOuterIsRootObject))
				{
					WriteFullNetObjectReference(Context.Serialization, OuterReference);
				}
			}
		}
	}
	else
	{
		WriteFullNetObjectReference(Context.Serialization, ObjectReference);
	}

	return true;
}

bool FNetSubObjectCreationHeader::Deserialize(const FCreationHeaderContext& Context)
{
	FNetBitStreamReader* Reader = Context.Serialization.GetBitStreamReader();

	bIsDynamic = Reader->ReadBool();
	if (bIsDynamic)
	{
		bIsNameStableForNetworking = Reader->ReadBool();
		if (bIsNameStableForNetworking)
		{
			ReadFullNetObjectReference(Context.Serialization, ObjectReference);
		}
		else
		{
			ReadFullNetObjectReference(Context.Serialization, ObjectClassReference);

			bOuterIsTransientLevel = Reader->ReadBool();
			if (!bOuterIsTransientLevel)
			{
				bOuterIsRootObject = Reader->ReadBool();
				if (!bOuterIsRootObject)
				{
					ReadFullNetObjectReference(Context.Serialization, OuterReference);
				}
			}
		}
	}
	else
	{
		ReadFullNetObjectReference(Context.Serialization, ObjectReference);
	}

	return true;
}

} // end namespace UE::Net

#endif //UE_WITH_IRIS
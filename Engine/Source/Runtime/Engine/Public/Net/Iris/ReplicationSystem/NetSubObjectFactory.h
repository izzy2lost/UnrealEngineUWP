// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/NetObjectFactory.h"

#if UE_WITH_IRIS
#include "Iris/Core/NetObjectReference.h"
#endif

#include "NetSubObjectFactory.generated.h"

/**
 * Responsible for creating headers allowing remote factories to spawn replicated actors
 */
UCLASS()
class UNetSubObjectFactory : public UNetObjectFactory
{
	GENERATED_BODY()

#if UE_WITH_IRIS

public:

	static FName GetFactoryName() { return TEXT("NetSubObjectFactory"); }

	virtual FInstantiateResult InstantiateReplicatedObjectFromHeader(const FInstantiateContext& Context, const UE::Net::FNetObjectCreationHeader* Header) override;

protected:

	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndFillHeader(UE::Net::FNetRefHandle Handle) override;
	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndDeserializeHeader(const UE::Net::FCreationHeaderContext& Context) override;

	virtual bool SerializeHeader(const UE::Net::FCreationHeaderContext& Context, const UE::Net::FNetObjectCreationHeader* Header)  override;
#endif // UE_WITH_IRIS
};


namespace UE::Net
{
#if UE_WITH_IRIS
/**
* Header information representing subobjects
*/
class FNetSubObjectCreationHeader : public FNetObjectCreationHeader
{
public:

	bool Serialize(const FCreationHeaderContext& Context) const;
	bool Deserialize(const FCreationHeaderContext& Context);

	virtual FString ToString() const override;

	FNetSubObjectCreationHeader()
		: bIsDynamic(false)
		, bIsNameStableForNetworking(false)
		, bUsePersistentLevel(false)
		, bOuterIsTransientLevel(false)
		, bOuterIsRootObject(false)
	{}

	FNetObjectReference ObjectReference; // Only for static objects
	FNetObjectReference ObjectClassReference; // Only for dynamic objects
	FNetObjectReference OuterReference; // Optional: Outer ref sent only for dynamic subobjects.
	uint8 bIsDynamic : 1;
	uint8 bIsNameStableForNetworking : 1;
	uint8 bUsePersistentLevel : 1;
	uint8 bOuterIsTransientLevel : 1;	// When set the OuterReference was not sent because the Outer is the default transient level.
	uint8 bOuterIsRootObject : 1;		// When set the OuterReference was not sent because the Outer is the known RootObject.

};
#endif // UE_WITH_IRIS
} // end namespace UE::Net
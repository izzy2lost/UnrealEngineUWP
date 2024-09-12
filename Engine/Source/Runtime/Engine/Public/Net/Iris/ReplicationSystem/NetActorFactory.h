// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/NetObjectFactory.h"

#if UE_WITH_IRIS
#include "Iris/Core/NetObjectReference.h"
#endif

#include "NetActorFactory.generated.h"

namespace UE::Net::Private
{
	enum class EActorNetSpawnInfoFlags : uint32;
}


namespace UE::Net
{

#if UE_WITH_IRIS

/**
* Header information representing dynamic actors
*/
class FActorNetCreationHeader : public FNetObjectCreationHeader
{
public:

	struct FActorNetSpawnInfo
	{
		FActorNetSpawnInfo()
		: Location(EForceInit::ForceInitToZero)
		, Rotation(EForceInit::ForceInitToZero)
		, Scale(FVector::OneVector)
		, Velocity(EForceInit::ForceInitToZero)
		{}

		FVector Location;
		FRotator Rotation;
		FVector Scale;
		FVector Velocity;
	};

	FActorNetSpawnInfo SpawnInfo;

	FNetObjectReference ObjectReference;
	FNetObjectReference ArchetypeReference;
	FNetObjectReference LevelReference; // Only when bUsePersistentLevel is false
	
	bool bIsDynamic = false;
	bool bUsePersistentLevel = false;

	TArray<uint8> CustomCreationData;
	uint16 CustomCreationDataBitCount = 0;
	
	bool Serialize(const FCreationHeaderContext& Context, UE::Net::Private::EActorNetSpawnInfoFlags SpawnFlags, const FActorNetSpawnInfo& DefaultSpawnInfo) const;
	bool Deserialize(const FCreationHeaderContext& Context, const FActorNetSpawnInfo& DefaultSpawnInfo);

	virtual FString ToString() const override;
};

#endif // UE_WITH_IRIS

} // end namespace UE::Net

/**
 * Responsible for creating headers allowing remote factories to spawn replicated actors
 */
UCLASS()
class UNetActorFactory : public UNetObjectFactory
{
	GENERATED_BODY()

#if UE_WITH_IRIS

public:

	static FName GetFactoryName() { return TEXT("NetActorFactory"); }

	virtual void OnInit() override;

	virtual FInstantiateResult InstantiateReplicatedObjectFromHeader(const FInstantiateContext& Context, const UE::Net::FNetObjectCreationHeader* Header) override;

	virtual void PostInstantiation(const FPostInstantiationContext& Context) override;

protected:

	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndFillHeader(UE::Net::FNetRefHandle Handle) override;
	virtual TUniquePtr<UE::Net::FNetObjectCreationHeader> CreateAndDeserializeHeader(const UE::Net::FCreationHeaderContext& Context) override;

	virtual bool SerializeHeader(const UE::Net::FCreationHeaderContext& Context, const UE::Net::FNetObjectCreationHeader* Header)  override;

private:

	UE::Net::Private::EActorNetSpawnInfoFlags SpawnInfoFlags;

	const UE::Net::FActorNetCreationHeader::FActorNetSpawnInfo DefaultSpawnInfo;

#endif // UE_WITH_IRIS
};




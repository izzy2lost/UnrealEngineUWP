// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanComponentBase.h"
#include "MetaHumanComponentUE.generated.h"


UCLASS(Blueprintable, ClassGroup = "Animation", HideCategories = (Navigation, Variable, Sockets, Tags, Activation, Cooking, Events, ComponentTick, ComponentReplication, AssetUserData, Replication), meta = (BlueprintSpawnableComponent, DisplayName = "MetaHuman Component"))
class UMetaHumanComponentUE
	: public UMetaHumanComponentBase
{
	GENERATED_BODY()

public:
	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	// End UActorComponent interface

private:
	void ConnectBodyPartAnimBPVariables(const FMetaHumanCustomizableBodyPart& BodyPart) const;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/UnrealEditor/HideObjectsNotInWorldLogic.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertClientSharedSlate
{
	FHideObjectsNotInWorldLogic::FHideObjectsNotInWorldLogic()
	{
		if (ensure(GEngine))
		{
			GEngine->OnWorldAdded().AddRaw(this, &FHideObjectsNotInWorldLogic::OnWorldAdded);
			GEngine->OnWorldDestroyed().AddRaw(this, &FHideObjectsNotInWorldLogic::OnWorldDestroyed);
		}
	}

	FHideObjectsNotInWorldLogic::~FHideObjectsNotInWorldLogic()
	{
		if (GEngine)
		{
			GEngine->OnWorldAdded().RemoveAll(this);
			GEngine->OnWorldDestroyed().RemoveAll(this);
		}
	}

	bool FHideObjectsNotInWorldLogic::ShouldShowObject(const FSoftObjectPath& ObjectPath) const
	{
		if (!GWorld)
		{
			return false;
		}

		// If it does not resolve, it's no in GWorld (the world should load all the actors in it).
		const UObject* Object = ObjectPath.ResolveObject();
		return Object && Object->IsIn(GWorld);
	}
}

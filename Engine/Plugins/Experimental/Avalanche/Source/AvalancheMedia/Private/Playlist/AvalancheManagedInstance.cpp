// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/AvalancheManagedInstance.h"

#include "Playlist/AvalancheManagedInstanceCache.h"
#include "RemoteControlPreset.h"

FAvalancheManagedInstance::FAvalancheManagedInstance(FAvalancheManagedInstanceCache* InParentCache, const FSoftObjectPath& InAssetPath)
	: ParentCache(InParentCache)
	, SourceAssetPath(InAssetPath)
{}

FAvalancheManagedInstance::~FAvalancheManagedInstance()
{}

void FAvalancheManagedInstance::InvalidateFromCache()
{
	if (ParentCache)
	{
		// The managed blueprint is not deleted immediately to avoid issues
		// with destroying delegates while they are being called.
		ParentCache->InvalidateNoDelete(SourceAssetPath);
		ParentCache = nullptr;
	}
}

void FAvalancheManagedInstance::RegisterSourceRemoteControlPresetDelegates(URemoteControlPreset* InSourceRemoteControlPreset)
{
	if (!::IsValid(InSourceRemoteControlPreset))
	{
		return;
	}
	
	InSourceRemoteControlPreset->OnEntityExposed().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlEntitiesExposed);
	InSourceRemoteControlPreset->OnEntityUnexposed().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlEntitiesUnexposed);
	InSourceRemoteControlPreset->OnEntitiesUpdated().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlEntitiesUpdated);
	InSourceRemoteControlPreset->OnExposedPropertiesModified().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlExposedPropertiesModified);
	InSourceRemoteControlPreset->OnControllerAdded().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlControllerAdded);
	InSourceRemoteControlPreset->OnControllerRemoved().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlControllerRemoved);
	InSourceRemoteControlPreset->OnControllerRenamed().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlControllerRenamed);
	InSourceRemoteControlPreset->OnControllerModified().AddRaw(
		this, &FAvalancheManagedInstance::OnSourceRemoteControlControllerModified);
}

void FAvalancheManagedInstance::UnregisterSourceRemoteControlPresetDelegates(URemoteControlPreset* InSourceRemoteControlPreset) const
{
	if (!::IsValid(InSourceRemoteControlPreset))
	{
		return;
	}
	
	InSourceRemoteControlPreset->OnEntityExposed().RemoveAll(this);
	InSourceRemoteControlPreset->OnEntityUnexposed().RemoveAll(this);
	InSourceRemoteControlPreset->OnEntitiesUpdated().RemoveAll(this);
	InSourceRemoteControlPreset->OnExposedPropertiesModified().RemoveAll(this);
	InSourceRemoteControlPreset->OnControllerAdded().RemoveAll(this);
	InSourceRemoteControlPreset->OnControllerRemoved().RemoveAll(this);
	InSourceRemoteControlPreset->OnControllerRenamed().RemoveAll(this);
	InSourceRemoteControlPreset->OnControllerModified().RemoveAll(this);
}

void FAvalancheManagedInstance::OnSourceRemoteControlEntitiesExposed(URemoteControlPreset* InSourcePreset, const FGuid& InEntityId)
{
	InvalidateFromCache();
}

void FAvalancheManagedInstance::OnSourceRemoteControlEntitiesUnexposed(URemoteControlPreset* InSourcePreset, const FGuid& InEntityId)
{
	InvalidateFromCache();
}

void FAvalancheManagedInstance::OnSourceRemoteControlEntitiesUpdated(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedEntities)
{
	// Info: Delegate called when the exposed entity wrapper itself is updated (ie. binding change, rename).
	// Not sure if we should invalidate the cache for this one.
}

void FAvalancheManagedInstance::OnSourceRemoteControlExposedPropertiesModified(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedProperties)
{
	// This will be called if values are modified from the Remote Control tab in the Ava BP editor.
	// Experimental: Propagate the default value immediately if it exists.
	// We don't want to apply those to the managed remote control because we don't want to overwrite the page's values.
	// We probably need an extra event to propagate a change in default value so pages with no values for that property
	// can be updated.

	// There is no event called when entities are unbound to actions, so rescan all actions just in case.
	DefaultRemoteControlValues.RefreshControlledEntities(InSourcePreset);

	for (const FGuid& PropertyId : InModifiedProperties)
	{
		// Update existing values only.
		// Remark: added values will go through OnSourceRemoteControlControllerAdded instead.
		if (DefaultRemoteControlValues.HasEntityValue(PropertyId))
		{
			DefaultRemoteControlValues.SetEntityValue(PropertyId, InSourcePreset, true);
		}
	}
}

void FAvalancheManagedInstance::OnSourceRemoteControlControllerAdded(URemoteControlPreset* InSourcePreset, const FName NewControllerName, const FGuid& InControllerId)
{
	InvalidateFromCache();
}

void FAvalancheManagedInstance::OnSourceRemoteControlControllerRemoved(URemoteControlPreset* InSourcePreset, const FGuid& InControllerId)
{
	InvalidateFromCache();
}

void FAvalancheManagedInstance::OnSourceRemoteControlControllerRenamed(URemoteControlPreset* InSourcePreset, const FName InOldLabel, const FName InNewLabel)
{
	InvalidateFromCache();
}

void FAvalancheManagedInstance::OnSourceRemoteControlControllerModified(URemoteControlPreset* InSourcePreset, const TSet<FGuid>& InModifiedControllerIds)
{
	// This will be called if values are modified from the Remote Control tab in the Ava BP editor.
	// Experimental: Propagate the default value immediately if it exists.
	// We don't want to apply those to the managed remote control because we don't want to overwrite the page's values.
	// We probably need an extra event to propagate a change in default value so pages with no values for that property
	// can be updated.
	for (const FGuid& ControllerId : InModifiedControllerIds)
	{
		// Update existing values only.
		if (DefaultRemoteControlValues.HasControllerValue(ControllerId))
		{
			DefaultRemoteControlValues.SetControllerValue(ControllerId, InSourcePreset, true);
		}
	}
}
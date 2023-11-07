// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EAuthorityMutability.h"

#include "Delegates/Delegate.h"

struct FSoftObjectPath;

namespace UE::MultiUserClient
{
	/** Synchronizes the a client's authority state with the server. */
	class IClientAuthoritySynchronizer
	{
	public:
		
		/** @return The reason of why it is or isn't possible to change the authority for the given object. */
		virtual EAuthorityMutability GetChangeAuthorityMutability(const FSoftObjectPath& ObjectPath) const = 0;
		/** @return Whether the ObjectPath's authority can be changed. */
		bool CanChangeAuthority(const FSoftObjectPath& ObjectPath) const { return GetChangeAuthorityMutability(ObjectPath) == EAuthorityMutability::Allowed; }

		/** @return Whether this client is sending anything at all. */
		virtual bool HasAnyAuthority() const = 0;

		/** @return Whether the local instance thinks this client has authority over ObjectPath. */
		virtual bool HasAuthorityOver(const FSoftObjectPath& ObjectPath) const = 0;

		DECLARE_MULTICAST_DELEGATE(FOnServerStateChanged);
		/** @return Event executed when authority state has been updated. */
		virtual FOnServerStateChanged& OnServerStateChanged() = 0;

		virtual ~IClientAuthoritySynchronizer() = default;
	};
}
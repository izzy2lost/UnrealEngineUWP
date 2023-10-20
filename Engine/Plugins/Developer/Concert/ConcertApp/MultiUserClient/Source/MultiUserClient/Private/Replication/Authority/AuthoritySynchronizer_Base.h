// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IClientAuthoritySynchronizer.h"

#include "Delegates/Delegate.h"
#include "Templates/UnrealTemplate.h"

namespace UE::MultiUserClient
{
	class FStreamChangeTracker;

	DECLARE_DELEGATE_RetVal_OneParam(bool, FDoesObjectHaveProperties, const FSoftObjectPath&);
	
	/**
	 * Util base implementation of CanChangeAuthority.
	 * Changing authority is only possible if the client has properties associated with the object.
	 */
	class FAuthoritySynchronizer_Base
		: public IClientAuthoritySynchronizer
		, public FNoncopyable
	{
	public:

		FAuthoritySynchronizer_Base(FDoesObjectHaveProperties InDoesObjectHavePropertiesDelegate);
		
		//~ Begin IClientAuthoritySynchronizer Interface
		virtual EAuthorityMutability GetChangeAuthorityMutability(const FSoftObjectPath& ObjectPath) const override;
		virtual FOnServerStateChanged& OnServerStateChanged() override { return OnServerStateChangedDelegate; }
		//~ End IClientAuthoritySynchronizer Interface

	protected:

		/** Triggered by subclasses when the authority state changes. */
		FOnServerStateChanged OnServerStateChangedDelegate;
		
	private:

		/** Used to check whether the client will have properties associated with the object after submitting the change. */
		FDoesObjectHaveProperties DoesObjectHavePropertiesDelegate;
	};
}

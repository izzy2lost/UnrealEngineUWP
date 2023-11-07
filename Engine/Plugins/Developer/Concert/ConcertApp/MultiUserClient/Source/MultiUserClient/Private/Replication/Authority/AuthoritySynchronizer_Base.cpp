// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthoritySynchronizer_Base.h"

namespace UE::MultiUserClient
{
	FAuthoritySynchronizer_Base::FAuthoritySynchronizer_Base(FDoesObjectHaveProperties InDoesObjectHavePropertiesDelegate)
		: DoesObjectHavePropertiesDelegate(MoveTemp(InDoesObjectHavePropertiesDelegate))
	{}

	EAuthorityMutability FAuthoritySynchronizer_Base::GetChangeAuthorityMutability(const FSoftObjectPath& ObjectPath) const
	{
		// You can always take away your own authority
		const bool bHasAuthority = HasAuthorityOver(ObjectPath);
		if (bHasAuthority)
		{
			return EAuthorityMutability::Allowed;
		}
		
		const bool bHasProperties = DoesObjectHavePropertiesDelegate.Execute(ObjectPath);
		if (!bHasProperties)
		{
			return EAuthorityMutability::NoProperties;
		}

		return EAuthorityMutability::Allowed;
	}
}

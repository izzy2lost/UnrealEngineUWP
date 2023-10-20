// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::MultiUserClient
{
	/** Various reasons in which an object can be when considering take over its authority */
	enum class EAuthorityMutability
	{
		/** Authority can be taken */
		Allowed,

		/** The object has no registered properties. Cannot take authority. */
		NoProperties,
		
		// TODO DP UE-198356: Add function to query which properties and clients are conflicting
		/** Another client has authority over some of the object's registered properties already. Cannot take authority. */
		ClientConflict,
		/** The operation is not implemented. */
		NotSupported
	};
}
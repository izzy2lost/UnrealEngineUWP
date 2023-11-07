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
		
		/** The operation is not implemented. */
		NotSupported
	};
}
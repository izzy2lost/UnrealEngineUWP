// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/IParameterSource.h"

struct FAnimNextParamInstanceIdentifier;

namespace UE::AnimNext
{
	class IParameterSource;
}

namespace UE::AnimNext
{

// Context passed to object accessor functions registered to RegisterObjectAccessor
struct FParameterSourceContext
{
	FParameterSourceContext() = default;

	FParameterSourceContext(const UObject* InObject)
		: Object(InObject)
	{
		check(Object != nullptr);
	}

	// The object that the AnimNext entry is bound to (e.g. a UAnimNextComponent)
	const UObject* Object = nullptr;
};

// Interface allowing other modules to extend and add to external parameter system
class IParameterSourceFactory
{
public:
	virtual ~IParameterSourceFactory() = default;

	// Factory method used to create a parameter source of the specified name, with a set of parameters that are initially required
	// @param  InContext              Context used to resolve the scope
	// @param  InInstanceId           The instance identifier associated with the parameters that are required
	// @param  InRequiredParameters   Any required parameters that the source should initially supply, can be empty, in which case all parameters are created
	// @return a new parameter source, or nullptr if the source was not found
	virtual TUniquePtr<IParameterSource> CreateParameterSource(const FParameterSourceContext& InContext, const TInstancedStruct<FAnimNextParamInstanceIdentifier>& InInstanceId, TConstArrayView<FName> InRequiredParameters = TConstArrayView<FName>()) const = 0;
};

}
// Copyright Epic Games, Inc. All Rights Reserved.

#if AVCODECS_USE_METAL

#pragma once

#include "Templates/RefCounting.h"

#include "AVContext.h"
#include "Video/VideoResource.h"

THIRD_PARTY_INCLUDES_START
#include "MetalInclude.h"
THIRD_PARTY_INCLUDES_END


/**
 * Metal platform video context and resource.
 */

class AVCODECSCORE_API FVideoContextMetal : public FAVContext
{
public:
	MTL::Device* Device;

	FVideoContextMetal(MTL::Device* Device);
};


class AVCODECSCORE_API FVideoResourceMetal : public TVideoResource<FVideoContextMetal>
{
private:
	MTL::Texture* Raw;

public:
	static FVideoDescriptor GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw);

	FORCEINLINE MTL::Texture* GetRaw() const { return Raw; }

	FVideoResourceMetal(TSharedRef<FAVDevice> const& Device, MTL::Texture* Raw, FAVLayout const& Layout);

	virtual FAVResult Validate() const override;
};

DECLARE_TYPEID(FVideoContextMetal, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoResourceMetal, AVCODECSCORE_API);

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"

#include "DMXGDTF.generated.h"

namespace UE::DMX::GDTF { class FDMXGDTFDescription; }


/* The implementation of the GDTF standard in Unreal Engine. **/
UCLASS()
class DMXGDTF_API UDMXGDTF
	: public UObject
{
	GENERATED_BODY()

	using FDMXGDTFDescription = UE::DMX::GDTF::FDMXGDTFDescription;

public:
	/** Initializes this object from .gdtf file data. Returns true on success. */
	void InitializeFromData(const TArray64<uint8>& Data);

	/** Returns the GDTF Description. */
	TSharedPtr<FDMXGDTFDescription> GetDescription() const { return Description; }

private:
	/** The GDTF description */
	TSharedPtr<FDMXGDTFDescription> Description;
};

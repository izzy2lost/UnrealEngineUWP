// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"

struct FGuid;

namespace UE
{
	namespace Sequencer
	{

		class SEQUENCER_API IBindingLifetimeExtension
		{
		public:

			UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IBindingLifetimeExtension)

			virtual ~IBindingLifetimeExtension() {}

			virtual const TArray<TRange<FFrameNumber>>& GetInverseLifetimeRange() const = 0;
		};

	} // namespace Sequencer
} // namespace UE


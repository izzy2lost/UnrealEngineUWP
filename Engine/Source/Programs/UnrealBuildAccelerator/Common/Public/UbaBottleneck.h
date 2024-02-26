// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaSynchronization.h"

namespace uba
{

	struct Bottleneck
	{
		Bottleneck(u32 mc) : underMax(true), activeCount(0), maxCount(mc) {}
		ReaderWriterLock lock;
		Event underMax;
		u32 activeCount;
		u32 maxCount;
	};

	struct BottleneckScope
	{
		BottleneckScope(Bottleneck& b) : bottleneck(b)
		{
			SCOPED_WRITE_LOCK(bottleneck.lock, lock);
			while (true)
			{
				if (bottleneck.activeCount < bottleneck.maxCount)
				{
					++bottleneck.activeCount;
					if (bottleneck.activeCount == bottleneck.maxCount)
						bottleneck.underMax.Reset();
					break;
				}

				lock.Leave();
				bottleneck.underMax.IsSet();
				lock.Enter();
			}
		}

		~BottleneckScope()
		{
			SCOPED_WRITE_LOCK(bottleneck.lock, lock);
			if (bottleneck.activeCount == bottleneck.maxCount)
				bottleneck.underMax.Set();
			--bottleneck.activeCount;
		}

		Bottleneck& bottleneck;
	};

}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaMemory.h"

namespace uba
{
	class Event;

	class Thread
	{
	public:
		Thread();
		Thread(Function<u32()>&& func);
		~Thread();
		void Start(Function<u32()>&& func);
		bool Wait(u32 milliseconds = ~0u, Event* wakeupEvent = nullptr);
	private:
		Function<u32()>	m_func;
		void* m_handle;
		Event m_finished; // Only used for non windows
	};


	bool AlternateGroupAffinity(void* nativeThreadHandle);
}

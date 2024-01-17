// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/SlateStyle.h"

namespace HarmonixMetasoundEditor
{
	class FSlateStyle final
		: public FSlateStyleSet
	{
	public:
		FSlateStyle();
		static const FSlateStyle& Get();
		const FSlateBrush* GetMidiStreamConnectedIcon() const;
		const FSlateBrush* GetMidiStreamDisconnectedIcon() const;
		const FSlateBrush* GetMidiClockConnectedIcon() const;
		const FSlateBrush* GetMidiClockDisconnectedIcon() const;
		const FSlateBrush* GetTransportConnectedIcon() const;
		const FSlateBrush* GetTransportDisconnectedIcon() const;

		~FSlateStyle();
	};
}

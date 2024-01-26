// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasoundSlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace HarmonixMetasoundEditor
{
	FSlateStyle::FSlateStyle()
		: FSlateStyleSet("HarmonixMetasoundSlateStyle")
	{
		const FVector2D Icon22x22(22.0f, 22.0f);
		const FVector2D Icon18x10(18.0f, 10.0f);
		const FVector2D Icon18x18(18.0f, 18.0f);
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Runtime/Harmonix/Content/Editor"));

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
		Set("HarmonixMetasound.MidiConnectedIcon", new IMAGE_BRUSH(TEXT("Icons/MidiConnectedPin"), Icon22x22));
		Set("HarmonixMetasound.MidiDisconnectedIcon", new IMAGE_BRUSH(TEXT("Icons/MidiDisconnectedPin"), Icon22x22));
		Set("HarmonixMetasound.ClockConnectedIcon", new IMAGE_BRUSH(TEXT("Icons/ClockConnectedPin"), Icon18x18));
		Set("HarmonixMetasound.ClockDisconnectedIcon", new IMAGE_BRUSH(TEXT("Icons/ClockDisconnectedPin"), Icon18x18));
		Set("HarmonixMetasound.TransportConnectedIcon", new IMAGE_BRUSH(TEXT("Icons/TransportConnectedPin"), Icon18x10));
		Set("HarmonixMetasound.TransportDisconnectedIcon", new IMAGE_BRUSH(TEXT("Icons/TransportDisconnectedPin"), Icon18x10));
#undef IMAGE_BRUSH

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}
	
	const FSlateStyle& FSlateStyle::Get()
	{
		static FSlateStyle SlateStyle;
		return SlateStyle;
	}

	const FSlateBrush* FSlateStyle::GetMidiStreamConnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.MidiConnectedIcon");
	}

	const FSlateBrush* FSlateStyle::GetMidiStreamDisconnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.MidiDisconnectedIcon");
	}

	const FSlateBrush* FSlateStyle::GetMidiClockConnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.ClockConnectedIcon");
	}

	const FSlateBrush* FSlateStyle::GetMidiClockDisconnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.ClockDisconnectedIcon");
	}

	const FSlateBrush* FSlateStyle::GetTransportConnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.TransportConnectedIcon");
	}

	const FSlateBrush* FSlateStyle::GetTransportDisconnectedIcon() const
	{
		return GetBrush("HarmonixMetasound.TransportDisconnectedIcon");
	}

	FSlateStyle::~FSlateStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}
}

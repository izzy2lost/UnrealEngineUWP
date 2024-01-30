// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AvaViewportColorPickerDelegates.h"

namespace UE::AvalancheLevelViewport::Private
{
	static FAvaOnColorPicked OnColorPicked;
	static FAvaOnColorSourceSelected OnColorSourceSelected;

	static FAvaColorChangeData LastColorData = FAvaColorChangeData();
}

FAvaOnColorPicked& FAvaViewportColorPickerDelegates::GetOnColorPicked()
{
	return UE::AvalancheLevelViewport::Private::OnColorPicked;
}

void FAvaViewportColorPickerDelegates::BroadcastColorPicked(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData)
{
	UE::AvalancheLevelViewport::Private::OnColorPicked.Broadcast(InEditor, InNewColorData);
	UE::AvalancheLevelViewport::Private::LastColorData = InNewColorData;
}

FAvaOnColorPicked& FAvaViewportColorPickerDelegates::GetOnColorSourceSelected()
{
	return UE::AvalancheLevelViewport::Private::OnColorSourceSelected;
}

void FAvaViewportColorPickerDelegates::BroadcastColorSourceSelected(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData)
{
	UE::AvalancheLevelViewport::Private::OnColorPicked.Broadcast(InEditor, InNewColorData);
	UE::AvalancheLevelViewport::Private::LastColorData = InNewColorData;
}

FAvaColorChangeData FAvaViewportColorPickerDelegates::GetLastColorData()
{
	return UE::AvalancheLevelViewport::Private::LastColorData;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Templates/SharedPointerFwd.h"

class FAvaPlaylistEditor;
class UAvaSequence;
class UAvalanchePlaylist;
struct FAvalanchePage;

struct FAvaMRQEditorPlaylistUtils
{
	AVALANCHEMRQEDITOR_API static void RenderSelectedPages(TConstArrayView<TWeakPtr<const FAvaPlaylistEditor>> InPlaylistEditors);

	AVALANCHEMRQEDITOR_API static void RenderPages(const UAvalanchePlaylist& InPlaylist, TConstArrayView<int32> InPageIds);

	AVALANCHEMRQEDITOR_API static void RenderPage(const UAvalanchePlaylist& InPlaylist, const FAvalanchePage& InPage);
};

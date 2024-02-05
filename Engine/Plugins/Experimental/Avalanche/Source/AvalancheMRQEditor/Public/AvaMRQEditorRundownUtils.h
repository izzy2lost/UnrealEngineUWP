// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Templates/SharedPointerFwd.h"

class FAvaRundownEditor;
class UAvaSequence;
class UAvaRundown;
struct FAvaRundownPage;

struct FAvaMRQEditorRundownUtils
{
	AVALANCHEMRQEDITOR_API static void RenderSelectedPages(TConstArrayView<TWeakPtr<const FAvaRundownEditor>> InRundownEditors);

	AVALANCHEMRQEDITOR_API static void RenderPages(const UAvaRundown& InRundown, TConstArrayView<int32> InPageIds);

	AVALANCHEMRQEDITOR_API static void RenderPage(const UAvaRundown& InRundown, const FAvaRundownPage& InPage);
};

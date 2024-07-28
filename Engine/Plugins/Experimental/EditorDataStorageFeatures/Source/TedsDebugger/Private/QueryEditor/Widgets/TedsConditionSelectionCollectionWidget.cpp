// Copyright Epic Games, Inc. All Rights Reserved.
#include "QueryEditor/Widgets/TedsConditionSelectionCollectionWidget.h"

namespace UE::Teds::Debug::QueryEditor
{
	void SConditionSelectionCollectionWidget::Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel)
	{
		Model = &InModel;
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.
#include "Widgets/QueryEditor/TedsConditionSelectionCollectionWidget.h"

namespace UE::EditorDataStorage::Debug::QueryEditor
{
	void SConditionSelectionCollectionWidget::Construct(const FArguments& InArgs, FTedsQueryEditorModel& InModel)
	{
		Model = &InModel;
	}
}
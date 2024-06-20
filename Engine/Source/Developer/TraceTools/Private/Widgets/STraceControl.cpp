// Copyright Epic Games, Inc. All Rights Reserved.

#include "STraceControl.h"

#include "Framework/Commands/UICommandList.h"
#include "STraceDataFilterWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/STraceControlToolbar.h"

namespace UE::TraceTools
{

STraceControl::STraceControl()
{
}

STraceControl::~STraceControl()
{

}

void STraceControl::Construct(const FArguments& InArgs, TSharedPtr<ITraceController> InTraceController)
{
	TraceController = InTraceController;

	UICommandList = MakeShareable(new FUICommandList);
	BindCommands();

	TraceController->SendStatusUpdateRequest();
	TraceController->SendChannelUpdateRequest();

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			SNew(STraceControlToolbar, UICommandList.ToSharedRef(), InTraceController)
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
		[
			SNew(STraceDataFilterWidget, InTraceController)
		]
	];
}

void STraceControl::BindCommands()
{

}

} // namespace UE::TraceTools

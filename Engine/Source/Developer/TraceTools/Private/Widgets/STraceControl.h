// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITraceController.h"
#include "Widgets/SCompoundWidget.h"

class ISessionManager;
class FUICommandList;

namespace UE::TraceTools
{

class STraceControl : public SCompoundWidget
{
public:
	/** Default constructor. */
	STraceControl();

	/** Virtual destructor. */
	virtual ~STraceControl();

	SLATE_BEGIN_ARGS(STraceControl) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs, TSharedPtr<ITraceController> InTraceController);

protected:
	void BindCommands();

private:
	TSharedPtr<ITraceController> TraceController;

	TSharedPtr<FUICommandList> UICommandList;
};

} // namespace UE::TraceTools

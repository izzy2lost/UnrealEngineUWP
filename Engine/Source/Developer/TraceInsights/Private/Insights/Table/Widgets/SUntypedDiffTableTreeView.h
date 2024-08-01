// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TraceInsights
#include "Insights/Table/ViewModels/UntypedTable.h"
#include "Insights/Table/Widgets/SUntypedTableTreeView.h"

namespace UE::Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class SUntypedDiffTableTreeView : public SUntypedTableTreeView
{
public:
	void UpdateSourceTableA(const FString& Name, TSharedPtr<TraceServices::IUntypedTable> SourceTable);
	void UpdateSourceTableB(const FString& Name, TSharedPtr<TraceServices::IUntypedTable> SourceTable);

protected:
	FReply SwapTables_OnClicked();
	FText GetSwapButtonText() const;

	virtual TSharedPtr<SWidget> ConstructToolbar() override;

	void RequestMergeTables();

private:
	TSharedPtr<TraceServices::IUntypedTable> SourceTableA;
	TSharedPtr<TraceServices::IUntypedTable> SourceTableB;
	FString TableNameA;
	FString TableNameB;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace UE::Insights

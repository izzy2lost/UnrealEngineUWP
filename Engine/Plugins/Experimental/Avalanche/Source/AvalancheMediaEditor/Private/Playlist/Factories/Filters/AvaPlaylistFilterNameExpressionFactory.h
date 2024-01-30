// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playlist/Factories/Filters/IAvaPlaylistFilterExpressionFactory.h"

class FAvaPlaylistFilterNameExpressionFactory : public IAvaPlaylistFilterExpressionFactory
{
public:
	static const FName KeyName;

	//~ Begin IAvaFilterExpressionFactory interface
	virtual FName GetFilterIdentifier() const override;
	virtual bool FilterExpression(const FAvalanchePage& InItem, const FAvaPlaylistTextFilterArgs& InArgs) const override;
	virtual bool SupportsComparisonOperation(ETextFilterComparisonOperation InComparisonOperation, EAvaPlaylistSearchListType InPlaylistSearchListType) const override;
	//~ End IAvaFilterExpressionFactory interface
};

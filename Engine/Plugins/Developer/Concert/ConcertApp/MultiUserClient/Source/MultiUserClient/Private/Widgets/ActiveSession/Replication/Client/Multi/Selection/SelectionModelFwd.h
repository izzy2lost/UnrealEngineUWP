// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISelectionModel.h"

namespace UE::MultiUserClient::Replication
{
	class FOnlineClient;
	struct FClientItem;
	
	using IOnlineClientSelectionModel = ISelectionModel<FOnlineClient>;
}

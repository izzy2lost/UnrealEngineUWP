// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include <catch2/catch_test_macros.hpp>

#include "Helpers/Identity/IdentityGetLoginByUserId.h"
#include "Helpers/Lobby/LobbyCreateHelper.h"
#include "Helpers/Lobby/LobbyJoinHelper.h"
#include "OnlineCatchHelper.h"

#define LOBBY_CREATE_INVITE_TAGS "[Lobby]"
#define LOBBY_CREATE_INVITE_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, LOBBY_CREATE_INVITE_TAGS __VA_ARGS__)

LOBBY_CREATE_INVITE_TEST_CASE("Basic lobby create and join test")
{
	int32 LocalUserNum = 0;
	UE::Online::FCreateLobby::Params LobbyCreateParams;
	LobbyCreateParams.LocalName = TEXT("TestLobby");
	LobbyCreateParams.SchemaId = TEXT("test");
	LobbyCreateParams.MaxMembers = 2;
	LobbyCreateParams.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;
	LobbyCreateParams.Attributes = { { TEXT("LobbyCreateTime"), (int64)10}};

	FJoinLobby::Params JoinLobbyParams;
	JoinLobbyParams.LocalName = TEXT("TestLobby");

	GetLoginPipeline(1)
		.EmplaceStep<FIdentityGetUniquePlayerIdStep>(LocalUserNum, 
			[&LobbyCreateParams, &JoinLobbyParams](UE::Online::FAccountInfo InAccountInfo) {
				LobbyCreateParams.LocalAccountId = InAccountInfo.AccountId;
				JoinLobbyParams.LocalAccountId = InAccountInfo.AccountId;
			})
		.EmplaceStep<FLobbyCreateHelper>(&LobbyCreateParams, [&JoinLobbyParams](UE::Online::FLobby InLobby) {
				JoinLobbyParams.LobbyId = InLobby.LobbyId; }, true);

	RunToCompletion();
}

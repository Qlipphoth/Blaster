// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "MultiplayerSessionsSubsystem.h"

/// @brief 重写 PostLogin，当连接到服务器的玩家数量达到 2 时，服务器将旅行到游戏地图。
/// @param NewPlayer  新连接的玩家
void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	int32 NumberOfPlayers = GameState.Get()->PlayerArray.Num();
	// if (NumberOfPlayers == 2)
	// {
	// 	UWorld* World = GetWorld();
	// 	if (World)
	// 	{
	// 		// 使用无缝旅行，这样我们就不会丢失玩家的连接。
	// 		bUseSeamlessTravel = true;
    //         // 作为监听服务器旅行到游戏地图
	// 		World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
	// 	}
	// }

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		UMultiplayerSessionsSubsystem* Subsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		
		// 如果 Subsystem 为 nullptr，则直接停止执行，check 只在 Debug 编译时有效。
		check(Subsystem);

		if (NumberOfPlayers == Subsystem->DesiredNumPublicConnections)
		{
			UWorld* World = GetWorld();
			if (World)
			{
				bUseSeamlessTravel = true;

				FString MatchType = Subsystem->DesiredMatchType;
				if (MatchType == "FreeForAll")
				{
					World->ServerTravel(FString("/Game/Maps/BlasterMap?listen"));
				}
				else if (MatchType == "Teams")
				{
					World->ServerTravel(FString("/Game/Maps/Teams?listen"));
				}
				else if (MatchType == "CaptureTheFlag")
				{
					World->ServerTravel(FString("/Game/Maps/CaptureFlag?listen"));
				}
			}
		}
	}

}
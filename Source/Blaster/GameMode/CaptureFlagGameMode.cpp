// Fill out your copyright notice in the Description page of Project Settings.


#include "CaptureFlagGameMode.h"
#include "Blaster/Weapon/Flag.h"
#include "Blaster/CaptureFlag/FlagZone.h"
#include "Blaster/GameState/BlasterGameState.h"

void ACaptureFlagGameMode::PlayerEliminated(class ABlasterCharacter* ElimmedCharacter, class ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
{
	ABlasterGameMode::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
}

void ACaptureFlagGameMode::FlagCaptured(AFlag* Flag, AFlagZone* Zone)
{
	// bool bValidCapture = Flag->GetTeam() != Zone->Team;
	ABlasterGameState* BGameState = Cast<ABlasterGameState>(GameState);
	if (BGameState)
	{
		if (Zone->Team == ETeam::ET_BlueTeam)
		{
			BGameState->BlueTeamScores();
		}
		if (Zone->Team == ETeam::ET_RedTeam)
		{
			BGameState->RedTeamScores();
		}
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "OverheadWidget.h"
#include "Components/TextBlock.h"

void UOverheadWidget::SetDisplayText(FString TextToDisplay)
{
	if (DisplayText)
	{
		DisplayText->SetText(FText::FromString(TextToDisplay));
	}
}

void UOverheadWidget::ShowPlayerNetRole(APawn* InPawn)
{
	// ENetRole RemoteRole = InPawn->GetRemoteRole();
	ENetRole RemoteRole = InPawn->GetLocalRole();

	FString Role;
	switch (RemoteRole)
	{
	case ENetRole::ROLE_Authority:
		Role = FString("Authority");
		break;
	case ENetRole::ROLE_AutonomousProxy:
		Role = FString("Autonomous Proxy");
		break;
	case ENetRole::ROLE_SimulatedProxy:
		Role = FString("Simulated Proxy");
		break;
	case ENetRole::ROLE_None:
		Role = FString("None");
		break;
	}
    
	// FString RemoteRoleString = FString::Printf(TEXT("Remote Role: %s"), *Role);
	FString RemoteRoleString = FString::Printf(TEXT("Local Role: %s"), *Role);
	SetDisplayText(RemoteRoleString);
}

/// @brief 重写OnLevelRemovedFromWorld函数，当Widget所在的Level被移除时，将Widget从Parent中移除
/// @param InLevel  当前Level 
/// @param InWorld  当前World
void UOverheadWidget::OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	RemoveFromParent();
	Super::OnLevelRemovedFromWorld(InLevel, InWorld);
} 
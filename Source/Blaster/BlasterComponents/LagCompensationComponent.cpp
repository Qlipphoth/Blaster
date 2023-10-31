// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "Blaster/Character/BlasterCharacter.h"
#include "Blaster/Weapon/Weapon.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"


#pragma region Initialization

ULagCompensationComponent::ULagCompensationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULagCompensationComponent::BeginPlay()
{
	Super::BeginPlay();

	FFramePackage Package;
	SaveFramePackage(Package);
	ShowFramePackage(Package, FColor::Orange);
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SaveFrames();
}

#pragma endregion


#pragma region FrameHistory

/// @brief 保留当前角色最近 MaxRecordTime 秒内的帧信息，存放在 FrameHistory 中
void ULagCompensationComponent::SaveFrames()
{
	if (Character == nullptr || !Character->HasAuthority()) return;
	if (FrameHistory.Num() <= 1)
	{
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	}
	else
	{
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		while (HistoryLength > MaxRecordTime)
		{
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);

		// ShowFramePackage(ThisFrame, FColor::Red);
	}
}

/// @brief 将角色当前的 HitBoxes 信息存放在 OutPackage 中
/// @param OutPackage 保存角色当前 HitBoxes 信息的结构体
void ULagCompensationComponent::SaveFramePackage(FFramePackage& OutPackage)
{
	Character = Character == nullptr ? Cast<ABlasterCharacter>(GetOwner()) : Character;
	if (Character)
	{
		OutPackage.Time = GetWorld()->GetTimeSeconds();
		for (auto& BoxPair : Character->HitCollisionBoxes)
		{
			FBoxInformation BoxInformation;
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			// BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			// BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();
			BoxInformation.Radius = BoxPair.Value->GetScaledSphereRadius();
			OutPackage.HitBoxInfo.Add(BoxPair.Key, BoxInformation);
		}
	}
}

#pragma endregion


#pragma region ServerSideRewind

/// @brief 客户端向服务器发起的倒带伤害判定请求，服务器根据客户端的 HitTime 进行 Rewind Hit 检测，判定成功则造成伤害
/// @param HitCharacter 被击中角色
/// @param TraceStart 射击起点
/// @param HitLocation 射击终点
/// @param HitTime 客户端射击时间
/// @param DamageCauser 造成伤害的武器
void ULagCompensationComponent::ServerScoreRequest_Implementation(
	ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, 
	const FVector_NetQuantize& HitLocation, float HitTime, AWeapon* DamageCauser)
{
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);

	if (Character && HitCharacter && DamageCauser && Confirm.bHitConfirmed)
	{
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			DamageCauser->GetDamage(),
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}

/// @brief 服务器倒带算法，根据客户端的 HitTime，从 FrameHistory 中找到对应的帧信息，然后进行 ConfirmHit 检测
/// @param HitCharacter 被击中的角色
/// @param TraceStart   射击起点
/// @param HitLocation  射击终点
/// @param HitTime      客户端射击时间
/// @return 返回一个结构体，表示是否击中，是否是 headshot
FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(
	ABlasterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, 
	const FVector_NetQuantize& HitLocation, float HitTime)
{
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

/// @brief 根据客户端的 HitTime，找到对应的帧信息
/// @param HitCharacter 被击中的角色
/// @param HitTime 	客户端射击时间
/// @return 返回击中那一帧的帧信息
FFramePackage ULagCompensationComponent::GetFrameToCheck(ABlasterCharacter* HitCharacter, float HitTime)
{
	bool bReturn =
		HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;
	if (bReturn) return FFramePackage();
	// Frame package that we check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true;
	// Frame history of the HitCharacter
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	if (OldestHistoryTime > HitTime)
	{
		// too far back - too laggy to do SSR
		return FFramePackage();
	}
	if (OldestHistoryTime == HitTime)
	{
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	if (NewestHistoryTime <= HitTime)
	{
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterpolate = false;
	}

	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	while (Older->GetValue().Time > HitTime) // is Older still younger than HitTime?
	{
		// March back until: OlderTime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime)
		{
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) // highly unlikely, but we found our frame to check
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	if (bShouldInterpolate)
	{
		// Interpolate between Younger and Older
		FrameToCheck = InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	FrameToCheck.Character = HitCharacter;
	return FrameToCheck;
}

/// @brief 在两帧之间进行插值，得到 HitTime 对应的帧信息，HitTime 介于 OlderFrame 和 YoungerFrame 之间
/// @param OlderFrame   较旧的帧信息
/// @param YoungerFrame 较新的帧信息
/// @param HitTime 	客户端射击时间
/// @return 返回插值后的帧信息
FFramePackage ULagCompensationComponent::InterpBetweenFrames(
	const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime)
{
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	for (auto& YoungerPair : YoungerFrame.HitBoxInfo)
	{
		const FName& BoxInfoName = YoungerPair.Key;

		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];

		FBoxInformation InterpBoxInfo;

		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, 1.f, InterpFraction);
		// InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, 1.f, InterpFraction);
		// InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;
		InterpBoxInfo.Radius = YoungerBox.Radius;

		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}

	return InterpFramePackage;
}

/// @brief 根据传入的 FramePackage 进行 Rewind Hit 检测，判断是否击中
/// @param Package Rewind 后得到的帧信息
/// @param HitCharacter 被击中的角色
/// @param TraceStart  射击起点
/// @param HitLocation 射击终点
/// @return 返回一个结构体，表示是否击中，是否是 headshot
FServerSideRewindResult ULagCompensationComponent::ConfirmHit(
	const FFramePackage& Package, ABlasterCharacter* HitCharacter, 
	const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation)
{
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	// 在进行 Rewind Hit 检测时，禁用角色的碰撞体
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Enable collision for the head first
	// UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World)
	{
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECollisionChannel::ECC_Visibility
		);
		// we hit the head, return early
		if (ConfirmHitResult.bBlockingHit) 
		{
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true };
		}
		// didn't hit head, check the rest of the boxes
		else 
		{
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
			{
				if (HitBoxPair.Value != nullptr)
				{
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(
						ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
				}
			}
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			if (ConfirmHitResult.bBlockingHit)
			{
				ResetHitBoxes(HitCharacter, CurrentFrame);
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false };
			}
		}
	}

	// 将 HitBoxes 恢复到原来的位置
	ResetHitBoxes(HitCharacter, CurrentFrame);
	// 重新启用角色的碰撞体
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

#pragma endregion


#pragma region ShotgunRewind

/// @brief 客户端向服务器发起的 Shotgun 倒带伤害判定请求，服务器根据客户端的 HitTime 进行 Rewind Hit 检测，判定成功则造成伤害
/// @param HitCharacters 被击中角色列表
/// @param TraceStart 射击起点
/// @param HitLocations 射击终点列表
/// @param HitTime 客户端射击时间
void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(
	const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, 
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime)
{
	FShotgunServerSideRewindResult Confirm;
	
	// Confirm 以引用的方式传入，函数内部会修改 Confirm 的值
	ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime, Confirm);

	// 如果组件的 Character 为空，或者 Character 的武器为空，直接返回
	if (Character == nullptr || Character->GetEquippedWeapon() == nullptr) return;
	
	// 遍历所有被击中的角色，计算伤害
	for (auto& HitCharacter : HitCharacters)
	{
		if (HitCharacter == nullptr) continue;

		float TotalDamage = 0.f;
		if (Confirm.HeadShots.Contains(HitCharacter))
		{
			float HeadShotDamage = Confirm.HeadShots[HitCharacter] * Character->GetEquippedWeapon()->GetDamage();
			TotalDamage += HeadShotDamage;
		}
		if (Confirm.BodyShots.Contains(HitCharacter))
		{
			float BodyShotDamage = Confirm.BodyShots[HitCharacter] * Character->GetEquippedWeapon()->GetDamage();
			TotalDamage += BodyShotDamage;
		}
		
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			TotalDamage,
			Character->Controller,
			Character->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}

/// @brief Shotgun 的服务器端倒带算法，根据客户端的 HitTime，从 FrameHistory 中找到对应的帧信息，然后进行 ConfirmHit 检测
/// @param HitCharacters 被击中角色列表
/// @param TraceStart  射击起点
/// @param HitLocations 射击终点列表
/// @param HitTime    客户端射击时间
/// @param OutShotgunResult 以引用方式传入的结构体，用于保存 Shotgun 的命中结果
void ULagCompensationComponent::ShotgunServerSideRewind(
	const TArray<ABlasterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, 
	const TArray<FVector_NetQuantize>& HitLocations, float HitTime,
	FShotgunServerSideRewindResult& OutShotgunResult)
{
	TArray<FFramePackage> FramesToCheck;
	for (ABlasterCharacter* HitCharacter : HitCharacters)
	{
		FramesToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}

	ShotgunConfirmHit(FramesToCheck, TraceStart, HitLocations, OutShotgunResult);
}

/// @brief 根据传入的 FramePackages 进行 Shotgun Rewind Hit 检测，判断是否击中
/// @param FramePackages Rewind 后得到的帧信息列表
/// @param TraceStart 射击起点
/// @param HitLocations 射击终点列表
/// @param OutShotgunResult 以引用方式传入的结构体，用于保存 Shotgun 的命中结果
void ULagCompensationComponent::ShotgunConfirmHit(
	const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, 
	const TArray<FVector_NetQuantize>& HitLocations,
	FShotgunServerSideRewindResult& OutShotgunResult)
{
	for (auto& Frame : FramePackages)
	{
		if (Frame.Character == nullptr) return;
	}

	// 保存命中结果，FShotgunServerSideRewindResult 为一个结构体，包含了 HeadShots 和 BodyShots 两个 TMap
	// FShotgunServerSideRewindResult ShotgunResult;

	// 缓存所有 FramePakcages 中角色的 HitBoxes 信息
	TArray<FFramePackage> CurrentFrames;
	
	// 缓存所有 FramePakcages 中角色的 HitBoxes 信息，并将 HitBoxes 移动到 Package 中保存的位置
	for (auto& Frame : FramePackages)
	{
		FFramePackage CurrentFrame;
		CurrentFrame.Character = Frame.Character;
		CacheBoxPositions(Frame.Character, CurrentFrame);
		MoveBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}

	// 启用所有 FramePakcages 中角色的头部碰撞体
	for (auto& Frame : FramePackages)
	{
		// Enable collision for the head first
		HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		HeadBox->SetCollisionResponseToChannel(
			ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	}

	UWorld* World = GetWorld();

	// 遍历所有命中点，检测是否击中头部
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				// 将 headshot 的结果保存在 ShotgunResult 结构体中
				if (OutShotgunResult.HeadShots.Contains(BlasterCharacter))
				{
					OutShotgunResult.HeadShots[BlasterCharacter]++;
				}
				else
				{
					OutShotgunResult.HeadShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}

	// 为所有 FramePakcages 中角色的 HitBoxes 启用除头部碰撞体以外的碰撞体
	for (auto& Frame : FramePackages)
	{
		for (auto& HitBoxPair : Frame.Character->HitCollisionBoxes)
		{
			if (HitBoxPair.Value != nullptr)
			{
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
			}
		}
		HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 遍历所有命中点，检测是否击中身体
	for (auto& HitLocation : HitLocations)
	{
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World)
		{
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECollisionChannel::ECC_Visibility
			);
			ABlasterCharacter* BlasterCharacter = Cast<ABlasterCharacter>(ConfirmHitResult.GetActor());
			if (BlasterCharacter)
			{
				// 将 bodyshot 的结果保存在 ShotgunResult 结构体中
				if (OutShotgunResult.BodyShots.Contains(BlasterCharacter))
				{
					OutShotgunResult.BodyShots[BlasterCharacter]++;
				}
				else
				{
					OutShotgunResult.BodyShots.Emplace(BlasterCharacter, 1);
				}
			}
		}
	}

	// 恢复所有 FramePakcages 中角色的 HitBoxes 信息，恢复角色本体的碰撞
	for (auto& Frame : CurrentFrames)
	{
		ResetHitBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::QueryAndPhysics);
	}

}

#pragma endregion


#pragma region Helpers

/// @brief 缓存 HitCharacter 当前的 HitBoxes 信息
/// @param HitCharacter 要缓存的角色
/// @param OutFramePackage 保存 HitBoxes 信息的结构体
void ULagCompensationComponent::CacheBoxPositions(
	ABlasterCharacter* HitCharacter, FFramePackage& OutFramePackage)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			FBoxInformation BoxInfo;
			BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			// BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			// BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			BoxInfo.Radius = HitBoxPair.Value->GetScaledSphereRadius();
			OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

/// @brief 将 HitCharacter 的 HitBoxes 移动到 Package 中保存的位置
/// @param HitCharacter 目标角色
/// @param Package 保存 HitBoxes 信息的结构体
void ULagCompensationComponent::MoveBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			// HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			// HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			HitBoxPair.Value->SetSphereRadius(Package.HitBoxInfo[HitBoxPair.Key].Radius);
		}
	}
}

/// @brief 将 HitCharacter 的 HitBoxes 恢复到 Package 中保存的位置
/// @param HitCharacter 目标角色
/// @param Package 保存 HitBoxes 信息的结构体
void ULagCompensationComponent::ResetHitBoxes(ABlasterCharacter* HitCharacter, const FFramePackage& Package)
{
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes)
	{
		if (HitBoxPair.Value != nullptr)
		{
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			// HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			// HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			HitBoxPair.Value->SetSphereRadius(Package.HitBoxInfo[HitBoxPair.Key].Radius);
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

/// @brief 将 HitCharacter 的碰撞体设置为 CollisionEnabled 类型
/// @param HitCharacter 目标角色
/// @param CollisionEnabled 碰撞体类型
void ULagCompensationComponent::EnableCharacterMeshCollision(ABlasterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled)
{
	if (HitCharacter && HitCharacter->GetMesh())
	{
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}


/// @brief 显示 FramePackage 中的碰撞体信息
/// @param Package  保存 HitBoxes 信息的结构体
/// @param Color  显示的颜色
void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color)
{
	for (auto& BoxInfo : Package.HitBoxInfo)
	{
		// DrawDebugBox(
		// 	GetWorld(),
		// 	BoxInfo.Value.Location,
		// 	// BoxInfo.Value.BoxExtent,
		// 	FVector(BoxInfo.Value.Radius),
		// 	FQuat(BoxInfo.Value.Rotation),
		// 	Color,
		// 	true
		// );

		DrawDebugSphere(
			GetWorld(),
			BoxInfo.Value.Location,
			BoxInfo.Value.Radius,
			8,
			Color,
			false,
			4.f
		);
	}
}

#pragma endregion
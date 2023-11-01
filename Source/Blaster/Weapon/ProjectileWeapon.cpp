// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Projectile.h"

void AProjectileWeapon::Fire(const FVector& HitTarget)
{
	Super::Fire(HitTarget);

	// Weapon 被设置为了 Replicate，所以只有服务器端才能发射子弹
	// 但 Projectile 也被设置为了 Replicate，所以服务器端生成的 Projectile 会被复制到客户端
	// if (!HasAuthority()) return;

	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName(FName("MuzzleFlash"));
	UWorld* World = GetWorld();
	if (MuzzleFlashSocket && World)
	{
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		// From muzzle flash socket to hit location from TraceUnderCrosshairs
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = InstigatorPawn;

		AProjectile* SpawnedProjectile = nullptr;
		if (bUseServerSideRewind)
		{
			// server
			if (InstigatorPawn->HasAuthority()) 
			{
				// 服务器端自己控制的角色，无需 SSR，发射 Replicated Projectile 同步到客户端即可
				if (InstigatorPawn->IsLocallyControlled()) 
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					SpawnedProjectile->Damage = Damage;
					// SpawnedProjectile->bUseServerSideRewind = false;
				}
				// 服务器端的其他角色，需要生成不会复制到客户端的本地 Projectile，需要使用 SSR
				else 
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ServerSideRewindProjectileClass, SocketTransform.GetLocation(), 
						TargetRotation, SpawnParams);
					// 由于服务器端的伤害计算条件是 
					// OwnerCharacter->HasAuthority() && (!bUseServerSideRewind || OwnerCharacter->IsLocallyControlled())
					// 如果这里设置为 bUseServerSideRewind = false 会导致客户端发射的子弹多造成一次伤害
					// 客户端本地调用 Rewind 造成伤害 + 服务器端的直接伤害
					// 因此这里需要设置为 bUseServerSideRewind = true
					// SpawnedProjectile->bUseServerSideRewind = true;
				}
				
			}
			// client, using SSR
			else 
			{
				// 客户端自己控制的角色，需要使用 SSR，并在本地生成 Projectile
				if (InstigatorPawn->IsLocallyControlled()) 
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					
					// 设置用于 PathPrediction 的参数
					SpawnedProjectile->TraceStart = SocketTransform.GetLocation();
					SpawnedProjectile->InitialVelocity = 
						SpawnedProjectile->GetActorForwardVector() * SpawnedProjectile->InitialSpeed;
					
					// 这里设置 Damage 实际上并不能起到作用，计算伤害使用 Server 端的数据
					SpawnedProjectile->Damage = Damage;
					// SpawnedProjectile->bUseServerSideRewind = true;
				}
				// 客户端的其他角色，不需要 SSR，生成本地的 Projectile 即可
				else 
				{
					SpawnedProjectile = World->SpawnActor<AProjectile>(
						ServerSideRewindProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
					// 客户端其他角色的 bUseServerSideRewind 设置也无关紧要，因为 Rewind 的伤害判定要经过
					// IsLocallyControlled() 的判断，否则不会造成伤害
					// SpawnedProjectile->bUseServerSideRewind = false;
				}
			}
		}
		// 不使用 SSR 的情况均转到 Server 生成 Replicated Projectile 再同步到客户端
		else 
		{
			if (InstigatorPawn->HasAuthority())
			{
				SpawnedProjectile = World->SpawnActor<AProjectile>(
					ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams);
				// SpawnedProjectile->bUseServerSideRewind = false;
				SpawnedProjectile->Damage = Damage;
			}
		}

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, 
			FString::Printf(TEXT("UseServerSideRewind: %d"), bUseServerSideRewind));

		// 统一将 Projectile 的 bUseServerSideRewind 设置为 Weapon 的 bUseServerSideRewind
		if (SpawnedProjectile) SpawnedProjectile->bUseServerSideRewind = bUseServerSideRewind;
	}
}

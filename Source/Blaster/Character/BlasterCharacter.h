// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Blaster/BlasterTypes/TurningInPlace.h"
#include "Blaster/Interfaces/InteractWithCrosshairsInterface.h"
#include "Components/TimelineComponent.h"
#include "Blaster/BlasterTypes/CombatState.h"
#include "BlasterCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLeftGame);

UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

public:
	ABlasterCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	void         PlayFireMontage(bool bAiming);
	void 	   	 PlayReloadMontage();	
	void		 PlayElimMontage();
	void         PlayHitReactMontage();
	void         PlayThrowGrenadeMontage();
	void         GrenadeButtonPressed();

	// 使用 Replicate 代替了 RPC
	// UFUNCTION(NetMulticast, Unreliable)
	// void MulticastHit();

	virtual void OnRep_ReplicatedMovement() override;
	
	void Elim(bool bPlayerLeftGame);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim(bool bPlayerLeftGame);

	virtual void Destroyed() override;

	UFUNCTION(BlueprintImplementableEvent)  // 在蓝图中实现
	void ShowSniperScopeWidget(bool bShowScope);

	void UpdateHUDHealth();
	void UpdateHUDShield();
	void UpdateHUDAmmo();

	void SpawnDefaultWeapon();

	UPROPERTY()
	// TMap<FName, class UBoxComponent*> HitCollisionBoxes;
	TMap<FName, class USphereComponent*> HitCollisionBoxes;

protected:
	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void ReloadButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void AimOffset(float DeltaTime);
	void CalculateAO_Pitch();
	void SimProxiesTurn();

	void FireButtonPressed();
	void FireButtonReleased();

	virtual void Jump() override;

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, 
		class AController* InstigatorController, AActor* DamageCauser);

	// Poll for any relelvant classes and initialize our HUD
	void PollInit();

	// Invicible 不需要同步，因为打击判定是在服务器上进行的，通过 Controller 设置该参数
	// 会让 Server 上的 Controller->Pawn 也变成无敌，因此逻辑不会有问题。
	bool bInvincible = false;

private:

	/** 
	* Blaster components
	*/

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCombatComponent* Combat;

	UPROPERTY(VisibleAnywhere)
	class UBuffComponent* Buff;

	UPROPERTY(VisibleAnywhere)
	class ULagCompensationComponent* LagCompensation;

	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;

	UFUNCTION()
	void OnRep_OverlappingWeapon(AWeapon* LastWeapon);

	// UPROPERTY(Replicated)
	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime);

	/** 
	* Animation montages
	*/

	UPROPERTY(EditAnywhere, Category = Combat)
	class UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ReloadMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ElimMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	void HideCameraIfCharacterClose();

	UPROPERTY(EditAnywhere)
	float CameraThreshold = 200.f;

	bool bRotateRootBone;
	float TurnThreshold = 0.5f;
	FRotator ProxyRotationLastFrame;
	FRotator ProxyRotation;
	float ProxyYaw;
	float TimeSinceLastMovementReplication;
	float CalculateSpeed();

	/**
	* Player health
	*/

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health, VisibleAnywhere, Category = "Player Stats")
	float Health = 100.f;

	// TODO: 接收参数是怎么回事？
	UFUNCTION()
	void OnRep_Health(float LastHealth);

	/** 
	* Player shield
	*/

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxShield = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Shield, EditAnywhere, Category = "Player Stats")
	float Shield = 0.f;

	UFUNCTION()
	void OnRep_Shield(float LastShield);


	UPROPERTY()
	class ABlasterPlayerController* BlasterPlayerController;

	bool bElimmed = false;

	FTimerHandle ElimTimer;

	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 2.f;

	void ElimTimerFinished();

	/**
	* Dissolve effect
	*/

	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	// Dynamic instance that we can change at runtime
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	// Material instance set on the Blueprint, used with the dynamic material instance
	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;

	/**
	* Elim bot
	*/

	UPROPERTY(EditAnywhere)
	UParticleSystem* ElimBotEffect;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;

	UPROPERTY(EditAnywhere)
	class USoundCue* ElimBotSound;

	UPROPERTY(VisibleAnywhere)
	class ABlasterPlayerState* BlasterPlayerState;

	/** 
	* Grenade
	*/

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

	/** 
	* Default weapon
	*/

	UPROPERTY(EditAnywhere)
	TSubclassOf<AWeapon> DefaultWeaponClass;

protected:  // Hit boxes used for server-side rewind

	void SetHitBoxes();

	UPROPERTY(EditAnywhere)
	class USphereComponent* head1;

	UPROPERTY(EditAnywhere)
	class USphereComponent* spine_01;

	UPROPERTY(EditAnywhere)
	class USphereComponent* spine1_02;

	UPROPERTY(EditAnywhere)
	class USphereComponent* spine_03;

	UPROPERTY(EditAnywhere)
	class USphereComponent* pelvis;


	// UPROPERTY(EditAnywhere)
	// UBoxComponent* pelvis;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* spine_02;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* spine_03;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* upperarm_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* upperarm_r;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* lowerarm_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* lowerarm_r;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* hand_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* hand_r;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* backpack;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* blanket;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* thigh_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* thigh_r;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* calf_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* calf_r;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* foot_l;

	// UPROPERTY(EditAnywhere)
	// UBoxComponent* foot_r;

public:	// Getter & Setter
	void SetOverlappingWeapon(AWeapon* Weapon);
	bool IsWeaponEquipped() const;
	bool IsAiming();
	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	AWeapon* GetEquippedWeapon() const;
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	FVector GetHitTarget() const;
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
	
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE void SetHealth(float Amount) { Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
	FORCEINLINE float GetShield() const { return Shield; }
	FORCEINLINE void SetShield(float Amount) { Shield = Amount; }
	FORCEINLINE float GetMaxShield() const { return MaxShield; }
	
	ECombatState GetCombatState() const;
	FORCEINLINE UCombatComponent* GetCombat() const { return Combat; }

	FORCEINLINE bool IsInvincible() const { return bInvincible; }
	void SetInvincible(bool Invincible) { bInvincible = Invincible; }

	FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
	FORCEINLINE UStaticMeshComponent* GetAttachedGrenade() const { return AttachedGrenade; }
	FORCEINLINE UBuffComponent* GetBuff() const { return Buff; }
	
	bool IsLocallyReloading();

	FORCEINLINE ULagCompensationComponent* GetLagCompensation() const { return LagCompensation; }


// ========================= swap weapon ========================= // 
private:
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* SwapMontage;

public:
	void PlaySwapMontage();
	bool bFinishedSwapping = false;

// ========================= leave game ========================= //
public:	
	UFUNCTION(Server, Reliable)
	void ServerLeaveGame();

	FOnLeftGame OnLeftGame;

private:
	bool bLeftGame = false;

// ============================ Crown =========================== //
private:
	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* CrownSystem;

	UPROPERTY()
	class UNiagaraComponent* CrownComponent;

public:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastGainedTheLead();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastLostTheLead();

};

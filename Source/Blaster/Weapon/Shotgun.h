// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HitScanWeapon.h"
#include "Shotgun.generated.h"

/**
 * 
 */
UCLASS()
class BLASTER_API AShotgun : public AHitScanWeapon
{
	GENERATED_BODY()
public:
	// shotgun 采取和父类不同的射击方式
	// virtual void Fire(const FVector& HitTarget) override;
	// void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector>& HitTargets);

	virtual void FireShotgun(const TArray<FVector_NetQuantize>& HitTargets);
	void ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets);

private:

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	uint32 NumberOfPellets = 10;
};

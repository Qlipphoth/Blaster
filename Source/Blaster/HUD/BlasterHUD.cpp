// Fill out your copyright notice in the Description page of Project Settings.


#include "BlasterHUD.h"

void ABlasterHUD::DrawHUD()
{
    Super::DrawHUD();

    FVector2D ViewportSize;
	if (GEngine)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
        float SpreadScaled = HUDPackage.CrosshairSpread * CrosshairSpreadMax;

		if (HUDPackage.CrosshairsCenter)
		{
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, FVector2D(0.f, 0.f));
		}
		if (HUDPackage.CrosshairsLeft)
		{
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, FVector2D(-SpreadScaled, 0.f));
		}
		if (HUDPackage.CrosshairsRight)
		{
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, FVector2D(SpreadScaled, 0.f));
		}
		if (HUDPackage.CrosshairsTop)
		{
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, FVector2D(0.f, -SpreadScaled));
		}
		if (HUDPackage.CrosshairsBottom)
		{
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, FVector2D(0.f, SpreadScaled));
		}
	}
}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread)
{
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
    // 调整位置至正中央
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - (TextureWidth / 2.f) + Spread.X,
		ViewportCenter.Y - (TextureHeight / 2.f) + Spread.Y
	);

	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f,
		0.f,
		1.f,
		1.f,
		FLinearColor::White
	);
}
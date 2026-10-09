#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DroneHUD.generated.h"
UCLASS()
class PROIECT_ECLIPSA_API ADroneHUD : public AHUD
{
	GENERATED_BODY()
public:
	ADroneHUD();
	virtual void DrawHUD() override;
	void DrawThickLine(UCanvas* Canvas, float X1, float Y1, float X2, float Y2, float Thickness, FLinearColor Color);
	bool bDrawTelemetry = true; //bool care imi spune daca desenez hud-ul sau nu
	UPROPERTY() class UVideoExportMenuWidget* VideoExportMenuWidget;
	UPROPERTY() class UCameraMenuWidget* CameraMenuWidget;
	TMap<FString, bool> VisibilityCache;
	//aici sunt timere pentru optimizari, sa nu stau sa calculez in permanenta tot
	float RaycastTimer = 0.0f;
	float LastValidAGL = 0.0f;
	float TimeSinceLastHit = 0.0f;
	float CachedUI = 1.0f;
	UPROPERTY()
	class AOrbitalManager* CachedOrbManager = nullptr;
	//texturi 2d pentru cele 4 iconite
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* IconitaHarta;
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* IconitaMeniu;
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* IconitaNori;
	UPROPERTY(EditAnywhere, Category = "HUD")
	class UTexture2D* IconitaPauza;
	bool bWasZoomedIn = false;

	UFUNCTION()
	void DrawTextWithShadow(FString Text, FLinearColor Color, float X, float Y, float Scale);

	//nou adaugate pentru optimizare
	UPROPERTY()
	TWeakObjectPtr<AActor> CachedCloudActor = nullptr;

	bool bCachedSunBlocked = false;
	float SunTraceTimer = 0.0f;
private:
	UPROPERTY()
	UFont* HUDFont;
protected:
	virtual void BeginPlay() override;

};

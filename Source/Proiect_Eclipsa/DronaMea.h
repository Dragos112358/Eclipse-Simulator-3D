#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/UserWidget.h"
#include "EclipseWidget.h"
#include "EclipseBrowserWidget.h"
#include "DronaMea.generated.h"
UCLASS()
class PROIECT_ECLIPSA_API ADronaMea : public APawn
{
	GENERATED_BODY()
public:
	ADronaMea();
	float CurrentFogIntensity = 0.0f; //intensitate subunitara, ca si pentru ploaie si ninsoare
protected:
	virtual void BeginPlay() override;
	void TeleportToMoonSurface();
	class AExponentialHeightFog* FogActorRef; //referinta la exponential height fog, pe care am pus-o in unreal cu bounds infinit

	//void OpresteServerPython();
	//FDelegateHandle EnginePreExitHandle;
public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	FProcHandle PythonServerHandle;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//parti drona
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* DroneMesh; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PropellerFL; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PropellerFR; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PropellerBL; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* PropellerBR; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	class UCameraComponent* Camera;
	float AirDrag; //frecare cu aerul, valoare mica pentru viteze mari
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightPhysics")
	FVector CurrentVelocity; //viteza
	FVector InputDirection; //directia data din taste
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightPhysics")
	float Acceleration;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlightPhysics")
	float MaxSpeed;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapMode")
	USpringArmComponent* MapSpringArm; //mapspring arm pentru harta
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapMode")
	UCameraComponent* MapCamera; 
	bool bMapModeActive; //bool foarte folosit in codul meu, deoarece verific daca sunt in modul third person sau mod harta
	UPROPERTY()
	UMainMenuWidget* MainMenuWidget;
	//functii pentru deplasare si rotatie pamant
	void MoveForward(float Val);
	void MoveRight(float Val);
	void MoveUp(float Val);
	void Turn(float Val);
	void LookUp(float Val);
	void AdjustSpeed(float Val);
	void HandleEclipseInput();
	UFUNCTION()
	void ToggleHelpMenu(); //pentru a apasa f10 si a da toggle in meniul help
	void ToggleMap(); //legat de bmapmodeactive, schimba valoarea        
	void MapZoom(float Val); //functie de dat zoom pe harta sau pe modul third person
	void TeleportClick(); //teleportare pe harta sau click pe ecran pe o planeta/satelit natural
	void UpdateEclipseWidgetText(FString NewText);
	UPROPERTY()
	UEclipseWidget* EclipseInfoWidget;
	bool bSearchingForGround = false; //bool care are rolul sa imi spuna cand caut suprafata Pamantului de la cesium
	float SearchTimeout = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	FString TrackingTarget = "Libera (Niciunul)";
	UPROPERTY()
	AActor* SunActorRef;
	UPROPERTY()
	AActor* MoonActorRef;
	void UpdateAutoTracking(float DeltaTime); 
	//booluri pentru meniul overlays si telemetrie
	bool bShowAzimuthGrid = false;
	bool bShowEquatorialGrid = false;
	bool bShowPlanets = false;
	bool bShowAstroText = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setari Control")
	float SensibilitateMouse = 1.0f; //sensibilitate aleasa tot din setari
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setari Control")
	bool bInvertMouseY = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vreme")
	class UNiagaraComponent* RainComponent; //ploaie, sistem niagara
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vreme")
	class UNiagaraComponent* SnowComponent; //ninsoare
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapCapture")
	class USceneCaptureComponent2D* MapCaptureComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapCapture")
	class UTextureRenderTarget2D* MapRenderTarget;
	UFUNCTION()
	void Export2DMapToDisk();  //exportare harta cu banda de totalitate
	UPROPERTY() bool bShowEclipsePath = false;
	TArray<FVector> EclipsePathCentral;
	TArray<FVector> EclipsePathNorth;
	TArray<FVector> EclipsePathSouth;
	void GenerateTestEclipsePath();
	void DrawEclipsePathOnGlobe();
	// În fișierul .h
	const float RebuildInterval = 1.0f / 30.0f; // Adică aprox 0.0333s
	float TimeSinceLastOriginRebase = 0.0f;
	bool bIsDraggingMap = false;
	bool bHasMovedDuringDrag = false;
	FVector2D LastMousePosition;
	FVector2D StartClickPosition;
	UPROPERTY(EditAnywhere, Category = "Map Controls")
	float MapDragSensitivity = 0.005f;
	void StartMapDrag();
	void StopMapDrag();
	void MoveMap(float ForwardVal, float RightVal);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	class USpringArmComponent* MinimapSpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	class USceneCaptureComponent2D* MinimapCapture;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	class UTextureRenderTarget2D* MinimapRenderTarget;
	//valori pentru cele 4 butoane de sus din hud
	float LastMapToggleTime = 0.0f;
	float LastXavToggleTime = 0.0f;
	float LastCloudToggleTime = 0.0f;
	float LastPauseToggleTime = 0.0f;
	//alte valori reglabile
	float SavedTimeSpeed = 1.0f;
	float SavedFogValue = 0.0f;
	bool bIsRainActive = false;
	bool bIsSnowActive = false;
	float LastTeleportTime = 0.0f;
	float SavedMaxFPS; // Variabilă pentru a salva FPS-ul anterior
	// Adaugă variabilele necesare
	FVector2D AccumulatedMovement;
	bool bNeedsOriginUpdate = false;
	// În DronaMea.h
	//void UpdateMapOrigin(); // Noua funcție care aplică mișcarea
	UPROPERTY() bool bDragThreadLagDisabled = false;
	UPROPERTY() int32 MapUpdateCounter = 0;

	UPROPERTY() FVector2D AccumulatedMapMovement = FVector2D::ZeroVector;
	//UPROPERTY() float TimeSinceLastOriginRebase = 0.0f;
	UPROPERTY() float OriginRebaseInterval = 0.05f; // maxim 30 rebase-uri/secunda, indiferent de FPS-ul jocului

	UPROPERTY()
	UMaterialInterface* MaterialDronaSalvat;
	//valori pentru setare densitate nori
	float SavedCloudSliderValue = 0.3f;
	float CurrentCloudDensity = -0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float SavedCloudAltSliderValue = 0.4f; // Ex: 0.4 ar insemna cam 4-5 km
protected:
	float TimeFlowSpeed = 0.0f;
	//lucru cu timp
	void SpeedUpTime();
	void SlowDownTime();
	void ResetTimeSpeed();
	void DrawAstronomicalGrids();
	void SetCameraTPV2(); //functie utila, seteaza corect directia camerei si a dronei
};

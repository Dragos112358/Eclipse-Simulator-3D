#include "DronaMea.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/BoxComponent.h" 
#include "Framework/Application/SlateApplication.h" 
#include "GameFramework/PlayerController.h"
#include "Moon.h"
#include "DrawDebugHelpers.h"
#include "CesiumGeoreference.h"
#include "GameFramework/InputSettings.h"
#include "DroneHUD.h"
#include "GameFramework/HUD.h"
#include <Kismet/GameplayStatics.h>
#include "Blueprint/UserWidget.h"
#include "EclipseWidget.h"
#include "EclipseBrowserWidget.h"
#include "Components/DirectionalLightComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EclipseBrowserWidget.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Components/SkyLightComponent.h"
#include "Misc/FileHelper.h"
#include "Engine/Canvas.h"
#include "GameFramework/GameUserSettings.h"
#include "EclipseBrowserWidget.h"
#include "EclipseBrowserWidget.h"
#include "Misc/CoreDelegates.h" //pentru oprirea serverului de python
ADronaMea::ADronaMea()
{
	PrimaryActorTick.bCanEverTick = true;
	bMapModeActive = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	UBoxComponent* CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	//aici aleg dimensiunea dronei, sa fie o cutie, pentru ca drona sa aiba coliziuni
	CollisionBox->SetBoxExtent(FVector(40.0f, 40.0f, 15.0f));
	CollisionBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionBox->SetHiddenInGame(true);
	DroneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneMesh"));
	DroneMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyAsset(TEXT("/Script/Engine.StaticMesh'/Game/SM_Drone2.SM_Drone2'"));
	//aici am incarcat doar corpul dronei (vreau ca drona si elicele sa se poata misca independent)
	if (BodyAsset.Succeeded())
	{
		DroneMesh->SetStaticMesh(BodyAsset.Object);
		DroneMesh->SetWorldScale3D(FVector(0.5f)); //scale mic, modelul importat de pe sketchfab era prea mare
	}
	//implementez fizica mea pe drona
	DroneMesh->SetSimulatePhysics(false);
	DroneMesh->SetCollisionProfileName(TEXT("NoCollision"));
	//aici incarc elicele (4 elice care au diverse viteze in functie de cum merge drona)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PropAsset(TEXT("/Script/Engine.StaticMesh'/Game/SM_Merged.SM_Merged'"));
	auto CreateProp = [&](FString Name, UStaticMeshComponent*& PropComp, FName SocketName)
		{
			PropComp = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
			PropComp->SetupAttachment(DroneMesh, SocketName);
			PropComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (PropAsset.Succeeded())
			{
				PropComp->SetStaticMesh(PropAsset.Object);
				PropComp->SetRelativeScale3D(FVector(0.20f));
				PropComp->SetRelativeLocation(FVector::ZeroVector);
				PropComp->SetRelativeRotation(FRotator::ZeroRotator);
			}
		};
	//asezare elice in socketuri
	CreateProp("PropFL", PropellerFL, TEXT("Socket_FL"));
	CreateProp("PropFR", PropellerFR, TEXT("Socket_FR"));
	CreateProp("PropBL", PropellerBL, TEXT("Socket_BL"));
	CreateProp("PropBR", PropellerBR, TEXT("Socket_BR"));
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	//creare springarm pentru a vedea drona in third person
	SpringArm->TargetArmLength = 600.0f;
	SpringArm->SocketOffset = FVector(0, 0, 150);
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = true;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.0f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	//map spring arm are rolul sa priveasca drona de sus (perpendicular pe suprafata pamantului din cesium)
	MapSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("MapSpringArm"));
	MapSpringArm->SetupAttachment(RootComponent);
	MapSpringArm->SetAbsolute(false, true, false);
	MapSpringArm->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
	MapSpringArm->TargetArmLength = 10000000.0f;
	MapSpringArm->bDoCollisionTest = false;
	MapCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("MapCamera"));
	MapCamera->SetupAttachment(MapSpringArm, USpringArmComponent::SocketName);
	MapCamera->SetActive(false);
	//acceleration este impropriu spus, de fapt este puterea motorului, care este de 15000 KW sau 20.11 cai-putere
	Acceleration = 15000.0f;
	AirDrag = 0.00025f; //rezistenta cu aerul (daca aveam valoare mare aici, drona nu mai prindea viteza mare)
	MaxSpeed = 10000.0f;
	CurrentVelocity = FVector::ZeroVector;
	RainComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RainComponent"));
	RainComponent->SetupAttachment(RootComponent);
	//incarcare sistem niagara pentru ploaie
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RainAsset(TEXT("/Script/Niagara.NiagaraSystem'/Game/Textures/NS_Rain.NS_Rain'"));
	if (RainAsset.Succeeded())
	{
		RainComponent->SetAsset(RainAsset.Object);
	}
	//20 de metri deasupra mea, de asemenea parametrul user.intensity va reprezenta un slider in meniul meu UI, pe care utilizatorul il poate controla
	RainComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2000.0f));
	RainComponent->SetFloatParameter(FName("User.Intensity"), 0.0f);
	RainComponent->SetAutoActivate(false);
	RainComponent->SetVisibility(false);
	SnowComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SnowComponent"));
	SnowComponent->SetupAttachment(RootComponent);
	//exact la fel si pentru ninsoare, poate fi setata de user, folosind paramtrul subunitar intensity
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SnowAsset(TEXT("/Script/Niagara.NiagaraSystem'/Game/Textures/NS_Snow.NS_Snow'"));
	if (SnowAsset.Succeeded())
	{
		SnowComponent->SetAsset(SnowAsset.Object);
	}
	SnowComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2000.0f));
	SnowComponent->SetFloatParameter(FName("User.Intensity"), 0.0f);
	SnowComponent->SetAutoActivate(false);
	SnowComponent->SetVisibility(false);
	//aici setez camera pentru modul map (privire de sus, de aceea rotesc cu 90 de grade pe axa X, ca sa privesc perpendicular pe sol)
	MapCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MapCaptureComponent"));
	MapCaptureComponent->SetupAttachment(RootComponent);
	MapCaptureComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 2000000000.0f));
	MapCaptureComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	MapCaptureComponent->bCaptureEveryFrame = false;
	MapCaptureComponent->bCaptureOnMovement = false;
	MinimapSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("MinimapSpringArm"));
	MinimapSpringArm->SetupAttachment(RootComponent);
	MinimapSpringArm->SetUsingAbsoluteRotation(true);
	MinimapSpringArm->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
	MinimapSpringArm->TargetArmLength = 150000.0f;
	MinimapSpringArm->bDoCollisionTest = false;
	MinimapCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("MinimapCapture"));
	MinimapCapture->SetupAttachment(MinimapSpringArm, USpringArmComponent::SocketName);
	MinimapCapture->bCaptureEveryFrame = true;
	MinimapCapture->bCaptureOnMovement = true;
	MinimapCapture->ProjectionType = ECameraProjectionMode::Perspective;
	MinimapCapture->FOVAngle = 60.0f;
	MinimapCapture->ShowFlags.SetAtmosphere(false);
	MinimapCapture->ShowFlags.SetFog(false);
	MinimapCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	MinimapCapture->HiddenActors.Add(this);
	//aici extrag materialul negru total
	//problema a fost ca drona originala reflecta lumina soarelui foarte puternic (soarele din Unreal are 110.000 Lux), deci orice reflexie orbea camera
	//de aceea am creat eu un material custom in unreal, cu scopul de a fi complet negru
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Script/Engine.Material'/Game/Meshes/M_Negru_Total.M_Negru_Total'"));
	if (MatFinder.Succeeded())
	{
		MaterialDronaSalvat = MatFinder.Object;
	}
	else
	{
		MaterialDronaSalvat = nullptr;
	}
}
void ADronaMea::BeginPlay()
{
	/* Aceasta functie este foarte importanta, deoarece comunic cu serverul de python, extrag datele salvate in fisierul config.ini si setez camerele (atat cea
	* care priveste de sus, cat si cea normala third-person care urmareste drona). De asemenea, pun toate valorile sliderelor de la vreme pe 0
	*/
	Super::BeginPlay();
	/* In Encoder, am salvat absolut toate asseturile pe care vreau sa le includ in executabilul final, inclusiv serverul meu de python, care ruleaza simultan cu executabilul
	* Unreal. Folosesc CreateProc pentru a apela Server.exe din folderul Encoder. Mentionez ca am folosit PyInstaller pentru a face fisierul .py in .exe
	* Am ales sa includ si acest executabil direct in executabilul mare Unreal, deoarece nu puteam rula un fisier facut .exe din python fara sa imi apara alerte de securitate.
	*/
	FString ServerDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Encoder"));
	FString ServerPath = FPaths::Combine(ServerDir, TEXT("Server.exe"));
	ServerDir = FPaths::ConvertRelativePathToFull(ServerDir);
	ServerPath = FPaths::ConvertRelativePathToFull(ServerPath);
	FPaths::MakePlatformFilename(ServerDir);
	FPaths::MakePlatformFilename(ServerPath);
	if (FPaths::FileExists(ServerPath))
	{
		PythonServerHandle = FPlatformProcess::CreateProc(
			*ServerPath,
			TEXT(""),
			true,
			true,
			true,
			nullptr,
			0,
			*ServerDir,
			nullptr
		);
		if (PythonServerHandle.IsValid())
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Serverul Python a pornit cu succes!"));
			//EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddUObject(this, &ADronaMea::OpresteServerPython);
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, TEXT("EROARE: Windows a refuzat pornirea (Verifica Task Manager!)."));
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("EROARE: Server.exe nu a fost gasit!"));
	}

	FCoreDelegates::OnEnginePreExit.AddLambda([]()
		{
			FPlatformProcess::CreateProc(
				TEXT("taskkill.exe"),
				TEXT("/F /T /IM Server.exe"),
				true, true, true, nullptr, 0, nullptr, nullptr
			);
		});
	/*aici mapez tastele sa faca actiuni, m trece de la third person la privire de sus
	* MapClick ma poate duce in alta locatie pe modul harta sau daca sunt la sol si dau click pe o planeta/satelit, o voi urmari
	  OpenHelp este accesibila de pe f10, de regula f1 era tasta buna de help, dar unreal are default pe f1 alta setare
	*/
	UInputSettings* Settings = UInputSettings::GetInputSettings();
	if (Settings)
	{
		bool bNeedSave = false;
		if (!Settings->GetActionMappings().ContainsByPredicate([](const FInputActionKeyMapping& Map) { return Map.ActionName == "ToggleMap"; })) {
			Settings->AddActionMapping(FInputActionKeyMapping("ToggleMap", EKeys::M)); bNeedSave = true;
		}
		if (!Settings->GetActionMappings().ContainsByPredicate([](const FInputActionKeyMapping& Map) { return Map.ActionName == "MapClick"; })) {
			Settings->AddActionMapping(FInputActionKeyMapping("MapClick", EKeys::LeftMouseButton)); bNeedSave = true;
		}
		if (!Settings->GetAxisMappings().ContainsByPredicate([](const FInputAxisKeyMapping& Map) { return Map.AxisName == "MapZoom"; })) {
			Settings->AddAxisMapping(FInputAxisKeyMapping("MapZoom", EKeys::MouseWheelAxis)); bNeedSave = true;
		}
		if (!Settings->GetActionMappings().ContainsByPredicate([](const FInputActionKeyMapping& Map) { return Map.ActionName == "OpenHelp"; })) {
			Settings->AddActionMapping(FInputActionKeyMapping("OpenHelp", EKeys::F10)); bNeedSave = true;
		}
		if (bNeedSave) {
			Settings->SaveKeyMappings();
			for (TObjectIterator<UPlayerInput> It; It; ++It) It->ForceRebuildingKeyMaps(true);
		}
	}
	if (GConfig)
	{
		float SavedLat = 44.43f;
		float SavedLon = 26.00f;
		float SavedAlt = 200.0f;
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnLat"), SavedLat, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnLon"), SavedLon, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnAlt"), SavedAlt, GGameUserSettingsIni);
		ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
		if (GeoRef)
		{
			GeoRef->SetOriginLongitudeLatitudeHeight(FVector(SavedLon, SavedLat, SavedAlt));
		}
		this->SetActorLocation(FVector(0.f, 0.f, 0.f));
	}
	if (GConfig)
	{
		int32 Y = 2024, M = 3, D = 21;
		int32 UH = 10, UMin = 0, USec = 0;
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnYear"), Y, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMonth"), M, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnDay"), D, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnHourUTC"), UH, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMinuteUTC"), UMin, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnSecondUTC"), USec, GGameUserSettingsIni);
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager)
		{
			float SelectedHourFloat = (float)UH + (UMin / 60.0f) + (USec / 3600.0f);
			Manager->SetSimulationDateTime(D, M, Y, SelectedHourFloat);
		}
	}
	//aici setez materialele dronei sa aiba roughness 1, ca sa nu straluceasca
	/*if (DroneMesh && MaterialDronaSalvat)
	{
		int32 MaterialCount = DroneMesh->GetNumMaterials();
		for (int32 i = 0; i < MaterialCount; i++)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(MaterialDronaSalvat, this);
			if (DynMat)
			{
				DynMat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
				FLinearColor CuloareAleasa = (i == 1) ? FLinearColor(0.2f, 0.0f, 1.0f, 1.0f) : FLinearColor::Black;
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), CuloareAleasa);
				DroneMesh->SetMaterial(i, DynMat);
			}
		}
		UMaterialInstanceDynamic* PropMat = UMaterialInstanceDynamic::Create(MaterialDronaSalvat, this);
		if (PropMat) {
			PropMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.01f, 0.01f, 0.01f, 1.0f));
			if (PropellerFL) PropellerFL->SetMaterial(0, PropMat);
			if (PropellerFR) PropellerFR->SetMaterial(0, PropMat);
			if (PropellerBL) PropellerBL->SetMaterial(0, PropMat);
			if (PropellerBR) PropellerBR->SetMaterial(0, PropMat);
		}
	}*/

	if (DroneMesh)
	{
		TArray<FLinearColor> Culori = {
			FLinearColor(0.55f, 0.60f, 0.65f, 1.0f), // 0. Argintiu/Titan deschis (Corpul superior - reflexie puternică)
			FLinearColor(0.70f, 0.72f, 0.75f, 1.0f), // 1. Argintiu platină (Accente luminoase / margini brațe)
			FLinearColor(0.18f, 0.25f, 0.40f, 1.0f), // 2. Albastru Indigo Metalizat (Baza corpului și zona camerei)
			FLinearColor(0.12f, 0.14f, 0.16f, 1.0f), // 3. Titan întunecat (Gunmetal modern - contrast fin cu elicea)
			FLinearColor(0.35f, 0.38f, 0.40f, 1.0f), // 4. Gri titan mediu (Brațele dronei, textură satinată)
			FLinearColor(0.08f, 0.09f, 0.10f, 1.0f), // 5. Gri-negru fibră de carbon (Elicele și axul central)
			FLinearColor(0.40f, 0.30f, 0.15f, 1.0f), // 6. Bronz închis (Detalii mecanice interioare sub elice)
			FLinearColor(0.32f, 0.38f, 0.32f, 1.0f), // 7. Verde militar șters (Modulul de baterie de dedesubt)
		};

		int32 MaterialCount = DroneMesh->GetNumMaterials();
		for (int32 i = 0; i < MaterialCount; i++)
		{
			if (MaterialDronaSalvat)
			{
				UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(MaterialDronaSalvat, this);
				if (DynMat)
				{
					FLinearColor Culoare = Culori[i % Culori.Num()]; // cicleaza prin culori
					DynMat->SetVectorParameterValue(TEXT("BaseColor"), Culoare);
					DynMat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
					DroneMesh->SetMaterial(i, DynMat);
				}
			}
		}

		// Elice - negru mat
		if (MaterialDronaSalvat)
		{
			UMaterialInstanceDynamic* PropMat = UMaterialInstanceDynamic::Create(MaterialDronaSalvat, this);
			if (PropMat)
			{
				PropMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.08f, 0.09f, 0.1f, 1.0f));
				PropMat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
				if (PropellerFL) PropellerFL->SetMaterial(0, PropMat);
				if (PropellerFR) PropellerFR->SetMaterial(0, PropMat);
				if (PropellerBL) PropellerBL->SetMaterial(0, PropMat);
				if (PropellerBR) PropellerBR->SetMaterial(0, PropMat);
			}
		}
	}

	if (MapSpringArm)
	{
		MapSpringArm->SetUsingAbsoluteRotation(true);
		MapSpringArm->SetWorldRotation(FRotator(-90.0f, -90.0f, 0.0f));
	}
	if (GetWorld())
	{
		EclipseInfoWidget = CreateWidget<UEclipseWidget>(GetWorld(), UEclipseWidget::StaticClass());
		if (EclipseInfoWidget)
		{
			EclipseInfoWidget->AddToViewport();
			EclipseInfoWidget->AddToViewport(9999);
			EclipseInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		if (UEclipseWidget::StaticClass())
		{
			EclipseInfoWidget = CreateWidget<UEclipseWidget>(PC, UEclipseWidget::StaticClass());
			if (EclipseInfoWidget)
			{
				EclipseInfoWidget->AddToViewport(9999);
				EclipseInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UMainMenuWidget::StaticClass())
		{
			MainMenuWidget = CreateWidget<UMainMenuWidget>(PC, UMainMenuWidget::StaticClass());
			if (MainMenuWidget)
			{
				MainMenuWidget->AddToViewport(50);
				MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
			}
		}
	}
	TArray<AActor*> FoundSuns;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundSuns);
	for (AActor* Actor : FoundSuns)
	{
		if (Actor->GetName().Contains("CesiumSunSky"))
		{
			SunActorRef = Actor;
			break;
		}
	}
	TArray<AActor*> FoundMoons;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMoon::StaticClass(), FoundMoons);
	if (FoundMoons.Num() > 0)
	{
		MoonActorRef = FoundMoons[0];
	}
	//alte setari generale pe care le pot accesa din overlays si telemetrie
	if (GConfig)
	{
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("InversareAxaY"), bInvertMouseY, GGameUserSettingsIni);
		bool bHideHUD = false;
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bHideHUD, GGameUserSettingsIni);
	}
	float SavedSensitivity = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("Sensibilitate"), SavedSensitivity, GGameUserSettingsIni);
	}
	SensibilitateMouse = SavedSensitivity;
	TArray<AActor*> FoundFogs;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AExponentialHeightFog::StaticClass(), FoundFogs);
	if (FoundFogs.Num() > 0)
	{
		FogActorRef = Cast<AExponentialHeightFog>(FoundFogs[0]);
	}
	SavedFogValue = 0.0f;
	bIsRainActive = false;
	bIsSnowActive = false;
	MapRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	MapRenderTarget->InitCustomFormat(4096, 4096, PF_B8G8R8A8, true);
	//aici am incercat sa adaug un nou viewport in dreapta sus, dar nu am reusit, deoarece cesium este optimizat sa ofere cadre doar pt camera principala
	MapCaptureComponent->TextureTarget = MapRenderTarget;
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
	}
	MinimapRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	MinimapRenderTarget->InitCustomFormat(1024, 1024, PF_B8G8R8A8, true);
	MinimapCapture->TextureTarget = MinimapRenderTarget;

	UCameraComponent* TargetCamera = bMapModeActive ? MapCamera : Camera;
	//aici analizez cati pixeli luminosi sau intunecati exista pentru a avea contrast bun si a nu vedea ziua imaginea complet alba (arsa) si noaptea nimic
	if (TargetCamera)
	{
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		TargetCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
		TargetCamera->PostProcessSettings.AutoExposureMinBrightness = 0.001f;
		TargetCamera->PostProcessSettings.AutoExposureMaxBrightness = 20.0f;
		TargetCamera->PostProcessSettings.AutoExposureBias = 0.0f;
		TargetCamera->PostProcessSettings.bOverride_ColorContrast = true;
		TargetCamera->PostProcessSettings.bOverride_ColorGamma = true;
		TargetCamera->PostProcessSettings.ColorContrast = FVector(0.7f, 0.7f, 0.7f);
		TargetCamera->PostProcessSettings.ColorGamma = FVector(1.2f, 1.2f, 1.5f);
	}
	if (SunActorRef)
	{
		// Caută SkyLight-ul în interiorul actorului
		USkyLightComponent* SkyLightComp = Cast<USkyLightComponent>(SunActorRef->GetComponentByClass(USkyLightComponent::StaticClass()));
		if (SkyLightComp)
		{
			SkyLightComp->SetMobility(EComponentMobility::Movable);
		}
	}
}
void ADronaMea::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && !PC->bShowMouseCursor && bMapModeActive)
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
	}
	//aici caut solul
	if (bSearchingForGround)
	{
		SearchTimeout -= DeltaTime;
		FVector DroneLoc = GetActorLocation();
		FVector HighPoint = FVector(DroneLoc.X, DroneLoc.Y, 900000.0f);
		FVector LowPoint = HighPoint - FVector(0.0f, 0.0f, 2000000.0f);
		FHitResult GroundHit;
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);
		TraceParams.bTraceComplex = true;
		CurrentVelocity = FVector::ZeroVector;
		if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetRootComponent()))
		{
			RootPrim->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, HighPoint, LowPoint, ECC_Visibility, TraceParams))
		{
			FVector SafePosition = GroundHit.Location + FVector(0.0f, 0.0f, 5000.0f);
			SetActorLocation(SafePosition);
		}
		else
		{
			SetActorLocation(HighPoint);
		}
		if (SearchTimeout <= 0.0f)
		{
			bSearchingForGround = false;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("Teren HD incarcat complet. Teleportare reusita!"));
		}
		return;
	}
	/*UCameraComponent* TargetCamera = bMapModeActive ? MapCamera : Camera;
	//aici analizez cati pixeli luminosi sau intunecati exista pentru a avea contrast bun si a nu vedea ziua imaginea complet alba (arsa) si noaptea nimic
	if (TargetCamera)
	{
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMethod = true;
		TargetCamera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
		TargetCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
		TargetCamera->PostProcessSettings.AutoExposureMinBrightness = 0.001f;
		TargetCamera->PostProcessSettings.AutoExposureMaxBrightness = 20.0f;
		TargetCamera->PostProcessSettings.AutoExposureBias = 0.0f;
		TargetCamera->PostProcessSettings.bOverride_ColorContrast = true;
		TargetCamera->PostProcessSettings.bOverride_ColorGamma = true;
		TargetCamera->PostProcessSettings.ColorContrast = FVector(0.7f, 0.7f, 0.7f);
		TargetCamera->PostProcessSettings.ColorGamma = FVector(1.2f, 1.2f, 1.5f);
	}*/
	if (bMapModeActive) {
		CurrentVelocity = FVector::ZeroVector;
	}
	FVector PropulsionForce = FVector::ZeroVector;
	float ActiveBraking = 1.0f;
	if (!InputDirection.IsZero())
	{
		InputDirection.Normalize();
		FRotator MoveRot(0.0f, GetActorRotation().Yaw, 0.0f);
		PropulsionForce = MoveRot.RotateVector(InputDirection) * Acceleration;
	}
	else
	{
		ActiveBraking = 3.0f;
	}
	//formule pentru calcul acceleratiei dronei la orice moment de timp, daca apas w,a,s,d,q,e, accelereaza in directia respectiva, altfel are frecare cu aerul
	float Speed = CurrentVelocity.Size();
	FVector DragForce = -CurrentVelocity * (Speed * AirDrag * ActiveBraking);
	FVector TotalAcceleration = PropulsionForce + DragForce;
	//limitare acceleratie la 2g
	TotalAcceleration = TotalAcceleration.GetClampedToMaxSize(2000.0f);
	CurrentVelocity += TotalAcceleration * DeltaTime;
	if (InputDirection.IsZero())
	{
		float FrecareFortata = 2000.0f;
		if (Speed < 1000.0f)
		{
			float NewSpeed = FMath::Max(0.0f, Speed - (FrecareFortata * DeltaTime));
			if (NewSpeed <= 0.0f)
			{
				CurrentVelocity = FVector::ZeroVector;
			}
			else
			{
				CurrentVelocity = CurrentVelocity.GetSafeNormal() * NewSpeed;
			}
		}
	}
	//am limitat viteza maxima la 100 de metri pe secunda (360 km/h), drona prinde doar 279 m/h
	if (CurrentVelocity.Size() > MaxSpeed) CurrentVelocity = CurrentVelocity.GetSafeNormal() * MaxSpeed;
	if (GetAttachParentActor())
	{
		AddActorLocalOffset(CurrentVelocity * DeltaTime, true);
	}
	else
	{
		AddActorWorldOffset(CurrentVelocity * DeltaTime, true);
	}
	//aici setez rotatiile elicelor, ele se invarti in sens invers (cele din spate se invart in directii opuse una fata de alta, la fel si cele din fata)
	float RPM = 800.0f + (TotalAcceleration.Size() > 100.0f ? 2500.0f : 0) + Speed * 0.5f;
	FRotator SpinRot = FRotator(0.0f, RPM * DeltaTime, 0.0f);
	if (PropellerFL) PropellerFL->AddLocalRotation(SpinRot);
	if (PropellerFR) PropellerFR->AddLocalRotation(SpinRot * -1.0f);
	if (PropellerBL) PropellerBL->AddLocalRotation(SpinRot * -1.0f);
	if (PropellerBR) PropellerBR->AddLocalRotation(SpinRot);
	FRotator TargetMeshRotation = FRotator::ZeroRotator;
	if (TrackingTarget == "Libera (Niciunul)")
	{
		TargetMeshRotation = FRotator(InputDirection.X * -25.0f, 0.0f, InputDirection.Y * 25.0f);
	}
	DroneMesh->SetRelativeRotation(FMath::RInterpTo(DroneMesh->GetRelativeRotation(), TargetMeshRotation, DeltaTime, 5.0f));
	InputDirection = FVector::ZeroVector;
	if (TimeFlowSpeed != 0.0f)
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
		bool bFoundSun = false;
		for (AActor* Actor : AllActors)
		{
			if (Actor->GetName().Contains("CesiumSunSky"))
			{
				bFoundSun = true;
				FFloatProperty* FloatProp = FindFProperty<FFloatProperty>(Actor->GetClass(), "SolarTime");
				FDoubleProperty* DoubleProp = FindFProperty<FDoubleProperty>(Actor->GetClass(), "SolarTime");
				float CurrentTime = 0.0f;
				bool bPropertyFound = false;
				if (FloatProp)
				{
					CurrentTime = FloatProp->GetPropertyValue_InContainer(Actor);
					bPropertyFound = true;
				}
				else if (DoubleProp)
				{
					CurrentTime = (float)DoubleProp->GetPropertyValue_InContainer(Actor);
					bPropertyFound = true;
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(101, 0.1f, FColor::Red, TEXT("EROARE: Nu gasesc variabila 'SolarTime'!"));
				}
				//logica pentru trecerea timpului
				if (bPropertyFound)
				{
					float TimeStep = (TimeFlowSpeed * DeltaTime) / 3600.0f;
					float NewTime = CurrentTime + TimeStep;
					//aici setez data si ora corect in simulare
					if (NewTime >= 24.0f) NewTime -= 24.0f;
					if (NewTime < 0.0f) NewTime += 24.0f;
					if (FloatProp) FloatProp->SetPropertyValue_InContainer(Actor, NewTime);
					if (DoubleProp) DoubleProp->SetPropertyValue_InContainer(Actor, (double)NewTime);
					GEngine->AddOnScreenDebugMessage(102, 0.1f, FColor::Cyan, FString::Printf(TEXT("Ora: %.2f"), NewTime));
					UFunction* UpdateFunc = Actor->FindFunction(FName("UpdateSun"));
					if (UpdateFunc)
					{
						Actor->ProcessEvent(UpdateFunc, nullptr);
					}
					else
					{
						GEngine->AddOnScreenDebugMessage(103, 0.1f, FColor::Red, TEXT("EROARE: Nu gasesc functia 'UpdateSun'!"));
					}
				}
				break;
			}
		}
		if (!bFoundSun)
		{
			GEngine->AddOnScreenDebugMessage(104, 0.1f, FColor::Red, TEXT("EROARE: Nu gasesc actorul 'CesiumSunSky'!"));
		}
	}
	//aici vad daca tinta mea este libera sau nu si urmaresc planeta/luna/soarele
	UpdateAutoTracking(DeltaTime);
	//functia aceasta deseneaza linii pe cer (linii azimutale si ecuatoriale)
	//este o functionalitate inspirata de stellarium
	DrawAstronomicalGrids();
	if (SunActorRef && FogActorRef && FogActorRef->GetComponent())
	{
		//aici setez ceata, nu vreau ceata noaptea, ca se vede extrem de dubios
		UDirectionalLightComponent* SunLightComp = Cast<UDirectionalLightComponent>(SunActorRef->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
		if (SunLightComp)
		{
			if (bMapModeActive || SunLightComp->GetComponentRotation().Pitch > -5.0f)
			{
				FogActorRef->GetComponent()->SetFogDensity(0.0f);
			}
			else
			{
				FogActorRef->GetComponent()->SetFogDensity(CurrentFogIntensity);
			}
		}
	}
	//aici pot sa trag de harta sa ma plimb pe glob (Logica curatata)
	if (bMapModeActive)
	{
		APlayerController* PC9 = Cast<APlayerController>(GetController());
		if (PC9)
		{
			bool bIsMouseDown = PC9->IsInputKeyDown(EKeys::LeftMouseButton);
			float CurrentMouseX, CurrentMouseY;
			if (PC9->GetMousePosition(CurrentMouseX, CurrentMouseY))
			{
				if (bIsMouseDown)
				{
					if (!bIsDraggingMap)
					{
						StartClickPosition = FVector2D(CurrentMouseX, CurrentMouseY);
						bIsDraggingMap = true;
					}
					// AM STERS logica cu DeltaX si DeltaY de aici!
				}
				else
				{
					if (bIsDraggingMap)
					{
						bIsDraggingMap = false;
						float TotalMovement = FVector2D::Distance(StartClickPosition, FVector2D(CurrentMouseX, CurrentMouseY));
						// Daca nu am miscat mouse-ul mai mult de 5 pixeli, e un click simplu, facem teleport!
						if (TotalMovement <= 5.0f)
						{
							TeleportClick();
						}
					}
				}
			}
		}
		CurrentVelocity = FVector::ZeroVector;
	}
	APlayerController* PlayerCtrl = Cast<APlayerController>(GetController());
	ACesiumGeoreference* GeoRefMinimap = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	//aici era cod pentru a da zoom in si out apasand pe tastele i si o
	if (PlayerCtrl && MinimapSpringArm && MinimapCapture && GeoRefMinimap)
	{
		MinimapCapture->bCaptureEveryFrame = !bMapModeActive;
		if (!bMapModeActive)
		{
			FVector DroneLoc = GetActorLocation();
			FVector ECEF = GeoRefMinimap->TransformUnrealPositionToEarthCenteredEarthFixed(DroneLoc);
			FVector UpECEF = ECEF.GetSafeNormal();
			FVector UpUnreal = GeoRefMinimap->TransformEarthCenteredEarthFixedDirectionToUnreal(UpECEF).GetSafeNormal();
			FVector DownUnreal = -UpUnreal;
			FVector NorthECEF = FVector(0.0, 0.0, 1.0) - (UpECEF * UpECEF.Z);
			NorthECEF.Normalize();
			FVector NorthUnreal = GeoRefMinimap->TransformEarthCenteredEarthFixedDirectionToUnreal(NorthECEF).GetSafeNormal();
			FRotator MinimapRot = FRotationMatrix::MakeFromXZ(DownUnreal, NorthUnreal).Rotator();
			MinimapSpringArm->SetWorldRotation(MinimapRot);
			float MinimapZoomSpeed = MinimapSpringArm->TargetArmLength * 2.0f * DeltaTime;
			if (PlayerCtrl->IsInputKeyDown(EKeys::I))
			{
				MinimapSpringArm->TargetArmLength = FMath::Clamp(MinimapSpringArm->TargetArmLength - MinimapZoomSpeed, 50000.0f, 300000.0f);
			}
			else if (PlayerCtrl->IsInputKeyDown(EKeys::O))
			{
				MinimapSpringArm->TargetArmLength = FMath::Clamp(MinimapSpringArm->TargetArmLength + MinimapZoomSpeed, 50000.0f, 300000.0f);
			}
		}
	}
}
/*void ADronaMea::StartMapDrag()
{
	if (!bMapModeActive) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		float MouseX, MouseY;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			bool bClickedOnUI = false;
			if (MainMenuWidget)
			{
				//aici verific daca apare meniul principal desfasurat
				if (MainMenuWidget->MenuDropdownContainer && MainMenuWidget->MenuDropdownContainer->GetVisibility() == ESlateVisibility::Visible)
				{
					//aici ma uit sa vad daca am apasat pe butoanele din ui sau pe harta
					if (MouseX < 320.0f && MouseY < 750.0f) bClickedOnUI = true;
				}
				else if (MouseX < 200.0f && MouseY < 80.0f) bClickedOnUI = true;
			}
			if (EclipseInfoWidget && EclipseInfoWidget->IsHovered()) bClickedOnUI = true;
			if (bClickedOnUI) return;
			LastMousePosition = FVector2D(MouseX, MouseY);
			StartClickPosition = LastMousePosition;
			bIsDraggingMap = true;
			bHasMovedDuringDrag = false;
		}
	}
}
void ADronaMea::StopMapDrag()
{
	if (bMapModeActive && bIsDraggingMap)
	{
		bIsDraggingMap = false;
		if (!bHasMovedDuringDrag)
		{
			//teleport click este functia mea utila pentru a ma teleporta pe harta
			//dar functioneaza si atunci cand sunt pe sol in mod third person si dau click pe o planeta/soare/luna
			TeleportClick();
		}
	}
}*/
void ADronaMea::StartMapDrag()
{
	if (!bMapModeActive) return;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		float MouseX, MouseY;
		if (PC->GetMousePosition(MouseX, MouseY))
		{
			bool bClickedOnUI = false;
			if (MainMenuWidget)
			{
				if (MainMenuWidget->MenuDropdownContainer && MainMenuWidget->MenuDropdownContainer->GetVisibility() == ESlateVisibility::Visible)
				{
					if (MouseX < 320.0f && MouseY < 750.0f) bClickedOnUI = true;
				}
				else if (MouseX < 200.0f && MouseY < 80.0f) bClickedOnUI = true;
			}
			if (EclipseInfoWidget && EclipseInfoWidget->IsHovered()) bClickedOnUI = true;
			if (bClickedOnUI) return;

			SavedMaxFPS = GEngine->GetMaxFPS();

			// LIMITEAZĂ FPS-ul la 30
			//GEngine->SetMaxFPS(30.0f);

			LastMousePosition = FVector2D(MouseX, MouseY);
			StartClickPosition = LastMousePosition;
			bIsDraggingMap = true;
			bHasMovedDuringDrag = false;

			//dezactivez lag-ul de un frame doar in timpul dragului efectiv, nu global
			if (GEngine)
			{
				//GEngine->Exec(GetWorld(), TEXT("r.OneFrameThreadLag 0"));
				bDragThreadLagDisabled = true;
			}
		}
	}
}

void ADronaMea::StopMapDrag()
{
	if (bMapModeActive && bIsDraggingMap)
	{
		bIsDraggingMap = false;
		if (!bHasMovedDuringDrag)
		{
			TeleportClick();
		}
	}

	//repornesc lag-ul normal cand nu mai tragi, ca sa nu pierzi performanta restul timpului
	if (bDragThreadLagDisabled && GEngine)
	{
		//GEngine->Exec(GetWorld(), TEXT("r.OneFrameThreadLag 1"));
		bDragThreadLagDisabled = false;
		//GEngine->SetMaxFPS(SavedMaxFPS);
	}
}

//controale basic drona
void ADronaMea::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PlayerInputComponent->BindAxis("MoveForward", this, &ADronaMea::MoveForward); //tasta w
	PlayerInputComponent->BindAxis("MoveRight", this, &ADronaMea::MoveRight); //tasta d
	PlayerInputComponent->BindAxis("MoveUp", this, &ADronaMea::MoveUp); //tasta q
	PlayerInputComponent->BindAxis("Turn", this, &ADronaMea::Turn); //mouse
	PlayerInputComponent->BindAxis("LookUp", this, &ADronaMea::LookUp); //mouse
	PlayerInputComponent->BindAction("ToggleMap", IE_Pressed, this, &ADronaMea::ToggleMap); //tasta m
	PlayerInputComponent->BindAction("OpenHelp", IE_Pressed, this, &ADronaMea::ToggleHelpMenu); //f10
	PlayerInputComponent->BindAction("MapClick", IE_Pressed, this, &ADronaMea::TeleportClick); //un click clasic
	PlayerInputComponent->BindAxis("MapZoom", this, &ADronaMea::MapZoom); //scroll
}
void ADronaMea::HandleEclipseInput()
{
	AMoon* Moon = Cast<AMoon>(UGameplayStatics::GetActorOfClass(GetWorld(), AMoon::StaticClass()));
	if (Moon)
	{
		Moon->ToggleEclipseMode();
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange,
			Moon->bEclipseModeActive ? TEXT("Eclipsa Activata") : TEXT("Orbita Normala"));
	}
}
//cu tastele w,a,s,d,q si e pot controla si rotatia pamantului, de aceea aici peste tot apelez movemap
void ADronaMea::MoveForward(float Val)
{
	if (!bMapModeActive && TrackingTarget == "Libera (Niciunul)")
	{
		InputDirection.X = Val;
		FRotator CurrentRot = GetActorRotation();
		if (CurrentRot.Pitch != 0.0f || CurrentRot.Roll != 0.0f)
		{
			SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}
	else if (bMapModeActive && Val != 0.0f)
	{
		//modific doar latitudinea
		MoveMap(Val, 0.0f);
	}
}
/*void ADronaMea::MoveMap(float ForwardVal, float RightVal)
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef || (ForwardVal == 0.0f && RightVal == 0.0f)) return;
	FVector CurrentLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
	float RawZoomFactor = MapSpringArm->TargetArmLength / 100000000.0f;
	float ZoomFactor = FMath::Clamp(RawZoomFactor, 0.0000005f, 3.0f);
	float BaseSpeed = 15.0f * GetWorld()->GetDeltaSeconds();
	FRotator CameraRotation = MapSpringArm->GetComponentRotation();
	FRotator YawRotation(0.0f, CameraRotation.Yaw + 90.0f, 0.0f);
	FVector InputVector(ForwardVal, RightVal, 0.0f);
	FVector AdjustedMovement = YawRotation.RotateVector(InputVector);
	double NewLat = CurrentLLH.Y + (AdjustedMovement.X * BaseSpeed * ZoomFactor);
	double NewLon = CurrentLLH.X + (AdjustedMovement.Y * BaseSpeed * ZoomFactor);
	//extrag noua latitudine si longitudine pe baza celor vechi si in functie de zoomul dat pe ecran
	//mai jos puteam sa folosesc clamp, dar am fost fraier si m-am gandit tarziu la asta
	if (NewLat > 90.0)
	{
		NewLat = 180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	else if (NewLat < -90.0)
	{
		NewLat = -180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	while (NewLon > 180.0) NewLon -= 360.0;
	while (NewLon < -180.0) NewLon += 360.0;
	GeoRef->SetOriginLongitudeLatitudeHeight(FVector(NewLon, NewLat, CurrentLLH.Z));
	AOrbitalManager* Manager12345 = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager12345)
	{
		Manager12345->RebuildEclipsePathFromSaved();
	}
	//MoveUp(0.0f);
}*/

/*void ADronaMea::MoveMap(float ForwardVal, float RightVal)
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef || (ForwardVal == 0.0f && RightVal == 0.0f)) return;

	float RawZoomFactor = MapSpringArm->TargetArmLength / 100000000.0f;
	float ZoomFactor = FMath::Clamp(RawZoomFactor, 0.0000005f, 3.0f);
	float BaseSpeed = 15.0f * GetWorld()->GetDeltaSeconds();
	FRotator CameraRotation = MapSpringArm->GetComponentRotation();
	FRotator YawRotation(0.0f, CameraRotation.Yaw + 90.0f, 0.0f);
	FVector InputVector(ForwardVal, RightVal, 0.0f);
	FVector AdjustedMovement = YawRotation.RotateVector(InputVector);

	AccumulatedMapMovement.X += AdjustedMovement.X * BaseSpeed * ZoomFactor;
	AccumulatedMapMovement.Y += AdjustedMovement.Y * BaseSpeed * ZoomFactor;

	TimeSinceLastOriginRebase += GetWorld()->GetDeltaSeconds();
	if (TimeSinceLastOriginRebase < OriginRebaseInterval)
	{
		return;
	}
	TimeSinceLastOriginRebase = 0.0f;

	FVector CurrentLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
	double NewLat = CurrentLLH.Y + AccumulatedMapMovement.X;
	double NewLon = CurrentLLH.X + AccumulatedMapMovement.Y;
	AccumulatedMapMovement = FVector2D::ZeroVector;

	if (NewLat > 90.0)
	{
		NewLat = 180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	else if (NewLat < -90.0)
	{
		NewLat = -180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	while (NewLon > 180.0) NewLon -= 360.0;
	while (NewLon < -180.0) NewLon += 360.0;

	GeoRef->SetOriginLongitudeLatitudeHeight(FVector(NewLon, NewLat, CurrentLLH.Z));
	FlushRenderingCommands(); //sincronizare doar in acest frame, cu rebase efectiv

	AOrbitalManager* Manager12345 = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager12345)
	{
		Manager12345->RebuildEclipsePathFromSaved();
	}
}*/
void ADronaMea::MoveMap(float ForwardVal, float RightVal)
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef || (ForwardVal == 0.0f && RightVal == 0.0f)) return;

	//clamp pe delta time, ca sa evit un salt mare de miscare dupa un hitch/frame lung
	float SafeDeltaTime = FMath::Min(GetWorld()->GetDeltaSeconds(), 0.05f);

	float RawZoomFactor = MapSpringArm->TargetArmLength / 100000000.0f;
	float ZoomFactor = FMath::Clamp(RawZoomFactor, 0.0000005f, 3.0f);
	float BaseSpeed = 15.0f * SafeDeltaTime; //folosesc SafeDeltaTime in loc de GetDeltaSeconds() direct
	FRotator CameraRotation = MapSpringArm->GetComponentRotation();
	FRotator YawRotation(0.0f, CameraRotation.Yaw + 90.0f, 0.0f);
	FVector InputVector(ForwardVal, RightVal, 0.0f);
	FVector AdjustedMovement = YawRotation.RotateVector(InputVector);

	AccumulatedMapMovement.X += AdjustedMovement.X * BaseSpeed * ZoomFactor;
	AccumulatedMapMovement.Y += AdjustedMovement.Y * BaseSpeed * ZoomFactor;

	TimeSinceLastOriginRebase += SafeDeltaTime;
	if (TimeSinceLastOriginRebase < OriginRebaseInterval)
	{
		return;
	}
	TimeSinceLastOriginRebase = 0.0f;

	FVector CurrentLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
	double NewLat = CurrentLLH.Y + AccumulatedMapMovement.X;
	double NewLon = CurrentLLH.X + AccumulatedMapMovement.Y;
	AccumulatedMapMovement = FVector2D::ZeroVector;

	if (NewLat > 90.0)
	{
		NewLat = 180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	else if (NewLat < -90.0)
	{
		NewLat = -180.0 - NewLat;
		NewLon += 180.0;
		MapSpringArm->AddWorldRotation(FRotator(0.0f, 180.0f, 0.0f));
	}
	while (NewLon > 180.0) NewLon -= 360.0;
	while (NewLon < -180.0) NewLon += 360.0;

	GeoRef->SetOriginLongitudeLatitudeHeight(FVector(NewLon, NewLat, CurrentLLH.Z));

	AOrbitalManager* Manager12345 = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager12345)
	{
		Manager12345->RebuildEclipsePathFromSaved();
	}
}
//si aici aplic movemap pe longitudine daca sunt pe modul harta, daca sunt pe modul normal, doar misc drona asa cum am nevoie
void ADronaMea::MoveRight(float Val)
{
	if (!bMapModeActive && TrackingTarget == "Libera (Niciunul)")
	{
		InputDirection.Y = Val;
		FRotator CurrentRot = GetActorRotation();
		if (CurrentRot.Pitch != 0.0f || CurrentRot.Roll != 0.0f)
		{
			SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}
	else if (bMapModeActive && Val != 0.0f)
	{
		MoveMap(0.0f, Val);
	}
}
//aici nu mai apare movemap, practic daca sunt in modul harta, ma rotesc in jurul punctului in care sunt teleportat (tastele q si e)
//daca sunt in mod clasic, drona urca/coboara
void ADronaMea::MoveUp(float Val)
{
	if (!bMapModeActive && TrackingTarget == "Libera (Niciunul)")
	{
		InputDirection.Z = Val;
		FRotator CurrentRot = GetActorRotation();
		if (CurrentRot.Pitch != 0.0f || CurrentRot.Roll != 0.0f)
		{
			SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}
	//if (bmapmodeactive && Val != 0.0f)
	else if (bMapModeActive)
	{
		float VitezaRotatie = 90.0f * GetWorld()->GetDeltaSeconds();
		MapSpringArm->AddLocalRotation(FRotator(0.0f, 0.0f, Val * VitezaRotatie));
	}
}
//turn si lookup functioneaza pe acelasi principiu, doar ca aici tin cont de sensibilitatea mouse-ului aleasa din setari.
void ADronaMea::Turn(float Val)
{
	if (Val == 0.0f) return;
	if (bMapModeActive)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		// Daca tinem click stanga apasat, tragem stanga-dreapta de harta
		if (PC && PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			float DragSensitivity = 1.0f; // Poti ajusta valoarea asta cum iti place
			MoveMap(0.0f, -Val * DragSensitivity);
		}
	}
	else if (TrackingTarget == "Libera (Niciunul)")
	{
		float FinalTurnSpeed = Val * 2.0f * SensibilitateMouse;
		AddActorWorldRotation(FRotator(0.0f, FinalTurnSpeed, 0.0f));
		FRotator CurrentRot = GetActorRotation();
		if (CurrentRot.Pitch != 0.0f || CurrentRot.Roll != 0.0f)
		{
			SetActorRotation(FRotator(0.0f, CurrentRot.Yaw, 0.0f));
		}
	}
}
void ADronaMea::LookUp(float Val)
{
	if (Val == 0.0f) return;
	float VitezaFinala = Val * 2.0f * SensibilitateMouse;
	if (bInvertMouseY)
	{
		VitezaFinala *= -1.0f;
	}
	if (bMapModeActive)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		// Daca tinem click stanga apasat, tragem sus-jos de harta
		if (PC && PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			float DragSensitivity = 1.0f; // Poti ajusta valoarea asta cum iti place
			// Folosim Val direct (nu VitezaFinala), pentru ca nu vrem sa aplicam "Invert Y" de la TPV pe drag-ul pe harta
			MoveMap(Val * DragSensitivity, 0.0f);
		}
	}
	else if (TrackingTarget == "Libera (Niciunul)")
	{
		FRotator CurrentRot = SpringArm->GetRelativeRotation();
		CurrentRot.Pitch = FMath::Clamp(CurrentRot.Pitch + VitezaFinala, -89.9f, 89.9f);
		CurrentRot.Yaw = 0.0f;
		CurrentRot.Roll = 0.0f;
		SpringArm->SetRelativeRotation(CurrentRot);
	}
}
void ADronaMea::AdjustSpeed(float Val) {}
//togglemap presupune sa trec din modul normal in modul in care privesc harta de sus
void ADronaMea::ToggleMap()
{
	bMapModeActive = !bMapModeActive;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	UGameUserSettings* Settings = GEngine->GetGameUserSettings();
	float SavedFPSEnabled = Settings ? Settings->GetFrameRateLimit() : 0.0f; // 0.0 înseamnă Unlimited

	if (bMapModeActive)
	{
		// Intrăm în modul hartă: forțăm limita la 30 FPS pentru a reduce flicker-ul
		GEngine->SetMaxFPS(30.0f);
	}
	else
	{
		// Ieșim din modul hartă: restaurăm limita preferată de utilizator
		// Dacă SavedFPSEnabled este 0.0, va seta automat pe Unlimited
		GEngine->SetMaxFPS(SavedFPSEnabled);
	}

	AHUD* BaseHUD = PC->GetHUD();
	if (bMapModeActive)
	{
		MapSpringArm->SetUsingAbsoluteRotation(true);
		MapSpringArm->SetWorldRotation(FRotator(-90.0f, -90.0f, 0.0f));
		//aici permit tot ce tine de clickuri si mouse
		Camera->SetActive(false);
		MapCamera->SetActive(true);
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;
		if (BaseHUD) BaseHUD->SetActorHiddenInGame(true);
	}
	else
	{
		//aici trec din modul harta in modul clasic, imi aloc 3 secunde pentru a gasi solul
		MapCamera->SetActive(false);
		Camera->SetActive(true);
		SetActorLocation(FVector(0.0f, 0.0f, 2000000.0f));
		CurrentVelocity = FVector::ZeroVector;
		bSearchingForGround = true;
		SearchTimeout = 3.0f;
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Revenire din harta... Caut solul..."));
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = false;
		PC->bEnableMouseOverEvents = false;
		if (BaseHUD) BaseHUD->SetActorHiddenInGame(false);
	}
	//fac asta aici sa nu am probleme de mouse
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(InputMode);
}
void ADronaMea::MapZoom(float Val)
{
	//daca nu dau zoom, nu are rost functia
	if (Val == 0.0f) return;
	bool bIsPopUpOpen = false;
	//verific daca am deschis vreunul dintre meniurile prin care pot sa dau scroll
	/* Deoarece si meniurile si zoomul pe harta/pe sol se bazeaza pe scroll, imi aparea problema in care dadeam scroll prin setari, ajungeam la final si pe urma se activa map
	* zoom, care imi dadea zoom pe harta, cu meniul deschis. Este un UX naspa, de accea in iful mare de mai jos repar aceasta problema
	*/
	if (!bIsPopUpOpen)
	{
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetWorld() == GetWorld() && It->IsInViewport() && It->IsVisible())
			{
				if (It->IsA(UEclipseBrowserWidget::StaticClass()) ||
					It->IsA(UPlacementMenuWidget::StaticClass()) ||
					It->IsA(UTimeControlWidget::StaticClass()) ||
					It->IsA(UOpticsMenuWidget::StaticClass()) ||
					It->IsA(UOverlayMenuWidget::StaticClass()) ||
					It->IsA(UVideoExportMenuWidget::StaticClass()) ||
					It->IsA(UCameraMenuWidget::StaticClass()) ||
					It->IsA(UVisualEffectsMenuWidget::StaticClass()) ||
					It->IsA(UExitMenuWidget::StaticClass()) ||
					It->IsA(USettingsMenuWidget::StaticClass()) ||
					It->IsA(UBookmarksMenuWidget::StaticClass()) ||
					It->IsA(UHelpMenuWidget::StaticClass()) ||
					It->IsA(UWeatherMenuWidget::StaticClass()) ||
					It->IsA(UEclipseMapExportWidget::StaticClass()) ||
					It->IsA(UMapPickerWidget::StaticClass()) ||
					It->IsA(UCreditsMenuWidget::StaticClass()))
				{
					bIsPopUpOpen = true;
					break;
				}
			}
		}
	}
	if (bIsPopUpOpen)
	{
		return;
	}
	if (bMapModeActive)
	{
		//aici setez latitudinea, longitudinea si altitudinea, intre 10 metri si 2.5 milioane km
		APlayerController* PC = Cast<APlayerController>(GetController());
		ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
		float CurrentLen = MapSpringArm->TargetArmLength;
		float ZoomStrenght = 0.25f;
		float Multiplier = 1.0f - (Val * ZoomStrenght);
		float NewLen = FMath::Clamp(CurrentLen * Multiplier, 1000.0f, 2500000000000.0f);
		if (PC && GeoRef && CurrentLen != NewLen)
		{
			FVector WorldLocation, WorldDirection;
			if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
			{
				FHitResult Hit;
				FVector Start = WorldLocation;
				FVector End = Start + (WorldDirection * 1000000000000.0f);
				FCollisionQueryParams TraceParams;
				TraceParams.AddIgnoredActor(this);
				TraceParams.bTraceComplex = true;
				if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, TraceParams))
				{
					FVector TargetLLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.Location);
					FVector CurrentLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
					double PanFraction = (CurrentLen - NewLen) / CurrentLen;
					double NewLat = CurrentLLH.Y + (TargetLLH.Y - CurrentLLH.Y) * PanFraction;
					double LonDiff = TargetLLH.X - CurrentLLH.X;
					if (LonDiff > 180.0) LonDiff -= 360.0;
					if (LonDiff < -180.0) LonDiff += 360.0;
					double NewLon = CurrentLLH.X + (LonDiff * PanFraction);
					GeoRef->SetOriginLongitudeLatitudeHeight(FVector(NewLon, NewLat, CurrentLLH.Z));
				}
			}
		}
		MapSpringArm->TargetArmLength = NewLen;
	}
	else
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC && PC->PlayerCameraManager)
		{
			float CurrentFOV = PC->PlayerCameraManager->GetFOVAngle();
			float ZoomSpeed = CurrentFOV * 0.15f;
			float NewFOV = CurrentFOV - (Val * ZoomSpeed);
			//fovul meu este intre 0.00001 si 135.0f, de fapt unreal suporta maxim 0.0001, suficient cat sa vad Neptun bine
			NewFOV = FMath::Clamp(NewFOV, 0.00001f, 135.0f);
			if (CurrentFOV != NewFOV && TrackingTarget == "Libera (Niciunul)" && Val > 0.0f)
			{
				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
				{
					//aici setez rotatia camerei in functie de inclinarea pe planeta
					FVector LocalDir = Camera->GetComponentTransform().InverseTransformVectorNoScale(WorldDirection);
					float LocalYawDelta = FMath::RadiansToDegrees(FMath::Atan2(LocalDir.Y, LocalDir.X));
					float LocalPitchDelta = FMath::RadiansToDegrees(FMath::Asin(LocalDir.Z));
					float TanOld = FMath::Tan(FMath::DegreesToRadians(CurrentFOV * 0.5f));
					float TanNew = FMath::Tan(FMath::DegreesToRadians(NewFOV * 0.5f));
					float PanFraction = 1.0f - (TanNew / TanOld);
					float FinalYawDelta = LocalYawDelta * PanFraction;
					float FinalPitchDelta = LocalPitchDelta * PanFraction;
					AddActorLocalRotation(FRotator(0.0f, FinalYawDelta, 0.0f));
					FRotator SpringArmRot = SpringArm->GetRelativeRotation();
					SpringArmRot.Pitch = FMath::Clamp(SpringArmRot.Pitch + FinalPitchDelta, -89.9f, 89.9f);
					SpringArm->SetRelativeRotation(SpringArmRot);
				}
			}
			PC->PlayerCameraManager->SetFOV(NewFOV);
		}
	}
}
/*
* Aceasta functie este foarte importanta, deoarece fac cu ea teleport atat pe pamant, cat si urmaresc corpuri ceresti
*/
void ADronaMea::TeleportClick()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	bool bClickedOnUI = false;
	float MouseX, MouseY;

	//scalare pe ecran
	float VPW = 1920.0f, VPH = 1080.0f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D VS;
		GEngine->GameViewport->GetViewportSize(VS);
		VPW = VS.X; VPH = VS.Y;
	}

	ADroneHUD* MyHUD1234 = Cast<ADroneHUD>(PC->GetHUD());
	float UI = MyHUD1234 ? MyHUD1234->CachedUI : FMath::Min(VPW / 1920.0f, VPH / 1080.0f);
	auto S = [&](float Val) { return Val * UI; };

	if (PC->GetMousePosition(MouseX, MouseY))
	{
		int32 ViewportX, ViewportY;
		PC->GetViewportSize(ViewportX, ViewportY);
		if (MainMenuWidget)
		{
			//daca meniul principal este deschis
			if (MainMenuWidget->MenuDropdownContainer && MainMenuWidget->MenuDropdownContainer->GetVisibility() == ESlateVisibility::Visible)
			{
				if (MouseX < S(320.0f) && MouseY < S(750.0f)) bClickedOnUI = true;
			}
			else
			{
				if (MouseX < S(200.0f) && MouseY < S(80.0f)) bClickedOnUI = true;
			}
		}
	}

	bool bHideHUD = false;
	//aici verific daca hudul este deschis sau nu, in meniul overlays si telemetrie pot pune o bifa daca vreau sau nu sa vad hudul
	if (GConfig)
	{
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bHideHUD, GGameUserSettingsIni);
	}
	if (PC->GetHUD())
	{
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD && !MyHUD->bDrawTelemetry) bHideHUD = true;
	}
	if (!bHideHUD)
	{
		//daca e hudul ascuns, aici verific sa nu dau click pe butonul de meniu
		float BtnSize = S(100.0f);
		float BtnY = S(15.0f);

		//calcul latime dinamica
		float VPW2 = 1920.0f;
		if (GEngine && GEngine->GameViewport)
		{
			FVector2D ViewportSize;
			GEngine->GameViewport->GetViewportSize(ViewportSize);
			VPW2 = ViewportSize.X;
		}
		float BtnX_Map = S(340.0f);
		if (MainMenuWidget && MainMenuWidget->MenuDropdownContainer && MainMenuWidget->MenuDropdownContainer->GetVisibility() == ESlateVisibility::Visible)
		{
			BtnX_Map = S(700.0f);
		}
		float BtnX_Xav = BtnX_Map + BtnSize + S(15.0f);
		float BtnX_Cloud = BtnX_Xav + BtnSize + S(15.0f);
		float BtnX_Pause = BtnX_Cloud + BtnSize + S(15.0f);
		//aici am butoanele celelalte 4, pe care verific daca dau click

		//debugging
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(50, 5.0f, FColor::Red,
				FString::Printf(TEXT("Mouse: %.0f,%.0f | Pause: %.0f-%.0f | Y: %.0f-%.0f"),
					MouseX, MouseY,
					BtnX_Pause, BtnX_Pause + BtnSize,
					BtnY, BtnY + BtnSize));
		}
		GEngine->AddOnScreenDebugMessage(51, 5.0f, FColor::Yellow,
			FString::Printf(TEXT("UI=%.4f VPW=%.0f VPH=%.0f"), UI, VPW, VPH));

		if (MouseX >= BtnX_Map && MouseX <= BtnX_Map + BtnSize && MouseY >= BtnY && MouseY <= BtnY + BtnSize)
		{
			if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) return;
			float CurrentTime = GetWorld()->GetRealTimeSeconds();
			//mecanism de debounce de 0.2 secunde, ca sa nu dau click si sa imi ia mai multe 
			if (CurrentTime - LastMapToggleTime > 0.2f)
			{
				ToggleMap();
				LastMapToggleTime = CurrentTime;
			}
			return;
		}
		//aceeasi poveste si la butonul care imi prezinta tabelul de tip Xavier Jubier
		if (MouseX >= BtnX_Xav && MouseX <= BtnX_Xav + BtnSize && MouseY >= BtnY && MouseY <= BtnY + BtnSize)
		{
			if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) return;
			float CurrentTime = GetWorld()->GetRealTimeSeconds();
			if (CurrentTime - LastXavToggleTime > 0.2f)
			{
				if (EclipseInfoWidget)
				{
					if (EclipseInfoWidget->GetVisibility() == ESlateVisibility::Visible)
						EclipseInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
					else
					{
						if (!EclipseInfoWidget->IsInViewport()) EclipseInfoWidget->AddToViewport();
						EclipseInfoWidget->SetVisibility(ESlateVisibility::Visible);
					}
				}
				LastXavToggleTime = CurrentTime;
			}
			return;
		}
		//butonul de nori cu acelasi debounce de 0.2 secunde
		if (MouseX >= BtnX_Cloud && MouseX <= BtnX_Cloud + BtnSize && MouseY >= BtnY && MouseY <= BtnY + BtnSize)
		{
			if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) return;
			float CurrentTime = GetWorld()->GetRealTimeSeconds();
			if (CurrentTime - LastCloudToggleTime > 0.2f)
			{
				TArray<AActor*> NoriScena;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), NoriScena);
				for (AActor* Actor : NoriScena)
				{
					if (Actor->GetName().Contains("Cloud") || Actor->GetName().Contains("VolumetricCloud"))
					{
						Actor->SetActorHiddenInGame(!Actor->IsHidden());
					}
				}
				LastCloudToggleTime = CurrentTime;
			}
			return;
		}
		//butonul de pauza cu debounce de 0.2 secunde, verific daca dau click sau nu pe fiecare din cele 4 butoane ca sa dau return
		if (MouseX >= BtnX_Pause && MouseX <= BtnX_Pause + BtnSize && MouseY >= BtnY && MouseY <= BtnY + BtnSize)
		{
			if (!PC->IsInputKeyDown(EKeys::LeftMouseButton)) return;
			float CurrentTime = GetWorld()->GetRealTimeSeconds();
			if (CurrentTime - LastPauseToggleTime > 0.2f)
			{
				AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
				if (Manager)
				{
					//setez in orbital manager timpul la 0.0, adica pauza, dar tot ma pot plimba cu drona, doar timpul din simulare nu se modifica
					//practic trimit consistent la backendul de python aceeasi ora, adica soarele,luna si planetele nu se mai misca relativ fata de pamant
					if (Manager->TimeSpeed != 0.0f)
					{
						SavedTimeSpeed = Manager->TimeSpeed;
						Manager->TimeSpeed = 0.0f;
					}
					else
					{
						Manager->TimeSpeed = (SavedTimeSpeed != 0.0f) ? SavedTimeSpeed : 1.0f;
					}
				}
				LastPauseToggleTime = CurrentTime;
			}
			return;
		}
	}
	if (EclipseInfoWidget && EclipseInfoWidget->IsHovered())
	{
		bClickedOnUI = true;
	}
	if (bClickedOnUI)
	{
		return;
	}
	this->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	//partea aceasta este foarte importanta dpdv a clickului pe cer pentru stele/planete
	//detectez cel mai apropiat obiect de click, tin cont inclusiv de fov
	//daca de exemplu trebuie sa dau click pe Neptun/Triton, ambele au calculat unghiul de deschidere pentru a putea da click direct pe ecran pe planeta daca o vad
	//de asemenea, la fov 90, toate chestiile au fov de 2 grade pentru a putea da click pe ele usor fara sa fiu nevoit sa nimeresc doar un pixel
	//exista 2 tieruri, primul este pentru sateliti naturali, pe care este mai greu de dat click, iar al doilea este pentru planete
	FVector WorldLocation, WorldDirection;
	if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager)
		{
			FString HitBodyName = "";
			int32 BestHitTier = 999;
			float BestHitAngle = 999.0f;
			double BestHitDistance = 999999999999.0; //distante imense
			double BestHitRadius = 999999999999.0;
			float CurrentFOV = 90.0f;
			if (PC->PlayerCameraManager) CurrentFOV = PC->PlayerCameraManager->GetFOVAngle();
			float MinimumScreenAngle = (CurrentFOV / 1080.0f) * 8.0f;
			auto CheckBodyClick = [&](USceneComponent* Comp, FString Name, double BodyRadiusKm, double BodyDistanceKm)
				{
					if (!Comp || !Comp->IsVisible()) return;
					if (FMath::IsNaN(BodyDistanceKm) || BodyDistanceKm < 10000.0)
					{
						return;
					}
					FVector Center = Comp->GetComponentLocation();
					FVector DirToBody = (Center - WorldLocation).GetSafeNormal();
					double Dot = FVector::DotProduct(DirToBody, WorldDirection);
					Dot = FMath::Clamp(Dot, -1.0, 1.0);
					float AngleDeg = (float)FMath::RadiansToDegrees(FMath::Acos(Dot));
					double AngularRadiusRads = FMath::Atan2(BodyRadiusKm, BodyDistanceKm);
					float PhysicalAngle = (float)FMath::RadiansToDegrees(AngularRadiusRads);
					float EffectiveHitbox = FMath::Max(PhysicalAngle, MinimumScreenAngle);
					if (AngleDeg > EffectiveHitbox) return;
					int32 HitTier = (AngleDeg <= PhysicalAngle) ? 1 : 2;
					bool bIsBetter = false;
					if (HitTier < BestHitTier)
					{
						bIsBetter = true;
					}
					else if (HitTier == BestHitTier)
					{
						if (HitTier == 1)
						{
							if (BodyDistanceKm < BestHitDistance) bIsBetter = true;
						}
						else
						{
							if (AngleDeg < BestHitAngle - 0.0001f)
							{
								bIsBetter = true;
							}
							else if (FMath::Abs(AngleDeg - BestHitAngle) <= 0.0001f)
							{
								if (BodyRadiusKm < BestHitRadius) bIsBetter = true;
							}
						}
					}
					if (bIsBetter)
					{
						BestHitTier = HitTier;
						BestHitAngle = AngleDeg;
						BestHitDistance = BodyDistanceKm;
						BestHitRadius = BodyRadiusKm;
						HitBodyName = Name;
					}
				};
			bool bAllowAstroClick = true;
			if (PC->GetMousePosition(MouseX, MouseY))
			{
				int32 ViewportX, ViewportY;
				PC->GetViewportSize(ViewportX, ViewportY);
				if (ViewportX > 0 && ViewportY > 0)
				{
					float PctX = MouseX / (float)ViewportX;
					float PctY = MouseY / (float)ViewportY;
					if (MainMenuWidget && MainMenuWidget->TelemetryMenuInstance && MainMenuWidget->TelemetryMenuInstance->GetVisibility() == ESlateVisibility::Visible)
					{
						if (PctX > 0.75f && PctY < 0.60f)
						{
							bAllowAstroClick = false;
						}
					}
				}
			}
			//aici pun razele pentru toate planetele si stelele
			if (bAllowAstroClick)
			{
				TArray<FString> PNamesEn = { "Mercury", "Venus", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune" };
				TArray<double> PRadiusKm = { 2439.7, 6051.8, 3389.5, 71492.0, 58232.0, 25362.0, 24622.0 };
				for (int32 i = 0; i < Manager->PlanetMeshes.Num() && i < PNamesEn.Num() && i < Manager->PlanetsList.Num(); i++)
				{
					CheckBodyClick(Manager->PlanetMeshes[i], PNamesEn[i], PRadiusKm[i], Manager->PlanetsList[i].DistanceKm);
				}
				TArray<FString> MNames = { "Io", "Europa", "Ganymede", "Callisto", "Titan", "Triton" };
				TArray<double> MRadiusKm = { 1821.6, 1560.8, 2634.1, 2410.3, 2574.7, 1353.4 };
				for (int32 i = 0; i < Manager->MajorMoonsMeshes.Num() && i < MNames.Num() && i < Manager->MajorMoonsList.Num(); i++)
				{
					CheckBodyClick(Manager->MajorMoonsMeshes[i], MNames[i], MRadiusKm[i], Manager->MajorMoonsList[i].DistanceKm);
				}
				if (Manager->ControlledMoon)
				{
					CheckBodyClick(Manager->ControlledMoon->GetRootComponent(), "Moon", 1737.4, Manager->CurrentMoonDistanceKm);
				}
				//aici determin daca soarele este cel mai bun candidat
				if (Manager->SunSkyActor)
				{
					UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Manager->SunSkyActor->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
					if (SunLight)
					{
						FVector FakeSunLocation = WorldLocation - (SunLight->GetForwardVector() * 5000000.0f);
						FVector DirToSun = (FakeSunLocation - WorldLocation).GetSafeNormal();
						double DotSun = FVector::DotProduct(DirToSun, WorldDirection);
						DotSun = FMath::Clamp(DotSun, -1.0, 1.0);
						float AngleSun = (float)FMath::RadiansToDegrees(FMath::Acos(DotSun));
						double SunRadKm = 696340.0;
						double SunDistKm = Manager->CurrentSunDistanceKm;
						double AngularRadiusRads = FMath::Atan2(SunRadKm, SunDistKm);
						float PhysicalAngle = (float)FMath::RadiansToDegrees(AngularRadiusRads);
						float EffectiveHitbox = FMath::Max(PhysicalAngle, MinimumScreenAngle);
						if (AngleSun <= EffectiveHitbox)
						{
							int32 HitTier = (AngleSun <= PhysicalAngle) ? 1 : 2;
							bool bIsBetter = false;
							if (HitTier < BestHitTier) bIsBetter = true;
							else if (HitTier == BestHitTier)
							{
								if (HitTier == 1) { if (SunDistKm < BestHitDistance) bIsBetter = true; }
								else
								{
									if (AngleSun < BestHitAngle - 0.0001f) bIsBetter = true;
									else if (FMath::Abs(AngleSun - BestHitAngle) <= 0.0001f) { if (SunRadKm < BestHitRadius) bIsBetter = true; }
								}
							}
							if (bIsBetter)
							{
								BestHitTier = HitTier;
								BestHitAngle = AngleSun;
								BestHitDistance = SunDistKm;
								BestHitRadius = SunRadKm;
								HitBodyName = "Sun";
							}
						}
					}
				}
			}
			//aici selectez target pe obiectul pe care am dat click
			//acest target este vazut si in meniul optica si telescop
			if (!HitBodyName.IsEmpty())
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Tinta selectata: %s"), *HitBodyName));
				TrackingTarget = HitBodyName;
				if (MainMenuWidget)
				{
					MainMenuWidget->OpenTelemetryMenu();
					TrackingTarget = HitBodyName;
				}
				return;
			}
			else
			{
				TrackingTarget = TEXT("Libera (Niciunul)");
				if (MainMenuWidget) MainMenuWidget->CloseTelemetryMenu();
			}
		}
		if (!bMapModeActive) return;
		if (PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			return;
		}
		float CurrentTime = GetWorld()->GetRealTimeSeconds();
		if (CurrentTime - LastTeleportTime < 0.5f)
		{
			return;
		}
		ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
		if (!GeoRef) return;
		FHitResult Hit;
		FVector Start = WorldLocation;
		FVector End = Start + (WorldDirection * 1000000000000.0f);
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);
		TraceParams.bTraceComplex = true;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, TraceParams))
		{
			LastTeleportTime = CurrentTime;
			FVector TargetLLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Hit.Location);
			double SafetyLimit = 6700000.0;
			if (TargetLLH.Z > -SafetyLimit && TargetLLH.Z < SafetyLimit)
			{
				TargetLLH.Z = 1000.0f;
			}
			GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
			SetActorLocation(FVector(0, 0, 5000.0f));
			CurrentVelocity = FVector::ZeroVector;
			bSearchingForGround = true;
			SearchTimeout = 5.0f;

			if (PC && PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
			}

			// Fortez si Soarele sa isi faca update instantaneu in noul punct, ca sa nu ramana în urma
			AOrbitalManager* Manager12345 = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
			if (Manager12345 && Manager12345->SunSkyActor)
			{
				UFunction* UpdateFunc = Manager12345->SunSkyActor->FindFunction(FName("UpdateSun"));
				if (UpdateFunc)
				{
					Manager12345->SunSkyActor->ProcessEvent(UpdateFunc, nullptr);
				}
			}
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Teleportare initiata..."));
		}
	}
}
//functia aceasta era ca sa pot sa ma uit pe luna, dar deoarece se modifica in permanenta coordonatele cesium cand sunt pe luna, avea buguri, asa ca am renuntat la aceasta functionalitate
void ADronaMea::TeleportToMoonSurface()
{
	AMoon* Moon = Cast<AMoon>(UGameplayStatics::GetActorOfClass(GetWorld(), AMoon::StaticClass()));
	if (Moon && Moon->GeoRef)
	{
		this->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		FVector EarthPos = Moon->GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
		FVector MoonPos = Moon->GetActorLocation();
		FVector DirToEarth = (EarthPos - MoonPos).GetSafeNormal();
		double MoonRadiusCM = 1737400.0 * 100.0;
		double SafetyHeight = 20000.0;
		FVector TeleportLocation = MoonPos + (DirToEarth * (MoonRadiusCM + SafetyHeight));
		CurrentVelocity = FVector::ZeroVector;
		SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::TeleportPhysics);
		FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
		this->AttachToActor(Moon, AttachRules);
		FRotator LookAtEarth = DirToEarth.Rotation();
		if (GetController())
		{
			GetController()->SetControlRotation(LookAtEarth);
		}
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("Atasat de Luna! Misca-te cu WASD."));
	}
}
//functii basic pentru a accelera/incetini timpul, nu le folosesc in cadrul proiectului, deoarece am ales sa pot sa inmultesc/impart cu 2.
void ADronaMea::SpeedUpTime()
{
	TimeFlowSpeed += 2000.0f;
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("Viteza Timp: %.0f"), TimeFlowSpeed));
}
void ADronaMea::SlowDownTime()
{
	TimeFlowSpeed -= 2000.0f;
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, FString::Printf(TEXT("Viteza Timp: %.0f"), TimeFlowSpeed));
}
void ADronaMea::ResetTimeSpeed()
{
	TimeFlowSpeed = 0.0f;
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Timp Oprit"));
}
void ADronaMea::UpdateEclipseWidgetText(FString NewText)
{
	if (EclipseInfoWidget)
	{
		EclipseInfoWidget->SetReportText(NewText);
	}
}
//aici fac functia de autotracking, in care efectiv tin drona indreptata spre acel corp ceresc (adica drona se roteste dupa corp)
//de asemenea, si camera se roteste odata cu drona
void ADronaMea::UpdateAutoTracking(float DeltaTime)
{
	if (bMapModeActive) return;
	FVector TargetDirection = FVector::ZeroVector;
	bool bHasValidTarget = false;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager) return;
	//plec de  la presupunerea initiala ca nu urmaresc nimic
	FString NumeLibera = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
	if (TrackingTarget == NumeLibera || TrackingTarget == TEXT("Libera (Niciunul)") || TrackingTarget == TEXT("Free (None)"))
	{
		if (SpringArm && SpringArm->IsUsingAbsoluteRotation())
		{
			FRotator CameraLook = SpringArm->GetComponentRotation();
			SetActorRotation(FRotator(0.0f, CameraLook.Yaw, 0.0f));
			SpringArm->SetUsingAbsoluteRotation(false);
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->SetRelativeRotation(FRotator(CameraLook.Pitch, 0.0f, 0.0f));
			if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				PC->SetControlRotation(CameraLook);
			}
			if (DroneMesh) DroneMesh->SetRelativeRotation(FRotator::ZeroRotator);
		}
		return;
	}
	//targetul poate fi in romana sau engleza, am fost fraier si nu am avut consistenta la setarea variabile de target.
	FString TargetEn = TrackingTarget.TrimStartAndEnd();
	if (TargetEn.Equals(TEXT("Mercur"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Mercury");
	else if (TargetEn.Equals(TEXT("Marte"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Mars");
	else if (TargetEn.Equals(TEXT("Pamant"), ESearchCase::IgnoreCase) || TargetEn.Equals(TEXT("Terra"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Earth");
	else if (TargetEn.Equals(TEXT("Neptun"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Neptune");
	else if (TargetEn.Equals(TEXT("Soare"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Sun");
	else if (TargetEn.Equals(TEXT("Luna"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Moon");
	else if (TargetEn.Equals(TEXT("Calisto"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Callisto");
	else if (TargetEn.Equals(TEXT("Ganimede"), ESearchCase::IgnoreCase)) TargetEn = TEXT("Ganymede");
	//urmaresc soarele
	if (TargetEn == TEXT("Sun"))
	{
		if (Manager->SunSkyActor)
		{
			UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Manager->SunSkyActor->GetComponentByClass(UDirectionalLightComponent::StaticClass()));
			TargetDirection = SunLight ? -(SunLight->GetForwardVector()) : -(Manager->SunSkyActor->GetActorForwardVector());
			bHasValidTarget = true;
		}
	}
	//urmaresc luna
	else if (TargetEn == TEXT("Moon"))
	{
		if (Manager->ControlledMoon)
		{
			TargetDirection = (Manager->ControlledMoon->GetActorLocation() - SpringArm->GetComponentLocation()).GetSafeNormal();
			bHasValidTarget = true;
		}
	}
	else
	{
		//iterez prin lista de planete, in ifuri apar doar 4, deoarece doar 4 planete au nume diferit in romana fata de engleza
		for (const AOrbitalManager::FPlanetData& Planet : Manager->PlanetsList)
		{
			FString PNameEn = Planet.Name.TrimStartAndEnd();
			if (PNameEn.Equals(TEXT("Mercur"), ESearchCase::IgnoreCase)) PNameEn = TEXT("Mercury");
			else if (PNameEn.Equals(TEXT("Marte"), ESearchCase::IgnoreCase)) PNameEn = TEXT("Mars");
			else if (PNameEn.Equals(TEXT("Pamant"), ESearchCase::IgnoreCase) || PNameEn.Equals(TEXT("Terra"), ESearchCase::IgnoreCase)) PNameEn = TEXT("Earth");
			else if (PNameEn.Equals(TEXT("Neptun"), ESearchCase::IgnoreCase)) PNameEn = TEXT("Neptune");
			if (PNameEn.Equals(TargetEn, ESearchCase::IgnoreCase) || Planet.Name.Equals(TargetEn, ESearchCase::IgnoreCase) || Planet.InternalName.Equals(TargetEn, ESearchCase::IgnoreCase))
			{
				TargetDirection = Planet.Direction;
				bHasValidTarget = true;
				break;
			}
		}
		//daca nu exista target valid intre planete, poate urmaresc sateliti naturali
		if (!bHasValidTarget)
		{
			for (const AOrbitalManager::FPlanetData& SMoon : Manager->MajorMoonsList)
			{
				FString MNameEn = SMoon.Name.TrimStartAndEnd();
				if (MNameEn.Equals(TEXT("Calisto"), ESearchCase::IgnoreCase)) MNameEn = TEXT("Callisto");
				else if (MNameEn.Equals(TEXT("Ganimede"), ESearchCase::IgnoreCase)) MNameEn = TEXT("Ganymede");
				if (MNameEn.Equals(TargetEn, ESearchCase::IgnoreCase) || SMoon.Name.Equals(TargetEn, ESearchCase::IgnoreCase) || SMoon.InternalName.Equals(TargetEn, ESearchCase::IgnoreCase))
				{
					TargetDirection = SMoon.Direction;
					bHasValidTarget = true;
					break;
				}
			}
		}
	}
	//daca tinta este valida, setez rotatia dronei si a camerei pe corpul ceresc selectat
	if (bHasValidTarget)
	{
		if (SpringArm)
		{
			SpringArm->SetUsingAbsoluteRotation(true);
			FRotator TargetRot = TargetDirection.Rotation();
			SetActorRotation(FRotator(0.0f, TargetRot.Yaw, 0.0f));
			TargetRot.Roll = 0.0f;
			SpringArm->SetWorldRotation(TargetRot);
			if (DroneMesh)
			{
				DroneMesh->SetRelativeRotation(FRotator::ZeroRotator);
			}
		}
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FRotator SafeControlRot = TargetDirection.Rotation();
			SafeControlRot.Roll = 0.0f;
			PC->SetControlRotation(SafeControlRot);
		}
	}
}
//functia drawastronomicalgrids a fost mutata de aici, din cauza faptului ca foloseam drawdebugline
//eu am facut ulterior executabilul si am folosit modul shipping, deci automat liniile nu se mai vedeau
void ADronaMea::DrawAstronomicalGrids()
{
}
//aici setez camera in third person
void ADronaMea::SetCameraTPV2()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		MyDrone->SetActorHiddenInGame(false);
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(false); }
		USpringArmComponent* SpringArm2 = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm2)
		{
			float PitchCurent = SpringArm2->GetComponentRotation().Pitch;
			SpringArm2->SetUsingAbsoluteRotation(false);
			SpringArm2->bUsePawnControlRotation = false;
			SpringArm2->TargetArmLength = 600.0f;
			SpringArm2->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}
//aici export harta cu banda de totalitate a eclipselor totale
void ADronaMea::Export2DMapToDisk()
{
	if (!MapCaptureComponent || !MapRenderTarget) return;
	MapCaptureComponent->CaptureScene();
	FTextureRenderTargetResource* RTResource = MapRenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource) return;
	TArray<FColor> Bitmap;
	RTResource->ReadPixels(Bitmap);
	for (FColor& Color : Bitmap) { Color.A = 255; }
	FString FolderPath2 = FPaths::ProjectSavedDir() + TEXT("HartiEclipse/");
	FString FileName = FString::Printf(TEXT("Traseu_Eclipsa_%s.png"), *FDateTime::Now().ToString());
	FString FullFilePath = FolderPath2 + FileName;
	FIntPoint DestSize(MapRenderTarget->SizeX, MapRenderTarget->SizeY);
	TArray<uint8> CompressedBitmap;
	FImageUtils::CompressImageArray(DestSize.X, DestSize.Y, Bitmap, CompressedBitmap);
	if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FullFilePath))
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green, FString::Printf(TEXT("Harta a fost salvata cu succes in: %s"), *FullFilePath));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Eroare la salvarea hartii!"));
	}
}
//aceasta functie are rolul sa imi adauge meniul de help pe ecran la apasarea tastei f10
void ADronaMea::ToggleHelpMenu()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		UHelpMenuWidget* HelpWidget = CreateWidget<UHelpMenuWidget>(PC, UHelpMenuWidget::StaticClass());
		if (HelpWidget)
		{
			HelpWidget->AddToViewport(100);
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
		}
	}
}


void ADronaMea::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Pentru siguranta maxima, daca drona este distrusa in timpul jocului (ex: schimbi harta)
	// Tragem "glontul de gratie" si aici. Daca serverul e deja mort, comanda pur si simplu va fi ignorata.
	FPlatformProcess::CreateProc(
		TEXT("taskkill.exe"),
		TEXT("/F /T /IM Server.exe"),
		true, true, true, nullptr, 0, nullptr, nullptr
	);

	if (PythonServerHandle.IsValid())
	{
		FPlatformProcess::CloseProc(PythonServerHandle);
		PythonServerHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

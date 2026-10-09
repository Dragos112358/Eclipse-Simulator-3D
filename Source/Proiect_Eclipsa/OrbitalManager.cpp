#include "OrbitalManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "CesiumGeoreference.h" 
#include <cmath> 
#include "GameFramework/Pawn.h"
#include "DronaMea.h"
#include "Components/LightComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Networking.h" 
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "Components/SkyLightComponent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformApplicationMisc.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
AOrbitalManager::AOrbitalManager()
{
	PrimaryActorTick.bCanEverTick = true;
	//mesh pentru umbra (zona de totalitate)
	UmbraProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("UmbraProcMesh"));
	UmbraProceduralMesh->SetupAttachment(RootComponent);
	UmbraProceduralMesh->SetCastShadow(false);
	UmbraProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//aici este meshul pentru penumbra
	PenumbraProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PenumbraProcMesh"));
	PenumbraProceduralMesh->SetupAttachment(RootComponent);
	PenumbraProceduralMesh->SetCastShadow(false);
	PenumbraProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//aici sunt 7 planete, nu am inclus Pluto, fiindca e o planeta prea mica si s-ar vedea prost cu telscopul
	TArray<FString> PlanetNames = { "Mesh_Mercur", "Mesh_Venus", "Mesh_Marte", "Mesh_Jupiter", "Mesh_Saturn", "Mesh_Uranus", "Mesh_Neptun" };
	PlanetMeshes.Empty();
	//aici incarc meshurile pentru planete, inclusiv sfera pe care o am la luna
	for (int32 i = 0; i < 7; i++)
	{
		UStaticMeshComponent* NewPlanet = CreateDefaultSubobject<UStaticMeshComponent>(*PlanetNames[i]);
		NewPlanet->SetupAttachment(RootComponent);
		NewPlanet->SetCollisionProfileName(TEXT("NoCollision"));
		NewPlanet->SetCastShadow(false);
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Script/Engine.StaticMesh'/Game/Textures/Sphere_perfect.Sphere_perfect'"));
		if (SphereMeshAsset.Succeeded())
		{
			NewPlanet->SetStaticMesh(SphereMeshAsset.Object);
		}
		NewPlanet->SetVisibility(false);
		PlanetMeshes.Add(NewPlanet);
	}
	/*
	 * Am optat pentru un material de tip Unlit (Emissive) pentru a garanta vizibilitatea planetelor la distante astronomice.
	 * Logica din spatele nodurilor materialului este urmatoarea:
	 * * 1. Power (Exp 1.5) pe PlanetTexture: Creste contrastul texturii (ex. Jupiter), accentuand detaliile luminoase si inchizand zonele intunecate.
	 * 2. Desaturation (-0.5): O desaturare negativa suprasatureaza matematic imaginea, oferind un boost de vibranta pentru a compensa fundalul negru al spatiului.
	 * 3. EyeAdaptationInverse: Neutralizeaza efectul de Auto-Exposure. Daca drona priveste direct spre Soare si ecranul se intuneca automat,
	 * acest nod se asigura ca planetele nu se intuneca odata cu restul scenei, ramanand clare pe cer.
	 * 4. Multiply -> Emissive Color: Combina textura suprasaturata cu corectia de expunere, ignorand sistemul clasic de umbre (Directional Light) al motorului grafic.
	 */
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(TEXT("/Script/Engine.Material'/Game/Textures/M_PlanetMaster.M_PlanetMaster'"));
	if (MatFinder.Succeeded())
	{
		PlanetMasterMaterial = MatFinder.Object;
	}
	PlanetTextures.Empty();
	PlanetTextures.SetNum(7);
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexMercur(TEXT("/Script/Engine.Texture2D'/Game/Textures/Mercury.Mercury'"));
	if (TexMercur.Succeeded()) PlanetTextures[0] = TexMercur.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexVenus(TEXT("/Script/Engine.Texture2D'/Game/Textures/Venus.Venus'"));
	if (TexVenus.Succeeded()) PlanetTextures[1] = TexVenus.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexMarte(TEXT("/Script/Engine.Texture2D'/Game/Textures/mars.mars'"));
	if (TexMarte.Succeeded()) PlanetTextures[2] = TexMarte.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexJupiter(TEXT("/Script/Engine.Texture2D'/Game/Textures/Jupiter.Jupiter'"));
	if (TexJupiter.Succeeded()) PlanetTextures[3] = TexJupiter.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexSaturn(TEXT("/Script/Engine.Texture2D'/Game/Textures/Saturn.Saturn'"));
	if (TexSaturn.Succeeded()) PlanetTextures[4] = TexSaturn.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexUranus(TEXT("/Script/Engine.Texture2D'/Game/Textures/Uranus.Uranus'"));
	if (TexUranus.Succeeded()) PlanetTextures[5] = TexUranus.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexNeptun(TEXT("/Script/Engine.Texture2D'/Game/Textures/Neptune.Neptune'"));
	if (TexNeptun.Succeeded()) PlanetTextures[6] = TexNeptun.Object;
	PrimaryActorTick.bCanEverTick = true;
	StarsISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StarsISMC"));
	//starsismc are rolul de a randa cele 5069 de stele ale mele fara sa desenez cate un mesh pentru fiecare (ar crapa procesorul daca as face asta)
	StarsISMC->SetupAttachment(RootComponent);
	StarsISMC->NumCustomDataFloats = 4;
	StarsISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StarsISMC->SetCastShadow(false); //fara umbre
	StarsISMC->bReceivesDecals = false;
	StarsISMC->bUseAsOccluder = false; 
	StarsISMC->SetBoundsScale(10000.0f); 
	StarsISMC->bAllowCullDistanceVolume = false;
	StarsISMC->InstanceStartCullDistance = 0;
	StarsISMC->InstanceEndCullDistance = 0; 
	StarsISMC->SetTranslucentSortPriority(-100);
	//incarc baza de date pentru stele, am folosit baza de date hyg de 120000 de stele, din care am filtrat doar stelele vizibile (magnitudine sub 6), adica 5000 de stele
	static ConstructorHelpers::FObjectFinder<UDataTable> StarsDataObj(TEXT("/Script/Engine.DataTable'/Game/Eclipse_combined/Stele.Stele'"));
	if (StarsDataObj.Succeeded())
	{
		StarsDataTable = StarsDataObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshObj(TEXT("/Script/Engine.StaticMesh'/Game/Textures/Sphere_perfect.Sphere_perfect'"));
	if (SphereMeshObj.Succeeded())
	{
		StarsISMC->SetStaticMesh(SphereMeshObj.Object);
	}
	//material special pentru stele, am un parametru de intensitate, care variaza intre 10 si 10000 (10 este valoare realista, 10000 este ca sa iasa in evidenta)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StarMat(TEXT("/Script/Engine.Material'/Game/Textures/M_StarReal.M_StarReal'"));
	if (StarMat.Succeeded())
	{
		StarsISMC->SetMaterial(0, StarMat.Object);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("EROARE: Nu am gasit materialul M_StarReal!"));
	}
	//aici creez liniile constelatiilor. Am folosit un script python pentru a extrage ce stele sunt legate si a avea o baza de date
	ConstellationLinesISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ConstellationLines"));
	ConstellationLinesISMC->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshObj(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshObj.Succeeded())
	{
		ConstellationLinesISMC->SetStaticMesh(CylinderMeshObj.Object);
	}
	//material pentru linii stralucitoare
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatObj(TEXT("/Script/Engine.Material'/Game/Textures/M_LinieGlow.M_LinieGlow'"));
	if (DefaultMatObj.Succeeded())
	{
		ConstellationLinesISMC->SetMaterial(0, DefaultMatObj.Object);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("EROARE: Nu am gasit materialul defaultmaterial!"));
	}
	//baza de date cu constelatiile, fiecare constelatii contine stelele componente si un nume
	static ConstructorHelpers::FObjectFinder<UDataTable> ConstDataObj(TEXT("/Script/Engine.DataTable'/Game/Eclipse_combined/Constelatii.Constelatii'"));
	if (ConstDataObj.Succeeded())
	{
		ConstellationsDataTable = ConstDataObj.Object;
	}
	//ismc pentru constelatii, setat exact la fel ca pentru stele
	ConstellationLinesISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ConstellationLinesISMC->SetCastShadow(false);
	ConstellationLinesISMC->bReceivesDecals = false;
	ConstellationLinesISMC->SetBoundsScale(10000.0f); 
	ConstellationLinesISMC->SetTranslucentSortPriority(-99); 
	ConstellationLinesISMC->InstanceStartCullDistance = 0;
	ConstellationLinesISMC->InstanceEndCullDistance = 0;
	ConstellationLinesISMC->NumCustomDataFloats = 1;
	StarLabelsRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StarLabelsRoot"));
	StarLabelsRoot->SetupAttachment(RootComponent);
	ZodiacRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZodiacRoot"));
	ZodiacRoot->SetupAttachment(RootComponent);
	//se vad semnele zodiacale doar noaptea pentru un efect deosebit
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ZMat(TEXT("/Script/Engine.Material'/Game/Textures/M_ZodiacMaster.M_ZodiacMaster'"));
	if (ZMat.Succeeded())
	{
		ZodiacMasterMaterial = ZMat.Object;
	}
	//inelul cu diamant din timpul unei eclipse totale
	DiamondSpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("DiamondSpotLight"));
	DiamondSpotLight->SetupAttachment(RootComponent);
	DiamondSpotLight->SetIntensityUnits(ELightUnits::Candelas);
	DiamondSpotLight->SetIntensity(0.0f);
	DiamondSpotLight->SetInnerConeAngle(0.0f);
	DiamondSpotLight->SetOuterConeAngle(0.5f); 
	DiamondSpotLight->SetCastShadows(false);
	DiamondSpotLight->SetAttenuationRadius(1000000000.0f);
	DiamondFlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiamondFlashMesh"));
	DiamondFlashMesh->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshObj(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	//plan folosit pentru a pune o poza cu coroana solara in spatele lunii la totalitate
	if (PlaneMeshObj.Succeeded())
	{
		DiamondFlashMesh->SetStaticMesh(PlaneMeshObj.Object);
	}
	//incarcare material pentru inelul cu diamantul (dureaza 10 secunde inainte de totalitate)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMatObj(TEXT("/Script/Engine.Material'/Game/Textures/M_DiamondFlash.M_DiamondFlash'"));
	if (FlashMatObj.Succeeded())
	{
		DiamondFlashMesh->SetMaterial(0, FlashMatObj.Object);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CONSTRUCTOR: Materialul M_DiamondFlash NU a fost gasit la calea specificata!"));
	}
	DiamondFlashMesh->SetVisibility(false);
	DiamondFlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DiamondFlashMesh->SetCastShadow(false);
	DiamondFlashMesh->SetBoundsScale(1000.0f);
	BeadMeshes.Empty();
	//adaugare efect de margele Bailey, am ales sa adaug 6 margele
	for (int32 i = 0; i < 6; i++)
	{
		FString BeadName = FString::Printf(TEXT("BailyBead_%d"), i);
		UStaticMeshComponent* BeadComp = CreateDefaultSubobject<UStaticMeshComponent>(*BeadName);
		BeadComp->SetupAttachment(RootComponent);
		BeadComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BeadComp->SetCastShadow(false);
		BeadComp->SetBoundsScale(10000.0f); 
		if (PlaneMeshObj.Succeeded())
		{
			BeadComp->SetStaticMesh(PlaneMeshObj.Object);
		}
		if (FlashMatObj.Succeeded())
		{
			BeadComp->SetMaterial(0, FlashMatObj.Object);
		}
		BeadComp->SetVisibility(false);
		BeadMeshes.Add(BeadComp);
	}
	CoronaMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoronaMesh"));
	CoronaMesh->SetupAttachment(RootComponent);
	CoronaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoronaMesh->SetCastShadow(false);
	CoronaMesh->SetTranslucentSortPriority(-98); 
	CoronaMesh->SetBoundsScale(10000.0f); 
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshObj2(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMeshObj2.Succeeded())
	{
		CoronaMesh->SetStaticMesh(PlaneMeshObj2.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoronaMatObj(TEXT("/Script/Engine.Material'/Game/Textures/M_CoronaMagic.M_CoronaMagic'"));
	if (CoronaMatObj.Succeeded())
	{
		CoronaMesh->SetMaterial(0, CoronaMatObj.Object);
	}
	CoronaMesh->SetVisibility(false);
	//incarcare con de umbra
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LfMat(TEXT("/Script/Engine.Material'/Game/Textures/M_ConUmbra.M_ConUmbra'"));
	if (LfMat.Succeeded())
	{
		LightFunctionMasterMat = LfMat.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CONSTRUCTOR: Nu am gasit materialul M_ConUmbra pentru Light Function!"));
	}
	static ConstructorHelpers::FObjectFinder<UMaterial> MatUmbraObj(TEXT("/Script/Engine.Material'/Game/Textures/M_ShadowCone.M_ShadowCone'"));
	if (MatUmbraObj.Succeeded())
	{
		UmbraMaterial = MatUmbraObj.Object;
	}
	else
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,           
				15.0f,        
				FColor::Red,  
				TEXT("EROARE: Materialul conului de umbra NU a fost incarcat! Verifica calea.")
			);
		}
	}
	//aici adaug umbra, penumbra si antumbra, antumbra este doar pentru eclipse hibride
	SurfaceShadowMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceShadowMesh"));
	SurfaceShadowMesh->SetupAttachment(RootComponent);
	SurfaceShadowMesh->SetCastShadow(false); 
	SurfaceShadowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfaceShadowMesh->SetBoundsScale(1000.0f);
	SurfacePenumbraMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfacePenumbraMesh"));
	SurfacePenumbraMesh->SetupAttachment(RootComponent);
	SurfacePenumbraMesh->SetCastShadow(false);
	SurfacePenumbraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfacePenumbraMesh->SetBoundsScale(1000.0f);
	SurfacePenumbraMesh->SetTranslucentSortPriority(10); 
	SurfaceAntumbraMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SurfaceAntumbraMesh"));
	SurfaceAntumbraMesh->SetupAttachment(RootComponent);
	SurfaceAntumbraMesh->SetCastShadow(false);
	SurfaceAntumbraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfaceAntumbraMesh->SetBoundsScale(1000.0f);
	SurfaceAntumbraMesh->SetTranslucentSortPriority(11);
	static ConstructorHelpers::FObjectFinder<UMaterial> MatAntumbraObj(TEXT("/Script/Engine.Material'/Game/Textures/M_ShadowCone.M_ShadowCone'"));
	if (MatAntumbraObj.Succeeded())
	{
		AntumbraMaterial = MatAntumbraObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterial> MatPenumbraObj(TEXT("/Script/Engine.Material'/Game/Textures/M_ShadowCone.M_ShadowCone'"));
	if (MatPenumbraObj.Succeeded())
	{
		PenumbraMaterial = MatPenumbraObj.Object;
	}
	AntumbraProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("AntumbraProcMesh"));
	AntumbraProceduralMesh->SetupAttachment(RootComponent);
	AntumbraProceduralMesh->SetCastShadow(false);
	AntumbraProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SurfaceAntumbraMesh->SetBoundsScale(1000.0f);
	SurfaceAntumbraMesh->SetTranslucentSortPriority(12);
	TArray<FString> MoonsNames = { "Mesh_Io", "Mesh_Europa", "Mesh_Ganymede", "Mesh_Callisto", "Mesh_Titan", "Mesh_Triton" };
	MajorMoonsMeshes.Empty();
	MajorMoonsTextures.Empty();
	MajorMoonsTextures.SetNum(6);
	//incarcare meshuri pentru cei 6 sateliti mari din sistemul solar, sunt foarte multi sateliti si as fi incarcat prea mult sa fi avut mai multi sateliti
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexIo(TEXT("/Script/Engine.Texture2D'/Game/Textures/Io.Io'"));
	if (TexIo.Succeeded()) MajorMoonsTextures[0] = TexIo.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexEuropa(TEXT("/Script/Engine.Texture2D'/Game/Textures/Europa.Europa'"));
	if (TexEuropa.Succeeded()) MajorMoonsTextures[1] = TexEuropa.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexGanymede(TEXT("/Script/Engine.Texture2D'/Game/Textures/Ganymede_bun.Ganymede_bun'"));
	if (TexGanymede.Succeeded()) MajorMoonsTextures[2] = TexGanymede.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexCallisto(TEXT("/Script/Engine.Texture2D'/Game/Textures/Calisto.Calisto'"));
	if (TexCallisto.Succeeded()) MajorMoonsTextures[3] = TexCallisto.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexTitan(TEXT("/Script/Engine.Texture2D'/Game/Textures/Titan.Titan'"));
	if (TexTitan.Succeeded()) MajorMoonsTextures[4] = TexTitan.Object;
	static ConstructorHelpers::FObjectFinder<UTexture2D> TexTriton(TEXT("/Script/Engine.Texture2D'/Game/Textures/Triton.Triton'"));
	if (TexTriton.Succeeded()) MajorMoonsTextures[5] = TexTriton.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MoonSphereAsset(TEXT("/Script/Engine.StaticMesh'/Game/Textures/Sphere_perfect.Sphere_perfect'"));
	for (int32 i = 0; i < MoonsNames.Num(); i++)
	{
		UStaticMeshComponent* NewMoon = CreateDefaultSubobject<UStaticMeshComponent>(*MoonsNames[i]);
		NewMoon->SetupAttachment(RootComponent);
		NewMoon->SetCollisionProfileName(TEXT("NoCollision"));
		NewMoon->SetCastShadow(false);
		if (MoonSphereAsset.Succeeded())
		{
			NewMoon->SetStaticMesh(MoonSphereAsset.Object);
		}
		NewMoon->SetBoundsScale(10000.0f);
		NewMoon->SetVisibility(false);
		MajorMoonsMeshes.Add(NewMoon);
	}
	//adaugare inele saturn
	SaturnRingsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SaturnRings"));
	if (PlanetMeshes.IsValidIndex(4) && PlanetMeshes[4])
	{
		SaturnRingsMesh->SetupAttachment(PlanetMeshes[4]);
	}
	else
	{
		SaturnRingsMesh->SetupAttachment(RootComponent); 
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneRingAsset(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneRingAsset.Succeeded())
	{
		SaturnRingsMesh->SetStaticMesh(PlaneRingAsset.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMat(TEXT("/Script/Engine.Material'/Game/Textures/M_SaturnRings.M_SaturnRings'"));
	if (RingMat.Succeeded())
	{
		SaturnRingsMesh->SetMaterial(0, RingMat.Object);
	}
	//aici setez inelele lui Saturn si banda de totalitate
	SaturnRingsMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SaturnRingsMesh->SetCastShadow(false);
	SaturnRingsMesh->SetBoundsScale(10000.0f); 
	SaturnRingsMesh->SetRelativeScale3D(FVector(2.0f, 2.0f, 1.0f));
	SaturnRingsMesh->SetVisibility(false);
	EclipsePathMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EclipsePathMesh"));
	EclipsePathMesh->SetupAttachment(RootComponent);
	EclipsePathMesh->SetCastShadow(false);
	EclipsePathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EclipsePathMesh->SetBoundsScale(10000.0f);
	EclipsePathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("EclipsePathSpline"));
	EclipsePathSpline->SetupAttachment(RootComponent);
	EclipsePathISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("EclipsePathISMC"));
	EclipsePathISMC->SetupAttachment(RootComponent);
	EclipsePathISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EclipsePathISMC->SetCastShadow(false);
	EclipsePathISMC->bReceivesDecals = false;
	EclipsePathISMC->SetBoundsScale(10000.0f); 
	EclipsePathISMC->InstanceStartCullDistance = 0;
	EclipsePathISMC->InstanceEndCullDistance = 0;
	EclipsePathISMC->SetBoundsScale(1048576.0f); 
	EclipsePathISMC->SetTranslucentSortPriority(50);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		EclipsePathISMC->SetStaticMesh(CubeMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PathMat(TEXT("/Script/Engine.Material'/Game/Textures/M_TotalPath.M_TotalPath'"));
	if (PathMat.Succeeded())
	{
		EclipsePathMaterial = PathMat.Object;
		EclipsePathISMC->SetMaterial(0, EclipsePathMaterial);
	}

	EarthUmbraConeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EarthUmbraConeMesh"));
	EarthUmbraConeMesh->SetupAttachment(RootComponent);
	EarthUmbraConeMesh->SetCastShadow(false);
	EarthUmbraConeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EarthUmbraConeMesh->SetBoundsScale(100000000.0f);
	EarthUmbraConeMesh->SetTranslucentSortPriority(20);

	EarthPenumbraConeMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EarthPenumbraConeMesh"));
	EarthPenumbraConeMesh->SetupAttachment(RootComponent);
	EarthPenumbraConeMesh->SetCastShadow(false);
	EarthPenumbraConeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EarthPenumbraConeMesh->SetBoundsScale(100000000.0f);
	EarthPenumbraConeMesh->SetTranslucentSortPriority(19);

	//refolosim acelasi material de baza M_ShadowCone, dar cu instante dinamice proprii pentru culori separate
	static ConstructorHelpers::FObjectFinder<UMaterial> MatEarthUmbraObj(TEXT("/Script/Engine.Material'/Game/Textures/M_ShadowCone.M_ShadowCone'"));
	if (MatEarthUmbraObj.Succeeded())
	{
		EarthUmbraMaterial = MatEarthUmbraObj.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterial> MatEarthPenumbraObj(TEXT("/Script/Engine.Material'/Game/Textures/M_ShadowCone.M_ShadowCone'"));
	if (MatEarthPenumbraObj.Succeeded())
	{
		EarthPenumbraMaterial = MatEarthPenumbraObj.Object;
	}
}
void AOrbitalManager::BeginPlay()
{
	Super::BeginPlay();
	//sistem de securitate foarte bun pentru a nu permite oricui sa acceseze executabilul, chit ca il poate instala
	//extrag machine id de 32 de biti + textul ECLIPSA-SECRET, pe care il convertesc in md5
	//utilizatorulul i se va afisa codul pe ecran si i se va cere sa imi trimita un mail, iar eu ii dau cheia
	//este o tehnica buna, stiu ca va functiona pe un singur pc
/*
#if !WITH_EDITOR
	FString HardwareID = FPlatformMisc::GetMachineId().ToString();
	FString TextSecret = HardwareID + TEXT("-ECLIPSA-SECRET");
	FString CheiaAsteptata = FMD5::HashAnsiString(*TextSecret).ToLower();
	FString CaleFisier = FPaths::Combine(FPaths::RootDir(), TEXT("licenta.dat"));
	FString CheieCitita;
	bool bEchipaValida = false;
	if (FFileHelper::LoadFileToString(CheieCitita, *CaleFisier))
	{
		CheieCitita = CheieCitita.TrimStartAndEnd().ToLower();
		if (CheieCitita == CheiaAsteptata)
		{
			bEchipaValida = true;
		}
	}
	if (!bEchipaValida)
	{
		FString TextDinClipboard;
		FPlatformApplicationMisc::ClipboardPaste(TextDinClipboard); 
		TextDinClipboard = TextDinClipboard.TrimStartAndEnd().ToLower();
		if (TextDinClipboard == CheiaAsteptata)
		{
			FFileHelper::SaveStringToFile(CheiaAsteptata, *CaleFisier);
			bEchipaValida = true;
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("License activated successfully! The file has been generated automatically. Thank you!")));
		}
	}
	if (!bEchipaValida)
	{
		FPlatformApplicationMisc::ClipboardCopy(*HardwareID);
		FString MailToLink = FString::Printf(TEXT("mailto:dragos.eduard.gabriel@gmail.com?subject=%s"), *HardwareID);
		FPlatformProcess::LaunchURL(*MailToLink, nullptr, nullptr);
		FString MesajEroare = FString::Printf(TEXT("Eclipse Simulator is not activated!\n\nYour Hardware ID is:\n%s\n\n(Good news: We already COPIED this ID to your clipboard!)\n\nTo activate, send an email to dragos.eduard.gabriel@gmail.com with your ID as the Subject. (Your email app should have just opened automatically).\n\nIn 1 minute, you will receive your key. COPY it and restart the game!"), *HardwareID);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(MesajEroare));
		FGenericPlatformMisc::RequestExit(false);
		return;
	}
#endif
	UE_LOG(LogTemp, Warning, TEXT("Licenta ok! Pornim eclipsele."));
	//aici in begin play, setez timeout la 3 secunde, acest timeout reprezinta timpul alocat gasirii solului
	*/
	ADronaMea* Drona1234 = Cast<ADronaMea>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	if (Drona1234)
	{
		Drona1234->SetActorLocation(FVector(0.f, 0.f, 5000.0f));
		Drona1234->bSearchingForGround = true;
		Drona1234->SearchTimeout = 3.0f;
	}
	float SavedR = 0.0f;
	float SavedG = 0.0f;
	float SavedB = 0.0f;
	float SavedA = 0.85f;
	//extrag configurarile salvate in fisierul config.ini
	if (UmbraMaterial)
	{
		DynUmbraMaterial = UMaterialInstanceDynamic::Create(UmbraMaterial, this);
		if (UmbraProceduralMesh)
		{
			UmbraProceduralMesh->SetMaterial(0, DynUmbraMaterial);
		}
		if (DynUmbraMaterial)
		{
			if (GConfig)
			{
				GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareR"), SavedR, GGameUserSettingsIni);
				GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareG"), SavedG, GGameUserSettingsIni);
				GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareB"), SavedB, GGameUserSettingsIni);
				GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("Opacitate"), SavedA, GGameUserSettingsIni);
			}
			//setez culoarea si opacitatea din ini, ele pot fi modificate din setari
			DynUmbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(SavedR, SavedG, SavedB, 1.0f));
			DynUmbraMaterial->SetScalarParameterValue(FName("Opacitate"), SavedA);
		}
	}
	//antumbra si penumbra au si ele zone alocate pentru a putea schimba culorile, pot fi culori naturale default sau pot fi ce culori vrea utilizatorul
	if (AntumbraMaterial)
	{
		DynAntumbraMaterial = UMaterialInstanceDynamic::Create(AntumbraMaterial, this);
		if (AntumbraProceduralMesh) AntumbraProceduralMesh->SetMaterial(0, DynAntumbraMaterial);
		if (SurfaceAntumbraMesh) SurfaceAntumbraMesh->SetMaterial(0, DynAntumbraMaterial);
		if (DynAntumbraMaterial)
		{
			DynAntumbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(SavedR, SavedG, SavedB, 1.0f));
			DynAntumbraMaterial->SetScalarParameterValue(FName("Opacitate"), SavedA);
		}
	}
	if (PenumbraMaterial)
	{
		DynPenumbraMaterial = UMaterialInstanceDynamic::Create(PenumbraMaterial, this);
		if (PenumbraProceduralMesh) PenumbraProceduralMesh->SetMaterial(0, DynPenumbraMaterial);
		if (SurfacePenumbraMesh) SurfacePenumbraMesh->SetMaterial(0, DynPenumbraMaterial);
		float SavedPenR = 0.0f;
		float SavedPenG = 0.0f;
		float SavedPenB = 0.0f;
		float SavedPenA = 0.3f; 
		if (GConfig)
		{
			GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareR"), SavedPenR, GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareG"), SavedPenG, GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareB"), SavedPenB, GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("Opacitate"), SavedPenA, GGameUserSettingsIni);
		}
		if (DynPenumbraMaterial)
		{
			DynPenumbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(SavedPenR, SavedPenG, SavedPenB, 1.0f));
			DynPenumbraMaterial->SetScalarParameterValue(FName("Opacitate"), SavedPenA);
		}
	}

	//culoare separata pentru umbra proprie a Pamantului (implicit un albastru-inchis, distinct de umbra Lunii)
	if (EarthUmbraMaterial)
	{
		DynEarthUmbraMaterial = UMaterialInstanceDynamic::Create(EarthUmbraMaterial, this);
		if (EarthUmbraConeMesh) EarthUmbraConeMesh->SetMaterial(0, DynEarthUmbraMaterial);
		if (DynEarthUmbraMaterial)
		{
			DynEarthUmbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(0.02f, 0.02f, 0.08f, 1.0f));
			DynEarthUmbraMaterial->SetScalarParameterValue(FName("Opacitate"), 0.6f);
		}
	}

	//culoare separata pentru penumbra proprie a Pamantului
	if (EarthUmbraMaterial)
	{
		DynEarthUmbraMaterial = UMaterialInstanceDynamic::Create(EarthUmbraMaterial, this);
		if (EarthUmbraConeMesh) EarthUmbraConeMesh->SetMaterial(0, DynEarthUmbraMaterial);
	}
	if (EarthPenumbraMaterial)
	{
		DynEarthPenumbraMaterial = UMaterialInstanceDynamic::Create(EarthPenumbraMaterial, this);
		if (EarthPenumbraConeMesh) EarthPenumbraConeMesh->SetMaterial(0, DynEarthPenumbraMaterial);
	}
	ApplyEarthShadowColors(); // citeste culorile salvate (sau default-urile)

	//aici initializez socketul pentru portul udp 5000
	InitSocket();
	bUseDatabasePrecision = true; 
	LoadECEFData();         
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		EnableInput(PC);
	}
	//aici parsez baza de date cu eclipse
	LoadAndParseDatabase();
	LoadAndParseLunarDatabase(); // <-- adauga asta
	LoadCountryPoints();
	//tasta c reduce viteza de redare de 2 ori
	//tasta v o dubleaza
	//p vine de la pause sau pauza
	//iar tasta este pentru vizualizare nori
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AOrbitalManager::DecreaseTimeSpeed);
		InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AOrbitalManager::IncreaseTimeSpeed);
		InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AOrbitalManager::TogglePause);
		InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AOrbitalManager::ToggleClouds);
	}
	GenerateStarSphere();
	GenerateConstellationLabels();
	BeadMaterialsDynamic.Empty();
	//aici folosesc efectul special de margele bailey
	for (UStaticMeshComponent* Bead : BeadMeshes)
	{
		if (Bead && Bead->GetMaterial(0))
		{
			UMaterialInstanceDynamic* BeadDyn = UMaterialInstanceDynamic::Create(Bead->GetMaterial(0), this);
			Bead->SetMaterial(0, BeadDyn);
			BeadMaterialsDynamic.Add(BeadDyn);
		}
	}
	if (CoronaMesh && CoronaMesh->GetMaterial(0))
	{
		CoronaDynamicMat = UMaterialInstanceDynamic::Create(CoronaMesh->GetMaterial(0), this);
		CoronaMesh->SetMaterial(0, CoronaDynamicMat);
	}
	if (GConfig)
	{
		//aici setez ziua,luna si anul
		int32 SavedY = CurrentDate.Year;
		int32 SavedM = CurrentDate.Month;
		int32 SavedD = CurrentDate.Day;
		int32 SavedUH = 10, SavedUMin = 0, SavedUSec = 0;
		bool bHasCustomTime = false;
		if (GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnYear"), SavedY, GGameUserSettingsIni))
		{
			bHasCustomTime = true;
			GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMonth"), SavedM, GGameUserSettingsIni);
			GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnDay"), SavedD, GGameUserSettingsIni);
			GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnHourUTC"), SavedUH, GGameUserSettingsIni);
			GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMinuteUTC"), SavedUMin, GGameUserSettingsIni);
			GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnSecondUTC"), SavedUSec, GGameUserSettingsIni);
		}
		if (bHasCustomTime)
		{
			float SelectedHourFloat = (float)SavedUH + (SavedUMin / 60.0f) + (SavedUSec / 3600.0f);
			SetSimulationDateTime(SavedD, SavedM, SavedY, SelectedHourFloat);
		}
	}
	APlayerController* GlobalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (GlobalPC)
	{
		GlobalPC->bShowMouseCursor = true;
		GlobalPC->bEnableClickEvents = true;
		GlobalPC->bEnableMouseOverEvents = true;
		FInputModeGameAndUI GlobalInputMode;
		GlobalInputMode.SetHideCursorDuringCapture(false);
		GlobalInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		GlobalPC->SetInputMode(GlobalInputMode);
	}

	//verific daca este prima rulare a aplicatiei, daca da cer cheia Cesium
	/*bool bFirstRunDone = false;
	GConfig->GetBool(TEXT("SetariCustom"), TEXT("PrimaRulare"), bFirstRunDone, GGameUserSettingsIni);
	if (!bFirstRunDone && GlobalPC)
	{
		UFirstRunSetupWidget* SetupWidget = CreateWidget<UFirstRunSetupWidget>(GlobalPC, UFirstRunSetupWidget::StaticClass());
		if (SetupWidget)
		{
			SetupWidget->AddToViewport(20000); //Z-order mare, sa fie deasupra la tot (HUD, meniuri etc)
		}
	}*/
	/*bool bFirstRunDone = false;
	GConfig->GetBool(TEXT("SetariCustom"), TEXT("PrimaRulare"), bFirstRunDone, GGameUserSettingsIni);

	if (!bFirstRunDone)
	{
		// Așteptăm 0.5 secunde pentru ca Viewport-ul și PlayerController-ul să fie 100% inițializate
		FTimerHandle SetupTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(SetupTimerHandle, [this]()
			{
				APlayerController* TimerPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				if (TimerPC)
				{
					UFirstRunSetupWidget* SetupWidget = CreateWidget<UFirstRunSetupWidget>(TimerPC, UFirstRunSetupWidget::StaticClass());
					if (SetupWidget)
					{
						SetupWidget->AddToViewport(20000);

						// Este esențial să îi dăm focus, altfel userul poate da click pe hartă în fundal
						FInputModeUIOnly UIOnlyMode;
						UIOnlyMode.SetWidgetToFocus(SetupWidget->TakeWidget());
						TimerPC->SetInputMode(UIOnlyMode);
					}
				}
			}, 0.5f, false); // 0.5f reprezintă jumătate de secundă
	}*/

	/*bool bFirstRunDone = false;
	FString SavedCesiumKey = TEXT("");

	if (GConfig)
	{
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("PrimaRulare"), bFirstRunDone, GGameUserSettingsIni);
		GConfig->GetString(TEXT("SetariCustom"), TEXT("CesiumKey"), SavedCesiumKey, GGameUserSettingsIni);
	}

	// Arătăm meniul dacă e prima rulare SAU dacă lipsește cheia de Cesium complet
	if (!bFirstRunDone || SavedCesiumKey.TrimStartAndEnd().IsEmpty())
	{
		// Așteptăm 0.5 secunde pentru ca Viewport-ul și PlayerController-ul să fie 100% inițializate
		FTimerHandle SetupTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(SetupTimerHandle, [this]()
			{
				APlayerController* TimerPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				if (TimerPC)
				{
					UFirstRunSetupWidget* SetupWidget = CreateWidget<UFirstRunSetupWidget>(TimerPC, UFirstRunSetupWidget::StaticClass());
					if (SetupWidget)
					{
						SetupWidget->AddToViewport(20000);

						// Este esențial să îi dăm focus, altfel userul poate da click pe hartă în fundal
						FInputModeUIOnly UIOnlyMode;
						UIOnlyMode.SetWidgetToFocus(SetupWidget->TakeWidget());
						TimerPC->SetInputMode(UIOnlyMode);
					}
				}
			}, 0.5f, false);
	}*/
	/*if (GlobalPC)
	{
		UFirstRunSetupWidget* SetupWidget = CreateWidget<UFirstRunSetupWidget>(GlobalPC, UFirstRunSetupWidget::StaticClass());
		if (SetupWidget)
		{
			SetupWidget->AddToViewport(20000);
		}
	}*/


	// ==========================================================
	// VERIFICARE PRIMA RULARE CESIUM (Sistem cu rulare.dat)
	// ==========================================================
	bool bFirstRunDone = false;
	FString SavedCesiumKey = TEXT("");
	FString CaleFisierRulare = FPaths::Combine(FPaths::RootDir(), TEXT("rulare.dat"));

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*CaleFisierRulare))
	{
		if (FFileHelper::LoadFileToString(SavedCesiumKey, *CaleFisierRulare))
		{
			SavedCesiumKey = SavedCesiumKey.TrimStartAndEnd();
			if (!SavedCesiumKey.IsEmpty())
			{
				bFirstRunDone = true;
			}
		}
	}

	// Arătăm meniul dacă e prima rulare SAU dacă lipsește cheia din fișier
	if (!bFirstRunDone)
	{
		// Așteptăm 0.5 secunde pentru ca Viewport-ul și PlayerController-ul să fie 100% inițializate
		FTimerHandle SetupTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(SetupTimerHandle, [this]()
			{
				APlayerController* TimerPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				if (TimerPC)
				{
					UFirstRunSetupWidget* SetupWidget = CreateWidget<UFirstRunSetupWidget>(TimerPC, UFirstRunSetupWidget::StaticClass());
					if (SetupWidget)
					{
						SetupWidget->AddToViewport(20000);

						// Este esențial să îi dăm focus, altfel userul poate da click pe hartă în fundal
						FInputModeUIOnly UIOnlyMode;
						UIOnlyMode.SetWidgetToFocus(SetupWidget->TakeWidget());
						TimerPC->SetInputMode(UIOnlyMode);
					}
				}
			}, 0.5f, false);
	}
}

void AOrbitalManager::ApplyEarthShadowColors()
{
	// valorile default, folosite daca nu exista nimic salvat in config
	float UR = 0.02f, UG = 0.02f, UB = 0.08f, UA = 0.6f;
	float PR = 0.15f, PG = 0.1f, PB = 0.02f, PA = 0.25f;

	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareR"), UR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareG"), UG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareB"), UB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("Opacitate"), UA, GGameUserSettingsIni);

		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareR"), PR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareG"), PG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareB"), PB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("Opacitate"), PA, GGameUserSettingsIni);
	}

	if (DynEarthUmbraMaterial)
	{
		DynEarthUmbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(UR, UG, UB, 1.0f));
		DynEarthUmbraMaterial->SetScalarParameterValue(FName("Opacitate"), UA);
	}
	if (DynEarthPenumbraMaterial)
	{
		DynEarthPenumbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(PR, PG, PB, 1.0f));
		DynEarthPenumbraMaterial->SetScalarParameterValue(FName("Opacitate"), PA);
	}
}
//aceasta functie imi activeaza/dezactiveaza norii
void AOrbitalManager::ToggleClouds()
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
	bool bFoundAny = false;
	for (AActor* Actor : FoundActors)
	{
		if (Actor && Actor->GetName().Contains("VolumetricCloud"))
		{
			bool bIsHidden = Actor->IsHidden();
			Actor->SetActorHiddenInGame(!bIsHidden);
			FString State = (!bIsHidden) ? TEXT("OPRITI") : TEXT("PORNITI");
			FColor Col = (!bIsHidden) ? FColor::Red : FColor::Green;
			GEngine->AddOnScreenDebugMessage(-1, 3.f, Col, FString::Printf(TEXT("NORI: %s"), *State));
			bFoundAny = true;
			break; 
		}
	}
	if (!bFoundAny)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("EROARE: Nu gasesc actor cu numele 'VolumetricCloud'!"));
	}
}
//aceasta functie e de test
void AOrbitalManager::ToggleECEFMode()
{
	bUseECEFMode = !bUseECEFMode;
	if (bUseECEFMode)
	{
		SetSimulationDateTime(12, 8, 2026, 16.0f);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, TEXT("MOD ECEF ACTIVAT: Date NASA Globale"));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("MOD ECEF DEZACTIVAT: Revenire la Standard"));
	}
}
//si aceasta
void AOrbitalManager::LoadECEFData()
{
	FString FilePath = FPaths::ProjectContentDir() + TEXT("Eclipse_combined/Eclipse_Global_ECEF.csv");
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		CachedECEFData.Empty();
		for (int32 i = 1; i < Lines.Num(); i++)
		{
			TArray<FString> Parts;
			Lines[i].ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() >= 9)
			{
				FEclipseECEFRow NewRow;
				int32 H = FCString::Atoi(*Parts[0]);
				int32 M = FCString::Atoi(*Parts[1]);
				int32 S = FCString::Atoi(*Parts[2]);
				NewRow.TotalSeconds = H * 3600.0 + M * 60.0 + S;
				NewRow.SunECEF_Km.X = FCString::Atod(*Parts[3]);
				NewRow.SunECEF_Km.Y = FCString::Atod(*Parts[4]);
				NewRow.SunECEF_Km.Z = FCString::Atod(*Parts[5]);
				NewRow.MoonECEF_Km.X = FCString::Atod(*Parts[6]);
				NewRow.MoonECEF_Km.Y = FCString::Atod(*Parts[7]);
				NewRow.MoonECEF_Km.Z = FCString::Atod(*Parts[8]);
				CachedECEFData.Add(NewRow);
			}
		}
		bECEFDataLoaded = true;
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("CSV ECEF Loaded: %d rows"), CachedECEFData.Num()));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc Eclipse_Global_ECEF.csv!"));
	}
}
//functia aceasta imi punea luna la 500.000 km si o scala, era o functie importanta implementata in februarie, dar care nu isi mai are rostul, din cauza utilizarii backendului de python
void AOrbitalManager::UpdateFromECEF(float CurrentTotalSeconds)
{
	if (CachedECEFData.Num() < 2) return;
	FEclipseECEFRow RowA, RowB;
	float Alpha = 0.0f;
	bool bFound = false;
	for (int32 i = 0; i < CachedECEFData.Num() - 1; i++)
	{
		if (CurrentTotalSeconds >= CachedECEFData[i].TotalSeconds &&
			CurrentTotalSeconds < CachedECEFData[i + 1].TotalSeconds)
		{
			RowA = CachedECEFData[i];
			RowB = CachedECEFData[i + 1];
			float Duration = RowB.TotalSeconds - RowA.TotalSeconds;
			if (Duration > 0) Alpha = (CurrentTotalSeconds - RowA.TotalSeconds) / Duration;
			bFound = true;
			break;
		}
	}
	if (!bFound) return;
	FVector SunECEF_Km = FMath::Lerp(RowA.SunECEF_Km, RowB.SunECEF_Km, Alpha);
	FVector MoonECEF_Km = FMath::Lerp(RowA.MoonECEF_Km, RowB.MoonECEF_Km, Alpha);
	FVector SunECEF_Cm = SunECEF_Km * 100000.0;
	FVector MoonECEF_Cm = MoonECEF_Km * 100000.0;
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!GeoRef || !Player) return;
	FVector PlayerPosUnreal = Player->GetActorLocation();
	FVector PlayerECEF_Cm = GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPosUnreal);
	FVector SunDirection_ECEF = (SunECEF_Cm - PlayerECEF_Cm).GetSafeNormal();
	FVector MoonDirection_ECEF = (MoonECEF_Cm - PlayerECEF_Cm).GetSafeNormal();
	FVector SunDirUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(SunDirection_ECEF);
	FVector MoonDirUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(MoonDirection_ECEF);
	if (SunSkyActor)
	{
		UDirectionalLightComponent* Light = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
		if (Light)
		{
			Light->SetWorldRotation((-SunDirUnreal).Rotation());
			Light->SetLightSourceAngle(0.53f);
			float Intensity = 10.0f; 
		}
	}
	if (ControlledMoon)
	{
		double VisualDistCm = 500000000.0; 
		FVector MoonPosViz = PlayerPosUnreal + (MoonDirUnreal * VisualDistCm);
		ControlledMoon->SetActorLocation(MoonPosViz);
		ControlledMoon->SetActorRotation((PlayerPosUnreal - MoonPosViz).Rotation());
		double RealDistCm = FVector::Dist(MoonECEF_Cm, PlayerECEF_Cm);
		double MoonDiamCm = 347480000.0;
	}
	if (bUseECEFMode)
	{
		float Dot = FVector::DotProduct(SunDirUnreal, MoonDirUnreal);
		float Angle = FMath::RadiansToDegrees(acos(FMath::Clamp(Dot, -1.f, 1.f)));
		GEngine->AddOnScreenDebugMessage(810, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Unghi Unreal: %.4f"), Angle));
	}
}
//alta functie de test
void AOrbitalManager::CheckAndSelectActiveEclipse()
{
	CurrentEclipseData.bIsValid = false;
	for (const FBesselianElements& Elem : AllEclipses)
	{
		if (Elem.Year == CurrentDate.Year &&
			Elem.Month == CurrentDate.Month &&
			Elem.Day == CurrentDate.Day)
		{
			CurrentEclipseData = Elem;
			CurrentEclipseData.bIsValid = true; 
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("ECLIPSA ACTIVA: %s (T0: %.2f)"), *Elem.DateString, Elem.T0));
			return;
		}
	}
}
// Aici parsez efectiv baza de date de eclipse, contine date foarte importante, cum ar fi unghiuri, ciclul saros din care face parte eclipsa etc.
/*void AOrbitalManager::LoadAndParseDatabase()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("Eclipse_de_soare.txt"));
	TArray<FString> Lines;
	AllEclipses.Empty();

	if (FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		UE_LOG(LogTemp, Display, TEXT("Fisier incarcat: %d linii. Incep parsarea..."), Lines.Num());
		int32 ParsedCount = 0;

		for (const FString& Line : Lines)
		{
			if (Line.Len() < 100) continue;

			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() < 46) continue;
			if (!Parts[0].Contains(TEXT("-"))) continue;

			FBesselianElements NewEclipse;
			NewEclipse.DateString = Parts[0];

			// =================================================================
			// SCUT ANTI-CRASH Î.HR. (Suport pentru anii negativi între -13000 și 17000)
			// =================================================================
			bool bIsNegativeYear = false;
			FString DateToParse = NewEclipse.DateString;

			// Dacă data începe cu minus, înseamnă că e an dinainte de Hristos
			if (DateToParse.StartsWith(TEXT("-")))
			{
				bIsNegativeYear = true;
				DateToParse = DateToParse.RightChop(1); // Tăiem primul minus ca ParseIntoArray să împartă curat [An, Lună, Zi]
			}

			TArray<FString> DateParts;
			DateToParse.ParseIntoArray(DateParts, TEXT("-"), true);

			// Extragere dată calendaristică
			if (DateParts.Num() >= 3)
			{
				NewEclipse.Year = FCString::Atoi(*DateParts[0]);

				// Dacă am detectat inițial că e an negativ, îi punem valoarea astronomică cu minus
				if (bIsNegativeYear)
				{
					NewEclipse.Year = -NewEclipse.Year;
				}

				// Noua barieră extinsă de siguranță conform efemeridelor NASA DE441
				if (NewEclipse.Year < -13000 || NewEclipse.Year > 17000) continue;

				NewEclipse.Month = GetMonthFromStr(DateParts[1]);
				NewEclipse.Day = FCString::Atoi(*DateParts[2]);
			}
			else continue;
			// =================================================================

			// Extragere Saros
			NewEclipse.Saros = FCString::Atoi(*Parts[6]);
			double RawT0 = FCString::Atod(*Parts[10]);

			if (RawT0 > 100.0)
			{
				double JdFromMidnight = RawT0 + 0.5;
				double FractionalDay = JdFromMidnight - floor(JdFromMidnight);
				NewEclipse.T0 = FractionalDay * 24.0;
			}
			else
			{
				NewEclipse.T0 = RawT0;
			}

			/* Extragere date relevante
			x, y, z sunt coordonatele centrului umbrei lunii pe planul fundamental al Terrei
			d = unghiul axei umbrei fata de ecuatorul ceresc
			mu = unghiul orar
			l1 = raza penumbrei
			l2 = raza umbrei
			tanf1 = tangenta semi unghiului conului de penumbra
			tanf2 = tangenta semi unghiului conului de umbra */
			/*for (int i = 0; i < 4; i++) NewEclipse.x[i] = FCString::Atod(*Parts[11 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.y[i] = FCString::Atod(*Parts[15 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.z[i] = FCString::Atod(*Parts[19 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.d[i] = FCString::Atod(*Parts[23 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.mu[i] = FCString::Atod(*Parts[27 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l1[i] = FCString::Atod(*Parts[31 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l2[i] = FCString::Atod(*Parts[35 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf1[i] = FCString::Atod(*Parts[39 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf2[i] = FCString::Atod(*Parts[43 + i]);

			NewEclipse.bIsValid = true;
			AllEclipses.Add(NewEclipse);
			ParsedCount++;
		}

		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			FString::Printf(TEXT("SUCCESS: %d Eclipse incarcate complet (interval -13000 -> 17000)."), ParsedCount));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc fisierul!"));
	}
}*/

//functie pentru parsare puncte pentru tari
// Parser CSV care respecta ghilimelele: "Bonaire, Sint Eustatius and Saba" ramane un singur camp
static void ParseCsvLine(const FString& Line, TArray<FString>& OutFields)
{
	OutFields.Reset();
	FString Current;
	bool bInQuotes = false;

	for (int32 i = 0; i < Line.Len(); ++i)
	{
		const TCHAR C = Line[i];

		if (C == TEXT('"'))
		{
			// "" in interiorul unui camp intre ghilimele = o ghilimea escapata
			if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
			{
				Current.AppendChar(TEXT('"'));
				++i;
			}
			else
			{
				bInQuotes = !bInQuotes;
			}
		}
		else if (C == TEXT(',') && !bInQuotes)
		{
			OutFields.Add(Current.TrimStartAndEnd());
			Current.Reset();
		}
		else
		{
			Current.AppendChar(C);
		}
	}
	OutFields.Add(Current.TrimStartAndEnd());
}
void AOrbitalManager::LoadCountryPoints()
{
	CountryPoints.Empty();
	AllCitiesFlat.Empty();
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("CountryPoints.csv"));
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			AOrbitalManager::TrString("EROARE: Nu gasesc CountryPoints.csv!", "ERROR: CountryPoints.csv not found!"));
		return;
	}

	int32 ParsedCount = 0;
	int32 SkippedCount = 0;
	TArray<FString> Parts;

	for (int32 i = 1; i < Lines.Num(); i++) // sarim header-ul
	{
		const FString& Line = Lines[i];
		if (Line.Len() < 3) continue;

		ParseCsvLine(Line, Parts);

		// Country,City,Latitude,Longitude,Elevation_m
		if (Parts.Num() != 5)
		{
			UE_LOG(LogTemp, Warning, TEXT("CountryPoints.csv linia %d: %d coloane (asteptat 5): %s"), i + 1, Parts.Num(), *Line);
			SkippedCount++;
			continue;
		}

		if (!Parts[2].IsNumeric() || !Parts[3].IsNumeric())
		{
			UE_LOG(LogTemp, Warning, TEXT("CountryPoints.csv linia %d: lat/lon non-numeric: %s"), i + 1, *Line);
			SkippedCount++;
			continue;
		}

		const FString Country = Parts[0];

		FCountryPoint Point;
		Point.CityName = Parts[1];
		Point.Lat = FCString::Atod(*Parts[2]);
		Point.Lon = FCString::Atod(*Parts[3]);
		Point.Alt = Parts[4].IsNumeric() ? FCString::Atod(*Parts[4]) : 0.0;

		if (Point.Lat < -90.0 || Point.Lat > 90.0 || Point.Lon < -180.0 || Point.Lon > 180.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("CountryPoints.csv linia %d: coordonate in afara intervalului: %s"), i + 1, *Line);
			SkippedCount++;
			continue;
		}

		CountryPoints.FindOrAdd(Country).Add(Point);
		AllCitiesFlat.Add(TPair<FString, FCountryPoint>(Country, Point));
		ParsedCount++;
	}

	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		FString::Printf(TEXT("Incarcate %d puncte pentru %d tari (%d sarite)."), ParsedCount, CountryPoints.Num(), SkippedCount));
}
/*void AOrbitalManager::LoadCountryPoints()
{
	CountryPoints.Empty();
	AllCitiesFlat.Empty(); // 1. OBLIGATORIU: Curatam si lista plata la fiecare reincarcare!
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("CountryPoints.csv"));
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red,
			AOrbitalManager::TrString("EROARE: Nu gasesc CountryPoints.csv!", "ERROR: CountryPoints.csv not found!"));
		return;
	}
	int32 ParsedCount = 0;
	for (int32 i = 0; i < Lines.Num(); i++)
	{
		if (i == 0) continue; // sarim header-ul: Country,City,Latitude,Longitude,Elevation_m
		FString Line = Lines[i];
		if (Line.Len() < 3) continue;
		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() < 5) continue;

		const FString& Country = Parts[0];

		FCountryPoint Point;
		//Point.CountryName = Country; // 2. OBLIGATORIU: Salveaza tara si in Point!
		Point.CityName = Parts[1];
		Point.Lat = FCString::Atod(*Parts[2]);
		Point.Lon = FCString::Atod(*Parts[3]);
		Point.Alt = FCString::Atod(*Parts[4]);

		CountryPoints.FindOrAdd(Country).Add(Point);
		AllCitiesFlat.Add(TPair<FString, FCountryPoint>(Country, Point));
		//AllCitiesFlat.Add(Point);
		ParsedCount++;
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
		FString::Printf(TEXT("Incarcate %d puncte pentru %d tari."), ParsedCount, CountryPoints.Num()));
}*/
/*void AOrbitalManager::LoadAndParseDatabase()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("Eclipse_de_soare.txt"));
	TArray<FString> Lines;
	AllEclipses.Empty();

	if (FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		UE_LOG(LogTemp, Display, TEXT("Fisier incarcat: %d linii. Incep parsarea..."), Lines.Num());
		int32 ParsedCount = 0;

		for (const FString& Line : Lines)
		{
			if (Line.Len() < 100) continue;

			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() < 46) continue;
			if (!Parts[0].Contains(TEXT("-"))) continue;

			FBesselianElements NewEclipse;
			NewEclipse.DateString = Parts[0];

			bool bIsNegativeYear = false;
			FString DateToParse = NewEclipse.DateString;

			// Verificăm doar Anul din text pentru filtrul principal
			if (DateToParse.StartsWith(TEXT("-")))
			{
				bIsNegativeYear = true;
				DateToParse = DateToParse.RightChop(1);
			}

			TArray<FString> DateParts;
			DateToParse.ParseIntoArray(DateParts, TEXT("-"), true);

			if (DateParts.Num() >= 3)
			{
				NewEclipse.Year = FCString::Atoi(*DateParts[0]);
				if (bIsNegativeYear) NewEclipse.Year = -NewEclipse.Year;

				// Filtrul de siguranță
				if (NewEclipse.Year < -13000 || NewEclipse.Year > 17000) continue;

				// Păstrăm datele din text provizoriu
				NewEclipse.Month = GetMonthFromStr(DateParts[1]);
				NewEclipse.Day = FCString::Atoi(*DateParts[2]);
			}
			else continue;

			NewEclipse.Saros = FCString::Atoi(*Parts[6]);

			// =================================================================
			// EXTRAGERE DATĂ ȘI ORĂ PERFECTĂ DIRECT DIN JULIAN DATE (JDTT)
			// =================================================================
			//double RawT0 = FCString::Atod(*Parts[10]); // Acesta este JD_TT (Julian Date Atomic)
			const double RawT0 = FCString::Atod(*Parts[10]); // JD_TT din coloana catalogului

			// 1. SALVĂM JULIAN DATE-UL EXACT PENTRU PYTHON
			NewEclipse.T0 = RawT0;

			if (NewEclipse.Year < 1)
			{
				// 1. Calculăm Delta T (Decalajul rotației Pământului)
				double t_centuries = (NewEclipse.Year - 1820.0) / 100.0;
				double DeltaT_sec = -20.0f + 32.0 * (t_centuries * t_centuries);
				double DeltaT_days = DeltaT_sec / 86400.0;

				// 2. Scădem zilele pentru a afla Timpul Universal (UT)
				double JD_UT = RawT0 - DeltaT_days;

				// 3. SHIFT DE 40.000 DE ANI (14.610.000 ZILE)
				// Facem asta ca să transformăm JD-ul negativ antic într-unul pozitiv, 
				// altfel funcțiile matematice (Floor) ne decalează complet zilele în C++.
				double Shifted_JD = JD_UT + 14610000.0;

				double Z_double = FMath::FloorToDouble(Shifted_JD + 0.5);
				double F = (Shifted_JD + 0.5) - Z_double; // Fracțiunea de zi (Ora exactă UT)

				int32 Z = (int32)Z_double;
				int32 A = Z; // Regula simplă pentru Calendarul Iulian
				int32 B = A + 1524;
				int32 C = FMath::FloorToInt((B - 122.1) / 365.25);
				int32 D_day = FMath::FloorToInt(365.25 * C);
				int32 E = FMath::FloorToInt((B - D_day) / 30.6001);

				// 4. Suprascriem data textului cu data exactă calculată (Corectată UT!)
				NewEclipse.Day = B - D_day - FMath::FloorToInt(30.6001 * E);
				NewEclipse.Month = (E < 14) ? (E - 1) : (E - 13);
				NewEclipse.Year = (NewEclipse.Month > 2) ? (C - 4716) : (C - 4715);

				// Readucem anul înapoi în antichitate
				NewEclipse.Year -= 40000;

				// 5. Salvăm ora corectată!
				//NewEclipse.T0 = F * 24.0;
				NewEclipse.Hour = (float)(F * 24.0);
			}
			else
			{
				// 1. Calculăm Delta T (Decalajul rotației Pământului)
				double DeltaT_sec = 0.0;
				if (NewEclipse.Year < 2150)
				{
					// Formula exactă pentru epoca modernă/apropiată
					double y = (double)NewEclipse.Year + ((double)NewEclipse.Month - 0.5) / 12.0;
					double t_centuries = (y - 2000.0) / 100.0;
					DeltaT_sec = 69.0 + 3.0 * (t_centuries * t_centuries);
				}
				else
				{
					// Formula pe termen lung (NASA/Meeus) pentru viitorul îndepărtat (ex: Anul 16.000)
					// Fără asta, calculele se decalează cu zile întregi!
					double t_centuries = (NewEclipse.Year - 1820.0) / 100.0;
					DeltaT_sec = -20.0 + 32.0 * (t_centuries * t_centuries);
				}

				double DeltaT_days = DeltaT_sec / 86400.0;
				double JD_UT = RawT0 - DeltaT_days;

				// 2. Extragem Ora EXACTĂ (T0)
				double Z_double = FMath::FloorToDouble(JD_UT + 0.5);
				double F = (JD_UT + 0.5) - Z_double; // Fracțiunea de zi
				NewEclipse.T0 = F * 24.0;

				// 3. RECALCULĂM DATA (An, Lună, Zi) 
				// Obligatoriu! Deoarece Delta T este de ~8 zile în anul 16.000, 
				// ziua din textul fișierului este greșită pentru Timpul Universal.
				int32 Z = (int32)Z_double;
				int32 A = Z;

				// Algoritmul pentru Calendarul Gregorian (aplicabil după 15 oct. 1582)
				// Baza NASA este Gregoriană pentru viitor.
				if (Z >= 2299161)
				{
					int32 alpha = FMath::FloorToInt((Z - 1867216.25) / 36524.25);
					A = Z + 1 + alpha - FMath::FloorToInt(alpha / 4.0);
				}

				int32 B = A + 1524;
				int32 C = FMath::FloorToInt((B - 122.1) / 365.25);
				int32 D_day = FMath::FloorToInt(365.25 * C);
				int32 E = FMath::FloorToInt((B - D_day) / 30.6001);

				// Suprascriem variabilele cu data astronomică perfect corectată
				NewEclipse.Day = B - D_day - FMath::FloorToInt(30.6001 * E);
				NewEclipse.Month = (E < 14) ? (E - 1) : (E - 13);
				NewEclipse.Year = (NewEclipse.Month > 2) ? (C - 4716) : (C - 4715);
			}
			// =================================================================

			// Restul parametrilor rămân perfect neatinși
			for (int i = 0; i < 4; i++) NewEclipse.x[i] = FCString::Atod(*Parts[11 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.y[i] = FCString::Atod(*Parts[15 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.z[i] = FCString::Atod(*Parts[19 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.d[i] = FCString::Atod(*Parts[23 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.mu[i] = FCString::Atod(*Parts[27 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l1[i] = FCString::Atod(*Parts[31 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l2[i] = FCString::Atod(*Parts[35 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf1[i] = FCString::Atod(*Parts[39 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf2[i] = FCString::Atod(*Parts[43 + i]);

			NewEclipse.bIsValid = true;
			AllEclipses.Add(NewEclipse);
			ParsedCount++;
		}

		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			FString::Printf(TEXT("SUCCESS: %d Eclipse incarcate complet (interval -13000 -> 17000)."), ParsedCount));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc fisierul!"));
	}
}*/

void AOrbitalManager::LoadAndParseDatabase()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("Eclipse_de_soare.txt"));
	TArray<FString> Lines;
	AllEclipses.Empty();

	if (FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		UE_LOG(LogTemp, Display, TEXT("Fisier incarcat: %d linii. Incep parsarea..."), Lines.Num());
		int32 ParsedCount = 0;

		for (const FString& Line : Lines)
		{
			if (Line.Len() < 100) continue;

			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() < 46) continue;
			if (!Parts[0].Contains(TEXT("-"))) continue;

			FBesselianElements NewEclipse;
			NewEclipse.DateString = Parts[0];

			bool bIsNegativeYear = false;
			FString DateToParse = NewEclipse.DateString;

			if (DateToParse.StartsWith(TEXT("-")))
			{
				bIsNegativeYear = true;
				DateToParse = DateToParse.RightChop(1);
			}

			TArray<FString> DateParts;
			DateToParse.ParseIntoArray(DateParts, TEXT("-"), true);

			if (DateParts.Num() >= 3)
			{
				NewEclipse.Year = FCString::Atoi(*DateParts[0]);
				if (bIsNegativeYear) NewEclipse.Year = -NewEclipse.Year;

				if (NewEclipse.Year < -13000 || NewEclipse.Year > 17000) continue;

				NewEclipse.Month = GetMonthFromStr(DateParts[1]);
				NewEclipse.Day = FCString::Atoi(*DateParts[2]);
			}
			else continue;

			NewEclipse.Saros = FCString::Atoi(*Parts[6]);

			// =================================================================
			// EXTRAGERE DATĂ ȘI ORĂ PERFECTĂ DIRECT DIN JULIAN DATE (JDTT)
			// =================================================================
			const double RawT0 = FCString::Atod(*Parts[10]); // JD_TT din coloana catalogului

			// SALVAM JULIAN DATE-UL EXACT (JDTT T0) PENTRU PYTHON
			// Aceasta valoare (-3026900... sau +2461265...) NU mai trebuie suprascrisa!
			NewEclipse.T0 = RawT0;

			if (NewEclipse.Year < 1)
			{
				// 1. Delta T Meeus
				const double t_centuries = (NewEclipse.Year - 1820.0) / 100.0;
				const double DeltaT_sec = -20.0f + 32.0 * (t_centuries * t_centuries);
				const double DeltaT_days = DeltaT_sec / 86400.0;

				// 2. Timpul Universal (UT)
				const double JD_UT = RawT0 - DeltaT_days;

				// 3. Shift 40.000 ani
				const double Shifted_JD = JD_UT + 14610000.0;

				const double Z_double = FMath::FloorToDouble(Shifted_JD + 0.5);
				const double F = (Shifted_JD + 0.5) - Z_double;

				const int32 Z = (int32)Z_double;
				const int32 A = Z;
				const int32 B = A + 1524;
				const int32 C = FMath::FloorToInt((B - 122.1) / 365.25);
				const int32 D_day = FMath::FloorToInt(365.25 * C);
				const int32 E = FMath::FloorToInt((B - D_day) / 30.6001);

				NewEclipse.Day = B - D_day - FMath::FloorToInt(30.6001 * E);
				NewEclipse.Month = (E < 14) ? (E - 1) : (E - 13);
				NewEclipse.Year = (NewEclipse.Month > 2) ? (C - 4716) : (C - 4715);
				NewEclipse.Year -= 40000;

				// Salvam ora in Hour, NU in T0!
				NewEclipse.Hour = (float)(F * 24.0);
			}
			else
			{
				double DeltaT_sec = 0.0;
				if (NewEclipse.Year < 2150)
				{
					const double y = (double)NewEclipse.Year + ((double)NewEclipse.Month - 0.5) / 12.0;
					const double t_centuries = (y - 2000.0) / 100.0;
					DeltaT_sec = 69.0 + 3.0 * (t_centuries * t_centuries);
				}
				else
				{
					const double t_centuries = (NewEclipse.Year - 1820.0) / 100.0;
					DeltaT_sec = -20.0 + 32.0 * (t_centuries * t_centuries);
				}

				const double DeltaT_days = DeltaT_sec / 86400.0;
				const double JD_UT = RawT0 - DeltaT_days;

				// Extragem Ora EXACTĂ în Hour, NU în T0!
				const double Z_double = FMath::FloorToDouble(JD_UT + 0.5);
				const double F = (JD_UT + 0.5) - Z_double;
				NewEclipse.Hour = (float)(F * 24.0); // <--- AICI ERA BUG-UL (era NewEclipse.T0 = ...)

				// Recalculare calendar Gregorian
				const int32 Z = (int32)Z_double;
				int32 A = Z;

				if (Z >= 2299161)
				{
					const int32 alpha = FMath::FloorToInt((Z - 1867216.25) / 36524.25);
					A = Z + 1 + alpha - FMath::FloorToInt(alpha / 4.0);
				}

				const int32 B = A + 1524;
				const int32 C = FMath::FloorToInt((B - 122.1) / 365.25);
				const int32 D_day = FMath::FloorToInt(365.25 * C);
				const int32 E = FMath::FloorToInt((B - D_day) / 30.6001);

				NewEclipse.Day = B - D_day - FMath::FloorToInt(30.6001 * E);
				NewEclipse.Month = (E < 14) ? (E - 1) : (E - 13);
				NewEclipse.Year = (NewEclipse.Month > 2) ? (C - 4716) : (C - 4715);
			}
			// =================================================================

			for (int i = 0; i < 4; i++) NewEclipse.x[i] = FCString::Atod(*Parts[11 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.y[i] = FCString::Atod(*Parts[15 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.z[i] = FCString::Atod(*Parts[19 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.d[i] = FCString::Atod(*Parts[23 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.mu[i] = FCString::Atod(*Parts[27 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l1[i] = FCString::Atod(*Parts[31 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.l2[i] = FCString::Atod(*Parts[35 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf1[i] = FCString::Atod(*Parts[39 + i]);
			for (int i = 0; i < 4; i++) NewEclipse.tanf2[i] = FCString::Atod(*Parts[43 + i]);

			NewEclipse.bIsValid = true;
			AllEclipses.Add(NewEclipse);
			ParsedCount++;
		}

		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			FString::Printf(TEXT("SUCCESS: %d Eclipse incarcate complet (interval -13000 -> 17000)."), ParsedCount));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc fisierul!"));
	}
}

bool AOrbitalManager::IsPlaceholder(const FString& Str)
{
	for (TCHAR C : Str)
	{
		if (FChar::IsDigit(C)) return false;
	}
	return true; // n-are nicio cifra -> e "-----" sau "---------"
}

//transform "14:26:57.680" in ore zecimale (14.449...)
double AOrbitalManager::ParseTimeToHours(const FString& TimeStr)
{
	TArray<FString> Parts;
	TimeStr.ParseIntoArray(Parts, TEXT(":"), true);
	if (Parts.Num() < 3) return 0.0;
	double H = FCString::Atod(*Parts[0]);
	double M = FCString::Atod(*Parts[1]);
	double S = FCString::Atod(*Parts[2]);
	return H + (M / 60.0) + (S / 3600.0);
}

//parsare baza de date pentru eclipsele de luna (catalogul SkyWare, format cu coloane separate, nu Julian Date brut ca la soare)
/*void AOrbitalManager::LoadAndParseLunarDatabase()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("Eclipse_de_luna.txt")); // schimba numele daca al tau difera
	TArray<FString> Lines;
	AllLunarEclipses.Empty();

	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc fisierul Eclipse_de_luna.txt!"));
		return;
	}

	int32 ParsedCount = 0;
	for (const FString& Line : Lines)
	{
		if (Line.Len() < 80) continue; // sarim header-ul si liniile de separator

		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 22) continue;
		if (!Parts[0].Contains(TEXT("-"))) continue; // trebuie sa fie o data valida (an-luna-zi)

		FLunarEclipseElements NewEclipse;
		NewEclipse.DateString = Parts[0];

		// =================================================================
		// Parsare data, exact ca la eclipsele de soare (suport ani negativi, Î.Hr.)
		// =================================================================
		bool bIsNegativeYear = false;
		FString DateToParse = NewEclipse.DateString;
		if (DateToParse.StartsWith(TEXT("-")))
		{
			bIsNegativeYear = true;
			DateToParse = DateToParse.RightChop(1);
		}

		TArray<FString> DateParts;
		DateToParse.ParseIntoArray(DateParts, TEXT("-"), true);
		if (DateParts.Num() < 3) continue;

		NewEclipse.Year = FCString::Atoi(*DateParts[0]);
		if (bIsNegativeYear) NewEclipse.Year = -NewEclipse.Year;
		if (NewEclipse.Year < -13000 || NewEclipse.Year > 17000) continue;

		NewEclipse.Month = GetMonthFromStr(DateParts[1]);
		NewEclipse.Day = FCString::Atoi(*DateParts[2]);

		// Ora TT a eclipsei maxime - vine deja gata calculata in catalog, nu mai trebuie sa o extragem dintr-un JD
		NewEclipse.T0 = ParseTimeToHours(Parts[1]);

		// Tip eclipsa
		NewEclipse.TypeCode = Parts[2];
		NewEclipse.TypeNumber = FCString::Atoi(*Parts[3]);

		NewEclipse.Gamma = FCString::Atod(*Parts[4]);
		NewEclipse.Pmag = FCString::Atod(*Parts[5]);
		NewEclipse.Umag = FCString::Atod(*Parts[6]);

		NewEclipse.Lunation = FCString::Atoi(*Parts[7]);
		NewEclipse.Saros = FCString::Atoi(*Parts[8]);
		NewEclipse.Inex = FCString::Atoi(*Parts[9]);

		NewEclipse.Dur_P4P1 = IsPlaceholder(Parts[10]) ? -1.0 : FCString::Atod(*Parts[10]);
		NewEclipse.Dur_U4U1 = IsPlaceholder(Parts[11]) ? -1.0 : FCString::Atod(*Parts[11]);
		NewEclipse.Dur_U3U2 = IsPlaceholder(Parts[12]) ? -1.0 : FCString::Atod(*Parts[12]);

		NewEclipse.P1 = IsPlaceholder(Parts[13]) ? -999.0 : FCString::Atod(*Parts[13]);
		NewEclipse.U1 = IsPlaceholder(Parts[14]) ? -999.0 : FCString::Atod(*Parts[14]);
		NewEclipse.U2 = IsPlaceholder(Parts[15]) ? -999.0 : FCString::Atod(*Parts[15]);
		NewEclipse.U3 = IsPlaceholder(Parts[16]) ? -999.0 : FCString::Atod(*Parts[16]);
		NewEclipse.U4 = IsPlaceholder(Parts[17]) ? -999.0 : FCString::Atod(*Parts[17]);
		NewEclipse.P4 = IsPlaceholder(Parts[18]) ? -999.0 : FCString::Atod(*Parts[18]);

		NewEclipse.eLngW_GE = FCString::Atod(*Parts[19]);
		NewEclipse.Lat_GE = FCString::Atod(*Parts[20]);
		NewEclipse.ELT = FCString::Atod(*Parts[21]);

		NewEclipse.bIsValid = true;
		AllLunarEclipses.Add(NewEclipse);
		ParsedCount++;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			FString::Printf(TEXT("SUCCESS: %d Eclipse de Luna incarcate (interval -13000 -> 17000)."), ParsedCount));
	}
}*/
void AOrbitalManager::LoadAndParseLunarDatabase()
{
	FString ContentDir = FPaths::ProjectContentDir();
	FString FilePath = FPaths::Combine(ContentDir, TEXT("Encoder"), TEXT("Eclipse_de_luna.txt"));
	TArray<FString> Lines;
	AllLunarEclipses.Empty();

	if (!FFileHelper::LoadFileToStringArray(Lines, *FilePath))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Nu gasesc fisierul Eclipse_de_luna.txt!"));
		return;
	}

	// ------------------------------------------------------------------
	// Delta T exact, identic cu delta_t_seconds() din Server.py (4 ramuri).
	// y_frac = An + (Luna - 0.5)/12, exact ca in Python si ca in Meeus.
	// ------------------------------------------------------------------
	auto DeltaTSeconds = [](double y_frac) -> double
		{
			if (y_frac < -500.0)
			{
				const double u = (y_frac - 1820.0) / 100.0;
				return -20.0 + 32.0 * u * u;
			}
			else if (y_frac < 500.0)
			{
				const double u = y_frac / 100.0;
				return 10583.6 - 1014.41 * u + 33.78311 * FMath::Pow(u, 2) - 5.952053 * FMath::Pow(u, 3)
					- 0.1798452 * FMath::Pow(u, 4) + 0.022174192 * FMath::Pow(u, 5) + 0.0090316521 * FMath::Pow(u, 6);
			}
			else if (y_frac < 1600.0)
			{
				const double u = (y_frac - 1000.0) / 100.0;
				return 1574.2 - 556.01 * u + 71.23472 * FMath::Pow(u, 2) + 0.319781 * FMath::Pow(u, 3)
					- 0.8503463 * FMath::Pow(u, 4) - 0.005050998 * FMath::Pow(u, 5) + 0.0083572073 * FMath::Pow(u, 6);
			}
			else
			{
				const double u = (y_frac - 1820.0) / 100.0;
				return -20.0 + 32.0 * u * u;
			}
		};

	// ------------------------------------------------------------------
	// Data calendaristica (An, Luna, Zi, Ora zecimala) -> Julian Date.
	// Granita Iulian/Gregorian e verificata pe DATA REALA (15 oct. 1582),
	// nu doar pe semnul anului - asa cum specifica header-ul catalogului NASA.
	// ------------------------------------------------------------------
	auto DateToJD = [](int32 Year, int32 Month, int32 Day, double Hour) -> double
		{
			int32 y = Year, m = Month;
			if (m <= 2) { y -= 1; m += 12; }

			const bool bIsGregorian = (Year > 1582) || (Year == 1582 && (Month > 10 || (Month == 10 && Day >= 15)));
			double B = 0.0;
			if (bIsGregorian)
			{
				const int32 a = FMath::FloorToInt((double)y / 100.0);
				B = 2 - a + FMath::FloorToInt((double)a / 4.0);
			}

			return FMath::Floor(365.25 * (y + 4716)) + FMath::Floor(30.6001 * (m + 1)) + Day + B - 1524.5 + (Hour / 24.0);
		};

	// ------------------------------------------------------------------
	// Julian Date -> Data calendaristica (An, Luna, Zi, Ora zecimala), algoritm Meeus.
	// Shift-ul de 40000 de ani se declanseaza pe baza SEMNULUI REAL al JD
	// (nu pe baza anului brut TT), ca sa fie corect chiar si cand Delta T
	// impinge data peste granita anului 0/1. Cand e activ shift-ul, data e
	// intotdeauna considerata Iuliana (asa cum sunt toate datele din antichitate
	// in conventia catalogului), fara corectia alpha gregoriana.
	// ------------------------------------------------------------------
	auto JDToDate = [](double JD, int32& OutYear, int32& OutMonth, int32& OutDay, double& OutHour)
		{
			const bool bNeedsShift = (JD + 0.5) < 0.0;
			const double ShiftDays = 14610000.0; // 40000 ani astronomici (40000 * 365.25, exact)
			const double WorkingJD = bNeedsShift ? (JD + ShiftDays) : JD;

			const double Z_double = FMath::FloorToDouble(WorkingJD + 0.5);
			const double F = (WorkingJD + 0.5) - Z_double;
			const int32 Z = (int32)Z_double;

			int32 A = Z;
			if (!bNeedsShift && Z >= 2299161)
			{
				const int32 alpha = FMath::FloorToInt((Z - 1867216.25) / 36524.25);
				A = Z + 1 + alpha - FMath::FloorToInt(alpha / 4.0);
			}

			const int32 B = A + 1524;
			const int32 C = FMath::FloorToInt((B - 122.1) / 365.25);
			const int32 D_day = FMath::FloorToInt(365.25 * C);
			const int32 E = FMath::FloorToInt((B - D_day) / 30.6001);

			OutDay = B - D_day - FMath::FloorToInt(30.6001 * E);
			OutMonth = (E < 14) ? (E - 1) : (E - 13);
			OutYear = (OutMonth > 2) ? (C - 4716) : (C - 4715);
			if (bNeedsShift) OutYear -= 40000;

			OutHour = F * 24.0;
		};

	int32 ParsedCount = 0;
	for (const FString& Line : Lines)
	{
		if (Line.Len() < 80) continue;

		TArray<FString> Parts;
		Line.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 22) continue;
		if (!Parts[0].Contains(TEXT("-"))) continue;

		FLunarEclipseElements NewEclipse;
		NewEclipse.DateString = Parts[0];

		bool bIsNegativeYear = false;
		FString DateToParse = NewEclipse.DateString;
		if (DateToParse.StartsWith(TEXT("-")))
		{
			bIsNegativeYear = true;
			DateToParse = DateToParse.RightChop(1);
		}

		TArray<FString> DateParts;
		DateToParse.ParseIntoArray(DateParts, TEXT("-"), true);
		if (DateParts.Num() < 3) continue;

		int32 RawYear = FCString::Atoi(*DateParts[0]);
		if (bIsNegativeYear) RawYear = -RawYear;
		if (RawYear < -13000 || RawYear > 17000) continue;

		const int32 RawMonth = GetMonthFromStr(DateParts[1]);
		const int32 RawDay = FCString::Atoi(*DateParts[2]);
		const double TT_Hours = ParseTimeToHours(Parts[1]);

		// 1. JD_TT exact, respectand granita gregoriana reala a catalogului
		const double JD_TT = DateToJD(RawYear, RawMonth, RawDay, TT_Hours);

		// 2. Delta T exact (aceeasi formula ca Server.py), cu anul fractionat
		const double y_frac = (double)RawYear + ((double)RawMonth - 0.5) / 12.0;
		const double DeltaT_sec = DeltaTSeconds(y_frac);
		const double DeltaT_days = DeltaT_sec / 86400.0;
		const double DeltaT_hours = DeltaT_sec / 3600.0;
		const double JD_UT = JD_TT - DeltaT_days;

		// 3. Reconstructie exacta a datei/orei UT din JD_UT
		int32 FinalYear, FinalMonth, FinalDay;
		double FinalHour;
		JDToDate(JD_UT, FinalYear, FinalMonth, FinalDay, FinalHour);
		NewEclipse.Year = FinalYear;
		NewEclipse.Month = FinalMonth;
		NewEclipse.Day = FinalDay;
		//NewEclipse.T0 = (float)FinalHour;
		NewEclipse.T0 = JD_TT;

		// 4. Corectie geografica longitudine pentru rotatia Pamantului (Delta T)
		double RawLngW = FCString::Atod(*Parts[19]);
		double EarthRotationSpeed = 360.985647 / 24.0;
		double AdjustedLngW = RawLngW - (DeltaT_hours * EarthRotationSpeed);
		while (AdjustedLngW < 0.0) AdjustedLngW += 360.0;
		while (AdjustedLngW >= 360.0) AdjustedLngW -= 360.0;
		NewEclipse.eLngW_GE = AdjustedLngW;
		NewEclipse.Lat_GE = FCString::Atod(*Parts[20]);

		// Restul coloanelor specifice eclipsei de luna
		NewEclipse.TypeCode = Parts[2];
		NewEclipse.TypeNumber = FCString::Atoi(*Parts[3]);
		NewEclipse.Gamma = FCString::Atod(*Parts[4]);
		NewEclipse.Pmag = FCString::Atod(*Parts[5]);
		NewEclipse.Umag = FCString::Atod(*Parts[6]);
		NewEclipse.Lunation = FCString::Atoi(*Parts[7]);
		NewEclipse.Saros = FCString::Atoi(*Parts[8]);
		NewEclipse.Inex = FCString::Atoi(*Parts[9]);
		NewEclipse.Dur_P4P1 = IsPlaceholder(Parts[10]) ? -1.0 : FCString::Atod(*Parts[10]);
		NewEclipse.Dur_U4U1 = IsPlaceholder(Parts[11]) ? -1.0 : FCString::Atod(*Parts[11]);
		NewEclipse.Dur_U3U2 = IsPlaceholder(Parts[12]) ? -1.0 : FCString::Atod(*Parts[12]);
		NewEclipse.P1 = IsPlaceholder(Parts[13]) ? -999.0 : FCString::Atod(*Parts[13]);
		NewEclipse.U1 = IsPlaceholder(Parts[14]) ? -999.0 : FCString::Atod(*Parts[14]);
		NewEclipse.U2 = IsPlaceholder(Parts[15]) ? -999.0 : FCString::Atod(*Parts[15]);
		NewEclipse.U3 = IsPlaceholder(Parts[16]) ? -999.0 : FCString::Atod(*Parts[16]);
		NewEclipse.U4 = IsPlaceholder(Parts[17]) ? -999.0 : FCString::Atod(*Parts[17]);
		NewEclipse.P4 = IsPlaceholder(Parts[18]) ? -999.0 : FCString::Atod(*Parts[18]);
		NewEclipse.ELT = FCString::Atod(*Parts[21]);

		NewEclipse.bIsValid = true;
		AllLunarEclipses.Add(NewEclipse);
		ParsedCount++;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
			FString::Printf(TEXT("SUCCESS: %d Eclipse de Luna incarcate cu Delta T si rotatie pamanteana corectate."), ParsedCount));
	}
}

//extragere data juliana in functie de data curenta
double AOrbitalManager::GetJulianDate(int32 Year, int32 Month, int32 Day, double Hour)
{
	if (Month <= 2) {
		Year -= 1;
		Month += 12;
	}
	int32 A = Year / 100;
	int32 B = 2 - A + (A / 4);
	double JD = floor(365.25 * (Year + 4716)) + floor(30.6001 * (Month + 1)) + Day + B - 1524.5;
	JD += Hour / 24.0;
	return JD;
}
//normalizare grade
double NormalizeDegrees(double Angle)
{
	Angle = fmod(Angle, 360.0);
	if (Angle < 0) Angle += 360.0;
	return Angle;
}
//aici este tickul, este o parte importanta
void AOrbitalManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC && PC->PlayerCameraManager)
	{
		float CurrentFOV = PC->PlayerCameraManager->GetFOVAngle();
		if (FMath::Abs(CurrentFOV - LastFOV) > 0.05f)
		{
			LastFOV = CurrentFOV;
			//desenare constelatii (liniile care leaga stelele)
			DrawConstellations(); 
		}
	}
	if (FMath::Abs(HighResHour - (double)CurrentDate.Hour) > 0.5)
	{
		HighResHour = (double)CurrentDate.Hour;
	}
	if (TimeSpeed != 0.0f)
	{
		// 1. Calculam pasul de timp
		double HourStep = ((double)DeltaTime * (double)TimeSpeed) / 3600.0;
		double NextHour = HighResHour + HourStep;
		// 2. VERIFICARE LIMITA SUPERIOARA (An 9999)
		if (TimeSpeed > 0 && CurrentDate.Year >= 9999 && CurrentDate.Month >= 12 && CurrentDate.Day >= 31 && NextHour >= 23.999)
		{
			HighResHour = 23.999;
			CurrentDate.Hour = 23.999f;
			TimeSpeed = 0.0f; // Oprim simularea la "capatul timpului"
		}
		// 3. VERIFICARE LIMITA INFERIOARA (An 1)
		else if (TimeSpeed < 0 && CurrentDate.Year <= 1 && CurrentDate.Month <= 1 && CurrentDate.Day <= 1 && NextHour <= 0.0)
		{
			HighResHour = 0.0;
			CurrentDate.Hour = 0.0f;
			TimeSpeed = 0.0f; // Oprim simularea la "inceputul timpului"
		}
		else
		{
			// Daca suntem in limite, aplicam pasul
			HighResHour = NextHour;
		}
		CurrentDate.Hour = (float)HighResHour;
		// 4. LOGICA TRANZITIE ZILE (Inainte)
		if (HighResHour >= 24.0)
		{
			HighResHour -= 24.0;
			CurrentDate.Hour = (float)HighResHour;
			CurrentDate.Day++;
			CurrentDate.DayOfYear++;
			if (CurrentDate.Day > GetDaysInMonth(CurrentDate.Month, CurrentDate.Year))
			{
				CurrentDate.Day = 1;
				CurrentDate.Month++;
				if (CurrentDate.Month > 12)
				{
					CurrentDate.Month = 1;
					CurrentDate.Year++;
					CurrentDate.DayOfYear = 1;
				}
			}
		}
		// 5. LOGICA TRANZITIE ZILE (Inapoi) - Lipsea din codul tau!
		else if (HighResHour < 0.0)
		{
			HighResHour += 24.0;
			CurrentDate.Hour = (float)HighResHour;
			CurrentDate.Day--;
			CurrentDate.DayOfYear--;
			if (CurrentDate.Day < 1)
			{
				CurrentDate.Month--;
				if (CurrentDate.Month < 1)
				{
					CurrentDate.Month = 12;
					CurrentDate.Year--;
				}
				CurrentDate.Day = GetDaysInMonth(CurrentDate.Month, CurrentDate.Year);
				// Re-calculare DayOfYear daca e necesar pentru logica ta de stele
			}
		}
	}
	//trimit si primesc date de la python prin socketul udp, port 5000.
	//SendDataToPython();
	//ReceiveDataFromPython();

	TimeSinceLastUDPSend += DeltaTime;
	if (TimeSinceLastUDPSend >= UDPSendInterval)
	{
		TimeSinceLastUDPSend = 0.0f;
		SendDataToPython();
	}
	ReceiveDataFromPython();
	return;
}
double AOrbitalManager::EvaluatePoly(double t, const double* c) {
	return c[0] + t * (c[1] + t * (c[2] + t * c[3]));
}
//aici este algoritmul besselian, folosit inainte sa am backendul de python
void AOrbitalManager::CalculateBesselianPositions(const FBesselianElements& Elem, FVector& OutSunPos, FVector& OutMoonPos)
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;
	double CurrentJD_UTC = GetJulianDate(CurrentDate.Year, CurrentDate.Month, CurrentDate.Day, CurrentDate.Hour);
	double DeltaT_Sec = 72.0;
	double DeltaT_Days = DeltaT_Sec / 86400.0;
	double CurrentJD_TT = CurrentJD_UTC + DeltaT_Days;
	double EclipseRefJD = GetJulianDate(Elem.Year, Elem.Month, Elem.Day, Elem.T0);
	double t = (CurrentJD_TT - EclipseRefJD) * 24.0;
	double x = EvaluatePoly(t, Elem.x);
	double y = EvaluatePoly(t, Elem.y);
	double z = EvaluatePoly(t, Elem.z); 
	double d_deg = EvaluatePoly(t, Elem.d);   
	double mu_deg = EvaluatePoly(t, Elem.mu); 
	double d_rad = FMath::DegreesToRadians(d_deg);
	double mu_rad = FMath::DegreesToRadians(mu_deg);
	double sin_mu = sin(mu_rad);
	double cos_mu = cos(mu_rad);
	double sin_d = sin(d_rad);
	double cos_d = cos(d_rad);
	//calculez coordonatele ecef pentru soare si luna, aveam o acuratete proasta la eclipse, de aceea am ales backendul de python
	double MoonX_ECEF = -sin_mu * x - (sin_d * cos_mu) * y + (cos_d * cos_mu) * z;
	double MoonY_ECEF = cos_mu * x - (sin_d * sin_mu) * y + (cos_d * sin_mu) * z;
	double MoonZ_ECEF = cos_d * y + sin_d * z;
	double EarthRadiusCm = 6378137.0;
	FVector MoonPosECEF(MoonX_ECEF, MoonY_ECEF, MoonZ_ECEF);
	MoonPosECEF *= EarthRadiusCm;
	double SunX_ECEF = cos_d * cos_mu;
	double SunY_ECEF = cos_d * sin_mu;
	double SunZ_ECEF = sin_d;
	FVector SunDirECEF(SunX_ECEF, SunY_ECEF, SunZ_ECEF);
	//functii unreal pt a calcula pozitia soarelui si a lunii din coordonate ecef in coordonatele motorului grafic
	OutSunPos = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(SunDirECEF);
	FVector EarthCenterUnreal = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
	FVector MoonVecUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(MoonPosECEF.GetSafeNormal());
	OutMoonPos = EarthCenterUnreal + (MoonVecUnreal * MoonPosECEF.Size());
}
//actualizare orbite pe masura ce trece timpul
//nu folosesc aceasta functie, din cauza faptului ca avea acuratete foarte slaba, imi gresea pozitiile cam cu jumatate de grad, care e cat diametrul aparent al Lunii
//deci practic nu puteam vedea eclipse
void AOrbitalManager::UpdateOrbitals(float DeltaTime)
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!GeoRef || !PlayerPawn) return;
	double JD_UTC = GetJulianDate(CurrentDate.Year, CurrentDate.Month, CurrentDate.Day, CurrentDate.Hour);
	double DeltaT_Days = 70.0 / 86400.0;
	double JD_TT = JD_UTC + DeltaT_Days;
	double T = (JD_UTC - 2451545.0) / 36525.0;
	FVector PreciseSunDir = FVector::ZeroVector;
	FVector PreciseMoonPos = FVector::ZeroVector;
	if (bUseDatabasePrecision && CurrentEclipseData.bIsValid)
	{
		double EclipseRefJD = GetJulianDate(CurrentEclipseData.Year, CurrentEclipseData.Month, CurrentEclipseData.Day, CurrentEclipseData.T0);
		double TimeDiffDays = JD_TT - EclipseRefJD;
		double TimeDiffHours = TimeDiffDays * 24.0;
		if (FMath::Abs(TimeDiffHours) < 6.0)
		{
			CalculateBesselianPositions(CurrentEclipseData, PreciseSunDir, PreciseMoonPos);
		}
	}
	if (bEclipseModeActive)
	{
		if (SunSkyActor)
		{
			UDirectionalLightComponent* LightComp = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
			if (LightComp)
			{
				FVector LightDir = -PreciseSunDir;
				LightComp->SetWorldRotation(LightDir.Rotation());
				LightComp->SetLightSourceAngle(0.0f);
				LightComp->SetLightSourceSoftAngle(0.0f);
				LightComp->SetCastShadows(true);
			}
		}
		if (ControlledMoon)
		{
			ControlledMoon->SetActorLocation(PreciseMoonPos);
			FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
			FVector LookAtDir = EarthCenter - PreciseMoonPos;
			ControlledMoon->SetActorRotation(LookAtDir.Rotation());
		}
	}
	else
	{
		//formule de calculat pozitia soarelui pe cer in coordonate ecef
		double L0 = NormalizeDegrees(280.46646 + 36000.76983 * T);
		double M_Sun = NormalizeDegrees(357.52911 + 35999.05029 * T);
		double Lambda = L0 + 1.9146 * sin(FMath::DegreesToRadians(M_Sun)) + 0.0200 * sin(FMath::DegreesToRadians(2 * M_Sun));
		double Obliquity = 23.439 - 0.013 * T;
		double Rad_Obl = FMath::DegreesToRadians(Obliquity);
		double Rad_Lam = FMath::DegreesToRadians(Lambda);
		double Alpha = FMath::RadiansToDegrees(atan2(cos(Rad_Obl) * sin(Rad_Lam), cos(Rad_Lam)));
		double Delta = FMath::RadiansToDegrees(asin(sin(Rad_Obl) * sin(Rad_Lam)));
		double GMST = NormalizeDegrees(280.460618 + 360.985647 * (JD_UTC - 2451545.0));
		double HourAngleRad = FMath::DegreesToRadians(Alpha - GMST);
		double Dec_Rad = FMath::DegreesToRadians(Delta);
		FVector SunECEF(
			cos(Dec_Rad) * cos(HourAngleRad),
			cos(Dec_Rad) * sin(HourAngleRad),
			sin(Dec_Rad)
		);
		if (SunSkyActor)
		{
			FVector UnrealSunDir = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(SunECEF.GetSafeNormal());
			UDirectionalLightComponent* LightComp = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
			if (LightComp) LightComp->SetWorldRotation((-UnrealSunDir).Rotation());
		}
		//aceeasi poveste si pentru luna
		if (ControlledMoon)
		{
			double L_Moon = NormalizeDegrees(218.316 + 13.176396 * (JD_UTC - 2451545.0));
			double M_Moon = NormalizeDegrees(134.963 + 13.064993 * (JD_UTC - 2451545.0));
			double F_Moon = NormalizeDegrees(93.272 + 13.229350 * (JD_UTC - 2451545.0));
			double MoonLon = L_Moon + 6.289 * sin(FMath::DegreesToRadians(M_Moon));
			double MoonLat = 5.128 * sin(FMath::DegreesToRadians(F_Moon));
			double LonRad = FMath::DegreesToRadians(MoonLon);
			double LatRad = FMath::DegreesToRadians(MoonLat);
			double GMST_Rad = FMath::DegreesToRadians(GMST);
			double x_eq = cos(LatRad) * cos(LonRad);
			double y_eq = cos(LatRad) * sin(LonRad) * cos(Rad_Obl) - sin(LatRad) * sin(Rad_Obl);
			double z_eq = cos(LatRad) * sin(LonRad) * sin(Rad_Obl) + sin(LatRad) * cos(Rad_Obl);
			double X_ECEF = 384400000.0 * (x_eq * cos(GMST_Rad) + y_eq * sin(GMST_Rad));
			double Y_ECEF = 384400000.0 * (-x_eq * sin(GMST_Rad) + y_eq * cos(GMST_Rad));
			double Z_ECEF = 384400000.0 * z_eq;
			FVector MoonPos_Geo(X_ECEF, Y_ECEF, Z_ECEF);
			FVector PlayerECEF = GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPawn->GetActorLocation());
			FVector MoonPos_Topo = MoonPos_Geo - PlayerECEF;
			FVector UnrealMoonDir = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(MoonPos_Topo.GetSafeNormal());
			FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
			ControlledMoon->SetActorLocation(EarthCenter + (UnrealMoonDir * 384400000.0));
			ControlledMoon->SetActorRotation((EarthCenter - ControlledMoon->GetActorLocation()).Rotation());
		}
		GEngine->AddOnScreenDebugMessage(98, 0.f, FColor::Yellow, TEXT("MOD: ORBITA STANDARD (Low Precision)"));
	}
}
//aici sunt datele pe care le trimit hudului sa le afisez (inclusiv date utc si timpul solar)
FString AOrbitalManager::GetDateForHUD()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!PlayerPawn || !GeoRef) return TEXT("Initializare...");
	double RawUtc = (double)CurrentDate.Hour;
	int32 UtcH = (int32)RawUtc;                  
	double RemMinUtc = (RawUtc - UtcH) * 60.0;   
	int32 UtcM = (int32)RemMinUtc;               
	double RemSecUtc = (RemMinUtc - UtcM) * 60.0;
	int32 UtcS = (int32)RemSecUtc;               
	int32 UtcMS = (int32)((RemSecUtc - UtcS) * 100.0);
	FVector UnrealLoc = PlayerPawn->GetActorLocation();
	//pun coordonate, calculez timpul de offset (360/24 = 15 grade pentru fiecare ora)
	FVector Coords = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(UnrealLoc);
	double Longitude = Coords.X;
	double TimeOffset = Longitude / 15.0;
	double LocalTimeDouble = RawUtc + TimeOffset;
	if (LocalTimeDouble >= 24.0) LocalTimeDouble -= 24.0;
	if (LocalTimeDouble < 0.0) LocalTimeDouble += 24.0;
	int32 LocH = (int32)LocalTimeDouble;
	double RemMinLoc = (LocalTimeDouble - LocH) * 60.0;
	int32 LocM = (int32)RemMinLoc;
	double RemSecLoc = (RemMinLoc - LocM) * 60.0;
	int32 LocS = (int32)RemSecLoc;
	int32 LocMS = (int32)((RemSecLoc - LocS) * 100.0);
	return FString::Printf(TEXT("Data: %02d/%02d/%d\nUTC:   %02d:%02d:%02d.%02d\nLocal: %02d:%02d:%02d.%02d"),
		CurrentDate.Day, CurrentDate.Month, CurrentDate.Year,
		UtcH, UtcM, UtcS, UtcMS,
		LocH, LocM, LocS, LocMS);
}
//aceasta functie este utila pentru a vedea fiecare luna cate zile are
int32 AOrbitalManager::GetDaysInMonth(int32 M, int32 Y)
{
	if (M == 2) return (Y % 4 == 0) ? 29 : 28;
	if (M == 4 || M == 6 || M == 9 || M == 11) return 30;
	return 31;
}
//aici reduc viteza de rulare la jumatate 
void AOrbitalManager::DecreaseTimeSpeed()
{
	if (TimeSpeed > 0.0625f)
	{
		TimeSpeed /= 2.0f;
	}
	else
	{
		TimeSpeed = 0.0625f;
	}
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Time Speed: %.1f x"), TimeSpeed));
}
//marire viteza
void AOrbitalManager::IncreaseTimeSpeed()
{
	if (TimeSpeed == 0.0f)
	{
		TimeSpeed = 1.0f;
	}
	else
	{
		// Inmultesc cu 2, dar plafonez viteza maxima la 86400 (o zi pe secunda reala)
		TimeSpeed = FMath::Clamp(TimeSpeed * 2.0f, 0.0f, 512288.0f);
	}
	GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Time Speed: %.1f x"), TimeSpeed));
}
//aici schimb daca dau pauza sau play
void AOrbitalManager::TogglePause()
{
	if (TimeSpeed > 0.0f)
	{
		SavedTimeSpeed = TimeSpeed; 
		TimeSpeed = 0.0f;           
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("TIMP: PAUZA"));
	}
	else
	{
		if (SavedTimeSpeed <= 0.0f) SavedTimeSpeed = 1.0f; 
		TimeSpeed = 0.125f;
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("TIMP: REPORNIT"));
	}
}
//functie pe care nu o mai folosesc, era utilizata pe tasta l (mapata)
void AOrbitalManager::ToggleMoonTravel()
{
	if (!ControlledMoon) return;
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn) return;
	if (!bIsOnMoon)
	{
		SavedEarthLocation = PlayerPawn->GetActorLocation();
		SavedEarthRotation = PlayerPawn->GetActorRotation();
		FVector MoonCenter = ControlledMoon->GetActorLocation();
		FVector LandingOffset = FVector(0, 0, 20000.0f); 
		PlayerPawn->SetActorLocation(MoonCenter + LandingOffset);
		PlayerPawn->AttachToActor(ControlledMoon, FAttachmentTransformRules::KeepWorldTransform);
		FVector DirToEarth = (FVector::ZeroVector - PlayerPawn->GetActorLocation()).GetSafeNormal();
		PlayerPawn->SetActorRotation(DirToEarth.Rotation());
		bIsOnMoon = true;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("TRAVEL: WELCOME TO THE MOON"));
	}
	else
	{
		PlayerPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		PlayerPawn->SetActorLocation(SavedEarthLocation);
		PlayerPawn->SetActorRotation(SavedEarthRotation);
		bIsOnMoon = false;
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("TRAVEL: WELCOME BACK TO EARTH"));
	}
}
//functie foarte utila, in care pot sa fac sarituri in timp, ma pot duce la orice zi si data vreau
void AOrbitalManager::SetSimulationDateTime(int32 Day, int32 Month, int32 Year, float Hour)
{
	CurrentDate.Year = Year;
	CurrentDate.Month = FMath::Clamp(Month, 1, 12);
	int32 MaxDays = GetDaysInMonth(CurrentDate.Month, CurrentDate.Year);
	CurrentDate.Day = FMath::Clamp(Day, 1, MaxDays);
	CurrentDate.Hour = FMath::Clamp(Hour, 0.0f, 23.99f);
	int32 CalculatedDayOfYear = 0;
	for (int32 m = 1; m < CurrentDate.Month; m++)
	{
		CalculatedDayOfYear += GetDaysInMonth(m, CurrentDate.Year);
	}
	CalculatedDayOfYear += CurrentDate.Day;
	CurrentDate.DayOfYear = CalculatedDayOfYear;
	CurrentEclipseData.bIsValid = false;
	HighResHour = (double)CurrentDate.Hour;
	//ma uit sa vad daca ziua aleasa se potriveste cu vreo eclipsa
	for (const FBesselianElements& Elem : AllEclipses)
	{
		if (Elem.Year == CurrentDate.Year &&
			Elem.Month == CurrentDate.Month &&
			Elem.Day == CurrentDate.Day)
		{
			CurrentEclipseData = Elem;
			CurrentEclipseData.bIsValid = true;
			GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green,
				FString::Printf(TEXT("ECLIPSA ACTIVATA: %s (T0: %.2f)"), *Elem.DateString, Elem.T0));
			break;
		}
	}
	//afisez pe ecran ca am sarit la data respectiva
	UpdateOrbitals(0.0f);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Magenta,
		FString::Printf(TEXT("JUMP TO: %02d/%02d/%d - Ora %.2f"),
			(int32)CurrentDate.Day,
			(int32)CurrentDate.Month,
			(int32)CurrentDate.Year,
			(float)CurrentDate.Hour));
}
//aici sunt eclipse de test pe care imi doream sa le vad in simulator inainte de a avea meniul de eclipse
//Spania, 12 august 2026
void AOrbitalManager::JumpToEclipse()
{
	SetSimulationDateTime(12, 8, 2026, 17.00f);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("!!! JUMP TO ECLIPSE !!!"));
}
//Luxor, 2 august 2027
void AOrbitalManager::JumpToEclipse2()
{
	SetSimulationDateTime(2, 8, 2027, 9.0f);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("!!! JUMP TO ECLIPSE !!!"));
}
//in aceste functii am incercat sa ajustez de mana offsetul lunii pentru a se potrivi peste soare (o idee buna pentru stadiul incipient al lucrarii)
void AOrbitalManager::AdjustMoonOffsetPlus()
{
	MoonPhaseOffset += 0.01f; 
	UpdateOrbitals(0.0f);    
	GEngine->AddOnScreenDebugMessage(55, 1.f, FColor::Yellow, FString::Printf(TEXT("Moon Offset: %.2f"), MoonPhaseOffset));
}
void AOrbitalManager::AdjustMoonOffsetMinus()
{
	MoonPhaseOffset -= 0.01f;
	UpdateOrbitals(0.0f);
	GEngine->AddOnScreenDebugMessage(55, 1.f, FColor::Yellow, FString::Printf(TEXT("Moon Offset: %.2f"), MoonPhaseOffset));
}
void AOrbitalManager::AdjustInclinationUp()
{
	MoonInclinationOffset += 0.01f; 
	UpdateOrbitals(0.0f);
	GEngine->AddOnScreenDebugMessage(56, 2.f, FColor::Cyan, FString::Printf(TEXT("Inclinatie Offset: %.2f"), MoonInclinationOffset));
}
void AOrbitalManager::AdjustInclinationDown()
{
	MoonInclinationOffset -= 0.01f;
	UpdateOrbitals(0.0f);
	GEngine->AddOnScreenDebugMessage(56, 2.f, FColor::Cyan, FString::Printf(TEXT("Inclinatie Offset: %.2f"), MoonInclinationOffset));
}
//eclipsa in grecia 2030
void AOrbitalManager::JumpToEclipse_2030_Jun()
{
	float TargetHour = 6.0f + (29.0f / 60.0f); 
	SetSimulationDateTime(1, 6, 2030, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Eclipsa Inelara 2030 (Grecia) - Muta drona la Lat: 37.5, Lon: 27.0"));
}
//eclipsa totala in Africa de Sud
void AOrbitalManager::JumpToEclipse_2030_Nov()
{
	float TargetHour = 6.0f + (51.0f / 60.0f);
	SetSimulationDateTime(25, 11, 2030, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eclipsa Totala 2030 (Africa Sud) - Muta drona la Lat: -30.5, Lon: 25.5"));
}
void AOrbitalManager::JumpToEclipse_2031_May()
{
	float TargetHour = 7.0f + (16.0f / 60.0f);
	SetSimulationDateTime(21, 5, 2031, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Eclipsa Inelara 2031 (India) - Muta drona la Lat: 10.5, Lon: 76.5"));
}
void AOrbitalManager::JumpToEclipse_2031_Nov()
{
	float TargetHour = 0.0f;
	SetSimulationDateTime(14, 11, 2031, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Purple, TEXT("Eclipsa Hibrida 2031 (Pacific) - Muta drona la Lat: 0.5, Lon: -138.0"));
}
void AOrbitalManager::JumpToEclipse_2034_Mar()
{
	float TargetHour = 10.0f + (18.0f / 60.0f);
	SetSimulationDateTime(20, 3, 2034, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eclipsa Totala 2034 (Egipt) - Muta drona la Lat: 26.5, Lon: 34.0"));
}
void AOrbitalManager::JumpToEclipse_2035_Sep()
{
	float TargetHour = 0.0f;
	SetSimulationDateTime(2, 9, 2035, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eclipsa Totala 2035 (China) - Muta drona la Lat: 40.0, Lon: 116.5"));
}
void AOrbitalManager::JumpToEclipse_Annular_2024()
{
	float TargetHour = 19.0f + (7.0f / 60.0f);
	SetSimulationDateTime(2, 10, 2024, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange,
		TEXT("INELARA 2024 (Insula Pastelui) - Muta Drona: Lat -27.1, Lon -109.4"));
}
void AOrbitalManager::JumpToEclipse_Annular_2026()
{
	float TargetHour = 11.0f + (13.0f / 60.0f); 
	SetSimulationDateTime(17, 2, 2026, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange,
		TEXT("INELARA 2026 (Corectata) - Muta Drona: Lat -64.7, Lon 86.75"));
}
void AOrbitalManager::JumpToEclipse_Partial_2025()
{
	float TargetHour = 10.0f + (18.0f / 60.0f);
	SetSimulationDateTime(29, 3, 2025, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow,
		TEXT("PARTIALA 2025 (Canada) - Muta Drona: Lat 44.6, Lon -63.6"));
}
void AOrbitalManager::JumpToEclipse_Partial_2029()
{
	float TargetHour = 17.0f + (15.0f / 60.0f);
	SetSimulationDateTime(14, 1, 2029, TargetHour);
	GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow,
		TEXT("PARTIALA 2029 (Seattle) - Muta Drona: Lat 47.6, Lon -122.3"));
}
//aici extrag luna ca numar int din string
int32 AOrbitalManager::GetMonthFromStr(const FString& MonthStr)
{
	if (MonthStr.Contains("Jan")) return 1;
	if (MonthStr.Contains("Feb")) return 2;
	if (MonthStr.Contains("Mar")) return 3;
	if (MonthStr.Contains("Apr")) return 4;
	if (MonthStr.Contains("May")) return 5;
	if (MonthStr.Contains("Jun")) return 6;
	if (MonthStr.Contains("Jul")) return 7;
	if (MonthStr.Contains("Aug")) return 8;
	if (MonthStr.Contains("Sep")) return 9;
	if (MonthStr.Contains("Oct")) return 10;
	if (MonthStr.Contains("Nov")) return 11;
	if (MonthStr.Contains("Dec")) return 12;
	return 0;
}
//trimitere date la python (legare de backend)
void AOrbitalManager::SendDataToPython()
{
	if (!Socket || !RemoteAddr.IsValid()) return;
	double PlayerLat = 41.096f;
	double PlayerLon = -2.553f;
	double PlayerAlt = 0.0f;
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (Player && GeoRef)
	{
		FVector PlayerUnreal = Player->GetActorLocation();
		FVector LLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(PlayerUnreal);
		PlayerLon = (double)LLH.X;
		PlayerLat = (double)LLH.Y;
		PlayerAlt = (double)LLH.Z;
	}
	double DataToSend[10];
	//trimit ca date anul,luna, ziua,ora, minutul si secunda , latitudinea, longitudinea, altitudinea si limba
	//pentru limba, 1 inseamna engleza si 0 romana
	DataToSend[0] = (double)CurrentDate.Year; 
	DataToSend[1] = (double)CurrentDate.Month;
	DataToSend[2] = (double)CurrentDate.Day;
	int32 HourInt = (int32)HighResHour;
	double HourFrac = HighResHour - (double)HourInt;
	double MinTotal = HourFrac * 60.0;
	int32 MinInt = (int32)MinTotal;
	double MinFrac = MinTotal - (double)MinInt;
	double PreciseSec = MinFrac * 60.0;
	DataToSend[3] = (double)HourInt;
	DataToSend[4] = (double)MinInt;
	DataToSend[5] = (double)PreciseSec; 
	DataToSend[6] = PlayerLat;
	DataToSend[7] = PlayerLon;
	DataToSend[8] = PlayerAlt;
	FString CurrentLang = TEXT("Romana"); 
	if (GConfig)
	{
		GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), CurrentLang, GGameUserSettingsIni);
	}
	double LangCode = (CurrentLang == TEXT("English")) ? 1.0 : 0.0;
	DataToSend[9] = LangCode;
	int32 BytesSent = 0;
	Socket->SendTo((uint8*)DataToSend, sizeof(DataToSend), BytesSent, *RemoteAddr);
}
//aici primesc date de la python, este o functie extrem de lunga, primesc 352 de doubleuri si maximum 900 de caractere pentru locatie si datele pentru tabelul lui xavier jubier
//am ales double in loc de float pentru o precizie cat mai buna pe numere mari
void AOrbitalManager::ReceiveDataFromPython()
{
	if (!Socket) return;
	uint32 Size;
	while (Socket->HasPendingData(Size))
	{
		uint8 ReceivedData[4096];
		int32 BytesRead;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		FString FullText = TEXT("No Data");
		FString LocationName = TEXT("No Data");
		if (Socket->RecvFrom(ReceivedData, sizeof(ReceivedData), BytesRead, *Sender))
		{
			int32 DoubleOffset = (BytesRead >= 2856) ? 2856 : ((BytesRead >= 376) ? 376 : 56);
			if (BytesRead >= DoubleOffset)
			{
				//primele 8 doubleuri sunt pozitia soarelui, a lunii (cate 3 doubleuri pentru cele 3 axe) si unul pentru acoperire soare de catre luna si unul pentru grade 
				// (cu cate grade este rotita bolta cereasca)
				double* DoubleData = (double*)ReceivedData;
				FVector3d SunKM = FVector3d(DoubleData[0], DoubleData[1], DoubleData[2]);
				FVector3d MoonKM = FVector3d(DoubleData[3], DoubleData[4], DoubleData[5]);
				float Coverage = (float)DoubleData[6]; 
				double GAST_Deg = DoubleData[7];
				PlanetsList.Empty();
				//mai jos, sparg pachetul in alte 21 de doubleuri (cate 3 doubleuri pentru fiecare din cele 7 planete)
				if (DoubleOffset >= 232)
				{
					TArray<FString> PNames = { "Mercur", "Venus", "Marte", "Jupiter", "Saturn", "Uranus", "Neptun" };
					TArray<FString> PNamesRo = { TEXT("Mercur"), TEXT("Venus"), TEXT("Marte"), TEXT("Jupiter"), TEXT("Saturn"), TEXT("Uranus"), TEXT("Neptun") };
					TArray<FString> PNamesEn = { TEXT("Mercury"), TEXT("Venus"), TEXT("Mars"), TEXT("Jupiter"), TEXT("Saturn"), TEXT("Uranus"), TEXT("Neptune") };
					TArray<FColor> PColors = { FColor(150, 150, 150), FColor(255, 255, 200), FColor(255, 100, 50), FColor(255, 200, 150), FColor(250, 230, 180), FColor(150, 200, 255), FColor(50, 100, 255) };
					TArray<double> PRadiiKm = { 2439.7, 6051.8, 3389.5, 71492.0, 58232.0, 25362.0, 24622.0 };
					ACesiumGeoreference* GeoRefLocal = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
					APawn* PlayerLocal = UGameplayStatics::GetPlayerPawn(this, 0);
					APlayerCameraManager* CamManager2 = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
					if (GeoRefLocal && PlayerLocal)
					{
						FVector PlayerPosUnreal = PlayerLocal->GetActorLocation();
						FVector CameraPosUnreal = CamManager2 ? CamManager2->GetCameraLocation() : PlayerPosUnreal;
						FVector3d PlayerECEF_Cm = GeoRefLocal->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPosUnreal);
						int32 DataIndex = 8;
						for (int32 i = 0; i < 7; i++) //sunt 7 planete
						{
							FVector3d PlanetKM = FVector3d(DoubleData[DataIndex], DoubleData[DataIndex + 1], DoubleData[DataIndex + 2]);
							DataIndex += 3;
							FVector3d PlanetECEF_Cm = PlanetKM * 100000.0;
							FVector3d CameraECEF_Cm = GeoRefLocal->TransformUnrealPositionToEarthCenteredEarthFixed(CameraPosUnreal);
							FVector3d TrueDirECEF = (PlanetECEF_Cm - CameraECEF_Cm).GetSafeNormal();
							FVector PlanetDirUnreal = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(TrueDirECEF.X, TrueDirECEF.Y, TrueDirECEF.Z));
							double RealDistKm = FVector3d::Distance(PlanetKM, CameraECEF_Cm / 100000.0);
							FPlanetData PData;
							//fiecare planeta are un nume, o directie in care privesc spre ea, o distanta si o culoare specifica pentru eticheta
							PData.InternalName = PNames[i];
							PData.Name = AOrbitalManager::TrString(PNamesRo[i], PNamesEn[i]);
							PData.Direction = PlanetDirUnreal;
							PData.Color = PColors[i];
							PData.DistanceKm = RealDistKm;
							PlanetsList.Add(PData);
							//aici setez o textura pentru fiecare planeta
							if (PlanetMeshes.IsValidIndex(i) && PlanetMeshes[i])
							{
								if (PlanetMasterMaterial && PlanetTextures.IsValidIndex(i) && PlanetTextures[i])
								{
									UMaterialInterface* CurrentMat = PlanetMeshes[i]->GetMaterial(0);
									if (!CurrentMat || !CurrentMat->IsA(UMaterialInstanceDynamic::StaticClass()))
									{
										UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(PlanetMasterMaterial, this);
										DynMat->SetTextureParameterValue(FName("PlanetTexture"), PlanetTextures[i]);
										PlanetMeshes[i]->SetMaterial(0, DynMat);
									}
								}
								double VisualDistCm = PlanetVisualDistanceKm * 100000.0;
								FVector PlanetPosViz = CameraPosUnreal + (PlanetDirUnreal * VisualDistCm);
								PlanetMeshes[i]->SetWorldLocation(PlanetPosViz);
								//planetele se rotesc si in jurul axelor lor
								TArray<double> RotationPeriodsDays = {
									 58.6462,    
									-243.0226,   
									  0.99727,   
									  1.02596,   
									  0.41354,   
									  0.44401,   
									 -0.71833,   
								};
								//Venus si Neptun sunt cu minus, ca se invart in directie opusa fata de restul
								double JD = GetJulianDate(CurrentDate.Year, CurrentDate.Month, CurrentDate.Day, CurrentDate.Hour);
								double d = JD - 2451545.0; 
								double W_Angle = 0.0; //unghiuri setate pentru fiecare   
								double AxialTilt = 0.0; 
								switch (i)
								{
								case 0: 
									W_Angle = 328.46 + 6.1385025 * d;
									AxialTilt = 0.034; break;
								case 1: 
									W_Angle = 160.20 - 1.4813688 * d;
									AxialTilt = 177.36; break;
								case 2: 
									W_Angle = 176.630 + 350.89198226 * d;
									AxialTilt = 25.19; break;
								case 3: 
									W_Angle = 284.95 + 870.5360000 * d;
									AxialTilt = 3.13; break;
								case 4: 
									W_Angle = 38.90 + 810.7939024 * d;
									AxialTilt = 26.73; break;
								case 5: 
									W_Angle = 203.81 - 501.1600928 * d;
									AxialTilt = 97.77; break;
								case 6: 
									W_Angle = 253.18 + 536.3128492 * d;
									AxialTilt = 28.32; break;
								}
								W_Angle = FMath::Fmod(W_Angle, 360.0);
								if (W_Angle < 0.0) W_Angle += 360.0;
								FVector ForwardAxis = (-PlanetDirUnreal).GetSafeNormal();
								FVector EarthNorth = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
								if (FMath::Abs(FVector::DotProduct(ForwardAxis, EarthNorth)) > 0.99f) {
									EarthNorth = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
								}
								FVector RightAxis = FVector::CrossProduct(EarthNorth, ForwardAxis).GetSafeNormal();
								FVector UpAxis = FVector::CrossProduct(ForwardAxis, RightAxis).GetSafeNormal();
								FQuat TiltQuat = FQuat(RightAxis, FMath::DegreesToRadians(AxialTilt));
								UpAxis = TiltQuat.RotateVector(UpAxis);
								ForwardAxis = TiltQuat.RotateVector(ForwardAxis);
								RightAxis = FVector::CrossProduct(UpAxis, ForwardAxis).GetSafeNormal();
								FQuat SpinQuat = FQuat(UpAxis, FMath::DegreesToRadians(W_Angle));
								ForwardAxis = SpinQuat.RotateVector(ForwardAxis);
								RightAxis = SpinQuat.RotateVector(RightAxis);
								FMatrix PlanetMatrix(ForwardAxis, RightAxis, UpAxis, FVector::ZeroVector);
								PlanetMeshes[i]->SetWorldRotation(PlanetMatrix.Rotator());
								double TrueScale = (PRadiiKm[i] / RealDistKm) * (VisualDistCm / 50.0) * 100.0f;
								PlanetMeshes[i]->SetWorldScale3D(FVector(TrueScale));
								if (!PlanetMeshes[i]->IsVisible())
								{
									PlanetMeshes[i]->SetVisibility(true);
									if (i == 4 && SaturnRingsMesh)
									{
										SaturnRingsMesh->SetVisibility(true);
									}
								}
							}
						}
						//urmatoarele 18 doubleuri sunt pentru cei 6 sateliti naturali
						if (DoubleOffset >= 376)
						{
							MajorMoonsList.Empty();
							TArray<double> MoonsRadiiKm = { 1821.6, 1560.8, 2634.1, 2410.3, 2574.7, 1353.4 };
							TArray<FString> MajorMoonsNames = { "Io", "Europa", "Ganymede", "Callisto", "Titan", "Triton" };
							int32 MoonDataIndex = 29;
							static FVector SmoothSatOffset[6] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
							float DeltaTime = GetWorld()->GetDeltaSeconds();
							float CurrentTimeSpeed = FMath::Abs(TimeSpeed);
							for (int32 i = 0; i < 6; i++) //iterare prin cei 6 sateliti naturali
							{
								FVector3d SatelliteKM = FVector3d(DoubleData[MoonDataIndex], DoubleData[MoonDataIndex + 1], DoubleData[MoonDataIndex + 2]);
								MoonDataIndex += 3;
								FVector3d SatelliteECEF_Cm = SatelliteKM * 100000.0;
								int32 ParentIdx = 3;
								if (i == 4) ParentIdx = 4; //parentidx se refera la planeta-mama
								if (i == 5) ParentIdx = 6;
								int32 ParentDataIndex = 8 + (ParentIdx * 3);
								FVector3d ParentKM = FVector3d(DoubleData[ParentDataIndex], DoubleData[ParentDataIndex + 1], DoubleData[ParentDataIndex + 2]);
								FVector3d ParentECEF_Cm = ParentKM * 100000.0;
								FVector3d OffsetECEF_Cm = SatelliteECEF_Cm - ParentECEF_Cm;
								double ParentRealDistCm = FVector3d::Distance(ParentECEF_Cm, PlayerECEF_Cm); //calculez offsetul fata de planeta parinte
								double VisualDistCm = 50000.0f * 100000.0; 
								double ScaleFactor = VisualDistCm / ParentRealDistCm;
								FVector3d ParentDirECEF_3D = (ParentECEF_Cm - PlayerECEF_Cm).GetSafeNormal();
								FVector ParentDirUnreal = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(ParentDirECEF_3D.X, ParentDirECEF_3D.Y, ParentDirECEF_3D.Z));
								FVector ParentCenterViz = CameraPosUnreal + (ParentDirUnreal * VisualDistCm);
								double OffsetLengthReal = OffsetECEF_Cm.Size();
								FVector3d OffsetDirECEF = OffsetECEF_Cm.GetSafeNormal();
								//setez directia pt offset
								FVector OffsetDirUnreal = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(OffsetDirECEF.X, OffsetDirECEF.Y, OffsetDirECEF.Z));
								FVector TargetSatOffsetUnreal = OffsetDirUnreal * (OffsetLengthReal * ScaleFactor);
								if (SmoothSatOffset[i].IsNearlyZero() || CurrentTimeSpeed > 2.0f) {
									SmoothSatOffset[i] = TargetSatOffsetUnreal;
								}
								else {
									SmoothSatOffset[i] = FMath::VInterpTo(SmoothSatOffset[i], TargetSatOffsetUnreal, DeltaTime, 15.0f);
								}
								FVector SatPosViz = ParentCenterViz + SmoothSatOffset[i];
								FVector SatelliteDirUnreal = (SatPosViz - CameraPosUnreal).GetSafeNormal();
								double SatelliteRealDistKm = FVector3d::Distance(SatelliteKM, PlayerECEF_Cm / 100000.0);
								FPlanetData MoonData;
								MoonData.Name = MajorMoonsNames[i];
								MoonData.Direction = SatelliteDirUnreal;
								MoonData.DistanceKm = SatelliteRealDistKm;
								MoonData.Color = FColor::White;
								MajorMoonsList.Add(MoonData);
								//aici pun meshurile pe satelitii naturali
								//fiecare are cate un mesh separat
								if (MajorMoonsMeshes.IsValidIndex(i) && MajorMoonsMeshes[i])
								{
									if (PlanetMasterMaterial && MajorMoonsTextures.IsValidIndex(i) && MajorMoonsTextures[i])
									{
										UMaterialInterface* CurrentMat = MajorMoonsMeshes[i]->GetMaterial(0);
										if (!CurrentMat || !CurrentMat->IsA(UMaterialInstanceDynamic::StaticClass()))
										{
											UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(PlanetMasterMaterial, this);
											DynMat->SetTextureParameterValue(FName("PlanetTexture"), MajorMoonsTextures[i]);
											MajorMoonsMeshes[i]->SetMaterial(0, DynMat);
										}
									}
									MajorMoonsMeshes[i]->SetWorldLocation(SatPosViz);
									FVector SatForwardAxis = (-SatelliteDirUnreal).GetSafeNormal();
									FVector SatEarthNorth = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
									if (FMath::Abs(FVector::DotProduct(SatForwardAxis, SatEarthNorth)) > 0.99f) {
										SatEarthNorth = GeoRefLocal->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
									}
									FVector SatRightAxis = FVector::CrossProduct(SatEarthNorth, SatForwardAxis).GetSafeNormal();
									FVector SatUpAxis = FVector::CrossProduct(SatForwardAxis, SatRightAxis).GetSafeNormal();
									FMatrix SatMatrix(SatForwardAxis, SatRightAxis, SatUpAxis, FVector::ZeroVector);
									MajorMoonsMeshes[i]->SetWorldRotation(SatMatrix.Rotator());
									double SatTrueScale = (MoonsRadiiKm[i] / SatelliteRealDistKm) * (VisualDistCm / 50.0) * 100.0f;
									MajorMoonsMeshes[i]->SetWorldScale3D(FVector(SatTrueScale));
									if (!MajorMoonsMeshes[i]->IsVisible()) MajorMoonsMeshes[i]->SetVisibility(true);
								}
							}
						} 
					}
				}
				bool CameraMap2 = false;
				APlayerController* PC3 = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				ADronaMea* Drona3 = Cast<ADronaMea>(PC3->GetPawn());
				if (Drona3)
				{
					CameraMap2 = Drona3->bMapModeActive;
				}
				if (EclipsePathMesh)
				{
					EclipsePathMesh->SetVisibility(CameraMap2);
				}
				//urmatoarele 300 de valori double vin de la asta:
				//cate 100 de perechi de coordonate latitudine-longitudine
				//cate 100 de latimi (fiecare segment din banda de totalitate are latime usor diferita)
				if (DoubleOffset >= 2784)
				{
					NumPathPoints = FMath::RoundToInt(DoubleData[47]);
					ACesiumGeoreference* GeoRefPath2 = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
					FVector CurrentEarthCenter = GeoRefPath2 ? GeoRefPath2->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
					bool bPathDataChanged = (NumPathPoints != LastPathCount) || (NumPathPoints > 0 && DoubleData[48] != LastFirstLon);
					bool bEarthMoved = !CurrentEarthCenter.Equals(LastEarthCenter, 100.0f);
					bool bIsNewPath = bPathDataChanged || bEarthMoved;
					//pot sa vad banda de totalitate daca sunt pe bmapmodeactive si utilizatorul a dat click sa o vada
					bool bShouldShowPath2 = true; //verific daca trebuie sa vad banda de totalitate
					APlayerController* PC_Map2 = UGameplayStatics::GetPlayerController(GetWorld(), 0);
					if (PC_Map2)
					{
						ADronaMea* MyDrone2 = Cast<ADronaMea>(PC_Map2->GetPawn());
						if (MyDrone2)
						{
							bShouldShowPath2 = bShowTotalityZone && MyDrone2->bMapModeActive;
						}
					}
					EclipsePathMesh->SetVisibility(bShouldShowPath2);
					if (NumPathPoints > 0)
					{
						SavedEclipsePath.Empty();
						for (int32 i = 0; i < NumPathPoints && i < 100; i++)
						{
							//extrag latitudinea, longitudinea si latimea benzii de totalitate
							double Lon = DoubleData[48 + (i * 2)];
							double Lat = DoubleData[48 + (i * 2) + 1];
							double Latime = DoubleData[48 + 200 + i];
							SavedEclipsePath.Add(FVector(Lon, Lat, Latime));
						}
					}
					else
					{
						SavedEclipsePath.Empty();
					}
					//daca este cale noua, vreau sa o salvez, din simplul motiv ca nu vreau sa primesc mereu datele frame de frame, ca poate nu mai imi trimite serverul de python
					if (bIsNewPath)
					{
						LastPathCount = NumPathPoints;
						if (NumPathPoints > 0) LastFirstLon = DoubleData[48];
						LastEarthCenter = CurrentEarthCenter; 
						SavedEclipsePath.Empty();
						if (NumPathPoints > 0)
						{
							//aceste min si max sunt pentru a determina latimea minima si maxima a benzii de totalitate
							double max = 0.0f;
							double min = 1000.0f;
							if (EclipsePathMesh)
							{
								//imi creez meshul pe baza datelor primite
								ACesiumGeoreference* GeoRefPath = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
								TArray<FVector> UnrealPoints;
								for (int32 i = 0; i < NumPathPoints && i < 100; i++)
								{
									double Lon = DoubleData[48 + (i * 2)];
									double Lat = DoubleData[48 + (i * 2) + 1];
									double Latime = DoubleData[48 + 200 + i];
									SavedEclipsePath.Add(FVector(Lon, Lat, Latime));
									if (Latime > max) max = Latime;
									if (Latime < min) min = Latime;
									FVector LLH(Lon, Lat, 20000.0f);
									FVector UnrealPos = GeoRefPath ? GeoRefPath->TransformLongitudeLatitudeHeightPositionToUnreal(LLH) : FVector::ZeroVector;
									UnrealPoints.Add(UnrealPos);
								}
								//aici creez efectiv meshul pe care il asez peste pamant
								TArray<FVector> Vertices;
								TArray<int32> Triangles;
								TArray<FVector2D> UVs;
								TArray<FVector> Normals;
								FVector EarthCenter = GeoRefPath ? GeoRefPath->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
								for (int32 i = 0; i < UnrealPoints.Num(); i++)
								{
									FVector Point = UnrealPoints[i];
									FVector Up = (Point - EarthCenter).GetSafeNormal();
									float HalfWidthCm = DoubleData[48 + 200 + i] * 50000.0f;
									FVector Forward;
									if (i < UnrealPoints.Num() - 1)
									{
										Forward = (UnrealPoints[i + 1] - Point).GetSafeNormal();
									}
									else
									{
										Forward = (Point - UnrealPoints[i - 1]).GetSafeNormal();
									}
									FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
									Vertices.Add(Point - Right * HalfWidthCm); 
									Vertices.Add(Point + Right * HalfWidthCm); 
									float U = (float)i / (float)(UnrealPoints.Num() - 1);
									UVs.Add(FVector2D(U, 0.0f));
									UVs.Add(FVector2D(U, 1.0f));
									Normals.Add(Up);
									Normals.Add(Up);
								}
								for (int32 i = 0; i < UnrealPoints.Num() - 1; i++)
								{
									int32 V0 = i * 2;       
									int32 V1 = V0 + 1;      
									int32 V2 = V0 + 2;      
									int32 V3 = V0 + 3;      
									Triangles.Add(V0);
									Triangles.Add(V1);
									Triangles.Add(V2);
									Triangles.Add(V1);
									Triangles.Add(V3);
									Triangles.Add(V2);
								}
								EclipsePathMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
								if (EclipsePathMaterial)
								{
									EclipsePathMesh->SetMaterial(0, EclipsePathMaterial);
								}
								//setez vizibilitatea meshului si afisez mesaje de debugging
								EclipsePathMesh->SetVisibility(bShowTotalityZone); 
								if (GEngine) GEngine->AddOnScreenDebugMessage(111, 2.0f, FColor::Green, TEXT(">>> PANGLICA PROCEDURALA GENERATA PERFECT! <<<"));
								if (GEngine) GEngine->AddOnScreenDebugMessage(112, 2.0f, FColor::Green, FString::Printf(TEXT(">>> Latime Max: %.2f | Min: %.2f <<<"), max, min));
								if (EclipsePathISMC) EclipsePathISMC->ClearInstances();
							}
						}
						else
						{
							//daca nu detectez eclipse, sterg tot
							if (EclipsePathMesh) EclipsePathMesh->ClearAllMeshSections();
							if (EclipsePathISMC) EclipsePathISMC->ClearInstances();
							if (GEngine) GEngine->AddOnScreenDebugMessage(113, 2.0f, FColor::Orange, TEXT("Nicio eclipsa detectata. Am curatat harta."));
						}
					}
				}
				//daca 
				if (DoubleOffset >= 2856)
				{
					LiveShadowLat = DoubleData[348];
					LiveShadowLon = DoubleData[349];
					Deltatseconds = DoubleData[350];

					SunCenterECEF = FVector(DoubleData[351], DoubleData[352], DoubleData[353]);
					MoonCenterECEF = FVector(DoubleData[354], DoubleData[355], DoubleData[356]);
					//aici afisez umbra live
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(700, 0.0f, FColor::Purple,
							FString::Printf(TEXT("Umbra Live Python: Lat %.4f, Lon %.4f"), LiveShadowLat, LiveShadowLon));
						
					}

				}
				else
				{
					LiveShadowLat = -999.0f;
					LiveShadowLon = -999.0f;
				}
				if (BytesRead > DoubleOffset)
				{
					int32 TextLength = BytesRead - DoubleOffset;
					char TextBuffer[2048];
					if (TextLength >= 2048) TextLength = 2047;
					FMemory::Memcpy(TextBuffer, &ReceivedData[DoubleOffset], TextLength);
					TextBuffer[TextLength] = '\0'; 
					FullText = UTF8_TO_TCHAR(TextBuffer);
				}
				FString Separator = AOrbitalManager::TrString(TEXT("\nCOORDONATE:"), TEXT("\nCOORDS:"));
				FString LocationOnly;
				FString RestOfData;
				if (FullText.Split(Separator, &LocationOnly, &RestOfData))
				{
					FString CleanLocation = LocationOnly;
					CleanLocation = CleanLocation.Replace(TEXT("LOCATIE: "), TEXT(""));
					CleanLocation = CleanLocation.Replace(TEXT("LOCATION: "), TEXT(""));
					LocationName = CleanLocation.TrimStartAndEnd();
					CurrentLocationText = LocationName; 
					FString SeparatorFaraEnter = AOrbitalManager::TrString(TEXT("COORDONATE:"), TEXT("COORDS:"));
					FString FinalTableText = SeparatorFaraEnter + RestOfData;
					APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
					if (ADronaMea* MyDrone = Cast<ADronaMea>(PlayerPawn))
					{
						MyDrone->UpdateEclipseWidgetText(FinalTableText);
					}
				}
				else
				{
					LocationName = FullText;
					CurrentLocationText = LocationName;
				}
				FVector SunECEF_Cm = FVector(SunKM.X, SunKM.Y, SunKM.Z) * 1000.0f;
				FVector MoonECEF_Cm = FVector(MoonKM.X, MoonKM.Y, MoonKM.Z) * 1000.0f;
				APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
				ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
				if (Player && GeoRef)
				{
					FVector PlayerPosUnreal = Player->GetActorLocation();
					FVector PlayerECEF_Cm = GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPosUnreal);
					FVector SunDirECEF = (SunECEF_Cm - PlayerECEF_Cm).GetSafeNormal();
					FVector MoonDirECEF = (MoonECEF_Cm - PlayerECEF_Cm).GetSafeNormal();
					FVector SunDirUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(SunDirECEF);
					FVector MoonDirUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(MoonDirECEF);
					double DistSun_Cm = FVector::Dist(SunECEF_Cm, PlayerECEF_Cm);
					double DistMoon_Cm = FVector::Dist(MoonECEF_Cm, PlayerECEF_Cm);
					CurrentSunDistanceKm = DistSun_Cm / 1000.0;
					CurrentMoonDistanceKm = DistMoon_Cm / 1000.0;
					if (SunSkyActor)
					{
						UDirectionalLightComponent* Light = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
						if (Light)
						{
							Light->SetWorldRotation((-SunDirUnreal).Rotation());
							const double SunRadiusKm = 696340.0;
							if (CurrentSunDistanceKm > 1.0) 
							{
								double AngleRadians = 2.0 * atan(SunRadiusKm / CurrentSunDistanceKm);
								float RealSunAngleDeg = (float)FMath::RadiansToDegrees(AngleRadians);
								Light->SetLightSourceAngle(RealSunAngleDeg);
								Light->SetLightSourceSoftAngle(RealSunAngleDeg);
							}
							float Intensity = 10.0f; 
						}
					}
					//aici plasez luna si ii pun si dimensiunea exacta
					if (ControlledMoon)
					{
						double RealDist_Cm = FVector::Dist(MoonECEF_Cm, PlayerECEF_Cm);
						FVector RealMoonPos = PlayerPosUnreal + (MoonDirUnreal * RealDist_Cm);
						ControlledMoon->SetActorLocation(RealMoonPos);
						FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
						FVector ForwardAxis = (RealMoonPos - EarthCenter).GetSafeNormal();
						FVector EarthNorth = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
						FVector RightAxis = FVector::CrossProduct(EarthNorth, ForwardAxis).GetSafeNormal();
						FVector UpAxis = FVector::CrossProduct(ForwardAxis, RightAxis).GetSafeNormal();
						FMatrix MoonMatrix(ForwardAxis, RightAxis, UpAxis, FVector::ZeroVector);
						ControlledMoon->SetActorRotation(MoonMatrix.Rotator());
						double ExactScale = 347480000.0 / 100.0;
						ControlledMoon->SetActorScale3D(FVector(ExactScale));
					}
					//aici pun stele si constelatii pe ecran, Gast_deg reprezinta offsetul in grade intre Pamant si bolta cereasca
					if (StarsISMC)
					{
						FVector Unreal_X = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0));
						FVector Unreal_Z = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1));
						FMatrix EcefToUnrealMat = FRotationMatrix::MakeFromXZ(Unreal_X, Unreal_Z);
						FRotator IcrsToEcefRot(0.0f, GAST_Deg, 0.0f);
						FMatrix StarDomeMatrix = IcrsToEcefRot.Quaternion().ToMatrix() * EcefToUnrealMat;
						CurrentStarDomeMatrix = StarDomeMatrix;
						StarsISMC->SetWorldLocationAndRotation(PlayerPosUnreal, StarDomeMatrix.Rotator());
						if (ConstellationLinesISMC)
						{
							ConstellationLinesISMC->SetWorldLocationAndRotation(PlayerPosUnreal, StarDomeMatrix.Rotator());
						}
						if (ZodiacRoot)
						{
							ZodiacRoot->SetWorldLocationAndRotation(PlayerPosUnreal, StarDomeMatrix.Rotator());
						}
					}
					bool CameraMap = false;
					APlayerController* PC2 = UGameplayStatics::GetPlayerController(GetWorld(), 0);
					if (PC2)
					{
						ADronaMea* Drona2 = Cast<ADronaMea>(PC2->GetPawn());
						if (Drona2)
						{
							CameraMap = Drona2->bMapModeActive;
						}
					}
					//aici aleg cand incepe fiecare fenomen in parte
					float BaseSunIntensity = 110000.0f;
					float TargetMainIntensity = 110000.0f; 
					bool bShowDiamondFlash = false;
					float TargetFlashIntensity = 0.0f;
					float DiamondStart = 0.94f;
					float startDiamondRing = 0.997f; //diamond ring incepe la 99.7% din acoperire
					float startBailey = 0.99925f; //Margelele lui Bailey incep la 99.925% din acoperire (pana la threshold)
					float threshold_activation = 0.99999; 
					FVector FlashDirection = SunDirUnreal;
					FVector FlashDirection2 = SunDirUnreal;
					if (DiamondFlashMesh && !DiamondFlashMaterialDynamic)
					{
						UMaterialInterface* BaseMat = DiamondFlashMesh->GetMaterial(0);
						if (BaseMat) {
							DiamondFlashMaterialDynamic = UMaterialInstanceDynamic::Create(BaseMat, this);
							DiamondFlashMesh->SetMaterial(0, DiamondFlashMaterialDynamic);
						}
					}
					//aici plasez inelul pe ecran, iese 0.02 grade din Soare
					if (Coverage >= startDiamondRing && Coverage <= threshold_activation)
					{
						FVector RotationAxis = FVector::CrossProduct(MoonDirUnreal, SunDirUnreal).GetSafeNormal();
						if (RotationAxis.IsNearlyZero())
						{
							RotationAxis = FVector::CrossProduct(MoonDirUnreal, FVector::RightVector).GetSafeNormal();
							if (RotationAxis.IsNearlyZero()) {
								RotationAxis = FVector::CrossProduct(MoonDirUnreal, FVector::ForwardVector).GetSafeNormal();
							}
						}
						//diamantul iese fix pe partea opusa pe care vine luna (adica exista o directie relativa a deplasarii Soarelui si a Lunii)
						float EdgeOffsetDegrees = 0.3f;
						FlashDirection = MoonDirUnreal.RotateAngleAxis(EdgeOffsetDegrees, RotationAxis);
						float EdgeOffsetDegrees2 = 0.28f;
						FlashDirection2 = MoonDirUnreal.RotateAngleAxis(EdgeOffsetDegrees2, RotationAxis);
						if (Coverage >= startDiamondRing && Coverage < startBailey)
						{
							float DiamondAlpha = (Coverage - startDiamondRing) / (startBailey - startDiamondRing);
							float DiamondCurve = FMath::Sin(DiamondAlpha * PI);
							bShowDiamondFlash = true;
							TargetFlashIntensity = DiamondCurve * 150000000.0f;
						}
						else
						{
							bShowDiamondFlash = false;
							TargetFlashIntensity = 0.0f;
						}
						TargetMainIntensity = 110000.0f; 
					}
					else
					{
						//altfel nu afisez efectul de diamant
						bShowDiamondFlash = false;
						TargetFlashIntensity = 0.0f;
						float FadeAlpha = FMath::Clamp(Coverage / startDiamondRing, 0.0f, 1.0f);
						TargetMainIntensity = 110000.0f;
					}
					if (DiamondFlashMesh && DiamondFlashMaterialDynamic)
					{
						//activez efectul de inel cu diamant doar daca nu sunt in modul harta si bloomul este activ peste 0.3
						float CurrentBloom = 1.0f;
						APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
						if (PC)
						{
							ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
							if (Drona)
							{
								UCameraComponent* ActiveCam = Drona->bMapModeActive ? Drona->MapCamera : Drona->Camera;
								if (ActiveCam && ActiveCam->PostProcessSettings.bOverride_BloomIntensity) {
									CurrentBloom = ActiveCam->PostProcessSettings.BloomIntensity;
								}
								else if (GConfig) {
									GConfig->GetFloat(TEXT("SetariCustom"), TEXT("Bloom"), CurrentBloom, GGameUserSettingsIni);
								}
							}
						}
						bool bAllowLightFX = (CurrentBloom >= 0.3f);
						bool bFinalDiamondVis = bShowDiamondFlash && bAllowLightFX && !CameraMap;
						DiamondFlashMesh->SetVisibility(bFinalDiamondVis);
						if (bFinalDiamondVis)
						{
							//aici aleg dimensiunea diamantului si pozitia sa.
							FVector FlashPos = PlayerPosUnreal + (FlashDirection * 10000.0f * 1000.f * 6.f);
							DiamondFlashMesh->SetWorldLocation(FlashPos);
							DiamondFlashMesh->SetWorldRotation((PlayerPosUnreal - FlashPos).Rotation());
							DiamondFlashMesh->SetWorldScale3D(FVector(8.72f * 10000.f));
							DiamondFlashMaterialDynamic->SetScalarParameterValue(TEXT("FlashIntensity"), TargetFlashIntensity * 100.f);
						}
						//similar fac si pentru margelele lui Bailey
						//daca sunt intre acoperire 99.925% si 99.999%
						if (BeadMeshes.Num() == 6 && BeadMaterialsDynamic.Num() == 6 && CameraMap == false)
						{
							//plasez margelele pe partea opusa fata de luna, ca sa para cat mai realist
							bool bShowBeads = bAllowLightFX && (Coverage >= startBailey) && (Coverage <= threshold_activation);
							TArray<double> BeadAngles = { 10.0, -5.0, 22.0, -23.0, 32.0, -31.0 };
							for (int32 i = 0; i < BeadMeshes.Num(); i++)
							{
								if (!BeadMeshes[i]) continue;
								BeadMeshes[i]->SetVisibility(bShowBeads);
								if (bShowBeads && i < BeadAngles.Num())
								{
									//fac exact aceleasi lucruri ca si la diamondring, adica calculez pozitia, rotatia si il plasez in lume
									double BeadAlpha = (double)(Coverage - startBailey) / (threshold_activation - startBailey);
									double BeadCurve = FMath::Sin(BeadAlpha * PI);
									FVector3d CenterDir3D = FVector3d(MoonDirUnreal).GetSafeNormal();
									FVector3d FlashDir3D = FVector3d(FlashDirection2).GetSafeNormal();
									double AngleRads = FMath::DegreesToRadians(BeadAngles[i]);
									FQuat4d RotationQuat(CenterDir3D, AngleRads);
									FVector3d BeadDir3D = RotationQuat.RotateVector(FlashDir3D).GetSafeNormal();
									double ExactDistance = 60000000.0;
									FVector3d PlayerPos3D = FVector3d(PlayerPosUnreal);
									FVector3d BeadPos3D = PlayerPos3D + (BeadDir3D * ExactDistance);
									BeadMeshes[i]->SetWorldLocation(FVector(BeadPos3D));
									BeadMeshes[i]->SetWorldRotation((PlayerPos3D - BeadPos3D).ToOrientationRotator());
									BeadMeshes[i]->SetWorldScale3D(FVector(100.0));
									double TimeSeconds = (double)GetWorld()->GetTimeSeconds();
									double OrganicFlicker = FMath::Abs(FMath::Sin(TimeSeconds * (10.0 + (double)i * 2.0)));
									double FinalBeadInt = BeadCurve * 4000000000.0 * (0.8 + 0.2 * OrganicFlicker);
									BeadMaterialsDynamic[i]->SetScalarParameterValue(TEXT("FlashIntensity"), (float)(FinalBeadInt * 100.0));
								}
							}
						}
						float TotalityLightIntensity = 10.0f; 
						float CurrentSunIntensity;
						//acoperire peste 1.0 inseamna eclipsa totala
						if (Coverage >= 1.0f)
						{
							CurrentSunIntensity = TotalityLightIntensity;
							if (SunSkyActor)
							{
								UDirectionalLightComponent* Light = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
								if (Light) Light->SetIntensity(15.0f); 
							}
						}
						if (CoronaMesh && CoronaDynamicMat && ControlledMoon && CameraMap == false)
						{
							bool bShowCorona = bAllowLightFX && (Coverage >= 1.00f);
							CoronaMesh->SetVisibility(bShowCorona);
							if (bShowCorona)
							{
								//plasez un plan in spatele lunii
								FVector3d SunDir3D = FVector3d(SunDirUnreal).GetSafeNormal();
								FVector3d PlayerPos3D = FVector3d(PlayerPosUnreal);
								FVector3d MoonPos3D = FVector3d(ControlledMoon->GetActorLocation());
								FVector3d CoronaPos3D = MoonPos3D + (SunDir3D * 2000000.0);
								CoronaMesh->SetWorldLocation(FVector(CoronaPos3D));
								FRotator FacePlayer = (FVector(PlayerPos3D) - FVector(CoronaPos3D)).Rotation();
								FacePlayer.Pitch -= 90.0f; 
								CoronaMesh->SetWorldRotation(FacePlayer);
								FVector MoonScale = ControlledMoon->GetActorScale3D();
								//coroana este de 1.85 ori mai mare decat soarele, deci se extinde cu 42.5% in exteriorul Soarelui la totalitate
								CoronaMesh->SetWorldScale3D(MoonScale * 1.85f);
								float CoronaAlpha = FMath::Clamp((Coverage - 0.998f) / (1.0f - 0.998f), 0.0f, 1.0f);
								CoronaDynamicMat->SetScalarParameterValue(FName("CoronaIntensity"), CoronaAlpha);
							}
						}
					}
					if (SunSkyActor)
					{
						UDirectionalLightComponent* Light2 = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
						if (Light2)
						{
							Light2->SetWorldRotation((-SunDirUnreal).Rotation());
							Light2->SetIntensity(TargetMainIntensity); 
							Light2->BloomScale = bShowDiamondFlash ? 0.0f : 1.0f;
							Light2->SetLightFunctionMaterial(nullptr);
							if (EclipseLightFunctionMat)
							{
								FVector RealMoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
								EclipseLightFunctionMat->SetVectorParameterValue(FName("MoonPos"), RealMoonPos);
								EclipseLightFunctionMat->SetVectorParameterValue(FName("SunDir"), SunDirUnreal);
								EclipseLightFunctionMat->SetScalarParameterValue(FName("ShadowRadius"), 173740000.0);
							}
						}
						bool bCanSeeGroundShadows = false;
						APlayerController* PC_Shadows = UGameplayStatics::GetPlayerController(GetWorld(), 0);
						if (PC_Shadows)
						{
							ADronaMea* MyDrone = Cast<ADronaMea>(PC_Shadows->GetPawn());
							if (MyDrone && MyDrone->bMapModeActive)
							{
								bCanSeeGroundShadows = true;
							}
						}
						if (!bCanSeeGroundShadows)
						{
							ACesiumGeoreference* GeoRefShadows = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
							if (GeoRefShadows)
							{
								FVector LLH = GeoRefShadows->TransformUnrealPositionToLongitudeLatitudeHeight(PlayerPosUnreal);
								if (LLH.Z > 100000.0)
								{
									bCanSeeGroundShadows = true;
								}
							}
						}
						//aici actualizez umbra pe sol
						UpdateLiveUmbraTarget(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
						//bCanSeeGroundShadows = true;
						if (bShowPenumbraOnGround && bCanSeeGroundShadows)
						{
							//desenez penumbra pe sol (care este mare, are cateva mii de km la o eclipsa tipica)
							DrawPenumbraSurfaceMesh(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
						}
						else
						{
							//daca nu o afisez, sterg meshul
							if (SurfacePenumbraMesh)
							{
								SurfacePenumbraMesh->ClearAllMeshSections();
							}
						}
						//afisare con de penumbra
						if (bShowPenumbraConeFull)
						{
							DrawPenumbraCone3D(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);     
						}
						else
						{
							if (PenumbraProceduralMesh)
							{
								PenumbraProceduralMesh->ClearAllMeshSections();
							}
						}
						//afisare con de umbra (aici desenez umbra daca este eclipsa totala)
						//daca este eclipsa hibrida, are si antumbra
						if (bShowShadowConeFull)
						{
							DrawEclipseShadowCones(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
							DrawAntumbraCone3D(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
						}
						else
						{
							if (UmbraProceduralMesh)
							{
								UmbraProceduralMesh->ClearAllMeshSections();
							}
							if (AntumbraProceduralMesh) AntumbraProceduralMesh->ClearAllMeshSections();
						}
						if (bShowShadowOnGround && bCanSeeGroundShadows)
						{
							//desenare umbra pe sol, tipic are in jur de 250-300 km diametru
							DrawShadowSurfaceMesh(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
							DrawAntumbraSurfaceMesh(SunDirUnreal, DistSun_Cm, MoonDirUnreal, DistMoon_Cm, PlayerPosUnreal);
						}
						else
						{
							if (SurfaceShadowMesh)
							{
								SurfaceShadowMesh->ClearAllMeshSections();
							}
							if (SurfaceAntumbraMesh)
							{
								SurfaceAntumbraMesh->ClearAllMeshSections();
							}
						}
						//aici este partea pentru conurile de umbra si penumbra
						if (bShowEarthShadowUmbra && bCanSeeGroundShadows)
						{
							DrawEarthUmbraCone(SunDirUnreal, DistSun_Cm, PlayerPosUnreal);
						}
						else
						{
							if (EarthUmbraConeMesh) EarthUmbraConeMesh->ClearAllMeshSections();
						}

						if (bShowEarthShadowPenumbra && bCanSeeGroundShadows)
						{
							DrawEarthPenumbraCone(SunDirUnreal, DistSun_Cm, PlayerPosUnreal);
						}
						else
						{
							if (EarthPenumbraConeMesh) EarthPenumbraConeMesh->ClearAllMeshSections();
						}
					}
				}
				if (GEngine)
				{
					FColor TextColor = (Coverage > 0.9f) ? FColor::Red : FColor::Green;
					SunCoverage = Coverage * 100.0f;
				}
			}
		}
	}
}
//aici initializez socketul udp pe portul 5000
void AOrbitalManager::InitSocket()
{
	Socket = FUdpSocketBuilder(TEXT("EclipseSocket"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(5001) 
		.Build();
	if (Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDP Socket Initializat cu succes pe portul 5001!"));
	}
	RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	bool bIsValid;
	RemoteAddr->SetIp(TEXT("127.0.0.1"), bIsValid);
	RemoteAddr->SetPort(5000);
}
//la inchidere, nu las socketul in aer, ci il inchid
void AOrbitalManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
//aici citesc stelele din baza mea de date de la hyg, de 120.000 de stele
//de mentionat ca am pastrat doar stelele cu magnitudine sub 6, care sunt vizibile cu ochiul liber.
void AOrbitalManager::GenerateStarSphere()
{
	if (!StarsISMC || !StarsDataTable)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Avertisment: Tabelul CSV cu stele nu a fost setat in OrbitalManager!"));
		return;
	}
	StarsISMC->ClearInstances();
	StarPositionsMap.Empty();
	HUDStarLabels.Empty(); 
	//materialul m_star_real are un glow specific care se estompeaza pe margini (e similar cu stelele)
	UMaterialInterface* BaseStarMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Script/Engine.Material'/Game/Textures/M_StarReal.M_StarReal'"));
	if (BaseStarMaterial)
	{
		DynamicStarMaterial = UMaterialInstanceDynamic::Create(BaseStarMaterial, this);
		if (DynamicStarMaterial)
		{
			StarsISMC->SetMaterial(0, DynamicStarMaterial);
			//la asta, pot umbla in setari sa setez luminozitatea stelelor
			DynamicStarMaterial->SetScalarParameterValue(FName("StarGlowMultiplier"), CurrentStarGlowMultiplier);
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("EROARE: Nu pot gasi / incarca M_StarReal pentru materialul dinamic!"));
	}
	auto RowMap = StarsDataTable->GetRowMap();
	int32 ValidStarsCount = 0;
	for (auto& It : RowMap)
	{
		//extrag steaua din lista
		FString ActualStarName = It.Key.ToString().TrimStartAndEnd();
		FStarDataRow* Star = (FStarDataRow*)It.Value;
		if (!Star) continue;
		if (Star->Mag < -2.0) continue;
		float RA_Degrees = Star->RA * 15.0f;
		float Dec_Degrees = Star->Dec;
		float RaRads = FMath::DegreesToRadians(RA_Degrees); //extrag unghiurile
		float DecRads = FMath::DegreesToRadians(Dec_Degrees);
		float X = CelestialSphereRadius * FMath::Cos(DecRads) * FMath::Cos(RaRads);
		float Y = -CelestialSphereRadius * FMath::Cos(DecRads) * FMath::Sin(RaRads);
		float Z = CelestialSphereRadius * FMath::Sin(DecRads);
		//calculez coordonatele direct in coord Unreal
		FTransform StarTransform;
		StarTransform.SetLocation(FVector(X, Y, Z));
		StarPositionsMap.Add(ActualStarName, FVector(X, Y, Z));
		float ScaleBase = FMath::Clamp(1.0f - (Star->Mag / 6.0f), 1500.0f, 2000.0f) * Scaling;
		StarTransform.SetScale3D(FVector(ScaleBase));
		int32 InstanceIndex = StarsISMC->AddInstance(StarTransform);
		//setez iluminarea stelei in functie de stralucirea din tabel
		float BrightnessMultiplier = FMath::Pow(2.512f, -Star->Mag);
		BrightnessMultiplier = FMath::Clamp(BrightnessMultiplier, 0.0f, 20.0f);
		StarsISMC->SetCustomDataValue(InstanceIndex, 0, BrightnessMultiplier);
		float BV = Star->ColorIndex;
		float R = 1.0f, G = 1.0f, B = 1.0f;
		//setez culoarea stelei in functie de color index din tabel
		if (BV < 0.0f) { R = 0.5f; G = 0.7f; B = 1.0f; }
		else if (BV >= 0.0f && BV < 0.5f) { R = 1.0f; G = 0.9f; B = 0.8f; }
		else if (BV >= 0.5f && BV < 1.0f) { R = 1.0f; G = 0.7f; B = 0.4f; }
		else { R = 1.0f; G = 0.4f; B = 0.2f; }
		StarsISMC->SetCustomDataValue(InstanceIndex, 1, R);
		StarsISMC->SetCustomDataValue(InstanceIndex, 2, G);
		StarsISMC->SetCustomDataValue(InstanceIndex, 3, B);
		ValidStarsCount++;
		//Daca steaua are magnitudine sub 6, e foarte buna
		if (Star->Mag <= 6.0f && !ActualStarName.StartsWith(TEXT("Star_")))
		{
			FStarHUDLabel NewLabel;
			NewLabel.Name = ActualStarName;
			NewLabel.LocalPosition = FVector(X, Y, Z).GetSafeNormal();
			HUDStarLabels.Add(NewLabel);
		}
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("Sfera Cereasca generata! %d stele salvate."), ValidStarsCount));
	DrawConstellations();
}
//aici desenez constelatiile folosind ConstellationsDataTable
void AOrbitalManager::DrawConstellations()
{
	if (!ConstellationLinesISMC || !ConstellationsDataTable)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("EROARE: Componenta sau Tabelul de Constelatii lipsesc!"));
		return;
	}
	ConstellationLinesISMC->ClearInstances();
	static const FString ContextString(TEXT("ConstellationContext"));
	TArray<FConstellationLineRow*> AllLines;
	ConstellationsDataTable->GetAllRows<FConstellationLineRow>(ContextString, AllLines);
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Player) {
		ConstellationLinesISMC->SetWorldLocation(Player->GetActorLocation());
	}
	float CurrentFOV = 90.0f;
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC && PC->PlayerCameraManager)
	{
		CurrentFOV = PC->PlayerCameraManager->GetFOVAngle();
	}
	float FOVScalingFactor = CurrentFOV / 90.0f;
	FOVScalingFactor = FMath::Max(FOVScalingFactor, 0.001f);
	float DesiredThickness = 80000.0f * Scaling * FOVScalingFactor;
	//linia are grosime in functie de fov pentru un efect natural
	for (FConstellationLineRow* Line : AllLines)
	{
		if (!Line) continue;
		//extrag numele  stelelor de la fiecare capat
		FString CleanA = Line->StarA_Name.TrimStartAndEnd();
		FString CleanB = Line->StarB_Name.TrimStartAndEnd();
		if (StarPositionsMap.Contains(CleanA) && StarPositionsMap.Contains(CleanB))
		{
			FVector PosA = StarPositionsMap[CleanA];
			FVector PosB = StarPositionsMap[CleanB];
			//extrag punctul de mijloc, il rotesc corect si scalez linia
			FVector MidPoint = (PosA + PosB) * 0.5f;
			FRotator Rotation = FRotationMatrix::MakeFromZ(PosB - PosA).Rotator();
			float Distance = FVector::Dist(PosA, PosB);
			FVector Scale3D = FVector(DesiredThickness / 100.0f, DesiredThickness / 100.0f, Distance / 100.0f);
			int32 idx = ConstellationLinesISMC->AddInstance(FTransform(Rotation, MidPoint, Scale3D));
			ConstellationLinesISMC->SetCustomDataValue(idx, 0, 100.0f);
		}
	}
}

void AOrbitalManager::RebuildEclipsePathFromSaved()
{
	if (SavedEclipsePath.Num() < 2 || !EclipsePathMesh) return;

	ACesiumGeoreference* GeoRefPath = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRefPath) return;

	// Recalculeaza vertexii in noile coordonate Unreal
	TArray<FVector> UnrealPoints;
	for (const FVector& LonLatWidth : SavedEclipsePath)
	{
		FVector LLH(LonLatWidth.X, LonLatWidth.Y, 20000.0f);
		FVector UnrealPos = GeoRefPath->TransformLongitudeLatitudeHeightPositionToUnreal(LLH);
		UnrealPoints.Add(UnrealPos);
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;

	FVector EarthCenter = GeoRefPath->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);

	for (int32 i = 0; i < UnrealPoints.Num(); i++)
	{
		FVector Point = UnrealPoints[i];
		FVector Up = (Point - EarthCenter).GetSafeNormal();
		float HalfWidthCm = SavedEclipsePath[i].Z * 50000.0f;

		FVector Forward;
		if (i < UnrealPoints.Num() - 1)
			Forward = (UnrealPoints[i + 1] - Point).GetSafeNormal();
		else
			Forward = (Point - UnrealPoints[i - 1]).GetSafeNormal();

		FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();

		Vertices.Add(Point - Right * HalfWidthCm);
		Vertices.Add(Point + Right * HalfWidthCm);
		float U = (float)i / (float)(UnrealPoints.Num() - 1);
		UVs.Add(FVector2D(U, 0.0f));
		UVs.Add(FVector2D(U, 1.0f));
		Normals.Add(Up);
		Normals.Add(Up);
	}

	for (int32 i = 0; i < UnrealPoints.Num() - 1; i++)
	{
		int32 V0 = i * 2;
		Triangles.Add(V0); Triangles.Add(V0 + 1); Triangles.Add(V0 + 2);
		Triangles.Add(V0 + 1); Triangles.Add(V0 + 3); Triangles.Add(V0 + 2);
	}

	EclipsePathMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs,
		TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);

	if (EclipsePathMaterial)
		EclipsePathMesh->SetMaterial(0, EclipsePathMaterial);

	// Actualizeaza LastEarthCenter ca sa nu triggezi regenerare inutila din ReceiveDataFromPython
	LastEarthCenter = GeoRefPath->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
}
//aici pot afisa/ascunde stelele
void AOrbitalManager::SetStarsVisibility(bool bShowStars)
{
	if (StarsISMC)
	{
		StarsISMC->SetVisibility(bShowStars);
		if (GEngine)
		{
			FString Status = bShowStars ? TEXT("AFISATE") : TEXT("ASCUNSE");
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Stelele sunt acum: %s"), *Status));
		}
	}
}
//la constelatii, pot sa vad liniile constelatiilor sau nu, folosind fisierul config.ini
void AOrbitalManager::SetConstellationsVisibility(bool bShow)
{
	if (ConstellationLinesISMC)
	{
		ConstellationLinesISMC->SetVisibility(bShow);
		if (GEngine)
		{
			FString Status = bShow ? TEXT("AFISATE") : TEXT("ASCUNSE");
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Liniile constelatiilor sunt acum: %s"), *Status));
		}
	}
}
//vizibilitate nume stele
void AOrbitalManager::SetStarLabelsVisibility(bool bShow)
{
	bShowStarLabels = bShow;
	if (GEngine)
	{
		FString Status = bShow ? TEXT("AFISATE") : TEXT("ASCUNSE");
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Numele stelelor sunt acum: %s"), *Status));
	}
}
//vizibilitate semne zodiacale (luate de la Stellarium)
void AOrbitalManager::SetZodiacVisibility(bool bShow)
{
	if (ZodiacRoot)
	{
		ZodiacRoot->SetVisibility(bShow, true);
		if (GEngine)
		{
			FString Status = bShow ? TEXT("AFISATE") : TEXT("ASCUNSE");
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, FString::Printf(TEXT("Desenele Zodiilor sunt: %s"), *Status));
		}
	}
}
//setare multiplier pentru stralucire stele, in EclipseBrowserWidget setez valoarea de la 10 la 10.000
void AOrbitalManager::SetStarGlowMultiplier(float NewValue)
{
	CurrentStarGlowMultiplier = NewValue;
	if (DynamicStarMaterial)
	{
		DynamicStarMaterial->SetScalarParameterValue(FName("StarGlowMultiplier"), CurrentStarGlowMultiplier);
	}
	else if (StarsISMC)
	{
		UMaterialInterface* CurrentMat = StarsISMC->GetMaterial(0);
		if (CurrentMat)
		{
			DynamicStarMaterial = UMaterialInstanceDynamic::Create(CurrentMat, this);
			StarsISMC->SetMaterial(0, DynamicStarMaterial);
			DynamicStarMaterial->SetScalarParameterValue(FName("StarGlowMultiplier"), CurrentStarGlowMultiplier);
		}
	}
}
//actualizare umbra la suprafata
void AOrbitalManager::UpdateSurfaceShadow(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!SunSkyActor || !EclipseLightFunctionMat) return;
	UDirectionalLightComponent* LightComp = SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
	if (!LightComp) return;
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	FVector EarthCenter = GeoRef ? GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
	double EarthRadiusCm = 637813700.0;
	double EarthRadiusSq = EarthRadiusCm * EarthRadiusCm;
	FVector L = EarthCenter - MoonPos;
	double tca = FVector::DotProduct(L, LightDirection);
	double d2 = L.SizeSquared() - (tca * tca);
	bool bHitsEarthSurface = false;
	double DistanceToSurface = -1.0;
	if (d2 <= EarthRadiusSq)
	{
		double thc = FMath::Sqrt(EarthRadiusSq - d2);
		DistanceToSurface = tca - thc;
		if (DistanceToSurface > 0.0)
		{
			bHitsEarthSurface = true;
		}
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(600, 0.0f, FColor::White, TEXT("--- DEBUG LIGHT FUNCTION ---"));
	if (bHitsEarthSurface)
	{
		double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
		if (DistanceToSurface <= UmbraLengthCm)
		{
			double SurfaceUmbraRadiusCm = MoonRadiusCm * (1.0 - (DistanceToSurface / UmbraLengthCm));
			FVector ShadowHitLocation = MoonPos + (LightDirection * DistanceToSurface);
			if (GEngine)
			{
				//debugging
				GEngine->AddOnScreenDebugMessage(601, 0.0f, FColor::Green, TEXT("STATUS: ECLIPSA TOTALA (Atinge Solul)"));
				GEngine->AddOnScreenDebugMessage(602, 0.0f, FColor::Yellow, FString::Printf(TEXT("1. MoonPos (Locatie Sol): X:%.0f Y:%.0f Z:%.0f (KM)"), ShadowHitLocation.X / 100000.0, ShadowHitLocation.Y / 100000.0, ShadowHitLocation.Z / 100000.0));
				GEngine->AddOnScreenDebugMessage(603, 0.0f, FColor::Cyan, FString::Printf(TEXT("2. SunDir (Vector): X:%.3f Y:%.3f Z:%.3f"), LightDirection.X, LightDirection.Y, LightDirection.Z));
				GEngine->AddOnScreenDebugMessage(604, 0.0f, FColor::Orange, FString::Printf(TEXT("3. Raza Umbra: %.0f CM (%.1f KM)"), SurfaceUmbraRadiusCm, SurfaceUmbraRadiusCm / 100000.0));
				GEngine->AddOnScreenDebugMessage(605, 0.0f, FColor::Magenta, FString::Printf(TEXT("   Distanta Raza pana la Sol: %.0f KM | Lungime Fizica Con: %.0f KM"), DistanceToSurface / 100000.0, UmbraLengthCm / 100000.0));
			}
			EclipseLightFunctionMat->SetVectorParameterValue(FName("MoonPos"), ShadowHitLocation);
			EclipseLightFunctionMat->SetVectorParameterValue(FName("SunDir"), LightDirection);
			EclipseLightFunctionMat->SetScalarParameterValue(FName("ShadowRadius"), SurfaceUmbraRadiusCm);
			LightComp->SetLightFunctionMaterial(EclipseLightFunctionMat);
			LightComp->LightFunctionFadeDistance = 0.0f;
			LightComp->SetIntensity(110000.0f);
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(601, 0.0f, FColor::Red, TEXT("STATUS: ECLIPSA INELARA (Conul se termina in aer)"));
			LightComp->SetLightFunctionMaterial(nullptr);
		}
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(601, 0.0f, FColor::Red, TEXT("STATUS: RATEAZA PAMANTUL (In afara planetei)"));
		LightComp->SetLightFunctionMaterial(nullptr);
	}
}
//aici generez un con dinamic procedural adaptat nevoilor mele
void AOrbitalManager::GenerateDynamicCone(UProceduralMeshComponent* TargetMesh, FVector StartPos, FVector EndPos, float StartRadius, float EndRadius, int32 Segments, FVector EarthCenter, double EarthRadius)
{
	if (!TargetMesh) return;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	FVector Direction = (EndPos - StartPos).GetSafeNormal();
	FVector UpVector = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(Direction, UpVector)) > 0.999f)
	{
		UpVector = FVector::RightVector;
	}
	FVector RightVector = FVector::CrossProduct(Direction, UpVector).GetSafeNormal();
	UpVector = FVector::CrossProduct(RightVector, Direction).GetSafeNormal();
	float AngleStep = PI * 2.0f / Segments;
	double EarthRadiusSq = EarthRadius * EarthRadius;
	for (int32 i = 0; i <= Segments; i++)
	{
		float Angle = i * AngleStep;
		float CosA = FMath::Cos(Angle);
		float SinA = FMath::Sin(Angle);
		FVector RadialOffset = RightVector * CosA + UpVector * SinA;
		FVector RayStart = StartPos + RadialOffset * StartRadius;
		FVector TheoreticalEnd = EndPos + RadialOffset * EndRadius;
		FVector RayDir = (TheoreticalEnd - RayStart).GetSafeNormal();
		double TheoreticalDist = FVector::Distance(RayStart, TheoreticalEnd);
		double FinalDist = TheoreticalDist;
		if (EarthRadius > 0.0)
		{
			FVector L = EarthCenter - RayStart;
			double tca = FVector::DotProduct(L, RayDir);
			if (tca > 0.0) 
			{
				double d2 = L.SizeSquared() - (tca * tca);
				if (d2 <= EarthRadiusSq) 
				{
					double thc = FMath::Sqrt(EarthRadiusSq - d2);
					double t0 = tca - thc; 
					if (t0 > 0.0 && t0 < TheoreticalDist)
					{
						FinalDist = t0;
					}
				}
			}
		}
		Vertices.Add(RayStart);
		UVs.Add(FVector2D(static_cast<float>(i) / Segments, 0.0f));
		Normals.Add(RadialOffset);
		FVector ActualEnd = RayStart + (RayDir * FinalDist);
		Vertices.Add(ActualEnd);
		UVs.Add(FVector2D(static_cast<float>(i) / Segments, 1.0f));
		Normals.Add(RadialOffset);
	}
	int32 CenterStartIndex = Vertices.Add(StartPos);
	UVs.Add(FVector2D(0.5f, 0.5f));
	Normals.Add(-Direction);
	for (int32 i = 0; i < Segments; i++)
	{
		int32 BaseIndex = i * 2;
		Triangles.Add(BaseIndex);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 1);
		Triangles.Add(BaseIndex + 1);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex + 3);
		Triangles.Add(CenterStartIndex);
		Triangles.Add(BaseIndex + 2);
		Triangles.Add(BaseIndex);
	}
	TargetMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}
//aici desenez conul pt partea de totalitate
void AOrbitalManager::DrawEclipseShadowCones(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!UmbraProceduralMesh) return;
	UmbraProceduralMesh->ClearAllMeshSections();
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	FVector EarthCenter = GeoRef ? GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
	double EarthRadiusWithAtmosphereCm = 637813700.0 - 1050000.0; //10.5 km deasupra solului
	double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
	FVector UmbraTipPos = MoonPos + (LightDirection * UmbraLengthCm);
	if (GeoRef)
	{
		FVector TipLLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(UmbraTipPos);
		UmbraTipAltitudeKm = TipLLH.Z / 1000.0;
	}
	if (DynUmbraMaterial) UmbraProceduralMesh->SetMaterial(0, DynUmbraMaterial);
	else if (UmbraMaterial) UmbraProceduralMesh->SetMaterial(0, UmbraMaterial);
	GenerateDynamicCone(UmbraProceduralMesh, MoonPos, UmbraTipPos, MoonRadiusCm, 0.0f, 64, EarthCenter, EarthRadiusWithAtmosphereCm);
	if (GEngine)
	{
		double UmbraLengthKm = UmbraLengthCm / 100000.0;
		GEngine->AddOnScreenDebugMessage(203, 0.1f, FColor::Red, FString::Printf(TEXT("Lungime Con Umbra: %.0f km"), UmbraLengthKm));
	}
}
//desenare suprafata de contact intre conul (cel de totalitate) si sol
void AOrbitalManager::DrawShadowSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!SurfaceShadowMesh || !UmbraMaterial) return;
	SurfaceShadowMesh->ClearAllMeshSections();
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;
	FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
	FVector EarthPolarAxis = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
	FVector EarthEqAxisX = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
	FVector EarthEqAxisY = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 1, 0)).GetSafeNormal();
	//functie lambda pentru gasirea punctului exact de intersectie
	auto FindIntersectionExact = [&](const FVector& RayStart, const FVector& RayDir, FVector& OutHit, double& OutDist) -> bool
		{
			FVector3d RelStart = FVector3d(RayStart - EarthCenter) / 100000.0;
			FVector3d Dir = FVector3d(RayDir).GetSafeNormal();
			FVector3d O(FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(RelStart, FVector3d(EarthPolarAxis)));
			FVector3d D(FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(Dir, FVector3d(EarthPolarAxis)));
			double a_km = 6378.137;
			double c_km = 6356.752314;
			double A = (D.X * D.X) / (a_km * a_km) + (D.Y * D.Y) / (a_km * a_km) + (D.Z * D.Z) / (c_km * c_km);
			double B = 2.0 * ((O.X * D.X) / (a_km * a_km) + (O.Y * D.Y) / (a_km * a_km) + (O.Z * D.Z) / (c_km * c_km));
			double C = (O.X * O.X) / (a_km * a_km) + (O.Y * O.Y) / (a_km * a_km) + (O.Z * O.Z) / (c_km * c_km) - 1.0;
			double Discriminant = B * B - 4.0 * A * C;
			if (Discriminant >= 0.0)
			{
				double t = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
				OutDist = t * 100000.0;
				OutHit = RayStart + RayDir * OutDist;
				return true;
			}
			else
			{
				double t_closest = -B / (2.0 * A);
				FVector3d Closest_s = FVector3d(O.X / a_km, O.Y / a_km, O.Z / c_km) + FVector3d(D.X / a_km, D.Y / a_km, D.Z / c_km) * t_closest;
				Closest_s.Normalize();
				FVector3d Hit_km(Closest_s.X * a_km, Closest_s.Y * a_km, Closest_s.Z * c_km);
				FVector3d HitUnreal = FVector3d(EarthCenter) + (FVector3d(EarthEqAxisX) * Hit_km.X + FVector3d(EarthEqAxisY) * Hit_km.Y + FVector3d(EarthPolarAxis) * Hit_km.Z) * 100000.0;
				OutHit = FVector(HitUnreal);
				OutDist = FVector::Distance(RayStart, OutHit);
				return false; 
			}
		};
	//calcule pentru generare procedurala a suprafetei (aduce cu o elipsa, daca e la ecuator e aproape circulara)
	FVector HitLocationCenter;
	double DistanceToSurface;
	bool bCenterHitsEarth = FindIntersectionExact(MoonPos, LightDirection, HitLocationCenter, DistanceToSurface);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
	if (DistanceToSurface > UmbraLengthCm) return;
	FVector UpDir = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(LightDirection, UpDir)) > 0.99f) UpDir = FVector::RightVector;
	FVector RightVector = FVector::CrossProduct(LightDirection, UpDir).GetSafeNormal();
	FVector UpVectorCone = FVector::CrossProduct(RightVector, LightDirection).GetSafeNormal();
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	FVector NormalCenter = (HitLocationCenter - EarthCenter).GetSafeNormal();
	//adaugare punct central
	Vertices.Add(HitLocationCenter + NormalCenter * 5000000.0); 
	UVs.Add(FVector2D(0.5f, 0.5f));
	int32 Segments = 180; //180 de segmente pentru o reprezentare buna
	float AngleStep = PI * 2.0f / Segments;
	FVector UmbraTipPos = MoonPos + (LightDirection * UmbraLengthCm);
	bool bAtLeastOnePartOnEarth = bCenterHitsEarth;
	for (int32 i = 0; i <= Segments; i++)
	{
		//adaugare margini
		float Angle = i * AngleStep;
		FVector MoonEdge = MoonPos + (RightVector * FMath::Cos(Angle) * MoonRadiusCm) + (UpVectorCone * FMath::Sin(Angle) * MoonRadiusCm);
		FVector RayDir_i = (UmbraTipPos - MoonEdge).GetSafeNormal();
		FVector EdgeHit;
		double dist_i;
		bool bEdgeHits = FindIntersectionExact(MoonEdge, RayDir_i, EdgeHit, dist_i);
		if (bEdgeHits) bAtLeastOnePartOnEarth = true;
		FVector EdgeNormal = (EdgeHit - EarthCenter).GetSafeNormal();
		Vertices.Add(EdgeHit + EdgeNormal * 1500.0); 
		UVs.Add(FVector2D((FMath::Cos(Angle) + 1.0f) * 0.5f, (FMath::Sin(Angle) + 1.0f) * 0.5f));
		if (i > 0)
		{
			Triangles.Add(0);
			Triangles.Add(i);
			Triangles.Add(i + 1);
		}
	}
	//daca macar o zona este pe suprafata Pamantului, o desenez
	if (bAtLeastOnePartOnEarth)
	{
		SurfaceShadowMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		SurfaceShadowMesh->SetMaterial(0, DynUmbraMaterial);
	}
}
//aici generez elipsa umbrei
void AOrbitalManager::GenerateShadowEllipse(UProceduralMeshComponent* TargetMesh, FVector Center, FVector Normal, FVector LightDir, float Radius, float Stretch, int32 Segments)
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	FVector RightVector = FVector::CrossProduct(Normal, LightDir).GetSafeNormal();
	FVector ForwardVector = FVector::CrossProduct(RightVector, Normal).GetSafeNormal();
	Vertices.Add(Center); 
	UVs.Add(FVector2D(0.5f, 0.5f));
	float AngleStep = PI * 2.0f / Segments;
	//adaug raza, centru si vertices
	for (int32 i = 0; i <= Segments; i++)
	{
		float Angle = i * AngleStep;
		FVector Pos = Center +
			(RightVector * FMath::Cos(Angle) * Radius) +
			(ForwardVector * FMath::Sin(Angle) * Radius * Stretch);
		Vertices.Add(Pos);
		UVs.Add(FVector2D((FMath::Cos(Angle) + 1.0f) * 0.5f, (FMath::Sin(Angle) + 1.0f) * 0.5f));
		if (i > 0)
		{
			Triangles.Add(0);
			Triangles.Add(i);
			Triangles.Add(i + 1);
		}
	}
	TargetMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}
//desenare con de penumbra
void AOrbitalManager::DrawPenumbraSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!SurfacePenumbraMesh) return;
	if (!PenumbraMaterial && !DynPenumbraMaterial)
	{
		PenumbraMaterial = UmbraMaterial;
		if (!PenumbraMaterial) return;
	}
	//extrag pozitii pentru soare si luna
	SurfacePenumbraMesh->ClearAllMeshSections();
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	FVector EarthCenter = GeoRef ? GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
	FVector EarthPolarAxis = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
	FVector EarthEqAxisX = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
	FVector EarthEqAxisY = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 1, 0)).GetSafeNormal();
	//functie lambda pentru intersectia cu Pamantul (intersectie con-Pamanat, exact ca la conul de umbra
	auto FindIntersectionExact = [&](const FVector& RayStart, const FVector& RayDir, FVector& OutHit, double& OutDist) -> bool
		{
			FVector3d RelStart = FVector3d(RayStart - EarthCenter) / 100000.0;
			FVector3d Dir = FVector3d(RayDir).GetSafeNormal();
			FVector3d O(
				FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisX)),
				FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisY)),
				FVector3d::DotProduct(RelStart, FVector3d(EarthPolarAxis))
			);
			FVector3d D(
				FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisX)),
				FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisY)),
				FVector3d::DotProduct(Dir, FVector3d(EarthPolarAxis))
			);
			//raza ecuatoriala si respectiv polara a Pamantului
			double a_km = 6378.137;
			double c_km = 6356.752314;
			FVector3d Os(O.X / a_km, O.Y / a_km, O.Z / c_km);
			FVector3d Ds(D.X / a_km, D.Y / a_km, D.Z / c_km);
			double A = Ds.SizeSquared();
			double B = 2.0 * FVector3d::DotProduct(Os, Ds);
			double C = Os.SizeSquared() - 1.0;
			double Discriminant = B * B - 4.0 * A * C;
			if (Discriminant >= 0.0)
			{
				double t = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
				OutDist = t * 100000.0;
				OutHit = RayStart + RayDir * OutDist;
				return true;
			}
			else
			{
				double t_closest = -B / (2.0 * A);
				FVector3d Closest_s = Os + Ds * t_closest;
				Closest_s.Normalize();
				FVector3d Hit_km(Closest_s.X * a_km, Closest_s.Y * a_km, Closest_s.Z * c_km);
				FVector3d HitUnreal = FVector3d(EarthCenter) +
					(FVector3d(EarthEqAxisX) * Hit_km.X +
						FVector3d(EarthEqAxisY) * Hit_km.Y +
						FVector3d(EarthPolarAxis) * Hit_km.Z) * 100000.0;
				OutHit = FVector(HitUnreal);
				OutDist = FVector::Distance(RayStart, OutHit);
				return false;
			}
		};
	FVector HitLocationCenter;
	double DistanceToSurface;
	bool bCenterHitsEarth = FindIntersectionExact(MoonPos, LightDirection, HitLocationCenter, DistanceToSurface);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	double PenumbraVirtualLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm + MoonRadiusCm);
	FVector PenumbraVirtualTipPos = MoonPos - (LightDirection * PenumbraVirtualLengthCm);
	FVector UpDir = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(LightDirection, UpDir)) > 0.99f) UpDir = FVector::RightVector;
	FVector RightVector = FVector::CrossProduct(LightDirection, UpDir).GetSafeNormal();
	FVector UpVectorCone = FVector::CrossProduct(RightVector, LightDirection).GetSafeNormal();
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	//deoarece este o zona foarte mare, nu merge aceeasi abordare cu deseneare de elipsa curbata, deoarece ar trece prin Pamant
	//zona de penumbra este mult mai mare, de aceea folosesc inele
	int32 NumRings = 10;   
	int32 Segments = 72; 
	float AngleStep = PI * 2.0f / Segments;
	bool bAtLeastOnePartOnEarth = bCenterHitsEarth;
	for (int32 r = 0; r <= NumRings; r++)
	{
		float RingFrac = (float)r / (float)NumRings;
		double CurrentOffset = FMath::Lerp(15000000.0, 15000000.0, RingFrac);
		for (int32 s = 0; s <= Segments; s++)
		{
			float Angle = s * AngleStep;
			FVector CurrentMoonEdge = MoonPos + (RightVector * FMath::Cos(Angle) * MoonRadiusCm * RingFrac)
				+ (UpVectorCone * FMath::Sin(Angle) * MoonRadiusCm * RingFrac);
			FVector RayDir_i = (CurrentMoonEdge - PenumbraVirtualTipPos).GetSafeNormal();
			FVector EdgeHit;
			double dist_i;
			bool bEdgeHits = FindIntersectionExact(CurrentMoonEdge, RayDir_i, EdgeHit, dist_i);
			if (bEdgeHits) bAtLeastOnePartOnEarth = true;
			FVector EdgeNormal = (EdgeHit - EarthCenter).GetSafeNormal();
			Vertices.Add(EdgeHit + EdgeNormal * CurrentOffset);
			float U = 0.5f + (FMath::Cos(Angle) * 0.5f * RingFrac);
			float V = 0.5f + (FMath::Sin(Angle) * 0.5f * RingFrac);
			UVs.Add(FVector2D(U, V));
		}
	}
	for (int32 r = 0; r < NumRings; r++)
	{
		for (int32 s = 0; s < Segments; s++)
		{
			int32 Current = r * (Segments + 1) + s;
			int32 Next = Current + 1;
			int32 Above = (r + 1) * (Segments + 1) + s;
			int32 AboveNext = Above + 1;
			Triangles.Add(Current);
			Triangles.Add(Above);
			Triangles.Add(Next);
			Triangles.Add(Next);
			Triangles.Add(Above);
			Triangles.Add(AboveNext);
		}
	}
	//daca macar o parte este pe Pamant, o randez
	if (bAtLeastOnePartOnEarth)
	{
		SurfacePenumbraMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (DynPenumbraMaterial)
		{
			SurfacePenumbraMesh->SetMaterial(0, DynPenumbraMaterial);
		}
		else if (PenumbraMaterial)
		{
			SurfacePenumbraMesh->SetMaterial(0, PenumbraMaterial);
		}
	}
}
// aici desenez conul de penumbra
void AOrbitalManager::DrawPenumbraCone3D(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!PenumbraProceduralMesh) return;
	PenumbraProceduralMesh->ClearAllMeshSections();
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	FVector EarthCenter = GeoRef ? GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
	double EarthRadiusCm = 637813700.0;
	double EarthRadiusSq = EarthRadiusCm * EarthRadiusCm;
	FVector L = EarthCenter - MoonPos;
	double DistanceToEarthCenterPlane = FVector::DotProduct(L, LightDirection);
	double DistanceToRaySq = L.SizeSquared() - (DistanceToEarthCenterPlane * DistanceToEarthCenterPlane);
	// FIX 1: Ma asigur ca Pamantul este in fata Lunii (DistanceToEarthCenterPlane > 0)
	bool bHitsEarth = (DistanceToRaySq <= EarthRadiusSq) && (DistanceToEarthCenterPlane > 0);
	double FinalPenumbraLength = 0.0;
	// FIX 2: Daca Pamantul e in fata, trunchiem conul normal. Daca e in spate, il desenez adanc in spatiu.
	if (DistanceToEarthCenterPlane > 0)
	{
		double MaxDrawLength = DistanceToEarthCenterPlane + (EarthRadiusCm * 0.5);
		FinalPenumbraLength = bHitsEarth ? MaxDrawLength : (DistanceToEarthCenterPlane + EarthRadiusCm);
	}
	else
	{
		FinalPenumbraLength = 40000000000.0;
	}
	double PenumbraVirtualLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm + MoonRadiusCm);
	FVector PenumbraEndPos = MoonPos + (LightDirection * FinalPenumbraLength);
	double PenumbraEndRadiusCm = MoonRadiusCm * ((PenumbraVirtualLengthCm + FinalPenumbraLength) / PenumbraVirtualLengthCm);
	if (DynPenumbraMaterial)
	{
		PenumbraProceduralMesh->SetMaterial(0, DynPenumbraMaterial);
	}
	else if (PenumbraMaterial)
	{
		PenumbraProceduralMesh->SetMaterial(0, PenumbraMaterial);
	}
	GenerateDynamicCone(PenumbraProceduralMesh, MoonPos, PenumbraEndPos, MoonRadiusCm, PenumbraEndRadiusCm, 36);
}
//desenare antumbra
void AOrbitalManager::DrawAntumbraSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!SurfaceAntumbraMesh) return;
	SurfaceAntumbraMesh->ClearAllMeshSections();
	if (!AntumbraMaterial)
	{
		AntumbraMaterial = UmbraMaterial;
		if (!AntumbraMaterial) return;
	}
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;
	FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
	FVector EarthPolarAxis = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
	FVector EarthEqAxisX = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
	FVector EarthEqAxisY = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 1, 0)).GetSafeNormal();
	//aceeasi functie de intersectie ca si la umbra si penumbra
	auto FindIntersectionExact = [&](const FVector& RayStart, const FVector& RayDir, FVector& OutHit, double& OutDist) -> bool
		{
			FVector3d RelStart = FVector3d(RayStart - EarthCenter) / 100000.0;
			FVector3d Dir = FVector3d(RayDir).GetSafeNormal();
			FVector3d O(FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(RelStart, FVector3d(EarthPolarAxis)));
			FVector3d D(FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(Dir, FVector3d(EarthPolarAxis)));
			double a_km = 6378.137;
			double c_km = 6356.752314;
			double A = (D.X * D.X) / (a_km * a_km) + (D.Y * D.Y) / (a_km * a_km) + (D.Z * D.Z) / (c_km * c_km);
			double B = 2.0 * ((O.X * D.X) / (a_km * a_km) + (O.Y * D.Y) / (a_km * a_km) + (O.Z * D.Z) / (c_km * c_km));
			double C = (O.X * O.X) / (a_km * a_km) + (O.Y * O.Y) / (a_km * a_km) + (O.Z * O.Z) / (c_km * c_km) - 1.0;
			double Discriminant = B * B - 4.0 * A * C;
			if (Discriminant >= 0.0)
			{
				double t = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
				OutDist = t * 100000.0;
				OutHit = RayStart + RayDir * OutDist;
				return true;
			}
			else
			{
				double t_closest = -B / (2.0 * A);
				FVector3d Closest_s = FVector3d(O.X / a_km, O.Y / a_km, O.Z / c_km) + FVector3d(D.X / a_km, D.Y / a_km, D.Z / c_km) * t_closest;
				Closest_s.Normalize();
				FVector3d Hit_km(Closest_s.X * a_km, Closest_s.Y * a_km, Closest_s.Z * c_km);
				FVector3d HitUnreal = FVector3d(EarthCenter) + (FVector3d(EarthEqAxisX) * Hit_km.X + FVector3d(EarthEqAxisY) * Hit_km.Y + FVector3d(EarthPolarAxis) * Hit_km.Z) * 100000.0;
				OutHit = FVector(HitUnreal);
				OutDist = FVector::Distance(RayStart, OutHit);
				return false;
			}
		};
	FVector HitLocationCenter;
	double DistanceToSurface;
	bool bCenterHitsEarth = FindIntersectionExact(MoonPos, LightDirection, HitLocationCenter, DistanceToSurface);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
	if (UmbraLengthCm >= DistanceToSurface)
	{
		return;
	}
	//fac aici calcule pentru vedea right vector si up vector
	FVector UpDir = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(LightDirection, UpDir)) > 0.99f) UpDir = FVector::RightVector;
	FVector RightVector = FVector::CrossProduct(LightDirection, UpDir).GetSafeNormal();
	FVector UpVectorCone = FVector::CrossProduct(RightVector, LightDirection).GetSafeNormal();
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector2D> UVs;
	FVector NormalCenter = (HitLocationCenter - EarthCenter).GetSafeNormal();
	Vertices.Add(HitLocationCenter + NormalCenter * 5000000.0); 
	UVs.Add(FVector2D(0.5f, 0.5f));
	int32 Segments = 360; 
	float AngleStep = PI * 2.0f / Segments;
	FVector UmbraTipPos = MoonPos + (LightDirection * UmbraLengthCm);
	bool bAtLeastOnePartOnEarth = bCenterHitsEarth;
	//generare de segmente
	for (int32 i = 0; i <= Segments; i++)
	{
		float Angle = i * AngleStep;
		FVector MoonEdge = MoonPos + (RightVector * FMath::Cos(Angle) * MoonRadiusCm) + (UpVectorCone * FMath::Sin(Angle) * MoonRadiusCm);
		FVector RayDir_i = (UmbraTipPos - MoonEdge).GetSafeNormal();
		FVector EdgeHit;
		double dist_i;
		bool bEdgeHits = FindIntersectionExact(MoonEdge, RayDir_i, EdgeHit, dist_i);
		if (bEdgeHits) bAtLeastOnePartOnEarth = true;
		FVector EdgeNormal = (EdgeHit - EarthCenter).GetSafeNormal();
		Vertices.Add(EdgeHit + EdgeNormal * 2000.0); 
		UVs.Add(FVector2D((FMath::Cos(Angle) + 1.0f) * 0.5f, (FMath::Sin(Angle) + 1.0f) * 0.5f));
		if (i > 0)
		{
			//adaug vertecsii triunghiurilor
			Triangles.Add(0);
			Triangles.Add(i + 1);
			Triangles.Add(i);
		}
	}
	if (bAtLeastOnePartOnEarth)
	{
		SurfaceAntumbraMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		SurfaceAntumbraMesh->SetMaterial(0, DynAntumbraMaterial);
	}
}

// Deseneaza doar conul de UMBRA propriu al Pamantului (zona unde Soarele e complet blocat).
// Cand Luna intra aici, avem eclipsa TOTALA de Luna.
void AOrbitalManager::DrawEarthUmbraCone(FVector SunDirUnreal, double DistSun_Cm, FVector PlayerPosUnreal)
{
	if (!EarthUmbraConeMesh) return;
	EarthUmbraConeMesh->ClearAllMeshSections();

	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;

	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);

	//directia in care se intinde umbra: de la Soare, prin centrul Pamantului, mai departe in spatiu
	FVector ShadowDirection = (EarthCenter - SunPos).GetSafeNormal();
	double RealDistSunEarthCm = FVector::Distance(SunPos, EarthCenter);

	const double EarthRadiusCm = 637813700.0;

	//aceeasi formula ca la umbra Lunii, dar cu raza Pamantului
	double EarthUmbraLengthCm = (EarthRadiusCm * RealDistSunEarthCm) / (SunRadiusCm - EarthRadiusCm);
	FVector EarthUmbraTipPos = EarthCenter + (ShadowDirection * EarthUmbraLengthCm);

	if (DynEarthUmbraMaterial) EarthUmbraConeMesh->SetMaterial(0, DynEarthUmbraMaterial);
	else if (EarthUmbraMaterial) EarthUmbraConeMesh->SetMaterial(0, EarthUmbraMaterial);

	GenerateDynamicCone(EarthUmbraConeMesh, EarthCenter, EarthUmbraTipPos, EarthRadiusCm, 0.0f, 64);
}

// Deseneaza doar conul de PENUMBRA propriu al Pamantului (intunecare partiala, mult mai larga).
// Cand Luna intra aici (fara sa fie si in umbra), avem eclipsa PENUMBRALA de Luna.
void AOrbitalManager::DrawEarthPenumbraCone(FVector SunDirUnreal, double DistSun_Cm, FVector PlayerPosUnreal)
{
	if (!EarthPenumbraConeMesh) return;
	EarthPenumbraConeMesh->ClearAllMeshSections();

	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;

	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);

	FVector ShadowDirection = (EarthCenter - SunPos).GetSafeNormal();
	double RealDistSunEarthCm = FVector::Distance(SunPos, EarthCenter);

	const double EarthRadiusCm = 637813700.0;

	//varful virtual al penumbrei este INAINTEA Pamantului (intre Soare si Pamant)
	double EarthPenumbraVirtualLengthCm = (EarthRadiusCm * RealDistSunEarthCm) / (SunRadiusCm + EarthRadiusCm);

	//desenam penumbra pe o distanta care sa acopere confortabil orbita Lunii (~3x distanta medie Pamant-Luna)
	//const double PenumbraDrawLengthCm = 384400.0 * 100000.0 * 3.0;
	//double EarthPenumbraVirtualLengthCm = (EarthRadiusCm * RealDistSunEarthCm) / (SunRadiusCm + EarthRadiusCm);

	//penumbra se deseneaza pe aceeasi lungime ca umbra, ca sa se termine amandoua in acelasi loc
	const double EarthUmbraLengthCm = (EarthRadiusCm * RealDistSunEarthCm) / (SunRadiusCm - EarthRadiusCm);
	const double MinDrawLengthCm = 384400.0 * 100000.0 * 3.0; //minim: ~3x distanta Pamant-Luna
	const double PenumbraDrawLengthCm = FMath::Max(EarthUmbraLengthCm, MinDrawLengthCm);

	FVector EarthPenumbraEndPos = EarthCenter + (ShadowDirection * PenumbraDrawLengthCm);
	double EarthPenumbraEndRadiusCm = EarthRadiusCm *
		((EarthPenumbraVirtualLengthCm + PenumbraDrawLengthCm) / EarthPenumbraVirtualLengthCm);

	if (DynEarthPenumbraMaterial) EarthPenumbraConeMesh->SetMaterial(0, DynEarthPenumbraMaterial);
	else if (EarthPenumbraMaterial) EarthPenumbraConeMesh->SetMaterial(0, EarthPenumbraMaterial);

	GenerateDynamicCone(EarthPenumbraConeMesh, EarthCenter, EarthPenumbraEndPos, EarthRadiusCm, EarthPenumbraEndRadiusCm, 48);
}

//aici desenez conul de antumbra, ca si la celelalte
void AOrbitalManager::DrawAntumbraCone3D(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	if (!AntumbraProceduralMesh) return;
	AntumbraProceduralMesh->ClearAllMeshSections();
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	FVector EarthCenter = GeoRef ? GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector) : FVector::ZeroVector;
	double EarthRadiusWithAtmosphereCm = 637813700.0 - 1050000.0;
	double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
	FVector L = EarthCenter - MoonPos;
	double DistToEarthSurface = L.Size() - 637813700.0;
	if (UmbraLengthCm >= DistToEarthSurface)
	{
		return;
	}
	FVector AntumbraStartPos = MoonPos + (LightDirection * UmbraLengthCm);
	double DistanceToEarthCenterPlane = FVector::DotProduct(L, LightDirection);
	double AntumbraDrawLength = (DistanceToEarthCenterPlane + 500000000.0) - UmbraLengthCm;
	if (AntumbraDrawLength <= 0) return;
	FVector AntumbraEndPos = AntumbraStartPos + (LightDirection * AntumbraDrawLength);
	double AntumbraEndRadius = AntumbraDrawLength * (MoonRadiusCm / UmbraLengthCm);
	if (DynAntumbraMaterial)
	{
		AntumbraProceduralMesh->SetMaterial(0, DynAntumbraMaterial);
	}
	else if (AntumbraMaterial)
	{
		AntumbraProceduralMesh->SetMaterial(0, AntumbraMaterial);
	}
	GenerateDynamicCone(AntumbraProceduralMesh, AntumbraStartPos, AntumbraEndPos, 0.0f, AntumbraEndRadius, 64, EarthCenter, EarthRadiusWithAtmosphereCm);
}
//aici generez numele pentru constelatii cu pozitiile lor relative, cu tot cu scale si roll (cat de mult sunt rotite)
//sunt de ajuns ra si dec, deoarece le pun pe suprafata unei sfere uriase
void AOrbitalManager::GenerateConstellationLabels()
{
	HUDConstellationLabels.Empty();
	struct FConstCenter {
		FString Name;
		float RA_Hours;   
		float Dec_Deg;    
		float ScaleMult;  
		float RollOffset; 
	};
	FConstCenter Constellations[88] = {
		{TEXT("Andromeda"), 0.8f, 41.5f, 1.0f, 0.0f}, {TEXT("Antlia"), 10.1f, -32.5f, 1.0f, 0.0f}, {TEXT("Apus"), 16.1f, -75.3f, 1.0f, 0.0f},
		{TEXT("Aquarius"), 22.3f, -11.4f, 1.0f, 0.0f}, {TEXT("Aquila"), 19.7f, 3.4f, 1.0f, 0.0f}, {TEXT("Ara"), 17.4f, -56.2f, 1.0f, 0.0f},
		{TEXT("Aries"), 2.6f, 20.8f, 1.0f, 0.0f}, {TEXT("Auriga"), 6.0f, 42.0f, 1.0f, 0.0f}, {TEXT("Bootes"), 14.7f, 31.2f, 1.0f, 0.0f},
		{TEXT("Caelum"), 4.7f, -37.9f, 1.0f, 0.0f}, {TEXT("Camelopardalis"), 8.9f, 69.4f, 1.0f, 0.0f}, {TEXT("Cancer"), 8.4f, 20.0f, 1.0f, 0.0f},
		{TEXT("Canes Venatici"), 13.1f, 40.1f, 1.0f, 0.0f}, {TEXT("Canis Major"), 6.8f, -22.1f, 1.0f, 0.0f}, {TEXT("Canis Minor"), 7.6f, 6.4f, 1.0f, 0.0f},
		{TEXT("Capricornus"), 21.0f, -18.0f, 1.0f, 0.0f}, {TEXT("Carina"), 9.0f, -60.5f, 1.0f, 0.0f}, {TEXT("Cassiopeia"), 1.3f, 62.2f, 1.0f, 0.0f},
		{TEXT("Centaurus"), 13.0f, -47.3f, 1.0f, 0.0f}, {TEXT("Cepheus"), 22.0f, 71.0f, 1.0f, 0.0f}, {TEXT("Cetus"), 1.4f, -7.2f, 1.0f, 0.0f},
		{TEXT("Chamaeleon"), 10.7f, -79.2f, 1.0f, 0.0f}, {TEXT("Circinus"), 15.3f, -63.0f, 1.0f, 0.0f}, {TEXT("Columba"), 5.8f, -35.1f, 1.0f, 0.0f},
		{TEXT("Coma Berenices"), 12.8f, 23.3f, 1.0f, 0.0f}, {TEXT("Corona Australis"), 18.6f, -41.1f, 1.0f, 0.0f}, {TEXT("Corona Borealis"), 15.8f, 32.6f, 1.0f, 0.0f},
		{TEXT("Corvus"), 12.4f, -18.4f, 1.0f, 0.0f}, {TEXT("Crater"), 11.4f, -15.9f, 1.0f, 0.0f}, {TEXT("Crux"), 12.4f, -60.2f, 1.0f, 0.0f},
		{TEXT("Cygnus"), 20.6f, 44.5f, 1.0f, 0.0f}, {TEXT("Delphinus"), 20.7f, 11.7f, 1.0f, 0.0f}, {TEXT("Dorado"), 5.2f, -59.4f, 1.0f, 0.0f},
		{TEXT("Draco"), 17.3f, 61.8f, 1.0f, 0.0f}, {TEXT("Equuleus"), 21.2f, 7.8f, 1.0f, 0.0f}, {TEXT("Eridanus"), 3.3f, -29.6f, 1.0f, 0.0f},
		{TEXT("Fornax"), 2.8f, -31.6f, 1.0f, 0.0f}, {TEXT("Gemini"), 7.1f, 22.6f, 1.0f, 0.0f}, {TEXT("Grus"), 22.5f, -46.9f, 1.0f, 0.0f},
		{TEXT("Hercules"), 17.3f, 30.0f, 1.0f, 0.0f}, {TEXT("Horologium"), 3.3f, -53.3f, 1.0f, 0.0f}, {TEXT("Hydra"), 10.6f, -14.4f, 1.0f, 0.0f},
		{TEXT("Hydrus"), 2.3f, -70.4f, 1.0f, 0.0f}, {TEXT("Indus"), 21.0f, -59.7f, 1.0f, 0.0f}, {TEXT("Lacerta"), 22.5f, 46.1f, 1.0f, 0.0f},
		{TEXT("Leo"), 10.7f, 13.1f, 1.0f, 0.0f}, {TEXT("Leo Minor"), 10.2f, 32.1f, 1.0f, 0.0f}, {TEXT("Lepus"), 5.6f, -19.0f, 1.0f, 0.0f},
		{TEXT("Libra"), 15.2f, -15.2f, 1.0f, 0.0f}, {TEXT("Lupus"), 15.3f, -42.8f, 1.0f, 0.0f}, {TEXT("Lynx"), 8.0f, 47.5f, 1.0f, 0.0f},
		{TEXT("Lyra"), 18.9f, 36.8f, 1.0f, 0.0f}, {TEXT("Mensa"), 5.4f, -77.5f, 1.0f, 0.0f}, {TEXT("Microscopium"), 21.0f, -36.3f, 1.0f, 0.0f},
		{TEXT("Monoceros"), 7.1f, -5.9f, 1.0f, 0.0f}, {TEXT("Musca"), 12.4f, -70.1f, 1.0f, 0.0f}, {TEXT("Norma"), 15.9f, -51.3f, 1.0f, 0.0f},
		{TEXT("Octans"), 21.0f, -82.0f, 1.0f, 0.0f}, {TEXT("Ophiuchus"), 17.4f, 7.9f, 1.0f, 0.0f}, {TEXT("Orion"), 5.6f, 5.9f, 1.0f, 0.0f},
		{TEXT("Pavo"), 19.6f, -65.8f, 1.0f, 0.0f}, {TEXT("Pegasus"), 22.7f, 19.4f, 1.0f, 0.0f}, {TEXT("Perseus"), 3.2f, 45.0f, 1.0f, 0.0f},
		{TEXT("Phoenix"), 1.2f, -50.8f, 1.0f, 0.0f}, {TEXT("Pictor"), 5.7f, -53.4f, 1.0f, 0.0f}, {TEXT("Pisces"), 0.5f, 13.7f, 1.0f, 0.0f},
		{TEXT("Piscis Austrinus"), 22.3f, -30.6f, 1.0f, 0.0f}, {TEXT("Puppis"), 7.6f, -31.2f, 1.0f, 0.0f}, {TEXT("Pyxis"), 8.9f, -27.4f, 1.0f, 0.0f},
		{TEXT("Reticulum"), 3.9f, -60.0f, 1.0f, 0.0f}, {TEXT("Sagitta"), 19.6f, 18.8f, 1.0f, 0.0f}, {TEXT("Sagittarius"), 19.1f, -25.8f, 1.0f, 0.0f},
		{TEXT("Scorpius"), 16.9f, -30.7f, 1.0f, 0.0f}, {TEXT("Sculptor"), 0.4f, -32.1f, 1.0f, 0.0f}, {TEXT("Scutum"), 18.7f, -10.1f, 1.0f, 0.0f},
		{TEXT("Serpens"), 16.9f, 1.2f, 1.0f, 0.0f}, {TEXT("Sextans"), 10.3f, -2.6f, 1.0f, 0.0f}, {TEXT("Taurus"), 4.7f, 14.9f, 1.0f, 0.0f},
		{TEXT("Telescopium"), 19.3f, -51.0f, 1.0f, 0.0f}, {TEXT("Triangulum"), 2.1f, 31.5f, 1.0f, 0.0f}, {TEXT("Triangulum Australe"), 16.1f, -65.4f, 1.0f, 0.0f},
		{TEXT("Tucana"), 23.8f, -65.8f, 1.0f, 0.0f}, {TEXT("Ursa Major"), 11.3f, 50.7f, 1.0f, 0.0f}, {TEXT("Ursa Minor"), 15.0f, 77.5f, 1.0f, 0.0f},
		{TEXT("Vela"), 9.4f, -47.1f, 1.0f, 0.0f}, {TEXT("Virgo"), 13.4f, -4.2f, 1.0f, 0.0f}, {TEXT("Volans"), 8.0f, -70.5f, 1.0f, 0.0f},
		{TEXT("Vulpecula"), 20.2f, 24.4f, 1.0f, 0.0f}
	};
	//sunt 88 de constelatii
	for (int32 i = 0; i < 88; i++)
	{
		float RA_Degrees = Constellations[i].RA_Hours * 15.0f;
		float Dec_Degrees = Constellations[i].Dec_Deg;
		float RaRads = FMath::DegreesToRadians(RA_Degrees);
		float DecRads = FMath::DegreesToRadians(Dec_Degrees);
		float X = FMath::Cos(DecRads) * FMath::Cos(RaRads);
		float Y = -FMath::Cos(DecRads) * FMath::Sin(RaRads);
		float Z = FMath::Sin(DecRads);
		//calculez x,y,z direct ca coordonate Unreal
		FConstellationHUDLabel NewLabel;
		NewLabel.Name = Constellations[i].Name;
		NewLabel.LocalPosition = FVector(X, Y, Z).GetSafeNormal();
		HUDConstellationLabels.Add(NewLabel);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, TEXT("88 Constelatii IAU incarcate si calculate!"));
	}
	for (UStaticMeshComponent* Comp : ZodiacMeshes)
	{
		if (Comp) Comp->DestroyComponent();
	}
	ZodiacMeshes.Empty();
	//fiecare zodie are un plan in spate pe care desenez
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	int32 ImaginiIncarcate = 0;
	for (int32 i = 0; i < HUDConstellationLabels.Num(); i++)
	{
		const FConstellationHUDLabel& ConstData = HUDConstellationLabels[i];
		const FConstCenter& ConstInfo = Constellations[i]; 
		FString FormattedName = ConstData.Name.ToLower().Replace(TEXT(" "), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
		FString TexturePath = FString::Printf(TEXT("/Script/Engine.Texture2D'/Game/Textures/Zodii/%s.%s'"), *FormattedName, *FormattedName);
		UTexture2D* ConstTexture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (!ConstTexture) continue;
		FVector Pos = ConstData.LocalPosition * CelestialSphereRadius;
		UStaticMeshComponent* ZodiacComp = NewObject<UStaticMeshComponent>(this);
		ZodiacComp->SetupAttachment(ZodiacRoot); //adaug constelatiile rand pe rand la zodiac root
		ZodiacComp->RegisterComponent();
		ZodiacComp->SetStaticMesh(PlaneMesh);
		ZodiacComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); //fara coliziuni
		ZodiacComp->SetCastShadow(false);
		ZodiacComp->bReceivesDecals = false;
		ZodiacComp->SetBoundsScale(10000.0f);
		ZodiacComp->SetTranslucentSortPriority(-101);
		ZodiacComp->SetRelativeLocation(Pos);
		FVector DirToEarth = (FVector::ZeroVector - Pos).GetSafeNormal();
		FRotator BaseLookAt = DirToEarth.Rotation();
		//folosesc quaternioni pentru rotatii mai precise
		FQuat FinalRotQuat = BaseLookAt.Quaternion() * FRotator(-90.0f, 0.0f, ConstInfo.RollOffset).Quaternion();
		ZodiacComp->SetRelativeRotation(FinalRotQuat);
		float BaseScale = (CelestialSphereRadius / 100.0f) * 0.30f * ConstInfo.ScaleMult;
		ZodiacComp->SetRelativeScale3D(FVector(BaseScale,BaseScale,BaseScale));
		//setez materialul zodiacmaster
		if (ZodiacMasterMaterial)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(ZodiacMasterMaterial, this);
			DynMat->SetTextureParameterValue(FName("ArtTexture"), ConstTexture);
			ZodiacComp->SetMaterial(0, DynMat);
		}
		ZodiacComp->SetVisibility(false);
		ZodiacMeshes.Add(ZodiacComp);
		ImaginiIncarcate++;
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT(">> S-au incarcat perfect %d / 88 desene! <<"), ImaginiIncarcate));
	if (ZodiacRoot) ZodiacRoot->SetVisibility(false, true);
}
//functie care imi deplaseaza umbra pe pamant
void AOrbitalManager::UpdateLiveUmbraTarget(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal)
{
	bIsUmbraOnGround = false; 
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef) return;
	FVector SunPos = PlayerPosUnreal + (SunDirUnreal * DistSun_Cm);
	FVector MoonPos = PlayerPosUnreal + (MoonDirUnreal * DistMoon_Cm);
	FVector LightDirection = (MoonPos - SunPos).GetSafeNormal();
	FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
	FVector EarthPolarAxis = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
	FVector EarthEqAxisX = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(1, 0, 0)).GetSafeNormal();
	FVector EarthEqAxisY = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 1, 0)).GetSafeNormal();
	//verific daca umbra intersecteaza solul vreun pic, folosind raze
	auto FindIntersectionExact = [&](const FVector& RayStart, const FVector& RayDir, FVector& OutHit, double& OutDist) -> bool
		{
			FVector3d RelStart = FVector3d(RayStart - EarthCenter) / 100000.0;
			FVector3d Dir = FVector3d(RayDir).GetSafeNormal();
			FVector3d O(FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(RelStart, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(RelStart, FVector3d(EarthPolarAxis)));
			FVector3d D(FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisX)), FVector3d::DotProduct(Dir, FVector3d(EarthEqAxisY)), FVector3d::DotProduct(Dir, FVector3d(EarthPolarAxis)));
			double a_km = 6378.137;
			double c_km = 6356.752314;
			double A = (D.X * D.X) / (a_km * a_km) + (D.Y * D.Y) / (a_km * a_km) + (D.Z * D.Z) / (c_km * c_km);
			double B = 2.0 * ((O.X * D.X) / (a_km * a_km) + (O.Y * D.Y) / (a_km * a_km) + (O.Z * D.Z) / (c_km * c_km));
			double C = (O.X * O.X) / (a_km * a_km) + (O.Y * O.Y) / (a_km * a_km) + (O.Z * O.Z) / (c_km * c_km) - 1.0;
			double Discriminant = B * B - 4.0 * A * C;
			if (Discriminant >= 0.0)
			{
				double t = (-B - FMath::Sqrt(Discriminant)) / (2.0 * A);
				OutDist = t * 100000.0;
				OutHit = RayStart + RayDir * OutDist;
				return true;
			}
			return false;
		};
	//initializare variabile pentru calcule
	FVector HitLocationCenter;
	double DistanceToSurface;
	bool bCenterHitsEarth = FindIntersectionExact(MoonPos, LightDirection, HitLocationCenter, DistanceToSurface);
	double RealDistSunMoonCm = FVector::Distance(SunPos, MoonPos);
	double UmbraLengthCm = (MoonRadiusCm * RealDistSunMoonCm) / (SunRadiusCm - MoonRadiusCm);
	FVector TargetPosUnreal;
	if (bCenterHitsEarth && DistanceToSurface <= UmbraLengthCm)
	{
		//umbra este pe sol
		bIsUmbraOnGround = true; 
		TargetPosUnreal = HitLocationCenter; 
	}
	else
	{
		bIsUmbraOnGround = false; 
		double TargetDist = FMath::Max(0.0, UmbraLengthCm - 10000000.0);
		TargetPosUnreal = MoonPos + (LightDirection * TargetDist);
	}
	LiveUmbraCenterLLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(TargetPosUnreal);
}
//functie simpla care primeste 2 stringuri, primul in romana si al doilea in engleza si verifica in config.ini sa vada care limba este corecta, returnand stringul bun
//este echivalentul unui multiplexor 2 la 1, ca sa evit  multe ifuri inutile
FText AOrbitalManager::Tr(const FString& TextRO, const FString& TextEN)
{
	FString CurrentLang = TEXT("Romana"); 
	if (GConfig)
	{
		GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), CurrentLang, GGameUserSettingsIni);
	}
	CurrentLang.TrimStartAndEndInline();
	if (CurrentLang == TEXT("English"))
	{
		return FText::FromString(TextEN);
	}
	return FText::FromString(TextRO);
}
//aceasta functie nu face trim, in rest e la fel ca si cea de mai sus
FString AOrbitalManager::TrString(const FString& TextRO, const FString& TextEN)
{
	FString CurrentLang = TEXT("Romana"); 
	if (GConfig)
	{
		GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), CurrentLang, GGameUserSettingsIni);
	}
	if (CurrentLang == TEXT("English"))
	{
		return TextEN;
	}
	return TextRO;
}

#include "Moon.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "CesiumGeoreference.h"
#include "OrbitalManager.h"
#include "Materials/MaterialInstanceDynamic.h" 
AMoon::AMoon()
{
	PrimaryActorTick.bCanEverTick = true;
	MoonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoonMesh"));
	RootComponent = MoonMesh;
	//mi-am creat propria mea sfera buna pentru luna, varianta default avea doar 544 de vertecsi si parea colturoasa la nivel planetar (luna avea colturi)
	//am facut o sfera mult mai high-poly pentru nevoile mele
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Script/Engine.StaticMesh'/Game/Textures/Sphere_perfect.Sphere_perfect'"));
	if (SphereAsset.Succeeded())
	{
		MoonMesh->SetStaticMesh(SphereAsset.Object);
	}
	//folosesc un material deosebit pentru luna
	//partea intunecata este amestecata cu nuanta cerului, adica practic vad doar partea luminoasa
	//pot vedea bine fazele lunii de pe pamant
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatAsset(TEXT("/Script/Engine.Material'/Game/Textures/M_Luna.M_Luna'"));
	if (MatAsset.Succeeded())
	{
		MoonMaterial = MatAsset.Object;
	}
	MoonMesh->SetMobility(EComponentMobility::Movable);
	MoonMesh->SetCastShadow(false);
	MoonMesh->bCastDynamicShadow = false;
	MoonMesh->bCastFarShadow = false;
	MoonMesh->bCastHiddenShadow = false;
	MoonMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	MoonMesh->SetBoundsScale(10000.0f);
}
void AMoon::BeginPlay()
{
	Super::BeginPlay();
	if (MoonMaterial)
	{
		UMaterialInstanceDynamic* DynMaterial = UMaterialInstanceDynamic::Create(MoonMaterial, this);
		if (DynMaterial)
		{
			MoonMesh->SetMaterial(0, DynMaterial);
		}
	}
	if (!GeoRef)
	{
		AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), ACesiumGeoreference::StaticClass());
		GeoRef = Cast<ACesiumGeoreference>(FoundActor);
	}
	double MoonRadiusCM = 1737400.0 * 100.0;
	double MeshRadiusCM = 50.0;
	double Scale = MoonRadiusCM / MeshRadiusCM;
	MoonMesh->SetWorldScale3D(FVector(Scale));
	MoonMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
}
//aici activez modul eclipsa, inainte sa fi avut optiunea de a folosi python si calcule in skyfield, puneam din cpp luna peste soare pentru a incerca sa simulez o eclipsa
void AMoon::ToggleEclipseMode()
{
	bEclipseModeActive = !bEclipseModeActive;
	bShowAlignmentDebug = bEclipseModeActive;
	if (bEclipseModeActive)
	{
		TArray<AActor*> AllActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
		for (AActor* Actor : AllActors)
		{
			if (Actor && Actor->GetName().Contains("CesiumSunSky"))
			{
				UActorComponent* LightComp = Actor->FindComponentByClass<ULightComponent>();
				if (LightComp)
				{
					SunLightSource = Actor; 
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Eclipsa: Soare Cesium gasit!"));
					return;
				}
			}
		}
		TArray<AActor*> DirLights;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADirectionalLight::StaticClass(), DirLights);
		if (DirLights.Num() > 0)
		{
			SunLightSource = DirLights[0];
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Eclipsa: DirectionalLight detectat!"));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("EROARE: Nu am gasit nicio sursa de lumina!"));
			bEclipseModeActive = false;
		}
	}
}
void AMoon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GeoRef) return;
	//raza lunii * 100, pentru ca Unreal masoara distantele in cm
	double MoonRadiusCM = 1737400.0 * 100.0;
	double MeshRadiusCM = 50.0;
	double BaseScale = MoonRadiusCM / MeshRadiusCM;
	MoonMesh->SetWorldScale3D(FVector(BaseScale));
	AOrbitalManager* OrbManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	//aici verific daca luna nu este controlata de manager, e logica redundanta, fiindca acum orbital manager primeste date udp de la serverul de python, care face toata treaba grea
	if (!bControlledByManager)
	{
		FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
		float CurrentTimeSpeed = (OrbManager && OrbManager->TimeSpeed == 0.0f) ? 0.0f : 1.0f;
		if (CurrentTimeSpeed > 0.0f)
		{
			CurrentOrbitAngle += OrbitSpeed * DeltaTime;
		}
		double Rad = FMath::DegreesToRadians(CurrentOrbitAngle);
		double DistCM = DistanceMeters * 100.0;
		FVector OrbitPos(FMath::Cos(Rad) * DistCM, FMath::Sin(Rad) * DistCM, 0.0);
		SetActorLocation(EarthCenter + OrbitPos);
		FVector DirectionToEarth = (EarthCenter - GetActorLocation()).GetSafeNormal();
		FVector EarthNorthAxis = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(FVector(0, 0, 1)).GetSafeNormal();
		SetActorRotation(FRotationMatrix::MakeFromXZ(DirectionToEarth, EarthNorthAxis).Rotator());
	}
	if (!SunLightSource && OrbManager && OrbManager->SunSkyActor)
	{
		SunLightSource = OrbManager->SunSkyActor;
	}
	if (SunLightSource && OrbManager)
	{
		UDirectionalLightComponent* LightComp = SunLightSource->FindComponentByClass<UDirectionalLightComponent>();
		if (LightComp)
		{
			FVector DirToSun = -LightComp->GetForwardVector().GetSafeNormal();

			// 1. Calculam directia spre Luna fata de centrul Pamantului (sau pozitia dronei)
			FVector EarthCenter = GeoRef->TransformEarthCenteredEarthFixedPositionToUnreal(FVector::ZeroVector);
			FVector DirToMoon = (GetActorLocation() - EarthCenter).GetSafeNormal();

			// 2. Calculam distanta unghiulara dintre Soare si Luna
			float Dot = FVector::DotProduct(DirToSun, DirToMoon);
			Dot = FMath::Clamp(Dot, -1.0f, 1.0f);
			float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

			// 3. Setam plaja de grade pe care se intampla disparitia (Fade)
			// La ~0.5 grade, corpurile abia se ating (limita eclipsei). 
			// Vrem sa dispara treptat pe masura ce se indeparteaza spre 2.5 grade.
			float FadeStartAngle = 0.5f;
			float FadeEndAngle = 2.5f;

			// 4. Calculam Factorul de Fade (1.0 = Eclipsa Neagra, 0.0 = Invizibila pe cer)
			float FadeFactor = 1.0f - FMath::Clamp((AngleDegrees - FadeStartAngle) / (FadeEndAngle - FadeStartAngle), 0.0f, 1.0f);

			// Folosim SmoothStep ca sa curbam tranzitia, sa nu para o disparitie liniara si mecanica
			float SmoothFade = FMath::SmoothStep(0.0f, 1.0f, FadeFactor);

			if (UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(MoonMesh->GetMaterial(0)))
			{
				DynMat->SetVectorParameterValue(TEXT("SunLightDir"), FLinearColor(DirToSun.X, DirToSun.Y, DirToSun.Z, 0.0f));

				// APLICAM TRANZITIA: Fara if / else !
				// Cand sunt suprapuse (SmoothFade = 1.0): E 100% solida, fara brightness/specular.
				// Cand se indeparteaza: Revine usor, usor la materialul de baza.
				DynMat->SetScalarParameterValue(TEXT("ForceSolid"), SmoothFade);
				DynMat->SetScalarParameterValue(TEXT("Brightness"), 1.0f - SmoothFade);
				DynMat->SetScalarParameterValue(TEXT("Specular"), 0.5f * (1.0f - SmoothFade));
			}
		}
	}
}

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Moon.h" 
#include "CesiumGlobeAnchorComponent.h"
#include "Components/TextRenderComponent.h"
#include "ProceduralMeshComponent.h"
#include "OrbitalManager.generated.h"
/*Structura pentru stele, fiecare stea are RA (right ascension), Dec (declinatie), magnitudine (din cele 120.000 de stele din baza de date hyg, am extras doar 5069) si
culoare, stelele pot avea culori diverse (rosu, galben, alb, albastru), in functie de temperatura suprafetei lor*/
USTRUCT(BlueprintType)
struct FStarDataRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float RA;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Dec;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Mag;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ColorIndex;
};
/*Fiecare constelatie are un nume si un vector format din 2 coordonate pe post de pozitie*/
USTRUCT(BlueprintType)
struct FConstellationHUDLabel
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString Name;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector LocalPosition; 
};
/*Etichete pentru stele, le folosesc pentru a stii efectiv unde pun numele stelei pe cer*/
USTRUCT(BlueprintType)
struct FStarHUDLabel
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category = "Astronomy")
	FString Name;
	UPROPERTY(BlueprintReadWrite, Category = "Astronomy")
	FVector LocalPosition;
};
/*Structura in care retin data si ora, este redundant dayofyear, avand in vedere ca retin deja luna si ziua*/
USTRUCT(BlueprintType)
struct FSimulationDate
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Year = 2024;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Month = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Day = 21; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite) double Hour = 12.0f;
	UPROPERTY(BlueprintReadOnly) double DayOfYear = 80.0f;
};
/*Structura in care retin date despre constelatii (partea de arta mitologica)
Fiecare desen are un nume, ca si la stele are right ascension si declinatie, o textura specifica, o scalare (default la 1.0f) si o rotatie (se poate roti intr-un singur plan,
si anume cel perpendicular pe dreata data de ra si dec, deci am un singur unghi)*/
USTRUCT(BlueprintType)
struct FZodiacArtData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	float RA; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	float Dec; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	UTexture2D* ArtTexture; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	float ScaleMultiplier = 1.0f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zodiac")
	float RollOffset = 0.0f; 
};
/*Structura de elemente besseliene extrase din baza de date de eclipse, fiecare eclipsa are coordonatele centrului umbrei lunii pe planul fundamental (x,y,z), declinatie (mu),
d,l1 (raza penumbrei), l2 (raza umbrei),tanf1 (tangenta semiunghiului conului de penumbra), tanf2 (tangenta semiunghiului conului de umbra)*/
USTRUCT(BlueprintType)
struct FBesselianElements
{
	GENERATED_BODY()
	FString DateString;  
	int32 Day = 0;
	int32 Month = 0;
	int32 Year = 0;      
	float Hour = 0.0f;
	int32 Saros;
	double T0 = 0;       
	double x[4] = { 0 };
	double y[4] = { 0 };
	double z[4] = { 0 };
	double d[4] = { 0 };
	double mu[4] = { 0 };
	double l1[4] = { 0 };
	double l2[4] = { 0 };
	double tanf1[4];
	double tanf2[4];
	bool bIsValid = false;
	//double T0 = 0;
	double HourUT = 0.0; // <-- Aici salvăm ora UT (0..24) pentru UI
};

// Structura pentru elementele unei eclipse de Luna, extrase din catalogul SkyWare/DE441
USTRUCT(BlueprintType)
struct FLunarEclipseElements
{
	GENERATED_BODY()

	FString DateString;
	int32 Day = 0;
	int32 Month = 0;
	int32 Year = 0;
	double T0 = 0.0; // ora TT a eclipsei maxime (0-24), citita direct din coloana "TT GE (M)"

	FString TypeCode;     // PNP, PNT, UMP, UMT, UMC
	int32 TypeNumber = 0; // 1 = Penumbrala Partiala ... 5 = Umbrala Centrala

	double Gamma = 0.0;
	double Pmag = 0.0;
	double Umag = 0.0;

	int32 Lunation = 0;
	int32 Saros = 0;
	int32 Inex = 0;

	double Dur_P4P1 = -1.0; // minute
	double Dur_U4U1 = -1.0; // minute, -1 daca eclipsa e doar penumbrala
	double Dur_U3U2 = -1.0; // minute, -1 daca nu e totala

	// Toate astea sunt ore fata de momentul eclipsei maxime (T0); -999 = nu se aplica
	double P1 = -999.0;
	double U1 = -999.0;
	double U2 = -999.0;
	double U3 = -999.0;
	double U4 = -999.0;
	double P4 = -999.0;

	double eLngW_GE = 0.0; // longitudine vestica a punctului sub-lunar
	double Lat_GE = 0.0;   // latitudine geodezica a punctului sub-lunar
	double ELT = 0.0;      // light-time in secunde

	bool bIsValid = false;
};
/*Structura care imi defineste liniile constelatiilor de pe cer. Practic, fiecare linie de pe cer are 2 capete cu cate o stea si un nume de constelatie din care face parte*/
USTRUCT(BlueprintType)
struct FConstellationLineRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StarA_Name; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString StarB_Name; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ConstellationName;
};

USTRUCT(BlueprintType)
struct FCountryPoint
{
	GENERATED_BODY()
	FString CityName;
	double Lat = 0.0;
	double Lon = 0.0;
	double Alt = 0.0;
};
/*Structura de test, nu o folosesc activ, aveam la un moment dat tabele cu un numar de secunde si stiam distanta soare-luna o data la 10 secunde*/
USTRUCT(BlueprintType)
struct FEclipseECEFRow
{
	GENERATED_BODY()
	double TotalSeconds; 
	FVector SunECEF_Km;
	FVector MoonECEF_Km;
};
class UNiagaraComponent;
class UNiagaraSystem;
UCLASS()
class PROIECT_ECLIPSA_API AOrbitalManager : public AActor
{
	GENERATED_BODY()
public:
	AOrbitalManager();
	virtual void Tick(float DeltaTime) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stars")
	bool bShowConstellationLabels = false; //bool ca sa arat numele constelatiilor pe cer
	void GenerateConstellationLabels();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stars")
	TArray<FConstellationHUDLabel> HUDConstellationLabels;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomy")
	USceneComponent* StarLabelsRoot;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Astronomy|Ephemeris")
	FVector SunCenterECEF = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Astronomy|Ephemeris")
	FVector MoonCenterECEF = FVector::ZeroVector;


	// În OrbitalManager.h:
	//UPROPERTY()
	TArray<TPair<FString, FCountryPoint>> AllCitiesFlat;
	//TArray<FCountryPoint> AllCitiesFlat;

	UPROPERTY()
	TArray<UTextRenderComponent*> StarTextLabels; //vector de stringuri pentru nume stele
	UPROPERTY(VisibleAnywhere, Category = "Planete")
	UStaticMeshComponent* SaturnRingsMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	float TimeSpeed = 1.0f; //viteza globala a simularii (am considerat ca nu poate trece de 2^19, adica 512.288)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	FSimulationDate CurrentDate;
	UPROPERTY(EditAnywhere, Category = "Actors")
	AMoon* ControlledMoon; 
	UPROPERTY(EditAnywhere, Category = "Actors")
	//sunskyactor din unreal engine, care imi monitorizeaza soarele, norii si atmosfera
	AActor* SunSkyActor;   
	UFUNCTION(BlueprintCallable, Category = "HUD")
	FString GetDateForHUD(); 
	UFUNCTION(BlueprintCallable, Exec, Category = "Simulation")
	void SetSimulationDateTime(int32 Day, int32 Month, int32 Year, float Hour);
	double GetJulianDate(int32 Year, int32 Month, int32 Day, double Hour);
	void CheckAndSelectActiveEclipse();
	void JumpToEclipse();
	void JumpToEclipse2();
	//functii pentru a sari la eclipse la diferite date (nu aveam meniul de catalog eclipse)
	void JumpToEclipse_2030_Jun();  
	void JumpToEclipse_2030_Nov();  
	void JumpToEclipse_2031_May();  
	void JumpToEclipse_2031_Nov();  
	void JumpToEclipse_2034_Mar();  
	void JumpToEclipse_2035_Sep();  
	void JumpToEclipse_Annular_2024(); 
	void JumpToEclipse_Annular_2026(); 
	void JumpToEclipse_Partial_2025(); 
	void JumpToEclipse_Partial_2029(); 
	void LoadEclipseData();

	//pentru eclipsele de luna
	TArray<FLunarEclipseElements> AllLunarEclipses;
	void LoadAndParseLunarDatabase();
	UPROPERTY() float TimeSinceLastUDPSend = 0.0f;
	UPROPERTY() float UDPSendInterval = 1.0f / 30.0f;

	//aici este adaugata functia pentru parsarea punctelor din tari

	//UPROPERTY() TMap<FString, TArray<FCountryPoint>> CountryPoints;
	TMap<FString, TArray<FCountryPoint>> CountryPoints; // fara UPROPERTY - nested containers nu sunt suportate de UHT
	void LoadCountryPoints();
	UPROPERTY()
	TArray<FStarHUDLabel> HUDStarLabels;
	FMatrix CurrentStarDomeMatrix;
	UPROPERTY(BlueprintReadWrite, Category = "Astronomy")
	bool bShowStarLabels = false; //aici arat nume de stele
	UFUNCTION(BlueprintCallable, Category = "Astronomy")
	void SetStarLabelsVisibility(bool bShow);
	TMap<FString, FVector> StarPositionsMap; 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomie")
	UInstancedStaticMeshComponent* ConstellationLinesISMC;
	UFUNCTION(BlueprintCallable, Category = "Astronomie")
	void SetStarsVisibility(bool bShowStars);
	UPROPERTY(EditAnywhere, Category = "Sfera Cereasca")
	UDataTable* StarsDataTable; //tabel cu stele
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sfera Cereasca")
	class UInstancedStaticMeshComponent* StarsISMC;
	UPROPERTY(EditAnywhere, Category = "Sfera Cereasca")
	float Scaling = 1000.0f; //scaling de 1000, fiindca 1 km = 1000 m
	UPROPERTY(EditAnywhere, Category = "Sfera Cereasca")
	float CelestialSphereRadius = 50000000.0f * Scaling; //totul este randat la 500k km
	UFUNCTION(BlueprintCallable, Category = "Sfera Cereasca")
	void GenerateStarSphere();
	TArray<FEclipseECEFRow> CachedECEFData;
	bool bECEFDataLoaded = false;
	bool bUseECEFMode = false; 
	void LoadECEFData(); 
	void ToggleECEFMode(); 
	void UpdateFromECEF(float CurrentTotalSeconds); 
	TArray<FBesselianElements> AllEclipses;
	void LoadAndParseDatabase(); //incarcare baza de date eclipse
	UFUNCTION(BlueprintCallable, Category = "Astronomy")
	void SetConstellationsVisibility(bool bShow);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MoonPhaseOffset = -0.09f;
	void AdjustMoonOffsetPlus();
	void AdjustMoonOffsetMinus();
	bool bEclipseModeActive = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	float MoonInclinationOffset = 0.02f;
	void AdjustInclinationUp();
	void AdjustInclinationDown();
	int32 GetMonthFromStr(const FString& MonthStr);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	double UmbraTipAltitudeKm = 0.0;
	FBesselianElements CurrentEclipseData;
	bool bUseDatabasePrecision = false;
	void CalculateBesselianPositions(const FBesselianElements& Elem, FVector& OutSunPos, FVector& OutMoonPos);
	double EvaluatePoly(double t, const double* c);
	const int32 StartYear = 2024;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
	double CurrentSunDistanceKm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
	double CurrentMoonDistanceKm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
    FString CurrentLocationText = TEXT("Initializare locatie...");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Telemetry")
	float SunCoverage = 0.0f;
	UPROPERTY(VisibleAnywhere, Category = "Eclipse Visuals")
	UProceduralMeshComponent* UmbraProceduralMesh;
	UPROPERTY(VisibleAnywhere, Category = "Eclipse Visuals")
	UProceduralMeshComponent* PenumbraProceduralMesh;
	UPROPERTY(EditAnywhere, Category = "Eclipse Visuals")
	UMaterialInterface* UmbraMaterial;
	UPROPERTY()
	UMaterialInstanceDynamic* DynUmbraMaterial;
	UPROPERTY() UMaterialInstanceDynamic* DynAntumbraMaterial;
	UPROPERTY() UMaterialInstanceDynamic* DynPenumbraMaterial;
	UPROPERTY(EditAnywhere, Category = "Eclipse Visuals")
	UMaterialInterface* PenumbraMaterial;
	//generare con dinamic
	void GenerateDynamicCone(UProceduralMeshComponent* TargetMesh, FVector StartPos, FVector EndPos, float StartRadius, float EndRadius, int32 Segments, FVector EarthCenter = FVector::ZeroVector, double EarthRadius = 0.0);
	const double SunRadiusCm = 69550794300.0; 
	const double MoonRadiusCm = 173793000.0; 
	UPROPERTY() double LiveShadowLon = -999.0f;
	UPROPERTY() double LiveShadowLat = -999.0f;

	UPROPERTY() double Deltatseconds = 0.0f;
	//structura pentru planete, are nume, directie in care e plasata, culoare, distanta km si internal name, care este in engleza
	struct FPlanetData
	{
		FString Name;
		FVector Direction; 
		FColor Color;      
		double DistanceKm;
		FString InternalName;
	};
	TArray<FPlanetData> PlanetsList;
	TArray<FPlanetData> MajorMoonsList;
	UPROPERTY(VisibleAnywhere, Category = "Planets")
	TArray<UStaticMeshComponent*> PlanetMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planets Configuration")
	UMaterialInterface* PlanetMasterMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planets Configuration")
	TArray<UTexture2D*> PlanetTextures;
	UPROPERTY(EditAnywhere, Category = "Planets Config")
	float PlanetVisualDistanceKm = 50000.0f;
	UPROPERTY(EditAnywhere, Category = "Sfera Cereasca")
	UDataTable* ConstellationsDataTable; 
	UPROPERTY(EditAnywhere, Category = "Planets Config")
	float PlanetScaleBase = 5000.0f;
	UFUNCTION(BlueprintCallable, Category = "Astronomie")
	void DrawConstellations();
	UPROPERTY(EditAnywhere, Category = "Astronomie | Desene Constelatii")
	TArray<FZodiacArtData> ZodiacArts;
	UPROPERTY(EditAnywhere, Category = "Astronomie | Desene Constelatii")
	UMaterialInterface* ZodiacMasterMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astronomie")
	USceneComponent* ZodiacRoot;
	UPROPERTY()
	TArray<UStaticMeshComponent*> ZodiacMeshes;
	UFUNCTION(BlueprintCallable, Category = "Astronomie")
	void SetZodiacVisibility(bool bShow);
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicStarMaterial;
	UPROPERTY(BlueprintReadWrite, Category = "Sfera Cereasca")
	float CurrentStarGlowMultiplier = 20.0f; //multiplier de intensitate luminoasa pentru stele (de la 10 la 10.000)
	UFUNCTION(BlueprintCallable, Category = "Sfera Cereasca")
	void SetStarGlowMultiplier(float NewValue);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Planets")
	bool bShowMoonLabels = false; 
	UPROPERTY(VisibleAnywhere, Category = "Eclipse FX")
	UStaticMeshComponent* CoronaMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* CoronaDynamicMat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	class USpotLightComponent* DiamondSpotLight;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Diamond")
	class UStaticMeshComponent* DiamondFlashMesh; //inel cu diamant
	UPROPERTY(Transient)
	class UMaterialInstanceDynamic* DiamondFlashMaterialDynamic;
	UPROPERTY()
	TArray<UStaticMeshComponent*> BeadMeshes; //margelele lui Bailey
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> BeadMaterialsDynamic;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse FX")
	UNiagaraComponent* CoronaNiagaraComp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse FX")
	UNiagaraSystem* CoronaNiagaraSystemAsset;
	UPROPERTY(EditAnywhere, Category = "Eclipse Visuals")
	UMaterialInterface* LightFunctionMasterMat;
	UPROPERTY()
	UMaterialInstanceDynamic* EclipseLightFunctionMat;
	int32 GetDaysInMonth(int32 M, int32 Y);
	UFUNCTION(BlueprintCallable, Category = "Eclipse")
	void UpdateSurfaceShadow(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse", meta = (AllowPrivateAccess = "true"))
	class UProceduralMeshComponent* SurfaceShadowMesh; //mesh pentru umbra pe suprafata Terrei
	void DrawShadowSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	void GenerateShadowEllipse(UProceduralMeshComponent* TargetMesh, FVector Center, FVector Normal, FVector LightDir, float Radius, float Stretch, int32 Segments);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowShadowOnGround = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowShadowConeFull = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowShadowConeTruncated = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse Shadows")
	UProceduralMeshComponent* SurfacePenumbraMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowPenumbraOnGround = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowPenumbraConeFull = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Shadows")
	bool bShowPenumbraConeTruncated = false;

	//astea sunt pentru Pamant
	// Meshurile procedurale pentru umbra/penumbra proprie a Pamantului (eclipse de Luna)
	UPROPERTY() class UProceduralMeshComponent* EarthUmbraConeMesh;
	UPROPERTY() class UProceduralMeshComponent* EarthPenumbraConeMesh;

	// Materiale de baza + instantele dinamice (culoare separata fata de umbra/penumbra Lunii)
	UPROPERTY() class UMaterial* EarthUmbraMaterial;
	UPROPERTY() class UMaterial* EarthPenumbraMaterial;
	UPROPERTY() class UMaterialInstanceDynamic* DynEarthUmbraMaterial;
	UPROPERTY() class UMaterialInstanceDynamic* DynEarthPenumbraMaterial;

	// Comutatoare separate, exact ca la Luna
	bool bShowEarthShadowUmbra = false;
	bool bShowEarthShadowPenumbra = false;

	void DrawEarthUmbraCone(FVector SunDirUnreal, double DistSun_Cm, FVector PlayerPosUnreal);
	void DrawEarthPenumbraCone(FVector SunDirUnreal, double DistSun_Cm, FVector PlayerPosUnreal);

	UFUNCTION()
	void DrawPenumbraSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	UFUNCTION()
	void DrawPenumbraCone3D(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orbitals")
	TArray<UStaticMeshComponent*> MajorMoonsMeshes;
	UPROPERTY()
	TArray<UTexture2D*> MajorMoonsTextures;
	UPROPERTY(VisibleAnywhere, Category = "Timp")
	double HighResHour = 12.0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	UProceduralMeshComponent* SurfaceAntumbraMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	UProceduralMeshComponent* AntumbraProceduralMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse")
	UMaterialInterface* AntumbraMaterial;
	UFUNCTION()
	void DrawAntumbraSurfaceMesh(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	UFUNCTION()
	void DrawAntumbraCone3D(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse Path")
	bool bShowEclipsePath = true;
	//UFUNCTION(BlueprintCallable, Category = "Eclipse Path")
	//void GenerateCurrentEclipsePath();
	//void DrawEclipsePathLines();
	//fiecare punct de pe banda de totalitate are o coordonata centrala, limita nordica si sudica, eu trimit de la unreal 100 puncte si 100 de latimi
	struct FEclipsePathPoint
	{
		FVector CenterPath; 
		FVector NorthLimit; 
		FVector SouthLimit; 
		bool bIsValid;		
	};
	TArray<FEclipsePathPoint> CurrentEclipsePath;
	//FEclipsePathPoint CalculateShadowGeometryAtTime(double JD_TT);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse Path")
	class UProceduralMeshComponent* EclipsePathMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	UCesiumGlobeAnchorComponent* EclipsePathAnchor;

	UFUNCTION()
	void RebuildEclipsePathFromSaved();
	UPROPERTY()
	UMaterialInterface* EclipsePathMaterial;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse Path")
	USplineComponent* EclipsePathSpline; //desen sub forma de spline a benzii de totalitate
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse Path")
	class UInstancedStaticMeshComponent* EclipsePathISMC;
	UPROPERTY();
	bool bShowTotalityZone = false;
	UPROPERTY();
	int32 NumPathPoints = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse")
	TArray<FVector> SavedEclipsePath;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse")
	bool bIsUmbraOnGround = false;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse")
	FVector LiveUmbraCenterLLH = FVector::ZeroVector;
	void UpdateLiveUmbraTarget(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
	static FText Tr(const FString& TextRO, const FString& TextEN);
	static FString TrString(const FString& TextRO, const FString& TextEN);
	int32 LastPathCount = -1;
	double LastFirstLon = -999.0;
	FVector LastEarthCenter = FVector(-999999.0, -999999.0, -999999.0);
	//functie pentru con de umbra Paamant
	UFUNCTION(BlueprintCallable)
	void ApplyEarthShadowColors();
private:
	void UpdateOrbitals(float DeltaTime);
	//pentru eclipsele de luna
	static double ParseTimeToHours(const FString& TimeStr);
	static bool IsPlaceholder(const FString& Str);

protected:
	virtual void BeginPlay() override; //aici fac legatura cu serverul python (executabilul server.exe)
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; //aici rup legatura (inchid socketul) cu executabilul server.exe
private:
	float SavedTimeSpeed = 1.0f;
	void DecreaseTimeSpeed(); 
	void IncreaseTimeSpeed(); 
	void TogglePause();       
	bool bIsOnMoon = false;
	FVector SavedEarthLocation;
	FRotator SavedEarthRotation;
	void ToggleMoonTravel();
	void SendDataToPython(); //functie in care trimit date la python (trimit 20 de doubleuri)
	void ReceiveDataFromPython(); //aici primesc 352 de doubleuri de la python
	void InitSocket();
	void ToggleClouds();
	FSocket* Socket;
	TSharedPtr<FInternetAddr> RemoteAddr;
	float LastFOV = -1.0f;
	void DrawEclipseShadowCones(FVector SunDirUnreal, double DistSun_Cm, FVector MoonDirUnreal, double DistMoon_Cm, FVector PlayerPosUnreal);
};

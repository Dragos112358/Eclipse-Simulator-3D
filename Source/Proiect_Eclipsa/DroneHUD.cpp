#include "DroneHUD.h"
#include "DronaMea.h"
#include "Engine/Canvas.h"
#include "UObject/ConstructorHelpers.h"
#include "OrbitalManager.h"          
#include "Kismet/GameplayStatics.h"  
#include "Engine/World.h" 
#include "Components/DirectionalLightComponent.h"
#include "Components/LineBatchComponent.h"
#include "CesiumGeoreference.h"
#include "Misc/ConfigCacheIni.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
ADroneHUD::ADroneHUD()
{
	//in hud, folosesc fontul roboto
	static ConstructorHelpers::FObjectFinder<UFont> FontObj(TEXT("/Engine/EngineFonts/Roboto"));
	if (FontObj.Succeeded())
	{
		HUDFont = FontObj.Object;
	}
	//folosesc iconita pentru primul buton (modul harta)
	static ConstructorHelpers::FObjectFinder<UTexture2D> MapTex(TEXT("/Script/Engine.Texture2D'/Game/Textures/Meniu_map.Meniu_map'"));
	if (MapTex.Succeeded())
	{
		IconitaHarta = MapTex.Object;
	}
	//iconita meniu xavier (ilustrare eclipsa partiala)
	static ConstructorHelpers::FObjectFinder<UTexture2D> MenuTex(TEXT("/Script/Engine.Texture2D'/Game/Textures/Meniu_Xavier.Meniu_Xavier'"));
	if (MenuTex.Succeeded())
	{
		IconitaMeniu = MenuTex.Object;
	}
	//iconita cu 2 norisori
	static ConstructorHelpers::FObjectFinder<UTexture2D> CloudTex(TEXT("/Script/Engine.Texture2D'/Game/Textures/Meniu_nori.Meniu_nori'"));
	if (CloudTex.Succeeded()) IconitaNori = CloudTex.Object;
}
void ADroneHUD::BeginPlay()
{
	Super::BeginPlay();

	// Gasim norii o singura data la pornire
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		if (Actor && Actor->GetName().Contains(TEXT("Cloud")))
		{
			CachedCloudActor = Actor;
			break;
		}
	}
}

//functie de desenat linii, folosita pentru a desena grile azimutale si ecuatoriale (o mica functionalitate pe care am vazut-o la Stellarium)
void ADroneHUD::DrawThickLine(UCanvas* InCanvas, float X1, float Y1, float X2, float Y2, float Thickness, FLinearColor Color)
{
	if (!InCanvas) return;
	FCanvasLineItem LineItem(FVector2D(X1, Y1), FVector2D(X2, Y2));
	LineItem.SetColor(Color);
	LineItem.LineThickness = Thickness;
	InCanvas->DrawItem(LineItem);
}
void ADroneHUD::DrawTextWithShadow(FString Text, FLinearColor Color, float X, float Y, float Scale)
{
	// Desenam mai intai umbra (offset de 1.5 pixeli in dreapta-jos)
	DrawText(Text, FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), X + 1.5f, Y + 1.5f, HUDFont, Scale);
	// Desenam textul efectiv peste umbra
	DrawText(Text, Color, X, Y, HUDFont, Scale);
}
//aici desenez hud-ul, este functia care se apeleaza la fiecare tick
void ADroneHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!HUDFont) return;
	//daca am bifat in overlays si telemetrie ca nu vreau sa vad hudul, nu il vad
	if (!bDrawTelemetry) return;
	bool bHideHUD = false;
	//verific fisierul config.ini sa vad ce contine legat de setarile hudului
	if (GConfig)
	{
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bHideHUD, GGameUserSettingsIni);
	}
	bool bUseImperial = false;
	//buseimperial este pentru a vedea ce unitate de masura folosesc (km sau mile)
	if (GConfig)
	{
		GConfig->GetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bHideHUD, GGameUserSettingsIni);
		FString SavedUnit = TEXT("KM");
		GConfig->GetString(TEXT("SetariCustom"), TEXT("UnitatiMasura"), SavedUnit, GGameUserSettingsIni);
		if (SavedUnit == TEXT("MI"))
		{
			bUseImperial = true;
		}
	}
	//aici verific daca fovul este sub 10 grade, daca este sub 10 grade, scot vizibilitatea dronei
	APlayerController* PC101 = GetOwningPlayerController();
	if (PC101 && PC101->PlayerCameraManager)
	{
		float CurrentFOV = PC101->PlayerCameraManager->GetFOVAngle();
		APawn* MyDrone = PC101->GetPawn();
		if (MyDrone)
		{
			if (CurrentFOV < 10.0f)
			{
				if (!MyDrone->IsHidden())
				{
					MyDrone->SetActorHiddenInGame(true);
				}
				bWasZoomedIn = true;
			}
			else
			{
				if (bWasZoomedIn)
				{
					bool bSavedDroneVis = true;
					if (GConfig)
					{
						GConfig->GetBool(TEXT("SetariCustom"), TEXT("VizibilitateDrona"), bSavedDroneVis, GGameUserSettingsIni);
					}
					MyDrone->SetActorHiddenInGame(!bSavedDroneVis);
					bWasZoomedIn = false;
				}
			}
		}
	}

	//aici fac locul in care adaug scalare pe ecran
	float ScaleX = Canvas->ClipX / 1920.0f;
	float ScaleY = Canvas->ClipY / 1080.0f;

	float DPIScale = 1.0f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		DPIScale = GEngine->GameViewport->GetWindow()->GetDPIScaleFactor();
	}

	float UI = FMath::Min(ScaleX, ScaleY) * DPIScale;
	CachedUI = UI/DPIScale;
	auto S = [&](float Val) { return Val * UI; };



	ADronaMea* Drona = Cast<ADronaMea>(GetOwningPawn());
	//aici scriu date generale (fps-uri)
	//pe laptopul meu merge in maxim 110 fps-uri, pe alte laptopuri variaza.
	if (Drona)
	{
		// =========================================================================
	// DESIGN UI: PANOU LATERAL (STELLARIUM STYLE)
	// =========================================================================
		float PanelX = S(20.0f);           // Marginea din stanga
		float PanelY = S(135.0f);           // Marginea de sus
		float PanelWidth = S(460.0f);      // Latimea chenarului
		float PanelHeight = S(920.0f);     // Inaltimea chenarului (o poti ajusta)
		float PaddingX = S(20.0f);         // Marginea interioara stanga a textului
		float LineHeight = S(36.0f);       // Spatierea verticala intre randuri

		// Paleta de culori profesionala
		FLinearColor BgColor = FLinearColor(0.03f, 0.03f, 0.05f, 0.5f);     // Fundal gri/albastru inchis (50% opac)
		FLinearColor AccentLineColor = FLinearColor(0.0f, 0.6f, 1.0f, 1.0f); // Linie accent Cyan
		FLinearColor CategoryColor = FLinearColor(0.3f, 0.8f, 1.0f, 1.0f);   // Culoare titluri sectiuni
		FLinearColor TextStandard = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);    // Alb murdar (nu oboseste ochii)
		FLinearColor TextGreen = FLinearColor(0.2f, 1.0f, 0.2f, 1.0f);       // Verde clar
		FLinearColor TextWarning = FLinearColor(1.0f, 0.7f, 0.0f, 1.0f);     // Portocaliu
		FLinearColor TextDanger = FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);      // Rosu

		// DESENARE FUNDAL PANOU
		DrawRect(BgColor, PanelX, PanelY, PanelWidth, PanelHeight);
		DrawRect(AccentLineColor, PanelX, PanelY, S(4.0f), PanelHeight); // Linia de design din stanga

		float X_Pos = PanelX + PaddingX;
		float Y_Pos = PanelY + S(10.0f);

		// -------------------------------------------------------------------------
		// SECTIUNEA 1: SISTEM SI ZBOR
		// -------------------------------------------------------------------------
		FString TextSistem = AOrbitalManager::TrString(
			TEXT("SISTEM & NAVIGATIE"),
			TEXT("SYSTEM & NAVIGATION")
		);

		DrawTextWithShadow(TextSistem, CategoryColor, X_Pos, Y_Pos, S(2.5f));
		Y_Pos += LineHeight + S(15.0f);
		float DeltaTime = GetWorld()->GetDeltaSeconds();
		int32 CurrentFPS = (DeltaTime > 0.0f) ? FMath::RoundToInt(1.0f / DeltaTime) : 0;
		FLinearColor FPSColor = (CurrentFPS < 30) ? TextDanger : ((CurrentFPS < 60) ? TextWarning : TextGreen);
		DrawTextWithShadow(FString::Printf(TEXT("FPS: %d"), CurrentFPS), FPSColor, X_Pos, Y_Pos, S(2.5f));
		Y_Pos += LineHeight;

		if (!IsValid(CachedOrbManager)) CachedOrbManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		AOrbitalManager* OrbManager = CachedOrbManager;

		if (OrbManager)
		{
			DrawTextWithShadow(OrbManager->GetDateForHUD(), TextStandard, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += (LineHeight * 3.0f);
		}
		else
		{
			DrawTextWithShadow(TEXT("TIME SYNC: OFFLINE"), TextDanger, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += LineHeight;
		}

		float VitezaKmH = Drona->CurrentVelocity.Size() * 0.036f;
		float DisplaySpeed = bUseImperial ? VitezaKmH * 0.621371f : VitezaKmH;
		FString SpeedUnit = bUseImperial ? TEXT("mph") : TEXT("km/h");
		FString TextViteza = AOrbitalManager::TrString(FString::Printf(TEXT("VITEZA: %.0f %s"), DisplaySpeed, *SpeedUnit), FString::Printf(TEXT("SPEED: %.0f %s"), DisplaySpeed, *SpeedUnit));
		DrawTextWithShadow(TextViteza, TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;

		FString TextPutere = AOrbitalManager::TrString(FString::Printf(TEXT("PUTERE MOTOR: %.0f"), Drona->Acceleration), FString::Printf(TEXT("ENGINE POWER: %.0f"), Drona->Acceleration));
		DrawTextWithShadow(TextPutere, TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;

		float CurrentFOVValue = (PC101 && PC101->PlayerCameraManager) ? PC101->PlayerCameraManager->GetFOVAngle() : 90.0f;
		FString TextFOV = AOrbitalManager::TrString(FString::Printf(TEXT("UNGHI VIZUALIZARE: %.2f\u00B0"), CurrentFOVValue), FString::Printf(TEXT("FOV: %.2f\u00B0"), CurrentFOVValue));
		DrawTextWithShadow(TextFOV, TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;

		// Bara de viteza (integrata frumos in panou)
		float MaxBarWidth = PanelWidth - (PaddingX * 2);
		float BarHeight = 8.0f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f), X_Pos, Y_Pos, MaxBarWidth, BarHeight);
		float FillPercentage = FMath::Clamp(VitezaKmH / 279.0f, 0.0f, 1.0f);
		DrawRect((VitezaKmH > 100.0f ? TextDanger : TextGreen), X_Pos, Y_Pos, MaxBarWidth * FillPercentage, BarHeight);
		Y_Pos += LineHeight + S(10.0f);

		// -------------------------------------------------------------------------
		// SECTIUNEA 2: DATE GEOGRAFICE
		// -------------------------------------------------------------------------
		FString TextSistem2 = AOrbitalManager::TrString(
			TEXT("DATE GEOGRAFICE"),
			TEXT("GEOGRAPHIC DATA")
		);

		DrawTextWithShadow(TextSistem2, CategoryColor, X_Pos, Y_Pos, 2.5f);
		Y_Pos += LineHeight + S(10.0f);

		ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
		double Latitudine = 0, Longitudine = 0, AltitudineMSL = 0;
		if (GeoRef)
		{
			FVector Coords = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Drona->GetActorLocation());
			Longitudine = Coords.X; Latitudine = Coords.Y; AltitudineMSL = Coords.Z;
		}

		FHitResult HitResult;
		bool bHit = GetWorld()->SweepSingleByChannel(HitResult, Drona->GetActorLocation(), Drona->GetActorLocation() - (FVector::UpVector * 100000.0f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(50.0f), FCollisionQueryParams(NAME_None, true, Drona));
		float DisplayAGL = -1.0f;
		if (bHit) { LastValidAGL = HitResult.Distance / 100.0f; TimeSinceLastHit = 0.0f; DisplayAGL = LastValidAGL; }
		else { TimeSinceLastHit += DeltaTime; DisplayAGL = (TimeSinceLastHit < 0.5f) ? LastValidAGL : -1.0f; }

		float DisplayAltAGL = bUseImperial && DisplayAGL >= 0 ? DisplayAGL * 3.28084f : DisplayAGL;
		float DisplayAltMSL = bUseImperial ? AltitudineMSL * 3.28084f : AltitudineMSL;
		FString AltUnit = bUseImperial ? TEXT("ft") : TEXT("m");

		FString TextAGL = (DisplayAGL >= 0) ? AOrbitalManager::TrString(FString::Printf(TEXT("ALT. SOL (AGL): %.1f %s"), DisplayAltAGL, *AltUnit), FString::Printf(TEXT("GND ALT (AGL): %.1f %s"), DisplayAltAGL, *AltUnit)) : AOrbitalManager::TrString(FString::Printf(TEXT("ALT. SOL (AGL): >%s"), bUseImperial ? TEXT("3280 ft") : TEXT("1000 m")), FString::Printf(TEXT("GND ALT (AGL): >%s"), bUseImperial ? TEXT("3280 ft") : TEXT("1000 m")));
		DrawTextWithShadow(TextAGL, (DisplayAGL < 15.0f && DisplayAGL >= 0) ? TextDanger : TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;

		FString TextMSL = AOrbitalManager::TrString(FString::Printf(TEXT("ALT. ABSOLUTA (MSL): %.1f %s"), DisplayAltMSL, *AltUnit), FString::Printf(TEXT("ABS ALT (MSL): %.1f %s"), DisplayAltMSL, *AltUnit));
		DrawTextWithShadow(TextMSL, TextStandard, X_Pos, Y_Pos, 1.8f);
		Y_Pos += LineHeight;

		/*DrawTextWithShadow(FString::Printf(TEXT("LAT: %.5f"), Latitudine), TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;
		DrawTextWithShadow(FString::Printf(TEXT("LON: %.5f"), Longitudine), TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;*/

		FString LatHemisphere = (Latitudine >= 0.0) ? TEXT("N") : TEXT("S");
		FString LonHemisphere = (Longitudine >= 0.0) ? TEXT("E") : AOrbitalManager::TrString(TEXT("V"), TEXT("W"));

		DrawTextWithShadow(FString::Printf(TEXT("LAT: %.5f\u00B0 %s"), FMath::Abs(Latitudine), *LatHemisphere), TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;
		DrawTextWithShadow(FString::Printf(TEXT("LON: %.5f\u00B0 %s"), FMath::Abs(Longitudine), *LonHemisphere), TextStandard, X_Pos, Y_Pos, S(1.8f));
		Y_Pos += LineHeight;

		if (!GeoRef) { DrawTextWithShadow(TEXT("ERR: NO CESIUM REF"), TextDanger, X_Pos, Y_Pos, 1.8f); Y_Pos += LineHeight; }
		Y_Pos += S(10.0f);
		//calcul altitudine Soare
		float SunElevationDeg2 = 0.0f;
		FVector DirToSun2 = FVector::ZeroVector;
		FVector LocalUpUnreal2 = FVector::ZeroVector;
		bool bHasSunData2 = false;
		int32 HitCount2 = 0; // Câte raze au fost blocate

		if (OrbManager->SunSkyActor && GeoRef)
		{
			UDirectionalLightComponent* SunLight2 = OrbManager->SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
			if (SunLight2)
			{
				DirToSun2 = -SunLight2->GetForwardVector();
				FVector PlayerPosUnreal = Drona->GetActorLocation();
				LocalUpUnreal2 = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPosUnreal).GetSafeNormal());
				SunElevationDeg2 = FMath::RadiansToDegrees(FMath::Asin(FVector::DotProduct(DirToSun2, LocalUpUnreal2)));

				// Tragem razele INAINTE de a desena telemetria
				FCollisionQueryParams SunTraceParams(NAME_None, true, Drona);
				FVector RightVec = FVector::CrossProduct(LocalUpUnreal2, DirToSun2).GetSafeNormal();
				FVector UpVec = FVector::CrossProduct(DirToSun2, RightVec).GetSafeNormal();
				TArray<FVector> TraceDirections = { DirToSun2, DirToSun2.RotateAngleAxis(0.26f, RightVec), DirToSun2.RotateAngleAxis(-0.26f, RightVec), DirToSun2.RotateAngleAxis(0.26f, UpVec), DirToSun2.RotateAngleAxis(-0.26f, UpVec) };

				for (const FVector& RayDir : TraceDirections)
				{
					FHitResult SunHit;
					// Daca raza loveste geometria Pamantului sau un munte
					if (GetWorld()->LineTraceSingleByChannel(SunHit, PlayerPosUnreal, PlayerPosUnreal + (RayDir * 1000000000.0f), ECC_Visibility, SunTraceParams))
					{
						HitCount2++;
					}
				}
				bHasSunData2 = true;
			}
		}
		float VisibleCoverage = 0.0f;
		// -------------------------------------------------------------------------
		// SECTIUNEA 3: TELEMETRIE ASTRONOMICA
		// -------------------------------------------------------------------------
		if (OrbManager)
		{
			DrawTextWithShadow(AOrbitalManager::TrString(TEXT("TELEMETRIE ASTRONOMICA"), TEXT("ASTRONOMICAL TELEMETRY")), CategoryColor, X_Pos, Y_Pos, S(2.5f));
			Y_Pos += S(LineHeight + 10.0f);

			double SunDist = bUseImperial ? (OrbManager->CurrentSunDistanceKm / 1000000.0) * 0.621371 : (OrbManager->CurrentSunDistanceKm / 1000000.0);
			double MoonDist = bUseImperial ? OrbManager->CurrentMoonDistanceKm * 0.621371 : OrbManager->CurrentMoonDistanceKm;
			FString SunUnitStr = bUseImperial ? TEXT("Mil. mi") : TEXT("Mil. km");
			FString MoonUnitStr = bUseImperial ? TEXT("mi") : TEXT("km");

			DrawTextWithShadow(AOrbitalManager::TrString(FString::Printf(TEXT("DIST. TERRA - SOARE: %.3f %s"), SunDist, *SunUnitStr), FString::Printf(TEXT("EARTH - SUN DIST: %.3f %s"), SunDist, *SunUnitStr)), TextStandard, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += LineHeight;

			DrawTextWithShadow(AOrbitalManager::TrString(FString::Printf(TEXT("DIST. TERRA - LUNA:  %.3f %s"), MoonDist, *MoonUnitStr), FString::Printf(TEXT("EARTH - MOON DIST:  %.3f %s"), MoonDist, *MoonUnitStr)), TextStandard, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += LineHeight;
			FString Prefix = AOrbitalManager::TrString(TEXT("LOCATIE: "), TEXT("LOCATION: "));
			FString LocData = OrbManager->CurrentLocationText.IsEmpty()
				? AOrbitalManager::TrString(TEXT("Asteptare date GPS..."), TEXT("Waiting for GPS..."))
				: OrbManager->CurrentLocationText;
			FString TextFinal = Prefix + LocData;
			DrawTextWithShadow(TextFinal, TextStandard, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += LineHeight;

			float Coverage = OrbManager->SunCoverage;
			
			// SOLUȚIA SUPREMĂ: Dacă toate cele 5 raze sunt blocate, Soarele e complet invizibil.
			// Astfel, acoperirea eclipsei văzută de cameră este 0, indiferent unde ești în Univers.
			if (bHasSunData2 && HitCount2 == 5)
			{
				VisibleCoverage = 0.0f;
			}
			else
			{
				VisibleCoverage = Coverage;
			}

			//DrawTextWithShadow(AOrbitalManager::TrString(FString::Printf(TEXT("ACOPERIRE SOARE: %.4f%%"), Coverage), FString::Printf(TEXT("SUN COVERAGE: %.4f%%"), Coverage)), (Coverage > 0.0f ? AccentLineColor : TextStandard), X_Pos, Y_Pos, S(1.8f));
			DrawTextWithShadow(AOrbitalManager::TrString(FString::Printf(TEXT("ACOPERIRE SOARE: %.4f%%"), VisibleCoverage), FString::Printf(TEXT("SUN COVERAGE: %.4f%%"), VisibleCoverage)), (VisibleCoverage > 0.0f ? AccentLineColor : TextStandard), X_Pos, Y_Pos, S(1.8f));

			Y_Pos += LineHeight;

			if (Coverage >= 100.0f) DrawTextWithShadow(AOrbitalManager::TrString(TEXT("STATUS: TOTALITATE (IN CON)"), TEXT("STATUS: TOTALITY (INSIDE)")), TextGreen, X_Pos, Y_Pos, S(1.8f));
			else if (Coverage > 0.0f) DrawTextWithShadow(AOrbitalManager::TrString(TEXT("STATUS: PARTIALA (PENUMBRA)"), TEXT("STATUS: PARTIAL (OUTSIDE)")), TextWarning, X_Pos, Y_Pos, S(1.8f));
			else DrawTextWithShadow(AOrbitalManager::TrString(TEXT("STATUS: NICIO ECLIPSA"), TEXT("STATUS: NO ECLIPSE")), TextStandard, X_Pos, Y_Pos, S(1.8f));
			Y_Pos += LineHeight;

			/*if (Coverage > 0.0f)
			{
				FString TipText = (OrbManager->UmbraTipAltitudeKm > 0.0) ?
					AOrbitalManager::TrString(FString::Printf(TEXT("FOCAR UMBRA: +%.1f km (INELARA)"), OrbManager->UmbraTipAltitudeKm), FString::Printf(TEXT("SHADOW FOCUS: +%.1f km (ANNULAR)"), OrbManager->UmbraTipAltitudeKm)) :
					AOrbitalManager::TrString(FString::Printf(TEXT("FOCAR UMBRA: %.1f km (TOTALA)"), OrbManager->UmbraTipAltitudeKm), FString::Printf(TEXT("SHADOW FOCUS: %.1f km (TOTAL)"), OrbManager->UmbraTipAltitudeKm));
				DrawTextWithShadow(TipText, (OrbManager->UmbraTipAltitudeKm > 0.0 ? TextWarning : TextGreen), X_Pos, Y_Pos, S(1.8f));
				Y_Pos += LineHeight;
			}
			Y_Pos += S(10.0f);*/

			// -------------------------------------------------------------------------
			// SECTIUNEA 4: VIZIBILITATE
			// -------------------------------------------------------------------------
			if (OrbManager->SunSkyActor && GeoRef && Coverage > 0.0f)
			{
				UDirectionalLightComponent* SunLight = OrbManager->SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
				if (SunLight)
				{
					DrawTextWithShadow(AOrbitalManager::TrString(TEXT("CONDITII VIZIBILITATE"), TEXT("VISIBILITY CONDITIONS")), CategoryColor, X_Pos, Y_Pos, S(2.5f));
					Y_Pos += LineHeight + S(10.0f);

					FVector DirToSun = -SunLight->GetForwardVector();
					FVector PlayerPosUnreal = Drona->GetActorLocation();
					FVector LocalUpUnreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(GeoRef->TransformUnrealPositionToEarthCenteredEarthFixed(PlayerPosUnreal).GetSafeNormal());
					float SunElevationDeg = FMath::RadiansToDegrees(FMath::Asin(FVector::DotProduct(DirToSun, LocalUpUnreal)));

					FCollisionQueryParams SunTraceParams(NAME_None, true, Drona);
					FVector RightVec = FVector::CrossProduct(LocalUpUnreal, DirToSun).GetSafeNormal();
					FVector UpVec = FVector::CrossProduct(DirToSun, RightVec).GetSafeNormal();
					TArray<FVector> TraceDirections = { DirToSun, DirToSun.RotateAngleAxis(0.26f, RightVec), DirToSun.RotateAngleAxis(-0.26f, RightVec), DirToSun.RotateAngleAxis(0.26f, UpVec), DirToSun.RotateAngleAxis(-0.26f, UpVec) };

					int32 HitCount = 0;
					for (const FVector& RayDir : TraceDirections)
					{
						FHitResult SunHit;
						if (GetWorld()->LineTraceSingleByChannel(SunHit, PlayerPosUnreal, PlayerPosUnreal + (RayDir * 1000000000.0f), ECC_Visibility, SunTraceParams)) HitCount++;
					}

					FString VisStr;
					FLinearColor VisCol;
					if (HitCount == 5) { VisStr = (SunElevationDeg < 0.0f) ? AOrbitalManager::TrString(TEXT("ASCUNS DE PLANETA (APUS)"), TEXT("HIDDEN BY PLANET (SUNSET)")) : AOrbitalManager::TrString(TEXT("BLOCAT DE RELIEF (MUNTI)"), TEXT("BLOCKED BY TERRAIN")); VisCol = TextDanger; }
					else if (HitCount > 0) { VisStr = AOrbitalManager::TrString(TEXT("PARTIAL ASCUNS DE ORIZONT"), TEXT("PARTIALLY HIDDEN BY HORIZON")); VisCol = TextWarning; }
					else { VisStr = (SunElevationDeg < 0.0f) ? AOrbitalManager::TrString(TEXT("PERFECT (AERIAN)"), TEXT("PERFECT (AERIAL)")) : AOrbitalManager::TrString(TEXT("VIZIBILITATE PERFECTA"), TEXT("PERFECT VISIBILITY")); VisCol = TextGreen; }

					DrawTextWithShadow(VisStr, VisCol, X_Pos, Y_Pos, S(1.8f));
				}
			}
		}
		//aici desenez busola
		/*Am ales sa desenez busola procedural in c++, in loc sa import o imagine, deoarece nu am gasit pe internet exact ceea ce cautam si voiam sa arate cat mai bine
		De asemenea, imi doream sa pot roti dinamic unghiurile, iar sa rotesc poza efectiv ar fi condus la artefacte vizuale*/
		float CompassRadius = S(70.0f);
		float CenterX = Canvas->ClipX - CompassRadius - S(50.0f); 
		float CenterY = CompassRadius + S(50.0f);
		FLinearColor CompassColor = FLinearColor::White;
		int32 Segments = 36;
		float AngleStep = 360.0f / Segments;
		//folosesc drawthickline pentru a desena busola pe ecran
		for (int32 i = 0; i < Segments; i++)
		{
			float Rad1 = FMath::DegreesToRadians(i * AngleStep);
			float Rad2 = FMath::DegreesToRadians((i + 1) * AngleStep);
			float X1 = CenterX + CompassRadius * FMath::Cos(Rad1);
			float Y1 = CenterY + CompassRadius * FMath::Sin(Rad1);
			float X2 = CenterX + CompassRadius * FMath::Cos(Rad2);
			float Y2 = CenterY + CompassRadius * FMath::Sin(Rad2);
			DrawThickLine(Canvas, X1, Y1, X2, Y2, 2.0f, CompassColor);
		}
		float MyYaw = Drona->GetActorRotation().Yaw;
		struct FCardinalPoint { FString Text; float Angle; FLinearColor Col; };
		TArray<FCardinalPoint> Points;
		Points.Add({ "N", 0.0f, FLinearColor::Red });    
		Points.Add({ "E", 90.0f, FLinearColor::White });
		Points.Add({ "S", 180.0f, FLinearColor::White });
		FString WestLetter = AOrbitalManager::TrString(TEXT("V"), TEXT("W")); //vest sau west in functie de limba
		Points.Add({ WestLetter, 270.0f, FLinearColor::White });
		for (const auto& Pt : Points)
		{
			float ScreenAngleDeg = Pt.Angle - MyYaw + 180.0f;
			float Rad = FMath::DegreesToRadians(ScreenAngleDeg);
			float TextRadius = CompassRadius - S(15.0f);
			float TX = CenterX + TextRadius * FMath::Cos(Rad) - 5.0f; 
			float TY = CenterY + TextRadius * FMath::Sin(Rad) - 10.0f;
			DrawText(Pt.Text, Pt.Col, TX, TY, HUDFont, S(1.2f));
			float TickInner = CompassRadius - S(5.0f);
			float TickOuter = CompassRadius + S(5.0f);
			DrawThickLine(Canvas,
				CenterX + TickInner * FMath::Cos(Rad), CenterY + TickInner * FMath::Sin(Rad),
				CenterX + TickOuter * FMath::Cos(Rad), CenterY + TickOuter * FMath::Sin(Rad),
				2.0f, Pt.Col);
		}
		float TopX = CenterX;
		float TopY = CenterY - CompassRadius;
		float TriSize = S(10.0f);
		FLinearColor ArrowColor = FLinearColor::Yellow;
		DrawThickLine(Canvas, TopX, TopY - 5.0f, TopX - TriSize, TopY - TriSize - 5.0f, 3.0f, ArrowColor);
		DrawThickLine(Canvas, TopX, TopY - 5.0f, TopX + TriSize, TopY - TriSize - 5.0f, 3.0f, ArrowColor);
		DrawThickLine(Canvas, TopX - TriSize, TopY - TriSize - 5.0f, TopX + TriSize, TopY - TriSize - 5.0f, 3.0f, ArrowColor);
		DrawRect(FLinearColor::White, CenterX - S(2.0f), CenterY - S(2.0f), S(4.0f), S(4.0f));
		//aici desenez directia in care este orientata pamantul (pot sa il rotesc in multe feluri si vreau sa vad cum e rotit la un moment dat)
		if (Drona->bMapModeActive && GeoRef && PC101 && PC101->PlayerCameraManager)
		{
			float GizmoRadius = S(100.0f);
			float GizmoX = CenterX - S(20.0f);
			float GizmoY = CenterY + CompassRadius + S(200.0f);
			FVector EarthX_ECEF = FVector(1.0, 0.0, 0.0); 
			FVector EarthY_ECEF = FVector(0.0, 1.0, 0.0); 
			FVector EarthZ_ECEF = FVector(0.0, 0.0, 1.0); 
			//extrag valorile normalizate pentru a nu avea probeleme cu instabilitatea numerica 
			FVector X_World = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(EarthX_ECEF).GetSafeNormal();
			FVector Y_World = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(EarthY_ECEF).GetSafeNormal();
			FVector Z_World = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(EarthZ_ECEF).GetSafeNormal();
			FRotator CamRot = PC101->PlayerCameraManager->GetCameraRotation();
			FRotationMatrix CamMat(CamRot);
			FVector X_Cam = CamMat.InverseTransformVector(X_World);
			FVector Y_Cam = CamMat.InverseTransformVector(Y_World);
			FVector Z_Cam = CamMat.InverseTransformVector(Z_World);
			struct FGizmoAxis { FVector Dir; FString Lbl; FLinearColor Col; };
			TArray<FGizmoAxis> Axes;
			Axes.Add({ X_Cam, TEXT("0\u00B0"), FLinearColor(1.0f, 0.2f, 0.2f) });  
			Axes.Add({ Y_Cam, TEXT("90\u00B0"), FLinearColor(0.2f, 0.5f, 1.0f) }); 
			Axes.Add({ Z_Cam, TEXT("N"), FLinearColor(0.2f, 1.0f, 0.2f) });    
			Axes.Sort([](const FGizmoAxis& A, const FGizmoAxis& B) {
				return A.Dir.X > B.Dir.X;
				});
			DrawText(TEXT("GLOBE 3D"), FLinearColor(0.7f, 0.7f, 0.7f), GizmoX - S(28.0f), GizmoY - GizmoRadius - S(20.0f), HUDFont, S(0.9f));
			//desenare axe si setare grosime
			for (const FGizmoAxis& Axis : Axes)
			{
				float ScreenX = Axis.Dir.Y * GizmoRadius;
				float ScreenY = -Axis.Dir.Z * GizmoRadius; 
				float EndX = GizmoX + ScreenX;
				float EndY = GizmoY + ScreenY;
				float DepthFactor = FMath::Clamp((Axis.Dir.X * -1.0f) + 1.0f, 0.5f, 1.5f);
				float Thickness = 2.5f * DepthFactor;
				FLinearColor FinalColor = Axis.Col;
				FinalColor.A = FMath::Clamp(DepthFactor, 0.3f, 1.0f);
				DrawThickLine(Canvas, GizmoX, GizmoY, EndX, EndY, Thickness, FinalColor);
				DrawRect(FinalColor, EndX - S(2.0f), EndY - S(2.0f), S(4.0f), S(4.0f));
				DrawText(Axis.Lbl, FinalColor, EndX + S(4.0f), EndY - S(8.0f), HUDFont, S(1.0f));
			}
			DrawRect(FLinearColor::White, GizmoX - S(2.0f), GizmoY - S(2.0f), S(4.0f), S(4.0f));
		}
		//aici desenez scara (fiecare harta are o scara)
		//deoarece eu ma pot duce de la distante mici la distante uriase, am setat mai multe valori in scalevalues
		if (Drona->bMapModeActive && Drona->MapSpringArm && Drona->MapCamera)
		{
			double CameraDistanceCm = FVector::Distance(Drona->MapCamera->GetComponentLocation(), Drona->GetActorLocation());
			APlayerController* PC123 = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			float FOV = 90.0f;
			if (PC123 && PC123->PlayerCameraManager)
			{
				FOV = PC123->PlayerCameraManager->GetFOVAngle();
			}
			double HalfFOVRads = FMath::DegreesToRadians(FOV / 2.0);
			double WorldWidthMeters = (2.0 * CameraDistanceCm * FMath::Tan(HalfFOVRads)) / 100.0;
			double MetersPerPixel = WorldWidthMeters / (double)Canvas->ClipX;
			double TargetMeters = MetersPerPixel * 150.0;
			double ScaleValues[] = { 0.000001,0.000002,0.000005,0.00001,0.00002,0.00005 ,0.0001,0.0002,0.0005,0.001,0.002,0.005,0.01,0.02,0.05,0.1,0.2,0.5, 1.0, 2.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0, 10000.0, 25000.0, 50000.0,
				100000.0, 250000.0, 500000.0, 1000000.0, 2500000.0, 5000000.0,
				10000000.0, 25000000.0, 50000000.0,
				100000000.0, 250000000.0, 500000000.0,
				1000000000.0, 2500000000.0, 5000000000.0 };
			double BestScaleMeters = ScaleValues[0];
			for (double Val : ScaleValues)
			{
				if (TargetMeters <= Val)
				{
					BestScaleMeters = Val;
					break;
				}
			}
			float ScalePixels = S((float)(BestScaleMeters / MetersPerPixel));
			FString ScaleText;
			//aici setez unitatile de masura
			if (BestScaleMeters >= 1000.0)
			{
				int64 ValoareKm = (int64)(BestScaleMeters / 1000.0);
				FString NumarFormatat = FString::Printf(TEXT("%lld"), ValoareKm);
				for (int32 i = NumarFormatat.Len() - 3; i > 0; i -= 3)
				{
					NumarFormatat.InsertAt(i, TEXT("."));
				}
				ScaleText = FString::Printf(TEXT("%s km"), *NumarFormatat);
			}
			else if (BestScaleMeters >= 1.0)
			{
				ScaleText = FString::Printf(TEXT("%lld m"), (int64)BestScaleMeters);
			}
			else if (BestScaleMeters >= 0.01)
			{
				int64 ValoareCm = FMath::RoundToInt(BestScaleMeters * 100.0);
				ScaleText = FString::Printf(TEXT("%lld cm"), ValoareCm);
			}
			else if (BestScaleMeters >= 0.001)
			{
				int64 ValoareMm = FMath::RoundToInt(BestScaleMeters * 1000.0);
				ScaleText = FString::Printf(TEXT("%lld mm"), ValoareMm);
			}
			else
			{
				int64 ValoareMicroni = FMath::RoundToInt(BestScaleMeters * 1000000.0);
				ScaleText = FString::Printf(TEXT("%lld \u03BCm"), ValoareMicroni);
			}
			float StartX = Canvas->ClipX - ScalePixels - S(50.0f);
			float EndX = Canvas->ClipX - S(50.0f);
			float LineY = CenterY + CompassRadius + S(60.0f);
			FLinearColor ScaleColor = FLinearColor::White;
			DrawThickLine(Canvas, StartX, LineY, EndX, LineY, S(3.0f), ScaleColor);
			DrawThickLine(Canvas, StartX, LineY - S(10.0f), StartX, LineY, S(3.0f), ScaleColor);
			DrawThickLine(Canvas, EndX, LineY - S(10.0f), EndX, LineY, S(3.0f), ScaleColor);
			float TextOffset = (ScaleText.Len() * 5.0f); 
			DrawText(ScaleText, ScaleColor, StartX + (ScalePixels / 2.0f) - TextOffset, LineY - 25.0f, HUDFont, 1.2f);
		}
	}
	bool bUpdateRaycastsThisFrame = false;
	RaycastTimer += GetWorld()->GetDeltaSeconds();
	if (RaycastTimer >= 1.0f) 
	{
		bUpdateRaycastsThisFrame = true;
		RaycastTimer = 0.0f;
	}
	bool bCheckMountains = true;
	ACesiumGeoreference* GeoRefOpt = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APawn* PlayerPawnOpt = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (GeoRefOpt && PlayerPawnOpt)
	{
		FVector LLH = GeoRefOpt->TransformUnrealPositionToLongitudeLatitudeHeight(PlayerPawnOpt->GetActorLocation());
		if (LLH.Z >= 1500.0)
		{
			bCheckMountains = false;
		}
	}
	APlayerController* PC = GetOwningPlayerController();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		AOrbitalManager* Manager = CachedOrbManager;
		//bshowstarlabeels este o valoare booleana declarata in orbital manager, scopul acestui cod este sa deseneze etichetele stelelor pe cer
		if (MyDrone && Manager && Manager->bShowStarLabels)
		{
			FVector Center = MyDrone->GetActorLocation();
			if (PC && PC->PlayerCameraManager)
			{
				Center = PC->PlayerCameraManager->GetCameraLocation();
			}
			float ProjectionRadius = 50000000000.0f;
			float TraceRadius = 5000000.0f; 
			float ZClip = Center.Z - TraceRadius;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(MyDrone);
			Params.bTraceComplex = true;
			for (const FStarHUDLabel& Label : Manager->HUDStarLabels)
			{
				FVector RotatedDir = Manager->CurrentStarDomeMatrix.GetMatrixWithoutScale().TransformVector(Label.LocalPosition);
				FVector VisualWorldPos = Center + (RotatedDir * ProjectionRadius);
				FVector TraceEndPos = Center + (RotatedDir * TraceRadius);
				if (VisualWorldPos.Z > ZClip)
				{
					FVector ScreenPos = Project(VisualWorldPos);
					if (ScreenPos.Z > 0.0f)
					{
						bool bHitMountain = false;
						if (!bCheckMountains)
						{
							bHitMountain = false;
						}
						else if (bUpdateRaycastsThisFrame)
						{
							FHitResult Hit;
							bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, Center, TraceEndPos, ECC_Visibility, Params);
							VisibilityCache.Add(Label.Name, bHitMountain); 
						}
						else
						{
							if (bool* CachedHit = VisibilityCache.Find(Label.Name))
							{
								bHitMountain = *CachedHit;
							}
						}
						if (!bHitMountain)
						{
							DrawText(Label.Name, FColor(0, 255, 255), ScreenPos.X + 10.0f, ScreenPos.Y - 15.0f, nullptr, 1.5f, false);
						}
					}
				}
			}
		}
		if (MyDrone && Manager && !MyDrone->bMapModeActive && MyDrone->bShowPlanets)
		{
			//aici desenez etichetele planetelor, trace radius este de 500.000 km, desenez totul la 500k km sa nu am probleme de randare
			FVector Center = MyDrone->GetActorLocation();
			APlayerController* PC_Cam = Cast<APlayerController>(MyDrone->GetController());
			if (PC_Cam && PC_Cam->PlayerCameraManager)
			{
				Center = PC_Cam->PlayerCameraManager->GetCameraLocation();
			}
			float TraceRadius = 50000000000.0f; 
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(MyDrone);
			Params.bTraceComplex = true;
			for (const auto& Planet : Manager->PlanetsList)
			{
				FVector VisualWorldPos = Center + (Planet.Direction * 500000000.0f);
				FVector TraceEndPos = Center + (Planet.Direction * TraceRadius);
				FVector ScreenPos = Project(VisualWorldPos);
				if (ScreenPos.Z > 0.0f)
				{
					bool bHitMountain = false;
					if (!bCheckMountains) bHitMountain = false;
					else if (bUpdateRaycastsThisFrame)
					{
						FHitResult Hit;
						bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, Center, TraceEndPos, ECC_Visibility, Params);
						VisibilityCache.Add(Planet.Name, bHitMountain);
					}
					else if (bool* CachedHit = VisibilityCache.Find(Planet.Name)) bHitMountain = *CachedHit;
					if (!bHitMountain)
					{
						float TWidth, THeight;
						GetTextSize(Planet.Name, TWidth, THeight, HUDFont, 1.5f);
						DrawText(Planet.Name, Planet.Color, ScreenPos.X - (TWidth / 2.0f), ScreenPos.Y - (THeight / 2.0f), HUDFont, 1.5f, false);
					}
				}
			}
		}
		FVector Center2 = MyDrone->GetActorLocation();
		APlayerController* PC_Cam2 = Cast<APlayerController>(MyDrone->GetController());
		float TraceRadius2 = 50000000000.0f;
		FCollisionQueryParams Params2;
		Params2.AddIgnoredActor(MyDrone);
		Params2.bTraceComplex = true;
		if (PC_Cam2 && PC_Cam2->PlayerCameraManager)
		{
			Center2 = PC_Cam2->PlayerCameraManager->GetCameraLocation();
		}
		if (Manager->bShowMoonLabels)
		{
			int32 CurrentYear = Manager->CurrentDate.Year;
			for (const auto& Moon : Manager->MajorMoonsList)
			{
				FString RawName = Moon.Name.TrimStartAndEnd();
				bool bIsEarthMoon = RawName.Equals(TEXT("Luna"), ESearchCase::IgnoreCase) || RawName.Equals(TEXT("Moon"), ESearchCase::IgnoreCase);
				if (!bIsEarthMoon && (CurrentYear < 1600 || CurrentYear > 2500))
				{
					continue; 
				}
				FVector VisualWorldPos = Center2 + (Moon.Direction * 500000000.0f);
				FVector TraceEndPos = Center2 + (Moon.Direction * TraceRadius2);
				FVector ScreenPos = Project(VisualWorldPos);
				if (ScreenPos.Z > 0.0f)
				{
					bool bHitMountain = false;
					if (!bCheckMountains) bHitMountain = false;
					else if (bUpdateRaycastsThisFrame)
					{
						FHitResult Hit;
						bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, Center2, TraceEndPos, ECC_Visibility, Params2);
						VisibilityCache.Add(Moon.Name, bHitMountain);
					}
					else if (bool* CachedHit = VisibilityCache.Find(Moon.Name)) bHitMountain = *CachedHit;
					//aici extrag numele clean al planetei
					if (!bHitMountain)
					{
						FString NumeRo = Moon.Name;
						FString NumeEn = NumeRo;
						FString CleanName = RawName; 
						if (CleanName.Equals(TEXT("Mercur"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Mercury");
						else if (CleanName.Equals(TEXT("Marte"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Mars");
						else if (CleanName.Equals(TEXT("Pamant"), ESearchCase::IgnoreCase) || CleanName.Equals(TEXT("Terra"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Earth");
						else if (CleanName.Equals(TEXT("Neptun"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Neptune");
						else if (CleanName.Equals(TEXT("Soare"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Sun");
						else if (CleanName.Equals(TEXT("Luna"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Moon");
						else if (CleanName.Equals(TEXT("Calisto"), ESearchCase::IgnoreCase)) NumeEn = TEXT("Callisto");
						else if (CleanName.Equals(TEXT("Ganimede"), ESearchCase::IgnoreCase) || (CleanName.Equals(TEXT("Ganymede"), ESearchCase::IgnoreCase))) {
							NumeEn = TEXT("Ganymede");
							NumeRo = TEXT("Ganimede");
						}
						//setez culorile pentru etichetele satelitilor naturali
						FString NumeFinal = AOrbitalManager::TrString(NumeRo, NumeEn);
						FColor MoonColor = FColor(200, 200, 200); 
						if (NumeEn == TEXT("Io"))
							MoonColor = FColor(255, 215, 0); 
						else if (NumeEn == TEXT("Europa"))
							MoonColor = FColor(135, 206, 250); 
						else if (NumeEn == TEXT("Ganymede"))
							MoonColor = FColor(180, 170, 160); 
						else if (NumeEn == TEXT("Callisto"))
							MoonColor = FColor(150, 120, 90); 
						else if (NumeEn == TEXT("Titan"))
							MoonColor = FColor(255, 140, 0); 
						else if (NumeEn == TEXT("Triton"))
							MoonColor = FColor(127, 255, 212); 
						else if (NumeEn == TEXT("Moon"))
							MoonColor = FColor(230, 230, 250); 
						float TWidth, THeight;
						GetTextSize(NumeFinal, TWidth, THeight, HUDFont, 1.5f);
						DrawText(NumeFinal, MoonColor, ScreenPos.X - (TWidth / 2.0f), ScreenPos.Y - (THeight / 2.0f), HUDFont, 1.5f, false);
					}
				}
			}
		}
		//aici desenez etichetele pentru soare si luna
		if (MyDrone && Manager && !Drona->bMapModeActive && MyDrone->bShowPlanets == true)
		{
			FVector CameraPosUnreal = MyDrone->GetActorLocation();
			if (PC && PC->PlayerCameraManager)
			{
				CameraPosUnreal = PC->PlayerCameraManager->GetCameraLocation();
			}
			float TraceRadius = 5000000.0f; 
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(MyDrone);
			Params.bTraceComplex = true;
			if (Manager->SunSkyActor)
			{
				UDirectionalLightComponent* SunLight = Manager->SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
				if (SunLight)
				{
					//extrag directia soarelui si pun in directia opusa numele stelei (soare sau sun)
					//daca soarele e ascuns dupa munti sau e apus, nu desenez eticheta
					FVector SunDir = -(SunLight->GetForwardVector());
					FVector SunVisualPos = CameraPosUnreal + (SunDir * 500000000.0f); 
					FVector SunTraceEnd = CameraPosUnreal + (SunDir * TraceRadius);
					FVector ScreenPos = Project(SunVisualPos);
					if (ScreenPos.Z > 0.0f)
					{
						bool bHitMountain = false;
						if (!bCheckMountains) bHitMountain = false;
						else if (bUpdateRaycastsThisFrame)
						{
							FHitResult Hit;
							bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, CameraPosUnreal, SunTraceEnd, ECC_Visibility, Params);
							VisibilityCache.Add(TEXT("Soare"), bHitMountain);
						}
						else if (bool* CachedHit = VisibilityCache.Find(TEXT("Soare"))) bHitMountain = *CachedHit;
						if (!bHitMountain)
						{
							FString NumeSoare = AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun"));
							float TWidth, THeight;
							GetTextSize(NumeSoare, TWidth, THeight, HUDFont, 1.5f);
							DrawText(NumeSoare, FColor::Yellow, ScreenPos.X - (TWidth / 2.0f), ScreenPos.Y - (THeight / 2.0f), HUDFont, 1.5f, false);
						}
					}
				}
			}
			//aici fac aceeasi poveste cu luna
			if (Manager->ControlledMoon)
			{
				//daca luna nu e vizibila pe cer, nu ii scriu eticheta
				//extrag directia controlledmoon si asez eticheta pe ecran
				FVector MoonDir = (Manager->ControlledMoon->GetActorLocation() - CameraPosUnreal).GetSafeNormal();
				FVector MoonVisualPos = CameraPosUnreal + (MoonDir * 500000000.0f);
				FVector MoonTraceEnd = CameraPosUnreal + (MoonDir * TraceRadius);
				FVector ScreenPos = Project(MoonVisualPos);
				if (ScreenPos.Z > 0.0f)
				{
					bool bHitMountain = false;
					if (!bCheckMountains) bHitMountain = false;
					else if (bUpdateRaycastsThisFrame)
					{
						FHitResult Hit;
						bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, CameraPosUnreal, MoonTraceEnd, ECC_Visibility, Params);
						VisibilityCache.Add(TEXT("Luna"), bHitMountain);
					}
					else if (bool* CachedHit = VisibilityCache.Find(TEXT("Luna"))) bHitMountain = *CachedHit;
					if (!bHitMountain)
					{
						FString NumeLuna = AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon"));
						float TWidth, THeight;
						GetTextSize(NumeLuna, TWidth, THeight, HUDFont, 1.5f);
						DrawText(NumeLuna, FColor::Silver, ScreenPos.X - (TWidth / 2.0f), ScreenPos.Y - (THeight / 2.0f), HUDFont, 1.5f, false);
					}
				}
			}
		}
		//aici desenez numele constelatiilor pe cer
		if (MyDrone && Manager && Manager->bShowConstellationLabels)
		{
			FVector Center = MyDrone->GetActorLocation();
			float ProjectionRadius = 50000000000.0f;
			float TraceRadius = 5000000.0f;
			float ZClip = Center.Z - TraceRadius;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(MyDrone);
			Params.bTraceComplex = true;
			//le desenez cu portocaliu
			FColor ConstColor = FColor(255, 180, 50);
			//iterez prin lista de constelatii
			for (const FConstellationHUDLabel& Label : Manager->HUDConstellationLabels)
			{
				FVector RotatedDir = Manager->CurrentStarDomeMatrix.GetMatrixWithoutScale().TransformVector(Label.LocalPosition);
				FVector VisualWorldPos = Center + (RotatedDir * ProjectionRadius);
				FVector TraceEndPos = Center + (RotatedDir * TraceRadius);
				if (VisualWorldPos.Z > ZClip)
				{
					FVector ScreenPos = Project(VisualWorldPos);
					if (ScreenPos.Z > 0.0f)
					{
						bool bHitMountain = false;
						if (!bCheckMountains)
						{
							bHitMountain = false;
						}
						//optimizare, verific daca este nevoie sa dau update la liniile trase pentru a vedea daca afisez sau nu numele constelatiilor (sunt ascunse de relief)
						else if (bUpdateRaycastsThisFrame)
						{
							FHitResult Hit;
							bHitMountain = GetWorld()->LineTraceSingleByChannel(Hit, Center, TraceEndPos, ECC_Visibility, Params);
							VisibilityCache.Add(Label.Name, bHitMountain);
						}
						else
						{
							if (bool* CachedHit = VisibilityCache.Find(Label.Name)) bHitMountain = *CachedHit;
						}
						if (!bHitMountain)
						{
							DrawText(Label.Name.ToUpper(), ConstColor, ScreenPos.X, ScreenPos.Y, HUDFont, 2.0f, false);
						}
					}
				}
			}
		}
	}
	//aici desenez grilele azimutale
	APlayerController* PC122 = GetOwningPlayerController();
	ADronaMea* MyDrone122 = Cast<ADronaMea>(GetOwningPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone122 && (MyDrone122->bShowAzimuthGrid || MyDrone122->bShowEquatorialGrid))
	{
		FVector CameraLocation = MyDrone122->GetActorLocation();
		if (PC122 && PC122->PlayerCameraManager)
		{
			CameraLocation = PC122->PlayerCameraManager->GetCameraLocation();
		}
		float GridRadius = 200000000.0f;
		ULineBatchComponent* AstroLineBatcher = MyDrone122->FindComponentByClass<ULineBatchComponent>();
		if (!AstroLineBatcher)
		{
			AstroLineBatcher = NewObject<ULineBatchComponent>(MyDrone122);
			AstroLineBatcher->RegisterComponent();
		}
		AstroLineBatcher->Flush();
		FCollisionQueryParams TextParams;
		TextParams.bTraceComplex = false; 
		TArray<FBatchedLine> BatchedLines;
		BatchedLines.Reserve(4000); 
		//functie lambda pentru a desena un segment in mod eficient pentru grilele astronomice
		auto DrawGridSegment3D = [&](FVector StartDir, FVector EndDir, FLinearColor LineColor)
			{
				FVector WorldStart = CameraLocation + (StartDir * GridRadius);
				FVector WorldEnd = CameraLocation + (EndDir * GridRadius);
				BatchedLines.Emplace(WorldStart, WorldEnd, LineColor, 0.2f, 375000.f, 0);
			};
		//aici desenez textul pe grila, adica cate grade/minute are
		auto DrawGridText = [&](FVector WorldDir, FString Lbl, FColor TxtColor, float FontScale)
			{
				FVector WorldPos = CameraLocation + (WorldDir * GridRadius);
				FVector ScreenPos = Project(WorldPos);
				if (ScreenPos.Z > 0.0f)
				{
					FHitResult Hit;
					if (!GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, WorldPos, ECC_Visibility, TextParams))
					{
						float TWidth, THeight;
						GetTextSize(Lbl, TWidth, THeight, HUDFont, FontScale);
						DrawText(Lbl, TxtColor, ScreenPos.X - (TWidth / 2.0f), ScreenPos.Y - (THeight / 2.0f), HUDFont, FontScale, false);
					}
				}
			};
		//aici testez daca utilizatorul a bifat in overlays & telemetrie optiunea de a vedea grilele azimutale
		//aici as fi putut sa dezvolt, cand dau zoom sa se zoomeze si grilele alea, adica mai pe scurt sa fie fix ca la stellarium
		if (MyDrone122->bShowAzimuthGrid)
		{
			FLinearColor AziColor(0.0f, 0.6f, 0.6f, 0.6f);
			//aici sunt liniile orizontale
			for (int32 Alt = -20; Alt <= 85; Alt += 5)
			{
				float AltRad = FMath::DegreesToRadians((float)Alt);
				FVector PrevDir(-1, -1, -1);
				for (int32 Az = 0; Az <= 360; Az += 5) //din 5 in 5 consider ca e suficiet
				{
					float AzRad = FMath::DegreesToRadians((float)Az);
					FVector CurrentDir(FMath::Cos(AzRad) * FMath::Cos(AltRad), FMath::Sin(AzRad) * FMath::Cos(AltRad), FMath::Sin(AltRad));
					if (Az > 0) DrawGridSegment3D(PrevDir, CurrentDir, AziColor);
					PrevDir = CurrentDir;
					if (Az == 180)
					{
						FString Prefix = (Alt > 0) ? TEXT("+") : TEXT("");
						DrawGridText(CurrentDir, FString::Printf(TEXT("%s%d\u00B0"), *Prefix, Alt), FColor(0, 150, 150), 1.0f);
					}
				}
			}
			//aici desenez liniile azimutului (verticale)
			for (int32 Az = 0; Az < 360; Az += 5)
			{
				float AzRad = FMath::DegreesToRadians((float)Az);
				FVector PrevDir(-1, -1, -1);
				for (int32 Alt = -25; Alt <= 90; Alt += 5)
				{
					float AltRad = FMath::DegreesToRadians((float)Alt);
					FVector CurrentDir(FMath::Cos(AzRad) * FMath::Cos(AltRad), FMath::Sin(AzRad) * FMath::Cos(AltRad), FMath::Sin(AltRad));
					if (Alt > -25) DrawGridSegment3D(PrevDir, CurrentDir, AziColor);
					PrevDir = CurrentDir;
					if (Alt == 0)
					{
						FString Lbl;
						if (Az == 0) Lbl = TEXT("N (0\u00B0)");
						else if (Az == 90) Lbl = TEXT("E (90\u00B0)");
						else if (Az == 180) Lbl = TEXT("S (180\u00B0)");
						else if (Az == 270) Lbl = AOrbitalManager::TrString(TEXT("V (270\u00B0)"), TEXT("W (270\u00B0)"));
						else Lbl = FString::Printf(TEXT("%d\u00B0"), Az);
						DrawGridText(CurrentDir, Lbl, FColor::Yellow, 1.2f);
					}
				}
			}
		}
		//aici desenez grila ecuatoriala, e in medie cu 45 de grade decalata fata de cea azimutala
		//Nota: In mare parte din timp am testat la latitudine 45 de grade, unde este Bucurestiul, iar eclipsa am studiat-o intens pe cea din Spania, pe 12 august 2026
		if (MyDrone122->bShowEquatorialGrid && GeoRef)
		{
			FLinearColor EqColor(0.6f, 0.0f, 0.6f, 0.6f);
			FVector CoordsLLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(CameraLocation);
			double Latitude = CoordsLLH.Y;
			FRotator EquatorialTilt = FRotator(Latitude - 90.0f, 0.0f, 0.0f);
			//linii orizontale
			for (int32 Dec = -85; Dec <= 85; Dec += 5)
			{
				float DecRad = FMath::DegreesToRadians((float)Dec);
				FVector PrevDir(-1, -1, -1);
				//linii verticale
				for (int32 RA = 0; RA <= 360; RA += 5)
				{
					float RARad = FMath::DegreesToRadians((float)RA);
					FVector LocalDir(FMath::Cos(RARad) * FMath::Cos(DecRad), FMath::Sin(RARad) * FMath::Cos(DecRad), FMath::Sin(DecRad));
					FVector CurrentDir = EquatorialTilt.RotateVector(LocalDir);
					if (RA > 0) DrawGridSegment3D(PrevDir, CurrentDir, EqColor);
					PrevDir = CurrentDir;
					if (RA == 180)
					{
						FString Prefix = (Dec > 0) ? TEXT("+") : TEXT("");
						DrawGridText(CurrentDir, FString::Printf(TEXT("%s%d\u00B0"), *Prefix, Dec), FColor(150, 0, 150), 1.0f);
					}
				}
			}
			for (int32 RA = 0; RA < 360; RA += 5)
			{
				float RARad = FMath::DegreesToRadians((float)RA);
				FVector PrevDir(-1, -1, -1);
				for (int32 Dec = -90; Dec <= 90; Dec += 5)
				{
					float DecRad = FMath::DegreesToRadians((float)Dec);
					FVector LocalDir(FMath::Cos(RARad) * FMath::Cos(DecRad), FMath::Sin(RARad) * FMath::Cos(DecRad), FMath::Sin(DecRad));
					FVector CurrentDir = EquatorialTilt.RotateVector(LocalDir);
					if (Dec > -90) DrawGridSegment3D(PrevDir, CurrentDir, EqColor);
					PrevDir = CurrentDir;
					if (Dec == 0)
					{
						int32 Ore = RA / 15;
						int32 Minute = ((RA % 15) * 60) / 15;
						FString Lbl = (Minute == 0) ? FString::Printf(TEXT("%dh"), Ore) : FString::Printf(TEXT("%dh %dm"), Ore, Minute);
						DrawGridText(CurrentDir, Lbl, FColor(150, 0, 150), 1.0f);
					}
				}
			}
		}
		//desenez liniile in batchuri ca optimizare extra
		if (BatchedLines.Num() > 0)
		{
			AstroLineBatcher->DrawLines(BatchedLines);
		}
	}
	//aici desenez butoanele din hud
	float VPW = 1920.0f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		VPW = ViewportSize.X;
	}

	float BtnSize = S(100.0f/DPIScale);
	float BtnY = S(15.0f/DPIScale); 
	float CanvasW = Canvas->ClipX / DPIScale;
	//float BtnX_Map = S(220.0f) + CanvasW * 8.0f / 100.f;
	float BtnX_Map = S(340.0f/DPIScale);
	//aici este butonul care face toggle intre harta si modul third person
	if (Drona->MainMenuWidget && Drona->MainMenuWidget->MenuDropdownContainer && Drona->MainMenuWidget->MenuDropdownContainer->GetVisibility() == ESlateVisibility::Visible)
	{
		//BtnX_Map = S(550.0f) + CanvasW * 8.0f / 100.f;
		BtnX_Map = S(700.0f/DPIScale);
	}
	//celelalte 3 butoane de desenat frumos
	//am ales iconita de 45 de pixeli
	//de asemenea, am iconita inclusiv in installer (am folosit inno setup), iar acolo poza are dimensiune 30*32
	float BtnX_Xav = BtnX_Map + BtnSize + S(15.0f);
	float BtnX_Cloud = BtnX_Xav + BtnSize + S(15.0f);
	float BtnX_Pause = BtnX_Cloud + BtnSize + S(15.0f);
	float IconSize = S(75.0f);
	float Offset = (BtnSize - IconSize) / 2.0f;
	auto DrawHUDButton = [&](float X, FLinearColor OutlineColor, UTexture2D* Icon, FString FallbackText)
		{
			DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.7f), X, BtnY, BtnSize, BtnSize);
			DrawRect(OutlineColor, X, BtnY, S(4.0f), BtnSize);
			if (Icon)
			{
				FCanvasTileItem Tile(FVector2D(X + Offset, BtnY + Offset), Icon->GetResource(), FVector2D(IconSize, IconSize), OutlineColor);
				Tile.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Tile);
			}
			else DrawText(FallbackText, OutlineColor, X + 10.0f, BtnY + 25.0f, HUDFont, 1.0f);
		};
	//afisez cu alb in stanga
	//apar texte ca fallback daca nu merg iconitele de incarcat, le-am lasat din faza de testing
	FLinearColor MapColor = Drona->bMapModeActive ? FLinearColor::Green : FLinearColor::White;
	DrawHUDButton(BtnX_Map, MapColor, IconitaHarta, TEXT("MAP"));
	bool bIsMenuOpen = Drona->EclipseInfoWidget && (Drona->EclipseInfoWidget->GetVisibility() == ESlateVisibility::Visible);
	FLinearColor MenuColor = bIsMenuOpen ? FLinearColor::Green : FLinearColor::White;
	DrawHUDButton(BtnX_Xav, MenuColor, IconitaMeniu, TEXT("MENU"));
	/*bool bCloudsVisible = true;
	TArray<AActor*> Clouds;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Clouds);
	for (AActor* Actor : Clouds)
	{
		if (Actor->GetName().Contains("Cloud") && Actor->IsHidden()) bCloudsVisible = false;
	}
	FLinearColor CloudColor = bCloudsVisible ? FLinearColor::Green : FLinearColor::White;
	DrawHUDButton(BtnX_Cloud, CloudColor, IconitaNori, TEXT("CLD"));
	*/
	bool bCloudsVisible = CachedCloudActor.IsValid() ? !CachedCloudActor->IsHidden() : true;
	FLinearColor CloudColor = bCloudsVisible ? FLinearColor::Green : FLinearColor::White;
	DrawHUDButton(BtnX_Cloud, CloudColor, IconitaNori, TEXT("CLD"));

	
	bool bIsTimePaused = false;
	if (CachedOrbManager)
	{
		bIsTimePaused = (CachedOrbManager->TimeSpeed == 0.0f);
	}
	FLinearColor PauseColor = bIsTimePaused ? FLinearColor::Red : FLinearColor::Green;
	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.7f), BtnX_Pause, BtnY, BtnSize, BtnSize);
	DrawRect(PauseColor, BtnX_Pause, BtnY, S(4.0f), BtnSize);
	float CenterX = BtnX_Pause + (BtnSize / 2.0f);
	float CenterY = BtnY + (BtnSize / 2.0f);
	//daca butonul este pe pauza, desenez play, format din un triunghi
	if (bIsTimePaused)
	{
		FVector2D Point1(CenterX - S(10.0f), CenterY - S(23.3f)); 
		FVector2D Point2(CenterX - S(10.0f), CenterY + S(23.3f)); 
		FVector2D Point3(CenterX + S(26.6f), CenterY);        
		FCanvasTriangleItem TriItem(Point1, Point2, Point3, Canvas->DefaultTexture->GetResource());
		TriItem.SetColor(PauseColor);
		Canvas->DrawItem(TriItem);
	}
	else
	{
		//aici desenez 2 bare, ca semnul de pauza
		//DrawRect(PauseColor, CenterX - S(10.0f), CenterY - S(12.0f), S(10.0f), S(40.0f));
		//DrawRect(PauseColor, CenterX + S(4.0f), CenterY - S(12.0f), S(10.0f), S(40.0f));
		float BarWidth = S(10.0f);
		float BarHeight = S(40.0f);
		float BarSpacing = S(7.0f);
		DrawRect(PauseColor, CenterX - BarSpacing - BarWidth, CenterY - BarHeight / 2.0f, BarWidth, BarHeight);
		DrawRect(PauseColor, CenterX + BarSpacing, CenterY - BarHeight / 2.0f, BarWidth, BarHeight);
	}
}

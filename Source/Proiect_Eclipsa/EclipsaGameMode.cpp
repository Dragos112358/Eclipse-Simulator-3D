#include "EclipsaGameMode.h"
#include "DronaMea.h"
#include "DroneHUD.h"
#include "Moon.h"
#include "OrbitalManager.h"
#include "Kismet/GameplayStatics.h"
AEclipsaGameMode::AEclipsaGameMode()
{
	//aici am setat clasele mele custom
	//drona mea controleaza drona si tot ce tine de teleportare, clickuri si deplasare
	//Hud-ul afiseaza informatii utile pe ecran, avand extra o busola, gizmo si afisare nume de planete, sateliti naturali, stele si constelatii
	//moon class imi definesc luna, ce mesh are, iar inainte sa am backendul de python, acea clasa controla si deplasarea lunii pe cer
	/*orbital manager este o clasa foarte complexa si importanta, ce include toate functionalitatile meniurilor(aici includ partea de afiseare banda de totalitate, afisare con de umbra,
	* afisare proiectie pe suprafata pamantului, comunicarea cu serverul de python, plasarea lunii, a soarelui si a planetelor, crearea de ismc-uri pentru a reprezenta eficient
	pe cer stele si constelatii.
	*/
	DefaultPawnClass = ADronaMea::StaticClass();
	HUDClass = ADroneHUD::StaticClass();
	DefaultMoonClass = AMoon::StaticClass();
	DefaultManagerClass = AOrbitalManager::StaticClass();
}
void AEclipsaGameMode::StartPlay()
{
	//in aceasta functie, ma asigur ca exista luna si toate clasele elementare sunt legate intre ele inainte de a prelua jucatorul controlul
	Super::StartPlay();
	AMoon* SpawnedMoon = nullptr;
	if (DefaultMoonClass)
	{
		TArray<AActor*> FoundMoons;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), DefaultMoonClass, FoundMoons);
		if (FoundMoons.Num() > 0)
		{
			//daca exista deja luna, nu o mai plasez pe ecran
			//am avut problema in care aveam mai multe luni pe cer plasate la coordonate ciudate, asa ca am ales sa fac verificarea de aici
			SpawnedMoon = Cast<AMoon>(FoundMoons[0]);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("GameMode: Am gasit o luna existenta."));
		}
		else
		{
			//aici plasez luna din Moon.cpp
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* NewActor = GetWorld()->SpawnActor<AActor>(DefaultMoonClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			SpawnedMoon = Cast<AMoon>(NewActor);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, TEXT("GameMode: Luna a fost creata automat!"));
		}
	}
	if (DefaultManagerClass)
	{
		//aici plasez OrbitalManager, care este cea mai complexa clasa dupa EclipseBrowserWidget, OrbitalManager are o mutltiudine de functionalitati pe care le-am descris mai sus
		AActor* FoundManager = UGameplayStatics::GetActorOfClass(GetWorld(), DefaultManagerClass);
		if (!FoundManager)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AOrbitalManager* NewManager = GetWorld()->SpawnActor<AOrbitalManager>(DefaultManagerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (NewManager)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("GameMode: Orbital Manager Activat!"));
				if (SpawnedMoon)
				{
					NewManager->ControlledMoon = SpawnedMoon;
				}
				TArray<AActor*> AllActors;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), AllActors);
				for (AActor* Actor : AllActors)
				{
					if (Actor && Actor->GetName().Contains("CesiumSunSky"))
					{
						NewManager->SunSkyActor = Actor;
						break;
					}
				}
			}
		}
	}
}

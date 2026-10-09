#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EclipsaGameMode.generated.h"
UCLASS()
class PROIECT_ECLIPSA_API AEclipsaGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AEclipsaGameMode();
	UPROPERTY(EditAnywhere, Category = "Classes")
	TSubclassOf<class AActor> DefaultMoonClass;
	UPROPERTY(EditDefaultsOnly, Category = "Classes")
	TSubclassOf<class AOrbitalManager> DefaultManagerClass;
protected:
	virtual void StartPlay() override;
};

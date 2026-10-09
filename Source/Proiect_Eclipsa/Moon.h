#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CesiumGeoreference.h" 
#include "Moon.generated.h"
UCLASS()
class PROIECT_ECLIPSA_API AMoon : public AActor
{
	GENERATED_BODY()
public:
	AMoon();
protected:
	virtual void BeginPlay() override;
public:
	virtual void Tick(float DeltaTime) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Moon")
	UStaticMeshComponent* MoonMesh;
	UPROPERTY(EditAnywhere, Category = "Moon")
	UMaterialInterface* MoonMaterial;
	UPROPERTY(EditAnywhere, Category = "Cesium")
	ACesiumGeoreference* GeoRef; //folosit de cesium pentru a putea extrage locatii
	UPROPERTY(EditAnywhere, Category = "Astronomy")
	double DistanceMeters = 384400000.0; //distanta medie pamant-luna
	UPROPERTY(EditAnywhere, Category = "Astronomy")
	double OrbitSpeed = 0.005; //valori de test, nu le mai folosesc
	double CurrentOrbitAngle = 0.0;
	UFUNCTION(BlueprintCallable, Category = "Moon")
	void ToggleEclipseMode();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon")
	bool bEclipseModeActive = false; 
	bool bShowAlignmentDebug = false;
	UPROPERTY(EditAnywhere, Category = "Astronomy")
	AActor* SunLightSource;
	bool bControlledByManager = true; //inainte era false, cand faceam teste la inceput
private:
	float InitialSunIntensity = -1.0f; 
	float MinLightIntensity = 0.5f;   
};

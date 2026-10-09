#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "OrbitalManager.h"
#include "Components/CheckBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Border.h"
#include "Components/WrapBox.h"
#include "Components/MenuAnchor.h"
#include "Components/TextBlock.h"
#include "EclipseBrowserWidget.generated.h"
//enumeratie cu cele 4 tipuri de eclipse
UENUM(BlueprintType)
enum class EEclipseType : uint8
{
	Total,
	Partial,
	Annular,
	Hybrid
};
USTRUCT(BlueprintType)
//date despre eclipsa, fiecare are un nume, o data, un tip, o obscuratie, serie Saros si un loc de unde este vizibilia
struct FEclipseData
{
	GENERATED_BODY()
	FString Name;
	FDateTime Date;
	float Obscuration;
	EEclipseType Type;
	int32 SarosSeries;
	float Latitude;
	float Longitude;
	//adaugate acuma
	int32 RealYear;
	int32 Day;
	int32 Month;
	int32 TargetYear;
	float Hour;
	FString FormattedDateStr;
	int32 SourceIndex = -1; //nou adaugat
};
//enumeratie pentru meniurile ce pot fi apelate din meniul de help (f10)
UENUM(BlueprintType)
enum class EMenuTarget : uint8
{
	None,
	Settings,
	Optics,
	Time,
	VisualFX,
	Bookmarks,
	Overlays,
	Browser,      
	Placement,    
	VideoExport,  
	Cameras,      
	Telemetry,     
	Weather,   
	Map2D,
	ExitApp,
	Credits
};
USTRUCT(BlueprintType)
//fiecare help are un titlu, o descriere, cuvinte cheie si meniul pe care il dschide
struct FHelpEntry
{
	GENERATED_BODY()
	UPROPERTY() FString Title;       
	UPROPERTY() FString Description; 
	UPROPERTY() FString Keywords;    
	UPROPERTY() EMenuTarget TargetMenu; 
};

// În fișierul tău .h (de preferat deasupra clasei AOrbitalManager):

USTRUCT(BlueprintType)
struct FLocalEclipseVisibility
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse Visibility")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse Visibility")
	bool bIsTotalOrAnnular = false;

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse Visibility")
	bool bIsAnnularLocal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse Visibility")
	float LocalMagnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse Visibility")
	double BestTHours = 0.0;
};


USTRUCT(BlueprintType)
//fiecare favorit are data, titlu, o descriere sumara, fovul camerei si tinta aleasa
struct FAstroBookmark
{
	GENERATED_BODY()
	UPROPERTY() FString Title;
	UPROPERTY() FString Description;
	UPROPERTY() int32 Year = 2026;
	UPROPERTY() int32 Month = 8;
	UPROPERTY() int32 Day = 12;
	UPROPERTY() int32 Hour = 12;
	UPROPERTY() int32 Minute = 0;
	UPROPERTY() double Latitude = 0.0;
	UPROPERTY() double Longitude = 0.0;
	UPROPERTY() double Altitude = 1000.0;
	UPROPERTY() float CameraFOV = 90.0f;
	UPROPERTY() FString TrackingTarget = "Libera (Niciunul)";
};

UENUM(BlueprintType)
enum class ELunarEclipseType : uint8
{
	PenumbralPartial,  // PNP - 1
	PenumbralTotal,    // PNT - 2
	UmbralPartial,     // UMP - 3
	UmbralTotal,       // UMT - 4
	UmbralCentral      // UMC - 5
};

// Enum pentru modul curent al catalogului
UENUM(BlueprintType)
enum class ECatalogMode : uint8
{
	Solar,
	Lunar
};

struct FEclipseCityHit
{
	FString CityName;
	FString Country;
	float Magnitude = 0.f;
	float Latitude = 0.f;
	float Longitude = 0.f;
	float Altitude = 0.f;
	double LocalTimeOffsetHours = 0.0; // <-- NOU
	float UtcHour = 0.0f; // <-- ORA EXACTĂ UTC A MAXIMULUI (ex: 17.7657)

};
class UEclipseJumpButton;
class UEditableTextBox;
//clasa ce imi ofera catalogul de eclipse
UCLASS()
class PROIECT_ECLIPSA_API UEclipseBrowserWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetEclipseData(const TArray<FEclipseData>& NewData);
	//void RequestJumpToTime(FDateTime TimeToJump, int32 RealYear);
	// Înlocuiește vechea declarație cu:
	void RequestJumpToTime(int32 JumpDay, int32 JumpMonth, int32 JumpYear, float JumpHour, bool bCloseWidget);
	void TeleportToEclipseLocation(float Latitude, float Longitude);
	void ApplySarosFilter(int32 SarosNumber);
	UFUNCTION() void ResetFilters();
	UFUNCTION() double ComputeGamma(const FBesselianElements& E);
	UFUNCTION() void RefreshFilterDropdownContent();

	void ShowEclipseDetails(const FEclipseData& Data);
	void ShowLunarEclipseDetails(const FLunarEclipseElements& E);
	void TeleportToExactLocation(float Latitude, float Longitude);


	// Butoane noi
	UPROPERTY()
	UButton* ExportCsvButton;

	UPROPERTY()
	UButton* ExportTxtButton;

	UPROPERTY()
	UButton* ToggleAllCitiesButton;

	// Indicator dacă afișăm doar orașele afectate sau absolut toate orașele
	bool bShowAllCities = false;

	// Funcții callback pentru butoane
	UFUNCTION()
	void OnExportCsvClicked();

	UFUNCTION()
	void OnExportTxtClicked();

	UFUNCTION()
	void OnToggleAllCitiesClicked();

	UFUNCTION(BlueprintCallable, Category = "Astronomy")
	FLocalEclipseVisibility ComputeLocalVisibility(
		const FBesselianElements& E,
		double LatDeg,
		double LonDegEast,
		double HeightMeters,
		double DeltaTSeconds);

	FLocalEclipseVisibility ComputeCountryVisibility(
		const FBesselianElements& E,
		const TArray<FCountryPoint>& Points, double DeltaTSeconds);

protected:
	UPROPERTY() UComboBoxString* SortComboBox;
	//UPROPERTY() UComboBoxString* FilterComboBox;

	//aici vine codul nou pentru bife

	/*UPROPERTY() UButton* BtnFilterTotal = nullptr;
	UPROPERTY() UButton* BtnFilterPartial = nullptr;
	UPROPERTY() UButton* BtnFilterAnnular = nullptr;
	UPROPERTY() UButton* BtnFilterHybrid = nullptr;*/

	//UPROPERTY() UButton* FilterMenuButton = nullptr;
	//UPROPERTY() UTextBlock* FilterMenuLabel = nullptr;
	UPROPERTY() UBorder* FilterDropdownPanel = nullptr;

	bool bFilterMenuOpen = false;

	UPROPERTY() UButton* FilterRowTotal = nullptr;
	UPROPERTY() UButton* FilterRowPartial = nullptr;
	UPROPERTY() UButton* FilterRowAnnular = nullptr;
	UPROPERTY() UButton* FilterRowHybrid = nullptr;
	UPROPERTY() UTextBlock* FilterRowTotalTxt = nullptr;
	UPROPERTY() UTextBlock* FilterRowPartialTxt = nullptr;
	UPROPERTY() UTextBlock* FilterRowAnnularTxt = nullptr;
	UPROPERTY() UTextBlock* FilterRowHybridTxt = nullptr;

	UFUNCTION() void OnFilterMenuButtonClicked();
	//void UpdateFilterMenuLabel();
	void RefreshFilterRowVisuals();
	UPROPERTY() UButton* FilterMenuButton = nullptr;
	UPROPERTY() UTextBlock* FilterMenuLabel = nullptr;
	UFUNCTION() UWidget* BuildFilterMenuContent();
	void UpdateFilterMenuLabel();

	bool bShowTotal = true;
	bool bShowPartial = true;
	bool bShowAnnular = true;
	bool bShowHybrid = true;

	UFUNCTION() void OnToggleFilterTotal();
	UFUNCTION() void OnToggleFilterPartial();
	UFUNCTION() void OnToggleFilterAnnular();
	UFUNCTION() void OnToggleFilterHybrid();
	void RefreshFilterButtonColors();


	UPROPERTY() UScrollBox* EclipseListScroll;
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() UButton* ResetFilterButton;
	UPROPERTY() UEditableTextBox* MinYearInput;
	UPROPERTY() UEditableTextBox* MaxYearInput;
	UPROPERTY() UButton* ApplyYearButton;
	UFUNCTION() void OnYearFilterClicked();
	TArray<FEclipseData> AllEclipses;


	ECatalogMode CurrentMode = ECatalogMode::Solar;
	UPROPERTY() UButton* TabSolarButton = nullptr;
	UPROPERTY() UButton* TabLunarButton = nullptr;
	UFUNCTION() void OnSwitchToSolar();
	UFUNCTION() void OnSwitchToLunar();
	UFUNCTION() void OnLunarRowClicked();
	FLinearColor GetLunarTypeColor(int32 TypeNumber);
	FString GetLunarTypeString(int32 TypeNumber);
	void RefreshTabColors();

	// filtre separate pentru cele 5 tipuri de eclipsa de luna
	bool bShowPenumbralPartial = true;
	bool bShowPenumbralTotal = true;
	bool bShowUmbralPartial = true;
	bool bShowUmbralTotal = true;
	bool bShowUmbralCentral = true;
	UFUNCTION() void OnToggleFilterPenumbralPartial();
	UFUNCTION() void OnToggleFilterPenumbralTotal();
	UFUNCTION() void OnToggleFilterUmbralPartial();
	UFUNCTION() void OnToggleFilterUmbralTotal();
	UFUNCTION() void OnToggleFilterUmbralCentral();

	void RefreshLunarList();
	void BuildLunarListRow(const FLunarEclipseElements& E);
	//UFUNCTION() void OnLunarRowClicked();

	TArray<FLunarEclipseElements> CachedFilteredLunar;
	int32 VisibleLunarCount = 0;
	TMap<UButton*, FLunarEclipseElements> LunarRowDataMap;
	FLunarEclipseElements CurrentDetailLunarEclipse;


	void RefreshList();
	void AddRow(const FEclipseData& Data);
	UFUNCTION() void OnSortChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	//UFUNCTION() void OnFilterChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnCloseClicked();
	TArray<FEclipseData> FullDataCache;
	int32 CurrentlyShownCount = 0;
	int32 ActiveSarosFilter = -9999;
	const int32 BatchSize = 50;
	UPROPERTY() UButton* LoadMoreButton;
	void RenderNextBatch(); 
	void RenderNextLunarBatch();
	UFUNCTION() void OnLoadMoreClicked();
	FString GetTypeString(EEclipseType Type);
	FLinearColor GetTypeColor(EEclipseType Type);

	//nou pentru partea de gasit eclipse la locatie
	/*UPROPERTY() UButton* LocationFilterToggle;
	bool bLocationFilterActive = false;
	double CurrentDroneLat = 0.0;
	double CurrentDroneLon = 0.0;
	double CurrentDroneAlt = 0.0;
	UFUNCTION() void OnToggleLocationFilter();*/

	enum class ELocationFilterMode : uint8 { None, Drone, Country };
	ELocationFilterMode LocationMode = ELocationFilterMode::None;

	UPROPERTY() UComboBoxString* LocationModeComboBox;
	double CurrentDroneLat = 0.0;
	double CurrentDroneLon = 0.0;
	double CurrentDroneAlt = 0.0;

	UFUNCTION() void OnLocationModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	FString ExtractCurrentCountryName() const;

	//pentru detalii eclipsa
	UPROPERTY() UWidgetSwitcher* RootSwitcher = nullptr;
	UPROPERTY() UBorder* DetailPanel = nullptr;
	UPROPERTY() UVerticalBox* DetailContentBox = nullptr;
	UPROPERTY() UButton* DetailBackButton = nullptr;


	UPROPERTY() UEditableTextBox* CitySearchInput = nullptr;
	UPROPERTY() UVerticalBox* CityListBox = nullptr;
	UPROPERTY() UTextBlock* CitySummaryText = nullptr;
	TArray<FEclipseCityHit> CachedCityHits;

	UFUNCTION() void OnCitySearchChanged(const FText& Text);
	void RenderCityList(const FString& FilterText);

	FEclipseData CurrentDetailEclipse;

	UFUNCTION() void OnDetailBackClicked();
	UWidget* BuildDetailPanel();
	void PopulateCityVisibility(const FEclipseData& Data);

	//chestii nou adaugate:
	void PopulateLunarCityVisibility(const FLunarEclipseElements& E);
	void PopulateCityVisibilityInternal(double TargetJDTT, bool bIsLunar);
	bool bLastCityQueryWasLunar = false;


};
//clasa pentru meniul principal (apare meniu in partea stanga sus a ecranului)
UCLASS()
class PROIECT_ECLIPSA_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	UFUNCTION() void OpenBrowser();
	UFUNCTION() void OpenTelemetryMenu();
	UFUNCTION() void CloseTelemetryMenu();
	UFUNCTION() void RebuildTelemetryForLanguage();
	UPROPERTY()
	UAstroTelemetryWidget* TelemetryMenuInstance;
	UPROPERTY() UButton* MenuToggleButton;
	UPROPERTY() UVerticalBox* MenuDropdownContainer;
protected:
	//aici sunt toate meniurile ce pot fi accesate din meniul principal
	UPROPERTY() UButton* OpenBrowserButton;
	UPROPERTY() UButton* OpenPlacementButton;
	UPROPERTY() UButton* OpenTimeButton;
	UPROPERTY() UButton* OpenOpticsButton;
	UPROPERTY() UButton* OpenOverlayButton; 
	UPROPERTY() UButton* OpenVideoExportButton;
	UPROPERTY() UButton* OpenCameraMenuButton;
	UPROPERTY() UButton* OpenSettingsMenuButton;
	UFUNCTION() void OpenSettingsMenu();
	UPROPERTY() UButton* OpenExitMenuButton;
	UFUNCTION() void OpenExitMenu();
	UFUNCTION() void OpenCameraMenu();
	UPROPERTY() UButton* OpenBookmarksButton;
	UPROPERTY() class UButton* OpenMapMenuButton;
	UFUNCTION() void OpenMapMenu();
	UFUNCTION() void OpenBookmarksMenu();
	UPROPERTY() UButton* OpenTelemetryButton;
	UPROPERTY() UButton* OpenVisualFXButton;
	UPROPERTY() UButton* OpenHelpButton; 
	UFUNCTION() void OpenHelpMenu();
	UPROPERTY() UButton* OpenWeatherButton;
	UFUNCTION() void OpenWeatherMenu();
	UFUNCTION() void OpenVisualFXMenu();
	UFUNCTION() void ToggleMenu();
	UFUNCTION() void OpenPlacementMenu();
	UFUNCTION() void OpenTimeMenu();
	UFUNCTION() void OpenOpticsMenu();
	UFUNCTION() void OpenOverlayMenu();
	UFUNCTION() void OpenVideoExportMenu();
};
UCLASS()
class PROIECT_ECLIPSA_API UEclipseJumpButton : public UButton
{
	GENERATED_BODY()
public:
	//FDateTime TargetDate; 
	UPROPERTY()
	int32 TargetDay;

	UPROPERTY()
	int32 TargetMonth;

	UPROPERTY()
	int32 TargetYear;

	UPROPERTY()
	float TargetHour;
	int32 TargetSaros;    
	bool bIsSarosButton;  
	class UEclipseBrowserWidget* ParentBrowser;
	UFUNCTION()
	void HandleClick();
	UPROPERTY()
	float TargetLat;
	UPROPERTY()
	float TargetLon;
	int32 RealYear;

	//in clasa cu saros, adaug si asta pentru cel mai bun efect posibil
	UPROPERTY() bool bIsDetailsButton = false;
	UPROPERTY() bool bUseExactCoordinates = false;
	UPROPERTY() FEclipseData DetailsData;
};
UCLASS()
//clasa care imi plaseaza drona la anumite coordonate si altitudine
class PROIECT_ECLIPSA_API UPlacementMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPlacementMenuWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() UButton* TeleportButton;
	UPROPERTY() UButton* CurrentLocationButton;
	UPROPERTY() UComboBoxString* FormatComboBox;
	UPROPERTY() UComboBoxString* LatDirCombo;
	UPROPERTY() UComboBoxString* LonDirCombo;
	UPROPERTY() UEditableTextBox* LatDegInput;
	UPROPERTY() UEditableTextBox* LatMinInput;
	UPROPERTY() UEditableTextBox* LatSecInput;
	UPROPERTY() UEditableTextBox* LonDegInput;
	UPROPERTY() UEditableTextBox* LonMinInput;
	UPROPERTY() UEditableTextBox* LonSecInput;
	UPROPERTY() UEditableTextBox* AltInput;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnFormatChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnTeleportClicked();
	UFUNCTION() void OnCurrentLocationClicked();
	UPROPERTY()
	class UButton* EclipseButton;
	UFUNCTION()
	void OnTeleportToEclipseClicked();
	UPROPERTY() class UImage* MapImage;
	UPROPERTY() class UBorder* MapClippingBorder;
	UPROPERTY() class UTexture2D* WorldMapTexture;
	float MapZoom;
	FVector2D MapPan;
	bool bIsPanning;
	bool bHasMarker;
	FVector2D LastMousePosition;
	FVector2D MarkerLatLon;
	FTimerHandle TeleportTimerHandle;
	void FinalizeTeleport(float RequestedZ, class ADronaMea* MyDrone);
	UFUNCTION() void UpdateInputsFromMarker();
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	
	//adaugate pentru cautare orase
	UPROPERTY()
	UEditableTextBox* CitySearchInput = nullptr;

	UPROPERTY()
	UComboBoxString* CityResultsComboBox = nullptr;

	// Harta locala pentru memorarea oraselor gasite la cautarea curenta
	TMap<FString, FEclipseCityHit> FoundCitiesMap;

	UFUNCTION()
	void OnCitySearchChanged(const FText& Text);

	UFUNCTION()
	void OnCitySelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	void ApplyCityCoordinates(const FEclipseCityHit& City);

};
//clasa pentru calendarul folosit la meniul de control timp
UCLASS()
class PROIECT_ECLIPSA_API UCalendarDayButton : public UButton
{
	GENERATED_BODY()
public:
	int32 DayNumber;
	UPROPERTY()
	UTimeControlWidget* ParentMenu;
	UFUNCTION()
	void HandleClick(); 
};
//clasa care imi controleaza controlul timpului
UCLASS()
class PROIECT_ECLIPSA_API UTimeControlWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() class UTextBlock* MonthYearLabel;
	UPROPERTY() UButton* PrevMonthBtn;
	UPROPERTY() UButton* NextMonthBtn;
	UPROPERTY() class UUniformGridPanel* CalendarGrid;
	UPROPERTY() UEditableTextBox* HourInput;
	UPROPERTY() UEditableTextBox* MinInput;
	UPROPERTY() UEditableTextBox* SecInput;
	UPROPERTY() UButton* ApplyTimeBtn;
	UPROPERTY() UEditableTextBox* SolarHourInput;
	UPROPERTY() UEditableTextBox* SolarMinInput;
	UPROPERTY() UEditableTextBox* SolarSecInput;
	UPROPERTY() UButton* PlayFwdBtn;
	UPROPERTY() UButton* PlayRevBtn;
	UPROPERTY() UButton* PauseBtn;
	UPROPERTY() UEditableTextBox* SpeedInput;
	UPROPERTY() UButton* MultiplySpeedBtn;
	UPROPERTY() UButton* DivideSpeedBtn;
	int32 CurrentSelectedYear;
	int32 CurrentSelectedMonth;
	int32 CurrentSelectedDay;
	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* DayInput;
	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* MonthInput;
	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* YearInput;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnPrevMonthClicked();
	UFUNCTION() void OnNextMonthClicked();
	UFUNCTION() void OnApplyTimeClicked();
	UFUNCTION() int32 GetDaysInMonthCustom(int32 Month, int32 Year);
	bool IsLeapYearCustom(int32 Year);
	void SelectDay(int32 Day);
	UFUNCTION() void OnPlayFwdClicked();
	UFUNCTION() void OnPlayRevClicked();
	UFUNCTION() void OnPauseClicked();
	UFUNCTION() void OnMultiplySpeedClicked();
	UFUNCTION() void OnDivideSpeedClicked();
	UFUNCTION() void OnSpeedInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void OnUTCInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void OnSolarInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void UpdateDateInputs();
	void SyncClocks(bool bFromUTC);
private:
	void GenerateCalendar();
	void ApplySpeedToManager(float NewSpeed);
};
//meniul pentru optica si telscop (un meniu foarte important pentru a putea observa de pe Pamant corpuri ceresti)
UCLASS()
class PROIECT_ECLIPSA_API UOpticsMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() UButton* ResetButton;
	UPROPERTY() UComboBoxString* TargetComboBox;
	UPROPERTY() class USlider* ZoomSlider;
	UPROPERTY() class UEditableTextBox* ZoomInput; 
	UPROPERTY() class USlider* ExposureSlider;
	UPROPERTY() class UTextBlock* ExposureValueText;
	UPROPERTY() UButton* FitWidthButton;
	UPROPERTY() UButton* FitHeightButton;
	UFUNCTION() void OnFitWidthClicked();
	UFUNCTION() void OnFitHeightClicked();
	void AutoFitFOV(bool bFitHeight);
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnTargetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnZoomChanged(float Value);
	UFUNCTION() void OnZoomInputCommitted(const FText& Text, ETextCommit::Type CommitMethod); 
	UFUNCTION() void OnResetClicked();
private:
	APlayerCameraManager* CachedCameraManager;
};
//overlays si telemetrie imi ofera o gama larga de setari pentru a imi face experienta jocului cat mai placuta (afisare con umbra, afisare con penumbra, etichete stele, planete etc.)
UCLASS()
class PROIECT_ECLIPSA_API UOverlayMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() class UCheckBox* CheckBox_HUD;
	UPROPERTY() class UCheckBox* CheckBox_Shadows;
	UPROPERTY() class UCheckBox* CheckBox_Atmosphere;
	UPROPERTY() class UCheckBox* CheckBox_Clouds;
	UPROPERTY() class UCheckBox* CheckBox_DiamondRing;
	UPROPERTY() class UCheckBox* CheckBox_GridAzimuth;
	UPROPERTY() class UCheckBox* CheckBox_GridEquatorial;
	UPROPERTY() class UCheckBox* CheckBox_LabelsStars;
	UPROPERTY() class UCheckBox* CheckBox_LabelsPlanets;
	UPROPERTY() class UCheckBox* CheckBox_AstroText;
	UPROPERTY() class UCheckBox* CheckBox_DroneVis;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UCheckBox* CheckBox_Constellations;
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	class UCheckBox* CheckBox_StarNames; 
	UPROPERTY()
	class UCheckBox* CheckBox_ShadowGround;
	UPROPERTY()
	class UCheckBox* CheckBox_ShadowConeFull;
	UPROPERTY()
	class UCheckBox* CheckBox_ShadowConeTruncated;
	UPROPERTY()
	UCheckBox* CheckBox_PenumbraGround;
	UPROPERTY()
	UCheckBox* CheckBox_PenumbraConeFull;
	UPROPERTY()
	UCheckBox* CheckBox_PenumbraConeTruncated;
	UPROPERTY()
	class UCheckBox* CheckBox_MoonLabels;
	UPROPERTY(meta = (BindWidget))
	class UCheckBox* CheckBox_ConstellationArt;
	UFUNCTION()
	void OnConstellationArtToggled(bool bIsChecked);
	UPROPERTY(meta = (BindWidget))
	class UCheckBox* CheckBox_ConstellationNames;
	UFUNCTION()
	void OnConstellationNamesToggled(bool bIsChecked);
	UFUNCTION()
	void OnPenumbraGroundToggled(bool bIsChecked);
	UFUNCTION()
	void OnPenumbraConeFullToggled(bool bIsChecked);
	UFUNCTION()
	void OnPenumbraConeTruncatedToggled(bool bIsChecked);
	bool bDrawShadowGround2 = true;
	bool bDrawConeFull2 = false;
	bool bDrawConeTruncated2 = false;
	UFUNCTION()
	void OnShadowGroundToggled(bool bIsChecked);
	UFUNCTION()
	void OnShadowConeFullToggled(bool bIsChecked);
	UFUNCTION()
	void OnShadowConeTruncatedToggled(bool bIsChecked);
	UFUNCTION()
	void OnStarNamesToggled(bool bIsChecked);
	UFUNCTION()
	void OnConstellationsToggled(bool bIsChecked);
	UFUNCTION()
	void OnMoonLabelsToggled(bool bIsChecked);
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnHUDToggled(bool bIsChecked);
	UFUNCTION() void OnShadowsToggled(bool bIsChecked);
	UFUNCTION() void OnCloudsToggled(bool bIsChecked);
	UFUNCTION() void OnGridAzimuthToggled(bool bIsChecked);
	UFUNCTION() void OnGridEquatorialToggled(bool bIsChecked);
	UFUNCTION() void OnLabelsStarsToggled(bool bIsChecked);
	UFUNCTION() void OnLabelsPlanetsToggled(bool bIsChecked);
	UFUNCTION() void OnAstroTextToggled(bool bIsChecked);
	UFUNCTION() void OnDroneVisToggled(bool bIsChecked);
	//pentru conul de umbra/penumbra Pamant
	UPROPERTY() UCheckBox* CheckBox_EarthUmbra = nullptr;
	UPROPERTY() UCheckBox* CheckBox_EarthPenumbra = nullptr;
	UFUNCTION() void OnEarthUmbraToggled(bool bIsChecked);
	UFUNCTION() void OnEarthPenumbraToggled(bool bIsChecked);
};
//export de video/ poze pentru meniul meu
UCLASS()
class PROIECT_ECLIPSA_API UVideoExportMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() class UEditableTextBox* StartYInput;
	UPROPERTY() class UEditableTextBox* StartMInput;
	UPROPERTY() class UEditableTextBox* StartDInput;
	UPROPERTY() class UEditableTextBox* StartHInput;
	UPROPERTY() class UEditableTextBox* StartMinInput;
	UPROPERTY() class UEditableTextBox* StartSInput;
	UPROPERTY() class UEditableTextBox* StopYInput;
	UPROPERTY() class UEditableTextBox* StopMInput;
	UPROPERTY() class UEditableTextBox* StopDInput;
	UPROPERTY() class UEditableTextBox* StopHInput;
	UPROPERTY() class UEditableTextBox* StopMinInput;
	UPROPERTY() class UEditableTextBox* StopSInput;
	UPROPERTY() UButton* Btn_GetCurrentStartTime;
	UPROPERTY() UButton* TakeScreenshotButton;
	UPROPERTY() UButton* Btn_GetCurrentStopTime;
	UFUNCTION() void OnGetStartTimeClicked();
	UFUNCTION() void OnGetStopTimeClicked();
	UFUNCTION()
	void OnTakeScreenshotClicked();
	UFUNCTION()
	void ExecuteScreenshotCapture();
	UPROPERTY() class UEditableTextBox* LatInput;
	UPROPERTY() class UEditableTextBox* LonInput;
	UPROPERTY() class UEditableTextBox* AltInput;
	UPROPERTY() UButton* Btn_GetCurrentLocation;
	UFUNCTION() void OnGetLocationClicked();
	UPROPERTY() class UComboBoxString* TargetComboBox;
	UPROPERTY() class UEditableTextBox* FovInput; 
	UPROPERTY() class UComboBoxString* ResolutionComboBox;
	UPROPERTY() class UComboBoxString* FramerateComboBox;
	UPROPERTY() class UEditableTextBox* PlaybackSpeedInput;
	UPROPERTY() UButton* StartRecordingButton;
	UPROPERTY() class UTextBlock* StatusText;
	UPROPERTY() class UCheckBox* HideDroneCheckBox;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnStartRecordingClicked();
	FTimerHandle CheckTimeTimerHandle;
	FDateTime TargetStopTime;
	FString FinalSavePath;
	UFUNCTION() void StartActualRecording();
	UFUNCTION() void PrepareNextFrame();
	UFUNCTION() void CheckRecordingTime();
	UFUNCTION() void CompileVideo();

	int32 SimYear; int32 SimMonth; int32 SimDay;
	int32 SimHour; int32 SimMinute; int32 SimSecond;

	int32 StopYear; int32 StopMonth; int32 StopDay;
	int32 StopHour; int32 StopMinute; int32 StopSecond;
	void AdvanceSimulationTime(float SecondsToAdd);
};
//meniu pentru camera, pot alege din 6 tipuri diferite de camera
UCLASS()
class PROIECT_ECLIPSA_API UCameraMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UButton* Btn_FPV;
	UPROPERTY() class UButton* Btn_TPV;
	UPROPERTY() class UButton* Btn_Cinematic;
	UPROPERTY() class UButton* Btn_TopDown;
	UPROPERTY() class UButton* Btn_LEO;
	UPROPERTY() class UButton* Btn_GEO;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void SetCameraFPV();
	UFUNCTION() void SetCameraTPV();
	UFUNCTION() void SetCameraCinematic();
	UFUNCTION() void SetCameraTopDown();
	UFUNCTION() void SetCameraLEO();
	UFUNCTION() void SetCameraGEO();
	UFUNCTION() void UpdateOrbit(); 
	FTimerHandle OrbitTimerHandle;
	float CurrentOrbitDegreesPerSecond;
	FVector CurrentOrbitLLH;
	bool bWasInSpace;
	FVector SavedEarthLLH;
};
//meniul de telemetrie live, dau click pe un corp ceresc si in dreapta imi apare un meniu frumos cu toate detaliile despre acel corp ceresc (distata, distanta_UA, timp_lumina, iluminare, elongatie etc.)
UCLASS()
class PROIECT_ECLIPSA_API UAstroTelemetryWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UTextBlock* TargetNameText;
	UPROPERTY() class UTextBlock* DistanceKmText;
	UPROPERTY() class UTextBlock* DistanceLightText;
	UPROPERTY() class UTextBlock* AngularSizeText;
	UPROPERTY() class UTextBlock* MagnitudeText;
	UPROPERTY() class UTextBlock* AltAzText;
	UPROPERTY()
	UTextBlock* DistanceAUText;
	UPROPERTY()
	UTextBlock* IlluminationText;
	UPROPERTY()
	UTextBlock* ElongationText;
	UTextBlock* LabelTarget;
	UTextBlock* LabelDistance;
	UTextBlock* LabelDistanceAU;
	UTextBlock* LabelLightTime;
	UTextBlock* LabelAngularSize;
	UTextBlock* LabelMagnitude;
	UTextBlock* LabelAltAz;
	UTextBlock* LabelIllumination;
	UTextBlock* LabelElongation;
	UTextBlock* TitleTextBlock;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void UpdateTelemetryInfo();
	void StartTelemetry();
	FTimerHandle TelemetryTimerHandle;
	UPROPERTY() AOrbitalManager* OrbitalManagerRef;
};
//clasa de efecte vfx (aici pot seta intensitatea stelelor, bloomul si expunerea)
UCLASS()
class PROIECT_ECLIPSA_API UVisualEffectsMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UButton* ResetButton;
	UPROPERTY() class USlider* BloomSlider;
	UPROPERTY() class UTextBlock* BloomValueText;
	UPROPERTY() class USlider* StarIntensitySlider;
	UPROPERTY() class UTextBlock* StarIntensityValueText;
	UPROPERTY() class USlider* EclipseFXSlider;
	UPROPERTY() class UTextBlock* EclipseFXValueText;
	UPROPERTY() class USlider* ExposureSlider;
	UPROPERTY() class UTextBlock* ExposureValueText;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnBloomChanged(float Value);
	UFUNCTION() void OnStarIntensityChanged(float Value);
	UFUNCTION() void OnEclipseFXChanged(float Value);
	UFUNCTION() void OnResetClicked();
	UFUNCTION() void OnExposureChanged(float Value);
};
//meniul de iesire este un meniu simplist, cu o modala de iesire
UCLASS()
class PROIECT_ECLIPSA_API UExitMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* Btn_No;
	UPROPERTY() class UButton* Btn_Yes;
	UFUNCTION() void OnNoClicked();
	UFUNCTION() void OnYesClicked();
};
//meniul de setari este foarte stufos si contine toate setarile pe care le contine un joc obisnuit
UCLASS()
class PROIECT_ECLIPSA_API USettingsMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UComboBoxString* QualityComboBox;
	UPROPERTY() class UComboBoxString* FPSComboBox;
	UPROPERTY() class UCheckBox* VSyncCheckBox;
	UPROPERTY() class USlider* ResScaleSlider;
	UPROPERTY() class UTextBlock* ResScaleValueText;
	UPROPERTY() class USlider* GammaSlider;
	UPROPERTY() class UTextBlock* GammaValueText;
	UPROPERTY() class UButton* ApplyButton;
	UPROPERTY() class UButton* ResetButton;
	UPROPERTY() class UComboBoxString* CesiumQualityComboBox;
	UPROPERTY() class UComboBoxString* GICheckBox;
	UPROPERTY() class UComboBoxString* CloudQualityComboBox;
	UPROPERTY() class UComboBoxString* CesiumCacheComboBox;
	UPROPERTY() class UComboBoxString* WindowModeComboBox;
	UPROPERTY() class UCheckBox* MotionBlurCheckBox;
	UPROPERTY() class USlider* AudioSlider;
	UPROPERTY() class UTextBlock* AudioValueText;
	UPROPERTY()
	class UBorder* ColorPreviewBorder;
	UPROPERTY()
	class USlider* SliderR;
	UPROPERTY()
	class USlider* SliderG;
	UPROPERTY()
	class USlider* SliderB;
	UPROPERTY()
	class USlider* SliderA;
	//partea pentru conul de umbra al Pamantului
	UPROPERTY() USlider* EarthUmbraSliderR = nullptr;
	UPROPERTY() USlider* EarthUmbraSliderG = nullptr;
	UPROPERTY() USlider* EarthUmbraSliderB = nullptr;
	UPROPERTY() USlider* EarthUmbraSliderA = nullptr;
	UPROPERTY() UBorder* EarthUmbraPreviewBorder = nullptr;

	UPROPERTY() USlider* EarthPenumbraSliderR = nullptr;
	UPROPERTY() USlider* EarthPenumbraSliderG = nullptr;
	UPROPERTY() USlider* EarthPenumbraSliderB = nullptr;
	UPROPERTY() USlider* EarthPenumbraSliderA = nullptr;
	UPROPERTY() UBorder* EarthPenumbraPreviewBorder = nullptr;

	UFUNCTION() void OnEarthUmbraColorSliderChanged(float Value);
	UFUNCTION() void OnEarthPenumbraColorSliderChanged(float Value);

	UPROPERTY()
	class UComboBoxString* LanguageComboBox;
	UPROPERTY()
	class UComboBoxString* MeasurementComboBox;
	UFUNCTION()
	void OnColorSliderChanged(float Value);
	UFUNCTION() void OnAudioChanged(float Value);
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnApplyClicked();
	UFUNCTION() void OnResScaleChanged(float Value);
	UFUNCTION() void OnGammaChanged(float Value);
	UPROPERTY() class USlider* SensitivitySlider;
	UPROPERTY() class UTextBlock* SensitivityValueText;
	UFUNCTION() void OnSensitivityChanged(float Value);
	UFUNCTION() void OnResetClicked();
	UPROPERTY() class USlider* FOVSlider;
	UPROPERTY() class UTextBlock* FOVValueText;
	UFUNCTION() void OnFOVChanged(float Value);
	UPROPERTY() class UCheckBox* InvertYCheckBox;
	UPROPERTY() class UCheckBox* HideHUDCheckBox;
	UPROPERTY() class UCheckBox* DroneVisCheckBox;
	UPROPERTY()
	class UButton* OpenMapButton;
	UFUNCTION()
	void OnOpenMapClicked();
	UPROPERTY() class USlider* PenumbraSliderR;
	UPROPERTY() class USlider* PenumbraSliderG;
	UPROPERTY() class USlider* PenumbraSliderB;
	UPROPERTY() class USlider* PenumbraSliderA;
	UPROPERTY() class UBorder* PenumbraColorPreviewBorder;
	UFUNCTION() void OnPenumbraColorSliderChanged(float Value);
	UPROPERTY()
	class UEditableTextBox* CesiumKeyTextBox;
	UPROPERTY()
	class UButton* ToggleKeyVisibilityButton;
	UPROPERTY()
	class UTextBlock* ToggleKeyVisibilityText;
	UPROPERTY()
	class UButton* CesiumLinkButton;
	UFUNCTION()
	void OnToggleCesiumKeyVisibilityClicked();
	UFUNCTION()
	void OnCesiumLinkClicked();
	UPROPERTY()
	float GCurrentMasterVolume = 100.0f;
	UPROPERTY() class UButton* OpenCreditsButton;
	UFUNCTION() void OnOpenCreditsClicked();
private:
	void LoadCurrentSettings();
};
UCLASS()

//meniul de favorite, in care pot pune titlu, descriere, coordonate, timp, corpul cersc urmarit si fov. Foarte util daca imi place un loc/eclipsa
class PROIECT_ECLIPSA_API UBookmarksMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UEditableTextBox* TitleInput;
	UPROPERTY() class UEditableTextBox* DescInput;
	UPROPERTY() class UEditableTextBox* YearInput;
	UPROPERTY() class UEditableTextBox* MonthInput;
	UPROPERTY() class UEditableTextBox* DayInput;
	UPROPERTY() class UEditableTextBox* HourInput;
	UPROPERTY() class UEditableTextBox* MinInput;
	UPROPERTY() class UEditableTextBox* LatInput;
	UPROPERTY() class UEditableTextBox* LonInput;
	UPROPERTY() class UEditableTextBox* AltInput;
	UPROPERTY() class UButton* GetCurrentDataButton;
	UPROPERTY() class UButton* SaveBookmarkButton;
	UPROPERTY() class UScrollBox* BookmarksListScroll;
	UPROPERTY() class USlider* SavedFovSlider;
	UPROPERTY() class UEditableTextBox* SavedFovInput;
	UPROPERTY() class UComboBoxString* SavedTrackCombo;
	UFUNCTION() void OnFovSliderChanged(float Value);
	UFUNCTION() void OnFovInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnGetCurrentDataClicked();
	UFUNCTION() void OnSaveBookmarkClicked();
	UFUNCTION() void OnTeleportClicked(int32 BookmarkIndex);
	UFUNCTION() void OnDeleteClicked(int32 BookmarkIndex);
private:
	TArray<FAstroBookmark> SavedBookmarks;
	void RefreshBookmarksList();
	void LoadBookmarksFromDisk();
	void SaveBookmarksToDisk();
};
//clasa pentru butonul de stergere al unui favorit
UCLASS()
class PROIECT_ECLIPSA_API UBookmarkActionBtn : public UButton
{
	GENERATED_BODY()
public:
	int32 BookmarkIndex;
	bool bIsDeleteBtn; 
	UPROPERTY()
	int32 TargetIndex;
	class UBookmarksMenuWidget* ParentMenu;
	UFUNCTION()
	void HandleClick();
};
//aici aleg care meniu este parintele cererii mele din meniul de help
UCLASS()
class PROIECT_ECLIPSA_API UHelpResultBtn : public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() EMenuTarget TargetMenuToOpen;
	UPROPERTY() class UHelpMenuWidget* ParentMenu;
	UFUNCTION()
	void HandleClick();
};
//meniu help, poate fi apelat si cu f10

UCLASS()
class PROIECT_ECLIPSA_API UHelpMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UEditableTextBox* SearchInput;
	UPROPERTY() class UScrollBox* ResultsScrollBox;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnSearchTextChanged(const FText& Text);
	void OpenTargetMenu(EMenuTarget Target);
	UPROPERTY() class UButton* ManualButton;
	UPROPERTY() class UBorder* ManualContainer;
	UPROPERTY() class UBorder* MainMenuContainer;
	UFUNCTION() void OnManualClicked();
private:
	TArray<FHelpEntry> HelpDatabase;
	void BuildDatabase();
	void UpdateResults(FString Query);
	void BuildManualUI();
};
//meniu pentru vreme, pot seta din slidere nivelul plafonului de nori, densitatea norilor si nivelul de ceata. De asemenea, pot activa ploaia sau ninsoarea.
UCLASS()
class PROIECT_ECLIPSA_API UWeatherMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UButton* ResetButton;
	UPROPERTY() class USlider* FogSlider;
	UPROPERTY() class UTextBlock* FogValueText;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnResetClicked();
	UFUNCTION()
	void OnFogChanged(float Value); 
	UFUNCTION()
	void OnRainChanged(bool bIsChecked); 
	UFUNCTION()
	void OnSnowChanged(bool bIsChecked); 
	class UCheckBox* RainCheckBox;
	class UCheckBox* SnowCheckBox;
	void ResetStaticWeatherMemory();
	USlider* CloudSlider;
	UTextBlock* CloudValueText;
	UFUNCTION()
	void OnCloudChanged(float Value);
	UPROPERTY()
	USlider* CloudAltSlider;
	UPROPERTY()
	UTextBlock* CloudAltValueText;
	UFUNCTION()
	void OnCloudAltitudeChanged(float Value);
private:
};
//clasa care are rolul sa imi exporte poze 2D cu harta lumii si banda de totalitate suprapusa peste
UCLASS()
class PROIECT_ECLIPSA_API UEclipseMapExportWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UEclipseMapExportWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY() class UButton* CloseButton;
	UPROPERTY() class UButton* ToggleLinesButton;
	UPROPERTY() class UImage* MapPreviewImage;
	UPROPERTY() class UButton* ExportImageButton;
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnToggleLinesClicked();
	UFUNCTION() void OnExportImageClicked();
	UPROPERTY()
	class UCheckBox* TotalityCheckBox;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Export")
	UTexture2D* WorldMapTexture;
	UFUNCTION()
	void OnTotalityCheckboxChanged(bool bIsChecked);
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};

//submeniu al setarilor, aici pot alege punctul de start al simularii. De asemenea, pot seta si data.
UCLASS()
class PROIECT_ECLIPSA_API UMapPickerWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UMapPickerWidget(const FObjectInitializer& ObjectInitializer);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
public:
	UPROPERTY() class UImage* MapImage;
	UPROPERTY() class UBorder* MapClippingBorder;
	UPROPERTY() class UEditableTextBox* InputLat;
	UPROPERTY() class UComboBoxString* ComboLat;
	UPROPERTY() class UEditableTextBox* InputLon;
	UPROPERTY() class UComboBoxString* ComboLon;
	UPROPERTY() class UEditableTextBox* InputAlt;
	UPROPERTY() class UEditableTextBox* InputYear;
	UPROPERTY() class UEditableTextBox* InputMonth;
	UPROPERTY() class UEditableTextBox* InputDay;
	UPROPERTY() class UEditableTextBox* InputLocalHour;
	UPROPERTY() class UEditableTextBox* InputLocalMinute;
	UPROPERTY() class UEditableTextBox* InputLocalSecond;
	UPROPERTY() class UEditableTextBox* InputUTCHour;
	UPROPERTY() class UEditableTextBox* InputUTCMinute;
	UPROPERTY() class UEditableTextBox* InputUTCSecond;
	UPROPERTY() class UTextBlock* TxtTimeZoneInfo;
	UPROPERTY() class UButton* SaveButton;
	UPROPERTY() class UButton* CancelButton;
	UPROPERTY() class UTexture2D* WorldMapTexture;
	UPROPERTY() class UButton* GetCurrentLocButton;
	UPROPERTY() class UButton* GetCurrentTimeButton;
	UFUNCTION() void OnGetCurrentLocationClicked();
	UFUNCTION() void OnGetCurrentTimeClicked();
	float MapZoom;
	FVector2D MapPan;
	bool bIsPanning;
	FVector2D LastMousePosition;
	bool bHasMarker;
	FVector2D MarkerLatLon;
	bool bIsUpdatingTime;
	UFUNCTION() void OnSaveClicked();
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnInputTextChanged(const FText& Text);
	UFUNCTION() void OnComboSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnDateChanged(const FText& Text);
	UFUNCTION() void OnLocalTimeChanged(const FText& Text);
	UFUNCTION() void OnUTCTimeChanged(const FText& Text);
	void LoadSavedCoordinates();
	void UpdateInputsFromMarker();
	void UpdateMarkerFromInputs();
	int32 GetTimeZoneOffset();
	void SyncTime(bool bFromLocal);

	//aici adaug partea pentru selectare de orase
	UPROPERTY()
	UEditableTextBox* CitySearchInput = nullptr;

	UPROPERTY()
	UComboBoxString* CityResultsComboBox = nullptr;

	// Harta locala pentru rezultatele cautarii curente
	TMap<FString, FEclipseCityHit> FoundCitiesMap;

	UFUNCTION()
	void OnCitySearchChanged(const FText& Text);

	UFUNCTION()
	void OnCitySelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	void ApplyCityCoordinates(const FEclipseCityHit& City);
};

//submeniu al setarilor, meniul de credite are doar text scris, insa este foarte util ca sa arate munca mea si sursele de inspiratie
UCLASS()
class PROIECT_ECLIPSA_API UCreditsMenuWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
public:
	UPROPERTY() class UButton* CloseButton;
	UFUNCTION() void OnCloseClicked();
};

UCLASS()
class PROIECT_ECLIPSA_API ULunarJumpButton : public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() int32 TargetDay;
	UPROPERTY() int32 TargetMonth;
	UPROPERTY() int32 TargetYear;
	UPROPERTY() float TargetHour;
	UPROPERTY() float TargetLat;   // NOU
	UPROPERTY() float TargetLon;   // NOU
	UPROPERTY() class UEclipseBrowserWidget* ParentBrowser;
	//UFUNCTION() void HandleClick();

	UPROPERTY() int32 TargetSaros = -9999;
	UPROPERTY() bool bIsSarosButton = false;
	UPROPERTY() bool bIsDetailsButton = false;
	UPROPERTY() FLunarEclipseElements LunarDetailsData;

	UFUNCTION() void HandleClick();
};


UCLASS()
class PROIECT_ECLIPSA_API UFirstRunSetupWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY() UEditableTextBox* CesiumKeyTextBox;
	UPROPERTY() UButton* ConfirmButton;
	UPROPERTY() UButton* CesiumLinkButton;
	UPROPERTY() UButton* ToggleKeyVisibilityButton;
	UPROPERTY() UTextBlock* ToggleKeyVisibilityText;
	UPROPERTY() UTextBlock* ErrorText;

	UFUNCTION() void OnConfirmClicked();
	UFUNCTION() void OnCesiumLinkClicked();
	UFUNCTION() void OnToggleCesiumKeyVisibilityClicked();

	void MarkFirstRunDone();
};
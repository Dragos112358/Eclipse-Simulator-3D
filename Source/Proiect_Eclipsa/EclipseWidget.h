#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/SizeBox.h" 
#include "EclipseWidget.generated.h"
class UEclipseWidget;
UCLASS()
class PROIECT_ECLIPSA_API UEclipseRowButton : public UButton
{
	GENERATED_BODY()
public:
	FString PhaseID;
	FLinearColor HoverColor;
	UEclipseWidget* ParentWidget;
	void SetupRow(UEclipseWidget* InParent, FString InID, FLinearColor InColor);
	UFUNCTION() void HandleHover();
	UFUNCTION() void HandleUnhover();
};
UCLASS()
class PROIECT_ECLIPSA_API UEclipseWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void SetReportText(FString Text);
	void OnRowHovered(UEclipseRowButton* ButtonCtx, FString PhaseID, FLinearColor Color);
	void OnRowUnhovered(UEclipseRowButton* ButtonCtx);
	UEclipseWidget(const FObjectInitializer& ObjectInitializer);
protected:
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnSaveTxtClicked();
	UFUNCTION() void OnSaveCsvClicked();
	UPROPERTY() UButton* SaveTxtButton;
	UPROPERTY() UButton* SaveCsvButton;
	FString RawReportText; //aici salvez textul brut txt
	FString RawCsvText;    //aici salvez textul brut csv
	UPROPERTY()
	FTimerHandle CloseTimerHandle;
	UFUNCTION() void ExecuteClose();

	UPROPERTY()
	UTexture2D* TexC1;

	UPROPERTY()
	UTexture2D* TexC2;

	UPROPERTY()
	UTexture2D* TexC3;

	UPROPERTY()
	UTexture2D* TexC4;

	UPROPERTY()
	UTexture2D* TexMax;
private:
	UPROPERTY() UCanvasPanel* RootCanvas; //il folosesc pentru meniu
	UPROPERTY() UBorder* MainWindowBorder;
	UPROPERTY() UButton* CloseButton;
	UPROPERTY() USizeBox* ImageContainerBox;
	UPROPERTY() UImage* PhaseIndicatorImage;
	UPROPERTY() UTextBlock* GeneralInfoText; //informatii generale cu privire la eclipse
	UPROPERTY() UVerticalBox* TableContainer; //continut tabel (am de la 3 la 6 linii)
	void AddColumnText(UHorizontalBox* RowBox, FString Text, float WidthWeight, bool bIsHeader);
	void CreateTableHeader();
	void CreateTableRow(FString LineText, FString PhaseID);
};

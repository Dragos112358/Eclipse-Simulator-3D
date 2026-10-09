#include "EclipseWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h" 
#include "UObject/ConstructorHelpers.h"
#include "Engine/Font.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "Framework/Application/SlateApplication.h"
#include "OrbitalManager.h" 
#include "Kismet/GameplayStatics.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/MinWindows.h"
#include <commdlg.h> 
#include "Windows/HideWindowsPlatformTypes.h"
#endif

//constante pentru dimensiuni coloane tabel
const float COL_EVENT = 0.8f;
const float COL_TIME = 1.5f;
const float COL_LIMB = 1.1f;
const float COL_ALT = 1.8f;
const float COL_AZI = 1.0f;
const float COL_P = 0.8f;

FSlateFontInfo GetMonoFont(int32 Size, bool bBold = false)
{
	static UFont* MonoFontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/RobotoMono.RobotoMono"));
	static UFont* DefaultFontObj = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	UFont* UsedFont = MonoFontObj ? MonoFontObj : DefaultFontObj;
	if (UsedFont)
	{
		FSlateFontInfo FontInfo = UsedFont->GetLegacySlateFontInfo();
		FontInfo.Size = Size;
		if (bBold) FontInfo.TypefaceFontName = "Bold";
		return FontInfo;
	}
	return FSlateFontInfo();
}

UEclipseWidget::UEclipseWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FinderC1(TEXT("/Game/Textures/C1"));
	if (FinderC1.Succeeded()) TexC1 = FinderC1.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FinderC2(TEXT("/Game/Textures/C2"));
	if (FinderC2.Succeeded()) TexC2 = FinderC2.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FinderMax(TEXT("/Game/Textures/Max"));
	if (FinderMax.Succeeded()) TexMax = FinderMax.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FinderC3(TEXT("/Game/Textures/C3"));
	if (FinderC3.Succeeded()) TexC3 = FinderC3.Object;

	static ConstructorHelpers::FObjectFinder<UTexture2D> FinderC4(TEXT("/Game/Textures/C4"));
	if (FinderC4.Succeeded()) TexC4 = FinderC4.Object;
}

void UEclipseRowButton::SetupRow(UEclipseWidget* InParent, FString InID, FLinearColor InColor)
{
	ParentWidget = InParent;
	PhaseID = InID;
	HoverColor = InColor;
	OnHovered.AddDynamic(this, &UEclipseRowButton::HandleHover);
	OnUnhovered.AddDynamic(this, &UEclipseRowButton::HandleUnhover);
}

void UEclipseRowButton::HandleHover() { if (ParentWidget) ParentWidget->OnRowHovered(this, PhaseID, HoverColor); }
void UEclipseRowButton::HandleUnhover() { if (ParentWidget) ParentWidget->OnRowUnhovered(this); }

TSharedRef<SWidget> UEclipseWidget::RebuildWidget()
{
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
		MainWindowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("WindowBorder"));
		MainWindowBorder->SetBrushColor(FLinearColor::White);
		UCanvasPanelSlot* BorderSlot = RootCanvas->AddChildToCanvas(MainWindowBorder);
		if (BorderSlot)
		{
			BorderSlot->SetAnchors(FAnchors(0.5f));
			BorderSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			BorderSlot->SetSize(FVector2D(850.0f, 700.0f));
			BorderSlot->SetPosition(FVector2D(0.0f, 0.0f));
			BorderSlot->SetZOrder(100);
		}
		UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainVBox"));
		MainWindowBorder->SetContent(MainVBox);
		UHorizontalBox* HeaderHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderHBox"));
		UVerticalBoxSlot* HeaderSlot = MainVBox->AddChildToVerticalBox(HeaderHBox);
		HeaderSlot->SetPadding(FMargin(5.0f, 2.0f, 5.0f, 2.0f));
		HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		USpacer* LeftSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		UHorizontalBoxSlot* SpacerSlot = HeaderHBox->AddChildToHorizontalBox(LeftSpacer);
		FSlateChildSize FillSize(ESlateSizeRule::Fill);
		FillSize.Value = 1.0f;
		SpacerSlot->SetSize(FillSize);
		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseBtn"));
		CloseButton->SetBackgroundColor(FLinearColor(0.8f, 0.0f, 0.0f, 1.0f));
		USizeBox* BtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BtnSize"));
		BtnSizeBox->SetWidthOverride(30.0f);
		BtnSizeBox->SetHeightOverride(30.0f);
		BtnSizeBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		UTextBlock* XText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XText"));
		XText->SetText(FText::FromString("X"));
		XText->SetFont(GetMonoFont(14, true));
		XText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		XText->SetJustification(ETextJustify::Center);
		XText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BtnSizeBox->SetContent(XText);
		CloseButton->AddChild(BtnSizeBox);
		CloseButton->OnClicked.AddDynamic(this, &UEclipseWidget::OnCloseClicked);
		HeaderHBox->AddChildToHorizontalBox(CloseButton);
		UBorder* ContentBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ContentBorder"));
		FLinearColor ContentColor = FLinearColor(0.93f, 0.78f, 0.39f, 1.0f);
		ContentBorder->SetBrushColor(ContentColor);
		ContentBorder->SetPadding(FMargin(15.0f));
		UVerticalBoxSlot* ContentSlot = MainVBox->AddChildToVerticalBox(ContentBorder);
		ContentSlot->SetSize(FillSize);
		ContentSlot->SetPadding(FMargin(3.0f));
		UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentVBox"));
		ContentBorder->SetContent(ContentVBox);
		UHorizontalBox* TopRowContainer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopRow3Cols"));
		UVerticalBoxSlot* TopSectionSlot = ContentVBox->AddChildToVerticalBox(TopRowContainer);
		TopSectionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
		GeneralInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GenInfo"));
		GeneralInfoText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		GeneralInfoText->SetFont(GetMonoFont(13, true));
		GeneralInfoText->SetText(FText::FromString("Initializare..."));
		GeneralInfoText->SetJustification(ETextJustify::Left);
		GeneralInfoText->SetAutoWrapText(true);
		UHorizontalBoxSlot* LeftSlot = TopRowContainer->AddChildToHorizontalBox(GeneralInfoText);
		FSlateChildSize ColFill(ESlateSizeRule::Fill);
		ColFill.Value = 1.0f;
		LeftSlot->SetSize(ColFill);
		LeftSlot->SetVerticalAlignment(VAlign_Top);
		UVerticalBox* CenterImgContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterImgBox"));
		UHorizontalBoxSlot* CenterSlot = TopRowContainer->AddChildToHorizontalBox(CenterImgContainer);
		CenterSlot->SetSize(ColFill);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);
		ImageContainerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ImgBox"));
		ImageContainerBox->SetWidthOverride(400.0f);
		ImageContainerBox->SetHeightOverride(161.0f);
		ImageContainerBox->SetVisibility(ESlateVisibility::Hidden);
		PhaseIndicatorImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PhaseImg"));
		ImageContainerBox->SetContent(PhaseIndicatorImage);
		CenterImgContainer->AddChildToVerticalBox(ImageContainerBox);
		UTextBlock* RightInfoText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RightInfoTextWidget"));
		RightInfoText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
		RightInfoText->SetFont(GetMonoFont(13, true));
		RightInfoText->SetText(AOrbitalManager::Tr(TEXT("Click pe tabel\npentru simulare!"), TEXT("Click on table\nfor simulation!")));
		RightInfoText->SetJustification(ETextJustify::Right);
		UHorizontalBoxSlot* RightSlot = TopRowContainer->AddChildToHorizontalBox(RightInfoText);
		RightSlot->SetSize(ColFill);
		RightSlot->SetVerticalAlignment(VAlign_Top);
		TableContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TableContainer"));
		UVerticalBoxSlot* TableSlot = ContentVBox->AddChildToVerticalBox(TableContainer);
		TableSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UHorizontalBox* FooterHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FooterHBox"));
		UVerticalBoxSlot* FooterSlot = ContentVBox->AddChildToVerticalBox(FooterHBox);
		FooterSlot->SetPadding(FMargin(0.0f, 15.0f, 0.0f, 0.0f));
		FooterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		USpacer* FooterSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		UHorizontalBoxSlot* FSpacerSlot = FooterHBox->AddChildToHorizontalBox(FooterSpacer);
		FSpacerSlot->SetSize(FillSize);

		SaveTxtButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SaveTxtBtn"));
		SaveTxtButton->SetBackgroundColor(FLinearColor(0.2f, 0.4f, 0.8f, 1.0f));
		UTextBlock* SaveTxtLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SaveTxtLblWidget"));
		SaveTxtLbl->SetText(AOrbitalManager::Tr(TEXT(" Salveaza TXT "), TEXT(" Save TXT ")));
		SaveTxtLbl->SetFont(GetMonoFont(11, true));
		SaveTxtLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		SaveTxtLbl->SetMargin(FMargin(10.f, 5.f));
		SaveTxtButton->AddChild(SaveTxtLbl);
		SaveTxtButton->OnClicked.AddDynamic(this, &UEclipseWidget::OnSaveTxtClicked);
		UHorizontalBoxSlot* BtnTxtSlot = FooterHBox->AddChildToHorizontalBox(SaveTxtButton);
		BtnTxtSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
		SaveCsvButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SaveCsvBtn"));
		SaveCsvButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.2f, 1.0f));
		UTextBlock* SaveCsvLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SaveCsvLblWidget"));
		SaveCsvLbl->SetText(AOrbitalManager::Tr(TEXT(" Salveaza CSV (Excel) "), TEXT(" Save CSV (Excel) ")));
		SaveCsvLbl->SetFont(GetMonoFont(11, true));
		SaveCsvLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		SaveCsvLbl->SetMargin(FMargin(10.f, 5.f));
		SaveCsvButton->AddChild(SaveCsvLbl);
		SaveCsvButton->OnClicked.AddDynamic(this, &UEclipseWidget::OnSaveCsvClicked);
		FooterHBox->AddChildToHorizontalBox(SaveCsvButton);
	}
	return Super::RebuildWidget();
}

void UEclipseWidget::OnCloseClicked()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(CloseTimerHandle);
		World->GetTimerManager().SetTimer(CloseTimerHandle, this, &UEclipseWidget::ExecuteClose, 0.01f, false);
	}
}

void UEclipseWidget::OnRowHovered(UEclipseRowButton* ButtonCtx, FString PhaseID, FLinearColor Color)
{
	if (ImageContainerBox && PhaseIndicatorImage)
	{
		UTexture2D* PhaseTex = nullptr;
		if (PhaseID.Contains("C1")) PhaseTex = TexC1;
		else if (PhaseID.Contains("C2")) PhaseTex = TexC2;
		else if (PhaseID.Contains("MAX")) PhaseTex = TexMax;
		else if (PhaseID.Contains("C3")) PhaseTex = TexC3;
		else if (PhaseID.Contains("C4")) PhaseTex = TexC4;

		if (PhaseTex)
		{
			PhaseIndicatorImage->SetBrushFromTexture(PhaseTex);
			PhaseIndicatorImage->SetColorAndOpacity(FLinearColor::White);
			ImageContainerBox->SetVisibility(ESlateVisibility::Visible);
		}
	}
	if (ButtonCtx) ButtonCtx->SetBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
}

void UEclipseWidget::OnRowUnhovered(UEclipseRowButton* ButtonCtx)
{
	if (ImageContainerBox) ImageContainerBox->SetVisibility(ESlateVisibility::Hidden);
	if (ButtonCtx) ButtonCtx->SetBackgroundColor(FLinearColor::Transparent);
}

void UEclipseWidget::AddColumnText(UHorizontalBox* RowBox, FString Text, float WidthWeight, bool bIsHeader)
{
	UTextBlock* CellText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CellText->SetText(FText::FromString(Text));
	CellText->SetFont(GetMonoFont(13, bIsHeader));
	CellText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	CellText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* CellSlot = RowBox->AddChildToHorizontalBox(CellText);
	FSlateChildSize FillProp(ESlateSizeRule::Fill);
	FillProp.Value = WidthWeight;
	CellSlot->SetSize(FillProp);
	CellSlot->SetHorizontalAlignment(HAlign_Fill);
	CellSlot->SetVerticalAlignment(VAlign_Center);
}

void UEclipseWidget::CreateTableHeader()
{
	if (!TableContainer) return;
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.1f));
	HeaderBg->SetPadding(FMargin(0.0f, 5.0f));
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderRow);
	AddColumnText(HeaderRow, AOrbitalManager::TrString(TEXT("Eveniment"), TEXT("Event")), COL_EVENT, true);
	AddColumnText(HeaderRow, AOrbitalManager::TrString(TEXT("Ora UTC"), TEXT("UTC Time")), COL_TIME, true);
	AddColumnText(HeaderRow, AOrbitalManager::TrString(TEXT("Limb Lunar"), TEXT("Limb Correction")), COL_LIMB, true);
	AddColumnText(HeaderRow, AOrbitalManager::TrString(TEXT("Alt"), TEXT("Alt")), COL_ALT, true);
	AddColumnText(HeaderRow, AOrbitalManager::TrString(TEXT("Azi"), TEXT("Azi")), COL_AZI, true);
	AddColumnText(HeaderRow, TEXT("P"), COL_P, true);
	TableContainer->AddChildToVerticalBox(HeaderBg);
	UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Divider->SetBrushColor(FLinearColor::Black);
	UVerticalBoxSlot* DivSlot = TableContainer->AddChildToVerticalBox(Divider);
	Divider->SetContent(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
	Cast<USpacer>(Divider->GetContent())->SetSize(FVector2D(1.0f, 2.0f));
}

void UEclipseWidget::CreateTableRow(FString LineText, FString PhaseID)
{
	if (!TableContainer) return;
	TArray<FString> Parts;
	LineText.ParseIntoArray(Parts, TEXT("|"), true);
	if (Parts.Num() < 5) return;

	UOverlay* RowOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UEclipseRowButton* RowBtn = WidgetTree->ConstructWidget<UEclipseRowButton>(UEclipseRowButton::StaticClass());
	RowBtn->SetBackgroundColor(FLinearColor::Transparent);
	FLinearColor IndColor = FLinearColor::Transparent;
	RowBtn->SetupRow(this, PhaseID, IndColor);

	UOverlaySlot* BtnSlot = RowOverlay->AddChildToOverlay(RowBtn);
	BtnSlot->SetHorizontalAlignment(HAlign_Fill);
	BtnSlot->SetVerticalAlignment(VAlign_Fill);

	UHorizontalBox* RowContent = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowContent->SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlaySlot* ContentSlot = RowOverlay->AddChildToOverlay(RowContent);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);

	// Parametrii veniți direct din linia formatată de SetReportText
	FString EventText = Parts[0].TrimStartAndEnd();
	FString TimeText = Parts[1].TrimStartAndEnd();
	FString LimbText = Parts[2].TrimStartAndEnd(); // Aici va fi diferența calculată în secunde
	FString AltText = Parts[3].TrimStartAndEnd();
	FString AziText = Parts[4].TrimStartAndEnd();
	FString PText = Parts[5].TrimStartAndEnd();

	if (EventText.Contains(TEXT("Rasarit"))) EventText = AOrbitalManager::TrString(TEXT("Rasarit"), TEXT("Sunrise"));
	if (EventText.Contains(TEXT("Apus"))) EventText = AOrbitalManager::TrString(TEXT("Apus"), TEXT("Sunset"));

	float AltValue = FCString::Atof(*AltText);
	if (AltValue < 0.0f)
	{
		if (PhaseID.Contains("C1") || PhaseID.Contains("C2"))
		{
			AltText += AOrbitalManager::TrString(TEXT(" (Nerasarit)"), TEXT(" (Not Risen)"));
		}
		else if (PhaseID.Contains("C3") || PhaseID.Contains("C4"))
		{
			AltText += AOrbitalManager::TrString(TEXT(" (Apus)"), TEXT(" (Set)"));
		}
		else if (PhaseID.Contains("MAX"))
		{
			AltText += AOrbitalManager::TrString(TEXT(" (Invizibil)"), TEXT(" (Invisible)"));
		}
	}

	// Ordinea exactă a coloanelor în interfață
	AddColumnText(RowContent, EventText, COL_EVENT, false);
	AddColumnText(RowContent, TimeText, COL_TIME, false);
	AddColumnText(RowContent, LimbText, COL_LIMB, false); // Coloana de Limb Correction corect plasată
	AddColumnText(RowContent, AltText, COL_ALT, false);
	AddColumnText(RowContent, AziText, COL_AZI, false);
	AddColumnText(RowContent, PText, COL_P, false);

	UVerticalBoxSlot* RowSlot = TableContainer->AddChildToVerticalBox(RowOverlay);
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
}

void UEclipseWidget::ExecuteClose()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UEclipseWidget::SetReportText(FString Text)
{
	if (UTextBlock* RightTxt = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RightInfoTextWidget")))) {
		RightTxt->SetText(AOrbitalManager::Tr(TEXT("Click pe tabel\npentru simulare!"), TEXT("Click on table\nfor simulation!")));
	}
	if (UTextBlock* TxtBtnTxt = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("SaveTxtLblWidget")))) {
		TxtBtnTxt->SetText(AOrbitalManager::Tr(TEXT(" Salveaza TXT "), TEXT(" Save TXT ")));
	}
	if (UTextBlock* CsvBtnTxt = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("SaveCsvLblWidget")))) {
		CsvBtnTxt->SetText(AOrbitalManager::Tr(TEXT(" Salveaza CSV (Excel) "), TEXT(" Save CSV (Excel) ")));
	}

	RawReportText = Text;
	RawCsvText = TEXT("Event,UTC Time,Limb Correction,Alt,Azi,P\n");

	if (!TableContainer) return;
	TableContainer->ClearChildren();
	CreateTableHeader();

	TArray<FString> Lines;
	Text.ParseIntoArray(Lines, TEXT("\n"), false);
	FString GeneralTextAccumulator = "";

	struct FEventPair
	{
		FString NormalLine;
		FString LimbLine;
	};
	TMap<FString, FEventPair> PairedEvents;
	TArray<FString> OrderedKeys;

	for (FString& Line : Lines)
	{
		FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.StartsWith("-") || Trimmed.StartsWith("Event")) continue;

		if (Trimmed.StartsWith("C1") || Trimmed.StartsWith("C2") ||
			Trimmed.StartsWith("C3") || Trimmed.StartsWith("C4") ||
			Trimmed.StartsWith("MAX") ||
			Trimmed.StartsWith("Rasarit") || Trimmed.StartsWith("Apus") ||
			Trimmed.StartsWith("Sunrise") || Trimmed.StartsWith("Sunset"))
		{
			TArray<FString> Parts;
			Trimmed.ParseIntoArray(Parts, TEXT("|"), true);
			if (Parts.Num() >= 5)
			{
				FString EvRaw = Parts[0].TrimStartAndEnd();
				FString BaseEv = EvRaw.Replace(TEXT("L"), TEXT(""));

				if (!PairedEvents.Contains(BaseEv))
				{
					PairedEvents.Add(BaseEv, FEventPair());
					OrderedKeys.Add(BaseEv);
				}

				if (EvRaw.EndsWith("L"))
				{
					PairedEvents[BaseEv].LimbLine = Trimmed;
				}
				else
				{
					PairedEvents[BaseEv].NormalLine = Trimmed;
				}
			}
		}
		else
		{
			FString ProcessedLine = Trimmed.Replace(TEXT(" | "), TEXT("\n"));
			ProcessedLine = ProcessedLine.Replace(TEXT("|"), TEXT("\n"));
			GeneralTextAccumulator += ProcessedLine + "\n";
		}
	}

	// Helper pentru a transforma ora "HH:MM:SS.s" în secunde
	auto TimeToSeconds = [](FString TimeStr) -> double
		{
			TArray<FString> TimeParts;
			TimeStr.ParseIntoArray(TimeParts, TEXT(":"), true);
			if (TimeParts.Num() == 3)
			{
				double H = FCString::Atof(*TimeParts[0]);
				double M = FCString::Atof(*TimeParts[1]);
				double S = FCString::Atof(*TimeParts[2]);
				return H * 3600.0 + M * 60.0 + S;
			}
			return 0.0;
		};

	for (const FString& Key : OrderedKeys)
	{
		FEventPair& Pair = PairedEvents[Key];
		// Preferăm linia cu Limb (`LimbLine`) dacă există, altfel o luăm pe cea normală
		FString ActiveLine = !Pair.LimbLine.IsEmpty() ? Pair.LimbLine : Pair.NormalLine;
		if (ActiveLine.IsEmpty()) continue;

		TArray<FString> Parts;
		ActiveLine.ParseIntoArray(Parts, TEXT("|"), true);
		if (Parts.Num() >= 5)
		{
			FString Ev = Parts[0].TrimStartAndEnd().Replace(TEXT("L"), TEXT("")); // Curățăm 'L'-ul din denumire pentru tabel
			FString Ti = Parts[1].TrimStartAndEnd();
			FString Al = Parts[2].TrimStartAndEnd();
			FString Az = Parts[3].TrimStartAndEnd();
			FString P_ = Parts[4].TrimStartAndEnd();

			FString LimbDiffStr = TEXT("0.0s");

			// Calculăm diferența reală dintre timpul normal și cel cu limb correction
			if (!Pair.NormalLine.IsEmpty() && !Pair.LimbLine.IsEmpty())
			{
				TArray<FString> NormParts, LimbParts;
				Pair.NormalLine.ParseIntoArray(NormParts, TEXT("|"), true);
				Pair.LimbLine.ParseIntoArray(LimbParts, TEXT("|"), true);

				if (NormParts.Num() >= 2 && LimbParts.Num() >= 2)
				{
					double SecNormal = TimeToSeconds(NormParts[1].TrimStartAndEnd());
					double SecLimb = TimeToSeconds(LimbParts[1].TrimStartAndEnd());
					double Diff = SecLimb - SecNormal;

					LimbDiffStr = FString::Printf(TEXT("%+.1fs"), Diff);
				}
			}

			// Construim linia unificată cu 6 elemente (Eveniment pur, Ora corectată, Limb Diff, Alt, Azi, P)
			FString FormattedLineForTable = FString::Printf(TEXT("%s | %s | %s | %s | %s | %s"), *Ev, *Ti, *LimbDiffStr, *Al, *Az, *P_);
			CreateTableRow(FormattedLineForTable, Ev);

			float AltValue = FCString::Atof(*Al);
			if (AltValue < 0.0f)
			{
				if (Ev.Contains("C1") || Ev.Contains("C2")) Al += AOrbitalManager::TrString(TEXT(" (Nerasarit)"), TEXT(" (Not Risen)"));
				else if (Ev.Contains("C3") || Ev.Contains("C4")) Al += AOrbitalManager::TrString(TEXT(" (Apus)"), TEXT(" (Set)"));
				else if (Ev.Contains("MAX")) Al += AOrbitalManager::TrString(TEXT(" (Invizibil)"), TEXT(" (Invisible)"));
			}
			if (Ev.Contains(TEXT("Rasarit"))) Ev = AOrbitalManager::TrString(TEXT("Rasarit"), TEXT("Sunrise"));
			if (Ev.Contains(TEXT("Apus"))) Ev = AOrbitalManager::TrString(TEXT("Apus"), TEXT("Sunset"));

			FString CsvLine = FString::Printf(TEXT("%s,%s,%s,%s,%s,%s\n"), *Ev, *Ti, *LimbDiffStr, *Al, *Az, *P_);
			RawCsvText += CsvLine;
		}
	}

	if (GeneralInfoText) GeneralInfoText->SetText(FText::FromString(GeneralTextAccumulator));
}

void UEclipseWidget::OnSaveTxtClicked()
{
	if (RawReportText.IsEmpty()) return;

	struct FRow { FString Ev, Time, Limb, Alt, Azi, P; };
	struct FPair { FString Normal; FString Limb; };

	auto IsEventLine = [](const FString& L) -> bool
		{
			return L.StartsWith("C1") || L.StartsWith("C2") || L.StartsWith("C3") || L.StartsWith("C4") ||
				L.StartsWith("MAX") || L.StartsWith("Rasarit") || L.StartsWith("Apus") ||
				L.StartsWith("Sunrise") || L.StartsWith("Sunset");
		};

	auto TimeToSeconds = [](const FString& S) -> double
		{
			TArray<FString> T;
			S.ParseIntoArray(T, TEXT(":"), true);
			if (T.Num() != 3) return 0.0;
			return FCString::Atod(*T[0]) * 3600.0 + FCString::Atod(*T[1]) * 60.0 + FCString::Atod(*T[2]);
		};

	// --- Parsare: exact aceeași logică ca în SetReportText ---
	TArray<FRow> Rows;
	TArray<FString> GeneralLines;
	{
		TMap<FString, FPair> Paired;
		TArray<FString> Ordered;
		TArray<FString> Lines;
		RawReportText.ParseIntoArray(Lines, TEXT("\n"), false);

		for (const FString& Line : Lines)
		{
			FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || Trimmed.StartsWith("-") || Trimmed.StartsWith("Event")) continue;

			if (!IsEventLine(Trimmed))
			{
				GeneralLines.Add(Trimmed);
				continue;
			}

			TArray<FString> Parts;
			Trimmed.ParseIntoArray(Parts, TEXT("|"), true);
			if (Parts.Num() < 5) continue;

			FString EvRaw = Parts[0].TrimStartAndEnd();
			bool bIsLimb = EvRaw.EndsWith("L");
			FString BaseEv = bIsLimb ? EvRaw.LeftChop(1) : EvRaw;

			if (!Paired.Contains(BaseEv))
			{
				Paired.Add(BaseEv, FPair());
				Ordered.Add(BaseEv);
			}
			if (bIsLimb) Paired[BaseEv].Limb = Trimmed;
			else         Paired[BaseEv].Normal = Trimmed;
		}

		for (const FString& Key : Ordered)
		{
			const FPair& Pair = Paired[Key];
			const FString& Active = !Pair.Limb.IsEmpty() ? Pair.Limb : Pair.Normal;
			if (Active.IsEmpty()) continue;

			TArray<FString> Parts;
			Active.ParseIntoArray(Parts, TEXT("|"), true);
			if (Parts.Num() < 5) continue;

			FRow R;
			R.Ev = Key;
			R.Time = Parts[1].TrimStartAndEnd();
			R.Alt = Parts[2].TrimStartAndEnd();
			R.Azi = Parts[3].TrimStartAndEnd();
			R.P = Parts[4].TrimStartAndEnd();
			R.Limb = TEXT("0.0s");

			if (!Pair.Normal.IsEmpty() && !Pair.Limb.IsEmpty())
			{
				TArray<FString> N, L;
				Pair.Normal.ParseIntoArray(N, TEXT("|"), true);
				Pair.Limb.ParseIntoArray(L, TEXT("|"), true);
				if (N.Num() >= 2 && L.Num() >= 2)
				{
					double Diff = TimeToSeconds(L[1].TrimStartAndEnd()) - TimeToSeconds(N[1].TrimStartAndEnd());
					if (Diff > 43200.0)  Diff -= 86400.0; // trecere peste miezul nopții UT
					if (Diff < -43200.0) Diff += 86400.0;
					R.Limb = FString::Printf(TEXT("%+.1fs"), Diff);
				}
			}

			if (FCString::Atof(*R.Alt) < 0.0f)
			{
				if (Key.Contains("C1") || Key.Contains("C2")) R.Alt += AOrbitalManager::TrString(TEXT(" (Nerasarit)"), TEXT(" (Not Risen)"));
				else if (Key.Contains("C3") || Key.Contains("C4")) R.Alt += AOrbitalManager::TrString(TEXT(" (Apus)"), TEXT(" (Set)"));
				else if (Key.Contains("MAX")) R.Alt += AOrbitalManager::TrString(TEXT(" (Invizibil)"), TEXT(" (Invisible)"));
			}
			if (R.Ev.Contains(TEXT("Rasarit"))) R.Ev = AOrbitalManager::TrString(TEXT("Rasarit"), TEXT("Sunrise"));
			if (R.Ev.Contains(TEXT("Apus")))    R.Ev = AOrbitalManager::TrString(TEXT("Apus"), TEXT("Sunset"));

			Rows.Add(R);
		}
	}

	// --- Antet fișier ---
	FString FormattedText = TEXT("=======================================================================\n");
	FormattedText += AOrbitalManager::TrString(TEXT("                 RAPORT ASTRONOMIC ECLIPSA\n"), TEXT("                 ECLIPSE ASTRONOMICAL REPORT\n"));
	FormattedText += AOrbitalManager::TrString(TEXT("                 Generat de Simulatorul Astronomic\n"), TEXT("                 Generated by Astronomical Simulator\n"));
	FormattedText += TEXT("=======================================================================\n\n");

	// --- Informații generale ---
	FString GeneralInfo = AOrbitalManager::TrString(TEXT("[ INFORMATII GENERALE ]\n"), TEXT("[ GENERAL INFORMATION ]\n"));
	for (const FString& G : GeneralLines)
	{
		FString CleanLine = G;
		CleanLine = CleanLine.Replace(TEXT(">>> SHOWING NEXT UPCOMING ECLIPSE <<<"),
			*AOrbitalManager::TrString(TEXT("Stare: ARATA URMATOAREA ECLIPSA"), TEXT("Status: SHOWING UPCOMING ECLIPSE")));
		CleanLine = CleanLine.Replace(TEXT(">>> SHOWING ACTIVE ECLIPSE <<<"),
			*AOrbitalManager::TrString(TEXT("Stare: ECLIPSA ESTE ACTIVA ACUM"), TEXT("Status: ECLIPSE IS CURRENTLY ACTIVE")));
		CleanLine = CleanLine.Replace(TEXT(" | "), TEXT("\n  > "));
		GeneralInfo += FString::Printf(TEXT("  > %s\n"), *CleanLine);
	}

	// --- Tabel: aceleași coloane și același antet ca în fereastră ---
	auto MakeRow = [](const FString& Ev, const FString& Time, const FString& Limb,
		const FString& Alt, const FString& Azi, const FString& P) -> FString
		{
			return FString::Printf(TEXT("  %-10s|  %-12s|  %-16s|  %-20s|  %-10s|  %-4s\n"),
				*Ev, *Time, *Limb, *Alt, *Azi, *P);
		};

	FString HeaderRow = MakeRow(
		AOrbitalManager::TrString(TEXT("Eveniment"), TEXT("Event")),
		AOrbitalManager::TrString(TEXT("Ora UTC"), TEXT("UTC Time")),
		AOrbitalManager::TrString(TEXT("Limb Lunar"), TEXT("Limb Correction")),
		TEXT("Alt"), TEXT("Azi"), TEXT("P"));
	FString Sep = FString::ChrN(HeaderRow.Len() - 1, TEXT('-')) + TEXT("\n");

	FString TableData = AOrbitalManager::TrString(TEXT("[ TABEL EVENIMENTE (CONTACTE) ]\n"), TEXT("[ EVENT TABLE (CONTACTS) ]\n"));
	TableData += Sep + HeaderRow + Sep;
	for (const FRow& R : Rows)
	{
		TableData += MakeRow(R.Ev, R.Time, R.Limb, R.Alt, R.Azi, R.P);
	}
	TableData += Sep;

	FormattedText += GeneralInfo + TEXT("\n") + TableData + TEXT("\n");
	FormattedText += TEXT("=======================================================================\n");

	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[512] = { 0 };
	FCString::Strcpy(szFile, TEXT("Raport_Eclipsa.txt"));

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("Fisier Text (*.txt)\0*.txt\0Toate Fisierele (*.*)\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("txt");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif

	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(".txt")) FinalSavePath += ".txt";

	if (FFileHelper::SaveStringToFile(FormattedText, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, AOrbitalManager::TrString(TEXT("Raport TXT Salvat cu Succes!"), TEXT("TXT Report Saved Successfully!")));
	}
}
void UEclipseWidget::OnSaveCsvClicked()
{
	if (RawReportText.IsEmpty()) return;

	struct FRow { FString Ev, Time, Limb, Alt, Azi, P; };
	struct FPair { FString Normal; FString Limb; };

	auto IsEventLine = [](const FString& L) -> bool
		{
			return L.StartsWith("C1") || L.StartsWith("C2") || L.StartsWith("C3") || L.StartsWith("C4") ||
				L.StartsWith("MAX") || L.StartsWith("Rasarit") || L.StartsWith("Apus") ||
				L.StartsWith("Sunrise") || L.StartsWith("Sunset");
		};

	auto TimeToSeconds = [](const FString& S) -> double
		{
			TArray<FString> T;
			S.ParseIntoArray(T, TEXT(":"), true);
			if (T.Num() != 3) return 0.0;
			return FCString::Atod(*T[0]) * 3600.0 + FCString::Atod(*T[1]) * 60.0 + FCString::Atod(*T[2]);
		};

	// --- Parsare: exact aceeași logică ca în SetReportText ---
	TArray<FRow> Rows;
	{
		TMap<FString, FPair> Paired;
		TArray<FString> Ordered;
		TArray<FString> Lines;
		RawReportText.ParseIntoArray(Lines, TEXT("\n"), false);

		for (const FString& Line : Lines)
		{
			FString Trimmed = Line.TrimStartAndEnd();
			if (Trimmed.IsEmpty() || Trimmed.StartsWith("-") || Trimmed.StartsWith("Event")) continue;
			if (!IsEventLine(Trimmed)) continue;

			TArray<FString> Parts;
			Trimmed.ParseIntoArray(Parts, TEXT("|"), true);
			if (Parts.Num() < 5) continue;

			FString EvRaw = Parts[0].TrimStartAndEnd();
			bool bIsLimb = EvRaw.EndsWith("L");
			FString BaseEv = bIsLimb ? EvRaw.LeftChop(1) : EvRaw;

			if (!Paired.Contains(BaseEv))
			{
				Paired.Add(BaseEv, FPair());
				Ordered.Add(BaseEv);
			}
			if (bIsLimb) Paired[BaseEv].Limb = Trimmed;
			else         Paired[BaseEv].Normal = Trimmed;
		}

		for (const FString& Key : Ordered)
		{
			const FPair& Pair = Paired[Key];
			const FString& Active = !Pair.Limb.IsEmpty() ? Pair.Limb : Pair.Normal;
			if (Active.IsEmpty()) continue;

			TArray<FString> Parts;
			Active.ParseIntoArray(Parts, TEXT("|"), true);
			if (Parts.Num() < 5) continue;

			FRow R;
			R.Ev = Key;
			R.Time = Parts[1].TrimStartAndEnd();
			R.Alt = Parts[2].TrimStartAndEnd();
			R.Azi = Parts[3].TrimStartAndEnd();
			R.P = Parts[4].TrimStartAndEnd();
			R.Limb = TEXT("0.0s");

			if (!Pair.Normal.IsEmpty() && !Pair.Limb.IsEmpty())
			{
				TArray<FString> N, L;
				Pair.Normal.ParseIntoArray(N, TEXT("|"), true);
				Pair.Limb.ParseIntoArray(L, TEXT("|"), true);
				if (N.Num() >= 2 && L.Num() >= 2)
				{
					double Diff = TimeToSeconds(L[1].TrimStartAndEnd()) - TimeToSeconds(N[1].TrimStartAndEnd());
					if (Diff > 43200.0)  Diff -= 86400.0;
					if (Diff < -43200.0) Diff += 86400.0;
					R.Limb = FString::Printf(TEXT("%+.1fs"), Diff);
				}
			}

			if (FCString::Atof(*R.Alt) < 0.0f)
			{
				if (Key.Contains("C1") || Key.Contains("C2")) R.Alt += AOrbitalManager::TrString(TEXT(" (Nerasarit)"), TEXT(" (Not Risen)"));
				else if (Key.Contains("C3") || Key.Contains("C4")) R.Alt += AOrbitalManager::TrString(TEXT(" (Apus)"), TEXT(" (Set)"));
				else if (Key.Contains("MAX")) R.Alt += AOrbitalManager::TrString(TEXT(" (Invizibil)"), TEXT(" (Invisible)"));
			}
			if (R.Ev.Contains(TEXT("Rasarit"))) R.Ev = AOrbitalManager::TrString(TEXT("Rasarit"), TEXT("Sunrise"));
			if (R.Ev.Contains(TEXT("Apus")))    R.Ev = AOrbitalManager::TrString(TEXT("Apus"), TEXT("Sunset"));

			Rows.Add(R);
		}
	}

	// --- Construire CSV: același antet și aceleași coloane ca în tabel ---
	FString Csv = FString::Printf(TEXT("%s,%s,%s,Alt,Azi,P\n"),
		*AOrbitalManager::TrString(TEXT("Eveniment"), TEXT("Event")),
		*AOrbitalManager::TrString(TEXT("Ora UTC"), TEXT("UTC Time")),
		*AOrbitalManager::TrString(TEXT("Limb Lunar"), TEXT("Limb Correction")));

	for (const FRow& R : Rows)
	{
		Csv += FString::Printf(TEXT("%s,%s,%s,%s,%s,%s\n"), *R.Ev, *R.Time, *R.Limb, *R.Alt, *R.Azi, *R.P);
	}

	FString FinalSavePath = TEXT("");

#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[512] = { 0 };
	FCString::Strcpy(szFile, TEXT("Tabel_Date_Eclipsa.csv"));

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("Fisier Excel/CSV (*.csv)\0*.csv\0Toate Fisierele (*.*)\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("csv");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif

	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(".csv")) FinalSavePath += ".csv";

	if (FFileHelper::SaveStringToFile(Csv, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, AOrbitalManager::TrString(TEXT("Tabel CSV Salvat cu Succes!"), TEXT("CSV Table Saved Successfully!")));
	}
}
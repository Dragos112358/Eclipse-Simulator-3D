#include "EclipseBrowserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Font.h"
#include "OrbitalManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"
#include "Components/EditableTextBox.h" 
#include "DronaMea.h"                 
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Components/Slider.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CheckBox.h"
#include "DroneHUD.h"
#include "Components/ComboBoxString.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "HighResScreenshot.h"
#include "Misc/FileHelper.h"
#include "AVIWriter.h" 
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/ButtonSlot.h"
#include "Components/DirectionalLightComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Components/VolumetricCloudComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "NiagaraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/DrawElements.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "UObject/ConstructorHelpers.h"
#include <Components/ScaleBox.h>
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerStart.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"          
#include "Components/Overlay.h"        
#include "Components/OverlaySlot.h"    
#include "Components/ScaleBox.h"       
#include "Components/SizeBox.h"        
#include "Components/NativeWidgetHost.h"
#include "Widgets/Input/SHyperlink.h"
#include "HAL/PlatformProcess.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SkyLightComponent.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Async/ParallelFor.h"
#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/MinWindows.h"
#include <commdlg.h> 
#include "Windows/HideWindowsPlatformTypes.h"
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif
FSlateFontInfo GetFont(int32 Size, bool bBold = false)
{
	static UObject* CustomFontObj = LoadObject<UObject>(nullptr, TEXT("/Game/UI/Fonts/Inter_Font.Inter_Font"));
	if (CustomFontObj)
	{
		return FSlateFontInfo(CustomFontObj, Size, bBold ? FName("Bold") : FName("Regular"));
	}
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}
TSharedRef<SWidget> UEclipseBrowserWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* FullScreenBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FullScreenBackdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(FullScreenBackdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.f));
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(1300.0f, 800.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RootSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	BgBorder->SetContent(RootSwitcher);
	RootSwitcher->AddChild(MainLayout);
	RootSwitcher->AddChild(BuildDetailPanel());
	RootSwitcher->SetActiveWidgetIndex(0);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(HeaderBox)->SetPadding(FMargin(10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr("CATALOG ECLIPSE", "ECLIPSE CATALOG"));
	Title->SetFont(GetFont(24, true));
	HeaderBox->AddChildToHorizontalBox(Title);
	UHorizontalBox* ModeTabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBox->AddChildToHorizontalBox(ModeTabs)->SetPadding(FMargin(20.f, 0.f));
	auto MakeTabButton = [&](const FString& RO, const FString& EN, ECatalogMode Mode) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Btn->SetBackgroundColor(CurrentMode == Mode ? FLinearColor(0.1f, 0.4f, 0.8f) : FLinearColor(0.2f, 0.2f, 0.2f));
			UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Txt->SetText(AOrbitalManager::Tr(RO, EN));
			Txt->SetFont(GetFont(13, true));
			Txt->SetVisibility(ESlateVisibility::HitTestInvisible);
			Btn->AddChild(Txt);
			ModeTabs->AddChildToHorizontalBox(Btn)->SetPadding(FMargin(3.f, 0.f));
			return Btn;
		};
	TabSolarButton = MakeTabButton("SOARE", "SUN", ECatalogMode::Solar);
	TabSolarButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnSwitchToSolar);
	TabLunarButton = MakeTabButton("LUNA", "MOON", ECatalogMode::Lunar);
	TabLunarButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnSwitchToLunar);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	UHorizontalBoxSlot* SpSlot = HeaderBox->AddChildToHorizontalBox(Spacer);
	SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Red);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString(" X "));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	FEditableTextBoxStyle InputStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	InputStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	InputStyle.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	InputStyle.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	InputStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	InputStyle.TextStyle.Font = GetFont(12);
	UHorizontalBox* FilterBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(FilterBar)->SetPadding(FMargin(10.f, 0.f, 10.f, 10.f));
	UTextBlock* SortLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SortLbl->SetText(AOrbitalManager::Tr("Sorteaza: ", "Sort by: "));
	SortLbl->SetFont(GetFont(12));
	FilterBar->AddChildToHorizontalBox(SortLbl)->SetVerticalAlignment(VAlign_Center);
	FString Lang = AOrbitalManager::TrString("Romana", "English");
	SortComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	SortComboBox->SetWidgetStyle(ComboStyle);
	SortComboBox->ItemStyle = ComboRowStyle;
	SortComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	if (Lang == "English")
	{
		SortComboBox->AddOption("Year (Ascending)");
		SortComboBox->AddOption("Year (Descending)");
		SortComboBox->AddOption("Coverage (Max)");
	}
	else
	{
		SortComboBox->AddOption("An (Crescator)");
		SortComboBox->AddOption("An (Descrescator)");
		SortComboBox->AddOption("Acoperire (Maxim)");
	}
	SortComboBox->SetSelectedIndex(0);
	SortComboBox->OnSelectionChanged.AddDynamic(this, &UEclipseBrowserWidget::OnSortChanged);
	FilterBar->AddChildToHorizontalBox(SortComboBox)->SetPadding(FMargin(5.f, 0.f, 20.f, 0.f));
	UTextBlock* FilterLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FilterLbl->SetText(AOrbitalManager::Tr("Filtru: ", "Filter: "));
	FilterLbl->SetFont(GetFont(12));
	FilterBar->AddChildToHorizontalBox(FilterLbl)->SetVerticalAlignment(VAlign_Center);
	FilterMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FilterMenuButton->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.17f, 1.0f));
	FilterMenuButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnFilterMenuButtonClicked);
	FilterMenuLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FilterMenuLabel->SetFont(GetFont(12));
	FilterMenuButton->AddChild(FilterMenuLabel);
	USizeBox* FilterMenuSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	FilterMenuSizeBox->SetWidthOverride(400.f);
	FilterMenuSizeBox->SetContent(FilterMenuButton);
	FilterBar->AddChildToHorizontalBox(FilterMenuSizeBox)->SetVerticalAlignment(VAlign_Center);
	UpdateFilterMenuLabel();
	USpacer* YearSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	FilterBar->AddChildToHorizontalBox(YearSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MinYearInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	MinYearInput->SetWidgetStyle(InputStyle);
	MinYearInput->SetHintText(AOrbitalManager::Tr("An Min", "Min Yr"));
	MinYearInput->SetJustification(ETextJustify::Center);
	USizeBox* MinSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MinSizeBox->SetWidthOverride(120.0f);
	MinSizeBox->SetContent(MinYearInput);
	FilterBar->AddChildToHorizontalBox(MinSizeBox)->SetPadding(FMargin(10.f, 0.f, 5.f, 0.f));
	MaxYearInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	MaxYearInput->SetWidgetStyle(InputStyle);
	MaxYearInput->SetHintText(AOrbitalManager::Tr("An Max", "Max Yr"));
	MaxYearInput->SetJustification(ETextJustify::Center);
	USizeBox* MaxSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MaxSizeBox->SetWidthOverride(120.0f);
	MaxSizeBox->SetContent(MaxYearInput);
	FilterBar->AddChildToHorizontalBox(MaxSizeBox)->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	ApplyYearButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ApplyYearButton->SetBackgroundColor(FLinearColor(0.1f, 0.4f, 0.8f));
	UTextBlock* ApplyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LocationModeComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	LocationModeComboBox->SetWidgetStyle(ComboStyle);
	LocationModeComboBox->ItemStyle = ComboRowStyle;
	LocationModeComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	LocationModeComboBox->AddOption(AOrbitalManager::TrString("Toate Locurile", "All Locations"));
	LocationModeComboBox->AddOption(AOrbitalManager::TrString("Doar Aici (Drona)", "Only Here (Drone)"));
	FString DetectedCountry = ExtractCurrentCountryName();
	FString CountryOptionLabel;
	if (DetectedCountry.IsEmpty())
	{
		CountryOptionLabel = AOrbitalManager::TrString("Tara Curenta (Necunoscuta)", "Current Country (Unknown)");
	}
	else
	{
		CountryOptionLabel = FString::Printf(TEXT("%s: %s"),
			*AOrbitalManager::TrString("Tara Curenta", "Current Country"), *DetectedCountry);
	}
	LocationModeComboBox->AddOption(CountryOptionLabel);
	AOrbitalManager* InitManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (InitManager && InitManager->CountryPoints.Num() == 0)
	{
		InitManager->LoadCountryPoints();
	}
	LocationModeComboBox->SetSelectedIndex(0);
	LocationModeComboBox->OnSelectionChanged.AddDynamic(this, &UEclipseBrowserWidget::OnLocationModeChanged);
	FilterBar->AddChildToHorizontalBox(LocationModeComboBox)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	LocationModeComboBox->SetVisibility(CurrentMode == ECatalogMode::Lunar ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	ApplyTxt->SetText(AOrbitalManager::Tr(" CAUTA ", " SEARCH "));
	ApplyTxt->SetFont(GetFont(12, true));
	ApplyYearButton->AddChild(ApplyTxt);
	ApplyYearButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnYearFilterClicked);
	FilterDropdownPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FilterDropdownPanel->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	FilterDropdownPanel->SetVisibility(ESlateVisibility::Collapsed);
	MainLayout->AddChildToVerticalBox(FilterDropdownPanel)->SetPadding(FMargin(10.f, 0.f, 10.f, 10.f));
	RefreshFilterDropdownContent();
	FilterBar->AddChildToHorizontalBox(ApplyYearButton);
	UBorder* ListBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ListBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.f));
	EclipseListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ListBg->SetContent(EclipseListScroll);
	UVerticalBoxSlot* ListSlot = MainLayout->AddChildToVerticalBox(ListBg);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ListSlot->SetPadding(FMargin(10.f));
	UHorizontalBox* BottomButtonsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(BottomButtonsBox)->SetPadding(FMargin(10.f));
	LoadMoreButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	LoadMoreButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* MoreTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MoreTxt->SetText(AOrbitalManager::Tr("ARATA URMATOARELE 50...", "SHOW NEXT 50..."));
	MoreTxt->SetJustification(ETextJustify::Center);
	LoadMoreButton->AddChild(MoreTxt);
	LoadMoreButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnLoadMoreClicked);
	LoadMoreButton->SetVisibility(ESlateVisibility::Collapsed);
	UHorizontalBoxSlot* LoadMoreSlot = BottomButtonsBox->AddChildToHorizontalBox(LoadMoreButton);
	LoadMoreSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LoadMoreSlot->SetPadding(FMargin(5.f));
	ResetFilterButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ResetFilterButton->SetBackgroundColor(FLinearColor(0.8f, 0.1f, 0.1f, 1.0f));
	ResetFilterButton->SetVisibility(ESlateVisibility::Collapsed);
	ResetFilterButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::ResetFilters);
	UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(AOrbitalManager::Tr("X INCHIDE MODUL SAROS", "X CLOSE SAROS MODE"));
	ResetTxt->SetJustification(ETextJustify::Center);
	ResetTxt->SetFont(GetFont(14, true));
	ResetFilterButton->AddChild(ResetTxt);
	UHorizontalBoxSlot* ResetSlot = BottomButtonsBox->AddChildToHorizontalBox(ResetFilterButton);
	ResetSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ResetSlot->SetPadding(FMargin(5.f));
	if (AllEclipses.Num() == 0)
	{
		AllEclipses.Add({ "Eclipsa Spania", FDateTime(2026, 8, 12), 1.0f, EEclipseType::Total });
		AllEclipses.Add({ "Eclipsa Partiala RO", FDateTime(2027, 8, 2), 0.85f, EEclipseType::Partial });
		AllEclipses.Add({ "Eclipsa Inelara Chile", FDateTime(2024, 10, 2), 0.92f, EEclipseType::Annular });
		AllEclipses.Add({ "Eclipsa Totala Egipt", FDateTime(2027, 8, 2), 1.0f, EEclipseType::Total });
	}
	RefreshList();
	return Super::RebuildWidget();
}
void UEclipseBrowserWidget::RefreshFilterDropdownContent()
{
	if (!FilterDropdownPanel) return;
	FilterDropdownPanel->SetContent(BuildFilterMenuContent());
}
UWidget* UEclipseBrowserWidget::BuildFilterMenuContent()
{
	UBorder* MenuBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MenuBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MenuBg->SetPadding(FMargin(6.f));
	UHorizontalBox* Box = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MenuBg->SetContent(Box);
	auto AddToggleRow = [&](const FString& RO, const FString& EN, bool bActive, FLinearColor ActiveColor) -> UButton*
		{
			UButton* Row = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Row->SetBackgroundColor(bActive ? ActiveColor : FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
			UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Txt->SetText(FText::FromString(AOrbitalManager::TrString(RO, EN)));
			Txt->SetFont(GetFont(12, bActive));
			Txt->SetJustification(ETextJustify::Center);
			Txt->SetVisibility(ESlateVisibility::HitTestInvisible);
			Row->AddChild(Txt);
			return Row;
		};
	if (CurrentMode == ECatalogMode::Solar)
	{
		UButton* RowTotal = AddToggleRow("Totala", "Total", bShowTotal, FLinearColor(1.f, 0.2f, 0.2f));
		RowTotal->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterTotal);
		UHorizontalBoxSlot* SlotTotal = Box->AddChildToHorizontalBox(RowTotal);
		SlotTotal->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotTotal->SetPadding(FMargin(2.f, 0.f));
		UButton* RowPartial = AddToggleRow("Partiala", "Partial", bShowPartial, FLinearColor(1.f, 0.8f, 0.2f));
		RowPartial->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterPartial);
		UHorizontalBoxSlot* SlotPartial = Box->AddChildToHorizontalBox(RowPartial);
		SlotPartial->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotPartial->SetPadding(FMargin(2.f, 0.f));
		UButton* RowAnnular = AddToggleRow("Inelara", "Annular", bShowAnnular, FLinearColor(0.2f, 0.8f, 1.f));
		RowAnnular->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterAnnular);
		UHorizontalBoxSlot* SlotAnnular = Box->AddChildToHorizontalBox(RowAnnular);
		SlotAnnular->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotAnnular->SetPadding(FMargin(2.f, 0.f));
		UButton* RowHybrid = AddToggleRow("Hibrida", "Hybrid", bShowHybrid, FLinearColor(0.8f, 0.3f, 1.f));
		RowHybrid->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterHybrid);
		UHorizontalBoxSlot* SlotHybrid = Box->AddChildToHorizontalBox(RowHybrid);
		SlotHybrid->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotHybrid->SetPadding(FMargin(2.f, 0.f));
	}
	else
	{
		UButton* RowPnP = AddToggleRow("Penumbrala Partiala", "Penumbral Partial", bShowPenumbralPartial, FLinearColor(0.6f, 0.6f, 0.9f));
		RowPnP->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterPenumbralPartial);
		UHorizontalBoxSlot* SlotPnP = Box->AddChildToHorizontalBox(RowPnP);
		SlotPnP->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotPnP->SetPadding(FMargin(2.f, 0.f));
		UButton* RowPnT = AddToggleRow("Penumbrala Totala", "Penumbral Total", bShowPenumbralTotal, FLinearColor(0.4f, 0.4f, 0.9f));
		RowPnT->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterPenumbralTotal);
		UHorizontalBoxSlot* SlotPnT = Box->AddChildToHorizontalBox(RowPnT);
		SlotPnT->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotPnT->SetPadding(FMargin(2.f, 0.f));
		UButton* RowUmP = AddToggleRow("Umbrala Partiala", "Umbral Partial", bShowUmbralPartial, FLinearColor(0.9f, 0.7f, 0.2f));
		RowUmP->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterUmbralPartial);
		UHorizontalBoxSlot* SlotUmP = Box->AddChildToHorizontalBox(RowUmP);
		SlotUmP->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotUmP->SetPadding(FMargin(2.f, 0.f));
		UButton* RowUmT = AddToggleRow("Umbrala Totala", "Umbral Total", bShowUmbralTotal, FLinearColor(0.9f, 0.3f, 0.2f));
		RowUmT->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterUmbralTotal);
		UHorizontalBoxSlot* SlotUmT = Box->AddChildToHorizontalBox(RowUmT);
		SlotUmT->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotUmT->SetPadding(FMargin(2.f, 0.f));
		UButton* RowUmC = AddToggleRow("Umbrala Centrala", "Umbral Central", bShowUmbralCentral, FLinearColor(0.7f, 0.1f, 0.1f));
		RowUmC->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleFilterUmbralCentral);
		UHorizontalBoxSlot* SlotUmC = Box->AddChildToHorizontalBox(RowUmC);
		SlotUmC->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SlotUmC->SetPadding(FMargin(2.f, 0.f));
	}
	return MenuBg;
}
void UEclipseBrowserWidget::UpdateFilterMenuLabel()
{
	if (!FilterMenuLabel) return;
	if (CurrentMode == ECatalogMode::Solar)
	{
		if (bShowTotal && bShowPartial && bShowAnnular && bShowHybrid)
		{
			FilterMenuLabel->SetText(AOrbitalManager::Tr(" Filtru: Toate ", " Filter: All "));
			return;
		}
		if (!bShowTotal && !bShowPartial && !bShowAnnular && !bShowHybrid)
		{
			FilterMenuLabel->SetText(AOrbitalManager::Tr(" Filtru: Niciuna ", " Filter: None "));
			return;
		}
		TArray<FString> Parts;
		if (bShowTotal)   Parts.Add(AOrbitalManager::TrString("Totala", "Total"));
		if (bShowPartial) Parts.Add(AOrbitalManager::TrString("Partiala", "Partial"));
		if (bShowAnnular) Parts.Add(AOrbitalManager::TrString("Inelara", "Annular"));
		if (bShowHybrid)  Parts.Add(AOrbitalManager::TrString("Hibrida", "Hybrid"));
		FilterMenuLabel->SetText(FText::FromString(FString::Printf(TEXT(" %s "), *FString::Join(Parts, TEXT(", ")))));
	}
	else
	{
		if (bShowPenumbralPartial && bShowPenumbralTotal && bShowUmbralPartial && bShowUmbralTotal && bShowUmbralCentral)
		{
			FilterMenuLabel->SetText(AOrbitalManager::Tr(" Filtru: Toate ", " Filter: All "));
			return;
		}
		if (!bShowPenumbralPartial && !bShowPenumbralTotal && !bShowUmbralPartial && !bShowUmbralTotal && !bShowUmbralCentral)
		{
			FilterMenuLabel->SetText(AOrbitalManager::Tr(" Filtru: Niciuna ", " Filter: None "));
			return;
		}
		TArray<FString> Parts;
		if (bShowPenumbralPartial) Parts.Add(AOrbitalManager::TrString("PenumPartiala", "PenumPartial"));
		if (bShowPenumbralTotal)   Parts.Add(AOrbitalManager::TrString("PenumTotala", "PenumTotal"));
		if (bShowUmbralPartial)    Parts.Add(AOrbitalManager::TrString("UmbPartiala", "UmbPartial"));
		if (bShowUmbralTotal)      Parts.Add(AOrbitalManager::TrString("UmbTotala", "UmbTotal"));
		if (bShowUmbralCentral)    Parts.Add(AOrbitalManager::TrString("UmbCentrala", "UmbCentral"));
		FilterMenuLabel->SetText(FText::FromString(FString::Printf(TEXT(" %s "), *FString::Join(Parts, TEXT(", ")))));
	}
}
void UEclipseBrowserWidget::OnToggleFilterPenumbralPartial()
{
	bShowPenumbralPartial = !bShowPenumbralPartial;
	RefreshFilterDropdownContent();
	UpdateFilterMenuLabel();
	RefreshList();
}
void UEclipseBrowserWidget::OnToggleFilterPenumbralTotal()
{
	bShowPenumbralTotal = !bShowPenumbralTotal;
	RefreshFilterDropdownContent();
	UpdateFilterMenuLabel();
	RefreshList();
}
void UEclipseBrowserWidget::OnToggleFilterUmbralPartial()
{
	bShowUmbralPartial = !bShowUmbralPartial;
	RefreshFilterDropdownContent();
	UpdateFilterMenuLabel();
	RefreshList();
}
void UEclipseBrowserWidget::OnToggleFilterUmbralTotal()
{
	bShowUmbralTotal = !bShowUmbralTotal;
	RefreshFilterDropdownContent();
	UpdateFilterMenuLabel();
	RefreshList();
}
void UEclipseBrowserWidget::OnToggleFilterUmbralCentral()
{
	bShowUmbralCentral = !bShowUmbralCentral;
	RefreshFilterDropdownContent();
	UpdateFilterMenuLabel();
	RefreshList();
}
UWidget* UEclipseBrowserWidget::BuildDetailPanel()
{
	DetailPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DetailPanel->SetBrushColor(FLinearColor(0.07f, 0.07f, 0.08f, 1.f));
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	DetailPanel->SetContent(Layout);
	auto MakeFont = [](int32 Size, bool bBold = false) -> FSlateFontInfo
		{
			return FSlateFontInfo(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
		};
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.f));
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(Header);
	Layout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	DetailBackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	DetailBackButton->SetBackgroundColor(FLinearColor(0.10f, 0.26f, 0.40f, 1.0f));
	UTextBlock* BackTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BackTxt->SetText(AOrbitalManager::Tr(" INAPOI ", " BACK "));
	BackTxt->SetFont(MakeFont(13, true));
	BackTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.95f, 1.0f)));
	DetailBackButton->AddChild(BackTxt);
	DetailBackButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnDetailBackClicked);
	Header->AddChildToHorizontalBox(DetailBackButton)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr("DETALII ECLIPSA", "ECLIPSE DETAILS"));
	Title->SetFont(MakeFont(22, true));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
	DetailContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Layout->AddChildToVerticalBox(DetailContentBox)->SetPadding(FMargin(20.f, 15.f, 20.f, 10.f));
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Sep->SetBrushColor(FLinearColor(0.22f, 0.22f, 0.24f));
	USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SepSize->SetHeightOverride(1.f);
	SepSize->SetContent(Sep);
	Layout->AddChildToVerticalBox(SepSize)->SetPadding(FMargin(20.f, 5.f, 20.f, 10.f));
	UHorizontalBox* CitiesHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Layout->AddChildToVerticalBox(CitiesHeaderRow)->SetPadding(FMargin(20.f, 0.f, 20.f, 8.f));
	UTextBlock* CitiesTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CitiesTitle->SetText(AOrbitalManager::Tr("Orase in traiectorie", "Cities along path"));
	CitiesTitle->SetFont(MakeFont(17, true));
	UHorizontalBoxSlot* CTSlot = CitiesHeaderRow->AddChildToHorizontalBox(CitiesTitle);
	CTSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CTSlot->SetVerticalAlignment(VAlign_Center);
	auto CreateActionBtn = [this, &MakeFont](const FText& BtnText, FLinearColor BgColor) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			Btn->SetBackgroundColor(BgColor);
			UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Txt->SetText(BtnText);
			Txt->SetFont(MakeFont(11, true));
			Txt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Btn->AddChild(Txt);
			return Btn;
		};
	ExportCsvButton = CreateActionBtn(FText::FromString(TEXT("CSV")), FLinearColor(0.12f, 0.40f, 0.25f, 1.f));
	ExportCsvButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnExportCsvClicked);
	UHorizontalBoxSlot* CsvSlot = CitiesHeaderRow->AddChildToHorizontalBox(ExportCsvButton);
	CsvSlot->SetVerticalAlignment(VAlign_Center);
	CsvSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	ExportTxtButton = CreateActionBtn(FText::FromString(TEXT("TXT")), FLinearColor(0.35f, 0.22f, 0.10f, 1.f));
	ExportTxtButton->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnExportTxtClicked);
	UHorizontalBoxSlot* TxtSlot = CitiesHeaderRow->AddChildToHorizontalBox(ExportTxtButton);
	TxtSlot->SetVerticalAlignment(VAlign_Center);
	TxtSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	FEditableTextBoxStyle SearchStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	SearchStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	SearchStyle.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	SearchStyle.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	SearchStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	SearchStyle.TextStyle.Font = MakeFont(12);
	CitySearchInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	CitySearchInput->SetWidgetStyle(SearchStyle);
	CitySearchInput->SetHintText(AOrbitalManager::Tr("Cauta oras sau tara...", "Search city or country..."));
	CitySearchInput->OnTextChanged.AddDynamic(this, &UEclipseBrowserWidget::OnCitySearchChanged);
	USizeBox* SearchSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SearchSizeBox->SetWidthOverride(190.f);
	SearchSizeBox->SetContent(CitySearchInput);
	CitiesHeaderRow->AddChildToHorizontalBox(SearchSizeBox)->SetVerticalAlignment(VAlign_Center);
	CitySummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CitySummaryText->SetFont(MakeFont(12));
	CitySummaryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
	Layout->AddChildToVerticalBox(CitySummaryText)->SetPadding(FMargin(20.f, 0.f, 20.f, 5.f));
	UBorder* ListBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ListBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.11f, 1.f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ListBg->SetContent(Scroll);
	UVerticalBoxSlot* ScrollSlot = Layout->AddChildToVerticalBox(ListBg);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	CityListBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(CityListBox);
	return DetailPanel;
}
/*void UEclipseBrowserWidget::OnToggleAllCitiesClicked()
{
	bShowAllCities = !bShowAllCities;
	if (ToggleAllCitiesButton)
	{
		if (UTextBlock* BtnTxt = Cast<UTextBlock>(ToggleAllCitiesButton->GetChildAt(0)))
		{
			BtnTxt->SetText(bShowAllCities ?
				AOrbitalManager::Tr("DOAR VIZIBILE", "ONLY VISIBLE") :
				AOrbitalManager::Tr("TOATE ORASELE", "ALL CITIES"));
		}
	}
	PopulateCityVisibility(CurrentDetailEclipse);
}*/
void UEclipseBrowserWidget::OnToggleAllCitiesClicked()
{
	bShowAllCities = !bShowAllCities;
	if (ToggleAllCitiesButton)
	{
		if (UTextBlock* BtnTxt = Cast<UTextBlock>(ToggleAllCitiesButton->GetChildAt(0)))
		{
			BtnTxt->SetText(bShowAllCities ?
				AOrbitalManager::Tr("DOAR VIZIBILE", "ONLY VISIBLE") :
				AOrbitalManager::Tr("TOATE ORASELE", "ALL CITIES"));
		}
	}
	RenderCityList(FString());   // <-- doar re-randare, fara refetch
}
static FString FormatUtcHourString(float UtcHour)
{
	float Wrapped = FMath::Fmod(UtcHour, 24.0f);
	if (Wrapped < 0.0f) Wrapped += 24.0f;
	const int32 H = FMath::FloorToInt(Wrapped);
	const float MinFrac = (Wrapped - H) * 60.0f;
	const int32 M = FMath::FloorToInt(MinFrac);
	const int32 S = FMath::FloorToInt((MinFrac - M) * 60.0f);
	return FString::Printf(TEXT("%02d:%02d:%02d"), H, M, S);
}
/*void UEclipseBrowserWidget::OnExportCsvClicked()
{
	if (CachedCityHits.Num() == 0) return;
	FString CsvContent = AOrbitalManager::TrString(
		TEXT("Oras,Tara,Latitudine,Longitudine,Altitudine,Magnitudine,Acoperire_Pct,Tip\n"),
		TEXT("City,Country,Latitude,Longitude,Altitude,Magnitude,Coverage_Pct,Type\n")
	);
	for (const FEclipseCityHit& Hit : CachedCityHits)
	{
		FString TypeStr = AOrbitalManager::TrString(TEXT("Partiala"), TEXT("Partial"));
		if (Hit.Magnitude >= 1.0f)
		{
			TypeStr = (CurrentDetailEclipse.Type == EEclipseType::Annular)
				? AOrbitalManager::TrString(TEXT("Inelara"), TEXT("Annular"))
				: AOrbitalManager::TrString(TEXT("Totala"), TEXT("Total"));
		}
		else if (Hit.Magnitude <= 0.0f)
		{
			TypeStr = AOrbitalManager::TrString(TEXT("Niciuna"), TEXT("None"));
		}
		CsvContent += FString::Printf(TEXT("\"%s\",\"%s\",%.4f,%.4f,%.1f,%.4f,%.3f%%,%s\n"),
			*Hit.CityName, *Hit.Country, Hit.Latitude, Hit.Longitude, Hit.Altitude, Hit.Magnitude, Hit.Magnitude * 100.f, *TypeStr);
	}
	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[1024] = { 0 };
	TCHAR szFilter[1024] = { 0 };
	const FString DefaultName = FString::Printf(TEXT("Eclipse_%d_%02d_%02d_Cities.csv"),
		CurrentDetailEclipse.RealYear, CurrentDetailEclipse.Month, CurrentDetailEclipse.Day);
	FCString::Strncpy(szFile, *DefaultName, 1024);
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = 1024;
	const FString FilterLabel = AOrbitalManager::TrString(TEXT("Fisier Excel/CSV (*.csv)|*.csv|Toate Fisierele (*.*)|*.*|"), TEXT("Excel/CSV File (*.csv)|*.csv|All Files (*.*)|*.*|"));
	FCString::Strncpy(szFilter, *FilterLabel, 1024);
	for (int32 i = 0; i < 1024 && szFilter[i] != TEXT('\0'); ++i)
	{
		if (szFilter[i] == TEXT('|'))
		{
			szFilter[i] = TEXT('\0');
		}
	}
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("csv");
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(TEXT(".csv"))) FinalSavePath += TEXT(".csv");
	if (FFileHelper::SaveStringToFile(CsvContent, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine)
		{
			const FString MsgPrefix = AOrbitalManager::TrString(TEXT("Tabel CSV Salvat: "), TEXT("CSV Table Saved: "));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, MsgPrefix + FinalSavePath);
		}
	}
}*/
void UEclipseBrowserWidget::OnExportCsvClicked()
{
	if (CachedCityHits.Num() == 0) return;

	const bool bLunar = bLastCityQueryWasLunar;
	FString CsvContent;
	FString DefaultName;

	if (bLunar)
	{
		const FLunarEclipseElements& LE = CurrentDetailLunarEclipse;
		const FString DateStr = FString::Printf(TEXT("%02d.%02d.%d"), LE.Day, LE.Month, LE.Year);
		const FString TypeStr = GetLunarTypeString(LE.TypeNumber);

		DefaultName = FString::Printf(TEXT("LunarEclipse_%d_%02d_%02d_Cities.csv"), LE.Year, LE.Month, LE.Day);

		CsvContent = AOrbitalManager::TrString(
			TEXT("Oras,Tara,Latitudine,Longitudine,Altitudine,Data,Ora_Max_UTC,Tip_Eclipsa,Saros\n"),
			TEXT("City,Country,Latitude,Longitude,Altitude,Date,Max_Time_UTC,Eclipse_Type,Saros\n"));

		for (const FEclipseCityHit& Hit : CachedCityHits)
		{
			CsvContent += FString::Printf(TEXT("\"%s\",\"%s\",%.4f,%.4f,%.1f,%s,%s,\"%s\",%d\n"),
				*Hit.CityName, *Hit.Country, Hit.Latitude, Hit.Longitude, Hit.Altitude,
				*DateStr, *FormatUtcHourString(Hit.UtcHour), *TypeStr, LE.Saros);
		}
	}
	else
	{
		DefaultName = FString::Printf(TEXT("Eclipse_%d_%02d_%02d_Cities.csv"),
			CurrentDetailEclipse.RealYear, CurrentDetailEclipse.Month, CurrentDetailEclipse.Day);

		CsvContent = AOrbitalManager::TrString(
			TEXT("Oras,Tara,Latitudine,Longitudine,Altitudine,Magnitudine,Acoperire_Pct,Ora_Max_UTC,Tip\n"),
			TEXT("City,Country,Latitude,Longitude,Altitude,Magnitude,Coverage_Pct,Max_Time_UTC,Type\n"));

		for (const FEclipseCityHit& Hit : CachedCityHits)
		{
			FString TypeStr = AOrbitalManager::TrString(TEXT("Partiala"), TEXT("Partial"));
			if (Hit.Magnitude >= 1.0f)
			{
				TypeStr = (CurrentDetailEclipse.Type == EEclipseType::Annular)
					? AOrbitalManager::TrString(TEXT("Inelara"), TEXT("Annular"))
					: AOrbitalManager::TrString(TEXT("Totala"), TEXT("Total"));
			}
			else if (Hit.Magnitude <= 0.0f)
			{
				TypeStr = AOrbitalManager::TrString(TEXT("Niciuna"), TEXT("None"));
			}
			CsvContent += FString::Printf(TEXT("\"%s\",\"%s\",%.4f,%.4f,%.1f,%.4f,%.3f%%,%s,%s\n"),
				*Hit.CityName, *Hit.Country, Hit.Latitude, Hit.Longitude, Hit.Altitude,
				Hit.Magnitude, Hit.Magnitude * 100.f, *FormatUtcHourString(Hit.UtcHour), *TypeStr);
		}
	}

	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[1024] = { 0 };
	TCHAR szFilter[1024] = { 0 };
	FCString::Strncpy(szFile, *DefaultName, 1024);
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = 1024;
	const FString FilterLabel = AOrbitalManager::TrString(TEXT("Fisier Excel/CSV (*.csv)|*.csv|Toate Fisierele (*.*)|*.*|"), TEXT("Excel/CSV File (*.csv)|*.csv|All Files (*.*)|*.*|"));
	FCString::Strncpy(szFilter, *FilterLabel, 1024);
	for (int32 i = 0; i < 1024 && szFilter[i] != TEXT('\0'); ++i)
	{
		if (szFilter[i] == TEXT('|')) szFilter[i] = TEXT('\0');
	}
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("csv");
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(TEXT(".csv"))) FinalSavePath += TEXT(".csv");

	if (FFileHelper::SaveStringToFile(CsvContent, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine)
		{
			const FString MsgPrefix = AOrbitalManager::TrString(TEXT("Tabel CSV Salvat: "), TEXT("CSV Table Saved: "));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, MsgPrefix + FinalSavePath);
		}
	}
}

/*void UEclipseBrowserWidget::OnExportTxtClicked()
{
	if (CachedCityHits.Num() == 0) return;
	FString TxtContent;
	TxtContent += TEXT("====================================================================================\n");
	TxtContent += FString::Printf(TEXT("%s: %s\n"),
		*AOrbitalManager::TrString(TEXT("RAPORT DETALII ECLIPSA"), TEXT("ECLIPSE DETAILS REPORT")),
		*CurrentDetailEclipse.Name);
	TxtContent += FString::Printf(TEXT("%s: %s | %s: %d\n"),
		*AOrbitalManager::TrString(TEXT("DATA"), TEXT("DATE")), *CurrentDetailEclipse.FormattedDateStr,
		*AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")), CurrentDetailEclipse.SarosSeries);
	TxtContent += TEXT("====================================================================================\n\n");
	TxtContent += AOrbitalManager::TrString(
		TEXT("Oras                       | Tara                 | Lat      | Lon      | Mag    | Acoper.| Tip\n"),
		TEXT("City                       | Country              | Lat      | Lon      | Mag    | Cover  | Type\n")
	);
	TxtContent += TEXT("------------------------------------------------------------------------------------\n");
	for (const FEclipseCityHit& Hit : CachedCityHits)
	{
		FString TypeStr = AOrbitalManager::TrString(TEXT("PARTIALA"), TEXT("PARTIAL"));
		if (Hit.Magnitude >= 1.0f)
		{
			TypeStr = (CurrentDetailEclipse.Type == EEclipseType::Annular)
				? AOrbitalManager::TrString(TEXT("INELARA"), TEXT("ANNULAR"))
				: AOrbitalManager::TrString(TEXT("TOTALA"), TEXT("TOTAL"));
		}
		else if (Hit.Magnitude <= 0.0f)
		{
			TypeStr = AOrbitalManager::TrString(TEXT("NICIUNA"), TEXT("NONE"));
		}
		TxtContent += FString::Printf(TEXT("%-26s | %-20s | %+08.3f | %+08.3f | %.4f | %.3f%% | %s\n"),
			*Hit.CityName.Left(26), *Hit.Country.Left(20), Hit.Latitude, Hit.Longitude, Hit.Magnitude, Hit.Magnitude * 100.f, *TypeStr);
	}
	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[1024] = { 0 };
	TCHAR szFilter[1024] = { 0 };
	const FString DefaultName = FString::Printf(TEXT("Eclipse_%d_%02d_%02d_Report.txt"),
		CurrentDetailEclipse.RealYear, CurrentDetailEclipse.Month, CurrentDetailEclipse.Day);
	FCString::Strncpy(szFile, *DefaultName, 1024);
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = 1024;
	const FString FilterLabel = AOrbitalManager::TrString(TEXT("Fisier Text (*.txt)|*.txt|Toate Fisierele (*.*)|*.*|"), TEXT("Text File (*.txt)|*.txt|All Files (*.*)|*.*|"));
	FCString::Strncpy(szFilter, *FilterLabel, 1024);
	for (int32 i = 0; i < 1024 && szFilter[i] != TEXT('\0'); ++i)
	{
		if (szFilter[i] == TEXT('|'))
		{
			szFilter[i] = TEXT('\0');
		}
	}
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("txt");
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(TEXT(".txt"))) FinalSavePath += TEXT(".txt");
	if (FFileHelper::SaveStringToFile(TxtContent, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine)
		{
			const FString MsgPrefix = AOrbitalManager::TrString(TEXT("Raport TXT Salvat: "), TEXT("TXT Report Saved: "));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, MsgPrefix + FinalSavePath);
		}
	}
}*/
void UEclipseBrowserWidget::OnExportTxtClicked()
{
	if (CachedCityHits.Num() == 0) return;

	const bool bLunar = bLastCityQueryWasLunar;
	FString TxtContent;
	FString DefaultName;
	const TCHAR* BigLine = TEXT("====================================================================================\n");
	const TCHAR* SmallLine = TEXT("------------------------------------------------------------------------------------\n");

	TxtContent += BigLine;

	if (bLunar)
	{
		const FLunarEclipseElements& LE = CurrentDetailLunarEclipse;
		const FString DateStr = FString::Printf(TEXT("%02d.%02d.%d"), LE.Day, LE.Month, LE.Year);
		const FString TypeStr = GetLunarTypeString(LE.TypeNumber);
		const FString MagStr = (LE.TypeNumber >= 3)
			? FString::Printf(TEXT("Umag: %.4f"), LE.Umag)
			: FString::Printf(TEXT("Pmag: %.4f"), LE.Pmag);

		DefaultName = FString::Printf(TEXT("LunarEclipse_%d_%02d_%02d_Report.txt"), LE.Year, LE.Month, LE.Day);

		TxtContent += FString::Printf(TEXT("%s: %s\n"),
			*AOrbitalManager::TrString(TEXT("RAPORT DETALII ECLIPSA DE LUNA"), TEXT("LUNAR ECLIPSE DETAILS REPORT")),
			*DateStr);
		TxtContent += FString::Printf(TEXT("%s: %s | %s: %d\n"),
			*AOrbitalManager::TrString(TEXT("TIP"), TEXT("TYPE")), *TypeStr,
			*AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")), LE.Saros);
		TxtContent += FString::Printf(TEXT("%s: %s\n"),
			*AOrbitalManager::TrString(TEXT("MAGNITUDINE"), TEXT("MAGNITUDE")), *MagStr);
		TxtContent += AOrbitalManager::TrString(
			TEXT("Eclipsa de Luna este vizibila de pe intreaga emisfera nocturna (oriunde Luna este deasupra orizontului).\n"),
			TEXT("A lunar eclipse is visible from the entire night hemisphere (anywhere the Moon is above the horizon).\n"));
		TxtContent += BigLine;
		TxtContent += TEXT("\n");

		TxtContent += AOrbitalManager::TrString(
			TEXT("Oras                       | Tara                 | Lat      | Lon      | Ora Max (UTC)\n"),
			TEXT("City                       | Country              | Lat      | Lon      | Max Time (UTC)\n"));
		TxtContent += SmallLine;

		for (const FEclipseCityHit& Hit : CachedCityHits)
		{
			TxtContent += FString::Printf(TEXT("%-26s | %-20s | %+08.3f | %+08.3f | %s\n"),
				*Hit.CityName.Left(26), *Hit.Country.Left(20), Hit.Latitude, Hit.Longitude,
				*FormatUtcHourString(Hit.UtcHour));
		}
	}
	else
	{
		DefaultName = FString::Printf(TEXT("Eclipse_%d_%02d_%02d_Report.txt"),
			CurrentDetailEclipse.RealYear, CurrentDetailEclipse.Month, CurrentDetailEclipse.Day);

		TxtContent += FString::Printf(TEXT("%s: %s\n"),
			*AOrbitalManager::TrString(TEXT("RAPORT DETALII ECLIPSA"), TEXT("ECLIPSE DETAILS REPORT")),
			*CurrentDetailEclipse.Name);
		TxtContent += FString::Printf(TEXT("%s: %s | %s: %d\n"),
			*AOrbitalManager::TrString(TEXT("DATA"), TEXT("DATE")), *CurrentDetailEclipse.FormattedDateStr,
			*AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")), CurrentDetailEclipse.SarosSeries);
		TxtContent += BigLine;
		TxtContent += TEXT("\n");

		TxtContent += AOrbitalManager::TrString(
			TEXT("Oras                       | Tara                 | Lat      | Lon      | Mag    | Acoper.| Ora UTC  | Tip\n"),
			TEXT("City                       | Country              | Lat      | Lon      | Mag    | Cover  | UTC Time | Type\n"));
		TxtContent += SmallLine;

		for (const FEclipseCityHit& Hit : CachedCityHits)
		{
			FString TypeStr = AOrbitalManager::TrString(TEXT("PARTIALA"), TEXT("PARTIAL"));
			if (Hit.Magnitude >= 1.0f)
			{
				TypeStr = (CurrentDetailEclipse.Type == EEclipseType::Annular)
					? AOrbitalManager::TrString(TEXT("INELARA"), TEXT("ANNULAR"))
					: AOrbitalManager::TrString(TEXT("TOTALA"), TEXT("TOTAL"));
			}
			else if (Hit.Magnitude <= 0.0f)
			{
				TypeStr = AOrbitalManager::TrString(TEXT("NICIUNA"), TEXT("NONE"));
			}
			TxtContent += FString::Printf(TEXT("%-26s | %-20s | %+08.3f | %+08.3f | %.4f | %.3f%% | %s | %s\n"),
				*Hit.CityName.Left(26), *Hit.Country.Left(20), Hit.Latitude, Hit.Longitude,
				Hit.Magnitude, Hit.Magnitude * 100.f, *FormatUtcHourString(Hit.UtcHour), *TypeStr);
		}
	}

	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[1024] = { 0 };
	TCHAR szFilter[1024] = { 0 };
	FCString::Strncpy(szFile, *DefaultName, 1024);
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = 1024;
	const FString FilterLabel = AOrbitalManager::TrString(TEXT("Fisier Text (*.txt)|*.txt|Toate Fisierele (*.*)|*.*|"), TEXT("Text File (*.txt)|*.txt|All Files (*.*)|*.*|"));
	FCString::Strncpy(szFilter, *FilterLabel, 1024);
	for (int32 i = 0; i < 1024 && szFilter[i] != TEXT('\0'); ++i)
	{
		if (szFilter[i] == TEXT('|')) szFilter[i] = TEXT('\0');
	}
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("txt");
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty()) return;
	if (!FinalSavePath.EndsWith(TEXT(".txt"))) FinalSavePath += TEXT(".txt");

	if (FFileHelper::SaveStringToFile(TxtContent, *FinalSavePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		if (GEngine)
		{
			const FString MsgPrefix = AOrbitalManager::TrString(TEXT("Raport TXT Salvat: "), TEXT("TXT Report Saved: "));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, MsgPrefix + FinalSavePath);
		}
	}
}
void UEclipseBrowserWidget::OnDetailBackClicked()
{
	if (RootSwitcher) RootSwitcher->SetActiveWidgetIndex(0);
}
void UEclipseBrowserWidget::ShowEclipseDetails(const FEclipseData& Data)
{
	CurrentDetailEclipse = Data;
	if (!DetailContentBox) return;
	DetailContentBox->ClearChildren();
	const double y = (double)Data.RealYear + ((double)Data.Month - 0.5) / 12.0;
	double ExpectedDeltaT = 69.184;
	if (y < -500.0)
	{
		const double u = (y - 1820.0) / 100.0;
		ExpectedDeltaT = -20.0 + 32.0 * u * u;
	}
	else if (y < 500.0)
	{
		const double u = y / 100.0;
		ExpectedDeltaT = 10583.6 - 1014.41 * u + 33.78311 * FMath::Pow(u, 2) - 5.952053 * FMath::Pow(u, 3)
			- 0.1798452 * FMath::Pow(u, 4) + 0.022174192 * FMath::Pow(u, 5) + 0.0090316521 * FMath::Pow(u, 6);
	}
	else if (y < 1600.0)
	{
		const double u = (y - 1000.0) / 100.0;
		ExpectedDeltaT = 1574.2 - 556.01 * u + 71.23472 * FMath::Pow(u, 2) + 0.319781 * FMath::Pow(u, 3)
			- 0.8503463 * FMath::Pow(u, 4) - 0.005050998 * FMath::Pow(u, 5) + 0.0083572073 * FMath::Pow(u, 6);
	}
	else if (y < 2050.0)
	{
		const double t = y - 2000.0;
		ExpectedDeltaT = 62.92 + 0.32217 * t + 0.005589 * t * t;
	}
	else
	{
		const double u = (y - 1820.0) / 100.0;
		ExpectedDeltaT = -20.0 + 32.0 * u * u;
	}
	const double MinDeltaT = FMath::Min(ExpectedDeltaT * 0.85, ExpectedDeltaT - 25.0);
	const double MaxDeltaT = FMath::Max(ExpectedDeltaT * 1.15, ExpectedDeltaT + 25.0);
	UTextBlock* LoadingTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LoadingTxt->SetText(AOrbitalManager::Tr(TEXT("Se sincronizează datele astronomice..."), TEXT("Synchronizing astronomical data...")));
	LoadingTxt->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 16)));
	LoadingTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.8f, 1.0f)));
	DetailContentBox->AddChildToVerticalBox(LoadingTxt)->SetPadding(FMargin(20.f));
	if (RootSwitcher) RootSwitcher->SetActiveWidgetIndex(1);
	RequestJumpToTime(Data.Day, Data.Month, Data.RealYear, Data.Hour, false);
	UWorld* World = GetWorld();
	if (!World) return;
	TWeakObjectPtr<UEclipseBrowserWidget> WeakThis(this);
	TSharedPtr<int32> PollAttempts = MakeShared<int32>(0);
	TSharedPtr<FTimerHandle> PollTimerHandle = MakeShared<FTimerHandle>();
	World->GetTimerManager().SetTimer(*PollTimerHandle, [WeakThis, PollAttempts, PollTimerHandle, ExpectedDeltaT, MinDeltaT, MaxDeltaT]()
		{
			if (!WeakThis.IsValid()) return;
			UEclipseBrowserWidget* Self = WeakThis.Get();
			UWorld* CurrentWorld = Self->GetWorld();
			if (!CurrentWorld) return;
			(*PollAttempts)++;
			AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(CurrentWorld, AOrbitalManager::StaticClass()));
			bool bDataReady = false;
			if (Manager)
			{
				bDataReady = (Manager->Deltatseconds >= MinDeltaT && Manager->Deltatseconds <= MaxDeltaT);
			}
			if (bDataReady || *PollAttempts >= 125)
			{
				CurrentWorld->GetTimerManager().ClearTimer(*PollTimerHandle);
				if (!Self->DetailContentBox || !Self->WidgetTree) return;
				const FEclipseData& EclipseData = Self->CurrentDetailEclipse;
				Self->DetailContentBox->ClearChildren();
				double ResolvedDeltaT = ExpectedDeltaT;
				if (Manager)
				{
					if (bDataReady)
					{
						ResolvedDeltaT = Manager->Deltatseconds;
					}
					else
					{
						Manager->Deltatseconds = ExpectedDeltaT;
						ResolvedDeltaT = ExpectedDeltaT;
					}
				}
				Self->PopulateCityVisibility(EclipseData);
				auto MakeFont = [](int32 Size, bool bBold = false) -> FSlateFontInfo
					{
						return FSlateFontInfo(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
					};
				UHorizontalBox* TitleRow = Self->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				Self->DetailContentBox->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
				FString DisplayTitle = EclipseData.Name;
				const FString SolarWord = AOrbitalManager::TrString(TEXT("Solara"), TEXT("Solar"));
				static const TArray<FString> TypeWords = {
					TEXT("TOTALA"), TEXT("TOTALĂ"), TEXT("TOTAL"),
					TEXT("PARTIALA"), TEXT("PARȚIALĂ"), TEXT("PARTIAL"),
					TEXT("INELARA"), TEXT("INELARĂ"), TEXT("ANNULAR"),
					TEXT("HIBRIDA"), TEXT("HIBRIDĂ"), TEXT("HYBRID")
				};
				for (const FString& Word : TypeWords)
				{
					DisplayTitle = DisplayTitle.Replace(*Word, *SolarWord, ESearchCase::IgnoreCase);
				}
				UTextBlock* NameTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				NameTxt->SetText(FText::FromString(DisplayTitle));
				NameTxt->SetFont(MakeFont(24, true));
				TitleRow->AddChildToHorizontalBox(NameTxt)->SetVerticalAlignment(VAlign_Center);
				UBorder* TypeBadge = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				TypeBadge->SetBrushColor(Self->GetTypeColor(EclipseData.Type));
				TypeBadge->SetPadding(FMargin(10.f, 4.f));
				UTextBlock* TypeBadgeTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				TypeBadgeTxt->SetText(FText::FromString(Self->GetTypeString(EclipseData.Type)));
				TypeBadgeTxt->SetFont(MakeFont(13, true));
				TypeBadgeTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
				TypeBadge->SetContent(TypeBadgeTxt);
				UHorizontalBoxSlot* BadgeSlot = TitleRow->AddChildToHorizontalBox(TypeBadge);
				BadgeSlot->SetVerticalAlignment(VAlign_Center);
				BadgeSlot->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
				UWrapBox* StatsWrap = Self->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
				StatsWrap->SetInnerSlotPadding(FVector2D(10.f, 10.f));
				Self->DetailContentBox->AddChildToVerticalBox(StatsWrap);
				auto AddStatCard = [&](const FString& Label, const FString& Value, FLinearColor AccentColor, float WidthOverride = 175.f, int32 ValueFontSize = 18)
					{
						UBorder* Card = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Card->SetBrushColor(FLinearColor(0.13f, 0.13f, 0.15f, 1.f));
						Card->SetPadding(FMargin(14.f, 10.f));
						UVerticalBox* CardBox = Self->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
						Card->SetContent(CardBox);
						UBorder* Accent = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Accent->SetBrushColor(AccentColor);
						USizeBox* AccentSize = Self->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
						AccentSize->SetHeightOverride(3.f);
						AccentSize->SetContent(Accent);
						CardBox->AddChildToVerticalBox(AccentSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
						UTextBlock* LabelTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						LabelTxt->SetText(FText::FromString(Label));
						LabelTxt->SetFont(MakeFont(11));
						LabelTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
						CardBox->AddChildToVerticalBox(LabelTxt);
						UTextBlock* ValueTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						ValueTxt->SetText(FText::FromString(Value));
						ValueTxt->SetFont(MakeFont(ValueFontSize, true));
						CardBox->AddChildToVerticalBox(ValueTxt);
						USizeBox* CardSize = Self->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
						CardSize->SetWidthOverride(WidthOverride);
						CardSize->SetContent(Card);
						StatsWrap->AddChildToWrapBox(CardSize);
					};
				float MaxMagnitude = EclipseData.Obscuration;
				if (Self->CachedCityHits.Num() > 0 && Self->CachedCityHits[0].Magnitude > MaxMagnitude)
				{
					MaxMagnitude = Self->CachedCityHits[0].Magnitude;
				}
				double GammaValue = 0.0;
				if (Manager && Manager->AllEclipses.IsValidIndex(EclipseData.SourceIndex))
				{
					GammaValue = Self->ComputeGamma(Manager->AllEclipses[EclipseData.SourceIndex]);
				}
				const int32 DateFontSize = (EclipseData.FormattedDateStr.Len() > 13) ? 16 : 18;
				AddStatCard(AOrbitalManager::TrString(TEXT("DATA"), TEXT("DATE")), EclipseData.FormattedDateStr, FLinearColor(0.3f, 0.6f, 1.f), 230.f, DateFontSize);
				AddStatCard(AOrbitalManager::TrString(TEXT("ACOPERIRE MAXIMA"), TEXT("MAX COVERAGE")),
					FString::Printf(TEXT("%.1f%%"), MaxMagnitude * 100.f), Self->GetTypeColor(EclipseData.Type), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")),
					FString::FromInt(EclipseData.SarosSeries), FLinearColor(0.8f, 0.3f, 1.f), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("FACTOR GAMMA"), TEXT("GAMMA FACTOR")),
					FString::Printf(TEXT("%+.4f"), GammaValue), FLinearColor(1.f, 0.8f, 0.2f), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("LATITUDINE MAXIM"), TEXT("MAX LATITUDE")),
					FString::Printf(TEXT("%.3f°"), EclipseData.Latitude), FLinearColor(0.4f, 0.9f, 0.6f), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("LONGITUDINE MAXIM"), TEXT("MAX LONGITUDE")),
					FString::Printf(TEXT("%.3f°"), EclipseData.Longitude), FLinearColor(0.4f, 0.9f, 0.6f), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("DELTA T"), TEXT("DELTA T")),
					FString::Printf(TEXT("%.2f s"), ResolvedDeltaT), FLinearColor(0.2f, 0.9f, 0.9f), 170.f, 18);
				Self->RootSwitcher->SetActiveWidgetIndex(1);
			}
		}, 0.04f, true);
}
/*void UEclipseBrowserWidget::ShowLunarEclipseDetails(const FLunarEclipseElements& E)
{
	CurrentDetailLunarEclipse = E;
	if (!DetailContentBox || !WidgetTree) return;
	DetailContentBox->ClearChildren();
	auto MakeFont = [](int32 Size, bool bBold = false) -> FSlateFontInfo
		{
			return FSlateFontInfo(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
		};
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DetailContentBox->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	FString DisplayTitle = AOrbitalManager::TrString(
		FString::Printf(TEXT("Eclipsa de Luna %02d.%02d.%d"), E.Day, E.Month, E.Year),
		FString::Printf(TEXT("Lunar Eclipse %02d.%02d.%d"), E.Day, E.Month, E.Year)
	);
	UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NameTxt->SetText(FText::FromString(DisplayTitle));
	NameTxt->SetFont(MakeFont(24, true));
	TitleRow->AddChildToHorizontalBox(NameTxt)->SetVerticalAlignment(VAlign_Center);
	UBorder* TypeBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TypeBadge->SetBrushColor(GetLunarTypeColor(E.TypeNumber));
	TypeBadge->SetPadding(FMargin(10.f, 4.f));
	UTextBlock* TypeBadgeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TypeBadgeTxt->SetText(FText::FromString(GetLunarTypeString(E.TypeNumber)));
	TypeBadgeTxt->SetFont(MakeFont(13, true));
	TypeBadgeTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	TypeBadge->SetContent(TypeBadgeTxt);
	UHorizontalBoxSlot* BadgeSlot = TitleRow->AddChildToHorizontalBox(TypeBadge);
	BadgeSlot->SetVerticalAlignment(VAlign_Center);
	BadgeSlot->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
	UWrapBox* StatsWrap = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	StatsWrap->SetInnerSlotPadding(FVector2D(10.f, 10.f));
	DetailContentBox->AddChildToVerticalBox(StatsWrap);
	auto AddStatCard = [&](const FString& Label, const FString& Value, FLinearColor AccentColor, float WidthOverride = 175.f, int32 ValueFontSize = 18)
		{
			UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Card->SetBrushColor(FLinearColor(0.13f, 0.13f, 0.15f, 1.f));
			Card->SetPadding(FMargin(14.f, 10.f));
			UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Card->SetContent(CardBox);
			UBorder* Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Accent->SetBrushColor(AccentColor);
			USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			AccentSize->SetHeightOverride(3.f);
			AccentSize->SetContent(Accent);
			CardBox->AddChildToVerticalBox(AccentSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			UTextBlock* LabelTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			LabelTxt->SetText(FText::FromString(Label));
			LabelTxt->SetFont(MakeFont(11));
			LabelTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
			CardBox->AddChildToVerticalBox(LabelTxt);
			UTextBlock* ValueTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			ValueTxt->SetText(FText::FromString(Value));
			ValueTxt->SetFont(MakeFont(ValueFontSize, true));
			CardBox->AddChildToVerticalBox(ValueTxt);
			USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			CardSize->SetWidthOverride(WidthOverride);
			CardSize->SetContent(Card);
			StatsWrap->AddChildToWrapBox(CardSize);
		};
	AddStatCard(AOrbitalManager::TrString(TEXT("DATA"), TEXT("DATE")),
		FString::Printf(TEXT("%02d.%02d.%d"), E.Day, E.Month, E.Year), FLinearColor(0.3f, 0.6f, 1.f), 200.f, 18);
	AddStatCard(AOrbitalManager::TrString(TEXT("TIP ECLIPSA"), TEXT("ECLIPSE TYPE")),
		GetLunarTypeString(E.TypeNumber), GetLunarTypeColor(E.TypeNumber), 220.f, 16);
	FString MagLabel = (E.TypeNumber >= 3)
		? FString::Printf(TEXT("Umag: %.4f"), E.Umag)
		: FString::Printf(TEXT("Pmag: %.4f"), E.Pmag);
	AddStatCard(AOrbitalManager::TrString(TEXT("MAGNITUDINE"), TEXT("MAGNITUDE")),
		MagLabel, FLinearColor(0.3f, 0.85f, 1.f), 170.f, 18);
	AddStatCard(AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")),
		FString::FromInt(E.Saros), FLinearColor(0.8f, 0.3f, 1.f), 150.f, 18);
	AddStatCard(AOrbitalManager::TrString(TEXT("LATITUDINE MAXIM (GE)"), TEXT("MAX LATITUDE (GE)")),
		FString::Printf(TEXT("%.3f°"), E.Lat_GE), FLinearColor(0.4f, 0.9f, 0.6f), 190.f, 18);
	float DisplayLon = -(float)E.eLngW_GE;
	while (DisplayLon > 180.0f) DisplayLon -= 360.0f;
	while (DisplayLon < -180.0f) DisplayLon += 360.0f;
	AddStatCard(AOrbitalManager::TrString(TEXT("LONGITUDINE MAXIM (GE)"), TEXT("MAX LONGITUDE (GE)")),
		FString::Printf(TEXT("%.3f°"), DisplayLon), FLinearColor(0.4f, 0.9f, 0.6f), 190.f, 18);
	UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Sep->SetBrushColor(FLinearColor(0.22f, 0.22f, 0.24f));
	USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SepSize->SetHeightOverride(1.f);
	SepSize->SetContent(Sep);
	DetailContentBox->AddChildToVerticalBox(SepSize)->SetPadding(FMargin(0.f, 15.f, 0.f, 10.f));
	UTextBlock* VisibilityNote = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	VisibilityNote->SetText(AOrbitalManager::Tr(
		TEXT("O eclipsa de Luna este vizibila simultan de pe intreaga emisfera nocturna a Pamantului (oriunde Luna este deasupra orizontului), spre deosebire de eclipsele de Soare, vizibile doar pe o banda ingusta."),
		TEXT("A lunar eclipse is visible simultaneously from the entire night hemisphere of Earth (anywhere the Moon is above the horizon), unlike solar eclipses, visible only along a narrow band.")));
	VisibilityNote->SetAutoWrapText(true);
	VisibilityNote->SetFont(MakeFont(12));
	VisibilityNote->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
	DetailContentBox->AddChildToVerticalBox(VisibilityNote);
	PopulateLunarCityVisibility(E);
	if (RootSwitcher) RootSwitcher->SetActiveWidgetIndex(1);
}*/

void UEclipseBrowserWidget::ShowLunarEclipseDetails(const FLunarEclipseElements& E)
{
	CurrentDetailLunarEclipse = E;
	if (!DetailContentBox) return;
	DetailContentBox->ClearChildren();

	// 1. Calculăm Delta T estimat pe baza Anului și Lunii eclipsei de lună
	const double y = (double)E.Year + ((double)E.Month - 0.5) / 12.0;
	double ExpectedDeltaT = 69.184;
	if (y < -500.0)
	{
		const double u = (y - 1820.0) / 100.0;
		ExpectedDeltaT = -20.0 + 32.0 * u * u;
	}
	else if (y < 500.0)
	{
		const double u = y / 100.0;
		ExpectedDeltaT = 10583.6 - 1014.41 * u + 33.78311 * FMath::Pow(u, 2) - 5.952053 * FMath::Pow(u, 3)
			- 0.1798452 * FMath::Pow(u, 4) + 0.022174192 * FMath::Pow(u, 5) + 0.0090316521 * FMath::Pow(u, 6);
	}
	else if (y < 1600.0)
	{
		const double u = (y - 1000.0) / 100.0;
		ExpectedDeltaT = 1574.2 - 556.01 * u + 71.23472 * FMath::Pow(u, 2) + 0.319781 * FMath::Pow(u, 3)
			- 0.8503463 * FMath::Pow(u, 4) - 0.005050998 * FMath::Pow(u, 5) + 0.0083572073 * FMath::Pow(u, 6);
	}
	else if (y < 2050.0)
	{
		const double t = y - 2000.0;
		ExpectedDeltaT = 62.92 + 0.32217 * t + 0.005589 * t * t;
	}
	else
	{
		const double u = (y - 1820.0) / 100.0;
		ExpectedDeltaT = -20.0 + 32.0 * u * u;
	}

	const double MinDeltaT = FMath::Min(ExpectedDeltaT * 0.85, ExpectedDeltaT - 25.0);
	const double MaxDeltaT = FMath::Max(ExpectedDeltaT * 1.15, ExpectedDeltaT + 25.0);

	// 2. Afișăm un mesaj de încărcare temporar în interfață
	UTextBlock* LoadingTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LoadingTxt->SetText(AOrbitalManager::Tr(TEXT("Se sincronizează datele astronomice..."), TEXT("Synchronizing astronomical data...")));
	LoadingTxt->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFontStyle("Bold", 16)));
	LoadingTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.8f, 1.0f)));
	DetailContentBox->AddChildToVerticalBox(LoadingTxt)->SetPadding(FMargin(20.f));

	if (RootSwitcher) RootSwitcher->SetActiveWidgetIndex(1);

	// 3. Extragem ora zecimală din Julian Date-ul complet (reținut în E.T0)
	double FracDay = FMath::Fmod(E.T0 + 0.5, 1.0);
	if (FracDay < 0.0) FracDay += 1.0;
	float EclipseHour = (float)(FracDay * 24.0);

	// 4. Setăm timpul în simulare și trimitem cererea către serverul Python
	RequestJumpToTime(E.Day, E.Month, E.Year, EclipseHour, false);

	// 5. Pornim cronometrul de verificare (polling) pentru a asigura sincronizarea cu Delta T
	UWorld* World = GetWorld();
	if (!World) return;
	TWeakObjectPtr<UEclipseBrowserWidget> WeakThis(this);
	TSharedPtr<int32> PollAttempts = MakeShared<int32>(0);
	TSharedPtr<FTimerHandle> PollTimerHandle = MakeShared<FTimerHandle>();

	World->GetTimerManager().SetTimer(*PollTimerHandle, [WeakThis, PollAttempts, PollTimerHandle, ExpectedDeltaT, MinDeltaT, MaxDeltaT]()
		{
			if (!WeakThis.IsValid()) return;
			UEclipseBrowserWidget* Self = WeakThis.Get();
			UWorld* CurrentWorld = Self->GetWorld();
			if (!CurrentWorld) return;

			(*PollAttempts)++;
			AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(CurrentWorld, AOrbitalManager::StaticClass()));
			bool bDataReady = false;
			if (Manager)
			{
				bDataReady = (Manager->Deltatseconds >= MinDeltaT && Manager->Deltatseconds <= MaxDeltaT);
			}

			if (bDataReady || *PollAttempts >= 125)
			{
				CurrentWorld->GetTimerManager().ClearTimer(*PollTimerHandle);
				if (!Self->DetailContentBox || !Self->WidgetTree) return;

				const FLunarEclipseElements& EclipseData = Self->CurrentDetailLunarEclipse;
				Self->DetailContentBox->ClearChildren();

				double ResolvedDeltaT = ExpectedDeltaT;
				if (Manager)
				{
					if (bDataReady)
					{
						ResolvedDeltaT = Manager->Deltatseconds;
					}
					else
					{
						Manager->Deltatseconds = ExpectedDeltaT;
						ResolvedDeltaT = ExpectedDeltaT;
					}
				}

				// Solicităm vizibilitatea pe orașe pentru eclipsa de lună
				Self->PopulateLunarCityVisibility(EclipseData);

				auto MakeFont = [](int32 Size, bool bBold = false) -> FSlateFontInfo
					{
						return FSlateFontInfo(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
					};

				// Construirea UI-ului cu detaliile eclipsei
				UHorizontalBox* TitleRow = Self->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
				Self->DetailContentBox->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));

				FString DisplayTitle = AOrbitalManager::TrString(
					FString::Printf(TEXT("Eclipsa de Luna %02d.%02d.%d"), EclipseData.Day, EclipseData.Month, EclipseData.Year),
					FString::Printf(TEXT("Lunar Eclipse %02d.%02d.%d"), EclipseData.Day, EclipseData.Month, EclipseData.Year)
				);

				UTextBlock* NameTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				NameTxt->SetText(FText::FromString(DisplayTitle));
				NameTxt->SetFont(MakeFont(24, true));
				TitleRow->AddChildToHorizontalBox(NameTxt)->SetVerticalAlignment(VAlign_Center);

				UBorder* TypeBadge = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				TypeBadge->SetBrushColor(Self->GetLunarTypeColor(EclipseData.TypeNumber));
				TypeBadge->SetPadding(FMargin(10.f, 4.f));

				UTextBlock* TypeBadgeTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				TypeBadgeTxt->SetText(FText::FromString(Self->GetLunarTypeString(EclipseData.TypeNumber)));
				TypeBadgeTxt->SetFont(MakeFont(13, true));
				TypeBadgeTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
				TypeBadge->SetContent(TypeBadgeTxt);

				UHorizontalBoxSlot* BadgeSlot = TitleRow->AddChildToHorizontalBox(TypeBadge);
				BadgeSlot->SetVerticalAlignment(VAlign_Center);
				BadgeSlot->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));

				UWrapBox* StatsWrap = Self->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
				StatsWrap->SetInnerSlotPadding(FVector2D(10.f, 10.f));
				Self->DetailContentBox->AddChildToVerticalBox(StatsWrap);

				auto AddStatCard = [&](const FString& Label, const FString& Value, FLinearColor AccentColor, float WidthOverride = 175.f, int32 ValueFontSize = 18)
					{
						UBorder* Card = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Card->SetBrushColor(FLinearColor(0.13f, 0.13f, 0.15f, 1.f));
						Card->SetPadding(FMargin(14.f, 10.f));
						UVerticalBox* CardBox = Self->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
						Card->SetContent(CardBox);

						UBorder* Accent = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
						Accent->SetBrushColor(AccentColor);
						USizeBox* AccentSize = Self->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
						AccentSize->SetHeightOverride(3.f);
						AccentSize->SetContent(Accent);
						CardBox->AddChildToVerticalBox(AccentSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

						UTextBlock* LabelTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						LabelTxt->SetText(FText::FromString(Label));
						LabelTxt->SetFont(MakeFont(11));
						LabelTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)));
						CardBox->AddChildToVerticalBox(LabelTxt);

						UTextBlock* ValueTxt = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
						ValueTxt->SetText(FText::FromString(Value));
						ValueTxt->SetFont(MakeFont(ValueFontSize, true));
						CardBox->AddChildToVerticalBox(ValueTxt);

						USizeBox* CardSize = Self->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
						CardSize->SetWidthOverride(WidthOverride);
						CardSize->SetContent(Card);
						StatsWrap->AddChildToWrapBox(CardSize);
					};

				AddStatCard(AOrbitalManager::TrString(TEXT("DATA"), TEXT("DATE")),
					FString::Printf(TEXT("%02d.%02d.%d"), EclipseData.Day, EclipseData.Month, EclipseData.Year), FLinearColor(0.3f, 0.6f, 1.f), 200.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("TIP ECLIPSA"), TEXT("ECLIPSE TYPE")),
					Self->GetLunarTypeString(EclipseData.TypeNumber), Self->GetLunarTypeColor(EclipseData.TypeNumber), 220.f, 16);

				FString MagLabel = (EclipseData.TypeNumber >= 3)
					? FString::Printf(TEXT("Umag: %.4f"), EclipseData.Umag)
					: FString::Printf(TEXT("Pmag: %.4f"), EclipseData.Pmag);
				AddStatCard(AOrbitalManager::TrString(TEXT("MAGNITUDINE"), TEXT("MAGNITUDE")),
					MagLabel, FLinearColor(0.3f, 0.85f, 1.f), 170.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("CICLU SAROS"), TEXT("SAROS CYCLE")),
					FString::FromInt(EclipseData.Saros), FLinearColor(0.8f, 0.3f, 1.f), 150.f, 18);
				AddStatCard(AOrbitalManager::TrString(TEXT("LATITUDINE MAXIM (GE)"), TEXT("MAX LATITUDE (GE)")),
					FString::Printf(TEXT("%.3f°"), EclipseData.Lat_GE), FLinearColor(0.4f, 0.9f, 0.6f), 190.f, 18);

				float DisplayLon = -(float)EclipseData.eLngW_GE;
				while (DisplayLon > 180.0f) DisplayLon -= 360.0f;
				while (DisplayLon < -180.0f) DisplayLon += 360.0f;
				AddStatCard(AOrbitalManager::TrString(TEXT("LONGITUDINE MAXIM (GE)"), TEXT("MAX LONGITUDE (GE)")),
					FString::Printf(TEXT("%.3f°"), DisplayLon), FLinearColor(0.4f, 0.9f, 0.6f), 190.f, 18);

				AddStatCard(AOrbitalManager::TrString(TEXT("DELTA T"), TEXT("DELTA T")),
					FString::Printf(TEXT("%.2f s"), ResolvedDeltaT), FLinearColor(0.2f, 0.9f, 0.9f), 170.f, 18);

				UBorder* Sep = Self->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				Sep->SetBrushColor(FLinearColor(0.22f, 0.22f, 0.24f));
				USizeBox* SepSize = Self->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
				SepSize->SetHeightOverride(1.f);
				SepSize->SetContent(Sep);
				Self->DetailContentBox->AddChildToVerticalBox(SepSize)->SetPadding(FMargin(0.f, 15.f, 0.f, 10.f));

				UTextBlock* VisibilityNote = Self->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				VisibilityNote->SetText(AOrbitalManager::Tr(
					TEXT("O eclipsa de Luna este vizibila simultan de pe intreaga emisfera nocturna a Pamantului (oriunde Luna este deasupra orizontului), spre deosebire de eclipsele de Soare, vizibile doar pe o banda ingusta."),
					TEXT("A lunar eclipse is visible simultaneously from the entire night hemisphere of Earth (anywhere the Moon is above the horizon), unlike solar eclipses, visible only along a narrow band.")));
				VisibilityNote->SetAutoWrapText(true);
				VisibilityNote->SetFont(MakeFont(12));
				VisibilityNote->SetColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.65f, 0.65f)));
				Self->DetailContentBox->AddChildToVerticalBox(VisibilityNote);

				Self->RootSwitcher->SetActiveWidgetIndex(1);
			}
		}, 0.04f, true);
}
void UEclipseBrowserWidget::OnLocationModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 Idx = LocationModeComboBox->GetSelectedIndex();
	if (Idx == 0)
	{
		LocationMode = ELocationFilterMode::None;
	}
	else if (Idx == 1)
	{
		ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
		ADronaMea* Drona = Cast<ADronaMea>(UGameplayStatics::GetActorOfClass(GetWorld(), ADronaMea::StaticClass()));
		if (!GeoRef || !Drona)
		{
			LocationMode = ELocationFilterMode::None;
			LocationModeComboBox->SetSelectedIndex(0);
			RefreshList();
			return;
		}
		FVector Coords = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Drona->GetActorLocation());
		CurrentDroneLon = Coords.X;
		CurrentDroneLat = Coords.Y;
		CurrentDroneAlt = Coords.Z;
		LocationMode = ELocationFilterMode::Drone;
	}
	else
	{
		LocationMode = ELocationFilterMode::Country;
	}
	RefreshList();
}
namespace
{
	FString StripRomanianDiacritics(FString Text)
	{
		Text = Text.Replace(TEXT("ă"), TEXT("a")).Replace(TEXT("Ă"), TEXT("A"));
		Text = Text.Replace(TEXT("â"), TEXT("a")).Replace(TEXT("Â"), TEXT("A"));
		Text = Text.Replace(TEXT("î"), TEXT("i")).Replace(TEXT("Î"), TEXT("I"));
		Text = Text.Replace(TEXT("ș"), TEXT("s")).Replace(TEXT("Ș"), TEXT("S")).Replace(TEXT("ş"), TEXT("s")).Replace(TEXT("Ş"), TEXT("S"));
		Text = Text.Replace(TEXT("ț"), TEXT("t")).Replace(TEXT("Ț"), TEXT("T")).Replace(TEXT("ţ"), TEXT("t")).Replace(TEXT("Ţ"), TEXT("T"));
		return Text.TrimStartAndEnd();
	}
	FString NormalizeToEnglishCountryName(const FString& RawCountryName)
	{
		FString CleanName = StripRomanianDiacritics(RawCountryName);
		static const TMap<FString, FString> RO_TO_EN_Map = {
			{ TEXT("Albania"), TEXT("Albania") },
			{ TEXT("Andorra"), TEXT("Andorra") },
			{ TEXT("Austria"), TEXT("Austria") },
			{ TEXT("Belarus"), TEXT("Belarus") },
			{ TEXT("Belgia"), TEXT("Belgium") },
			{ TEXT("Bosnia si Hertegovina"), TEXT("Bosnia and Herzegovina") },
			{ TEXT("Bosnia"), TEXT("Bosnia and Herzegovina") },
			{ TEXT("Bulgaria"), TEXT("Bulgaria") },
			{ TEXT("Cehia"), TEXT("Czechia") },
			{ TEXT("Republica Ceha"), TEXT("Czechia") },
			{ TEXT("Cipru"), TEXT("Cyprus") },
			{ TEXT("Croatia"), TEXT("Croatia") },
			{ TEXT("Danemarca"), TEXT("Denmark") },
			{ TEXT("Elvetia"), TEXT("Switzerland") },
			{ TEXT("Estonia"), TEXT("Estonia") },
			{ TEXT("Finlanda"), TEXT("Finland") },
			{ TEXT("Franta"), TEXT("France") },
			{ TEXT("Germania"), TEXT("Germany") },
			{ TEXT("Grecia"), TEXT("Greece") },
			{ TEXT("Irlanda"), TEXT("Ireland") },
			{ TEXT("Islanda"), TEXT("Iceland") },
			{ TEXT("Italia"), TEXT("Italy") },
			{ TEXT("Kosovo"), TEXT("Kosovo") },
			{ TEXT("Letonia"), TEXT("Latvia") },
			{ TEXT("Liechtenstein"), TEXT("Liechtenstein") },
			{ TEXT("Lituania"), TEXT("Lithuania") },
			{ TEXT("Luxemburg"), TEXT("Luxembourg") },
			{ TEXT("Macedonia de Nord"), TEXT("North Macedonia") },
			{ TEXT("Macedonia"), TEXT("North Macedonia") },
			{ TEXT("Malta"), TEXT("Malta") },
			{ TEXT("Marea Britanie"), TEXT("United Kingdom") },
			{ TEXT("Regatul Unit"), TEXT("United Kingdom") },
			{ TEXT("UK"), TEXT("United Kingdom") },
			{ TEXT("Moldova"), TEXT("Moldova") },
			{ TEXT("Republica Moldova"), TEXT("Moldova") },
			{ TEXT("Monaco"), TEXT("Monaco") },
			{ TEXT("Muntenegru"), TEXT("Montenegro") },
			{ TEXT("Norvegia"), TEXT("Norway") },
			{ TEXT("Olanda"), TEXT("Netherlands") },
			{ TEXT("Tarile de Jos"), TEXT("Netherlands") },
			{ TEXT("Polonia"), TEXT("Poland") },
			{ TEXT("Portugalia"), TEXT("Portugal") },
			{ TEXT("Romania"), TEXT("Romania") },
			{ TEXT("Rusia"), TEXT("Russia") },
			{ TEXT("Federatia Rusa"), TEXT("Russia") },
			{ TEXT("San Marino"), TEXT("San Marino") },
			{ TEXT("Serbia"), TEXT("Serbia") },
			{ TEXT("Slovacia"), TEXT("Slovakia") },
			{ TEXT("Slovenia"), TEXT("Slovenia") },
			{ TEXT("Spania"), TEXT("Spain") },
			{ TEXT("Suedia"), TEXT("Sweden") },
			{ TEXT("Ucraina"), TEXT("Ukraine") },
			{ TEXT("Ungaria"), TEXT("Hungary") },
			{ TEXT("Vatican"), TEXT("Vatican City") },
			{ TEXT("Afganistan"), TEXT("Afghanistan") },
			{ TEXT("Arabia Saudita"), TEXT("Saudi Arabia") },
			{ TEXT("Armenia"), TEXT("Armenia") },
			{ TEXT("Azerbaidjan"), TEXT("Azerbaijan") },
			{ TEXT("Bahrain"), TEXT("Bahrain") },
			{ TEXT("Bangladesh"), TEXT("Bangladesh") },
			{ TEXT("Bhutan"), TEXT("Bhutan") },
			{ TEXT("Brunei"), TEXT("Brunei") },
			{ TEXT("Cambodgia"), TEXT("Cambodia") },
			{ TEXT("China"), TEXT("China") },
			{ TEXT("Coreea de Nord"), TEXT("North Korea") },
			{ TEXT("Coreea de Sud"), TEXT("South Korea") },
			{ TEXT("Emiratele Arabe Unite"), TEXT("United Arab Emirates") },
			{ TEXT("EAU"), TEXT("United Arab Emirates") },
			{ TEXT("Filipine"), TEXT("Philippines") },
			{ TEXT("Georgia"), TEXT("Georgia") },
			{ TEXT("India"), TEXT("India") },
			{ TEXT("Indonezia"), TEXT("Indonesia") },
			{ TEXT("Iordania"), TEXT("Jordan") },
			{ TEXT("Irak"), TEXT("Iraq") },
			{ TEXT("Iran"), TEXT("Iran") },
			{ TEXT("Israel"), TEXT("Israel") },
			{ TEXT("Japonia"), TEXT("Japan") },
			{ TEXT("Kazahstan"), TEXT("Kazakhstan") },
			{ TEXT("Kirgistan"), TEXT("Kyrgyzstan") },
			{ TEXT("Kargazstan"), TEXT("Kyrgyzstan") },
			{ TEXT("Kuweit"), TEXT("Kuwait") },
			{ TEXT("Laos"), TEXT("Laos") },
			{ TEXT("Liban"), TEXT("Lebanon") },
			{ TEXT("Malaezia"), TEXT("Malaysia") },
			{ TEXT("Malaysia"), TEXT("Malaysia") },
			{ TEXT("Maldive"), TEXT("Maldives") },
			{ TEXT("Mongolia"), TEXT("Mongolia") },
			{ TEXT("Myanmar"), TEXT("Myanmar") },
			{ TEXT("Birmania"), TEXT("Myanmar") },
			{ TEXT("Nepal"), TEXT("Nepal") },
			{ TEXT("Oman"), TEXT("Oman") },
			{ TEXT("Pakistan"), TEXT("Pakistan") },
			{ TEXT("Palestina"), TEXT("Palestine") },
			{ TEXT("Qatar"), TEXT("Qatar") },
			{ TEXT("Singapore"), TEXT("Singapore") },
			{ TEXT("Siria"), TEXT("Syria") },
			{ TEXT("Sri Lanka"), TEXT("Sri Lanka") },
			{ TEXT("Tadjikistan"), TEXT("Tajikistan") },
			{ TEXT("Taiwan"), TEXT("Taiwan") },
			{ TEXT("Thailanda"), TEXT("Thailand") },
			{ TEXT("Timorul de Est"), TEXT("East Timor") },
			{ TEXT("Turcia"), TEXT("Turkey") },
			{ TEXT("Turkmenistan"), TEXT("Turkmenistan") },
			{ TEXT("Uzbekistan"), TEXT("Uzbekistan") },
			{ TEXT("Vietnam"), TEXT("Vietnam") },
			{ TEXT("Yemen"), TEXT("Yemen") },
			{ TEXT("Africa de Sud"), TEXT("South Africa") },
			{ TEXT("Algeria"), TEXT("Algeria") },
			{ TEXT("Angola"), TEXT("Angola") },
			{ TEXT("Benin"), TEXT("Benin") },
			{ TEXT("Botswana"), TEXT("Botswana") },
			{ TEXT("Burkina Faso"), TEXT("Burkina Faso") },
			{ TEXT("Burundi"), TEXT("Burundi") },
			{ TEXT("Camerun"), TEXT("Cameroon") },
			{ TEXT("Capul Verde"), TEXT("Cape Verde") },
			{ TEXT("Ciad"), TEXT("Chad") },
			{ TEXT("Comore"), TEXT("Comoros") },
			{ TEXT("Coasta de Fildes"), TEXT("Ivory Coast") },
			{ TEXT("Congo"), TEXT("Republic of the Congo") },
			{ TEXT("Republica Congo"), TEXT("Republic of the Congo") },
			{ TEXT("Republica Democrata Congo"), TEXT("Democratic Republic of the Congo") },
			{ TEXT("RDC"), TEXT("Democratic Republic of the Congo") },
			{ TEXT("Djibouti"), TEXT("Djibouti") },
			{ TEXT("Egipt"), TEXT("Egypt") },
			{ TEXT("Eritreea"), TEXT("Eritrea") },
			{ TEXT("Eswatini"), TEXT("Eswatini") },
			{ TEXT("Swaziland"), TEXT("Eswatini") },
			{ TEXT("Etiopia"), TEXT("Ethiopia") },
			{ TEXT("Gabon"), TEXT("Gabon") },
			{ TEXT("Gambia"), TEXT("Gambia") },
			{ TEXT("Ghana"), TEXT("Ghana") },
			{ TEXT("Guineea"), TEXT("Guinea") },
			{ TEXT("Guineea-Bissau"), TEXT("Guinea-Bissau") },
			{ TEXT("Guineea Ecuatoriala"), TEXT("Equatorial Guinea") },
			{ TEXT("Kenya"), TEXT("Kenya") },
			{ TEXT("Lesotho"), TEXT("Lesotho") },
			{ TEXT("Liberia"), TEXT("Liberia") },
			{ TEXT("Libia"), TEXT("Libya") },
			{ TEXT("Madagascar"), TEXT("Madagascar") },
			{ TEXT("Malawi"), TEXT("Malawi") },
			{ TEXT("Mali"), TEXT("Mali") },
			{ TEXT("Maroc"), TEXT("Morocco") },
			{ TEXT("Mauritania"), TEXT("Mauritania") },
			{ TEXT("Mauritius"), TEXT("Mauritius") },
			{ TEXT("Mozambic"), TEXT("Mozambique") },
			{ TEXT("Namibia"), TEXT("Namibia") },
			{ TEXT("Niger"), TEXT("Niger") },
			{ TEXT("Nigeria"), TEXT("Nigeria") },
			{ TEXT("Republica Centrafricana"), TEXT("Central African Republic") },
			{ TEXT("Rwanda"), TEXT("Rwanda") },
			{ TEXT("Sao Tome si Principe"), TEXT("Sao Tome and Principe") },
			{ TEXT("Senegal"), TEXT("Senegal") },
			{ TEXT("Seychelles"), TEXT("Seychelles") },
			{ TEXT("Sierra Leone"), TEXT("Sierra Leone") },
			{ TEXT("Somalia"), TEXT("Somalia") },
			{ TEXT("Sudan"), TEXT("Sudan") },
			{ TEXT("Sudanul de Sud"), TEXT("South Sudan") },
			{ TEXT("Tanzania"), TEXT("Tanzania") },
			{ TEXT("Togo"), TEXT("Togo") },
			{ TEXT("Tunisia"), TEXT("Tunisia") },
			{ TEXT("Uganda"), TEXT("Uganda") },
			{ TEXT("Zambia"), TEXT("Zambia") },
			{ TEXT("Zimbabwe"), TEXT("Zimbabwe") },
			{ TEXT("Antigua si Barbuda"), TEXT("Antigua and Barbuda") },
			{ TEXT("Bahamas"), TEXT("Bahamas") },
			{ TEXT("Barbados"), TEXT("Barbados") },
			{ TEXT("Belize"), TEXT("Belize") },
			{ TEXT("Canada"), TEXT("Canada") },
			{ TEXT("Costa Rica"), TEXT("Costa Rica") },
			{ TEXT("Cuba"), TEXT("Cuba") },
			{ TEXT("Dominica"), TEXT("Dominica") },
			{ TEXT("El Salvador"), TEXT("El Salvador") },
			{ TEXT("Grenada"), TEXT("Grenada") },
			{ TEXT("Guatemala"), TEXT("Guatemala") },
			{ TEXT("Haiti"), TEXT("Haiti") },
			{ TEXT("Honduras"), TEXT("Honduras") },
			{ TEXT("Jamaica"), TEXT("Jamaica") },
			{ TEXT("Mexic"), TEXT("Mexico") },
			{ TEXT("Nicaragua"), TEXT("Nicaragua") },
			{ TEXT("Panama"), TEXT("Panama") },
			{ TEXT("Republica Dominicana"), TEXT("Dominican Republic") },
			{ TEXT("Saint Kitts si Nevis"), TEXT("Saint Kitts and Nevis") },
			{ TEXT("Saint Lucia"), TEXT("Saint Lucia") },
			{ TEXT("Saint Vincent si Grenadine"), TEXT("Saint Vincent and the Grenadines") },
			{ TEXT("Statele Unite"), TEXT("United States") },
			{ TEXT("Statele Unite ale Americii"), TEXT("United States") },
			{ TEXT("SUA"), TEXT("United States") },
			{ TEXT("Trinidad si Tobago"), TEXT("Trinidad and Tobago") },
			{ TEXT("Argentina"), TEXT("Argentina") },
			{ TEXT("Bolivia"), TEXT("Bolivia") },
			{ TEXT("Brazilia"), TEXT("Brazil") },
			{ TEXT("Chile"), TEXT("Chile") },
			{ TEXT("Columbia"), TEXT("Colombia") },
			{ TEXT("Ecuador"), TEXT("Ecuador") },
			{ TEXT("Guyana"), TEXT("Guyana") },
			{ TEXT("Paraguay"), TEXT("Paraguay") },
			{ TEXT("Peru"), TEXT("Peru") },
			{ TEXT("Surinam"), TEXT("Suriname") },
			{ TEXT("Suriname"), TEXT("Suriname") },
			{ TEXT("Uruguay"), TEXT("Uruguay") },
			{ TEXT("Venezuela"), TEXT("Venezuela") },
			{ TEXT("Australia"), TEXT("Australia") },
			{ TEXT("Fiji"), TEXT("Fiji") },
			{ TEXT("Kiribati"), TEXT("Kiribati") },
			{ TEXT("Insulele Marshall"), TEXT("Marshall Islands") },
			{ TEXT("Micronezia"), TEXT("Micronesia") },
			{ TEXT("Nauru"), TEXT("Nauru") },
			{ TEXT("Noua Zeelanda"), TEXT("New Zealand") },
			{ TEXT("Palau"), TEXT("Palau") },
			{ TEXT("Papua Noua Guinee"), TEXT("Papua New Guinea") },
			{ TEXT("Samoa"), TEXT("Samoa") },
			{ TEXT("Insulele Solomon"), TEXT("Solomon Islands") },
			{ TEXT("Tonga"), TEXT("Tonga") },
			{ TEXT("Tuvalu"), TEXT("Tuvalu") },
			{ TEXT("Vanuatu"), TEXT("Vanuatu") },
			{ TEXT("Groenlanda"), TEXT("Greenland") },
			{ TEXT("Puerto Rico"), TEXT("Puerto Rico") },
			{ TEXT("Hong Kong"), TEXT("Hong Kong") },
			{ TEXT("Macao"), TEXT("Macao") },
			{ TEXT("Gibraltar"), TEXT("Gibraltar") },
			{ TEXT("Insulele Feroe"), TEXT("Faroe Islands") }
		};
		if (const FString* FoundEnName = RO_TO_EN_Map.Find(CleanName))
		{
			return *FoundEnName;
		}
		return CleanName;
	}
	FORCEINLINE double PolyEval(const double C[4], double t)
	{
		return C[0] + C[1] * t + C[2] * t * t + C[3] * t * t * t;
	}
	FORCEINLINE bool IsLeapYearGreg(int32 Year)
	{
		return (Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0));
	}
	int32 DaysInMonthGreg(int32 Month, int32 Year)
	{
		static const int32 DaysInMonth[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
		if (Month == 2 && IsLeapYearGreg(Year)) return 29;
		if (Month < 1 || Month > 12) return 30;
		return DaysInMonth[Month - 1];
	}
	void ComputeLocalEventDateTime(int32 BaseDay, int32 BaseMonth, int32 BaseYear, float BaseHour, double OffsetHours,
		int32& OutDay, int32& OutMonth, int32& OutYear, float& OutHour)
	{
		double TotalHour = (double)BaseHour + OffsetHours;
		int32 DayDelta = 0;
		while (TotalHour >= 24.0) { TotalHour -= 24.0; DayDelta++; }
		while (TotalHour < 0.0) { TotalHour += 24.0; DayDelta--; }
		OutDay = BaseDay;
		OutMonth = BaseMonth;
		OutYear = BaseYear;
		for (int32 i = 0; i < FMath::Abs(DayDelta); i++)
		{
			if (DayDelta > 0)
			{
				OutDay++;
				const int32 DaysThisMonth = DaysInMonthGreg(OutMonth, OutYear);
				if (OutDay > DaysThisMonth)
				{
					OutDay = 1;
					OutMonth++;
					if (OutMonth > 12) { OutMonth = 1; OutYear++; }
				}
			}
			else
			{
				OutDay--;
				if (OutDay < 1)
				{
					OutMonth--;
					if (OutMonth < 1) { OutMonth = 12; OutYear--; }
					OutDay = DaysInMonthGreg(OutMonth, OutYear);
				}
			}
		}
		OutHour = (float)TotalHour;
	}
}
FLocalEclipseVisibility UEclipseBrowserWidget::ComputeLocalVisibility(
	const FBesselianElements& E,
	double LatDeg,
	double LonDegEast,
	double HeightMeters,
	double DeltaTSeconds)
{
	FLocalEclipseVisibility Result;
	constexpr double A_EARTH_KM = 6378.137;
	constexpr double B_EARTH_KM = 6356.7523142;
	constexpr double e2 = 1.0 - (B_EARTH_KM * B_EARTH_KM) / (A_EARTH_KM * A_EARTH_KM);
	constexpr double OMEGA_EARTH_SIDEREAL = 7.292115146706979e-5;
	const double DeltaTRad = DeltaTSeconds * OMEGA_EARTH_SIDEREAL;
	constexpr double DELTA_K_MOON = (1737.93 - 1737.4) / A_EARTH_KM;
	const double LatRad = FMath::DegreesToRadians(LatDeg);
	const double LonEastRad = FMath::DegreesToRadians(LonDegEast);
	const double sinLat = FMath::Sin(LatRad);
	const double cosLat = FMath::Cos(LatRad);
	const double C_geo = 1.0 / FMath::Sqrt(1.0 - e2 * sinLat * sinLat);
	const double S_geo = C_geo * (1.0 - e2);
	const double HeightUnits = HeightMeters / (1000.0 * A_EARTH_KM);
	const double RhoCosPhi = (C_geo + HeightUnits) * cosLat;
	const double RhoSinPhi = (S_geo + HeightUnits) * sinLat;
	auto EvalAt = [&](double t, double& OutM, double& OutL1, double& OutL2, bool& bSunUp)
		{
			const double x = PolyEval(E.x, t);
			const double y = PolyEval(E.y, t);
			const double d = PolyEval(E.d, t);
			const double mu = PolyEval(E.mu, t);
			const double l1 = PolyEval(E.l1, t);
			const double l2 = PolyEval(E.l2, t);
			const double tf1 = PolyEval(E.tanf1, t);
			const double tf2 = PolyEval(E.tanf2, t);
			double H = (mu - DeltaTRad) + LonEastRad;
			H = FMath::Fmod(H + PI, 2.0 * PI);
			if (H < 0.0) H += 2.0 * PI;
			H -= PI;
			const double sinH = FMath::Sin(H), cosH = FMath::Cos(H);
			const double sind = FMath::Sin(d), cosd = FMath::Cos(d);
			const double xi = RhoCosPhi * sinH;
			const double eta = RhoSinPhi * cosd - RhoCosPhi * cosH * sind;
			const double zeta = RhoSinPhi * sind + RhoCosPhi * cosH * cosd;
			bSunUp = (zeta > -0.015);
			const double u_ = x - xi;
			const double v_ = y - eta;
			OutM = FMath::Sqrt(u_ * u_ + v_ * v_);
			OutL1 = (l1 - zeta * tf1) + DELTA_K_MOON;
			OutL2 = (l2 - zeta * tf2) - DELTA_K_MOON;
		};
	constexpr int32 CoarseSteps = 61;
	constexpr double HalfRangeHours = 5.0;
	double BestT = 0.0;
	double CoarseMinM = TNumericLimits<double>::Max();
	bool bAnyEclipse = false;
	for (int32 i = 0; i < CoarseSteps; i++)
	{
		const double t = -HalfRangeHours + (2.0 * HalfRangeHours) * i / (CoarseSteps - 1);
		double m, L1, L2;
		bool bSunUp;
		EvalAt(t, m, L1, L2, bSunUp);
		if (!bSunUp || m >= L1) continue;
		bAnyEclipse = true;
		if (m < CoarseMinM)
		{
			CoarseMinM = m;
			BestT = t;
		}
	}
	if (!bAnyEclipse)
	{
		Result.bVisible = false;
		Result.LocalMagnitude = 0.0f;
		Result.bIsTotalOrAnnular = false;
		Result.bIsAnnularLocal = false;
		return Result;
	}
	const double CoarseStepHours = (2.0 * HalfRangeHours) / (CoarseSteps - 1);
	constexpr int32 FineSteps = 241;
	double FinalM = 0.0, FinalL1 = 0.0, FinalL2 = 0.0;
	double FinalT = BestT;
	double FineMinM = TNumericLimits<double>::Max();
	for (int32 i = 0; i < FineSteps; i++)
	{
		const double t = BestT - CoarseStepHours + (2.0 * CoarseStepHours) * i / (FineSteps - 1);
		double m, L1, L2;
		bool bSunUp;
		EvalAt(t, m, L1, L2, bSunUp);
		if (!bSunUp) continue;
		if (m < FineMinM)
		{
			FineMinM = m;
			FinalM = m;
			FinalL1 = L1;
			FinalL2 = L2;
			FinalT = t;
		}
	}
	const double Denom = FinalL1 + FinalL2;
	double FinalMagnitude = 0.0;
	if (Denom > 0.0 && FinalM < FinalL1)
	{
		FinalMagnitude = (FinalL1 - FinalM) / Denom;
	}
	const double AbsL2 = FMath::Abs(FinalL2);
	const bool bInsideCentral = (FinalM < AbsL2);
	const bool bIsAnnular = bInsideCentral && (FinalL2 >= 0.0);
	Result.bVisible = (FinalMagnitude > 0.0);
	Result.bIsTotalOrAnnular = bInsideCentral;
	Result.bIsAnnularLocal = bIsAnnular;
	Result.LocalMagnitude = (float)FinalMagnitude;
	Result.BestTHours = FinalT;
	return Result;
}
double UEclipseBrowserWidget::ComputeGamma(const FBesselianElements& E)
{
	const double x0 = E.x[0], x1 = E.x[1];
	const double y0 = E.y[0], y1 = E.y[1];
	const double n2 = x1 * x1 + y1 * y1;
	if (n2 <= 0.0) return E.y[0];
	const double tm = -(x0 * x1 + y0 * y1) / n2;
	const double xm = PolyEval(E.x, tm);
	const double ym = PolyEval(E.y, tm);
	const double GammaDist = FMath::Sqrt(xm * xm + ym * ym);
	return (ym >= 0.0) ? GammaDist : -GammaDist;
}
FString UEclipseBrowserWidget::ExtractCurrentCountryName() const
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager || Manager->CurrentLocationText.IsEmpty())
	{
		return FString();
	}
	FString ExtractedCountry;
	FString Left, Right;
	if (Manager->CurrentLocationText.Split(TEXT(","), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		ExtractedCountry = Right.TrimStartAndEnd();
	}
	else
	{
		ExtractedCountry = Manager->CurrentLocationText.TrimStartAndEnd();
	}
	return NormalizeToEnglishCountryName(ExtractedCountry);
}
FLocalEclipseVisibility UEclipseBrowserWidget::ComputeCountryVisibility(const FBesselianElements& E, const TArray<FCountryPoint>& Points, double DeltaTSeconds)
{
	FLocalEclipseVisibility Best;
	Best.LocalMagnitude = -TNumericLimits<float>::Max();
	for (const FCountryPoint& P : Points)
	{
		FLocalEclipseVisibility V = ComputeLocalVisibility(E, P.Lat, P.Lon, P.Alt, DeltaTSeconds);
		if (V.bVisible && V.LocalMagnitude > Best.LocalMagnitude)
		{
			Best = V;
		}
	}
	return Best;
}
void UEclipseBrowserWidget::PopulateCityVisibility(const FEclipseData& Data)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager || !Manager->AllEclipses.IsValidIndex(Data.SourceIndex))
	{
		CachedCityHits.Empty();
		RenderCityList(FString());
		return;
	}
	const FBesselianElements& E = Manager->AllEclipses[Data.SourceIndex];
	PopulateCityVisibilityInternal(E.T0, false);
}

static double ComputeJulianDateTT(int32 Year, int32 Month, int32 Day, double Hour)
{
	int32 y = Year, m = Month;
	if (m <= 2) { y -= 1; m += 12; }

	double jd_midnight;
	if (Year <= 1582)
	{
		jd_midnight = FMath::Floor(365.25 * (y + 4716)) + FMath::Floor(30.6001 * (m + 1)) + Day + 1721116.5;
	}
	else
	{
		int32 a = y / 100;
		int32 b = 2 - a + (a / 4);
		jd_midnight = FMath::Floor(365.25 * (y + 4716)) + FMath::Floor(30.6001 * (m + 1)) + Day + b - 1524.5;
	}

	const double jd_ut = jd_midnight + (Hour / 24.0);

	// aceeasi formula Espenak-Meeus ca la ExpectedDeltaT din ShowEclipseDetails / delta_t_seconds din Python
	const double y_frac = (double)Year + ((double)Month - 0.5) / 12.0;
	double dt_sec;
	if (y_frac < -500.0)
	{
		const double u = (y_frac - 1820.0) / 100.0;
		dt_sec = -20.0 + 32.0 * u * u;
	}
	else if (y_frac < 500.0)
	{
		const double u = y_frac / 100.0;
		dt_sec = 10583.6 - 1014.41 * u + 33.78311 * FMath::Pow(u, 2) - 5.952053 * FMath::Pow(u, 3)
			- 0.1798452 * FMath::Pow(u, 4) + 0.022174192 * FMath::Pow(u, 5) + 0.0090316521 * FMath::Pow(u, 6);
	}
	else if (y_frac < 1600.0)
	{
		const double u = (y_frac - 1000.0) / 100.0;
		dt_sec = 1574.2 - 556.01 * u + 71.23472 * FMath::Pow(u, 2) + 0.319781 * FMath::Pow(u, 3)
			- 0.8503463 * FMath::Pow(u, 4) - 0.005050998 * FMath::Pow(u, 5) + 0.0083572073 * FMath::Pow(u, 6);
	}
	else if (y_frac < 2050.0)
	{
		const double t = y_frac - 2000.0;
		dt_sec = 62.92 + 0.32217 * t + 0.005589 * t * t;
	}
	else
	{
		const double u = (y_frac - 1820.0) / 100.0;
		dt_sec = -20.0 + 32.0 * u * u;
	}

	return jd_ut + dt_sec / 86400.0; // Julian Date TT
}
void UEclipseBrowserWidget::PopulateLunarCityVisibility(const FLunarEclipseElements& E)
{
	PopulateCityVisibilityInternal(E.T0, true);
	//const double JD_TT = ComputeJulianDateTT(E.Year, E.Month, E.Day, (double)E.T0);
	//PopulateCityVisibilityInternal(JD_TT, true);
}
void UEclipseBrowserWidget::PopulateCityVisibilityInternal(double TargetJDTT, bool bIsLunar)
{
	CachedCityHits.Empty();
	bLastCityQueryWasLunar = bIsLunar;
	if (CitySearchInput)
	{
		CitySearchInput->SetText(FText::GetEmpty());
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		RenderCityList(FString());
		return;
	}
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(World, AOrbitalManager::StaticClass()));
	if (!Manager)
	{
		RenderCityList(FString());
		return;
	}
	if (Manager->AllCitiesFlat.Num() == 0)
	{
		Manager->LoadCountryPoints();
	}
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		RenderCityList(FString());
		return;
	}
	FSocket* ClientSocket = FUdpSocketBuilder(TEXT("EclipseCityClientSocket"))
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(0)
		.AsNonBlocking()
		.WithReceiveBufferSize(524288)
		.WithSendBufferSize(4096)
		.Build();
	if (!ClientSocket)
	{
		RenderCityList(FString());
		return;
	}
	FIPv4Address ServerIP;
	FIPv4Address::Parse(TEXT("127.0.0.1"), ServerIP);
	TSharedRef<FInternetAddr> TargetAddr = SocketSubsystem->CreateInternetAddr();
	TargetAddr->SetIp(ServerIP.Value);
	TargetAddr->SetPort(5002);
	uint8 Payload[9];
	FMemory::Memcpy(Payload, &TargetJDTT, sizeof(double));
	Payload[8] = bIsLunar ? 1 : 0;
	int32 BytesSent = 0;
	ClientSocket->SendTo(Payload, sizeof(Payload), BytesSent, *TargetAddr);
	TArray<uint8> RecvBuffer;
	RecvBuffer.SetNumUninitialized(16384);
	TSharedRef<FInternetAddr> SenderAddr = SocketSubsystem->CreateInternetAddr();
	const double StartTime = FPlatformTime::Seconds();
	bool bFinishedReceiving = false;
	while ((FPlatformTime::Seconds() - StartTime) < 1.5 && !bFinishedReceiving)
	{
		uint32 PendingDataSize = 0;
		while (ClientSocket->HasPendingData(PendingDataSize) && PendingDataSize >= 12)
		{
			int32 BytesRead = 0;
			if (ClientSocket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr) && BytesRead >= 12)
			{
				uint32 ChunkCount = 0;
				uint32 TotalFound = 0;
				uint32 IsLastChunk = 0;
				FMemory::Memcpy(&ChunkCount, RecvBuffer.GetData(), sizeof(uint32));
				FMemory::Memcpy(&TotalFound, RecvBuffer.GetData() + 4, sizeof(uint32));
				FMemory::Memcpy(&IsLastChunk, RecvBuffer.GetData() + 8, sizeof(uint32));
				if (TotalFound == 0)
				{
					bFinishedReceiving = true;
					break;
				}
				int32 Offset = 12;
				for (uint32 i = 0; i < ChunkCount && (Offset + 20) <= BytesRead; ++i)
				{
					uint32 CityIdx = 0;
					float Magnitude = 0.0f;
					float OffsetMin = 0.0f;
					float UtcHour = 0.0f;
					int32 Flags = 0;
					FMemory::Memcpy(&CityIdx, RecvBuffer.GetData() + Offset, sizeof(uint32));
					FMemory::Memcpy(&Magnitude, RecvBuffer.GetData() + Offset + 4, sizeof(float));
					FMemory::Memcpy(&OffsetMin, RecvBuffer.GetData() + Offset + 8, sizeof(float));
					FMemory::Memcpy(&UtcHour, RecvBuffer.GetData() + Offset + 12, sizeof(float));
					FMemory::Memcpy(&Flags, RecvBuffer.GetData() + Offset + 16, sizeof(int32));
					Offset += 20;
					if (Manager->AllCitiesFlat.IsValidIndex(CityIdx))
					{
						const FString& CountryName = Manager->AllCitiesFlat[CityIdx].Key;
						const FCountryPoint& P = Manager->AllCitiesFlat[CityIdx].Value;
						FEclipseCityHit Hit;
						Hit.CityName = P.CityName;
						Hit.Country = CountryName;
						Hit.Latitude = P.Lat;
						Hit.Longitude = P.Lon;
						Hit.Altitude = P.Alt;
						Hit.Magnitude = Magnitude;
						Hit.LocalTimeOffsetHours = (double)OffsetMin / 60.0;
						Hit.UtcHour = UtcHour;
						CachedCityHits.Add(Hit);
					}
				}
				if (IsLastChunk == 1)
				{
					bFinishedReceiving = true;
					break;
				}
			}
		}
		FPlatformProcess::Sleep(0.002f);
	}
	ClientSocket->Close();
	SocketSubsystem->DestroySocket(ClientSocket);
	RenderCityList(FString());
}
void UEclipseBrowserWidget::OnCitySearchChanged(const FText& Text)
{
	RenderCityList(Text.ToString());
}
void UEclipseBrowserWidget::RenderCityList(const FString& FilterText)
{
	if (!CityListBox) return;
	CityListBox->ClearChildren();
	auto MakeFont = [](int32 Size, bool bBold = false) -> FSlateFontInfo
		{
			return FSlateFontInfo(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
		};
	TArray<FEclipseCityHit> Filtered;
	if (FilterText.IsEmpty())
	{
		Filtered = CachedCityHits;
	}
	else
	{
		for (const FEclipseCityHit& Hit : CachedCityHits)
		{
			if (Hit.CityName.Contains(FilterText, ESearchCase::IgnoreCase) ||
				Hit.Country.Contains(FilterText, ESearchCase::IgnoreCase))
			{
				Filtered.Add(Hit);
			}
		}
	}
	if (CitySummaryText)
	{
		TSet<FString> UniqueCountries;
		for (const FEclipseCityHit& H : CachedCityHits)
		{
			UniqueCountries.Add(H.Country);
		}
		FString SummaryStr = FString::Printf(
			TEXT("%d %s %s %d %s"),
			CachedCityHits.Num(),
			*AOrbitalManager::TrString("ORASE IDENTIFICATE", "CITIES IDENTIFIED"),
			*AOrbitalManager::TrString("SI", "AND"),
			UniqueCountries.Num(),
			*AOrbitalManager::TrString("TARI", "COUNTRIES")
		);
		CitySummaryText->SetText(FText::FromString(SummaryStr));
		CitySummaryText->SetFont(MakeFont(12, true));
		CitySummaryText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.85f, 1.0f, 1.0f)));
	}
	if (Filtered.Num() == 0)
	{
		UTextBlock* NoneTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NoneTxt->SetText(CachedCityHits.Num() == 0
			? AOrbitalManager::Tr("Niciun oras major in traiectorie.", "No major city in path.")
			: AOrbitalManager::Tr("Niciun rezultat pentru cautarea ta.", "No results for your search."));
		NoneTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
		NoneTxt->SetFont(MakeFont(12));
		CityListBox->AddChildToVerticalBox(NoneTxt)->SetPadding(FMargin(0.f, 12.f));
		return;
	}
	const float W_Rank = 0.40f;
	const float W_City = 2.20f;
	const float W_Country = 2.00f;
	const float W_Lat = 0.80f;
	const float W_Lon = 0.80f;
	const float W_Alt = 0.55f;
	//const float W_Coverage = 0.85f;
	const float W_Coverage = bLastCityQueryWasLunar ? 0.0f : 0.85f; // Ascunsă la Lună
	const float W_TimeUTC = 1.10f;
	const float W_Action = 0.70f;
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.09f, 1.0f));
	HeaderBg->SetPadding(FMargin(12.f, 7.f, 28.f, 7.f));
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderRow);
	auto AddHeaderCell = [&](const FText& Text, float Weight, EHorizontalAlignment HAlign = HAlign_Left)
		{
			if (Weight <= 0.0f)
			{
				return;
			}
			UTextBlock* ColTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			ColTxt->SetText(Text);
			ColTxt->SetFont(MakeFont(11, true));
			ColTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f)));
			UHorizontalBoxSlot* Slot = HeaderRow->AddChildToHorizontalBox(ColTxt);
			FSlateChildSize SizeRule(ESlateSizeRule::Fill);
			SizeRule.Value = Weight;
			Slot->SetSize(SizeRule);
			Slot->SetHorizontalAlignment(HAlign);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(4.f, 0.f));
		};
	AddHeaderCell(FText::FromString(TEXT("#")), W_Rank, HAlign_Center);
	AddHeaderCell(AOrbitalManager::Tr("ORAS", "CITY"), W_City, HAlign_Left);
	AddHeaderCell(AOrbitalManager::Tr("TARA", "COUNTRY"), W_Country, HAlign_Left);
	AddHeaderCell(AOrbitalManager::Tr("LATITUDINE", "LATITUDE"), W_Lat, HAlign_Left);
	AddHeaderCell(AOrbitalManager::Tr("LONGITUDINE", "LONGITUDE"), W_Lon, HAlign_Left);
	AddHeaderCell(AOrbitalManager::Tr("ALTITUDINE", "ALTITUDE"), W_Alt, HAlign_Right);
	if (!bLastCityQueryWasLunar)
	{
		AddHeaderCell(AOrbitalManager::Tr("ACOPERIRE", "COVERAGE"), W_Coverage, HAlign_Right);
	}
	//AddHeaderCell(AOrbitalManager::Tr("ACOPERIRE", "COVERAGE"), W_Coverage, HAlign_Right);
	AddHeaderCell(AOrbitalManager::Tr("ORA MAX (UTC)", "MAX TIME (UTC)"), W_TimeUTC, HAlign_Center);
	AddHeaderCell(FText::FromString(TEXT("")), W_Action, HAlign_Center);
	CityListBox->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
	const int32 MaxShown = bShowAllCities ? Filtered.Num() : FMath::Min(Filtered.Num(), 250);
	for (int32 i = 0; i < MaxShown; i++)
	{
		const FEclipseCityHit& Hit = Filtered[i];
		const bool bIsTop3 = (i < 3);
		UBorder* RowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FLinearColor RowColor = (i % 2 == 0) ? FLinearColor(0.12f, 0.12f, 0.13f, 1.f) : FLinearColor(0.15f, 0.15f, 0.16f, 1.f);
		if (bIsTop3)
		{
			RowColor = FLinearColor(0.13f, 0.22f, 0.16f, 1.f);
		}
		RowBg->SetBrushColor(RowColor);
		RowBg->SetPadding(FMargin(12.f, 6.f, 12.f, 6.f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RowBg->SetContent(Row);
		auto AddDataCell = [&](UWidget* Widget, float Weight, EHorizontalAlignment HAlign = HAlign_Left)
			{
				UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget);
				FSlateChildSize SizeRule(ESlateSizeRule::Fill);
				SizeRule.Value = Weight;
				Slot->SetSize(SizeRule);
				Slot->SetHorizontalAlignment(HAlign);
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(4.f, 0.f));
				return Slot;
			};
		UTextBlock* RankTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RankTxt->SetText(FText::FromString(FString::Printf(TEXT("#%d"), i + 1)));
		RankTxt->SetFont(MakeFont(12, bIsTop3));
		RankTxt->SetColorAndOpacity(FSlateColor(bIsTop3 ? FLinearColor(0.4f, 1.f, 0.45f) : FLinearColor(0.5f, 0.5f, 0.55f)));
		AddDataCell(RankTxt, W_Rank, HAlign_Center);
		UTextBlock* CityTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CityTxt->SetText(FText::FromString(Hit.CityName));
		CityTxt->SetFont(MakeFont(13, true));
		CityTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f)));
		AddDataCell(CityTxt, W_City, HAlign_Left);
		UTextBlock* CountryTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CountryTxt->SetText(FText::FromString(Hit.Country));
		CountryTxt->SetFont(MakeFont(12));
		CountryTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
		AddDataCell(CountryTxt, W_Country, HAlign_Left);
		FString LatFormatted = FString::Printf(TEXT("%.3f° %s"), FMath::Abs(Hit.Latitude), Hit.Latitude >= 0.f ? TEXT("N") : TEXT("S"));
		UTextBlock* LatTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LatTxt->SetText(FText::FromString(LatFormatted));
		LatTxt->SetFont(MakeFont(11));
		LatTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.75f, 0.85f)));
		AddDataCell(LatTxt, W_Lat, HAlign_Left);
		FString LonFormatted = FString::Printf(TEXT("%.3f° %s"), FMath::Abs(Hit.Longitude), Hit.Longitude >= 0.f ? TEXT("E") : TEXT("W"));
		UTextBlock* LonTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LonTxt->SetText(FText::FromString(LonFormatted));
		LonTxt->SetFont(MakeFont(11));
		LonTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.75f, 0.85f)));
		AddDataCell(LonTxt, W_Lon, HAlign_Center);
		FString AltFormatted = FString::Printf(TEXT("%d m"), FMath::RoundToInt(Hit.Altitude));
		UTextBlock* AltTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		AltTxt->SetText(FText::FromString(AltFormatted));
		AltTxt->SetFont(MakeFont(11));
		AltTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.85f)));
		AddDataCell(AltTxt, W_Alt, HAlign_Center);
		if (!bLastCityQueryWasLunar) {
			UTextBlock* PctTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			PctTxt->SetText(FText::FromString(FString::Printf(TEXT("%.3f%%"), Hit.Magnitude * 100.f)));
			PctTxt->SetFont(MakeFont(13, true));
			PctTxt->SetColorAndOpacity(FSlateColor(bIsTop3 ? FLinearColor(0.35f, 1.f, 0.45f) : FLinearColor(0.3f, 0.85f, 1.f)));
			AddDataCell(PctTxt, W_Coverage, HAlign_Center);
		}


		float WrappedUtc = FMath::Fmod(Hit.UtcHour, 24.0f);
		if (WrappedUtc < 0.0f) WrappedUtc += 24.0f;
		int32 UtcH = FMath::FloorToInt(WrappedUtc);
		float MinFrac = (WrappedUtc - UtcH) * 60.0f;
		int32 UtcM = FMath::FloorToInt(MinFrac);
		int32 UtcS = FMath::FloorToInt((MinFrac - UtcM) * 60.0f);
		UTextBlock* UtcTimeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		UtcTimeTxt->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), UtcH, UtcM, UtcS)));
		UtcTimeTxt->SetFont(MakeFont(12, true));
		UtcTimeTxt->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.35f, 1.0f)));
		AddDataCell(UtcTimeTxt, W_TimeUTC, HAlign_Center);
		/*int32 TargetYear = CurrentDetailEclipse.RealYear;
		int32 TargetMonth = CurrentDetailEclipse.Month;
		int32 TargetDay = CurrentDetailEclipse.Day;
		float TargetHour = Hit.UtcHour;*/
		int32 TargetYear, TargetMonth, TargetDay;
		if (bLastCityQueryWasLunar)
		{
			TargetYear = CurrentDetailLunarEclipse.Year;
			TargetMonth = CurrentDetailLunarEclipse.Month;
			TargetDay = CurrentDetailLunarEclipse.Day;
		}
		else
		{
			TargetYear = CurrentDetailEclipse.RealYear;
			TargetMonth = CurrentDetailEclipse.Month;
			TargetDay = CurrentDetailEclipse.Day;
		}
		float TargetHour = Hit.UtcHour;
		if (TargetHour >= 24.0f)
		{
			TargetHour -= 24.0f;
			TargetDay += 1;
			static const int32 DaysPerMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
			const int32 AbsY = FMath::Abs(TargetYear);
			const bool bLeap = ((AbsY % 4 == 0 && AbsY % 100 != 0) || (AbsY % 400 == 0));
			const int32 MaxDays = (TargetMonth == 2) ? (bLeap ? 29 : 28) : DaysPerMonth[FMath::Clamp(TargetMonth, 1, 12)];
			if (TargetDay > MaxDays)
			{
				TargetDay = 1;
				TargetMonth++;
				if (TargetMonth > 12)
				{
					TargetMonth = 1;
					TargetYear++;
				}
			}
		}
		else if (TargetHour < 0.0f)
		{
			TargetHour += 24.0f;
			TargetDay -= 1;
			if (TargetDay < 1)
			{
				TargetMonth--;
				if (TargetMonth < 1)
				{
					TargetMonth = 12;
					TargetYear--;
				}
				static const int32 DaysPerMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
				const int32 AbsY = FMath::Abs(TargetYear);
				const bool bLeap = ((AbsY % 4 == 0 && AbsY % 100 != 0) || (AbsY % 400 == 0));
				TargetDay = (TargetMonth == 2) ? (bLeap ? 29 : 28) : DaysPerMonth[FMath::Clamp(TargetMonth, 1, 12)];
			}
		}
		UEclipseJumpButton* CityGoBtn = WidgetTree->ConstructWidget<UEclipseJumpButton>(UEclipseJumpButton::StaticClass());
		CityGoBtn->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f));
		CityGoBtn->ParentBrowser = this;
		CityGoBtn->TargetLat = Hit.Latitude;
		CityGoBtn->TargetLon = Hit.Longitude;
		CityGoBtn->TargetDay = TargetDay;
		CityGoBtn->TargetMonth = TargetMonth;
		CityGoBtn->TargetYear = TargetYear;
		CityGoBtn->TargetHour = TargetHour;
		CityGoBtn->bUseExactCoordinates = true;
		CityGoBtn->OnClicked.AddDynamic(CityGoBtn, &UEclipseJumpButton::HandleClick);
		UTextBlock* CityGoBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CityGoBtnTxt->SetText(AOrbitalManager::Tr("VEZI", "VIEW"));
		CityGoBtnTxt->SetFont(MakeFont(11, true));
		CityGoBtnTxt->SetJustification(ETextJustify::Center);
		CityGoBtnTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
		CityGoBtn->AddChild(CityGoBtnTxt);
		AddDataCell(CityGoBtn, W_Action, HAlign_Fill);
		CityListBox->AddChildToVerticalBox(RowBg)->SetPadding(FMargin(0.f, 1.f));
	}
	if (!bShowAllCities && Filtered.Num() > MaxShown)
	{
		UButton* ShowAllBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		ShowAllBtn->SetBackgroundColor(FLinearColor(0.18f, 0.26f, 0.35f, 1.0f));
		ShowAllBtn->OnClicked.AddDynamic(this, &UEclipseBrowserWidget::OnToggleAllCitiesClicked);
		UTextBlock* ShowAllTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FString RoText = FString::Printf(TEXT("AFISEAZA TOATE CELE %d ORASE (+%d)"), Filtered.Num(), Filtered.Num() - MaxShown);
		const FString EnText = FString::Printf(TEXT("SHOW ALL %d CITIES (+%d)"), Filtered.Num(), Filtered.Num() - MaxShown);
		ShowAllTxt->SetText(FText::FromString(AOrbitalManager::TrString(RoText, EnText)));
		ShowAllTxt->SetFont(MakeFont(12, true));
		ShowAllTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.95f, 1.0f)));
		ShowAllTxt->SetJustification(ETextJustify::Center);
		ShowAllBtn->AddChild(ShowAllTxt);
		CityListBox->AddChildToVerticalBox(ShowAllBtn)->SetPadding(FMargin(0.f, 10.f, 0.f, 6.f));
	}
}
void UEclipseBrowserWidget::RefreshLunarList()
{
	if (!EclipseListScroll) return;
	EclipseListScroll->ClearChildren();
	LunarRowDataMap.Empty();
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager) return;
	int32 MinYear = (MinYearInput && !MinYearInput->GetText().IsEmpty()) ? FCString::Atoi(*MinYearInput->GetText().ToString()) : -13000;
	int32 MaxYear = (MaxYearInput && !MaxYearInput->GetText().IsEmpty()) ? FCString::Atoi(*MaxYearInput->GetText().ToString()) : 17000;
	TArray<FLunarEclipseElements> Filtered;
	for (const FLunarEclipseElements& E : Manager->AllLunarEclipses)
	{
		if (E.Year < MinYear || E.Year > MaxYear) continue;
		if (ActiveSarosFilter != -9999 && E.Saros != ActiveSarosFilter) continue;
		bool bTypeAllowed = false;
		switch (E.TypeNumber)
		{
		case 1: bTypeAllowed = bShowPenumbralPartial; break;
		case 2: bTypeAllowed = bShowPenumbralTotal;   break;
		case 3: bTypeAllowed = bShowUmbralPartial;    break;
		case 4: bTypeAllowed = bShowUmbralTotal;      break;
		case 5: bTypeAllowed = bShowUmbralCentral;    break;
		}
		if (!bTypeAllowed) continue;
		Filtered.Add(E);
	}
	const int32 SortIdx = SortComboBox ? SortComboBox->GetSelectedIndex() : 0;
	Filtered.Sort([SortIdx](const FLunarEclipseElements& A, const FLunarEclipseElements& B)
		{
			if (SortIdx == 1)
			{
				if (A.Year != B.Year) return A.Year > B.Year;
				if (A.Month != B.Month) return A.Month > B.Month;
				return A.Day > B.Day;
			}
			if (SortIdx == 2)
			{
				return A.Umag > B.Umag;
			}
			if (A.Year != B.Year) return A.Year < B.Year;
			if (A.Month != B.Month) return A.Month < B.Month;
			return A.Day < B.Day;
		});
	CachedFilteredLunar = Filtered;
	VisibleLunarCount = FMath::Min(50, Filtered.Num());
	for (int32 i = 0; i < VisibleLunarCount; i++)
	{
		BuildLunarListRow(Filtered[i]);
	}
	if (LoadMoreButton)
	{
		LoadMoreButton->SetVisibility(Filtered.Num() > VisibleLunarCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
FLinearColor UEclipseBrowserWidget::GetLunarTypeColor(int32 TypeNumber)
{
	switch (TypeNumber)
	{
	case 1: return FLinearColor(0.6f, 0.6f, 0.9f);
	case 2: return FLinearColor(0.4f, 0.4f, 0.9f);
	case 3: return FLinearColor(0.9f, 0.7f, 0.2f);
	case 4: return FLinearColor(0.9f, 0.3f, 0.2f);
	case 5: return FLinearColor(0.7f, 0.1f, 0.1f);
	}
	return FLinearColor::White;
}
FString UEclipseBrowserWidget::GetLunarTypeString(int32 TypeNumber)
{
	switch (TypeNumber)
	{
	case 1: return AOrbitalManager::TrString("PENUMBRALA PARTIALA", "PENUMBRAL PARTIAL");
	case 2: return AOrbitalManager::TrString("PENUMBRALA TOTALA", "PENUMBRAL TOTAL");
	case 3: return AOrbitalManager::TrString("UMBRALA PARTIALA", "UMBRAL PARTIAL");
	case 4: return AOrbitalManager::TrString("UMBRALA TOTALA", "UMBRAL TOTAL");
	case 5: return AOrbitalManager::TrString("UMBRALA CENTRALA", "UMBRAL CENTRAL");
	}
	return TEXT("?");
}
void UEclipseBrowserWidget::BuildLunarListRow(const FLunarEclipseElements& E)
{
	UBorder* RowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RowBg->SetBrushColor(FLinearColor(0.14f, 0.14f, 0.15f, 1.f));
	RowBg->SetPadding(FMargin(10.f, 8.f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowBg->SetContent(Row);
	UBorder* Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Accent->SetBrushColor(GetLunarTypeColor(E.TypeNumber));
	USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AccentSize->SetWidthOverride(6.f);
	AccentSize->SetContent(Accent);
	Row->AddChildToHorizontalBox(AccentSize)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	UVerticalBox* TextCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextCol);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock* DateTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DateTxt->SetText(FText::FromString(FString::Printf(TEXT("%02d/%02d/%d"), E.Day, E.Month, E.Year)));
	DateTxt->SetFont(GetFont(15, true));
	TextCol->AddChildToVerticalBox(DateTxt);
	UBorder* TypeBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TypeBadge->SetBrushColor(GetLunarTypeColor(E.TypeNumber));
	TypeBadge->SetPadding(FMargin(6.f, 2.f));
	UTextBlock* TypeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TypeTxt->SetText(FText::FromString(GetLunarTypeString(E.TypeNumber)));
	TypeTxt->SetFont(GetFont(10, true));
	TypeTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	TypeBadge->SetContent(TypeTxt);
	TextCol->AddChildToVerticalBox(TypeBadge)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
	UTextBlock* MagTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FString MagLabel = (E.TypeNumber >= 3)
		? FString::Printf(TEXT("Umag %.3f"), E.Umag)
		: FString::Printf(TEXT("Pmag %.3f"), E.Pmag);
	MagTxt->SetText(FText::FromString(MagLabel));
	MagTxt->SetFont(GetFont(13, true));
	MagTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.8f, 1.f)));
	UHorizontalBoxSlot* MagSlot = Row->AddChildToHorizontalBox(MagTxt);
	MagSlot->SetVerticalAlignment(VAlign_Center);
	MagSlot->SetPadding(FMargin(15.f, 0.f));
	ULunarJumpButton* GoBtn = WidgetTree->ConstructWidget<ULunarJumpButton>(ULunarJumpButton::StaticClass());
	GoBtn->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f));
	GoBtn->ParentBrowser = this;
	GoBtn->TargetDay = E.Day;
	GoBtn->TargetMonth = E.Month;
	GoBtn->TargetYear = E.Year;
	GoBtn->TargetHour = (float)E.T0;
	GoBtn->OnClicked.AddDynamic(GoBtn, &ULunarJumpButton::HandleClick);
	float ViewLon = -(float)E.eLngW_GE;
	while (ViewLon > 180.0f) ViewLon -= 360.0f;
	while (ViewLon < -180.0f) ViewLon += 360.0f;
	float ViewLat = (float)E.Lat_GE;
	GoBtn->TargetLat = ViewLat;
	GoBtn->TargetLon = ViewLon;
	UTextBlock* GoTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	GoTxt->SetText(AOrbitalManager::Tr(TEXT("VEZI"), TEXT("VIEW")));
	GoTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	GoBtn->AddChild(GoTxt);
	Row->AddChildToHorizontalBox(GoBtn)->SetVerticalAlignment(VAlign_Center);
	ULunarJumpButton* SarosBtn = WidgetTree->ConstructWidget<ULunarJumpButton>(ULunarJumpButton::StaticClass());
	SarosBtn->SetBackgroundColor(FLinearColor(0.1f, 0.4f, 0.8f, 1.0f));
	SarosBtn->ParentBrowser = this;
	SarosBtn->bIsSarosButton = true;
	SarosBtn->TargetSaros = E.Saros;
	SarosBtn->OnClicked.AddDynamic(SarosBtn, &ULunarJumpButton::HandleClick);
	UTextBlock* SarosTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SarosTxt->SetText(AOrbitalManager::Tr(TEXT("Vezi Ciclu Saros"), TEXT("View Saros Cycle")));
	SarosTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	SarosTxt->SetFont(GetFont(10, true));
	SarosBtn->AddChild(SarosTxt);
	Row->AddChildToHorizontalBox(SarosBtn)->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	ULunarJumpButton* DetailsBtn = WidgetTree->ConstructWidget<ULunarJumpButton>(ULunarJumpButton::StaticClass());
	DetailsBtn->SetBackgroundColor(FLinearColor(0.5f, 0.5f, 0.1f));
	DetailsBtn->ParentBrowser = this;
	DetailsBtn->bIsDetailsButton = true;
	DetailsBtn->LunarDetailsData = E;
	DetailsBtn->OnClicked.AddDynamic(DetailsBtn, &ULunarJumpButton::HandleClick);
	UTextBlock* DetailsTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DetailsTxt->SetText(AOrbitalManager::Tr(TEXT("DETALII"), TEXT("DETAILS")));
	DetailsTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailsTxt->SetFont(GetFont(10, true));
	DetailsBtn->AddChild(DetailsTxt);
	Row->AddChildToHorizontalBox(DetailsBtn)->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
	EclipseListScroll->AddChild(RowBg);
	USpacer* RowSpace = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	RowSpace->SetSize(FVector2D(1.f, 4.f));
	EclipseListScroll->AddChild(RowSpace);
}
void UEclipseBrowserWidget::OnLunarRowClicked()
{
}
void UEclipseBrowserWidget::SetEclipseData(const TArray<FEclipseData>& NewData)
{
	AllEclipses = NewData;
	RefreshList();
}
void UEclipseBrowserWidget::RefreshList()
{
	if (CurrentMode == ECatalogMode::Lunar)
	{
		RefreshLunarList();
		return;
	}
	if (!EclipseListScroll) return;
	EclipseListScroll->ClearChildren();
	CurrentlyShownCount = 0;
	FullDataCache.Empty();
	int32 MinYear = -99999;
	int32 MaxYear = 99999;
	if (MinYearInput && !MinYearInput->GetText().IsEmpty())
		MinYear = FCString::Atoi(*MinYearInput->GetText().ToString());
	if (MaxYearInput && !MaxYearInput->GetText().IsEmpty())
		MaxYear = FCString::Atoi(*MaxYearInput->GetText().ToString());
	if (MinYear > MaxYear)
	{
		int32 Temp = MinYear;
		MinYear = MaxYear;
		MaxYear = Temp;
		if (MinYearInput) MinYearInput->SetText(FText::FromString(FString::FromInt(MinYear)));
		if (MaxYearInput) MaxYearInput->SetText(FText::FromString(FString::FromInt(MaxYear)));
	}
	AOrbitalManager* LocManager = nullptr;
	const TArray<FCountryPoint>* ActiveCountryPoints = nullptr;
	double CachedDeltaT = 69.184;
	if (LocationMode != ELocationFilterMode::None)
	{
		LocManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (LocManager)
		{
			CachedDeltaT = LocManager->Deltatseconds;
		}
	}
	double DeltaTSeconds = CachedDeltaT;
	if (LocationMode != ELocationFilterMode::None)
	{
		LocManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	}
	bool bCountryUnrecognized = false;
	if (LocationMode == ELocationFilterMode::Country && LocManager)
	{
		FString CountryName = ExtractCurrentCountryName();
		ActiveCountryPoints = LocManager->CountryPoints.Find(CountryName);
		if (!ActiveCountryPoints || ActiveCountryPoints->Num() == 0)
		{
			bCountryUnrecognized = true;
		}
	}
	TArray<int32> CandidateIndices;
	CandidateIndices.Reserve(AllEclipses.Num());
	for (int32 i = 0; i < AllEclipses.Num(); i++)
	{
		const FEclipseData& E = AllEclipses[i];
		if (ActiveSarosFilter != -9999 && E.SarosSeries != ActiveSarosFilter) continue;
		if (E.RealYear < MinYear || E.RealYear > MaxYear) continue;
		CandidateIndices.Add(i);
	}
	const bool bUseLocation = (LocationMode == ELocationFilterMode::Drone) ||
		(LocationMode == ELocationFilterMode::Country && !bCountryUnrecognized);
	if (bUseLocation)
	{
		TArray<FEclipseData> ResultsArray;
		ResultsArray.SetNum(CandidateIndices.Num());
		TArray<bool> KeepFlags;
		KeepFlags.SetNumZeroed(CandidateIndices.Num());
		ParallelFor(CandidateIndices.Num(), [&](int32 i)
			{
				const FEclipseData& Eclipse = AllEclipses[CandidateIndices[i]];
				if (!LocManager || !LocManager->AllEclipses.IsValidIndex(Eclipse.SourceIndex))
				{
					return;
				}
				if (LocationMode == ELocationFilterMode::Country && ActiveCountryPoints)
				{
					bool bAnyPointClose = false;
					for (const FCountryPoint& P : *ActiveCountryPoints)
					{
						const double DLat = FMath::Abs(P.Lat - Eclipse.Latitude);
						const double DLon = FMath::Abs(P.Lon - Eclipse.Longitude);
						if (DLat < 90.0 && DLon < 130.0)
						{
							bAnyPointClose = true;
							break;
						}
					}
					if (!bAnyPointClose)
					{
						return;
					}
				}
				else if (LocationMode == ELocationFilterMode::Drone)
				{
					const double DLat = FMath::Abs(CurrentDroneLat - Eclipse.Latitude);
					const double DLon = FMath::Abs(CurrentDroneLon - Eclipse.Longitude);
					if (DLat >= 90.0 || DLon >= 130.0)
					{
						return;
					}
				}
				FLocalEclipseVisibility Vis;
				if (LocationMode == ELocationFilterMode::Drone)
				{
					Vis = ComputeLocalVisibility(LocManager->AllEclipses[Eclipse.SourceIndex], CurrentDroneLat, CurrentDroneLon, CurrentDroneAlt, DeltaTSeconds);
				}
				else
				{
					if (!ActiveCountryPoints || ActiveCountryPoints->Num() == 0)
					{
						return;
					}
					Vis = ComputeCountryVisibility(LocManager->AllEclipses[Eclipse.SourceIndex], *ActiveCountryPoints, DeltaTSeconds);
				}
				if (!Vis.bVisible)
				{
					return;
				}
				bool bKeep = false;
				if (bShowTotal && Vis.bIsTotalOrAnnular && !Vis.bIsAnnularLocal && Eclipse.Type == EEclipseType::Total)   bKeep = true;
				if (!bKeep && bShowPartial && !Vis.bIsTotalOrAnnular)                                                       bKeep = true;
				if (!bKeep && bShowAnnular && Vis.bIsTotalOrAnnular && Vis.bIsAnnularLocal && Eclipse.Type == EEclipseType::Annular) bKeep = true;
				if (!bKeep && bShowHybrid && Eclipse.Type == EEclipseType::Hybrid && Vis.bIsTotalOrAnnular)                bKeep = true;
				if (!bKeep)
				{
					return;
				}
				FEclipseData DisplayItem = Eclipse;
				if (Eclipse.Type == EEclipseType::Hybrid && Vis.bIsTotalOrAnnular)
				{
					DisplayItem.Type = EEclipseType::Hybrid;
				}
				else if (Vis.bIsTotalOrAnnular)
				{
					DisplayItem.Type = Vis.bIsAnnularLocal ? EEclipseType::Annular : EEclipseType::Total;
				}
				else
				{
					DisplayItem.Type = EEclipseType::Partial;
				}
				DisplayItem.Obscuration = Vis.LocalMagnitude;
				ResultsArray[i] = DisplayItem;
				KeepFlags[i] = true;
			});
		for (int32 i = 0; i < CandidateIndices.Num(); i++)
		{
			if (KeepFlags[i])
			{
				FullDataCache.Add(ResultsArray[i]);
			}
		}
	}
	else
	{
		for (int32 idx : CandidateIndices)
		{
			const FEclipseData& Eclipse = AllEclipses[idx];
			bool bKeep = false;
			switch (Eclipse.Type)
			{
			case EEclipseType::Total:   bKeep = bShowTotal;   break;
			case EEclipseType::Partial: bKeep = bShowPartial; break;
			case EEclipseType::Annular: bKeep = bShowAnnular; break;
			case EEclipseType::Hybrid:  bKeep = bShowHybrid;  break;
			}
			if (bKeep)
			{
				FullDataCache.Add(Eclipse);
			}
		}
	}
	int32 SortIndex = SortComboBox ? SortComboBox->GetSelectedIndex() : 0;
	FullDataCache.Sort([SortIndex](const FEclipseData& A, const FEclipseData& B) {
		if (SortIndex == 1)
		{
			if (A.RealYear != B.RealYear) return A.RealYear > B.RealYear;
			if (A.Month != B.Month) return A.Month > B.Month;
			return A.Day > B.Day;
		}
		if (SortIndex == 2)
		{
			return A.Obscuration > B.Obscuration;
		}
		if (A.RealYear != B.RealYear) return A.RealYear < B.RealYear;
		if (A.Month != B.Month) return A.Month < B.Month;
		return A.Day < B.Day;
		});
	RenderNextBatch();
	if (GEngine)
	{
		FString DebugTxt = AOrbitalManager::TrString(
			FString::Printf(TEXT("Am filtrat si au ramas: %d eclipse."), FullDataCache.Num()),
			FString::Printf(TEXT("Filtered and kept: %d eclipses."), FullDataCache.Num())
		);
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, DebugTxt);
	}
}
void UEclipseBrowserWidget::OnYearFilterClicked()
{
	RefreshList();
}
void UEclipseBrowserWidget::AddRow(const FEclipseData& Data)
{
	UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RowBorder->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
	RowBorder->SetPadding(FMargin(5.f));
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RowBorder->SetContent(RowBox);
	auto AddTxt = [&](FString Text, float Width, FLinearColor Color) {
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Text));
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetFont(GetFont(14));
		UHorizontalBoxSlot* Slot = RowBox->AddChildToHorizontalBox(T);
		FSlateChildSize Sz(ESlateSizeRule::Fill); Sz.Value = Width;
		Slot->SetSize(Sz);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
		};
	AddTxt(Data.FormattedDateStr, 1.3f, FLinearColor::White);
	AddTxt(Data.Name, 2.4f, FLinearColor::White);
	AddTxt(GetTypeString(Data.Type), 0.8f, GetTypeColor(Data.Type));
	FString CovStr = FString::Printf(TEXT("%.1f%%"), Data.Obscuration * 100.f);
	AddTxt(CovStr, 0.5f, FLinearColor::Gray);
	UEclipseJumpButton* GoBtn = WidgetTree->ConstructWidget<UEclipseJumpButton>(UEclipseJumpButton::StaticClass());
	GoBtn->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f));
	GoBtn->TargetMonth = Data.Month;
	GoBtn->TargetDay = Data.Day;
	GoBtn->TargetHour = Data.Hour;
	GoBtn->TargetLat = Data.Latitude;
	GoBtn->TargetLon = Data.Longitude;
	GoBtn->ParentBrowser = this;
	GoBtn->TargetYear = Data.RealYear;
	GoBtn->OnClicked.AddDynamic(GoBtn, &UEclipseJumpButton::HandleClick);
	UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BtnTxt->SetText(AOrbitalManager::Tr("VEZI", "VIEW"));
	BtnTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	GoBtn->AddChild(BtnTxt);
	RowBox->AddChildToHorizontalBox(GoBtn);
	EclipseListScroll->AddChild(RowBorder);
	USpacer* RowSpace = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	RowSpace->SetSize(FVector2D(1.f, 5.f));
	EclipseListScroll->AddChild(RowSpace);
	UEclipseJumpButton* SarosBtn = WidgetTree->ConstructWidget<UEclipseJumpButton>(UEclipseJumpButton::StaticClass());
	SarosBtn->SetBackgroundColor(FLinearColor(0.1f, 0.4f, 0.8f, 1.0f));
	SarosBtn->TargetSaros = Data.SarosSeries;
	SarosBtn->bIsSarosButton = true;
	SarosBtn->ParentBrowser = this;
	SarosBtn->OnClicked.AddDynamic(SarosBtn, &UEclipseJumpButton::HandleClick);
	UTextBlock* SarosTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SarosTxt->SetText(AOrbitalManager::Tr("Vezi Ciclu Saros", "View Saros Cycle"));
	SarosTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	SarosTxt->SetFont(GetFont(10, true));
	SarosBtn->AddChild(SarosTxt);
	RowBox->AddChildToHorizontalBox(SarosBtn);
	UEclipseJumpButton* DetailsBtn = WidgetTree->ConstructWidget<UEclipseJumpButton>(UEclipseJumpButton::StaticClass());
	DetailsBtn->SetBackgroundColor(FLinearColor(0.5f, 0.5f, 0.1f));
	DetailsBtn->bIsDetailsButton = true;
	DetailsBtn->DetailsData = Data;
	DetailsBtn->ParentBrowser = this;
	DetailsBtn->OnClicked.AddDynamic(DetailsBtn, &UEclipseJumpButton::HandleClick);
	UTextBlock* DetailsBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DetailsBtnTxt->SetText(AOrbitalManager::Tr("DETALII", "DETAILS"));
	DetailsBtnTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	DetailsBtnTxt->SetFont(GetFont(10, true));
	DetailsBtn->AddChild(DetailsBtnTxt);
	RowBox->AddChildToHorizontalBox(DetailsBtn);
}
void UEclipseBrowserWidget::ApplySarosFilter(int32 SarosNumber)
{
	ActiveSarosFilter = SarosNumber;
	if (ResetFilterButton) ResetFilterButton->SetVisibility(ESlateVisibility::Visible);
	RefreshList();
}
void UEclipseBrowserWidget::ResetFilters()
{
	ActiveSarosFilter = -9999;
	if (ResetFilterButton) ResetFilterButton->SetVisibility(ESlateVisibility::Collapsed);
	RefreshList();
}
void UEclipseBrowserWidget::OnToggleFilterTotal() { bShowTotal = !bShowTotal;     RefreshFilterButtonColors(); RefreshList(); }
void UEclipseBrowserWidget::OnToggleFilterPartial() { bShowPartial = !bShowPartial; RefreshFilterButtonColors(); RefreshList(); }
void UEclipseBrowserWidget::OnToggleFilterAnnular() { bShowAnnular = !bShowAnnular; RefreshFilterButtonColors(); RefreshList(); }
void UEclipseBrowserWidget::OnToggleFilterHybrid() { bShowHybrid = !bShowHybrid;   RefreshFilterButtonColors(); RefreshList(); }
void UEclipseBrowserWidget::OnFilterMenuButtonClicked()
{
	bFilterMenuOpen = !bFilterMenuOpen;
	if (FilterDropdownPanel)
	{
		FilterDropdownPanel->SetVisibility(bFilterMenuOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
void UEclipseBrowserWidget::RefreshFilterRowVisuals()
{
	auto SetRow = [](UButton* Btn, UTextBlock* Txt, bool bActive, FLinearColor ActiveColor, const FString& RO, const FString& EN)
		{
			if (!Btn || !Txt) return;
			Btn->SetBackgroundColor(bActive ? ActiveColor : FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
			Txt->SetText(AOrbitalManager::Tr(RO, EN));
		};
	SetRow(FilterRowTotal, FilterRowTotalTxt, bShowTotal, FLinearColor(1.f, 0.2f, 0.2f), "Totala", "Total");
	SetRow(FilterRowPartial, FilterRowPartialTxt, bShowPartial, FLinearColor(1.f, 0.8f, 0.2f), "Partiala", "Partial");
	SetRow(FilterRowAnnular, FilterRowAnnularTxt, bShowAnnular, FLinearColor(0.2f, 0.8f, 1.f), "Inelara", "Annular");
	SetRow(FilterRowHybrid, FilterRowHybridTxt, bShowHybrid, FLinearColor(0.8f, 0.3f, 1.f), "Hibrida", "Hybrid");
	UpdateFilterMenuLabel();
}
void UEclipseBrowserWidget::RefreshFilterButtonColors()
{
	RefreshFilterDropdownContent();
	RefreshFilterRowVisuals();
}
void UEclipseBrowserWidget::OnSortChanged(FString S, ESelectInfo::Type T) { RefreshList(); }
void UEclipseBrowserWidget::OnCloseClicked() { SetVisibility(ESlateVisibility::Collapsed); }
void UEclipseBrowserWidget::OnSwitchToSolar()
{
	CurrentMode = ECatalogMode::Solar;
	RefreshTabColors();
	UpdateFilterMenuLabel();
	RefreshFilterDropdownContent();
	if (LocationModeComboBox) LocationModeComboBox->SetVisibility(ESlateVisibility::Visible);
	RefreshList();
}
void UEclipseBrowserWidget::OnSwitchToLunar()
{
	CurrentMode = ECatalogMode::Lunar;
	RefreshTabColors();
	UpdateFilterMenuLabel();
	RefreshFilterDropdownContent();
	if (LocationModeComboBox)
	{
		LocationMode = ELocationFilterMode::None;
		LocationModeComboBox->SetSelectedIndex(0);
		LocationModeComboBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshList();
}
void UEclipseBrowserWidget::RefreshTabColors()
{
	if (TabSolarButton)
		TabSolarButton->SetBackgroundColor(CurrentMode == ECatalogMode::Solar ? FLinearColor(0.1f, 0.4f, 0.8f) : FLinearColor(0.2f, 0.2f, 0.2f));
	if (TabLunarButton)
		TabLunarButton->SetBackgroundColor(CurrentMode == ECatalogMode::Lunar ? FLinearColor(0.1f, 0.4f, 0.8f) : FLinearColor(0.2f, 0.2f, 0.2f));
}
FString UEclipseBrowserWidget::GetTypeString(EEclipseType Type)
{
	switch (Type) {
	case EEclipseType::Total: return AOrbitalManager::TrString("TOTALA", "TOTAL");
	case EEclipseType::Partial: return AOrbitalManager::TrString("Partiala", "Partial");
	case EEclipseType::Annular: return AOrbitalManager::TrString("Inelara", "Annular");
	default: return AOrbitalManager::TrString("Hibrida", "Hybrid");
	}
}
FLinearColor UEclipseBrowserWidget::GetTypeColor(EEclipseType Type)
{
	switch (Type) {
	case EEclipseType::Total: return FLinearColor(1.f, 0.2f, 0.2f);
	case EEclipseType::Partial: return FLinearColor(1.f, 0.8f, 0.2f);
	case EEclipseType::Annular: return FLinearColor(0.2f, 0.8f, 1.f);
	case EEclipseType::Hybrid: return FLinearColor(0.8f, 0.3f, 1.f);
	default: return FLinearColor::White;
	}
}
TSharedRef<SWidget> UMainMenuWidget::RebuildWidget()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Root;
	UVerticalBox* MenuVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCanvasPanelSlot* MenuSlot = Root->AddChildToCanvas(MenuVBox);
	MenuSlot->SetPosition(FVector2D(20.f, 20.f));
	MenuSlot->SetSize(FVector2D(250.f, 500.f));
	MenuSlot->SetAutoSize(true);
	MenuToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	MenuToggleButton->SetBackgroundColor(FLinearColor(0.1f, 0.1f, 0.5f));
	UTextBlock* MenuTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MenuTxt->SetText(AOrbitalManager::Tr(" MENIU", " MENU"));
	MenuTxt->SetFont(GetFont(20, true));
	MenuToggleButton->AddChild(MenuTxt);
	MenuToggleButton->OnClicked.AddDynamic(this, &UMainMenuWidget::ToggleMenu);
	MenuVBox->AddChildToVerticalBox(MenuToggleButton);
	MenuDropdownContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	MenuVBox->AddChildToVerticalBox(MenuDropdownContainer);
	OpenBrowserButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenBrowserButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* CatTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CatTxt->SetText(AOrbitalManager::Tr(" > Catalog Eclipse", " > Eclipse Catalog"));
	CatTxt->SetMargin(FMargin(7.f));
	OpenBrowserButton->AddChild(CatTxt);
	OpenBrowserButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenBrowser);
	MenuDropdownContainer->AddChildToVerticalBox(OpenBrowserButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenPlacementButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenPlacementButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* PlaceTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PlaceTxt->SetText(AOrbitalManager::Tr(" > Plasare Coordonate", " > Location Placement"));
	PlaceTxt->SetMargin(FMargin(7.f));
	OpenPlacementButton->AddChild(PlaceTxt);
	OpenPlacementButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenPlacementMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenPlacementButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenTimeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenTimeButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* TimeTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TimeTxt->SetText(AOrbitalManager::Tr(" > Control Timp", " > Time Control"));
	TimeTxt->SetMargin(FMargin(7.f));
	OpenTimeButton->AddChild(TimeTxt);
	OpenTimeButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenTimeMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenTimeButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenOpticsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenOpticsButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* OpticsTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OpticsTxt->SetText(AOrbitalManager::Tr(" > Optica si Telescop", " > Optics & Telescope"));
	OpticsTxt->SetMargin(FMargin(7.f));
	OpenOpticsButton->AddChild(OpticsTxt);
	OpenOpticsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenOpticsMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenOpticsButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenOverlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenOverlayButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* OverlayTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OverlayTxt->SetText(AOrbitalManager::Tr(" > Overlays & Telemetrie", " > Overlays & Telemetry"));
	OverlayTxt->SetMargin(FMargin(7.f));
	OpenOverlayButton->AddChild(OverlayTxt);
	OpenOverlayButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenOverlayMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenOverlayButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenVideoExportButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenVideoExportButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* VideoBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	VideoBtnTxt->SetText(AOrbitalManager::Tr(" > Export Video / Timelapse", " > Video / Timelapse Export"));
	VideoBtnTxt->SetMargin(FMargin(7.f));
	OpenVideoExportButton->AddChild(VideoBtnTxt);
	OpenVideoExportButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenVideoExportMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenVideoExportButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenCameraMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenCameraMenuButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* CameraTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CameraTxt->SetText(AOrbitalManager::Tr(" > Perspective Camera", " > Camera Perspectives"));
	CameraTxt->SetMargin(FMargin(7.f));
	OpenCameraMenuButton->AddChild(CameraTxt);
	OpenCameraMenuButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenCameraMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenCameraMenuButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenTelemetryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenTelemetryButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* TeleTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TeleTxt->SetText(AOrbitalManager::Tr(" > Telemetrie Live", " > Live Telemetry"));
	TeleTxt->SetMargin(FMargin(7.f));
	OpenTelemetryButton->AddChild(TeleTxt);
	OpenTelemetryButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenTelemetryMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenTelemetryButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenVisualFXButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenVisualFXButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* VFXTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	VFXTxt->SetText(AOrbitalManager::Tr(" > VFX / Grafica", " > VFX / Graphics"));
	VFXTxt->SetMargin(FMargin(7.f));
	OpenVisualFXButton->AddChild(VFXTxt);
	OpenVisualFXButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenVisualFXMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenVisualFXButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenSettingsMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenSettingsMenuButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* SettingsTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SettingsTxt->SetText(AOrbitalManager::Tr(" > Setari / Performanta", " > Settings / Performance"));
	SettingsTxt->SetMargin(FMargin(7.f));
	OpenSettingsMenuButton->AddChild(SettingsTxt);
	OpenSettingsMenuButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenSettingsMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenSettingsMenuButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenWeatherButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenWeatherButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* WeatherTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	WeatherTxt->SetText(AOrbitalManager::Tr(" > Conditii Atmosferice", " > Weather Conditions"));
	WeatherTxt->SetMargin(FMargin(7.f));
	OpenWeatherButton->AddChild(WeatherTxt);
	OpenWeatherButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenWeatherMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenWeatherButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenMapMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenMapMenuButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* MapMenuTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MapMenuTxt->SetText(AOrbitalManager::Tr(" > Harta Traseu 2D", " > 2D Eclipse Path Map"));
	MapMenuTxt->SetMargin(FMargin(7.f));
	OpenMapMenuButton->AddChild(MapMenuTxt);
	OpenMapMenuButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenMapMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenMapMenuButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenHelpButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenHelpButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* HelpTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HelpTxt->SetText(AOrbitalManager::Tr(" > Ajutor / Cautare", " > Help / Search"));
	HelpTxt->SetMargin(FMargin(7.f));
	OpenHelpButton->AddChild(HelpTxt);
	OpenHelpButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenHelpMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenHelpButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenBookmarksButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenBookmarksButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* TxtBookmarks = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtBookmarks->SetText(AOrbitalManager::Tr(" > Locatii / Bookmarks", " > Locations / Bookmarks"));
	TxtBookmarks->SetMargin(FMargin(7.f));
	OpenBookmarksButton->AddChild(TxtBookmarks);
	OpenBookmarksButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenBookmarksMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenBookmarksButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	OpenExitMenuButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenExitMenuButton->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* ExitTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ExitTxt->SetText(AOrbitalManager::Tr(" > Iesire Aplicatie", " > Exit Application"));
	ExitTxt->SetMargin(FMargin(7.f));
	OpenExitMenuButton->AddChild(ExitTxt);
	OpenExitMenuButton->OnClicked.AddDynamic(this, &UMainMenuWidget::OpenExitMenu);
	MenuDropdownContainer->AddChildToVerticalBox(OpenExitMenuButton)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	return Super::RebuildWidget();
}
void UMainMenuWidget::ToggleMenu()
{
	if (!MenuDropdownContainer) return;
	if (MenuDropdownContainer->GetVisibility() == ESlateVisibility::Collapsed)
		MenuDropdownContainer->SetVisibility(ESlateVisibility::Visible);
	else
		MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
}
void UMainMenuWidget::OpenBrowser()
{
	if (MenuDropdownContainer)
	{
		MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	UEclipseBrowserWidget* Browser = CreateWidget<UEclipseBrowserWidget>(PC, UEclipseBrowserWidget::StaticClass());
	if (Browser)
	{
		Browser->AddToViewport(100);
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager)
		{
			if (Manager->AllEclipses.Num() == 0)
			{
				Manager->LoadAndParseDatabase();
			}
			TArray<FEclipseData> UI_Data;
			UI_Data.Reserve(Manager->AllEclipses.Num());
			for (int32 Idx = 0; Idx < Manager->AllEclipses.Num(); Idx++)
			{
				const FBesselianElements& RawEclipse = Manager->AllEclipses[Idx];
				if (RawEclipse.Year < -13000 || RawEclipse.Year > 17000) continue;
				if (RawEclipse.Month < 1 || RawEclipse.Month > 12) continue;
				int32 AbsYear = FMath::Abs(RawEclipse.Year);
				bool bIsOriginalYearLeap = ((AbsYear % 4 == 0 && AbsYear % 100 != 0) || (AbsYear % 400 == 0));
				int32 MaxDays = 30;
				static const int32 DaysPerMonth[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
				if (RawEclipse.Month == 2)
				{
					MaxDays = bIsOriginalYearLeap ? 29 : 28;
				}
				else
				{
					MaxDays = DaysPerMonth[RawEclipse.Month];
				}
				int32 SafeDay = FMath::Clamp(RawEclipse.Day, 1, MaxDays);
				double Gamma = FMath::Sqrt(FMath::Square(RawEclipse.x[0]) + FMath::Square(RawEclipse.y[0]));
				double L1 = RawEclipse.l1[0];
				double L2 = RawEclipse.l2[0];
				float OraBruta = RawEclipse.Hour;
				int32 E_Hour = FMath::FloorToInt(OraBruta);
				int32 E_Min = FMath::FloorToInt((OraBruta - E_Hour) * 60.0f);
				int32 E_Sec = FMath::FloorToInt((((OraBruta - E_Hour) * 60.0f) - E_Min) * 60.0f);
				E_Hour = FMath::Clamp(E_Hour, 0, 23);
				E_Min = FMath::Clamp(E_Min, 0, 59);
				E_Sec = FMath::Clamp(E_Sec, 0, 59);
				FEclipseData Item;
				int32 SafeYearForDateTime = RawEclipse.Year;
				if (SafeYearForDateTime < 1 || SafeYearForDateTime > 9999)
				{
					SafeYearForDateTime = bIsOriginalYearLeap ? 4 : 1;
				}
				Item.Date = FDateTime(SafeYearForDateTime, RawEclipse.Month, SafeDay, E_Hour, E_Min, E_Sec);
				Item.SarosSeries = RawEclipse.Saros;
				double x = RawEclipse.x[0]; double y = RawEclipse.y[0]; double d = RawEclipse.d[0]; double mu = RawEclipse.mu[0];
				double rho2 = (x * x) + (y * y);
				double z1 = rho2 <= 1.0 ? FMath::Sqrt(1.0 - rho2) : 0.0;
				if (rho2 > 1.0) { x /= FMath::Sqrt(rho2); y /= FMath::Sqrt(rho2); }
				double LatRad = FMath::Asin(FMath::Clamp(y * FMath::Cos(d) + z1 * FMath::Sin(d), -1.0, 1.0));
				double H = FMath::Atan2(x, z1 * FMath::Cos(d) - y * FMath::Sin(d));
				double LonRad = H - mu;
				while (LonRad > PI) LonRad -= 2.0 * PI;
				while (LonRad < -PI) LonRad += 2.0 * PI;
				Item.Latitude = FMath::RadiansToDegrees(LatRad);
				Item.Longitude = FMath::RadiansToDegrees(LonRad);
				FString TypeStrRO = ""; FString TypeStrEN = "";
				if (Gamma > (0.9972 + FMath::Abs(L2)))
				{
					Item.Type = EEclipseType::Partial; TypeStrRO = "Partiala (Excentrica)"; TypeStrEN = "Partial (Eccentric)";
					Item.Obscuration = (float)FMath::Max(0.0, (L1 - (Gamma - 0.9972)) / (L1 + L2));
				}
				else
				{
					if (L2 < 0.0) { Item.Type = EEclipseType::Total; TypeStrRO = "TOTALA"; TypeStrEN = "TOTAL"; Item.Obscuration = 1.0f; }
					else
					{
						double RadiusAtSurface = L2 - (FMath::Sqrt(FMath::Max(0.0, 1.0 - (Gamma * Gamma))) * RawEclipse.tanf2[0]);
						if (RadiusAtSurface < 0.0) { Item.Type = EEclipseType::Hybrid; TypeStrRO = "Hibrida"; TypeStrEN = "Hybrid"; Item.Obscuration = 1.0f; }
						else { Item.Type = EEclipseType::Annular; TypeStrRO = "Inelara"; TypeStrEN = "Annular"; Item.Obscuration = (L1 + L2) != 0.0 ? (float)((L1 - L2) / (L1 + L2)) : 0.0f; }
					}
				}
				Item.RealYear = RawEclipse.Year;
				Item.Month = RawEclipse.Month;
				Item.Day = SafeDay;
				Item.Hour = RawEclipse.Hour;
				FString DayMonthPrefix = FString::Printf(TEXT("%02d.%02d."), SafeDay, RawEclipse.Month);
				if (RawEclipse.Year < 0)
				{
					FString EraStr = AOrbitalManager::TrString(TEXT("î.Hr."), TEXT("BC"));
					Item.FormattedDateStr = FString::Printf(TEXT("%s%d %s"), *DayMonthPrefix, FMath::Abs(RawEclipse.Year), *EraStr);
				}
				else
				{
					Item.FormattedDateStr = FString::Printf(TEXT("%s%04d"), *DayMonthPrefix, RawEclipse.Year);
				}
				FString YearNameRO = RawEclipse.Year < 0 ? FString::Printf(TEXT("%d î.Hr."), FMath::Abs(RawEclipse.Year)) : FString::Printf(TEXT("%d"), RawEclipse.Year);
				FString YearNameEN = RawEclipse.Year < 0 ? FString::Printf(TEXT("%d BC"), FMath::Abs(RawEclipse.Year)) : FString::Printf(TEXT("%d"), RawEclipse.Year);
				Item.Name = AOrbitalManager::TrString(
					FString::Printf(TEXT("Eclipsa %s %s"), *TypeStrRO, *YearNameRO),
					FString::Printf(TEXT("%s Eclipse %s"), *TypeStrEN, *YearNameEN)
				);
				Item.SourceIndex = Idx;
				UI_Data.Add(Item);
			}
			Browser->SetEclipseData(UI_Data);
			if (GEngine)
			{
				FString DebugRO = FString::Printf(TEXT("Incarcat %d eclipse in lista!"), UI_Data.Num());
				FString DebugEN = FString::Printf(TEXT("Loaded %d eclipses in the list!"), UI_Data.Num());
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, AOrbitalManager::TrString(DebugRO, DebugEN));
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, AOrbitalManager::TrString("EROARE: Nu am gasit AOrbitalManager in scena!", "ERROR: AOrbitalManager not found in scene!"));
			}
		}
	}
}
void UEclipseBrowserWidget::TeleportToExactLocation(float Latitude, float Longitude)
{
	UWorld* World = GetWorld();
	if (!World) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(UGameplayStatics::GetPlayerPawn(World, 0));
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(World);
	if (!MyDrone || !GeoRef) return;
	FVector TargetLLH(Longitude, Latitude, 1000.0f);
	GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
	MyDrone->SetActorLocation(FVector(0, 0, 5000.0f));
	MyDrone->CurrentVelocity = FVector::ZeroVector;
	MyDrone->bSearchingForGround = true;
	MyDrone->SearchTimeout = 5.0f;
	if (MyDrone->EclipseInfoWidget)
	{
		MyDrone->EclipseInfoWidget->SetVisibility(ESlateVisibility::Visible);
		MyDrone->EclipseInfoWidget->SetReportText(TEXT("Teleportare la locatia selectata."));
	}
}
void UEclipseBrowserWidget::TeleportToEclipseLocation(float Latitude, float Longitude)
{
	UWorld* World = GetWorld();
	if (!World) return;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(World, AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->LiveShadowLat = -999.0f;
		Manager->LiveShadowLon = -999.0f;
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("Se calculeaza Ground Zero... Asteptam motorul astronomic!"));
	TSharedPtr<int32> Attempts = MakeShared<int32>(0);
	TSharedPtr<FTimerHandle> PollingTimer = MakeShared<FTimerHandle>();
	World->GetTimerManager().SetTimer(*PollingTimer, FTimerDelegate::CreateWeakLambda(this, [this, Latitude, Longitude, Attempts, PollingTimer]()
		{
			UWorld* InnerWorld = GetWorld();
			if (!InnerWorld) return;
			AOrbitalManager* InnerManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(InnerWorld, AOrbitalManager::StaticClass()));
			ADronaMea* MyDrone = Cast<ADronaMea>(UGameplayStatics::GetPlayerPawn(InnerWorld, 0));
			ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(InnerWorld);
			if (!InnerManager || !MyDrone || !GeoRef) return;
			(*Attempts)++;
			bool bPythonFinished = (InnerManager->LiveShadowLat != -999.0f && InnerManager->LiveShadowLon != -999.0f);
			bool bTimeout = (*Attempts >= 30);
			if (bPythonFinished || bTimeout)
			{
				InnerWorld->GetTimerManager().ClearTimer(*PollingTimer);
				FVector TargetLLH;
				if (bPythonFinished)
				{
					TargetLLH.X = InnerManager->LiveShadowLon;
					TargetLLH.Y = InnerManager->LiveShadowLat;
					TargetLLH.Z = 1000.0f;
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Teleportat fix in centrul umbrei (Calcul Skyfield)!"));
				}
				else
				{
					TargetLLH = FVector(Longitude, Latitude, 1000.0f);
					if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Timeout Python! Teleportat la coordonatele din catalog."));
				}
				GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
				MyDrone->SetActorLocation(FVector(0, 0, 5000.0f));
				MyDrone->CurrentVelocity = FVector::ZeroVector;
				MyDrone->bSearchingForGround = true;
				MyDrone->SearchTimeout = 5.0f;
				if (MyDrone->EclipseInfoWidget)
				{
					MyDrone->EclipseInfoWidget->SetVisibility(ESlateVisibility::Visible);
					MyDrone->EclipseInfoWidget->SetReportText(TEXT("Teleportare finalizata la Ground Zero."));
				}
			}
		}), 0.5f, true);
}
void UEclipseBrowserWidget::RenderNextBatch()
{
	int32 EndIndex = FMath::Min(CurrentlyShownCount + BatchSize, FullDataCache.Num());
	for (int32 i = CurrentlyShownCount; i < EndIndex; i++)
	{
		AddRow(FullDataCache[i]);
	}
	CurrentlyShownCount = EndIndex;
	if (CurrentlyShownCount < FullDataCache.Num())
	{
		if (LoadMoreButton) LoadMoreButton->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		if (LoadMoreButton) LoadMoreButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}
void UEclipseBrowserWidget::RenderNextLunarBatch()
{
	int32 EndIndex = FMath::Min(VisibleLunarCount + BatchSize, CachedFilteredLunar.Num());
	for (int32 i = VisibleLunarCount; i < EndIndex; i++)
	{
		BuildLunarListRow(CachedFilteredLunar[i]);
	}
	VisibleLunarCount = EndIndex;
	if (LoadMoreButton)
	{
		LoadMoreButton->SetVisibility(VisibleLunarCount < CachedFilteredLunar.Num() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}
void UEclipseBrowserWidget::OnLoadMoreClicked()
{
	if (CurrentMode == ECatalogMode::Lunar)
	{
		RenderNextLunarBatch();
	}
	else
	{
		RenderNextBatch();
	}
	if (EclipseListScroll)
	{
		EclipseListScroll->ScrollWidgetIntoView(EclipseListScroll->GetChildAt(EclipseListScroll->GetChildrenCount() - 1));
	}
}
void UEclipseBrowserWidget::RequestJumpToTime(int32 JumpDay, int32 JumpMonth, int32 JumpYear, float JumpHour, bool bCloseWidget)
{
	UWorld* World = GetWorld();
	if (!World) return;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(World, AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->SetSimulationDateTime(JumpDay, JumpMonth, JumpYear, JumpHour);
		if (bCloseWidget)
		{
			OnCloseClicked();
		}
	}
}
void ULunarJumpButton::HandleClick()
{
	if (bIsDetailsButton && ParentBrowser)
	{
		ParentBrowser->ShowLunarEclipseDetails(LunarDetailsData);
		return;
	}
	if (ParentBrowser)
	{
		if (bIsSarosButton)
		{
			ParentBrowser->ApplySarosFilter(TargetSaros);
		}
		else
		{
			ParentBrowser->RequestJumpToTime(TargetDay, TargetMonth, TargetYear, TargetHour, true);
			ParentBrowser->TeleportToExactLocation(TargetLat, TargetLon);
		}
	}
}
void UEclipseJumpButton::HandleClick()
{
	if (bIsDetailsButton && ParentBrowser)
	{
		ParentBrowser->ShowEclipseDetails(DetailsData);
		return;
	}
	if (ParentBrowser)
	{
		if (bIsSarosButton)
		{
			ParentBrowser->ApplySarosFilter(TargetSaros);
		}
		else
		{
			ParentBrowser->RequestJumpToTime(TargetDay, TargetMonth, TargetYear, TargetHour, true);
			if (bUseExactCoordinates)
			{
				ParentBrowser->TeleportToExactLocation(TargetLat, TargetLon);
			}
			else
			{
				ParentBrowser->TeleportToEclipseLocation(TargetLat, TargetLon);
			}
		}
	}
}
void UMainMenuWidget::OpenOverlayMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	UOverlayMenuWidget* OverlayMenu = CreateWidget<UOverlayMenuWidget>(PC, UOverlayMenuWidget::StaticClass());
	if (OverlayMenu)
	{
		OverlayMenu->AddToViewport(100);
		ToggleMenu();
	}
}
void UMainMenuWidget::OpenPlacementMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	UPlacementMenuWidget* PlacementMenu = CreateWidget<UPlacementMenuWidget>(PC, UPlacementMenuWidget::StaticClass());
	if (PlacementMenu)
	{
		PlacementMenu->AddToViewport(100);
		ToggleMenu();
	}
}
void UMainMenuWidget::OpenOpticsMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	UOpticsMenuWidget* OpticsMenu = CreateWidget<UOpticsMenuWidget>(PC, UOpticsMenuWidget::StaticClass());
	if (OpticsMenu)
	{
		OpticsMenu->AddToViewport(100);
		ToggleMenu();
	}
}
UPlacementMenuWidget::UPlacementMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	MapZoom = 1.0f;
	MapPan = FVector2D(0.f, 0.f);
	bIsPanning = false;
	bHasMarker = false;
	static ConstructorHelpers::FObjectFinder<UTexture2D> MapTexObj(TEXT("/Game/Textures/Pamant2D.Pamant2D"));
	if (MapTexObj.Succeeded())
	{
		WorldMapTexture = MapTexObj.Object;
	}
}
TSharedRef<SWidget> UPlacementMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UOverlay* MainOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UCanvasPanelSlot* MainOverlaySlot = RootCanvas->AddChildToCanvas(MainOverlay);
	MainOverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	MainOverlaySlot->SetOffsets(FMargin(0, 0, 0, 0));
	UBorder* FullScreenBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FullScreenBg->SetBrushColor(FLinearColor(0.0f, 0.01f, 0.03f, 1.0f));
	UOverlaySlot* BgSlot = MainOverlay->AddChildToOverlay(FullScreenBg);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* VBoxSlot = MainOverlay->AddChildToOverlay(MainVBox);
	VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	VBoxSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlay* MapCenterOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UVerticalBoxSlot* MapSlot = MainVBox->AddChildToVerticalBox(MapCenterOverlay);
	MapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MapSlot->SetPadding(FMargin(20.0f, 35.0f, 20.0f, 5.0f));
	UScaleBox* MapScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	MapScaleBox->SetStretch(EStretch::ScaleToFit);
	UOverlaySlot* ScaleSlot = MapCenterOverlay->AddChildToOverlay(MapScaleBox);
	ScaleSlot->SetHorizontalAlignment(HAlign_Center);
	ScaleSlot->SetVerticalAlignment(VAlign_Center);
	USizeBox* MapSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MapSizeBox->SetWidthOverride(2048.0f);
	MapSizeBox->SetHeightOverride(1024.0f);
	MapScaleBox->SetContent(MapSizeBox);
	MapClippingBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MapClippingBorder->SetBrushColor(FLinearColor(0.01f, 0.03f, 0.08f, 1.0f));
	MapClippingBorder->SetClipping(EWidgetClipping::ClipToBounds);
	MapSizeBox->SetContent(MapClippingBorder);
	MapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (WorldMapTexture) MapImage->SetBrushFromTexture(WorldMapTexture);
	MapImage->SetRenderTransformPivot(FVector2D(0.f, 0.f));
	MapClippingBorder->SetContent(MapImage);
	UTextBlock* MapHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MapHint->SetText(AOrbitalManager::Tr(
		TEXT("Click Stanga = Alege Locatie | Click Dreapta = Muta Harta | Rotita = Zoom"),
		TEXT("Left Click = Pick Location | Right Click = Pan Map | Scroll = Zoom")));
	MapHint->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	MapHint->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)));
	UOverlaySlot* HintSlot = MapCenterOverlay->AddChildToOverlay(MapHint);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetVerticalAlignment(VAlign_Bottom);
	HintSlot->SetPadding(FMargin(0, 0, 0, 8.f));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor(0.85f, 0.15f, 0.15f, 1.0f));
	UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseTxt->SetText(FText::FromString(" X "));
	CloseTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
	CloseButton->AddChild(CloseTxt);
	CloseButton->OnClicked.AddDynamic(this, &UPlacementMenuWidget::OnCloseClicked);
	UOverlaySlot* CloseSlotInfo = MainOverlay->AddChildToOverlay(CloseButton);
	CloseSlotInfo->SetHorizontalAlignment(HAlign_Right);
	CloseSlotInfo->SetVerticalAlignment(VAlign_Top);
	CloseSlotInfo->SetPadding(FMargin(25.f, 25.f));
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	TextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	FSlateBrush InputBgBrush;
	InputBgBrush.TintColor = FSlateColor(FLinearColor(0.07f, 0.07f, 0.10f, 1.0f));
	TextBoxStyle.BackgroundImageNormal = InputBgBrush;
	TextBoxStyle.BackgroundImageHovered = InputBgBrush;
	TextBoxStyle.BackgroundImageFocused = InputBgBrush;
	TextBoxStyle.BackgroundImageReadOnly = InputBgBrush;
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
	auto MakeCardBorder = [&]() -> UBorder* {
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.07f, 0.95f));
		B->SetPadding(FMargin(16.f, 12.f));
		return B;
		};
	UBorder* BottomPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BottomPanel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.98f));
	BottomPanel->SetPadding(FMargin(25.0f, 15.0f, 25.0f, 15.0f));
	UVerticalBoxSlot* BottomSlotInfo = MainVBox->AddChildToVerticalBox(BottomPanel);
	if (BottomSlotInfo) BottomSlotInfo->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UVerticalBox* BottomVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BottomPanel->SetContent(BottomVBox);
	UHorizontalBox* ColumnsHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* ColsSlot = BottomVBox->AddChildToVerticalBox(ColumnsHBox);
	if (ColsSlot) ColsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	UBorder* Card1 = MakeCardBorder();
	UHorizontalBoxSlot* Card1Slot = ColumnsHBox->AddChildToHorizontalBox(Card1);
	Card1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card1Slot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	UVerticalBox* Col1 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card1->SetContent(Col1);
	UTextBlock* Title1 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title1->SetText(AOrbitalManager::Tr(TEXT("CAUTARE ORAS"), TEXT("CITY SEARCH")));
	Title1->SetFont(TitleFont);
	Title1->SetColorAndOpacity(FLinearColor(0.2f, 0.75f, 1.0f));
	Col1->AddChildToVerticalBox(Title1)->SetPadding(FMargin(0, 0, 0, 8.0f));
	CitySearchInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	CitySearchInput->SetWidgetStyle(TextBoxStyle);
	CitySearchInput->SetHintText(AOrbitalManager::Tr(TEXT("Scrie nume oras... (ex: New York)"), TEXT("Type city name... (e.g. New York)")));
	CitySearchInput->OnTextChanged.AddDynamic(this, &UPlacementMenuWidget::OnCitySearchChanged);
	Col1->AddChildToVerticalBox(CitySearchInput)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	CityResultsComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	CityResultsComboBox->SetWidgetStyle(ComboStyle);
	CityResultsComboBox->ItemStyle = ComboRowStyle;
	CityResultsComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("Selecteaza din rezultate"), TEXT("Select from results")));
	CityResultsComboBox->SetSelectedIndex(0);
	CityResultsComboBox->OnSelectionChanged.AddDynamic(this, &UPlacementMenuWidget::OnCitySelected);
	Col1->AddChildToVerticalBox(CityResultsComboBox);
	UBorder* Card2 = MakeCardBorder();
	UHorizontalBoxSlot* Card2Slot = ColumnsHBox->AddChildToHorizontalBox(Card2);
	Card2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card2Slot->SetPadding(FMargin(5.f, 0.f, 5.f, 0.f));
	UVerticalBox* Col2 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card2->SetContent(Col2);
	UTextBlock* Title2 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title2->SetText(AOrbitalManager::Tr(TEXT("COORDONATE GPS"), TEXT("GPS COORDINATES")));
	Title2->SetFont(TitleFont);
	Title2->SetColorAndOpacity(FLinearColor(0.4f, 0.9f, 0.5f));
	Col2->AddChildToVerticalBox(Title2)->SetPadding(FMargin(0, 0, 0, 8.0f));
	auto CreateCoordRow = [&](FString LabelRO, FString LabelEN, UComboBoxString*& OutDir, FString Dir1, FString Dir2, UEditableTextBox*& OutD, UEditableTextBox*& OutM, UEditableTextBox*& OutS)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
			Lbl->SetFont(LabelFont);
			UHorizontalBoxSlot* LblSlot = Row->AddChildToHorizontalBox(Lbl);
			LblSlot->SetVerticalAlignment(VAlign_Center);
			LblSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			OutDir = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
			OutDir->SetWidgetStyle(ComboStyle);
			OutDir->ItemStyle = ComboRowStyle;
			OutDir->ForegroundColor = FSlateColor(FLinearColor::White);
			OutDir->AddOption(Dir1); OutDir->AddOption(Dir2); OutDir->SetSelectedOption(Dir1);
			UHorizontalBoxSlot* DirSlot = Row->AddChildToHorizontalBox(OutDir);
			DirSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			OutD = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			OutD->SetWidgetStyle(TextBoxStyle);
			OutD->SetHintText(AOrbitalManager::Tr(TEXT("Grade"), TEXT("Deg")));
			OutD->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* DSlot = Row->AddChildToHorizontalBox(OutD);
			DSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			DSlot->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			OutM = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			OutM->SetWidgetStyle(TextBoxStyle);
			OutM->SetHintText(AOrbitalManager::Tr(TEXT("Min"), TEXT("Min")));
			OutM->SetVisibility(ESlateVisibility::Collapsed);
			OutM->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* MSlot = Row->AddChildToHorizontalBox(OutM);
			MSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			MSlot->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			OutS = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			OutS->SetWidgetStyle(TextBoxStyle);
			OutS->SetHintText(AOrbitalManager::Tr(TEXT("Sec"), TEXT("Sec")));
			OutS->SetVisibility(ESlateVisibility::Collapsed);
			OutS->SetJustification(ETextJustify::Center);
			UHorizontalBoxSlot* SSlot = Row->AddChildToHorizontalBox(OutS);
			SSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			return Row;
		};
	UHorizontalBox* LatRow = CreateCoordRow(TEXT("Lat:"), TEXT("Lat:"), LatDirCombo, "N", "S", LatDegInput, LatMinInput, LatSecInput);
	Col2->AddChildToVerticalBox(LatRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	UHorizontalBox* LonRow = CreateCoordRow(TEXT("Lon:"), TEXT("Lon:"), LonDirCombo, "E", "W", LonDegInput, LonMinInput, LonSecInput);
	Col2->AddChildToVerticalBox(LonRow);
	UBorder* Card3 = MakeCardBorder();
	UHorizontalBoxSlot* Card3Slot = ColumnsHBox->AddChildToHorizontalBox(Card3);
	Card3Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card3Slot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	UVerticalBox* Col3 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card3->SetContent(Col3);
	UTextBlock* Title3 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title3->SetText(AOrbitalManager::Tr(TEXT("FORMAT & ALTITUDINE"), TEXT("FORMAT & ALTITUDE")));
	Title3->SetFont(TitleFont);
	Title3->SetColorAndOpacity(FLinearColor(1.0f, 0.65f, 0.1f));
	Col3->AddChildToVerticalBox(Title3)->SetPadding(FMargin(0, 0, 0, 8.0f));
	UHorizontalBox* FormatBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* FmtLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FmtLbl->SetText(AOrbitalManager::Tr(TEXT("Format:"), TEXT("Format:")));
	FmtLbl->SetFont(LabelFont);
	UHorizontalBoxSlot* FmtLblSlot = FormatBox->AddChildToHorizontalBox(FmtLbl);
	FmtLblSlot->SetVerticalAlignment(VAlign_Center);
	FmtLblSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	FormatComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	FormatComboBox->SetWidgetStyle(ComboStyle);
	FormatComboBox->ItemStyle = ComboRowStyle;
	FormatComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	FormatComboBox->AddOption(AOrbitalManager::TrString(TEXT("DD (Grade Zecimale)"), TEXT("DD (Decimal Deg)")));
	FormatComboBox->AddOption(AOrbitalManager::TrString(TEXT("DMM (Grade, Min Zecimale)"), TEXT("DMM (Deg, Dec Min)")));
	FormatComboBox->AddOption(AOrbitalManager::TrString(TEXT("DMS (Grade, Min, Sec)"), TEXT("DMS (Deg, Min, Sec)")));
	FormatComboBox->SetSelectedIndex(0);
	FormatComboBox->OnSelectionChanged.AddDynamic(this, &UPlacementMenuWidget::OnFormatChanged);
	UHorizontalBoxSlot* FormatComboSlot = FormatBox->AddChildToHorizontalBox(FormatComboBox);
	FormatComboSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Col3->AddChildToVerticalBox(FormatBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	UHorizontalBox* AltRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* AltLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	AltLbl->SetText(AOrbitalManager::Tr(TEXT("Alt (m):"), TEXT("Alt (m):")));
	AltLbl->SetFont(LabelFont);
	UHorizontalBoxSlot* AltLblSlot = AltRow->AddChildToHorizontalBox(AltLbl);
	AltLblSlot->SetVerticalAlignment(VAlign_Center);
	AltLblSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	AltInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	AltInput->SetWidgetStyle(TextBoxStyle);
	AltInput->SetText(FText::FromString("0"));
	AltInput->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* AltInputSlot = AltRow->AddChildToHorizontalBox(AltInput);
	AltInputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Col3->AddChildToVerticalBox(AltRow);
	UHorizontalBox* ActionBtnsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BottomVBox->AddChildToVerticalBox(ActionBtnsBox);
	CurrentLocationButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CurrentLocationButton->SetBackgroundColor(FLinearColor(0.08f, 0.28f, 0.48f, 1.0f));
	UTextBlock* CurrTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CurrTxt->SetText(AOrbitalManager::Tr(TEXT("PREIA LOCATIA CURENTA"), TEXT("GET CURRENT LOCATION")));
	CurrTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	CurrTxt->SetMargin(FMargin(0.f, 8.f));
	CurrTxt->SetJustification(ETextJustify::Center);
	CurrentLocationButton->AddChild(CurrTxt);
	CurrentLocationButton->OnClicked.AddDynamic(this, &UPlacementMenuWidget::OnCurrentLocationClicked);
	ActionBtnsBox->AddChildToHorizontalBox(CurrentLocationButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USpacer* Sp1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Sp1->SetSize(FVector2D(12.f, 0.f));
	ActionBtnsBox->AddChildToHorizontalBox(Sp1)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	EclipseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	EclipseButton->SetBackgroundColor(FLinearColor(0.42f, 0.12f, 0.58f, 1.0f));
	UTextBlock* EclipseTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	EclipseTxt->SetText(AOrbitalManager::Tr(TEXT("TELEPORTARE IN UMBRA"), TEXT("TELEPORT TO SHADOW")));
	EclipseTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	EclipseTxt->SetMargin(FMargin(0.f, 8.f));
	EclipseTxt->SetJustification(ETextJustify::Center);
	EclipseButton->AddChild(EclipseTxt);
	EclipseButton->OnClicked.AddDynamic(this, &UPlacementMenuWidget::OnTeleportToEclipseClicked);
	ActionBtnsBox->AddChildToHorizontalBox(EclipseButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Sp2->SetSize(FVector2D(12.f, 0.f));
	ActionBtnsBox->AddChildToHorizontalBox(Sp2)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	TeleportButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	TeleportButton->SetBackgroundColor(FLinearColor(0.12f, 0.50f, 0.18f, 1.0f));
	UTextBlock* TeleTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TeleTxt->SetText(AOrbitalManager::Tr(TEXT("APLICA COORDONATE"), TEXT("APPLY COORDINATES")));
	TeleTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	TeleTxt->SetMargin(FMargin(0.f, 8.f));
	TeleTxt->SetJustification(ETextJustify::Center);
	TeleportButton->AddChild(TeleTxt);
	TeleportButton->OnClicked.AddDynamic(this, &UPlacementMenuWidget::OnTeleportClicked);
	ActionBtnsBox->AddChildToHorizontalBox(TeleportButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Super::RebuildWidget();
}
void UPlacementMenuWidget::OnCitySearchChanged(const FText& Text)
{
	if (!CityResultsComboBox) return;
	CityResultsComboBox->ClearOptions();
	FoundCitiesMap.Empty();
	FString Query = Text.ToString().TrimStartAndEnd();
	if (Query.Len() < 2)
	{
		CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("-- Scrie minim 2 caractere --"), TEXT("-- Type at least 2 chars --")));
		CityResultsComboBox->SetSelectedIndex(0);
		return;
	}
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager) return;
	if (Manager->CountryPoints.Num() == 0)
	{
		Manager->LoadCountryPoints();
	}
	int32 MatchesCount = 0;
	constexpr int32 MaxResults = 35;
	for (const auto& Pair : Manager->CountryPoints)
	{
		const FString& CountryName = Pair.Key;
		for (const FCountryPoint& P : Pair.Value)
		{
			if (P.CityName.Contains(Query, ESearchCase::IgnoreCase) || CountryName.Contains(Query, ESearchCase::IgnoreCase))
			{
				FString DisplayLabel2 = FString::Printf(TEXT("%s (%s)"), *P.CityName, *CountryName);
				FEclipseCityHit Hit;
				Hit.CityName = P.CityName;
				Hit.Country = CountryName;
				Hit.Latitude = P.Lat;
				Hit.Longitude = P.Lon;
				Hit.Altitude = P.Alt;
				FoundCitiesMap.Add(DisplayLabel2, Hit);
				CityResultsComboBox->AddOption(DisplayLabel2);
				MatchesCount++;
				if (MatchesCount >= MaxResults) break;
			}
		}
		if (MatchesCount >= MaxResults) break;
	}
	if (MatchesCount == 0)
	{
		CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("Niciun oras gasit"), TEXT("No cities found")));
	}
	CityResultsComboBox->SetSelectedIndex(0);
}
void UPlacementMenuWidget::OnCitySelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (const FEclipseCityHit* FoundCity = FoundCitiesMap.Find(SelectedItem))
	{
		ApplyCityCoordinates(*FoundCity);
	}
}
void UPlacementMenuWidget::ApplyCityCoordinates(const FEclipseCityHit& City)
{
	if (AltInput)
	{
		AltInput->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FMath::Max(0.0f, City.Altitude))));
	}
	if (LatDirCombo && LatDegInput)
	{
		LatDirCombo->SetSelectedOption(City.Latitude >= 0.0f ? TEXT("N") : TEXT("S"));
		LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), FMath::Abs(City.Latitude))));
		if (LatMinInput) LatMinInput->SetText(FText::FromString(TEXT("0")));
		if (LatSecInput) LatSecInput->SetText(FText::FromString(TEXT("0")));
	}
	if (LonDirCombo && LonDegInput)
	{
		LonDirCombo->SetSelectedOption(City.Longitude >= 0.0f ? TEXT("E") : TEXT("W"));
		LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), FMath::Abs(City.Longitude))));
		if (LonMinInput) LonMinInput->SetText(FText::FromString(TEXT("0")));
		if (LonSecInput) LonSecInput->SetText(FText::FromString(TEXT("0")));
	}
	if (FormatComboBox)
	{
		OnFormatChanged(FormatComboBox->GetSelectedOption(), ESelectInfo::Direct);
	}
}
void UPlacementMenuWidget::OnCurrentLocationClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!MyDrone || !GeoRef) return;
	FVector UnrealLoc = MyDrone->GetActorLocation();
	FVector LLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(UnrealLoc);
	double Lon = LLH.X;
	double Lat = LLH.Y;
	double Alt = LLH.Z;
	if (LatDirCombo) LatDirCombo->SetSelectedOption(Lat >= 0 ? "N" : "S");
	if (LonDirCombo) LonDirCombo->SetSelectedOption(Lon >= 0 ? "E" : "W");
	double AbsLat = FMath::Abs(Lat);
	double AbsLon = FMath::Abs(Lon);
	int32 LatD = FMath::FloorToInt(AbsLat);
	double LatRemainM = (AbsLat - LatD) * 60.0;
	int32 LatM = FMath::FloorToInt(LatRemainM);
	double LatS = (LatRemainM - LatM) * 60.0;
	int32 LonD = FMath::FloorToInt(AbsLon);
	double LonRemainM = (AbsLon - LonD) * 60.0;
	int32 LonM = FMath::FloorToInt(LonRemainM);
	double LonS = (LonRemainM - LonM) * 60.0;
	FString CurrentFormat = FormatComboBox ? FormatComboBox->GetSelectedOption() : "DD";
	if (CurrentFormat.Contains("DMS"))
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatD)));
		if (LatMinInput) LatMinInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatM)));
		if (LatSecInput) LatSecInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), LatS)));
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonD)));
		if (LonMinInput) LonMinInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonM)));
		if (LonSecInput) LonSecInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), LonS)));
	}
	else if (CurrentFormat.Contains("DMM"))
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatD)));
		if (LatMinInput) LatMinInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LatRemainM)));
		if (LatSecInput) LatSecInput->SetText(FText::GetEmpty());
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonD)));
		if (LonMinInput) LonMinInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LonRemainM)));
		if (LonSecInput) LonSecInput->SetText(FText::GetEmpty());
	}
	else
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), AbsLat)));
		if (LatMinInput) LatMinInput->SetText(FText::GetEmpty());
		if (LatSecInput) LatSecInput->SetText(FText::GetEmpty());
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), AbsLon)));
		if (LonMinInput) LonMinInput->SetText(FText::GetEmpty());
		if (LonSecInput) LonSecInput->SetText(FText::GetEmpty());
	}
	if (AltInput) AltInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Alt)));
}
void UPlacementMenuWidget::OnFormatChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	ESlateVisibility MinVis = ESlateVisibility::Collapsed;
	ESlateVisibility SecVis = ESlateVisibility::Collapsed;
	if (SelectedItem.Contains("DMM")) {
		MinVis = ESlateVisibility::Visible;
	}
	else if (SelectedItem.Contains("DMS")) {
		MinVis = ESlateVisibility::Visible;
		SecVis = ESlateVisibility::Visible;
	}
	if (LatMinInput) LatMinInput->SetVisibility(MinVis);
	if (LonMinInput) LonMinInput->SetVisibility(MinVis);
	if (LatSecInput) LatSecInput->SetVisibility(SecVis);
	if (LonSecInput) LonSecInput->SetVisibility(SecVis);
}
void UPlacementMenuWidget::OnCloseClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
void UPlacementMenuWidget::OnTeleportToEclipseClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("OrbitalManager nu a fost gasit!"));
		return;
	}
	double Lon = Manager->LiveUmbraCenterLLH.X;
	double Lat = Manager->LiveUmbraCenterLLH.Y;
	double Alt = Manager->LiveUmbraCenterLLH.Z;
	if (FormatComboBox) FormatComboBox->SetSelectedOption("DD (Grade Zecimale)");
	OnFormatChanged("DD (Grade Zecimale)", ESelectInfo::Direct);
	if (LatDirCombo) LatDirCombo->SetSelectedOption(Lat >= 0 ? "N" : "S");
	if (LonDirCombo) LonDirCombo->SetSelectedOption(Lon >= 0 ? "E" : "W");
	if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), FMath::Abs(Lat))));
	if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), FMath::Abs(Lon))));
	double FinalAlt = Manager->bIsUmbraOnGround ? 5000.0 : Alt;
	if (AltInput) AltInput->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), FinalAlt)));
	OnTeleportClicked();
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Magenta, TEXT("Saritura efectuata direct in conul de umbra!"));
}
void UPlacementMenuWidget::OnTeleportClicked()
{
	auto GetVal = [](UEditableTextBox* Box) -> float {
		if (!Box || Box->GetText().IsEmpty()) return 0.0f;
		return FCString::Atof(*Box->GetText().ToString());
		};
	float LatD = GetVal(LatDegInput);
	float LatM = GetVal(LatMinInput);
	float LatS = GetVal(LatSecInput);
	float LonD = GetVal(LonDegInput);
	float LonM = GetVal(LonMinInput);
	float LonS = GetVal(LonSecInput);
	float Alt = GetVal(AltInput);
	float FinalLat = LatD + (LatM / 60.0f) + (LatS / 3600.0f);
	float FinalLon = LonD + (LonM / 60.0f) + (LonS / 3600.0f);
	if (LatDirCombo && LatDirCombo->GetSelectedOption() == "S") FinalLat *= -1.0f;
	if (LonDirCombo && LonDirCombo->GetSelectedOption() == "W") FinalLon *= -1.0f;
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	if (!MyDrone)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("EROARE: Nu s-a putut accesa ADronaMea!"));
		return;
	}
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (!GeoRef)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("EROARE: CesiumGeoreference nu e in nivel!"));
		return;
	}
	MyDrone->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	FVector GroundLLH(FinalLon, FinalLat, 0.0f);
	GeoRef->SetOriginLongitudeLatitudeHeight(GroundLLH);
	MyDrone->SetActorLocation(FVector(0.0f, 0.0f, 1500000.0f));
	MyDrone->CurrentVelocity = FVector::ZeroVector;
	if (MyDrone->EclipseInfoWidget)
	{
		if (!MyDrone->EclipseInfoWidget->IsInViewport())
		{
			MyDrone->EclipseInfoWidget->AddToViewport();
		}
		MyDrone->EclipseInfoWidget->SetVisibility(ESlateVisibility::Visible);
		MyDrone->EclipseInfoWidget->SetReportText("Se descarca relieful...\nTe rog asteapta 2 secunde...");
	}
	float RequestedZ = Alt * 100.0f;
	FTimerDelegate TimerDel;
	TimerDel.BindUObject(this, &UPlacementMenuWidget::FinalizeTeleport, RequestedZ, MyDrone);
	GetWorld()->GetTimerManager().SetTimer(TeleportTimerHandle, TimerDel, 2.0f, false);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, TEXT("Teleportare initiata... Se asteapta generarea terenului."));
	OnCloseClicked();
}
void UPlacementMenuWidget::FinalizeTeleport(float RequestedZ, ADronaMea* MyDrone)
{
	if (!MyDrone || !GetWorld()) return;
	float FinalZ = RequestedZ;
	FVector TraceStart(0.0f, 0.0f, 1500000.0f);
	FVector TraceEnd(0.0f, 0.0f, -200000.0f);
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(MyDrone);
	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		float GroundZ = HitResult.Location.Z;
		if (RequestedZ < GroundZ)
		{
			FinalZ = GroundZ + 5000.0f;
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Orange, TEXT("Altitudine ajustata pentru siguranta."));
		}
	}
	else
	{
		if (RequestedZ < 0.0f) FinalZ = 5000.0f;
	}
	MyDrone->SetActorLocation(FVector(0.0f, 0.0f, FinalZ));
	MyDrone->CurrentVelocity = FVector::ZeroVector;
	if (MyDrone->EclipseInfoWidget)
	{
		MyDrone->EclipseInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Teren incarcat. Pozitie finala setata!"));
}
void UPlacementMenuWidget::UpdateInputsFromMarker()
{
	double Lat = MarkerLatLon.Y;
	double Lon = MarkerLatLon.X;
	if (LatDirCombo) LatDirCombo->SetSelectedOption(Lat >= 0 ? "N" : "S");
	if (LonDirCombo) LonDirCombo->SetSelectedOption(Lon >= 0 ? "E" : "W");
	double AbsLat = FMath::Abs(Lat);
	double AbsLon = FMath::Abs(Lon);
	int32 LatD = FMath::FloorToInt(AbsLat);
	double LatRemainM = (AbsLat - LatD) * 60.0;
	int32 LatM = FMath::FloorToInt(LatRemainM);
	double LatS = (LatRemainM - LatM) * 60.0;
	int32 LonD = FMath::FloorToInt(AbsLon);
	double LonRemainM = (AbsLon - LonD) * 60.0;
	int32 LonM = FMath::FloorToInt(LonRemainM);
	double LonS = (LonRemainM - LonM) * 60.0;
	FString CurrentFormat = FormatComboBox ? FormatComboBox->GetSelectedOption() : "DD";
	if (CurrentFormat.Contains("DMS"))
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatD)));
		if (LatMinInput) LatMinInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatM)));
		if (LatSecInput) LatSecInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), LatS)));
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonD)));
		if (LonMinInput) LonMinInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonM)));
		if (LonSecInput) LonSecInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), LonS)));
	}
	else if (CurrentFormat.Contains("DMM"))
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LatD)));
		if (LatMinInput) LatMinInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LatRemainM)));
		if (LatSecInput) LatSecInput->SetText(FText::GetEmpty());
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), LonD)));
		if (LonMinInput) LonMinInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), LonRemainM)));
		if (LonSecInput) LonSecInput->SetText(FText::GetEmpty());
	}
	else
	{
		if (LatDegInput) LatDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), AbsLat)));
		if (LatMinInput) LatMinInput->SetText(FText::GetEmpty());
		if (LatSecInput) LatSecInput->SetText(FText::GetEmpty());
		if (LonDegInput) LonDegInput->SetText(FText::FromString(FString::Printf(TEXT("%.6f"), AbsLon)));
		if (LonMinInput) LonMinInput->SetText(FText::GetEmpty());
		if (LonSecInput) LonSecInput->SetText(FText::GetEmpty());
	}
}
FReply UPlacementMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapClippingBorder && MapClippingBorder->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			bIsPanning = true;
			LastMousePosition = InMouseEvent.GetScreenSpacePosition();
			return FReply::Handled();
		}
		else if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FVector2D LocalPos = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			FVector2D MapSize = MapClippingBorder->GetCachedGeometry().GetLocalSize();
			FVector2D UnzoomedPos = (LocalPos - MapPan) / MapZoom;
			float U = FMath::Clamp(UnzoomedPos.X / MapSize.X, 0.0f, 1.0f);
			float V = FMath::Clamp(UnzoomedPos.Y / MapSize.Y, 0.0f, 1.0f);
			MarkerLatLon.Y = 90.0f - (V * 180.0f);
			MarkerLatLon.X = (U * 360.0f) - 180.0f;
			bHasMarker = true;
			UpdateInputsFromMarker();
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
FReply UPlacementMenuWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsPanning && MapClippingBorder)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D LocalCurrent = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(CurrentMousePos);
		FVector2D LocalLast = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(LastMousePosition);
		MapPan += (LocalCurrent - LocalLast);
		LastMousePosition = CurrentMousePos;
		FVector2D MapSize = MapClippingBorder->GetCachedGeometry().GetLocalSize();
		float MinPanX = -MapSize.X * (MapZoom - 1.0f);
		float MinPanY = -MapSize.Y * (MapZoom - 1.0f);
		MapPan.X = FMath::Clamp(MapPan.X, MinPanX, 0.0f);
		MapPan.Y = FMath::Clamp(MapPan.Y, MinPanY, 0.0f);
		if (MapImage) MapImage->SetRenderTranslation(MapPan);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
FReply UPlacementMenuWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bIsPanning = false;
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
FReply UPlacementMenuWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapClippingBorder && MapClippingBorder->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		float ZoomDelta = InMouseEvent.GetWheelDelta() * 0.15f;
		float OldZoom = MapZoom;
		MapZoom = FMath::Clamp(MapZoom + ZoomDelta, 1.0f, 20.0f);
		FVector2D LocalMouse = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		MapPan = LocalMouse - (LocalMouse - MapPan) * (MapZoom / OldZoom);
		if (MapZoom == 1.0f)
		{
			MapPan = FVector2D::ZeroVector;
		}
		else
		{
			FVector2D MapSize = MapClippingBorder->GetCachedGeometry().GetLocalSize();
			float MinPanX = -MapSize.X * (MapZoom - 1.0f);
			float MinPanY = -MapSize.Y * (MapZoom - 1.0f);
			MapPan.X = FMath::Clamp(MapPan.X, MinPanX, 0.0f);
			MapPan.Y = FMath::Clamp(MapPan.Y, MinPanY, 0.0f);
		}
		if (MapImage)
		{
			MapImage->SetRenderScale(FVector2D(MapZoom));
			MapImage->SetRenderTranslation(MapPan);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
int32 UPlacementMenuWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bHasMarker && MapClippingBorder)
	{
		FGeometry MapGeo = MapClippingBorder->GetCachedGeometry();
		FVector2D MapSize = MapGeo.GetLocalSize();
		float U = (MarkerLatLon.X + 180.0f) / 360.0f;
		float V = (90.0f - MarkerLatLon.Y) / 180.0f;
		FVector2D UnzoomedPos(U * MapSize.X, V * MapSize.Y);
		FVector2D FinalLocalPos = (UnzoomedPos * MapZoom) + MapPan;
		if (FinalLocalPos.X >= 0 && FinalLocalPos.X <= MapSize.X && FinalLocalPos.Y >= 0 && FinalLocalPos.Y <= MapSize.Y)
		{
			float CrossSize = 10.0f;
			FVector2D AbsolutePos = MapGeo.LocalToAbsolute(FinalLocalPos);
			FVector2D PaintPos = AllottedGeometry.AbsoluteToLocal(AbsolutePos);
			TArray<FVector2D> P1, P2;
			P1.Add(PaintPos + FVector2D(-CrossSize, -CrossSize)); P1.Add(PaintPos + FVector2D(CrossSize, CrossSize));
			P2.Add(PaintPos + FVector2D(-CrossSize, CrossSize)); P2.Add(PaintPos + FVector2D(CrossSize, -CrossSize));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), P1, ESlateDrawEffect::None, FLinearColor::Red, true, 3.0f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), P2, ESlateDrawEffect::None, FLinearColor::Red, true, 3.0f);
		}
	}
	return LayerId + 1;
}
bool  UTimeControlWidget::IsLeapYearCustom(int32 Year)
{
	if (Year % 4 != 0) return false;
	if (Year % 100 != 0) return true;
	return (Year % 400 == 0);
}
int32  UTimeControlWidget::GetDaysInMonthCustom(int32 Month, int32 Year)
{
	static const int32 DaysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (Month < 1 || Month > 12) return 30;
	if (Month == 2 && IsLeapYearCustom(Year))
	{
		return 29;
	}
	return DaysPerMonth[Month - 1];
}
void UMainMenuWidget::OpenTimeMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	UTimeControlWidget* TimeMenu = CreateWidget<UTimeControlWidget>(PC, UTimeControlWidget::StaticClass());
	if (TimeMenu)
	{
		TimeMenu->AddToViewport(100);
		ToggleMenu();
	}
}
void UCalendarDayButton::HandleClick()
{
	if (ParentMenu)
	{
		ParentMenu->SelectDay(DayNumber);
	}
}
TSharedRef<SWidget> UTimeControlWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.95f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(450.0f, 700.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("CONTROL TIMP"), TEXT("TIME CONTROL")));
	Title->SetFont(GetFont(14, true));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(GetFont(14, true));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.2f, 0.2f)));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UTimeControlWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	TextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	FSlateBrush BgBrush;
	BgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
	TextBoxStyle.BackgroundImageNormal = BgBrush;
	TextBoxStyle.BackgroundImageHovered = BgBrush;
	TextBoxStyle.BackgroundImageFocused = BgBrush;
	TextBoxStyle.BackgroundImageReadOnly = BgBrush;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	FDateTime Now = FDateTime::Now();
	float CurrentSimHour = Now.GetHour();
	if (Manager)
	{
		CurrentSelectedYear = Manager->CurrentDate.Year;
		CurrentSelectedMonth = Manager->CurrentDate.Month;
		CurrentSelectedDay = Manager->CurrentDate.Day;
		CurrentSimHour = Manager->CurrentDate.Hour;
	}
	else
	{
		CurrentSelectedYear = Now.GetYear();
		CurrentSelectedMonth = Now.GetMonth();
		CurrentSelectedDay = Now.GetDay();
	}
	UHorizontalBox* MonthBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(MonthBox)->SetPadding(FMargin(20.f, 10.f, 20.f, 10.f));
	PrevMonthBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	PrevMonthBtn->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.22f));
	UTextBlock* PrevTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PrevTxt->SetText(FText::FromString("  <  "));
	PrevMonthBtn->AddChild(PrevTxt);
	PrevMonthBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnPrevMonthClicked);
	MonthBox->AddChildToHorizontalBox(PrevMonthBtn);
	MonthYearLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MonthYearLabel->SetJustification(ETextJustify::Center);
	MonthYearLabel->SetFont(GetFont(16, true));
	UHorizontalBoxSlot* MYSlot = MonthBox->AddChildToHorizontalBox(MonthYearLabel);
	MYSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MYSlot->SetVerticalAlignment(VAlign_Center);
	NextMonthBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	NextMonthBtn->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.22f));
	UTextBlock* NextTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NextTxt->SetText(FText::FromString("  >  "));
	NextMonthBtn->AddChild(NextTxt);
	NextMonthBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnNextMonthClicked);
	MonthBox->AddChildToHorizontalBox(NextMonthBtn);
	CalendarGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	CalendarGrid->SetSlotPadding(FMargin(3.f));
	MainLayout->AddChildToVerticalBox(CalendarGrid)->SetPadding(FMargin(20.f, 5.f));
	UBorder* Div1 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Div1->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
	MainLayout->AddChildToVerticalBox(Div1)->SetPadding(FMargin(20.f, 15.f, 20.f, 5.f));
	UVerticalBox* DualTimeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBoxSlot* DualTimeSlot = MainLayout->AddChildToVerticalBox(DualTimeBox);
	DualTimeSlot->SetPadding(FMargin(20.f, 10.f));
	DualTimeSlot->SetHorizontalAlignment(HAlign_Center);
	auto StyleInput = [&](UEditableTextBox* Box, int32 Val, UHorizontalBox* ParentRow, bool bIsYear = false) {
		FString FormattedString = bIsYear ? FString::Printf(TEXT("%04d"), Val) : FString::Printf(TEXT("%02d"), Val);
		Box->SetText(FText::FromString(FormattedString));
		Box->SetJustification(ETextJustify::Center);
		Box->SetWidgetStyle(TextBoxStyle);
		Box->WidgetStyle.TextStyle.Font = GetFont(14, true);
		ParentRow->AddChildToHorizontalBox(Box)->SetPadding(FMargin(3.f, 0.f));
		};
	auto AddSeparator = [&](UHorizontalBox* ParentRow, FString CharStr) {
		UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Sep->SetText(FText::FromString(CharStr));
		Sep->SetFont(GetFont(14, true));
		ParentRow->AddChildToHorizontalBox(Sep)->SetVerticalAlignment(VAlign_Center);
		};
	UHorizontalBox* DateBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DualTimeBox->AddChildToVerticalBox(DateBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UTextBlock* DateLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DateLbl->SetText(AOrbitalManager::Tr(TEXT("DATA:        "), TEXT("DATE:        ")));
	DateLbl->SetFont(GetFont(12, true)); DateLbl->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
	DateBox->AddChildToHorizontalBox(DateLbl)->SetVerticalAlignment(VAlign_Center);
	DayInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(DayInput, CurrentSelectedDay, DateBox);
	AddSeparator(DateBox, "/");
	MonthInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(MonthInput, CurrentSelectedMonth, DateBox);
	AddSeparator(DateBox, "/");
	YearInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(YearInput, CurrentSelectedYear, DateBox, true);
	int32 H_UTC = FMath::FloorToInt(CurrentSimHour);
	int32 M_UTC = FMath::FloorToInt((CurrentSimHour - H_UTC) * 60.0f);
	int32 S_UTC = FMath::FloorToInt((((CurrentSimHour - H_UTC) * 60.0f) - M_UTC) * 60.0f);
	UHorizontalBox* UTCBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DualTimeBox->AddChildToVerticalBox(UTCBox)->SetPadding(FMargin(0.f, 5.f));
	UTextBlock* UTCLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UTCLbl->SetText(AOrbitalManager::Tr(TEXT("ORA (UTC):   "), TEXT("TIME (UTC):  ")));
	UTCLbl->SetFont(GetFont(12, true)); UTCLbl->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
	UTCBox->AddChildToHorizontalBox(UTCLbl)->SetVerticalAlignment(VAlign_Center);
	HourInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(HourInput, H_UTC, UTCBox); AddSeparator(UTCBox, ":");
	MinInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(MinInput, M_UTC, UTCBox); AddSeparator(UTCBox, ":");
	SecInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(SecInput, S_UTC, UTCBox);
	HourInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnUTCInputCommitted);
	MinInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnUTCInputCommitted);
	SecInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnUTCInputCommitted);
	UHorizontalBox* SolarBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DualTimeBox->AddChildToVerticalBox(SolarBox)->SetPadding(FMargin(0.f, 5.f));
	UTextBlock* SolarLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SolarLbl->SetText(AOrbitalManager::Tr(TEXT("ORA (Solar): "), TEXT("TIME (Solar):")));
	SolarLbl->SetFont(GetFont(12, true)); SolarLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.6f, 0.1f)));
	SolarBox->AddChildToHorizontalBox(SolarLbl)->SetVerticalAlignment(VAlign_Center);
	SolarHourInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(SolarHourInput, 0, SolarBox); AddSeparator(SolarBox, ":");
	SolarMinInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(SolarMinInput, 0, SolarBox); AddSeparator(SolarBox, ":");
	SolarSecInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInput(SolarSecInput, 0, SolarBox);
	SolarHourInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnSolarInputCommitted);
	SolarMinInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnSolarInputCommitted);
	SolarSecInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnSolarInputCommitted);
	SyncClocks(true);
	ApplyTimeBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ApplyTimeBtn->SetBackgroundColor(FLinearColor(0.15f, 0.45f, 0.85f));
	UTextBlock* ApplyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ApplyTxt->SetText(AOrbitalManager::Tr(TEXT(" APLICA DATA SI ORA "), TEXT(" APPLY DATE AND TIME ")));
	ApplyTxt->SetJustification(ETextJustify::Center);
	ApplyTxt->SetFont(GetFont(12, true));
	ApplyTxt->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyTimeBtn->AddChild(ApplyTxt);
	ApplyTimeBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnApplyTimeClicked);
	MainLayout->AddChildToVerticalBox(ApplyTimeBtn)->SetPadding(FMargin(50.f, 5.f, 50.f, 15.f));
	UBorder* Div2 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Div2->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 1.f));
	MainLayout->AddChildToVerticalBox(Div2)->SetPadding(FMargin(20.f, 5.f, 20.f, 5.f));
	UHorizontalBox* SpeedBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(SpeedBox)->SetPadding(FMargin(20.f, 15.f, 20.f, 5.f));
	DivideSpeedBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	DivideSpeedBtn->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.22f));
	UTextBlock* DivTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); DivTxt->SetText(FText::FromString(" /2 "));
	DivideSpeedBtn->AddChild(DivTxt);
	DivideSpeedBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnDivideSpeedClicked);
	SpeedBox->AddChildToHorizontalBox(DivideSpeedBtn);
	SpeedInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	SpeedInput->SetWidgetStyle(TextBoxStyle);
	float InitSpeed = Manager ? FMath::Abs(Manager->TimeSpeed) : 1.0f;
	SpeedInput->SetText(FText::FromString(FString::SanitizeFloat(InitSpeed)));
	SpeedInput->SetJustification(ETextJustify::Center);
	SpeedInput->OnTextCommitted.AddDynamic(this, &UTimeControlWidget::OnSpeedInputCommitted);
	UHorizontalBoxSlot* SpdSlot = SpeedBox->AddChildToHorizontalBox(SpeedInput);
	SpdSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SpdSlot->SetPadding(FMargin(10.f, 0.f));
	MultiplySpeedBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	MultiplySpeedBtn->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.22f));
	UTextBlock* MulTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()); MulTxt->SetText(FText::FromString(" x2 "));
	MultiplySpeedBtn->AddChild(MulTxt);
	MultiplySpeedBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnMultiplySpeedClicked);
	SpeedBox->AddChildToHorizontalBox(MultiplySpeedBtn);
	UHorizontalBox* PlaybackBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(PlaybackBox)->SetPadding(FMargin(20.f, 10.f, 20.f, 20.f));
	auto CreatePlayBtn = [&](UButton*& Btn, FString Txt, FLinearColor Col) {
		Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Btn->SetBackgroundColor(Col);
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Txt)); T->SetFont(GetFont(12, true)); T->SetJustification(ETextJustify::Center);
		Btn->AddChild(T);
		UHorizontalBoxSlot* S = PlaybackBox->AddChildToHorizontalBox(Btn);
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(2.f, 0.f));
		};
	CreatePlayBtn(PlayRevBtn, AOrbitalManager::TrString(TEXT("<< INAPOI"), TEXT("<< REV")), FLinearColor(0.7f, 0.5f, 0.1f));
	PlayRevBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnPlayRevClicked);
	CreatePlayBtn(PauseBtn, AOrbitalManager::TrString(TEXT("|| PAUZA"), TEXT("|| PAUSE")), FLinearColor(0.8f, 0.2f, 0.2f));
	PauseBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnPauseClicked);
	CreatePlayBtn(PlayFwdBtn, AOrbitalManager::TrString(TEXT("INAINTE >>"), TEXT("PLAY >>")), FLinearColor(0.2f, 0.7f, 0.3f));
	PlayFwdBtn->OnClicked.AddDynamic(this, &UTimeControlWidget::OnPlayFwdClicked);
	GenerateCalendar();
	return Super::RebuildWidget();
}
void UTimeControlWidget::GenerateCalendar()
{
	if (!CalendarGrid || !MonthYearLabel) return;
	CalendarGrid->ClearChildren();
	FString CurrentLang = TEXT("Romana");
	if (GConfig)
	{
		GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), CurrentLang, GGameUserSettingsIni);
		CurrentLang.TrimStartAndEndInline();
	}
	TArray<FString> NumeLuni = { "Ianuarie", "Februarie", "Martie", "Aprilie", "Mai", "Iunie", "Iulie", "August", "Septembrie", "Octombrie", "Noiembrie", "Decembrie" };
	TArray<FString> ZileSaptamana = { "Lu", "Ma", "Mi", "Jo", "Vi", "Sa", "Du" };
	if (CurrentLang == TEXT("English"))
	{
		NumeLuni = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
		ZileSaptamana = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
	}
	FString HeaderText = FString::Printf(TEXT("%s %d"), *NumeLuni[CurrentSelectedMonth - 1], CurrentSelectedYear);
	MonthYearLabel->SetText(FText::FromString(HeaderText));
	for (int32 i = 0; i < 7; i++)
	{
		UTextBlock* DayHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DayHeader->SetText(FText::FromString(ZileSaptamana[i]));
		DayHeader->SetJustification(ETextJustify::Center);
		DayHeader->SetFont(GetFont(11, true));
		DayHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
		UUniformGridSlot* GridSlot = CalendarGrid->AddChildToUniformGrid(DayHeader);
		GridSlot->SetRow(0);
		GridSlot->SetColumn(i);
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetVerticalAlignment(VAlign_Center);
	}
	int32 AbsoluteYear = CurrentSelectedYear + 40000;
	int32 Shift = AbsoluteYear % 400;
	int32 UEValidYear = 2000 + Shift;
	FDateTime FirstDayEquivalent(UEValidYear, CurrentSelectedMonth, 1);
	int32 UnrealDayEnum = (int32)FirstDayEquivalent.GetDayOfWeek();
	int32 StartColumn = UnrealDayEnum;
	int32 DaysInMonth = GetDaysInMonthCustom(CurrentSelectedMonth, CurrentSelectedYear);
	int32 CurrentRow = 1;
	int32 CurrentCol = StartColumn;
	if (CurrentSelectedDay > DaysInMonth) CurrentSelectedDay = DaysInMonth;
	for (int32 Day = 1; Day <= DaysInMonth; Day++)
	{
		UCalendarDayButton* DayBtn = WidgetTree->ConstructWidget<UCalendarDayButton>(UCalendarDayButton::StaticClass());
		DayBtn->DayNumber = Day;
		DayBtn->ParentMenu = this;
		DayBtn->OnClicked.AddDynamic(DayBtn, &UCalendarDayButton::HandleClick);
		if (Day == CurrentSelectedDay)
			DayBtn->SetBackgroundColor(FLinearColor(0.15f, 0.45f, 0.85f));
		else
			DayBtn->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.18f));
		UTextBlock* DayTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DayTxt->SetText(FText::FromString(FString::FromInt(Day)));
		DayTxt->SetJustification(ETextJustify::Center);
		DayTxt->SetFont(GetFont(12));
		DayBtn->AddChild(DayTxt);
		UUniformGridSlot* GridSlot = CalendarGrid->AddChildToUniformGrid(DayBtn);
		GridSlot->SetRow(CurrentRow);
		GridSlot->SetColumn(CurrentCol);
		GridSlot->SetHorizontalAlignment(HAlign_Fill);
		GridSlot->SetVerticalAlignment(VAlign_Fill);
		CurrentCol++;
		if (CurrentCol > 6)
		{
			CurrentCol = 0;
			CurrentRow++;
		}
	}
}
void UTimeControlWidget::SelectDay(int32 Day)
{
	CurrentSelectedDay = Day;
	GenerateCalendar();
	UpdateDateInputs();
}
void UTimeControlWidget::OnPrevMonthClicked()
{
	if (CurrentSelectedYear <= -13000 && CurrentSelectedMonth == 1)
	{
		return;
	}
	CurrentSelectedMonth--;
	if (CurrentSelectedMonth < 1)
	{
		CurrentSelectedMonth = 12;
		CurrentSelectedYear--;
	}
	GenerateCalendar();
	UpdateDateInputs();
}
void UTimeControlWidget::OnNextMonthClicked()
{
	if (CurrentSelectedYear >= 17000 && CurrentSelectedMonth == 12)
	{
		return;
	}
	CurrentSelectedMonth++;
	if (CurrentSelectedMonth > 12)
	{
		CurrentSelectedMonth = 1;
		CurrentSelectedYear++;
	}
	GenerateCalendar();
	UpdateDateInputs();
}
void UTimeControlWidget::OnApplyTimeClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager) return;
	if (DayInput && MonthInput && YearInput)
	{
		int32 InputDay = FCString::Atoi(*DayInput->GetText().ToString());
		int32 InputMonth = FCString::Atoi(*MonthInput->GetText().ToString());
		int32 InputYear = FCString::Atoi(*YearInput->GetText().ToString());
		if (InputMonth >= 1 && InputMonth <= 12 && InputYear >= -13000 && InputYear <= 17000)
		{
			int32 MaxDays = GetDaysInMonthCustom(InputMonth, InputYear);
			if (InputDay >= 1 && InputDay <= MaxDays)
			{
				CurrentSelectedDay = InputDay;
				CurrentSelectedMonth = InputMonth;
				CurrentSelectedYear = InputYear;
				GenerateCalendar();
			}
		}
	}
	float SelectedHour = 0.0f;
	if (HourInput && MinInput && SecInput)
	{
		int32 H = FCString::Atoi(*HourInput->GetText().ToString());
		int32 M = FCString::Atoi(*MinInput->GetText().ToString());
		int32 S = FCString::Atoi(*SecInput->GetText().ToString());
		H = FMath::Clamp(H, 0, 23);
		M = FMath::Clamp(M, 0, 59);
		S = FMath::Clamp(S, 0, 59);
		SelectedHour = (float)H + (M / 60.0f) + (S / 3600.0f);
	}
	Manager->SetSimulationDateTime(CurrentSelectedDay, CurrentSelectedMonth, CurrentSelectedYear, SelectedHour);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Timp si Data Sincronizate!"));
}
void UTimeControlWidget::OnCloseClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
void UTimeControlWidget::ApplySpeedToManager(float NewSpeed)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->TimeSpeed = NewSpeed;
		SpeedInput->SetText(FText::FromString(FString::SanitizeFloat(FMath::Abs(NewSpeed))));
	}
}
void UTimeControlWidget::OnPlayFwdClicked()
{
	float CurrSpeed = FCString::Atof(*SpeedInput->GetText().ToString());
	if (CurrSpeed <= 0.0f) CurrSpeed = 1.0f;
	ApplySpeedToManager(FMath::Abs(CurrSpeed));
}
void UTimeControlWidget::OnPlayRevClicked()
{
	float CurrSpeed = FCString::Atof(*SpeedInput->GetText().ToString());
	if (CurrSpeed <= 0.0f) CurrSpeed = 1.0f;
	ApplySpeedToManager(-FMath::Abs(CurrSpeed));
}
void UTimeControlWidget::OnPauseClicked()
{
	ApplySpeedToManager(0.0f);
}
void UTimeControlWidget::OnMultiplySpeedClicked()
{
	float CurrSpeed = FCString::Atof(*SpeedInput->GetText().ToString());
	if (CurrSpeed == 0.0f) CurrSpeed = 1.0f;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager && Manager->TimeSpeed < 0)
	{
		ApplySpeedToManager(-(CurrSpeed * 2.0f));
	}
	else
	{
		ApplySpeedToManager(CurrSpeed * 2.0f);
	}
}
void UTimeControlWidget::OnDivideSpeedClicked()
{
	float CurrSpeed = FCString::Atof(*SpeedInput->GetText().ToString());
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager && Manager->TimeSpeed < 0)
	{
		ApplySpeedToManager(-(CurrSpeed / 2.0f));
	}
	else
	{
		ApplySpeedToManager(CurrSpeed / 2.0f);
	}
}
void UTimeControlWidget::OnSpeedInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		float NewSpeed = FCString::Atof(*Text.ToString());
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager)
		{
			if (Manager->TimeSpeed < 0) ApplySpeedToManager(-FMath::Abs(NewSpeed));
			else ApplySpeedToManager(FMath::Abs(NewSpeed));
		}
	}
}
void UTimeControlWidget::SyncClocks(bool bFromUTC)
{
	if (!HourInput || !SolarHourInput) return;
	double CurrentLongitude = 0.0;
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (GeoRef)
	{
		CurrentLongitude = GeoRef->GetOriginLongitudeLatitudeHeight().X;
	}
	float TimeOffsetHours = CurrentLongitude / 15.0f;
	if (bFromUTC)
	{
		int32 H = FCString::Atoi(*HourInput->GetText().ToString());
		int32 M = FCString::Atoi(*MinInput->GetText().ToString());
		int32 S = FCString::Atoi(*SecInput->GetText().ToString());
		float UTCHourFloat = H + (M / 60.0f) + (S / 3600.0f);
		float SolarHourFloat = UTCHourFloat + TimeOffsetHours;
		if (SolarHourFloat >= 24.0f) SolarHourFloat -= 24.0f;
		if (SolarHourFloat < 0.0f) SolarHourFloat += 24.0f;
		int32 SolH = FMath::FloorToInt(SolarHourFloat);
		int32 SolM = FMath::FloorToInt((SolarHourFloat - SolH) * 60.0f);
		int32 SolS = FMath::FloorToInt((((SolarHourFloat - SolH) * 60.0f) - SolM) * 60.0f);
		SolarHourInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(SolH, 0, 23))));
		SolarMinInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(SolM, 0, 59))));
		SolarSecInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(SolS, 0, 59))));
	}
	else
	{
		int32 SolH = FCString::Atoi(*SolarHourInput->GetText().ToString());
		int32 SolM = FCString::Atoi(*SolarMinInput->GetText().ToString());
		int32 SolS = FCString::Atoi(*SolarSecInput->GetText().ToString());
		float SolarHourFloat = SolH + (SolM / 60.0f) + (SolS / 3600.0f);
		float UTCHourFloat = SolarHourFloat - TimeOffsetHours;
		if (UTCHourFloat >= 24.0f) UTCHourFloat -= 24.0f;
		if (UTCHourFloat < 0.0f) UTCHourFloat += 24.0f;
		int32 H = FMath::FloorToInt(UTCHourFloat);
		int32 M = FMath::FloorToInt((UTCHourFloat - H) * 60.0f);
		int32 S = FMath::FloorToInt((((UTCHourFloat - H) * 60.0f) - M) * 60.0f);
		HourInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(H, 0, 23))));
		MinInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(M, 0, 59))));
		SecInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), FMath::Clamp(S, 0, 59))));
	}
}
void UTimeControlWidget::OnUTCInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	SyncClocks(true);
}
void UTimeControlWidget::OnSolarInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	SyncClocks(false);
}
void UTimeControlWidget::UpdateDateInputs()
{
	if (DayInput) DayInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), CurrentSelectedDay)));
	if (MonthInput) MonthInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), CurrentSelectedMonth)));
	if (YearInput) YearInput->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentSelectedYear)));
}
TSharedRef<SWidget> UOpticsMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.95f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(400.0f, 300.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("OPTICA SI TELESCOP"), TEXT("OPTICS & TELESCOPE")));
	Title->SetFont(GetFont(14, true));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(GetFont(14, true));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UOpticsMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	TextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	FSlateBrush BgBrush;
	BgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
	TextBoxStyle.BackgroundImageNormal = BgBrush;
	TextBoxStyle.BackgroundImageHovered = BgBrush;
	TextBoxStyle.BackgroundImageFocused = BgBrush;
	TextBoxStyle.BackgroundImageReadOnly = BgBrush;
	UHorizontalBox* TrackBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(TrackBox)->SetPadding(FMargin(20.f, 20.f, 20.f, 10.f));
	UTextBlock* TrackLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TrackLbl->SetText(AOrbitalManager::Tr(TEXT("Tinta Tracking: "), TEXT("Tracking Target: ")));
	TrackLbl->SetFont(GetFont(12));
	TrackBox->AddChildToHorizontalBox(TrackLbl)->SetVerticalAlignment(VAlign_Center);
	TargetComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	TargetComboBox->SetWidgetStyle(ComboStyle);
	TargetComboBox->ItemStyle = ComboRowStyle;
	TargetComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Venus"), TEXT("Venus")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Jupiter"), TEXT("Jupiter")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Saturn"), TEXT("Saturn")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Uranus"), TEXT("Uranus")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune")));
	TargetComboBox->AddOption(TEXT("Io"));
	TargetComboBox->AddOption(TEXT("Europa"));
	TargetComboBox->AddOption(TEXT("Ganymede"));
	TargetComboBox->AddOption(TEXT("Callisto"));
	TargetComboBox->AddOption(TEXT("Titan"));
	TargetComboBox->AddOption(TEXT("Triton"));
	FString CurrentTarget = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
	APlayerController* PC100 = GetOwningPlayer();
	if (PC100)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC100->GetPawn());
		if (MyDrone && !MyDrone->TrackingTarget.IsEmpty())
		{
			FString T = MyDrone->TrackingTarget;
			if (T == "Libera (Niciunul)") CurrentTarget = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
			else if (T == "Soare") CurrentTarget = AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun"));
			else if (T == "Luna") CurrentTarget = AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon"));
			else if (T == "Mercur") CurrentTarget = AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury"));
			else if (T == "Marte") CurrentTarget = AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars"));
			else if (T == "Neptun") CurrentTarget = AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune"));
			else CurrentTarget = T;
		}
	}
	TargetComboBox->SetSelectedOption(CurrentTarget);
	TargetComboBox->OnSelectionChanged.AddDynamic(this, &UOpticsMenuWidget::OnTargetChanged);
	TrackBox->AddChildToHorizontalBox(TargetComboBox)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	UVerticalBox* ZoomBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ZoomBox)->SetPadding(FMargin(20.f, 15.f));
	UTextBlock* ZoomLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ZoomLbl->SetText(AOrbitalManager::Tr(TEXT("Zoom (FOV):"), TEXT("Zoom (FOV):")));
	ZoomLbl->SetFont(GetFont(12, true));
	ZoomBox->AddChildToVerticalBox(ZoomLbl);
	ZoomSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	ZoomSlider->SetMinValue(0.00001f);
	ZoomSlider->SetMaxValue(135.0f);
	ZoomBox->AddChildToVerticalBox(ZoomSlider)->SetPadding(FMargin(0.f, 10.f, 0.f, 5.f));
	ZoomInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	ZoomInput->SetWidgetStyle(TextBoxStyle);
	ZoomInput->SetJustification(ETextJustify::Center);
	ZoomInput->WidgetStyle.TextStyle.Font = GetFont(14, true);
	ZoomInput->OnTextCommitted.AddDynamic(this, &UOpticsMenuWidget::OnZoomInputCommitted);
	ZoomBox->AddChildToVerticalBox(ZoomInput)->SetPadding(FMargin(100.f, 5.f, 100.f, 0.f));
	UHorizontalBox* FitButtonsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ZoomBox->AddChildToVerticalBox(FitButtonsBox)->SetPadding(FMargin(0.f, 15.f, 0.f, 0.f));
	FitWidthButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FitWidthButton->SetBackgroundColor(FLinearColor(0.2f, 0.5f, 0.8f));
	UTextBlock* FitWidthTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FitWidthTxt->SetText(AOrbitalManager::Tr(TEXT("Fit Latime"), TEXT("Fit Width")));
	FitWidthTxt->SetFont(GetFont(11, true));
	FitWidthButton->AddChild(FitWidthTxt);
	FitWidthButton->OnClicked.AddDynamic(this, &UOpticsMenuWidget::OnFitWidthClicked);
	UHorizontalBoxSlot* WidthSlot = FitButtonsBox->AddChildToHorizontalBox(FitWidthButton);
	WidthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	WidthSlot->SetPadding(FMargin(0.f, 0.f, 5.f, 0.f));
	FitHeightButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FitHeightButton->SetBackgroundColor(FLinearColor(0.2f, 0.8f, 0.5f));
	UTextBlock* FitHeightTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FitHeightTxt->SetText(AOrbitalManager::Tr(TEXT("Fit Inaltime"), TEXT("Fit Height")));
	FitHeightTxt->SetFont(GetFont(11, true));
	FitHeightButton->AddChild(FitHeightTxt);
	FitHeightButton->OnClicked.AddDynamic(this, &UOpticsMenuWidget::OnFitHeightClicked);
	UHorizontalBoxSlot* HeightSlot = FitButtonsBox->AddChildToHorizontalBox(FitHeightButton);
	HeightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HeightSlot->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
	UHorizontalBox* ResetBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ZoomBox->AddChildToVerticalBox(ResetBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ResetButton->SetBackgroundColor(FLinearColor(0.8f, 0.2f, 0.2f));
	UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(AOrbitalManager::Tr(TEXT("Resetare la Default"), TEXT("Reset to Default")));
	ResetTxt->SetFont(GetFont(11, true));
	ResetTxt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ResetButton->AddChild(ResetTxt);
	ResetButton->OnClicked.AddDynamic(this, &UOpticsMenuWidget::OnResetClicked);
	UHorizontalBoxSlot* ResetSlot = ResetBox->AddChildToHorizontalBox(ResetButton);
	ResetSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerCameraManager)
	{
		CachedCameraManager = PC->PlayerCameraManager;
		float currentFOV = CachedCameraManager->GetFOVAngle();
		ZoomSlider->SetValue(currentFOV);
		ZoomInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), currentFOV)));
	}
	ZoomSlider->OnValueChanged.AddDynamic(this, &UOpticsMenuWidget::OnZoomChanged);
	return Super::RebuildWidget();
}
void UOpticsMenuWidget::OnCloseClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
void UOpticsMenuWidget::OnResetClicked()
{
	float DefaultFOV = 90.0f;
	if (CachedCameraManager) CachedCameraManager->SetFOV(DefaultFOV);
	if (ZoomSlider) ZoomSlider->SetValue(DefaultFOV);
	if (ZoomInput) ZoomInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), DefaultFOV)));
	FString DefaultTarget = "Libera (Niciunul)";
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone) MyDrone->TrackingTarget = DefaultTarget;
	}
	if (TargetComboBox)
	{
		TargetComboBox->SetSelectedOption(AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)")));
	}
}
void UOpticsMenuWidget::OnTargetChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	if (MyDrone)
	{
		FString InternalTarget = SelectedItem;
		if (SelectedItem == "Free (None)") InternalTarget = "Libera (Niciunul)";
		else if (SelectedItem == "Sun") InternalTarget = "Soare";
		else if (SelectedItem == "Moon") InternalTarget = "Luna";
		else if (SelectedItem == "Mercury") InternalTarget = "Mercur";
		else if (SelectedItem == "Mars") InternalTarget = "Marte";
		else if (SelectedItem == "Neptune") InternalTarget = "Neptun";
		MyDrone->TrackingTarget = InternalTarget;
	}
}
void UOpticsMenuWidget::OnZoomChanged(float Value)
{
	if (ZoomInput) ZoomInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Value)));
	if (CachedCameraManager) CachedCameraManager->SetFOV(Value);
}
void UOpticsMenuWidget::OnZoomInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		float Value = FCString::Atof(*Text.ToString());
		if (CachedCameraManager) CachedCameraManager->SetFOV(Value);
		if (ZoomSlider) ZoomSlider->SetValue(Value);
		if (ZoomInput) ZoomInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Value)));
	}
}
void UOpticsMenuWidget::OnFitWidthClicked()
{
	AutoFitFOV(false);
}
void UOpticsMenuWidget::OnFitHeightClicked()
{
	AutoFitFOV(true);
}
void UOpticsMenuWidget::AutoFitFOV(bool bFitHeight)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	if (!MyDrone) return;
	FString Target = MyDrone->TrackingTarget;
	if (Target == "None" || Target.IsEmpty() || Target == "Libera (Niciunul)" || Target == "Free (None)") return;
	AOrbitalManager* OrbManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!OrbManager) return;
	FString CleanTarget = Target.TrimStartAndEnd();
	double D = 0.0;
	double R = 0.0;
	if (CleanTarget == "Soare" || CleanTarget == "Sun") { D = OrbManager->CurrentSunDistanceKm; R = 696340.0; }
	else if (CleanTarget == "Luna" || CleanTarget == "Moon") { D = OrbManager->CurrentMoonDistanceKm; R = 1737.4; }
	else
	{
		bool bFound = false;
		for (const auto& Planet : OrbManager->PlanetsList)
		{
			if (CleanTarget.Equals(Planet.Name, ESearchCase::IgnoreCase) || CleanTarget.Equals(Planet.InternalName, ESearchCase::IgnoreCase))
			{
				D = Planet.DistanceKm;
				FString PName = Planet.InternalName.IsEmpty() ? Planet.Name : Planet.InternalName;
				if (PName == "Mercur") R = 2439.7;
				else if (PName == "Venus") R = 6051.8;
				else if (PName == "Marte") R = 3389.5;
				else if (PName == "Jupiter") R = 71492.0;
				else if (PName == "Saturn") R = 58232.0;
				else if (PName == "Uranus") R = 25362.0;
				else if (PName == "Neptun") R = 24622.0;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			for (const auto& MoonRef : OrbManager->MajorMoonsList)
			{
				FString MName = MoonRef.InternalName.IsEmpty() ? MoonRef.Name : MoonRef.InternalName;
				if (CleanTarget.Equals(MoonRef.Name, ESearchCase::IgnoreCase) || CleanTarget.Equals(MoonRef.InternalName, ESearchCase::IgnoreCase))
				{
					D = MoonRef.DistanceKm;
					if (MName.Equals(TEXT("Io"), ESearchCase::IgnoreCase)) R = 1821.6;
					else if (MName.Equals(TEXT("Europa"), ESearchCase::IgnoreCase)) R = 1560.8;
					else if (MName.Equals(TEXT("Ganymede"), ESearchCase::IgnoreCase)) R = 2634.1;
					else if (MName.Equals(TEXT("Callisto"), ESearchCase::IgnoreCase)) R = 2410.3;
					else if (MName.Equals(TEXT("Titan"), ESearchCase::IgnoreCase)) R = 2574.7;
					else if (MName.Equals(TEXT("Triton"), ESearchCase::IgnoreCase)) R = 1353.4;
					bFound = true;
					break;
				}
			}
		}
	}
	if (D > 0.0 && R > 0.0)
	{
		double AngularSizeRads = 2.0 * FMath::Atan(R / D);
		float BaseFOV = (float)(AngularSizeRads * (180.0 / PI));
		float FinalFOV = BaseFOV;
		if (bFitHeight)
		{
			int32 ViewportSizeX, ViewportSizeY;
			PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
			if (ViewportSizeY > 0)
			{
				float DynamicAspectRatio = (float)ViewportSizeX / (float)ViewportSizeY;
				FinalFOV *= DynamicAspectRatio;
			}
			else
			{
				FinalFOV *= 1.777f;
			}
		}
		FinalFOV = FMath::Clamp(FinalFOV, 0.001f, 135.0f);
		if (CachedCameraManager) CachedCameraManager->SetFOV(FinalFOV);
		if (ZoomSlider) ZoomSlider->SetValue(FinalFOV);
		if (ZoomInput) ZoomInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), FinalFOV)));
	}
}
TSharedRef<SWidget> UOverlayMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.95f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(450.0f, 1100.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("OVERLAYS & VIZUAL"), TEXT("OVERLAYS & VISUALS")));
	Title->SetFont(GetFont(14, true));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(GetFont(14, true));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UOverlayMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	auto CreateToggleRow = [&](UCheckBox*& OutCheckBox, FString LabelRO, FString LabelEN, bool bCurrentState) {
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		MainLayout->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(20.f, 12.f, 20.f, 5.f));
		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
		Lbl->SetFont(GetFont(12));
		UHorizontalBoxSlot* LblSlot = RowBox->AddChildToHorizontalBox(Lbl);
		LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LblSlot->SetVerticalAlignment(VAlign_Center);
		OutCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
		OutCheckBox->SetIsChecked(bCurrentState);
		RowBox->AddChildToHorizontalBox(OutCheckBox)->SetVerticalAlignment(VAlign_Center);
		UBorder* Div = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Div->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.5f));
		MainLayout->AddChildToVerticalBox(Div)->SetPadding(FMargin(20.f, 5.f, 20.f, 0.f));
		};
	bool bHudActive = true;
	bool bAzimuthActive = false;
	bool bEquatorialActive = false;
	bool bPlanetsActive = false;
	bool bAstroTextActive = false;
	bool bDroneVisible = true;
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD) { bHudActive = MyHUD->bDrawTelemetry; }
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			bAzimuthActive = MyDrone->bShowAzimuthGrid;
			bEquatorialActive = MyDrone->bShowEquatorialGrid;
			bPlanetsActive = MyDrone->bShowPlanets;
			bAstroTextActive = MyDrone->bShowAstroText;
			bDroneVisible = !MyDrone->IsHidden();
		}
	}
	bool bAreCloudsVisible = true;
	TArray<AActor*> FoundClouds;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundClouds);
	for (AActor* Actor : FoundClouds)
	{
		if (Actor && Actor->GetName().Contains("VolumetricCloud"))
		{
			bAreCloudsVisible = !Actor->IsHidden();
			break;
		}
	}
	bool bStarNamesVisible = true;
	bool bMoonNamesVisible = true;
	bool bConstellationNamesVisible = false;
	bool bArtVisible = false;
	bool bDrawShadowGround3 = true;
	bool bDrawConeFull3 = false;
	bool bDrawPenGround = true;
	bool bDrawPenFull = false;
	bool bDrawEarthUmbra = true;
	bool bDrawEarthPenumbra = true;
	AOrbitalManager* TempManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (TempManager)
	{
		bStarNamesVisible = TempManager->bShowStarLabels;
		bMoonNamesVisible = TempManager->bShowMoonLabels;
		bConstellationNamesVisible = TempManager->bShowConstellationLabels;
		if (TempManager->ZodiacRoot) bArtVisible = TempManager->ZodiacRoot->IsVisible();
		bDrawShadowGround3 = TempManager->bShowShadowOnGround;
		bDrawConeFull3 = TempManager->bShowShadowConeFull;
		bDrawPenGround = TempManager->bShowPenumbraOnGround;
		bDrawPenFull = TempManager->bShowPenumbraConeFull;
		bDrawEarthUmbra = TempManager->bShowEarthShadowUmbra;
		bDrawEarthPenumbra = TempManager->bShowEarthShadowPenumbra;
	}
	CreateToggleRow(CheckBox_HUD, TEXT("Afisare HUD (Telemetrie)"), TEXT("Show HUD (Telemetry)"), bHudActive);
	CheckBox_HUD->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnHUDToggled);
	CreateToggleRow(CheckBox_Clouds, TEXT("Nori Volumetrici"), TEXT("Volumetric Clouds"), bAreCloudsVisible);
	CheckBox_Clouds->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnCloudsToggled);
	CreateToggleRow(CheckBox_GridAzimuth, TEXT("Grila Cer (Azimutala)"), TEXT("Sky Grid (Azimuthal)"), bAzimuthActive);
	CheckBox_GridAzimuth->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnGridAzimuthToggled);
	CreateToggleRow(CheckBox_GridEquatorial, TEXT("Grila Cer (Ecuatoriala Z)"), TEXT("Sky Grid (Equatorial Z)"), bEquatorialActive);
	CheckBox_GridEquatorial->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnGridEquatorialToggled);
	CreateToggleRow(CheckBox_LabelsStars, TEXT("Afisare Stele"), TEXT("Show Stars"), true);
	CheckBox_LabelsStars->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnLabelsStarsToggled);
	CreateToggleRow(CheckBox_LabelsPlanets, TEXT("Afisare Planete"), TEXT("Show Planets"), bPlanetsActive);
	CheckBox_LabelsPlanets->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnLabelsPlanetsToggled);
	CreateToggleRow(CheckBox_AstroText, TEXT("Etichete Text (Soare/Luna/Planete)"), TEXT("Text Labels (Sun/Moon/Planets)"), bAstroTextActive);
	CheckBox_AstroText->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnAstroTextToggled);
	CreateToggleRow(CheckBox_StarNames, TEXT("Afisare Nume Stele"), TEXT("Show Star Names"), bStarNamesVisible);
	CheckBox_StarNames->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnStarNamesToggled);
	CreateToggleRow(CheckBox_MoonLabels, TEXT("Afisare Nume Sateliti Naturali"), TEXT("Show Natural Moon Names"), bMoonNamesVisible);
	CheckBox_MoonLabels->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnMoonLabelsToggled);
	CreateToggleRow(CheckBox_DroneVis, TEXT("Vizibilitate Model Drona"), TEXT("Drone Model Visibility"), bDroneVisible);
	CheckBox_DroneVis->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnDroneVisToggled);
	CreateToggleRow(CheckBox_Constellations, TEXT("Linii Constelatii"), TEXT("Constellation Lines"), true);
	CheckBox_Constellations->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnConstellationsToggled);
	CreateToggleRow(CheckBox_ConstellationNames, TEXT("Afisare Nume Constelatii"), TEXT("Show Constellation Names"), bConstellationNamesVisible);
	CheckBox_ConstellationNames->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnConstellationNamesToggled);
	CreateToggleRow(CheckBox_ConstellationArt, TEXT("Desene Artistice (Mitologie)"), TEXT("Artistic Drawings (Mythology)"), bArtVisible);
	CheckBox_ConstellationArt->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnConstellationArtToggled);
	UBorder* ShadowHeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ShadowHeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(ShadowHeaderBg)->SetPadding(FMargin(0.f, 15.f, 0.f, 5.f));
	UTextBlock* ShadowTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ShadowTitle->SetText(AOrbitalManager::Tr(TEXT("SETARI CON DE UMBRA"), TEXT("UMBRA CONE SETTINGS")));
	ShadowTitle->SetFont(GetFont(12, true));
	ShadowTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	ShadowHeaderBg->SetContent(ShadowTitle);
	ShadowHeaderBg->SetPadding(FMargin(15.f, 5.f));
	CreateToggleRow(CheckBox_ShadowGround, TEXT("Doar Umbra pe Sol (2D)"), TEXT("Only Shadow on Ground (2D)"), bDrawShadowGround3);
	CheckBox_ShadowGround->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnShadowGroundToggled);
	CreateToggleRow(CheckBox_ShadowConeFull, TEXT("Con de Umbra Full (3D)"), TEXT("Full Umbra Cone (3D)"), bDrawConeFull3);
	CheckBox_ShadowConeFull->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnShadowConeFullToggled);
	UBorder* PenumbraHeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PenumbraHeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(PenumbraHeaderBg)->SetPadding(FMargin(0.f, 15.f, 0.f, 5.f));
	UTextBlock* PenumbraTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PenumbraTitle->SetText(AOrbitalManager::Tr(TEXT("SETARI CON DE PENUMBRA"), TEXT("PENUMBRA CONE SETTINGS")));
	PenumbraTitle->SetFont(GetFont(12, true));
	PenumbraTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	PenumbraHeaderBg->SetContent(PenumbraTitle);
	PenumbraHeaderBg->SetPadding(FMargin(15.f, 5.f));
	CreateToggleRow(CheckBox_PenumbraGround, TEXT("Doar Penumbra pe Sol (2D)"), TEXT("Only Penumbra on Ground (2D)"), bDrawPenGround);
	CheckBox_PenumbraGround->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnPenumbraGroundToggled);
	CreateToggleRow(CheckBox_PenumbraConeFull, TEXT("Con de Penumbra Full (3D)"), TEXT("Full Penumbra Cone (3D)"), bDrawPenFull);
	CheckBox_PenumbraConeFull->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnPenumbraConeFullToggled);
	UBorder* EarthShadowHeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EarthShadowHeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(EarthShadowHeaderBg)->SetPadding(FMargin(0.f, 15.f, 0.f, 5.f));
	UTextBlock* EarthShadowTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	EarthShadowTitle->SetText(AOrbitalManager::Tr(TEXT("UMBRA SI PENUMBRA PAMANTULUI"), TEXT("EARTH UMBRA & PENUMBRA")));
	EarthShadowTitle->SetFont(GetFont(12, true));
	EarthShadowTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	EarthShadowHeaderBg->SetContent(EarthShadowTitle);
	EarthShadowHeaderBg->SetPadding(FMargin(15.f, 5.f));
	CreateToggleRow(CheckBox_EarthUmbra, TEXT("Con Umbra Pamant (3D)"), TEXT("Earth Umbra Cone (3D)"), bDrawEarthUmbra);
	CheckBox_EarthUmbra->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnEarthUmbraToggled);
	CreateToggleRow(CheckBox_EarthPenumbra, TEXT("Con Penumbra Pamant (3D)"), TEXT("Earth Penumbra Cone (3D)"), bDrawEarthPenumbra);
	CheckBox_EarthPenumbra->OnCheckStateChanged.AddDynamic(this, &UOverlayMenuWidget::OnEarthPenumbraToggled);
	return Super::RebuildWidget();
}
void UOverlayMenuWidget::OnEarthUmbraToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager) Manager->bShowEarthShadowUmbra = bIsChecked;
}
void UOverlayMenuWidget::OnEarthPenumbraToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager) Manager->bShowEarthShadowPenumbra = bIsChecked;
}
void UOverlayMenuWidget::OnMoonLabelsToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->bShowMoonLabels = bIsChecked;
	}
}
void UOverlayMenuWidget::OnCloseClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
void UOverlayMenuWidget::OnStarNamesToggled(bool bIsChecked)
{
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (OrbitalManager)
	{
		OrbitalManager->SetStarLabelsVisibility(bIsChecked);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eroare: Widget-ul nu gaseste OrbitalManager-ul pentru nume stele!"));
	}
}
void UOverlayMenuWidget::OnPenumbraGroundToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager) Manager->bShowPenumbraOnGround = bIsChecked;
}
void UOverlayMenuWidget::OnPenumbraConeFullToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager) Manager->bShowPenumbraConeFull = bIsChecked;
}
void UOverlayMenuWidget::OnPenumbraConeTruncatedToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager) Manager->bShowPenumbraConeTruncated = bIsChecked;
}
void UOverlayMenuWidget::OnConstellationNamesToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->bShowConstellationLabels = bIsChecked;
	}
}
void UOverlayMenuWidget::OnConstellationArtToggled(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->SetZodiacVisibility(bIsChecked);
	}
}
void UOverlayMenuWidget::OnDroneVisToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->SetActorHiddenInGame(!bIsChecked);
		}
	}
}
void UOverlayMenuWidget::OnHUDToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD)
		{
			MyHUD->bDrawTelemetry = bIsChecked;
		}
	}
}
void UOverlayMenuWidget::OnConstellationsToggled(bool bIsChecked)
{
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass());
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(FoundActor);
	if (OrbitalManager)
	{
		OrbitalManager->SetConstellationsVisibility(bIsChecked);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eroare: Widget-ul nu gaseste OrbitalManager-ul pentru constelatii!"));
	}
}
void UOverlayMenuWidget::OnShadowsToggled(bool bIsChecked)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, bIsChecked ? FColor::Green : FColor::Red, TEXT("Umbra Sol Toggled"));
}
void UOverlayMenuWidget::OnCloudsToggled(bool bIsChecked)
{
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
	bool bFoundAny = false;
	for (AActor* Actor : FoundActors)
	{
		if (Actor && Actor->GetName().Contains("VolumetricCloud"))
		{
			Actor->SetActorHiddenInGame(!bIsChecked);
			bFoundAny = true;
			break;
		}
	}
	if (!bFoundAny && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("UI Eroare: Nu gasesc actorul 'VolumetricCloud'!"));
	}
}
void UOverlayMenuWidget::OnGridAzimuthToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->bShowAzimuthGrid = bIsChecked;
		}
	}
}
void UOverlayMenuWidget::OnGridEquatorialToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->bShowEquatorialGrid = bIsChecked;
		}
	}
}
void UOverlayMenuWidget::OnLabelsStarsToggled(bool bIsChecked)
{
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, bIsChecked ? FColor::Cyan : FColor::Red, FString::Printf(TEXT("Afisare Stele: %s"), bIsChecked ? TEXT("ON") : TEXT("OFF")));
	AActor* FoundActor = UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass());
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(FoundActor);
	if (OrbitalManager)
	{
		OrbitalManager->SetStarsVisibility(bIsChecked);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eroare: Widget-ul nu gaseste OrbitalManager-ul!"));
	}
}
void UOverlayMenuWidget::OnLabelsPlanetsToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->bShowPlanets = bIsChecked;
		}
	}
}
void UOverlayMenuWidget::OnAstroTextToggled(bool bIsChecked)
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->bShowAstroText = bIsChecked;
		}
	}
}
void UOverlayMenuWidget::OnShadowGroundToggled(bool bIsChecked)
{
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (OrbitalManager)
	{
		OrbitalManager->bShowShadowOnGround = bIsChecked;
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, bIsChecked ? FColor::Green : FColor::Red, TEXT("Umbra pe Sol setata."));
	}
}
void UOverlayMenuWidget::OnShadowConeFullToggled(bool bIsChecked)
{
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (OrbitalManager)
	{
		OrbitalManager->bShowShadowConeFull = bIsChecked;
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, bIsChecked ? FColor::Green : FColor::Red, TEXT("Con de Umbra Full setat."));
	}
}
void UOverlayMenuWidget::OnShadowConeTruncatedToggled(bool bIsChecked)
{
	AOrbitalManager* OrbitalManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (OrbitalManager)
	{
		OrbitalManager->bShowShadowConeTruncated = bIsChecked;
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, bIsChecked ? FColor::Green : FColor::Red, TEXT("Con de Umbra Trunchiat setat."));
	}
}
int32 FrameCounter = 1;
float TimeStepSeconds = 60.0f;
FDateTime CurrentSimDate;
float CaptureFPS = 60.0f;
FString TempFramesFolder;
TSharedRef<SWidget> UVideoExportMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.98f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(650.0f, 850.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(20.f, 15.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("EXPORT TIMELAPSE (.MP4)"), TEXT("TIMELAPSE EXPORT (.MP4)")));
	Title->SetFont(GetFont(14, true));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(GetFont(14, true));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	FEditableTextBoxStyle InputStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	InputStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	InputStyle.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	InputStyle.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
	InputStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	UTextBlock* StartTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StartTitle->SetText(AOrbitalManager::Tr(TEXT("1. MOMENTUL DE START AL FILMARII"), TEXT("1. RECORDING START TIME")));
	StartTitle->SetFont(GetFont(12, true));
	StartTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
	MainLayout->AddChildToVerticalBox(StartTitle)->SetPadding(FMargin(20.f, 15.f, 20.f, 5.f));
	UBorder* StartCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	StartCard->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f));
	MainLayout->AddChildToVerticalBox(StartCard)->SetPadding(FMargin(20.f, 0.f, 20.f, 10.f));
	UVerticalBox* StartCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	StartCard->SetContent(StartCardBox);
	StartCard->SetPadding(FMargin(15.f));
	UHorizontalBox* StartHdr = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StartCardBox->AddChildToVerticalBox(StartHdr)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UTextBlock* TxtS = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtS->SetText(AOrbitalManager::Tr(TEXT("Data si Ora (UTC):"), TEXT("Date and Time (UTC):")));
	TxtS->SetFont(GetFont(11, true));
	StartHdr->AddChildToHorizontalBox(TxtS)->SetVerticalAlignment(VAlign_Center);
	StartHdr->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Btn_GetCurrentStartTime = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_GetCurrentStartTime->SetBackgroundColor(FLinearColor(0.15f, 0.35f, 0.65f));
	UTextBlock* BtnLblStart = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BtnLblStart->SetText(AOrbitalManager::Tr(TEXT("PUNE TIMPUL CURENT (LIVE)"), TEXT("SET CURRENT TIME (LIVE)")));
	BtnLblStart->SetFont(GetFont(9, true));
	BtnLblStart->SetMargin(FMargin(10.f, 5.f));
	Btn_GetCurrentStartTime->AddChild(BtnLblStart);
	Btn_GetCurrentStartTime->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnGetStartTimeClicked);
	StartHdr->AddChildToHorizontalBox(Btn_GetCurrentStartTime);
	UHorizontalBox* StartInputs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StartCardBox->AddChildToVerticalBox(StartInputs);
	auto CreateStyledInput = [&](UEditableTextBox*& Box, FString HintRO, FString HintEN, UHorizontalBox* Parent, float RightPad = 5.f) {
		Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
		Box->SetWidgetStyle(InputStyle);
		Box->SetHintText(AOrbitalManager::Tr(HintRO, HintEN));
		Box->SetJustification(ETextJustify::Center);
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Box);
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(0.f, 0.f, RightPad, 0.f));
		};
	CreateStyledInput(StartYInput, TEXT("An"), TEXT("Yr"), StartInputs);
	CreateStyledInput(StartMInput, TEXT("Luna"), TEXT("Mo"), StartInputs);
	CreateStyledInput(StartDInput, TEXT("Zi"), TEXT("Day"), StartInputs, 15.f);
	CreateStyledInput(StartHInput, TEXT("Ora"), TEXT("Hr"), StartInputs);
	CreateStyledInput(StartMinInput, TEXT("Min"), TEXT("Min"), StartInputs);
	CreateStyledInput(StartSInput, TEXT("Sec"), TEXT("Sec"), StartInputs, 0.f);
	UTextBlock* StopTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StopTitle->SetText(AOrbitalManager::Tr(TEXT("2. MOMENTUL DE OPRIRE AL FILMARII"), TEXT("2. RECORDING STOP TIME")));
	StopTitle->SetFont(GetFont(12, true));
	StopTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
	MainLayout->AddChildToVerticalBox(StopTitle)->SetPadding(FMargin(20.f, 10.f, 20.f, 5.f));
	UBorder* StopCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	StopCard->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f));
	MainLayout->AddChildToVerticalBox(StopCard)->SetPadding(FMargin(20.f, 0.f, 20.f, 10.f));
	UVerticalBox* StopCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	StopCard->SetContent(StopCardBox);
	StopCard->SetPadding(FMargin(15.f));
	UHorizontalBox* StopHdr = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StopCardBox->AddChildToVerticalBox(StopHdr)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UTextBlock* TxtE = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtE->SetText(AOrbitalManager::Tr(TEXT("Data si Ora (UTC):"), TEXT("Date and Time (UTC):")));
	TxtE->SetFont(GetFont(11, true));
	StopHdr->AddChildToHorizontalBox(TxtE)->SetVerticalAlignment(VAlign_Center);
	StopHdr->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Btn_GetCurrentStopTime = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_GetCurrentStopTime->SetBackgroundColor(FLinearColor(0.15f, 0.35f, 0.65f));
	UTextBlock* BtnLblStop = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BtnLblStop->SetText(AOrbitalManager::Tr(TEXT("PUNE TIMPUL CURENT (LIVE)"), TEXT("SET CURRENT TIME (LIVE)")));
	BtnLblStop->SetFont(GetFont(9, true));
	BtnLblStop->SetMargin(FMargin(10.f, 5.f));
	Btn_GetCurrentStopTime->AddChild(BtnLblStop);
	Btn_GetCurrentStopTime->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnGetStopTimeClicked);
	StopHdr->AddChildToHorizontalBox(Btn_GetCurrentStopTime);
	UHorizontalBox* StopInputs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StopCardBox->AddChildToVerticalBox(StopInputs);
	CreateStyledInput(StopYInput, TEXT("An"), TEXT("Yr"), StopInputs);
	CreateStyledInput(StopMInput, TEXT("Luna"), TEXT("Mo"), StopInputs);
	CreateStyledInput(StopDInput, TEXT("Zi"), TEXT("Day"), StopInputs, 15.f);
	CreateStyledInput(StopHInput, TEXT("Ora"), TEXT("Hr"), StopInputs);
	CreateStyledInput(StopMinInput, TEXT("Min"), TEXT("Min"), StopInputs);
	CreateStyledInput(StopSInput, TEXT("Sec"), TEXT("Sec"), StopInputs, 0.f);
	UTextBlock* LocTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LocTitle->SetText(AOrbitalManager::Tr(TEXT("3. COORDONATE OBSERVATOR (GPS)"), TEXT("3. OBSERVER COORDINATES (GPS)")));
	LocTitle->SetFont(GetFont(12, true));
	LocTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
	MainLayout->AddChildToVerticalBox(LocTitle)->SetPadding(FMargin(20.f, 10.f, 20.f, 5.f));
	UBorder* LocCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LocCard->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f));
	MainLayout->AddChildToVerticalBox(LocCard)->SetPadding(FMargin(20.f, 0.f, 20.f, 10.f));
	UVerticalBox* LocCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	LocCard->SetContent(LocCardBox);
	LocCard->SetPadding(FMargin(15.f));
	UHorizontalBox* LocHdr = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LocCardBox->AddChildToVerticalBox(LocHdr)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UTextBlock* TxtL = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtL->SetText(AOrbitalManager::Tr(TEXT("Locatia camerei:"), TEXT("Camera location:")));
	TxtL->SetFont(GetFont(11, true));
	LocHdr->AddChildToHorizontalBox(TxtL)->SetVerticalAlignment(VAlign_Center);
	LocHdr->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Btn_GetCurrentLocation = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_GetCurrentLocation->SetBackgroundColor(FLinearColor(0.15f, 0.35f, 0.65f));
	UTextBlock* BtnLblLoc = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BtnLblLoc->SetText(AOrbitalManager::Tr(TEXT("IA LOCATIA EXACTA A DRONEI"), TEXT("GET EXACT DRONE LOCATION")));
	BtnLblLoc->SetFont(GetFont(9, true));
	BtnLblLoc->SetMargin(FMargin(10.f, 5.f));
	Btn_GetCurrentLocation->AddChild(BtnLblLoc);
	Btn_GetCurrentLocation->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnGetLocationClicked);
	LocHdr->AddChildToHorizontalBox(Btn_GetCurrentLocation);
	UHorizontalBox* LocInputs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LocCardBox->AddChildToVerticalBox(LocInputs);
	CreateStyledInput(LatInput, TEXT("Latitudine"), TEXT("Latitude"), LocInputs, 10.f);
	CreateStyledInput(LonInput, TEXT("Longitudine"), TEXT("Longitude"), LocInputs, 10.f);
	CreateStyledInput(AltInput, TEXT("Altitudine"), TEXT("Altitude"), LocInputs, 0.f);
	AltInput->SetText(FText::FromString("100"));
	UTextBlock* RenderTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	RenderTitle->SetText(AOrbitalManager::Tr(TEXT("4. SETARI CAMERA SI RANDARE VIDEO"), TEXT("4. CAMERA & RENDER SETTINGS")));
	RenderTitle->SetFont(GetFont(12, true));
	RenderTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.7f, 1.0f)));
	MainLayout->AddChildToVerticalBox(RenderTitle)->SetPadding(FMargin(20.f, 10.f, 20.f, 5.f));
	UBorder* RenderCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RenderCard->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.1f, 1.0f));
	MainLayout->AddChildToVerticalBox(RenderCard)->SetPadding(FMargin(20.f, 0.f, 20.f, 10.f));
	UVerticalBox* RenderCardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RenderCard->SetContent(RenderCardBox);
	RenderCard->SetPadding(FMargin(15.f));
	UHorizontalBox* CamRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RenderCardBox->AddChildToVerticalBox(CamRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	TargetComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	TargetComboBox->SetWidgetStyle(ComboStyle);
	TargetComboBox->ItemStyle = ComboRowStyle;
	TargetComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Venus"), TEXT("Venus")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Jupiter"), TEXT("Jupiter")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Saturn"), TEXT("Saturn")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Uranus"), TEXT("Uranus")));
	TargetComboBox->AddOption(AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune")));
	FString InitialTarget = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
	float InitialFOV = 90.0f;
	if (APlayerController* PC = GetOwningPlayer())
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone && !MyDrone->TrackingTarget.IsEmpty())
		{
			FString T = MyDrone->TrackingTarget;
			if (T == "Libera (Niciunul)") InitialTarget = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
			else if (T == "Soare") InitialTarget = AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun"));
			else if (T == "Luna") InitialTarget = AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon"));
			else if (T == "Mercur") InitialTarget = AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury"));
			else if (T == "Marte") InitialTarget = AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars"));
			else if (T == "Neptun") InitialTarget = AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune"));
			else InitialTarget = T;
		}
		if (PC->PlayerCameraManager) InitialFOV = PC->PlayerCameraManager->GetFOVAngle();
	}
	TargetComboBox->SetSelectedOption(InitialTarget);
	UHorizontalBoxSlot* CamSlot1 = CamRow->AddChildToHorizontalBox(TargetComboBox);
	CamSlot1->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CamSlot1->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	FovInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	FovInput->SetWidgetStyle(InputStyle);
	FovInput->SetHintText(FText::FromString("FOV"));
	FovInput->SetJustification(ETextJustify::Center);
	FovInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), InitialFOV)));
	UHorizontalBoxSlot* CamSlot2 = CamRow->AddChildToHorizontalBox(FovInput);
	CamSlot2->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* VidRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RenderCardBox->AddChildToVerticalBox(VidRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	ResolutionComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	ResolutionComboBox->SetWidgetStyle(ComboStyle);
	ResolutionComboBox->ItemStyle = ComboRowStyle;
	ResolutionComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	ResolutionComboBox->AddOption("1920x1080 (FHD)"); ResolutionComboBox->AddOption("3840x2160 (4K)"); ResolutionComboBox->SetSelectedOption("1920x1080 (FHD)");
	UHorizontalBoxSlot* ResSlot = VidRow->AddChildToHorizontalBox(ResolutionComboBox);
	ResSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ResSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	FramerateComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	FramerateComboBox->SetWidgetStyle(ComboStyle);
	FramerateComboBox->ItemStyle = ComboRowStyle;
	FramerateComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	FramerateComboBox->AddOption("30 FPS"); FramerateComboBox->AddOption("60 FPS"); FramerateComboBox->SetSelectedOption("60 FPS");
	UHorizontalBoxSlot* FpsSlot = VidRow->AddChildToHorizontalBox(FramerateComboBox);
	FpsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FpsSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	PlaybackSpeedInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	PlaybackSpeedInput->SetWidgetStyle(InputStyle);
	PlaybackSpeedInput->SetHintText(AOrbitalManager::Tr(TEXT("Sari Timp(Sec)"), TEXT("Time Skip(Sec)")));
	PlaybackSpeedInput->SetJustification(ETextJustify::Center);
	PlaybackSpeedInput->SetText(FText::FromString("60"));
	UHorizontalBoxSlot* SpeedSlot = VidRow->AddChildToHorizontalBox(PlaybackSpeedInput);
	SpeedSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* CheckRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RenderCardBox->AddChildToVerticalBox(CheckRow);
	HideDroneCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass()); HideDroneCheckBox->SetIsChecked(true);
	CheckRow->AddChildToHorizontalBox(HideDroneCheckBox)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* CkLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CkLbl->SetText(AOrbitalManager::Tr(TEXT(" Ascunde modelul dronei in filmare (Mod Cinematic)"), TEXT(" Hide drone model in recording (Cinematic Mode)")));
	CkLbl->SetFont(GetFont(11));
	CheckRow->AddChildToHorizontalBox(CkLbl)->SetVerticalAlignment(VAlign_Center);
	USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	MainLayout->AddChildToVerticalBox(BottomSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StartRecordingButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	StartRecordingButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f, 1.0f));
	StartRecordingButton->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnStartRecordingClicked);
	UTextBlock* StartBtnTxtBig = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StartBtnTxtBig->SetText(AOrbitalManager::Tr(TEXT("PORNESTE INREGISTRAREA MP4"), TEXT("START MP4 RECORDING")));
	StartBtnTxtBig->SetFont(GetFont(14, true));
	StartBtnTxtBig->SetMargin(FMargin(0.f, 15.f));
	StartBtnTxtBig->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StartBtnTxtBig->SetJustification(ETextJustify::Center);
	StartRecordingButton->AddChild(StartBtnTxtBig);
	MainLayout->AddChildToVerticalBox(StartRecordingButton)->SetPadding(FMargin(20.f, 10.f, 20.f, 5.f));
	TakeScreenshotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	TakeScreenshotButton->SetBackgroundColor(FLinearColor(0.2f, 0.4f, 0.8f, 1.0f));
	TakeScreenshotButton->OnClicked.AddDynamic(this, &UVideoExportMenuWidget::OnTakeScreenshotClicked);
	UTextBlock* ScreenshotBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ScreenshotBtnTxt->SetText(AOrbitalManager::Tr(TEXT("FA O POZA (SCREENSHOT)"), TEXT("TAKE A SCREENSHOT")));
	ScreenshotBtnTxt->SetFont(GetFont(14, true));
	ScreenshotBtnTxt->SetMargin(FMargin(0.f, 15.f));
	ScreenshotBtnTxt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ScreenshotBtnTxt->SetJustification(ETextJustify::Center);
	TakeScreenshotButton->AddChild(ScreenshotBtnTxt);
	MainLayout->AddChildToVerticalBox(TakeScreenshotButton)->SetPadding(FMargin(20.f, 0.f, 20.f, 10.f));
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StatusText->SetText(AOrbitalManager::Tr(TEXT("Sistem executabil pregatit. FFMPEG este activ!"), TEXT("Executable system ready. FFMPEG is active!")));
	StatusText->SetFont(GetFont(10));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
	StatusText->SetJustification(ETextJustify::Center);
	MainLayout->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	return Super::RebuildWidget();
}
void UVideoExportMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD)
		{
			MyHUD->VideoExportMenuWidget = nullptr;
		}
	}
}
void UVideoExportMenuWidget::OnStartRecordingClicked()
{
	FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[512] = { 0 };
	FCString::Strcpy(szFile, TEXT("Eclipsa_Timelapse.mp4"));
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("Video MP4 (*.mp4)\0*.mp4\0Toate fisierele (*.*)\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("mp4");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty())
	{
		return;
	}
	if (!FinalSavePath.EndsWith(".mp4")) {
		FinalSavePath += ".mp4";
	}
	TempFramesFolder = FPaths::GetPath(FinalSavePath) + TEXT("/Temp_Frames/");
	IFileManager::Get().MakeDirectory(*TempFramesFolder, true);
	UWorld* World = GetWorld();
	if (!World) return;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(World, AOrbitalManager::StaticClass()));
	auto GetInt = [](UEditableTextBox* Box) -> int32 { return (!Box || Box->GetText().IsEmpty()) ? 0 : FCString::Atoi(*Box->GetText().ToString()); };
	int32 SY = GetInt(StartYInput); int32 SM = GetInt(StartMInput); int32 SD = GetInt(StartDInput);
	int32 SH = GetInt(StartHInput); int32 SMin = GetInt(StartMinInput); int32 SS = GetInt(StartSInput);
	if (SM < 1 || SM > 12 || SD < 1 || SD > 31)
	{
		if (Manager)
		{
			SY = Manager->CurrentDate.Year; SM = Manager->CurrentDate.Month; SD = Manager->CurrentDate.Day;
			SH = FMath::FloorToInt(Manager->CurrentDate.Hour);
			float Rem = (Manager->CurrentDate.Hour - SH) * 60.0f;
			SMin = FMath::FloorToInt(Rem); SS = FMath::FloorToInt((Rem - SMin) * 60.0f);
		}
		else { SY = 2024; SM = 1; SD = 1; }
	}
	SimYear = SY; SimMonth = SM; SimDay = SD;
	SimHour = SH; SimMinute = SMin; SimSecond = SS;
	int32 EY = GetInt(StopYInput); int32 EM = GetInt(StopMInput); int32 ED = GetInt(StopDInput);
	int32 EH = GetInt(StopHInput); int32 EMin = GetInt(StopMinInput); int32 ES = GetInt(StopSInput);
	if (EM < 1 || EM > 12 || ED < 1 || ED > 31)
	{
		StopYear = SimYear; StopMonth = SimMonth; StopDay = SimDay;
		StopHour = SimHour + 2; StopMinute = SimMinute; StopSecond = SimSecond;
		if (StopHour >= 24) { StopHour %= 24; StopDay += 1; }
	}
	else
	{
		StopYear = EY; StopMonth = EM; StopDay = ED;
		StopHour = EH; StopMinute = EMin; StopSecond = ES;
	}
	auto GetFloat = [](UEditableTextBox* Box) -> float { return (!Box || Box->GetText().IsEmpty()) ? 0.0f : FCString::Atof(*Box->GetText().ToString()); };
	float Lat = GetFloat(LatInput);
	float Lon = GetFloat(LonInput);
	float Alt = GetFloat(AltInput);
	bool bShouldTeleport = true;
	if (FMath::IsNearlyZero(Lat) && FMath::IsNearlyZero(Lon))
	{
		bShouldTeleport = false;
	}
	if (Manager) {
		Manager->TimeSpeed = 0.0f;
		float DecimalHour = SimHour + (SimMinute / 60.0f) + (SimSecond / 3600.0f);
		Manager->SetSimulationDateTime(SimDay, SimMonth, SimYear, DecimalHour);
	}
	APlayerController* PC = GetOwningPlayer();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(World);
	if (PC && GeoRef)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->CurrentVelocity = FVector::ZeroVector;
			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyDrone->GetRootComponent());
			if (RootPrim) RootPrim->SetEnableGravity(false);
			if (bShouldTeleport)
			{
				FVector TargetLLH(Lon, Lat, Alt);
				GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
				MyDrone->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				MyDrone->SetActorLocation(FVector(0.0f, 0.0f, 5000.0f));
				MyDrone->bSearchingForGround = true;
				MyDrone->SearchTimeout = 5.0f;
				float TargetFOV = FovInput && !FovInput->GetText().IsEmpty() ? FCString::Atof(*FovInput->GetText().ToString()) : 90.0f;
				TargetFOV = FMath::Clamp(TargetFOV, 0.001f, 135.0f);
				if (PC->PlayerCameraManager) PC->PlayerCameraManager->SetFOV(TargetFOV);
				if (TargetComboBox)
				{
					FString SelectedUI = TargetComboBox->GetSelectedOption();
					FString InternalTarget = SelectedUI;
					if (SelectedUI == "Free (None)") InternalTarget = "Libera (Niciunul)";
					else if (SelectedUI == "Sun") InternalTarget = "Soare";
					else if (SelectedUI == "Moon") InternalTarget = "Luna";
					else if (SelectedUI == "Mercury") InternalTarget = "Mercur";
					else if (SelectedUI == "Mars") InternalTarget = "Marte";
					else if (SelectedUI == "Neptune") InternalTarget = "Neptun";
					MyDrone->TrackingTarget = InternalTarget;
				}
			}
			if (HideDroneCheckBox && HideDroneCheckBox->IsChecked())
			{
				MyDrone->SetActorHiddenInGame(true);
			}
		}
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
	FrameCounter = 1;
	if (StatusText) {
		StatusText->SetText(AOrbitalManager::Tr(TEXT("Se pregateste zona pentru Timelapse (5 secunde)..."), TEXT("Preparing area for Timelapse (5 seconds)...")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)));
	}
	FTimerHandle DelayTimer;
	World->GetTimerManager().SetTimer(DelayTimer, this, &UVideoExportMenuWidget::StartActualRecording, 5.0f, false);
}
void UVideoExportMenuWidget::StartActualRecording()
{
	PrepareNextFrame();
}
void UVideoExportMenuWidget::PrepareNextFrame()
{
	bool bHasReachedEnd = false;
	if (SimYear > StopYear) bHasReachedEnd = true;
	else if (SimYear == StopYear) {
		if (SimMonth > StopMonth) bHasReachedEnd = true;
		else if (SimMonth == StopMonth) {
			if (SimDay > StopDay) bHasReachedEnd = true;
			else if (SimDay == StopDay) {
				float CurrentTimeDecimal = SimHour + (SimMinute / 60.0f) + (SimSecond / 3600.0f);
				float StopTimeDecimal = StopHour + (StopMinute / 60.0f) + (StopSecond / 3600.0f);
				if (CurrentTimeDecimal >= StopTimeDecimal) bHasReachedEnd = true;
			}
		}
	}
	if (bHasReachedEnd)
	{
		StatusText->SetText(AOrbitalManager::Tr(TEXT("Cadre gata. Asamblare MP4 in 2 secunde..."), TEXT("Frames done. Assembling MP4 in 2 seconds...")));
		FTimerHandle CompileTimer;
		GetWorld()->GetTimerManager().SetTimer(CompileTimer, this, &UVideoExportMenuWidget::CompileVideo, 2.0f, false);
		return;
	}
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		float NewHour = SimHour + (SimMinute / 60.0f) + (SimSecond / 3600.0f);
		Manager->SetSimulationDateTime(SimDay, SimMonth, SimYear, NewHour);
	}
	FString PrepText = AOrbitalManager::TrString(FString::Printf(TEXT("Pregatire Cadru %d..."), FrameCounter), FString::Printf(TEXT("Preparing Frame %d..."), FrameCounter));
	StatusText->SetText(FText::FromString(PrepText));
	GetWorld()->GetTimerManager().SetTimer(CheckTimeTimerHandle, this, &UVideoExportMenuWidget::CheckRecordingTime, 0.25f, false);
}
void UVideoExportMenuWidget::CheckRecordingTime()
{
	FString FilePath = TempFramesFolder + FString::Printf(TEXT("Frame_%06d.png"), FrameCounter);
	GetHighResScreenshotConfig().FilenameOverride = FilePath;
	GetHighResScreenshotConfig().SetResolution(1920, 1080, 1.0f);
	FScreenshotRequest::RequestScreenshot(FilePath, false, false);
	FString CapText = AOrbitalManager::TrString(FString::Printf(TEXT("Captura Cadru %d Reusita!"), FrameCounter), FString::Printf(TEXT("Captured Frame %d Successfully!"), FrameCounter));
	StatusText->SetText(FText::FromString(CapText));
	FrameCounter++;
	float SecondsToSkip = FCString::Atof(*PlaybackSpeedInput->GetText().ToString());
	AdvanceSimulationTime(SecondsToSkip);
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		float DecimalHour = SimHour + (SimMinute / 60.0f) + (SimSecond / 3600.0f);
		Manager->SetSimulationDateTime(SimDay, SimMonth, SimYear, DecimalHour);
	}
	FTimerHandle PrepareTimer;
	GetWorld()->GetTimerManager().SetTimer(PrepareTimer, this, &UVideoExportMenuWidget::PrepareNextFrame, 0.25f, false);
}
void UVideoExportMenuWidget::CompileVideo()
{
	FString FFMPEGPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Encoder"), TEXT("ffmpeg.exe"));
	FPaths::MakeStandardFilename(FFMPEGPath);
	FString InputFrames = FPaths::Combine(TempFramesFolder, TEXT("Frame_%06d.png"));
	FPaths::MakeStandardFilename(InputFrames);
	FString OutputVideo = FinalSavePath;
	FPaths::MakeStandardFilename(OutputVideo);
	FFMPEGPath = FPaths::ConvertRelativePathToFull(FFMPEGPath);
	InputFrames = FPaths::ConvertRelativePathToFull(InputFrames);
	OutputVideo = FPaths::ConvertRelativePathToFull(OutputVideo);
	FString Params = FString::Printf(TEXT("-y -framerate 60 -i \"%s\" -c:v libx264 -pix_fmt yuv420p -crf 18 \"%s\""), *InputFrames, *OutputVideo);
	int32 ReturnCode = -1;
	FString OutOut, OutErr;
	FPlatformProcess::ExecProcess(*FFMPEGPath, *Params, &ReturnCode, &OutOut, &OutErr);
	if (ReturnCode == 0) {
		StatusText->SetText(AOrbitalManager::Tr(TEXT("SUCCES! MP4 salvat."), TEXT("SUCCESS! MP4 saved.")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
		IFileManager::Get().DeleteDirectory(*TempFramesFolder, false, true);
	}
	else {
		StatusText->SetText(AOrbitalManager::Tr(TEXT("Eroare FFmpeg la asamblare."), TEXT("FFmpeg encoding error.")));
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyDrone->GetRootComponent());
			if (RootPrim)
			{
				RootPrim->SetEnableGravity(true);
			}
			MyDrone->SetActorHiddenInGame(false);
		}
	}
}
void UVideoExportMenuWidget::AdvanceSimulationTime(float SecondsToAdd)
{
	SimSecond += FMath::FloorToInt(SecondsToAdd);
	if (SimSecond >= 60) { SimMinute += SimSecond / 60; SimSecond %= 60; }
	if (SimMinute >= 60) { SimHour += SimMinute / 60; SimMinute %= 60; }
	if (SimHour >= 24) { SimDay += SimHour / 24; SimHour %= 24; }
}
void UVideoExportMenuWidget::OnTakeScreenshotClicked()
{
	FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[512] = { 0 };
	FCString::Strcpy(szFile, TEXT("Captura_Eclipsa.png"));
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("Imagine PNG (*.png)\0*.png\0Imagine JPEG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0Imagine BMP (*.bmp)\0*.bmp\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("png");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World) return;
	auto GetInt = [](UEditableTextBox* Box) -> int32 { return (!Box || Box->GetText().IsEmpty()) ? 0 : FCString::Atoi(*Box->GetText().ToString()); };
	int32 CheckYear = GetInt(StartYInput);
	int32 CheckMonth = GetInt(StartMInput);
	int32 CheckDay = GetInt(StartDInput);
	int32 CheckHour = GetInt(StartHInput);
	int32 CheckMin = GetInt(StartMinInput);
	int32 CheckSec = GetInt(StartSInput);
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(World, AOrbitalManager::StaticClass()));
	if (CheckMonth < 1 || CheckMonth > 12 || CheckDay < 1 || CheckDay > 31)
	{
		if (Manager)
		{
			CheckYear = Manager->CurrentDate.Year;
			CheckMonth = Manager->CurrentDate.Month;
			CheckDay = Manager->CurrentDate.Day;
			CheckHour = FMath::FloorToInt(Manager->CurrentDate.Hour);
			float Remainder = (Manager->CurrentDate.Hour - CheckHour) * 60.0f;
			CheckMin = FMath::FloorToInt(Remainder);
			CheckSec = FMath::FloorToInt((Remainder - CheckMin) * 60.0f);
		}
		else
		{
			CheckYear = 2024; CheckMonth = 1; CheckDay = 1;
		}
	}
	SimYear = CheckYear; SimMonth = CheckMonth; SimDay = CheckDay;
	SimHour = CheckHour; SimMinute = CheckMin; SimSecond = CheckSec;
	auto GetFloat = [](UEditableTextBox* Box) -> float { return (!Box || Box->GetText().IsEmpty()) ? 0.0f : FCString::Atof(*Box->GetText().ToString()); };
	float Lat = GetFloat(LatInput);
	float Lon = GetFloat(LonInput);
	float Alt = GetFloat(AltInput);
	bool bShouldTeleport = true;
	if (FMath::IsNearlyZero(Lat) && FMath::IsNearlyZero(Lon))
	{
		bShouldTeleport = false;
	}
	if (Manager)
	{
		Manager->TimeSpeed = 0.0f;
		float DecimalHour = CheckHour + (CheckMin / 60.0f) + (CheckSec / 3600.0f);
		Manager->SetSimulationDateTime(CheckDay, CheckMonth, CheckYear, DecimalHour);
	}
	APlayerController* PC = GetOwningPlayer();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(World);
	if (PC && GeoRef)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->CurrentVelocity = FVector::ZeroVector;
			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyDrone->GetRootComponent());
			if (RootPrim)
			{
				RootPrim->SetEnableGravity(false);
			}
			if (bShouldTeleport)
			{
				FVector TargetLLH(Lon, Lat, Alt);
				GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
				MyDrone->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				MyDrone->SetActorLocation(FVector(0.0f, 0.0f, 5000.0f));
				MyDrone->bSearchingForGround = true;
				MyDrone->SearchTimeout = 5.0f;
			}
			if (FovInput && !FovInput->GetText().IsEmpty())
			{
				float TargetFOV = FCString::Atof(*FovInput->GetText().ToString());
				TargetFOV = FMath::Clamp(TargetFOV, 0.001f, 135.0f);
				UCameraComponent* DroneCamera = MyDrone->FindComponentByClass<UCameraComponent>();
				if (DroneCamera) DroneCamera->SetFieldOfView(TargetFOV);
			}
			if (HideDroneCheckBox && HideDroneCheckBox->IsChecked())
			{
				MyDrone->SetActorHiddenInGame(true);
			}
		}
		SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StatusText) {
		StatusText->SetText(AOrbitalManager::Tr(TEXT("Se pregateste cadrul... Poza se face in 3 secunde."), TEXT("Preparing frame... Photo will be taken in 3 seconds.")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.0f)));
	}
	FTimerHandle SnapTimer;
	World->GetTimerManager().SetTimer(SnapTimer, this, &UVideoExportMenuWidget::ExecuteScreenshotCapture, 2.0f, false);
}
void UVideoExportMenuWidget::ExecuteScreenshotCapture()
{
	int32 ResX = 1920;
	int32 ResY = 1080;
	if (ResolutionComboBox && ResolutionComboBox->GetSelectedOption().Contains("4K"))
	{
		ResX = 3840;
		ResY = 2160;
	}
	GetHighResScreenshotConfig().FilenameOverride = FinalSavePath;
	GetHighResScreenshotConfig().SetResolution(ResX, ResY, 1.0f);
	FScreenshotRequest::RequestScreenshot(FinalSavePath, false, false);
	if (StatusText)
	{
		FString CapText = AOrbitalManager::TrString(FString::Printf(TEXT("SUCCES! Poza salvata la: %s"), *FinalSavePath), FString::Printf(TEXT("SUCCESS! Photo saved at: %s"), *FinalSavePath));
		StatusText->SetText(FText::FromString(CapText));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone)
		{
			MyDrone->TrackingTarget = TEXT("Libera (Niciunul)");
			MyDrone->SetActorHiddenInGame(false);
			UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(MyDrone->GetRootComponent());
			if (RootPrim)
			{
				RootPrim->SetEnableGravity(true);
			}
		}
	}
	SetVisibility(ESlateVisibility::Visible);
}
void UVideoExportMenuWidget::OnGetStartTimeClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		StartYInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Year)));
		StartMInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Month)));
		StartDInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Day)));
		int32 H = FMath::FloorToInt(Manager->CurrentDate.Hour);
		int32 M = FMath::FloorToInt((Manager->CurrentDate.Hour - H) * 60.0f);
		int32 S = FMath::FloorToInt((((Manager->CurrentDate.Hour - H) * 60.0f) - M) * 60.0f);
		StartHInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), H)));
		StartMinInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), M)));
		StartSInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), S)));
	}
}
void UMainMenuWidget::OpenVideoExportMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
	if (MyHUD)
	{
		if (MyHUD->VideoExportMenuWidget == nullptr)
		{
			MyHUD->VideoExportMenuWidget = CreateWidget<UVideoExportMenuWidget>(PC, UVideoExportMenuWidget::StaticClass());
			if (MyHUD->VideoExportMenuWidget)
			{
				MyHUD->VideoExportMenuWidget->AddToViewport(100);
			}
		}
		if (MyHUD->VideoExportMenuWidget)
		{
			MyHUD->VideoExportMenuWidget->SetVisibility(ESlateVisibility::Visible);
			ToggleMenu();
		}
	}
}
void UVideoExportMenuWidget::OnGetStopTimeClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		StopYInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Year)));
		StopMInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Month)));
		StopDInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Day)));
		int32 H = FMath::FloorToInt(Manager->CurrentDate.Hour);
		int32 M = FMath::FloorToInt((Manager->CurrentDate.Hour - H) * 60.0f);
		int32 S = FMath::FloorToInt((((Manager->CurrentDate.Hour - H) * 60.0f) - M) * 60.0f);
		StopHInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), H)));
		StopMinInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), M)));
		StopSInput->SetText(FText::FromString(FString::Printf(TEXT("%02d"), S)));
	}
}
void UVideoExportMenuWidget::OnGetLocationClicked()
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APlayerController* PC = GetOwningPlayer();
	if (GeoRef && PC)
	{
		ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
		if (Drona)
		{
			FVector Coords = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Drona->GetActorLocation());
			LatInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Coords.Y)));
			LonInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Coords.X)));
			AltInput->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Coords.Z)));
		}
	}
}
TSharedRef<SWidget> UCameraMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.05f, 0.95f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(450.0f, 500.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.15f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("PERSPECTIVE CAMERA"), TEXT("CAMERA PERSPECTIVES")));
	Title->SetFont(GetFont(14, true));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 1.0f)));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(GetFont(14, true));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UCameraMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	auto CreateCameraOption = [&](UButton*& OutBtn, FString CamName, FString Desc, FLinearColor AccentColor) {
		OutBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		OutBtn->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.2f, 1.0f));
		MainLayout->AddChildToVerticalBox(OutBtn)->SetPadding(FMargin(20.f, 5.f, 20.f, 10.f));
		UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UButtonSlot* BtnSlot = Cast<UButtonSlot>(OutBtn->AddChild(HBox)))
		{
			BtnSlot->SetHorizontalAlignment(HAlign_Fill);
			BtnSlot->SetVerticalAlignment(VAlign_Fill);
		}
		UBorder* ColorBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		ColorBar->SetBrushColor(AccentColor);
		UHorizontalBoxSlot* BarSlot = HBox->AddChildToHorizontalBox(ColorBar);
		FSlateChildSize BarSize;
		BarSize.SizeRule = ESlateSizeRule::Fill;
		BarSize.Value = 0.02f;
		BarSlot->SetSize(BarSize);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* VBoxSlot = HBox->AddChildToHorizontalBox(VBox);
		FSlateChildSize TextSize;
		TextSize.SizeRule = ESlateSizeRule::Fill;
		TextSize.Value = 0.98f;
		VBoxSlot->SetSize(TextSize);
		VBoxSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* TitleTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TitleTxt->SetText(FText::FromString(CamName));
		TitleTxt->SetFont(GetFont(12, true));
		VBox->AddChildToVerticalBox(TitleTxt)->SetPadding(FMargin(0.f, 5.f, 0.f, 2.f));
		UTextBlock* DescTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DescTxt->SetText(FText::FromString(Desc));
		DescTxt->SetFont(GetFont(9, false));
		DescTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
		VBox->AddChildToVerticalBox(DescTxt)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
		};
	CreateCameraOption(Btn_FPV,
		AOrbitalManager::TrString(TEXT("1. Mod Cockpit (FPV)"), TEXT("1. Cockpit Mode (FPV)")),
		AOrbitalManager::TrString(TEXT("Perspectiva din interiorul dronei. Perfect pentru explorare realista."), TEXT("Perspective from inside the drone. Perfect for realistic exploration.")),
		FLinearColor(1.0f, 0.2f, 0.2f));
	Btn_FPV->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraFPV);
	CreateCameraOption(Btn_TPV,
		AOrbitalManager::TrString(TEXT("2. Mod Standard (Urmarire)"), TEXT("2. Standard Mode (Follow)")),
		AOrbitalManager::TrString(TEXT("Camera pozitionata in spatele dronei pentru vizibilitate maxima."), TEXT("Camera positioned behind the drone for maximum visibility.")),
		FLinearColor(0.2f, 1.0f, 0.2f));
	Btn_TPV->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraTPV);
	CreateCameraOption(Btn_Cinematic,
		AOrbitalManager::TrString(TEXT("3. Orbitare Cinematica"), TEXT("3. Cinematic Orbit")),
		AOrbitalManager::TrString(TEXT("Perspectiva larga de film, unghi oblic si departat."), TEXT("Wide film perspective, oblique and distant angle.")),
		FLinearColor(1.0f, 0.8f, 0.2f));
	Btn_Cinematic->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraCinematic);
	CreateCameraOption(Btn_TopDown,
		AOrbitalManager::TrString(TEXT("4. Vedere Satelit (Zenith)"), TEXT("4. Satellite View (Zenith)")),
		AOrbitalManager::TrString(TEXT("Privire verticala de deasupra (90 de grade). Ideal pentru harti."), TEXT("Vertical view from above (90 degrees). Ideal for maps.")),
		FLinearColor(0.2f, 0.6f, 1.0f));
	Btn_TopDown->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraTopDown);
	CreateCameraOption(Btn_LEO,
		AOrbitalManager::TrString(TEXT("5. Orbita Joasa (LEO - 400km)"), TEXT("5. Low Earth Orbit (LEO - 400km)")),
		AOrbitalManager::TrString(TEXT("Zbor orbital. Viteza reala ISS (1 rotatie la 90 min)."), TEXT("Orbital flight. Real ISS speed (1 rotation per 90 min).")),
		FLinearColor(0.8f, 0.2f, 1.0f));
	Btn_LEO->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraLEO);
	CreateCameraOption(Btn_GEO,
		AOrbitalManager::TrString(TEXT("6. Orbita Geostationara (35.786km)"), TEXT("6. Geostationary Orbit (35,786km)")),
		AOrbitalManager::TrString(TEXT("Vedere globala de departare. Sincron cu rotatia Terrei."), TEXT("Global distant view. Synchronous with Earth's rotation.")),
		FLinearColor(1.0f, 0.6f, 0.0f));
	Btn_GEO->OnClicked.AddDynamic(this, &UCameraMenuWidget::SetCameraGEO);
	return Super::RebuildWidget();
}
void UCameraMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = true;
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD)
		{
			MyHUD->CameraMenuWidget = nullptr;
		}
	}
}
void UMainMenuWidget::OpenCameraMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
	if (MyHUD)
	{
		if (MyHUD->CameraMenuWidget == nullptr)
		{
			MyHUD->CameraMenuWidget = CreateWidget<UCameraMenuWidget>(PC, UCameraMenuWidget::StaticClass());
			if (MyHUD->CameraMenuWidget)
			{
				MyHUD->CameraMenuWidget->AddToViewport(100);
			}
		}
		if (MyHUD->CameraMenuWidget)
		{
			MyHUD->CameraMenuWidget->SetVisibility(ESlateVisibility::Visible);
			ToggleMenu();
		}
	}
}
void UCameraMenuWidget::SetCameraFPV()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (bWasInSpace)
		{
			GeoRef->SetOriginLongitudeLatitudeHeight(SavedEarthLLH);
			MyDrone->SetActorLocation(FVector::ZeroVector);
			bWasInSpace = false;
		}
		MyDrone->SetActorHiddenInGame(false);
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(false); }
		USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm)
		{
			SpringArm->SetUsingAbsoluteRotation(false);
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->TargetArmLength = 0.0f;
			SpringArm->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}
void UCameraMenuWidget::SetCameraTPV()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (bWasInSpace)
		{
			GeoRef->SetOriginLongitudeLatitudeHeight(SavedEarthLLH);
			MyDrone->SetActorLocation(FVector::ZeroVector);
			bWasInSpace = false;
		}
		MyDrone->SetActorHiddenInGame(false);
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(false); }
		USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm)
		{
			SpringArm->SetUsingAbsoluteRotation(false);
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->TargetArmLength = 600.0f;
			SpringArm->SetRelativeRotation(FRotator(-15.0f, 0.0f, 0.0f));
		}
	}
}
void UCameraMenuWidget::SetCameraCinematic()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (bWasInSpace)
		{
			GeoRef->SetOriginLongitudeLatitudeHeight(SavedEarthLLH);
			MyDrone->SetActorLocation(FVector::ZeroVector);
			bWasInSpace = false;
		}
		MyDrone->SetActorHiddenInGame(false);
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(false); }
		USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm)
		{
			SpringArm->SetUsingAbsoluteRotation(false);
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->TargetArmLength = 1500.0f;
			SpringArm->SetRelativeRotation(FRotator(-25.0f, 45.0f, 0.0f));
		}
	}
}
void UCameraMenuWidget::SetCameraTopDown()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (bWasInSpace)
		{
			GeoRef->SetOriginLongitudeLatitudeHeight(SavedEarthLLH);
			MyDrone->SetActorLocation(FVector::ZeroVector);
			bWasInSpace = false;
		}
		MyDrone->SetActorHiddenInGame(false);
		MyDrone->bMapModeActive = true;
		if (MyDrone->MapSpringArm)
		{
			MyDrone->MapSpringArm->SetUsingAbsoluteRotation(true);
			MyDrone->MapSpringArm->SetWorldRotation(FRotator(-90.0f, -90.0f, 0.0f));
			MyDrone->MapSpringArm->TargetArmLength = 1000.0f;
		}
		if (MyDrone->Camera) MyDrone->Camera->SetActive(false);
		if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(true);
		AHUD* BaseHUD = PC->GetHUD();
		if (BaseHUD) BaseHUD->SetActorHiddenInGame(true);
	}
}
void UCameraMenuWidget::SetCameraLEO()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (!bWasInSpace)
		{
			SavedEarthLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
			bWasInSpace = true;
		}
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); }
		USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm) {
			SpringArm->TargetArmLength = 0.0f;
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->SetRelativeRotation(FRotator(-89.9f, 0.0f, 0.0f));
		}
		MyDrone->SetActorHiddenInGame(true);
		if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(true);
		CurrentOrbitLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
		CurrentOrbitLLH.Z = 400000.0f;
		GeoRef->SetOriginLongitudeLatitudeHeight(CurrentOrbitLLH);
		CurrentOrbitDegreesPerSecond = 360.0f / 5400.0f;
		GetWorld()->GetTimerManager().SetTimer(OrbitTimerHandle, this, &UCameraMenuWidget::UpdateOrbit, 0.02f, true);
	}
}
void UCameraMenuWidget::SetCameraGEO()
{
	GetWorld()->GetTimerManager().ClearTimer(OrbitTimerHandle);
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (MyDrone && GeoRef)
	{
		if (!bWasInSpace)
		{
			SavedEarthLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
			bWasInSpace = true;
		}
		if (MyDrone->bMapModeActive) { MyDrone->bMapModeActive = false; if (MyDrone->MapCamera) MyDrone->MapCamera->SetActive(false); if (MyDrone->Camera) MyDrone->Camera->SetActive(true); }
		USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
		if (SpringArm) {
			SpringArm->TargetArmLength = 0.0f;
			SpringArm->bUsePawnControlRotation = false;
			SpringArm->SetRelativeRotation(FRotator(-89.9f, 0.0f, 0.0f));
		}
		MyDrone->SetActorHiddenInGame(true);
		if (PC->GetHUD()) PC->GetHUD()->SetActorHiddenInGame(true);
		CurrentOrbitLLH = GeoRef->GetOriginLongitudeLatitudeHeight();
		CurrentOrbitLLH.Z = 35786000.0f;
		GeoRef->SetOriginLongitudeLatitudeHeight(CurrentOrbitLLH);
		CurrentOrbitDegreesPerSecond = 360.0f / 86400.0f;
		GetWorld()->GetTimerManager().SetTimer(OrbitTimerHandle, this, &UCameraMenuWidget::UpdateOrbit, 0.02f, true);
	}
}
void UCameraMenuWidget::UpdateOrbit()
{
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APlayerController* PC = GetOwningPlayer();
	if (!GeoRef || !PC) return;
	ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
	if (!MyDrone) return;
	float TimeMultiplier = 1.0f;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager && Manager->TimeSpeed != 0.0f)
	{
		TimeMultiplier = Manager->TimeSpeed;
	}
	CurrentOrbitLLH.X += (CurrentOrbitDegreesPerSecond * 0.02f * TimeMultiplier);
	while (CurrentOrbitLLH.X > 180.0f) CurrentOrbitLLH.X -= 360.0f;
	while (CurrentOrbitLLH.X < -180.0f) CurrentOrbitLLH.X += 360.0f;
	FVector UELocation = GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(CurrentOrbitLLH);
	FVector LLH_Sus = CurrentOrbitLLH;
	LLH_Sus.Z += 1000.0f;
	FVector UELocation_Sus = GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(LLH_Sus);
	FVector UpVector = (UELocation_Sus - UELocation).GetSafeNormal();
	FVector LLH_Inainte = CurrentOrbitLLH;
	LLH_Inainte.X += 0.1f;
	while (LLH_Inainte.X > 180.0f) LLH_Inainte.X -= 360.0f;
	while (LLH_Inainte.X < -180.0f) LLH_Inainte.X += 360.0f;
	FVector UELocation_Inainte = GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(LLH_Inainte);
	FVector ForwardVector = (UELocation_Inainte - UELocation).GetSafeNormal();
	FRotator NewRotation = FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).Rotator();
	MyDrone->SetActorLocationAndRotation(UELocation, NewRotation);
	if (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		float MouseX = 0.0f, MouseY = 0.0f;
		PC->GetInputMouseDelta(MouseX, MouseY);
		if (MouseX != 0.0f || MouseY != 0.0f)
		{
			USpringArmComponent* SpringArm = MyDrone->FindComponentByClass<USpringArmComponent>();
			if (SpringArm)
			{
				FRotator CamRot = SpringArm->GetRelativeRotation();
				CamRot.Yaw += MouseX * 1.5f;
				CamRot.Pitch += MouseY * 1.5f;
				CamRot.Pitch = FMath::Clamp(CamRot.Pitch, -89.9f, 89.9f);
				SpringArm->SetRelativeRotation(CamRot);
			}
		}
	}
}
void UMainMenuWidget::CloseTelemetryMenu()
{
	if (TelemetryMenuInstance && TelemetryMenuInstance->IsInViewport())
	{
		TelemetryMenuInstance->OnCloseClicked();
	}
}
void UMainMenuWidget::OpenTelemetryMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	if (TelemetryMenuInstance == nullptr)
	{
		TelemetryMenuInstance = CreateWidget<UAstroTelemetryWidget>(PC, UAstroTelemetryWidget::StaticClass());
		if (TelemetryMenuInstance)
		{
			TelemetryMenuInstance->AddToViewport(0);
		}
	}
	if (TelemetryMenuInstance)
	{
		TelemetryMenuInstance->SetVisibility(ESlateVisibility::Visible);
		TelemetryMenuInstance->StartTelemetry();
		if (MenuDropdownContainer)
		{
			MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
TSharedRef<SWidget> UAstroTelemetryWidget::RebuildWidget()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;
	UBorder* PanelBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelBg->SetBrushColor(FLinearColor(0.02f, 0.05f, 0.08f, 0.85f));
	UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(PanelBg);
	BgSlot->SetAnchors(FAnchors(1.0f, 0.5f));
	BgSlot->SetAlignment(FVector2D(1.0f, 0.5f));
	BgSlot->SetPosition(FVector2D(-20.f, 0.f));
	BgSlot->SetSize(FVector2D(400.0f, 400.0f));
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelBg->SetContent(VBox);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	VBox->AddChildToVerticalBox(HeaderBox)->SetPadding(FMargin(10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleTextBlock = Title;
	Title->SetText(AOrbitalManager::Tr(TEXT("TELEMETRIE ASTRONOMICA"), TEXT("ASTRONOMICAL TELEMETRY")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.0f, 0.8f)));
	Title->SetFont(GetFont(14, true));
	HeaderBox->AddChildToHorizontalBox(Title);
	USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Sp)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Red);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString(" X "));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UAstroTelemetryWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrushColor(FLinearColor(0.2f, 1.0f, 0.8f, 0.5f));
	VBox->AddChildToVerticalBox(Line)->SetPadding(FMargin(10.f, 0.f, 10.f, 10.f));
	auto AddDataRow = [&](FString LabelText, UTextBlock*& OutLabel, UTextBlock*& OutValueText) {
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		OutLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutLabel->SetText(FText::FromString(LabelText));
		OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
		OutLabel->SetFont(GetFont(12));
		Row->AddChildToHorizontalBox(OutLabel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OutValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutValueText->SetText(FText::FromString("---"));
		OutValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		OutValueText->SetFont(GetFont(12, true));
		Row->AddChildToHorizontalBox(OutValueText);
		VBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(15.f, 5.f, 15.f, 5.f));
		};
	AddDataRow(TEXT("Tinta:"), LabelTarget, TargetNameText);
	AddDataRow(TEXT("Distanta:"), LabelDistance, DistanceKmText);
	AddDataRow(TEXT("Distanta (UA):"), LabelDistanceAU, DistanceAUText);
	AddDataRow(TEXT("Timp Lumina:"), LabelLightTime, DistanceLightText);
	AddDataRow(TEXT("Dimens. Angulara:"), LabelAngularSize, AngularSizeText);
	AddDataRow(TEXT("Magnitudine:"), LabelMagnitude, MagnitudeText);
	AddDataRow(TEXT("Altitudine/Azimut:"), LabelAltAz, AltAzText);
	AddDataRow(TEXT("Iluminare:"), LabelIllumination, IlluminationText);
	AddDataRow(TEXT("Elongatie:"), LabelElongation, ElongationText);
	return Super::RebuildWidget();
}
void UAstroTelemetryWidget::StartTelemetry()
{
	GetWorld()->GetTimerManager().SetTimer(TelemetryTimerHandle, this, &UAstroTelemetryWidget::UpdateTelemetryInfo, 0.1f, true);
}
void UAstroTelemetryWidget::OnCloseClicked()
{
	GetWorld()->GetTimerManager().ClearTimer(TelemetryTimerHandle);
	SetVisibility(ESlateVisibility::Collapsed);
}
void UAstroTelemetryWidget::UpdateTelemetryInfo()
{
	if (!OrbitalManagerRef)
	{
		OrbitalManagerRef = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (!OrbitalManagerRef) return;
	}
	if (!TargetNameText || !DistanceKmText || !DistanceAUText || !DistanceLightText ||
		!AngularSizeText || !MagnitudeText || !IlluminationText || !ElongationText || !AltAzText || !TitleTextBlock)
	{
		return;
	}
	TitleTextBlock->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("TELEMETRIE ASTRONOMICA"), TEXT("ASTRONOMICAL TELEMETRY"))));
	LabelTarget->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Tinta Curenta:"), TEXT("Current Target:"))));
	LabelDistance->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Distanta:"), TEXT("Distance:"))));
	LabelDistanceAU->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Distanta (UA):"), TEXT("Distance (AU):"))));
	LabelLightTime->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Timp Lumina:"), TEXT("Light Time:"))));
	LabelAngularSize->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Dimens. Angulara:"), TEXT("Angular Size:"))));
	LabelMagnitude->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Magnitudine:"), TEXT("Magnitude:"))));
	LabelAltAz->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Altitudine / Azimut:"), TEXT("Altitude / Azimuth:"))));
	LabelIllumination->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Iluminare Disc:"), TEXT("Disc Illumination:"))));
	LabelElongation->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("Elongatie Solara:"), TEXT("Solar Elongation:"))));
	FString CurrentTarget = "Libera (Niciunul)";
	APawn* PlayerPawn = nullptr;
	APlayerController* PC100 = GetOwningPlayer();
	if (PC100)
	{
		PlayerPawn = PC100->GetPawn();
		ADronaMea* MyDrone = Cast<ADronaMea>(PlayerPawn);
		if (MyDrone)
		{
			CurrentTarget = MyDrone->TrackingTarget;
		}
	}
	if (CurrentTarget == "None" || CurrentTarget.IsEmpty() || CurrentTarget == "Libera (Niciunul)")
	{
		TargetNameText->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("FARA TINTA"), TEXT("NO TARGET"))));
		DistanceKmText->SetText(FText::FromString("---"));
		DistanceAUText->SetText(FText::FromString("---"));
		DistanceLightText->SetText(FText::FromString("---"));
		AngularSizeText->SetText(FText::FromString("---"));
		MagnitudeText->SetText(FText::FromString("---"));
		IlluminationText->SetText(FText::FromString("---"));
		ElongationText->SetText(FText::FromString("---"));
		AltAzText->SetText(FText::FromString("---"));
		return;
	}
	double D = 0.0;
	double R = 0.0;
	FText MagText;
	float CurrentAlt = 0.0f;
	float CurrentAz = 0.0f;
	FVector CurrentTargetDir = FVector::ZeroVector;
	auto TranslateBodyName = [](FString Name) -> FText {
		if (Name == "Soare" || Name == "Sun") return FText::FromString(AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun")));
		if (Name == "Luna" || Name == "Moon") return FText::FromString(AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon")));
		if (Name == "Mercur" || Name == "Mercury") return FText::FromString(AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury")));
		if (Name == "Pamant" || Name == "Earth") return FText::FromString(AOrbitalManager::TrString(TEXT("Pamant"), TEXT("Earth")));
		if (Name == "Marte" || Name == "Mars") return FText::FromString(AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars")));
		if (Name == "Neptun" || Name == "Neptune") return FText::FromString(AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune")));
		return FText::FromString(Name);
		};
	FVector SunDir = FVector::ZeroVector;
	if (OrbitalManagerRef->SunSkyActor)
	{
		UDirectionalLightComponent* Light = OrbitalManagerRef->SunSkyActor->FindComponentByClass<UDirectionalLightComponent>();
		if (Light) SunDir = -Light->GetForwardVector();
	}
	FVector MoonDir = FVector::ZeroVector;
	if (OrbitalManagerRef->ControlledMoon && PlayerPawn)
	{
		MoonDir = (OrbitalManagerRef->ControlledMoon->GetActorLocation() - PlayerPawn->GetActorLocation()).GetSafeNormal();
	}
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	auto CalcAltAz = [GeoRef, PlayerPawn](FVector Direction, float& OutAlt, float& OutAz) {
		FVector NormDir = Direction.GetSafeNormal();
		if (!GeoRef || !PlayerPawn)
		{
			FRotator Rot = NormDir.Rotation();
			OutAlt = Rot.Pitch; OutAz = Rot.Yaw;
			if (OutAz < 0.0f) OutAz += 360.0f;
			return;
		}
		FVector PlayerLoc = PlayerPawn->GetActorLocation();
		FVector LLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(PlayerLoc);
		double LatRad = FMath::DegreesToRadians(LLH.Y);
		double LonRad = FMath::DegreesToRadians(LLH.X);
		double cosLat = FMath::Cos(LatRad);
		double sinLat = FMath::Sin(LatRad);
		double cosLon = FMath::Cos(LonRad);
		double sinLon = FMath::Sin(LonRad);
		FVector East_ECEF(-sinLon, cosLon, 0.0);
		FVector North_ECEF(-sinLat * cosLon, -sinLat * sinLon, cosLat);
		FVector Up_ECEF(cosLat * cosLon, cosLat * sinLon, sinLat);
		FVector East_Unreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(East_ECEF).GetSafeNormal();
		FVector North_Unreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(North_ECEF).GetSafeNormal();
		FVector Up_Unreal = GeoRef->TransformEarthCenteredEarthFixedDirectionToUnreal(Up_ECEF).GetSafeNormal();
		double DotUp = FVector::DotProduct(NormDir, Up_Unreal);
		DotUp = FMath::Clamp(DotUp, -1.0, 1.0);
		OutAlt = FMath::RadiansToDegrees(FMath::Asin(DotUp));
		double DotNorth = FVector::DotProduct(NormDir, North_Unreal);
		double DotEast = FVector::DotProduct(NormDir, East_Unreal);
		OutAz = FMath::RadiansToDegrees(FMath::Atan2(DotEast, DotNorth));
		if (OutAz < 0.0f)
		{
			OutAz += 360.0f;
		}
		};
	if (CurrentTarget == "Soare" || CurrentTarget == "Sun")
	{
		TargetNameText->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("SOARE (G2V Star)"), TEXT("SUN (G2V Star)"))));
		D = OrbitalManagerRef->CurrentSunDistanceKm;
		R = 696340.0;
		CurrentTargetDir = SunDir;
		CalcAltAz(SunDir, CurrentAlt, CurrentAz);
		double BaseMag = -26.74;
		float CovFrac = OrbitalManagerRef->SunCoverage / 100.0f;
		if (CovFrac > 0.0f)
		{
			float Flux = FMath::Max(1.0f - CovFrac, 0.00001f);
			double EclipseMag = BaseMag - (2.5 * FMath::LogX(10.0, Flux));
			FString MagFormatStr = AOrbitalManager::TrString(TEXT("{0} (Acoperire: {1}%)"), TEXT("{0} (Coverage: {1}%)"));
			MagText = FText::Format(FText::FromString(MagFormatStr), FText::AsNumber(EclipseMag, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(OrbitalManagerRef->SunCoverage, &FNumberFormattingOptions::DefaultNoGrouping()));
		}
		else
		{
			MagText = FText::FromString(FString::Printf(TEXT("%.2f (Max)"), BaseMag));
		}
	}
	else if (CurrentTarget == "Luna" || CurrentTarget == "Moon")
	{
		TargetNameText->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("LUNA (Satelit Natural)"), TEXT("MOON (Natural Satellite)"))));
		D = OrbitalManagerRef->CurrentMoonDistanceKm;
		R = 1737.4;
		CurrentTargetDir = MoonDir;
		CalcAltAz(MoonDir, CurrentAlt, CurrentAz);
		double PhaseFlux = 1.0;
		if (!SunDir.IsNearlyZero() && !MoonDir.IsNearlyZero())
		{
			double DotP = FVector::DotProduct(SunDir, MoonDir);
			PhaseFlux = 0.5 * (1.0 - DotP);
		}
		PhaseFlux = FMath::Max(PhaseFlux, 0.001);
		double BaseMoonMag = -12.74;
		double DistRatioM = D / 384400.0;
		double DynamicMoonMag = BaseMoonMag + (5.0 * FMath::LogX(10.0, DistRatioM)) - (2.5 * FMath::LogX(10.0, PhaseFlux));
		if (PhaseFlux > 0.9)
		{
			FString FullStr = AOrbitalManager::TrString(TEXT("{0} (Luna Plina)"), TEXT("{0} (Full Moon)"));
			MagText = FText::Format(FText::FromString(FullStr), FText::AsNumber(DynamicMoonMag, &FNumberFormattingOptions::DefaultNoGrouping()));
		}
		else if (PhaseFlux < 0.1)
		{
			FString NewStr = AOrbitalManager::TrString(TEXT("{0} (Luna Noua)"), TEXT("{0} (New Moon)"));
			MagText = FText::Format(FText::FromString(NewStr), FText::AsNumber(DynamicMoonMag, &FNumberFormattingOptions::DefaultNoGrouping()));
		}
		else
		{
			MagText = FText::FromString(FString::Printf(TEXT("%.2f"), DynamicMoonMag));
		}
	}
	else
	{
		FString SearchName = CurrentTarget;
		if (SearchName == "Mercury") SearchName = "Mercur";
		else if (SearchName == "Mars") SearchName = "Marte";
		else if (SearchName == "Earth") SearchName = "Pamant";
		else if (SearchName == "Neptune") SearchName = "Neptun";
		else if (SearchName == "Ganymede") SearchName = "Ganimede";
		else if (SearchName == "Callisto") SearchName = "Calisto";
		bool bFoundPlanet = false;
		bool bFoundTarget = false;
		for (const auto& Planet : OrbitalManagerRef->PlanetsList)
		{
			if (CurrentTarget == Planet.Name || SearchName == Planet.Name)
			{
				FText TranslatedPlanet = TranslateBodyName(Planet.Name);
				FString PlanetFormatStr = AOrbitalManager::TrString(TEXT("{0} (Planeta)"), TEXT("{0} (Planet)"));
				TargetNameText->SetText(FText::Format(FText::FromString(PlanetFormatStr), TranslatedPlanet));
				D = Planet.DistanceKm;
				CurrentTargetDir = Planet.Direction;
				CalcAltAz(Planet.Direction, CurrentAlt, CurrentAz);
				double V10 = 0.0;
				if (Planet.Name == "Mercur" || Planet.Name == "Mercury") { R = 2439.7; V10 = -0.60; }
				else if (Planet.Name == "Venus") { R = 6051.8; V10 = -4.40; }
				else if (Planet.Name == "Marte" || Planet.Name == "Mars") { R = 3389.5; V10 = -1.52; }
				else if (Planet.Name == "Jupiter") { R = 71492.0; V10 = -9.40; }
				else if (Planet.Name == "Saturn") { R = 58232.0; V10 = -8.88; }
				else if (Planet.Name == "Uranus") { R = 25362.0; V10 = -7.19; }
				else if (Planet.Name == "Neptun" || Planet.Name == "Neptune") { R = 24622.0; V10 = -6.87; }
				double DeltaAU = D / 149597870.7;
				double ElongationDot = FVector::DotProduct(CurrentTargetDir.GetSafeNormal(), SunDir.GetSafeNormal());
				double SunDKm = OrbitalManagerRef->CurrentSunDistanceKm;
				double r_km = FMath::Sqrt((D * D) + (SunDKm * SunDKm) - (2.0 * D * SunDKm * ElongationDot));
				double r_AU = r_km / 149597870.7;
				double DynamicMagP = V10 + 5.0 * FMath::LogX(10.0, r_AU * DeltaAU);
				MagText = FText::FromString(FString::Printf(TEXT("%.2f"), DynamicMagP));
				bFoundPlanet = true;
				bFoundTarget = true;
				break;
			}
		}
		if (!bFoundTarget)
		{
			for (const auto& Moon : OrbitalManagerRef->MajorMoonsList)
			{
				if (CurrentTarget == Moon.Name || SearchName == Moon.Name)
				{
					D = Moon.DistanceKm;
					CurrentTargetDir = Moon.Direction;
					CalcAltAz(Moon.Direction, CurrentAlt, CurrentAz);
					double V10 = 0.0;
					FString ParentPlanet = TEXT("Necunoscut");
					if (Moon.Name == "Io") { R = 1821.6; V10 = -1.68; ParentPlanet = TEXT("Jupiter"); }
					else if (Moon.Name == "Europa") { R = 1560.8; V10 = -1.41; ParentPlanet = TEXT("Jupiter"); }
					else if (Moon.Name == "Ganymede" || Moon.Name == "Ganimede") { R = 2634.1; V10 = -2.09; ParentPlanet = TEXT("Jupiter"); }
					else if (Moon.Name == "Callisto" || Moon.Name == "Calisto") { R = 2410.3; V10 = -1.05; ParentPlanet = TEXT("Jupiter"); }
					else if (Moon.Name == "Titan") { R = 2574.7; V10 = -1.25; ParentPlanet = TEXT("Saturn"); }
					else if (Moon.Name == "Triton") { R = 1353.4; V10 = -1.20; ParentPlanet = TEXT("Neptun"); }
					FText TranslatedMoon = TranslateBodyName(Moon.Name);
					FText TranslatedParent = TranslateBodyName(ParentPlanet);
					FString MoonFormatStr = AOrbitalManager::TrString(TEXT("{0} (Satelit {1})"), TEXT("{0} (Moon of {1})"));
					TargetNameText->SetText(FText::Format(FText::FromString(MoonFormatStr), TranslatedMoon, TranslatedParent));
					double DeltaAU = D / 149597870.7;
					double ElongationDot = FVector::DotProduct(CurrentTargetDir.GetSafeNormal(), SunDir.GetSafeNormal());
					double SunDKm = OrbitalManagerRef->CurrentSunDistanceKm;
					double r_km = FMath::Sqrt((D * D) + (SunDKm * SunDKm) - (2.0 * D * SunDKm * ElongationDot));
					double r_AU = r_km / 149597870.7;
					double DynamicMagM = V10 + 5.0 * FMath::LogX(10.0, r_AU * DeltaAU);
					MagText = FText::FromString(FString::Printf(TEXT("%.2f"), DynamicMagM));
					bFoundTarget = true;
					break;
				}
			}
		}
		if (!bFoundTarget)
		{
			FText TranslatedTarget = TranslateBodyName(CurrentTarget);
			FString UnknownFormatStr = AOrbitalManager::TrString(TEXT("{0} (Necunoscut)"), TEXT("{0} (Unknown)"));
			TargetNameText->SetText(FText::Format(FText::FromString(UnknownFormatStr), TranslatedTarget));
			D = 1000.0;
		}
	}
	bool bUseImperial = false;
	if (GConfig)
	{
		FString SavedUnit = TEXT("KM");
		GConfig->GetString(TEXT("SetariCustom"), TEXT("UnitatiMasura"), SavedUnit, GGameUserSettingsIni);
		if (SavedUnit == TEXT("MI"))
		{
			bUseImperial = true;
		}
	}
	double DisplayDistance = D;
	FString UnitString = TEXT("km");
	if (bUseImperial)
	{
		DisplayDistance = D * 0.621371;
		UnitString = TEXT("mi");
	}
	FString DistString = FString::Printf(TEXT("%s %s"), *FText::AsNumber((int64)DisplayDistance).ToString(), *UnitString);
	DistanceKmText->SetText(FText::FromString(DistString));
	double DistAU = D / 149597870.7;
	FString AUFormatStr = AOrbitalManager::TrString(TEXT("{0} UA"), TEXT("{0} AU"));
	DistanceAUText->SetText(FText::Format(FText::FromString(AUFormatStr), FText::AsNumber(DistAU, &FNumberFormattingOptions::DefaultNoGrouping())));
	const double SpeedOfLight = 299792.458;
	double LightSeconds = D / SpeedOfLight;
	if (LightSeconds < 60.0) {
		FString SecFormatStr = AOrbitalManager::TrString(TEXT("{0} secunde"), TEXT("{0} seconds"));
		DistanceLightText->SetText(FText::Format(FText::FromString(SecFormatStr), FText::AsNumber(LightSeconds, &FNumberFormattingOptions::DefaultNoGrouping())));
	}
	else if (LightSeconds < 3600.0) {
		int32 L_Min = FMath::FloorToInt(LightSeconds / 60.0);
		int32 L_Sec = FMath::FloorToInt(FMath::Fmod(LightSeconds, 60.0));
		FString MinSecFormatStr = AOrbitalManager::TrString(TEXT("{0} min {1} sec"), TEXT("{0} min {1} sec"));
		DistanceLightText->SetText(FText::Format(FText::FromString(MinSecFormatStr), FText::AsNumber(L_Min), FText::AsNumber(L_Sec)));
	}
	else {
		int32 L_Ore = FMath::FloorToInt(LightSeconds / 3600.0);
		int32 L_Min = FMath::FloorToInt(FMath::Fmod(LightSeconds / 60.0, 60.0));
		FString HrMinFormatStr = AOrbitalManager::TrString(TEXT("{0} ore {1} min"), TEXT("{0} hrs {1} min"));
		DistanceLightText->SetText(FText::Format(FText::FromString(HrMinFormatStr), FText::AsNumber(L_Ore), FText::AsNumber(L_Min)));
	}
	if (D > 0 && R > 0)
	{
		double AngularSizeRads = 2.0 * FMath::Atan(R / D);
		double AngularSizeArcSec = AngularSizeRads * (180.0 / PI) * 3600.0;
		if (AngularSizeArcSec > 1000.0) {
			AngularSizeText->SetText(FText::FromString(FString::Printf(TEXT("%.2f arcmin"), AngularSizeArcSec / 60.0)));
		}
		else {
			AngularSizeText->SetText(FText::FromString(FString::Printf(TEXT("%.2f arcsec"), AngularSizeArcSec)));
		}
	}
	else
	{
		AngularSizeText->SetText(FText::FromString("---"));
	}
	MagnitudeText->SetText(MagText);
	AltAzText->SetText(FText::FromString(FString::Printf(TEXT("Alt: %.1f\u00B0 / Az: %.1f\u00B0"), CurrentAlt, CurrentAz)));
	if (CurrentTarget == "Soare" || CurrentTarget == "Sun")
	{
		IlluminationText->SetText(FText::FromString(AOrbitalManager::TrString(TEXT("100.0% (Sursa Lumina)"), TEXT("100.0% (Light Source)"))));
		FString ElongationSource = FString::Printf(TEXT("0.0\u00B0 %s"), *AOrbitalManager::TrString(TEXT("(Sursa)"), TEXT("(Source)")));
		ElongationText->SetText(FText::FromString(ElongationSource));
	}
	else if (!CurrentTargetDir.IsNearlyZero() && !SunDir.IsNearlyZero())
	{
		FVector TargetDirNorm = CurrentTargetDir.GetSafeNormal();
		double Tx = TargetDirNorm.X; double Ty = TargetDirNorm.Y; double Tz = TargetDirNorm.Z;
		double Sx = SunDir.X; double Sy = SunDir.Y; double Sz = SunDir.Z;
		double ElongationDot = (Tx * Sx) + (Ty * Sy) + (Tz * Sz);
		ElongationDot = FMath::Clamp(ElongationDot, -1.0, 1.0);
		double ElongationDeg = FMath::RadiansToDegrees(FMath::Acos(ElongationDot));
		ElongationText->SetText(FText::FromString(FString::Printf(TEXT("%.1f\u00B0"), ElongationDeg)));
		double TPosX = Tx * D;
		double TPosY = Ty * D;
		double TPosZ = Tz * D;
		double SunD = OrbitalManagerRef->CurrentSunDistanceKm;
		double SPosX = Sx * SunD;
		double SPosY = Sy * SunD;
		double SPosZ = Sz * SunD;
		double ToSunX = SPosX - TPosX;
		double ToSunY = SPosY - TPosY;
		double ToSunZ = SPosZ - TPosZ;
		double ToSunLen = FMath::Sqrt((ToSunX * ToSunX) + (ToSunY * ToSunY) + (ToSunZ * ToSunZ));
		if (ToSunLen > 0.0001)
		{
			ToSunX /= ToSunLen; ToSunY /= ToSunLen; ToSunZ /= ToSunLen;
		}
		double ToEarthX = -Tx;
		double ToEarthY = -Ty;
		double ToEarthZ = -Tz;
		double PhaseDot = (ToSunX * ToEarthX) + (ToSunY * ToEarthY) + (ToSunZ * ToEarthZ);
		double IlluminationPct = ((1.0 + PhaseDot) / 2.0) * 100.0;
		IlluminationText->SetText(FText::FromString(FString::Printf(TEXT("%.1f %%"), IlluminationPct)));
	}
	else
	{
		IlluminationText->SetText(FText::FromString("---"));
		ElongationText->SetText(FText::FromString("---"));
	}
}
void UMainMenuWidget::RebuildTelemetryForLanguage()
{
	if (TelemetryMenuInstance)
	{
		bool bWasVisible = (TelemetryMenuInstance->GetVisibility() == ESlateVisibility::Visible);
		TelemetryMenuInstance->OnCloseClicked();
		TelemetryMenuInstance->RemoveFromParent();
		TelemetryMenuInstance = nullptr;
		if (bWasVisible)
		{
			OpenTelemetryMenu();
		}
	}
}
void UMainMenuWidget::OpenVisualFXMenu()
{
	if (MenuDropdownContainer) MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	UVisualEffectsMenuWidget* VFXWidget = CreateWidget<UVisualEffectsMenuWidget>(GetOwningPlayer(), UVisualEffectsMenuWidget::StaticClass());
	if (VFXWidget)
	{
		VFXWidget->AddToViewport(999);
	}
}
TSharedRef<SWidget> UVisualEffectsMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.95f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(380.0f, 420.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.14f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("EFECTE VIZUALE (VFX)"), TEXT("VISUAL EFFECTS (VFX)")));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	Title->SetFont(TitleFont);
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(TitleFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UVisualEffectsMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	auto CreateSliderRow = [&](USlider*& OutSlider, UTextBlock*& OutValueText, FString LabelRO, FString LabelEN, float MinVal, float MaxVal, float DefaultVal) {
		UVerticalBox* RowBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		MainLayout->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(20.f, 15.f));
		UHorizontalBox* TextBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RowBox->AddChildToVerticalBox(TextBox);
		FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
		Lbl->SetFont(LabelFont);
		TextBox->AddChildToHorizontalBox(Lbl)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OutValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		OutValueText->SetText(FText::FromString(FString::SanitizeFloat(DefaultVal)));
		OutValueText->SetFont(LabelFont);
		OutValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
		TextBox->AddChildToHorizontalBox(OutValueText)->SetHorizontalAlignment(HAlign_Right);
		OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
		OutSlider->SetMinValue(MinVal);
		OutSlider->SetMaxValue(MaxVal);
		OutSlider->SetValue(DefaultVal);
		RowBox->AddChildToVerticalBox(OutSlider)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		UBorder* Div = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Div->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.5f));
		MainLayout->AddChildToVerticalBox(Div)->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
		};
	float CurrentBloom = 1.0f;
	float CurrentStarIntensity = 20.0f;
	float CurrentEclipseFX = 1.0f;
	float CurrentExposure = 0.0f;
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone && MyDrone->Camera)
		{
			if (MyDrone->Camera->PostProcessSettings.bOverride_BloomIntensity)
			{
				CurrentBloom = MyDrone->Camera->PostProcessSettings.BloomIntensity;
			}
			if (MyDrone->Camera->PostProcessSettings.bOverride_BloomSizeScale)
			{
				CurrentEclipseFX = MyDrone->Camera->PostProcessSettings.BloomSizeScale;
			}
			if (MyDrone->Camera->PostProcessSettings.bOverride_AutoExposureBias)
			{
				CurrentExposure = MyDrone->Camera->PostProcessSettings.AutoExposureBias;
			}
		}
	}
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		CurrentStarIntensity = Manager->CurrentStarGlowMultiplier;
	}
	CreateSliderRow(BloomSlider, BloomValueText, TEXT("Intensitate Bloom (General)"), TEXT("Bloom Intensity (Overall)"), 0.0f, 10.0f, CurrentBloom);
	BloomSlider->OnValueChanged.AddDynamic(this, &UVisualEffectsMenuWidget::OnBloomChanged);
	CreateSliderRow(StarIntensitySlider, StarIntensityValueText, TEXT("Stralucire Stele"), TEXT("Star Brightness (Glow)"), 20.0f, 10000.0f, CurrentStarIntensity);
	StarIntensitySlider->OnValueChanged.AddDynamic(this, &UVisualEffectsMenuWidget::OnStarIntensityChanged);
	CreateSliderRow(EclipseFXSlider, EclipseFXValueText, TEXT("Efect Coroana Eclipsa"), TEXT("Eclipse Corona Effect"), 0.0f, 10.0f, CurrentEclipseFX);
	EclipseFXSlider->OnValueChanged.AddDynamic(this, &UVisualEffectsMenuWidget::OnEclipseFXChanged);
	CreateSliderRow(ExposureSlider, ExposureValueText, TEXT("Compensare Expunere (ISO)"), TEXT("Exposure Comp. (ISO)"), -5.0f, 5.0f, CurrentExposure);
	ExposureSlider->OnValueChanged.AddDynamic(this, &UVisualEffectsMenuWidget::OnExposureChanged);
	USpacer* ResetSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	ResetSpacer->SetSize(FVector2D(1.0f, 15.0f));
	MainLayout->AddChildToVerticalBox(ResetSpacer);
	UHorizontalBox* ResetBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ResetBox)->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	USpacer* PushRight = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	ResetBox->AddChildToHorizontalBox(PushRight)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ResetButton->SetBackgroundColor(FLinearColor(0.8f, 0.2f, 0.2f));
	UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(AOrbitalManager::Tr(TEXT("Resetare la Default"), TEXT("Reset to Default")));
	FSlateFontInfo ResetFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	ResetTxt->SetFont(ResetFont);
	ResetTxt->SetMargin(FMargin(10.f, 5.f));
	ResetButton->AddChild(ResetTxt);
	ResetButton->OnClicked.AddDynamic(this, &UVisualEffectsMenuWidget::OnResetClicked);
	ResetBox->AddChildToHorizontalBox(ResetButton);
	return Super::RebuildWidget();
}
void UVisualEffectsMenuWidget::OnCloseClicked()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
void UVisualEffectsMenuWidget::OnBloomChanged(float Value)
{
	if (BloomValueText) BloomValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone && MyDrone->Camera)
		{
			MyDrone->Camera->PostProcessSettings.bOverride_BloomIntensity = true;
			MyDrone->Camera->PostProcessSettings.BloomIntensity = Value;
		}
	}
}
void UVisualEffectsMenuWidget::OnStarIntensityChanged(float Value)
{
	if (StarIntensityValueText) StarIntensityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->SetStarGlowMultiplier(Value);
	}
}
void UVisualEffectsMenuWidget::OnEclipseFXChanged(float Value)
{
	if (EclipseFXValueText) EclipseFXValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone && MyDrone->Camera)
		{
			MyDrone->Camera->PostProcessSettings.bOverride_LensFlareIntensity = true;
			MyDrone->Camera->PostProcessSettings.LensFlareIntensity = 0.0f;
			MyDrone->Camera->PostProcessSettings.bOverride_BloomSizeScale = true;
			MyDrone->Camera->PostProcessSettings.BloomSizeScale = Value;
		}
	}
}
void UVisualEffectsMenuWidget::OnResetClicked()
{
	float DefaultBloom = 1.0f;
	float DefaultStarIntensity = 20.0f;
	float DefaultEclipseFX = 1.0f;
	float DefaultExposure = 0.0f;
	if (BloomSlider) BloomSlider->SetValue(DefaultBloom);
	if (StarIntensitySlider) StarIntensitySlider->SetValue(DefaultStarIntensity);
	if (EclipseFXSlider) EclipseFXSlider->SetValue(DefaultEclipseFX);
	if (ExposureSlider) ExposureSlider->SetValue(DefaultExposure);
	if (BloomValueText) BloomValueText->SetText(FText::FromString(FString::SanitizeFloat(DefaultBloom)));
	if (StarIntensityValueText) StarIntensityValueText->SetText(FText::FromString(FString::SanitizeFloat(DefaultStarIntensity)));
	if (EclipseFXValueText) EclipseFXValueText->SetText(FText::FromString(FString::SanitizeFloat(DefaultEclipseFX)));
	if (ExposureValueText) ExposureValueText->SetText(FText::FromString(FString::SanitizeFloat(DefaultExposure)));
	OnBloomChanged(DefaultBloom);
	OnStarIntensityChanged(DefaultStarIntensity);
	OnEclipseFXChanged(DefaultEclipseFX);
	OnExposureChanged(DefaultExposure);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("VFX Resetat la Setarile Default"));
	}
}
void UVisualEffectsMenuWidget::OnExposureChanged(float Value)
{
	if (ExposureValueText) ExposureValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn());
		if (MyDrone && MyDrone->Camera)
		{
			MyDrone->Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
			MyDrone->Camera->PostProcessSettings.AutoExposureBias = Value;
		}
	}
}
void UMainMenuWidget::OpenExitMenu()
{
	if (MenuDropdownContainer) MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	UExitMenuWidget* ExitWidget = CreateWidget<UExitMenuWidget>(GetOwningPlayer(), UExitMenuWidget::StaticClass());
	if (ExitWidget)
	{
		ExitWidget->AddToViewport(9999);
	}
}
TSharedRef<SWidget> UExitMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.98f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(350.0f, 150.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	UTextBlock* QuestionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	QuestionText->SetText(AOrbitalManager::Tr(TEXT("Sigur doriti sa iesiti?"), TEXT("Are you sure you want to exit?")));
	QuestionText->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	QuestionText->SetFont(TitleFont);
	MainLayout->AddChildToVerticalBox(QuestionText)->SetPadding(FMargin(0.f, 30.f, 0.f, 20.f));
	UHorizontalBox* ButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ButtonBox)->SetPadding(FMargin(30.f, 10.f, 30.f, 20.f));
	FSlateFontInfo BtnFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	Btn_No = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_No->SetBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f));
	UTextBlock* TxtNo = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtNo->SetText(AOrbitalManager::Tr(TEXT("NU"), TEXT("NO")));
	TxtNo->SetFont(BtnFont);
	TxtNo->SetMargin(FMargin(30.f, 10.f));
	TxtNo->SetJustification(ETextJustify::Center);
	Btn_No->AddChild(TxtNo);
	Btn_No->OnClicked.AddDynamic(this, &UExitMenuWidget::OnNoClicked);
	UHorizontalBoxSlot* NoSlot = ButtonBox->AddChildToHorizontalBox(Btn_No);
	NoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Spacer->SetSize(FVector2D(20.0f, 1.0f));
	ButtonBox->AddChildToHorizontalBox(Spacer);
	Btn_Yes = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Btn_Yes->SetBackgroundColor(FLinearColor(0.8f, 0.1f, 0.1f));
	UTextBlock* TxtYes = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtYes->SetText(AOrbitalManager::Tr(TEXT("DA"), TEXT("YES")));
	TxtYes->SetFont(BtnFont);
	TxtYes->SetMargin(FMargin(30.f, 10.f));
	TxtYes->SetJustification(ETextJustify::Center);
	Btn_Yes->AddChild(TxtYes);
	Btn_Yes->OnClicked.AddDynamic(this, &UExitMenuWidget::OnYesClicked);
	UHorizontalBoxSlot* YesSlot = ButtonBox->AddChildToHorizontalBox(Btn_Yes);
	YesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return Super::RebuildWidget();
}
void UExitMenuWidget::OnNoClicked()
{
	this->RemoveFromParent();
}
void UExitMenuWidget::OnYesClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
void UMainMenuWidget::OpenSettingsMenu()
{
	if (MenuDropdownContainer) MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	USettingsMenuWidget* SettingsWidget = CreateWidget<USettingsMenuWidget>(GetOwningPlayer(), USettingsMenuWidget::StaticClass());
	if (SettingsWidget)
	{
		SettingsWidget->AddToViewport(999);
	}
}
TSharedRef<SWidget> USettingsMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.98f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(650.0f, 900.0f));
	UScrollBox* MainScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	BgBorder->SetContent(MainScroll);
	BgBorder->SetPadding(FMargin(0.f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainScroll->AddChild(MainLayout);
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(15.f, 12.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("SETARI SI PERFORMANTA"), TEXT("SETTINGS & PERFORMANCE")));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	Title->SetFont(TitleFont);
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(TitleFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	auto AddRowHelper = [&](UWidget* LeftWidget, UWidget* RightWidget) {
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		MainLayout->AddChildToVerticalBox(Row)->SetPadding(FMargin(25.f, 15.f));
		UHorizontalBoxSlot* LSlot = Row->AddChildToHorizontalBox(LeftWidget);
		LSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LSlot->SetVerticalAlignment(VAlign_Center);
		LSlot->SetHorizontalAlignment(HAlign_Left);
		UHorizontalBoxSlot* RSlot = Row->AddChildToHorizontalBox(RightWidget);
		RSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		RSlot->SetVerticalAlignment(VAlign_Center);
		RSlot->SetHorizontalAlignment(HAlign_Right);
		UBorder* Divider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Divider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
		MainLayout->AddChildToVerticalBox(Divider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
		};
	auto StyleComboBoxHelper = [](UComboBoxString* Combo) {
		Combo->ForegroundColor = FSlateColor(FLinearColor::White);
		FComboBoxStyle CustomComboStyle = Combo->WidgetStyle;
		FSlateColor DarkBg(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));
		FSlateColor HoverBg(FLinearColor(0.25f, 0.25f, 0.25f, 1.0f));
		CustomComboStyle.ComboButtonStyle.ButtonStyle.Normal.TintColor = DarkBg;
		CustomComboStyle.ComboButtonStyle.ButtonStyle.Hovered.TintColor = HoverBg;
		CustomComboStyle.ComboButtonStyle.ButtonStyle.Pressed.TintColor = DarkBg;
		Combo->WidgetStyle = CustomComboStyle;
		FTableRowStyle CustomItemStyle = Combo->ItemStyle;
		CustomItemStyle.TextColor = FSlateColor(FLinearColor::White);
		CustomItemStyle.SelectedTextColor = FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f, 1.0f));
		Combo->ItemStyle = CustomItemStyle;
		};
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	UTextBlock* TxtWindow = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtWindow->SetText(AOrbitalManager::Tr(TEXT("Mod Afisare (Window):"), TEXT("Display Mode:")));
	TxtWindow->SetFont(LabelFont);
	UTextBlock* TxtLanguage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtLanguage->SetText(FText::FromString("Limba / Language:"));
	TxtLanguage->SetFont(LabelFont);
	LanguageComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	LanguageComboBox->AddOption("Romana");
	LanguageComboBox->AddOption("English");
	StyleComboBoxHelper(LanguageComboBox);
	AddRowHelper(TxtLanguage, LanguageComboBox);
	UTextBlock* TxtMeasurement = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtMeasurement->SetText(AOrbitalManager::Tr(TEXT("Unitati de Masura:"), TEXT("Measurement Units:")));
	TxtMeasurement->SetFont(LabelFont);
	MeasurementComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	MeasurementComboBox->AddOption(AOrbitalManager::TrString(TEXT("Kilometri"), TEXT("Kilometers")));
	MeasurementComboBox->AddOption(AOrbitalManager::TrString(TEXT("Mile"), TEXT("Miles")));
	StyleComboBoxHelper(MeasurementComboBox);
	AddRowHelper(TxtMeasurement, MeasurementComboBox);
	WindowModeComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	WindowModeComboBox->AddOption("Fullscreen (Max FPS)");
	WindowModeComboBox->AddOption("Windowed Fullscreen");
	WindowModeComboBox->AddOption("Windowed");
	StyleComboBoxHelper(WindowModeComboBox);
	AddRowHelper(TxtWindow, WindowModeComboBox);
	UTextBlock* TxtQuality = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtQuality->SetText(AOrbitalManager::Tr(TEXT("Calitate Generala:"), TEXT("Overall Quality:")));
	TxtQuality->SetFont(LabelFont);
	QualityComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	QualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Scazuta (Performanta)"), TEXT("Low (Performance)")));
	QualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Medie"), TEXT("Medium")));
	QualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Ridicata"), TEXT("High")));
	QualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Epic (Calitate Maxima)"), TEXT("Epic (Max Quality)")));
	StyleComboBoxHelper(QualityComboBox);
	AddRowHelper(TxtQuality, QualityComboBox);
	UTextBlock* TxtClouds = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtClouds->SetText(AOrbitalManager::Tr(TEXT("Calitate Nori (Volumetric):"), TEXT("Cloud Quality (Volumetric):")));
	TxtClouds->SetFont(LabelFont);
	CloudQualityComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	CloudQualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Oprit (Fara nori)"), TEXT("Off (No clouds)")));
	CloudQualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Low (Echilibrat)"), TEXT("Low (Balanced)")));
	CloudQualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("High (Clar)"), TEXT("High (Clear)")));
	StyleComboBoxHelper(CloudQualityComboBox);
	AddRowHelper(TxtClouds, CloudQualityComboBox);
	UTextBlock* TxtCesium = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtCesium->SetText(AOrbitalManager::Tr(TEXT("Detaliu Teren 3D (Cesium):"), TEXT("3D Terrain Detail (Cesium):")));
	TxtCesium->SetFont(LabelFont);
	CesiumQualityComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	CesiumQualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Low (Incarcare rapida/Urat)"), TEXT("Low (Fast Load/Ugly)")));
	CesiumQualityComboBox->AddOption("Medium");
	CesiumQualityComboBox->AddOption("High");
	CesiumQualityComboBox->AddOption(AOrbitalManager::TrString(TEXT("Epic (Precizie max/Lent)"), TEXT("Epic (Max Precision/Slow)")));
	StyleComboBoxHelper(CesiumQualityComboBox);
	AddRowHelper(TxtCesium, CesiumQualityComboBox);
	UTextBlock* TxtCache = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtCache->SetText(AOrbitalManager::Tr(TEXT("Memorie Cache 3D (RAM):"), TEXT("3D Cache Memory (RAM):")));
	TxtCache->SetFont(LabelFont);
	CesiumCacheComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	CesiumCacheComboBox->AddOption(AOrbitalManager::TrString(TEXT("256 MB (PC Slab)"), TEXT("256 MB (Low-end PC)")));
	CesiumCacheComboBox->AddOption(AOrbitalManager::TrString(TEXT("512 MB (Standard)"), TEXT("512 MB (Standard)")));
	CesiumCacheComboBox->AddOption(AOrbitalManager::TrString(TEXT("1024 MB (PC Gaming)"), TEXT("1024 MB (Gaming PC)")));
	StyleComboBoxHelper(CesiumCacheComboBox);
	AddRowHelper(TxtCache, CesiumCacheComboBox);
	UVerticalBox* CesiumKeySection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(CesiumKeySection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtCesiumKey = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtCesiumKey->SetText(AOrbitalManager::Tr(TEXT("Cheie Acces Cesium Ion (Token):"), TEXT("Cesium Ion Access Token:")));
	TxtCesiumKey->SetFont(LabelFont);
	CesiumKeySection->AddChildToVerticalBox(TxtCesiumKey)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	UHorizontalBox* KeyInputHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CesiumKeySection->AddChildToVerticalBox(KeyInputHBox);
	CesiumKeyTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	CesiumKeyTextBox->SetHintText(AOrbitalManager::Tr(TEXT("Lasa gol pentru cheia implicita..."), TEXT("Leave blank for default key...")));
	CesiumKeyTextBox->SetIsPassword(true);
	FEditableTextBoxStyle CustomTextBoxStyle = CesiumKeyTextBox->WidgetStyle;
	FSlateColor DarkBgColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
	CustomTextBoxStyle.BackgroundImageNormal.TintColor = DarkBgColor;
	CustomTextBoxStyle.BackgroundImageHovered.TintColor = DarkBgColor;
	CustomTextBoxStyle.BackgroundImageFocused.TintColor = DarkBgColor;
	CustomTextBoxStyle.BackgroundImageReadOnly.TintColor = DarkBgColor;
	CustomTextBoxStyle.TextStyle.Font = LabelFont;
	CustomTextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	CesiumKeyTextBox->WidgetStyle = CustomTextBoxStyle;
	UHorizontalBoxSlot* KeyInputSlot = KeyInputHBox->AddChildToHorizontalBox(CesiumKeyTextBox);
	KeyInputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	KeyInputSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	ToggleKeyVisibilityButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ToggleKeyVisibilityButton->SetBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f));
	ToggleKeyVisibilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ToggleKeyVisibilityText->SetText(AOrbitalManager::Tr(TEXT("Arata"), TEXT("Show")));
	ToggleKeyVisibilityText->SetFont(LabelFont);
	ToggleKeyVisibilityButton->AddChild(ToggleKeyVisibilityText);
	ToggleKeyVisibilityButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnToggleCesiumKeyVisibilityClicked);
	UHorizontalBoxSlot* EyeSlot = KeyInputHBox->AddChildToHorizontalBox(ToggleKeyVisibilityButton);
	EyeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	EyeSlot->SetVerticalAlignment(VAlign_Fill);
	CesiumLinkButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CesiumLinkButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* LinkText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LinkText->SetText(AOrbitalManager::Tr(TEXT("Ia un Token gratuit de pe ion.cesium.com"), TEXT("Get a free token from ion.cesium.com")));
	LinkText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
	LinkText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f)));
	CesiumLinkButton->AddChild(LinkText);
	CesiumLinkButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnCesiumLinkClicked);
	UVerticalBoxSlot* LinkSlot = CesiumKeySection->AddChildToVerticalBox(CesiumLinkButton);
	LinkSlot->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
	LinkSlot->SetHorizontalAlignment(HAlign_Left);
	UBorder* KeyDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	KeyDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(KeyDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UTextBlock* TxtFPS = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtFPS->SetText(AOrbitalManager::Tr(TEXT("Limita Cadre (FPS):"), TEXT("Framerate Limit (FPS):")));
	TxtFPS->SetFont(LabelFont);
	FPSComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	FPSComboBox->AddOption("30 FPS");
	FPSComboBox->AddOption("40 FPS");
	FPSComboBox->AddOption("60 FPS");
	FPSComboBox->AddOption(AOrbitalManager::TrString(TEXT("Nelimitat"), TEXT("Unlimited")));
	StyleComboBoxHelper(FPSComboBox);
	AddRowHelper(TxtFPS, FPSComboBox);
	UTextBlock* TxtVSync = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtVSync->SetText(AOrbitalManager::Tr(TEXT("Sincronizare Verticala (VSync):"), TEXT("Vertical Sync (VSync):")));
	TxtVSync->SetFont(LabelFont);
	VSyncCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	AddRowHelper(TxtVSync, VSyncCheckBox);
	UTextBlock* TxtBlur = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtBlur->SetText(AOrbitalManager::Tr(TEXT("Motion Blur (Incetosare Miscare):"), TEXT("Motion Blur:")));
	TxtBlur->SetFont(LabelFont);
	MotionBlurCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	AddRowHelper(TxtBlur, MotionBlurCheckBox);
	UTextBlock* TxtDroneVis = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtDroneVis->SetText(AOrbitalManager::Tr(TEXT("Arata Model Drona (Zbor FPV):"), TEXT("Show Drone Model (FPV Flight):")));
	TxtDroneVis->SetFont(LabelFont);
	DroneVisCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	AddRowHelper(TxtDroneVis, DroneVisCheckBox);
	UTextBlock* TxtHideHUD = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtHideHUD->SetText(AOrbitalManager::Tr(TEXT("Ascunde Interfata / Texte Verzi:"), TEXT("Hide UI / Green Text:")));
	TxtHideHUD->SetFont(LabelFont);
	HideHUDCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	AddRowHelper(TxtHideHUD, HideHUDCheckBox);
	UVerticalBox* ResSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ResSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtRes = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtRes->SetText(AOrbitalManager::Tr(TEXT("Scala Rezolutie 3D (33-100%):"), TEXT("3D Resolution Scale (33-100%):")));
	TxtRes->SetFont(LabelFont);
	ResSection->AddChildToVerticalBox(TxtRes)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* ResBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ResSection->AddChildToVerticalBox(ResBox);
	ResScaleSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	ResScaleSlider->SetMinValue(33.0f);
	ResScaleSlider->SetMaxValue(100.0f);
	ResScaleSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnResScaleChanged);
	ResScaleValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResScaleValueText->SetFont(LabelFont);
	ResScaleValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	UHorizontalBoxSlot* SliderSlot = ResBox->AddChildToHorizontalBox(ResScaleSlider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	ResBox->AddChildToHorizontalBox(ResScaleValueText)->SetVerticalAlignment(VAlign_Center);
	UBorder* ResDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ResDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(ResDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* GammaSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(GammaSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtGamma = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtGamma->SetText(AOrbitalManager::Tr(TEXT("Luminozitate Ecran (Gamma):"), TEXT("Screen Brightness (Gamma):")));
	TxtGamma->SetFont(LabelFont);
	GammaSection->AddChildToVerticalBox(TxtGamma)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* GammaBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	GammaSection->AddChildToVerticalBox(GammaBox);
	GammaSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	GammaSlider->SetMinValue(1.0f);
	GammaSlider->SetMaxValue(3.0f);
	GammaSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnGammaChanged);
	GammaValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	GammaValueText->SetFont(LabelFont);
	GammaValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	UHorizontalBoxSlot* GSliderSlot = GammaBox->AddChildToHorizontalBox(GammaSlider);
	GSliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GSliderSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	GammaBox->AddChildToHorizontalBox(GammaValueText)->SetVerticalAlignment(VAlign_Center);
	UBorder* GammaDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GammaDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(GammaDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* AudioSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(AudioSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtAudio = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtAudio->SetText(AOrbitalManager::Tr(TEXT("Volum Audio Principal:"), TEXT("Master Audio Volume:")));
	TxtAudio->SetFont(LabelFont);
	AudioSection->AddChildToVerticalBox(TxtAudio)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* AudioBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AudioSection->AddChildToVerticalBox(AudioBox);
	AudioSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	AudioSlider->SetMinValue(0.0f);
	AudioSlider->SetMaxValue(100.0f);
	AudioSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnAudioChanged);
	AudioValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	AudioValueText->SetFont(LabelFont);
	AudioValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	UHorizontalBoxSlot* ASliderSlot = AudioBox->AddChildToHorizontalBox(AudioSlider);
	ASliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ASliderSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	AudioBox->AddChildToHorizontalBox(AudioValueText)->SetVerticalAlignment(VAlign_Center);
	UBorder* AudioDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AudioDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(AudioDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* SensSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(SensSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtSens = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtSens->SetText(AOrbitalManager::Tr(TEXT("Sensibilitate Mouse (0.1 - 3.0):"), TEXT("Mouse Sensitivity (0.1 - 3.0):")));
	TxtSens->SetFont(LabelFont);
	SensSection->AddChildToVerticalBox(TxtSens)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* SensBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SensSection->AddChildToVerticalBox(SensBox);
	SensitivitySlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	SensitivitySlider->SetMinValue(0.1f);
	SensitivitySlider->SetMaxValue(3.0f);
	SensitivitySlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnSensitivityChanged);
	SensitivityValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SensitivityValueText->SetFont(LabelFont);
	SensitivityValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	UHorizontalBoxSlot* SensSliderSlot = SensBox->AddChildToHorizontalBox(SensitivitySlider);
	SensSliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SensSliderSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	SensBox->AddChildToHorizontalBox(SensitivityValueText)->SetVerticalAlignment(VAlign_Center);
	UBorder* SensDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SensDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(SensDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* FOVSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(FOVSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtFOV = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtFOV->SetText(AOrbitalManager::Tr(TEXT("Camp Vizual / FOV (0.001 - 135):"), TEXT("Field of View / FOV (0.001 - 135):")));
	TxtFOV->SetFont(LabelFont);
	FOVSection->AddChildToVerticalBox(TxtFOV)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* FOVBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FOVSection->AddChildToVerticalBox(FOVBox);
	FOVSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	FOVSlider->SetMinValue(0.001f);
	FOVSlider->SetMaxValue(135.0f);
	FOVValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FOVValueText->SetFont(LabelFont);
	FOVValueText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
	UHorizontalBoxSlot* FOVSliderSlot = FOVBox->AddChildToHorizontalBox(FOVSlider);
	FOVSliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FOVSliderSlot->SetPadding(FMargin(0.f, 0.f, 15.f, 0.f));
	FOVBox->AddChildToHorizontalBox(FOVValueText)->SetVerticalAlignment(VAlign_Center);
	UBorder* FOVDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FOVDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(FOVDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* ColorSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ColorSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtColorTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtColorTitle->SetText(AOrbitalManager::Tr(TEXT("Personalizare Culoare Umbra (R, G, B, Opacitate):"), TEXT("Custom Shadow Color (R, G, B, Opacity):")));
	TxtColorTitle->SetFont(LabelFont);
	ColorSection->AddChildToVerticalBox(TxtColorTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	ColorPreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ColorPreviewBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
	UVerticalBoxSlot* PrevSlot = ColorSection->AddChildToVerticalBox(ColorPreviewBorder);
	PrevSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	PrevSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	USizeBox* PreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PreviewSizeBox->SetHeightOverride(40.0f);
	ColorPreviewBorder->SetContent(PreviewSizeBox);
	auto AddColorSlider = [&](FString LabelRO, FString LabelEN, USlider*& OutSlider, FLinearColor HandleColor)
		{
			UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			ColorSection->AddChildToVerticalBox(HBox)->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
			UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
			Lbl->SetFont(LabelFont);
			UHorizontalBoxSlot* LblSlot = HBox->AddChildToHorizontalBox(Lbl);
			LblSlot->SetVerticalAlignment(VAlign_Center);
			USizeBox* TxtSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			TxtSizeBox->SetWidthOverride(120.0f);
			TxtSizeBox->SetContent(Lbl);
			LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			OutSlider->SetMinValue(0.0f);
			OutSlider->SetMaxValue(1.0f);
			OutSlider->SetSliderHandleColor(HandleColor);
			OutSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnColorSliderChanged);
			UHorizontalBoxSlot* SliderSlot = HBox->AddChildToHorizontalBox(OutSlider);
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		};
	AddColorSlider(TEXT("Rosu (R):"), TEXT("Red (R):"), SliderR, FLinearColor::Red);
	AddColorSlider(TEXT("Verde (G):"), TEXT("Green (G):"), SliderG, FLinearColor::Green);
	AddColorSlider(TEXT("Albastru (B):"), TEXT("Blue (B):"), SliderB, FLinearColor::Blue);
	AddColorSlider(TEXT("Opacitate (A):"), TEXT("Opacity (A):"), SliderA, FLinearColor::White);
	float SavedR = 0.0f, SavedG = 0.0f, SavedB = 0.0f, SavedA = 0.8f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareR"), SavedR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareG"), SavedG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("CuloareB"), SavedB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbra"), TEXT("Opacitate"), SavedA, GGameUserSettingsIni);
		if (PenumbraSliderR && PenumbraSliderG && PenumbraSliderB && PenumbraSliderA)
		{
			GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareR"), PenumbraSliderR->GetValue(), GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareG"), PenumbraSliderG->GetValue(), GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareB"), PenumbraSliderB->GetValue(), GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("Opacitate"), PenumbraSliderA->GetValue(), GGameUserSettingsIni);
		}
	}
	SliderR->SetValue(SavedR);
	SliderG->SetValue(SavedG);
	SliderB->SetValue(SavedB);
	SliderA->SetValue(SavedA);
	UBorder* ColorDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ColorDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(ColorDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	auto AddColorSlider2 = [&](UVerticalBox* TargetSection, FString LabelRO, FString LabelEN, USlider*& OutSlider, FLinearColor HandleColor, bool bIsPenumbra)
		{
			UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			TargetSection->AddChildToVerticalBox(HBox)->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
			UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
			Lbl->SetFont(LabelFont);
			UHorizontalBoxSlot* LblSlot = HBox->AddChildToHorizontalBox(Lbl);
			LblSlot->SetVerticalAlignment(VAlign_Center);
			USizeBox* TxtSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			TxtSizeBox->SetWidthOverride(120.0f);
			TxtSizeBox->SetContent(Lbl);
			LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			OutSlider->SetMinValue(0.0f);
			OutSlider->SetMaxValue(1.0f);
			OutSlider->SetSliderHandleColor(HandleColor);
			if (bIsPenumbra)
			{
				OutSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnPenumbraColorSliderChanged);
			}
			else
			{
				OutSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnColorSliderChanged);
			}
			UHorizontalBoxSlot* SliderSlot = HBox->AddChildToHorizontalBox(OutSlider);
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		};
	UVerticalBox* PenumbraColorSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(PenumbraColorSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtPenumbraTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtPenumbraTitle->SetText(AOrbitalManager::Tr(TEXT("Personalizare Culoare Penumbra (Partialitate):"), TEXT("Custom Penumbra Color (Partiality):")));
	TxtPenumbraTitle->SetFont(LabelFont);
	PenumbraColorSection->AddChildToVerticalBox(TxtPenumbraTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	PenumbraColorPreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PenumbraColorPreviewBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
	UVerticalBoxSlot* PenPrevSlot = PenumbraColorSection->AddChildToVerticalBox(PenumbraColorPreviewBorder);
	PenPrevSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	PenPrevSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	USizeBox* PenPreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PenPreviewSizeBox->SetHeightOverride(40.0f);
	PenumbraColorPreviewBorder->SetContent(PenPreviewSizeBox);
	AddColorSlider2(PenumbraColorSection, TEXT("Rosu (R):"), TEXT("Red (R):"), PenumbraSliderR, FLinearColor::Red, true);
	AddColorSlider2(PenumbraColorSection, TEXT("Verde (G):"), TEXT("Green (G):"), PenumbraSliderG, FLinearColor::Green, true);
	AddColorSlider2(PenumbraColorSection, TEXT("Albastru (B):"), TEXT("Blue (B):"), PenumbraSliderB, FLinearColor::Blue, true);
	AddColorSlider2(PenumbraColorSection, TEXT("Opacitate (A):"), TEXT("Opacity (A):"), PenumbraSliderA, FLinearColor::White, true);
	float PenR = 0.0f, PenG = 0.0f, PenB = 0.0f, PenA = 0.3f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareR"), PenR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareG"), PenG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("CuloareB"), PenB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbra"), TEXT("Opacitate"), PenA, GGameUserSettingsIni);
	}
	PenumbraSliderR->SetValue(PenR); PenumbraSliderG->SetValue(PenG); PenumbraSliderB->SetValue(PenB); PenumbraSliderA->SetValue(PenA);
	UBorder* PenColorDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PenColorDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(PenColorDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	auto AddEarthColorSlider = [&](UVerticalBox* TargetSection, FString LabelRO, FString LabelEN, USlider*& OutSlider, FLinearColor HandleColor, bool bIsPenumbra)
		{
			UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			TargetSection->AddChildToVerticalBox(HBox)->SetPadding(FMargin(0.f, 5.f, 0.f, 5.f));
			UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
			Lbl->SetFont(LabelFont);
			UHorizontalBoxSlot* LblSlot = HBox->AddChildToHorizontalBox(Lbl);
			LblSlot->SetVerticalAlignment(VAlign_Center);
			LblSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			LblSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			OutSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
			OutSlider->SetMinValue(0.0f);
			OutSlider->SetMaxValue(1.0f);
			OutSlider->SetSliderHandleColor(HandleColor);
			if (bIsPenumbra)
				OutSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnEarthPenumbraColorSliderChanged);
			else
				OutSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnEarthUmbraColorSliderChanged);
			UHorizontalBoxSlot* SliderSlot = HBox->AddChildToHorizontalBox(OutSlider);
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		};
	UVerticalBox* EarthUmbraSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(EarthUmbraSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtEarthUmbra = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtEarthUmbra->SetText(AOrbitalManager::Tr(TEXT("Culoare Umbra Pamant (Eclipsa de Luna):"), TEXT("Earth Umbra Color (Lunar Eclipse):")));
	TxtEarthUmbra->SetFont(LabelFont);
	EarthUmbraSection->AddChildToVerticalBox(TxtEarthUmbra)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	EarthUmbraPreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UVerticalBoxSlot* EUPrevSlot = EarthUmbraSection->AddChildToVerticalBox(EarthUmbraPreviewBorder);
	EUPrevSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	EUPrevSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	USizeBox* EUPreviewSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EUPreviewSize->SetHeightOverride(40.0f);
	EarthUmbraPreviewBorder->SetContent(EUPreviewSize);
	AddEarthColorSlider(EarthUmbraSection, TEXT("Rosu (R):"), TEXT("Red (R):"), EarthUmbraSliderR, FLinearColor::Red, false);
	AddEarthColorSlider(EarthUmbraSection, TEXT("Verde (G):"), TEXT("Green (G):"), EarthUmbraSliderG, FLinearColor::Green, false);
	AddEarthColorSlider(EarthUmbraSection, TEXT("Albastru (B):"), TEXT("Blue (B):"), EarthUmbraSliderB, FLinearColor::Blue, false);
	AddEarthColorSlider(EarthUmbraSection, TEXT("Opacitate (A):"), TEXT("Opacity (A):"), EarthUmbraSliderA, FLinearColor::White, false);
	float EUR = 0.02f, EUG = 0.02f, EUB = 0.08f, EUA = 0.6f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareR"), EUR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareG"), EUG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareB"), EUB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("Opacitate"), EUA, GGameUserSettingsIni);
	}
	EarthUmbraSliderR->SetValue(EUR); EarthUmbraSliderG->SetValue(EUG);
	EarthUmbraSliderB->SetValue(EUB); EarthUmbraSliderA->SetValue(EUA);
	EarthUmbraPreviewBorder->SetBrushColor(FLinearColor(EUR, EUG, EUB, EUA));
	UBorder* EarthUmbraDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EarthUmbraDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(EarthUmbraDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UVerticalBox* EarthPenSection = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(EarthPenSection)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtEarthPen = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtEarthPen->SetText(AOrbitalManager::Tr(TEXT("Culoare Penumbra Pamant (Eclipsa de Luna):"), TEXT("Earth Penumbra Color (Lunar Eclipse):")));
	TxtEarthPen->SetFont(LabelFont);
	EarthPenSection->AddChildToVerticalBox(TxtEarthPen)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	EarthPenumbraPreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UVerticalBoxSlot* EPPrevSlot = EarthPenSection->AddChildToVerticalBox(EarthPenumbraPreviewBorder);
	EPPrevSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	EPPrevSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	USizeBox* EPPreviewSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	EPPreviewSize->SetHeightOverride(40.0f);
	EarthPenumbraPreviewBorder->SetContent(EPPreviewSize);
	AddEarthColorSlider(EarthPenSection, TEXT("Rosu (R):"), TEXT("Red (R):"), EarthPenumbraSliderR, FLinearColor::Red, true);
	AddEarthColorSlider(EarthPenSection, TEXT("Verde (G):"), TEXT("Green (G):"), EarthPenumbraSliderG, FLinearColor::Green, true);
	AddEarthColorSlider(EarthPenSection, TEXT("Albastru (B):"), TEXT("Blue (B):"), EarthPenumbraSliderB, FLinearColor::Blue, true);
	AddEarthColorSlider(EarthPenSection, TEXT("Opacitate (A):"), TEXT("Opacity (A):"), EarthPenumbraSliderA, FLinearColor::White, true);
	float EPR = 0.15f, EPG = 0.1f, EPB = 0.02f, EPA = 0.25f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareR"), EPR, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareG"), EPG, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareB"), EPB, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("Opacitate"), EPA, GGameUserSettingsIni);
	}
	EarthPenumbraSliderR->SetValue(EPR); EarthPenumbraSliderG->SetValue(EPG);
	EarthPenumbraSliderB->SetValue(EPB); EarthPenumbraSliderA->SetValue(EPA);
	EarthPenumbraPreviewBorder->SetBrushColor(FLinearColor(EPR, EPG, EPB, EPA));
	UBorder* EarthPenDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	EarthPenDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(EarthPenDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	UHorizontalBox* MapStartBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(MapStartBox)->SetPadding(FMargin(25.f, 15.f));
	UTextBlock* TxtMapTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtMapTitle->SetText(AOrbitalManager::Tr(TEXT("Locatie Start Zbor:"), TEXT("Flight Start Location:")));
	TxtMapTitle->SetFont(LabelFont);
	MapStartBox->AddChildToHorizontalBox(TxtMapTitle)->SetVerticalAlignment(VAlign_Center);
	USpacer* MapSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	MapStartBox->AddChildToHorizontalBox(MapSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	OpenMapButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenMapButton->SetBackgroundColor(FLinearColor(0.1f, 0.4f, 0.8f));
	UTextBlock* OpenMapTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OpenMapTxt->SetText(AOrbitalManager::Tr(TEXT(" DESCHIDE HARTA INTERACTIVA "), TEXT(" OPEN INTERACTIVE MAP ")));
	OpenMapTxt->SetFont(LabelFont);
	OpenMapTxt->SetMargin(FMargin(15.f, 8.f));
	OpenMapButton->AddChild(OpenMapTxt);
	OpenMapButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnOpenMapClicked);
	MapStartBox->AddChildToHorizontalBox(OpenMapButton);
	UBorder* MapDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MapDivider->SetBrushColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.3f));
	MainLayout->AddChildToVerticalBox(MapDivider)->SetPadding(FMargin(25.f, 0.f, 25.f, 0.f));
	OpenCreditsButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OpenCreditsButton->SetBackgroundColor(FLinearColor(0.2f, 0.3f, 0.5f));
	UTextBlock* CredTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CredTxt->SetText(AOrbitalManager::Tr(TEXT(" Vezi Credite si Licente (Credits) "), TEXT(" View Credits & Licenses ")));
	CredTxt->SetFont(TitleFont);
	CredTxt->SetMargin(FMargin(15.f, 8.f));
	OpenCreditsButton->AddChild(CredTxt);
	OpenCreditsButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnOpenCreditsClicked);
	UVerticalBoxSlot* CreditsSlot = MainLayout->AddChildToVerticalBox(OpenCreditsButton);
	CreditsSlot->SetPadding(FMargin(25.f, 20.f, 25.f, 0.f));
	CreditsSlot->SetHorizontalAlignment(HAlign_Center);
	USpacer* EndSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	MainLayout->AddChildToVerticalBox(EndSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* ApplyBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MainLayout->AddChildToVerticalBox(ApplyBox)->SetPadding(FMargin(25.f, 20.f));
	ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ResetButton->SetBackgroundColor(FLinearColor(0.5f, 0.1f, 0.1f));
	UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(AOrbitalManager::Tr(TEXT(" RESETEAZA LA DEFAULT "), TEXT(" RESET TO DEFAULT ")));
	ResetTxt->SetFont(TitleFont);
	ResetTxt->SetMargin(FMargin(25.f, 10.f));
	ResetButton->AddChild(ResetTxt);
	ResetButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnResetClicked);
	ApplyBox->AddChildToHorizontalBox(ResetButton);
	USpacer* PushRight = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	ApplyBox->AddChildToHorizontalBox(PushRight)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ApplyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ApplyButton->SetBackgroundColor(FLinearColor(0.1f, 0.5f, 0.1f));
	UTextBlock* ApplyTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ApplyTxt->SetText(AOrbitalManager::Tr(TEXT(" APLICA SETARILE "), TEXT(" APPLY SETTINGS ")));
	ApplyTxt->SetFont(TitleFont);
	ApplyTxt->SetMargin(FMargin(25.f, 10.f));
	ApplyButton->AddChild(ApplyTxt);
	ApplyButton->OnClicked.AddDynamic(this, &USettingsMenuWidget::OnApplyClicked);
	ApplyBox->AddChildToHorizontalBox(ApplyButton);
	LoadCurrentSettings();
	if (FOVSlider)
	{
		FOVSlider->OnValueChanged.AddDynamic(this, &USettingsMenuWidget::OnFOVChanged);
	}
	return Super::RebuildWidget();
}
void USettingsMenuWidget::OnEarthUmbraColorSliderChanged(float Value)
{
	if (EarthUmbraSliderR && EarthUmbraSliderG && EarthUmbraSliderB && EarthUmbraSliderA && EarthUmbraPreviewBorder)
	{
		EarthUmbraPreviewBorder->SetBrushColor(FLinearColor(
			EarthUmbraSliderR->GetValue(), EarthUmbraSliderG->GetValue(),
			EarthUmbraSliderB->GetValue(), EarthUmbraSliderA->GetValue()));
	}
}
void USettingsMenuWidget::OnEarthPenumbraColorSliderChanged(float Value)
{
	if (EarthPenumbraSliderR && EarthPenumbraSliderG && EarthPenumbraSliderB && EarthPenumbraSliderA && EarthPenumbraPreviewBorder)
	{
		EarthPenumbraPreviewBorder->SetBrushColor(FLinearColor(
			EarthPenumbraSliderR->GetValue(), EarthPenumbraSliderG->GetValue(),
			EarthPenumbraSliderB->GetValue(), EarthPenumbraSliderA->GetValue()));
	}
}
void USettingsMenuWidget::OnOpenCreditsClicked()
{
	if (GetWorld())
	{
		UCreditsMenuWidget* CreditsMenu = CreateWidget<UCreditsMenuWidget>(GetWorld(), UCreditsMenuWidget::StaticClass());
		if (CreditsMenu)
		{
			CreditsMenu->AddToViewport(10000);
			this->RemoveFromParent();
		}
	}
}
void USettingsMenuWidget::OnPenumbraColorSliderChanged(float Value)
{
	if (PenumbraSliderR && PenumbraSliderG && PenumbraSliderB && PenumbraSliderA && PenumbraColorPreviewBorder)
	{
		FLinearColor NewColor(
			PenumbraSliderR->GetValue(),
			PenumbraSliderG->GetValue(),
			PenumbraSliderB->GetValue(),
			PenumbraSliderA->GetValue()
		);
		PenumbraColorPreviewBorder->SetBrushColor(NewColor);
	}
}
void USettingsMenuWidget::OnColorSliderChanged(float Value)
{
	if (SliderR && SliderG && SliderB && SliderA && ColorPreviewBorder)
	{
		FLinearColor NewColor(
			SliderR->GetValue(),
			SliderG->GetValue(),
			SliderB->GetValue(),
			SliderA->GetValue()
		);
		ColorPreviewBorder->SetBrushColor(NewColor);
	}
}
void USettingsMenuWidget::OnOpenMapClicked()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		this->RemoveFromParent();
		UUserWidget* MapPicker = CreateWidget<UUserWidget>(PC, UMapPickerWidget::StaticClass());
		if (MapPicker)
		{
			MapPicker->AddToViewport(100);
		}
	}
}
void USettingsMenuWidget::LoadCurrentSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (Settings)
	{
		EWindowMode::Type WMode = Settings->GetFullscreenMode();
		if (WindowModeComboBox)
		{
			if (WMode == EWindowMode::Fullscreen) WindowModeComboBox->SetSelectedIndex(0);
			else if (WMode == EWindowMode::WindowedFullscreen) WindowModeComboBox->SetSelectedIndex(1);
			else WindowModeComboBox->SetSelectedIndex(2);
		}
		if (MeasurementComboBox)
		{
			FString SavedUnit = TEXT("KM");
			GConfig->GetString(TEXT("SetariCustom"), TEXT("UnitatiMasura"), SavedUnit, GGameUserSettingsIni);
			if (SavedUnit == TEXT("MI") || SavedUnit.Contains(TEXT("Mil")))
			{
				MeasurementComboBox->SetSelectedOption(AOrbitalManager::TrString(TEXT("Mile"), TEXT("Miles")));
			}
			else
			{
				MeasurementComboBox->SetSelectedOption(AOrbitalManager::TrString(TEXT("Kilometri"), TEXT("Kilometers")));
			}
		}
		if (LanguageComboBox)
		{
			FString SavedLanguage = TEXT("Romana");
			GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), SavedLanguage, GGameUserSettingsIni);
			LanguageComboBox->SetSelectedOption(SavedLanguage);
		}
		int32 Quality = Settings->GetOverallScalabilityLevel();
		if (Quality < 0) Quality = Settings->GetPostProcessingQuality();
		if (QualityComboBox) QualityComboBox->SetSelectedIndex(Quality);
		if (VSyncCheckBox) VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
		float FPS = Settings->GetFrameRateLimit();
		if (FPSComboBox)
		{
			if (FMath::IsNearlyEqual(FPS, 30.0f)) FPSComboBox->SetSelectedIndex(0);
			else if (FMath::IsNearlyEqual(FPS, 40.0f)) FPSComboBox->SetSelectedIndex(1);
			else if (FMath::IsNearlyEqual(FPS, 60.0f)) FPSComboBox->SetSelectedIndex(2);
			else FPSComboBox->SetSelectedIndex(3);
		}
		float CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue;
		Settings->GetResolutionScaleInformationEx(CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
		if (ResScaleSlider)
		{
			ResScaleSlider->SetValue(CurrentScaleValue);
			if (ResScaleValueText) ResScaleValueText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(CurrentScaleValue))));
		}
	}
	if (GammaSlider && GEngine)
	{
		float SavedGamma = GEngine->DisplayGamma;
		GammaSlider->SetValue(SavedGamma);
		if (GammaValueText) GammaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), SavedGamma)));
	}
	if (MotionBlurCheckBox)
	{
		static IConsoleVariable* CVarBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"));
		if (CVarBlur)
		{
			MotionBlurCheckBox->SetIsChecked(CVarBlur->GetInt() > 0);
		}
		else
		{
			MotionBlurCheckBox->SetIsChecked(true);
		}
	}
	if (AudioSlider)
	{
		AudioSlider->SetValue(GCurrentMasterVolume);
		if (AudioValueText)
		{
			AudioValueText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(GCurrentMasterVolume))));
		}
	}
	if (SensitivitySlider)
	{
		float SavedSens = 1.0f;
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("Sensibilitate"), SavedSens, GGameUserSettingsIni);
		SensitivitySlider->SetValue(SavedSens);
		if (SensitivityValueText) SensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), SavedSens)));
	}
	if (FOVSlider)
	{
		float CurrentLiveFOV = 90.0f;
		APlayerController* PC = GetOwningPlayer();
		if (PC && PC->PlayerCameraManager)
		{
			CurrentLiveFOV = PC->PlayerCameraManager->GetFOVAngle();
		}
		else
		{
			GConfig->GetFloat(TEXT("SetariCustom"), TEXT("FOV"), CurrentLiveFOV, GGameUserSettingsIni);
		}
		FOVSlider->SetValue(CurrentLiveFOV);
		if (FOVValueText) FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), CurrentLiveFOV)));
	}
	if (CesiumQualityComboBox || CesiumCacheComboBox)
	{
		TArray<AActor*> CesiumTilesets;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACesium3DTileset::StaticClass(), CesiumTilesets);
		if (CesiumTilesets.Num() > 0)
		{
			ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(CesiumTilesets[0]);
			if (Tileset)
			{
				if (CesiumQualityComboBox)
				{
					float CurrentSSE = Tileset->MaximumScreenSpaceError;
					if (CurrentSSE >= 128.0f) CesiumQualityComboBox->SetSelectedIndex(0);
					else if (CurrentSSE >= 64.0f) CesiumQualityComboBox->SetSelectedIndex(1);
					else if (CurrentSSE >= 32.0f) CesiumQualityComboBox->SetSelectedIndex(2);
					else CesiumQualityComboBox->SetSelectedIndex(3);
				}
				if (CesiumCacheComboBox)
				{
					float CurrentCacheMB = Tileset->MaximumCachedBytes / (1024.0f * 1024.0f);
					if (CurrentCacheMB <= 256.0f) CesiumCacheComboBox->SetSelectedIndex(0);
					else if (CurrentCacheMB <= 512.0f) CesiumCacheComboBox->SetSelectedIndex(1);
					else CesiumCacheComboBox->SetSelectedIndex(2);
				}
			}
		}
	}
	if (CloudQualityComboBox)
	{
		TArray<AActor*> CloudActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), CloudActors);
		bool bCloudsAreHidden = false;
		bool bIsLowQuality = false;
		for (AActor* Actor : CloudActors)
		{
			if (Actor && Actor->GetName().Contains("VolumetricCloud"))
			{
				bCloudsAreHidden = Actor->IsHidden();
				if (Actor->ActorHasTag(FName("CloudQuality_Low")))
				{
					bIsLowQuality = true;
				}
				break;
			}
		}
		if (bCloudsAreHidden)
		{
			CloudQualityComboBox->SetSelectedIndex(0);
		}
		else if (bIsLowQuality)
		{
			CloudQualityComboBox->SetSelectedIndex(1);
		}
		else
		{
			CloudQualityComboBox->SetSelectedIndex(2);
		}
	}
	if (DroneVisCheckBox)
	{
		bool bSavedDroneVis = true;
		if (!GConfig->GetBool(TEXT("SetariCustom"), TEXT("VizibilitateDrona"), bSavedDroneVis, GGameUserSettingsIni))
		{
			bSavedDroneVis = true;
		}
		DroneVisCheckBox->SetIsChecked(bSavedDroneVis);
	}
	if (HideHUDCheckBox)
	{
		bool bSavedHideHUD = false;
		if (!GConfig->GetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bSavedHideHUD, GGameUserSettingsIni))
		{
			bSavedHideHUD = false;
		}
		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
			if (MyHUD)
			{
				MyHUD->bDrawTelemetry = !bSavedHideHUD;
			}
		}
		HideHUDCheckBox->SetIsChecked(bSavedHideHUD);
	}
	if (CesiumKeyTextBox)
	{
		FString SavedCesiumKey = TEXT("");
		if (GConfig->GetString(TEXT("SetariCustom"), TEXT("CesiumKey"), SavedCesiumKey, GGameUserSettingsIni))
		{
			CesiumKeyTextBox->SetText(FText::FromString(SavedCesiumKey));
		}
	}
	if (CesiumQualityComboBox)
	{
		int32 SavedCesiumQual = 1;
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("CalitateCesium"), SavedCesiumQual, GGameUserSettingsIni);
		CesiumQualityComboBox->SetSelectedIndex(SavedCesiumQual);
	}
	if (CesiumCacheComboBox)
	{
		int32 SavedCesiumCache = 1;
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("CacheCesium"), SavedCesiumCache, GGameUserSettingsIni);
		CesiumCacheComboBox->SetSelectedIndex(SavedCesiumCache);
	}
	if (EarthUmbraSliderR && EarthUmbraSliderG && EarthUmbraSliderB && EarthUmbraSliderA && EarthUmbraPreviewBorder)
	{
		float R = 0.02f, G = 0.02f, B = 0.08f, A = 0.6f;
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareR"), R, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareG"), G, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareB"), B, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariUmbraPamant"), TEXT("Opacitate"), A, GGameUserSettingsIni);
		EarthUmbraSliderR->SetValue(R);
		EarthUmbraSliderG->SetValue(G);
		EarthUmbraSliderB->SetValue(B);
		EarthUmbraSliderA->SetValue(A);
		EarthUmbraPreviewBorder->SetBrushColor(FLinearColor(R, G, B, A));
	}
	if (EarthPenumbraSliderR && EarthPenumbraSliderG && EarthPenumbraSliderB && EarthPenumbraSliderA && EarthPenumbraPreviewBorder)
	{
		float R = 0.15f, G = 0.1f, B = 0.02f, A = 0.25f;
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareR"), R, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareG"), G, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareB"), B, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariPenumbraPamant"), TEXT("Opacitate"), A, GGameUserSettingsIni);
		EarthPenumbraSliderR->SetValue(R);
		EarthPenumbraSliderG->SetValue(G);
		EarthPenumbraSliderB->SetValue(B);
		EarthPenumbraSliderA->SetValue(A);
		EarthPenumbraPreviewBorder->SetBrushColor(FLinearColor(R, G, B, A));
	}
}
void USettingsMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
}
void USettingsMenuWidget::OnResScaleChanged(float Value)
{
	if (ResScaleValueText) ResScaleValueText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Value))));
}
void USettingsMenuWidget::OnGammaChanged(float Value)
{
	if (GammaValueText) GammaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Value)));
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FString Command = FString::Printf(TEXT("gamma %f"), Value);
		PC->ConsoleCommand(Command);
	}
}
void USettingsMenuWidget::OnApplyClicked()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	APlayerController* PC = GetOwningPlayer();
	if (!Settings) return;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager && Manager->DynUmbraMaterial && SliderR && SliderG && SliderB && SliderA)
	{
		FLinearColor FinalColor(SliderR->GetValue(), SliderG->GetValue(), SliderB->GetValue(), 1.0f);
		float FinalOpacity = SliderA->GetValue();
		Manager->DynUmbraMaterial->SetVectorParameterValue(FName("Culoare"), FinalColor);
		Manager->DynUmbraMaterial->SetScalarParameterValue(FName("Opacitate"), FinalOpacity);
		Manager->DynAntumbraMaterial->SetVectorParameterValue(FName("Culoare"), FinalColor);
		Manager->DynAntumbraMaterial->SetScalarParameterValue(FName("Opacitate"), FinalOpacity);
		if (Manager->DynPenumbraMaterial && PenumbraSliderR && PenumbraSliderG && PenumbraSliderB && PenumbraSliderA)
		{
			FLinearColor PenColor(PenumbraSliderR->GetValue(), PenumbraSliderG->GetValue(), PenumbraSliderB->GetValue(), 1.0f);
			float PenOpacity = PenumbraSliderA->GetValue();
			Manager->DynPenumbraMaterial->SetVectorParameterValue(FName("Culoare"), PenColor);
			Manager->DynPenumbraMaterial->SetScalarParameterValue(FName("Opacitate"), PenOpacity);
		}
	}
	if (MeasurementComboBox)
	{
		FString SelectedOption = MeasurementComboBox->GetSelectedOption();
		FString MileOptionName = AOrbitalManager::TrString(TEXT("Mile"), TEXT("Miles"));
		FString UnitToSave = (SelectedOption == MileOptionName) ? TEXT("MI") : TEXT("KM");
		GConfig->SetString(TEXT("SetariCustom"), TEXT("UnitatiMasura"), *UnitToSave, GGameUserSettingsIni);
	}
	if (WindowModeComboBox)
	{
		int32 WIndex = WindowModeComboBox->GetSelectedIndex();
		if (WIndex == 0) Settings->SetFullscreenMode(EWindowMode::Fullscreen);
		else if (WIndex == 1) Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
		else Settings->SetFullscreenMode(EWindowMode::Windowed);
	}
	if (QualityComboBox)
	{
		int32 SelectedQuality = QualityComboBox->GetSelectedIndex();
		Settings->SetOverallScalabilityLevel(SelectedQuality);
		if (PC)
		{
			PC->ConsoleCommand(FString::Printf(TEXT("sg.PostProcessQuality %d"), SelectedQuality));
			PC->ConsoleCommand(FString::Printf(TEXT("sg.ShadowQuality %d"), SelectedQuality));
			PC->ConsoleCommand(FString::Printf(TEXT("sg.AntiAliasingQuality %d"), SelectedQuality));
			PC->ConsoleCommand(FString::Printf(TEXT("sg.EffectsQuality %d"), SelectedQuality));
			PC->ConsoleCommand(FString::Printf(TEXT("sg.TextureQuality %d"), SelectedQuality));
			PC->ConsoleCommand(FString::Printf(TEXT("sg.ViewDistanceQuality %d"), SelectedQuality));
			if (SelectedQuality <= 1)
			{
				PC->ConsoleCommand(TEXT("sg.GlobalIlluminationQuality 0"));
				PC->ConsoleCommand(TEXT("sg.ReflectionQuality 0"));
			}
			else
			{
				PC->ConsoleCommand(FString::Printf(TEXT("sg.GlobalIlluminationQuality %d"), SelectedQuality));
				PC->ConsoleCommand(FString::Printf(TEXT("sg.ReflectionQuality %d"), SelectedQuality));
			}
			if (SelectedQuality == 0)
			{
				PC->ConsoleCommand(TEXT("r.RayTracing 0"));
				PC->ConsoleCommand(TEXT("r.SkyLight.RealTimeReflectionCapture 0"));
				PC->ConsoleCommand(TEXT("r.Lumen.Reflections.Allow 0"));
				PC->ConsoleCommand(TEXT("r.DynamicGlobalIlluminationMethod 0"));
				PC->ConsoleCommand(TEXT("r.ReflectionMethod 0"));
				PC->ConsoleCommand(TEXT("r.SSR.Quality 0"));
				PC->ConsoleCommand(TEXT("r.AmbientOcclusionLevels 0"));
				PC->ConsoleCommand(TEXT("r.VolumetricFog 0"));
				PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
				PC->ConsoleCommand(TEXT("r.BloomQuality 1"));
				PC->ConsoleCommand(TEXT("foliage.DensityScale 0.3"));
				if (CloudQualityComboBox)
				{
					CloudQualityComboBox->SetSelectedIndex(0);
				}
			}
			if (SelectedQuality == 0 && Manager && Manager->SunSkyActor)
			{
				USkyLightComponent* SkyLight = Manager->SunSkyActor->FindComponentByClass<USkyLightComponent>();
				if (SkyLight)
				{
					SkyLight->SetRealTimeCaptureEnabled(false);
					SkyLight->SetMobility(EComponentMobility::Stationary);
					SkyLight->MarkRenderStateDirty();
				}
			}
		}
	}
	if (CloudQualityComboBox)
	{
		int32 CloudIndex = CloudQualityComboBox->GetSelectedIndex();
		TArray<AActor*> CloudActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), CloudActors);
		for (AActor* Actor : CloudActors)
		{
			if (Actor && Actor->GetName().Contains("VolumetricCloud"))
			{
				Actor->Tags.Remove(FName("CloudQuality_Low"));
				if (CloudIndex == 0)
				{
					Actor->SetActorHiddenInGame(true);
				}
				else
				{
					Actor->SetActorHiddenInGame(false);
					UActorComponent* CloudComponent = Actor->GetComponentByClass(UVolumetricCloudComponent::StaticClass());
					UVolumetricCloudComponent* VCloud = Cast<UVolumetricCloudComponent>(CloudComponent);
					if (VCloud)
					{
						if (CloudIndex == 1)
						{
							Actor->Tags.Add(FName("CloudQuality_Low"));
							VCloud->ViewSampleCountScale = 0.35f;
							VCloud->ShadowViewSampleCountScale = 0.1f;
							if (PC) PC->ConsoleCommand(TEXT("r.VolumetricCloud.ViewRaySampleCountMax 32"));
						}
						else
						{
							VCloud->ViewSampleCountScale = 1.0f;
							VCloud->ShadowViewSampleCountScale = 1.0f;
							if (PC) PC->ConsoleCommand(TEXT("r.VolumetricCloud.ViewRaySampleCountMax 128"));
						}
					}
				}
				break;
			}
		}
	}
	if (CesiumQualityComboBox || CesiumCacheComboBox)
	{
		float TargetSSE = 16.0f;
		int64 TargetCacheBytes = 536870912;
		if (CesiumQualityComboBox)
		{
			int32 CesiumIndex = CesiumQualityComboBox->GetSelectedIndex();
			if (CesiumIndex == 0) TargetSSE = 128.0f;
			else if (CesiumIndex == 1) TargetSSE = 64.0f;
			else if (CesiumIndex == 2) TargetSSE = 32.0f;
			GConfig->SetInt(TEXT("SetariCustom"), TEXT("CalitateCesium"), CesiumIndex, GGameUserSettingsIni);
		}
		if (CesiumCacheComboBox)
		{
			int32 CacheIndex = CesiumCacheComboBox->GetSelectedIndex();
			if (CacheIndex == 0) TargetCacheBytes = 268435456;
			else if (CacheIndex == 1) TargetCacheBytes = 536870912;
			else TargetCacheBytes = 1073741824;
			GConfig->SetInt(TEXT("SetariCustom"), TEXT("CacheCesium"), CacheIndex, GGameUserSettingsIni);
		}
		TArray<AActor*> CesiumTilesets;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACesium3DTileset::StaticClass(), CesiumTilesets);
		for (AActor* Actor : CesiumTilesets)
		{
			ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(Actor);
			if (Tileset)
			{
				Tileset->MaximumScreenSpaceError = TargetSSE;
				Tileset->MaximumCachedBytes = TargetCacheBytes;
			}
		}
	}
	if (FPSComboBox)
	{
		int32 FPSIndex = FPSComboBox->GetSelectedIndex();
		float NewFPS = 0.0f;
		if (FPSIndex == 0) NewFPS = 30.0f;
		else if (FPSIndex == 1) NewFPS = 40.0f;
		else if (FPSIndex == 2) NewFPS = 60.0f;
		Settings->SetFrameRateLimit(NewFPS);
		if (PC) PC->ConsoleCommand(FString::Printf(TEXT("t.MaxFPS %d"), (int32)NewFPS));
	}
	if (VSyncCheckBox)
	{
		Settings->SetVSyncEnabled(VSyncCheckBox->IsChecked());
	}
	if (MotionBlurCheckBox && PC)
	{
		if (MotionBlurCheckBox->IsChecked())
		{
			PC->ConsoleCommand(TEXT("r.MotionBlurQuality 4"));
		}
		else
		{
			PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));
		}
	}
	if (ResScaleSlider)
	{
		float ResVal = ResScaleSlider->GetValue();
		Settings->SetResolutionScaleValueEx(ResVal);
		if (PC) PC->ConsoleCommand(FString::Printf(TEXT("r.ScreenPercentage %f"), ResVal));
	}
	Settings->ApplySettings(true);
	Settings->SaveSettings();
	if (DroneVisCheckBox && PC)
	{
		bool bDroneVisible = DroneVisCheckBox->IsChecked();
		GConfig->SetBool(TEXT("SetariCustom"), TEXT("VizibilitateDrona"), bDroneVisible, GGameUserSettingsIni);
		ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
		if (Drona)
		{
			Drona->SetActorHiddenInGame(!bDroneVisible);
		}
	}
	if (HideHUDCheckBox && PC)
	{
		bool bHideHUD = HideHUDCheckBox->IsChecked();
		GConfig->SetBool(TEXT("SetariCustom"), TEXT("AscundeHUD"), bHideHUD, GGameUserSettingsIni);
		if (PC)
		{
			ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
			if (MyHUD)
			{
				MyHUD->bDrawTelemetry = !bHideHUD;
			}
		}
	}
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareR"), SliderR->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareG"), SliderG->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareB"), SliderB->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("Opacitate"), SliderA->GetValue(), GGameUserSettingsIni);
	}
	if (PenumbraSliderR && PenumbraSliderG && PenumbraSliderB && PenumbraSliderA && GConfig)
	{
		GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareR"), PenumbraSliderR->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareG"), PenumbraSliderG->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareB"), PenumbraSliderB->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("Opacitate"), PenumbraSliderA->GetValue(), GGameUserSettingsIni);
	}
	if (GConfig && EarthUmbraSliderR && EarthUmbraSliderG && EarthUmbraSliderB && EarthUmbraSliderA)
	{
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareR"), EarthUmbraSliderR->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareG"), EarthUmbraSliderG->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareB"), EarthUmbraSliderB->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("Opacitate"), EarthUmbraSliderA->GetValue(), GGameUserSettingsIni);
	}
	if (GConfig && EarthPenumbraSliderR && EarthPenumbraSliderG && EarthPenumbraSliderB && EarthPenumbraSliderA)
	{
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareR"), EarthPenumbraSliderR->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareG"), EarthPenumbraSliderG->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareB"), EarthPenumbraSliderB->GetValue(), GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("Opacitate"), EarthPenumbraSliderA->GetValue(), GGameUserSettingsIni);
	}
	if (Manager)
	{
		Manager->ApplyEarthShadowColors();
	}
	if (CesiumKeyTextBox)
	{
		FString NewCesiumKey = CesiumKeyTextBox->GetText().ToString();
		FString OldCesiumKey = TEXT("");
		GConfig->GetString(TEXT("SetariCustom"), TEXT("CesiumKey"), OldCesiumKey, GGameUserSettingsIni);
		if (NewCesiumKey != OldCesiumKey)
		{
			GConfig->SetString(TEXT("SetariCustom"), TEXT("CesiumKey"), *NewCesiumKey, GGameUserSettingsIni);
			TArray<AActor*> CesiumTilesets;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACesium3DTileset::StaticClass(), CesiumTilesets);
			for (AActor* Actor : CesiumTilesets)
			{
				ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(Actor);
				if (Tileset)
				{
					Tileset->SetIonAccessToken(NewCesiumKey);
					Tileset->RefreshTileset();
				}
			}
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Harta Cesium se reincarca cu noul Token..."));
		}
	}
	bool bLanguageChanged = false;
	if (LanguageComboBox)
	{
		FString SelectedLang = LanguageComboBox->GetSelectedOption();
		FString OldLang = TEXT("Romana");
		GConfig->GetString(TEXT("SetariCustom"), TEXT("Limba"), OldLang, GGameUserSettingsIni);
		if (SelectedLang != OldLang)
		{
			GConfig->SetString(TEXT("SetariCustom"), TEXT("Limba"), *SelectedLang, GGameUserSettingsIni);
			bLanguageChanged = true;
		}
	}
	GConfig->Flush(false, GGameUserSettingsIni);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("SETARILE AU FOST APLICATE CU SUCCES!"));
	}
	if (bLanguageChanged)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TEXT("Limba aplicata live!"));
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UMainMenuWidget::StaticClass(), true);
		for (UUserWidget* Widget : FoundWidgets)
		{
			UMainMenuWidget* MainMenu = Cast<UMainMenuWidget>(Widget);
			if (MainMenu)
			{
				MainMenu->RebuildTelemetryForLanguage();
				MainMenu->RemoveFromParent();
			}
		}
		if (PC)
		{
			UMainMenuWidget* NewMainMenu = CreateWidget<UMainMenuWidget>(PC, UMainMenuWidget::StaticClass());
			if (NewMainMenu)
			{
				NewMainMenu->AddToViewport(10);
			}
			USettingsMenuWidget* NewSettings = CreateWidget<USettingsMenuWidget>(PC, USettingsMenuWidget::StaticClass());
			if (NewSettings)
			{
				NewSettings->AddToViewport(100);
			}
		}
		this->RemoveFromParent();
		return;
	}
	else
	{
		OnCloseClicked();
	}
}
void USettingsMenuWidget::OnAudioChanged(float Value)
{
	GCurrentMasterVolume = Value;
	if (AudioValueText)
	{
		AudioValueText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Value))));
	}
	float VolumeMultiplier = Value / 100.0f;
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		FString Command = FString::Printf(TEXT("s.Volume %f"), VolumeMultiplier);
		PC->ConsoleCommand(Command);
	}
}
void USettingsMenuWidget::OnSensitivityChanged(float Value)
{
	if (SensitivityValueText)
	{
		SensitivityValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
	GConfig->SetFloat(TEXT("SetariCustom"), TEXT("Sensibilitate"), Value, GGameUserSettingsIni);
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
		if (Drona)
		{
			Drona->SensibilitateMouse = Value;
		}
	}
}
void USettingsMenuWidget::OnFOVChanged(float Value)
{
	if (FOVValueText)
	{
		FOVValueText->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), Value)));
	}
	GConfig->SetFloat(TEXT("SetariCustom"), TEXT("FOV"), Value, GGameUserSettingsIni);
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->FOV(Value);
	}
}
void USettingsMenuWidget::OnResetClicked()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (Settings)
	{
		Settings->SetOverallScalabilityLevel(0);
		Settings->SetResolutionScaleValueEx(100.0f);
		Settings->SetFrameRateLimit(30.0f);
		Settings->SetVSyncEnabled(true);
		Settings->ApplySettings(true);
		Settings->SaveSettings();
	}
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareR"), 0.02f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareG"), 0.02f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("CuloareB"), 0.08f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariUmbraPamant"), TEXT("Opacitate"), 0.6f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareR"), 0.15f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareG"), 0.1f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("CuloareB"), 0.02f, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariPenumbraPamant"), TEXT("Opacitate"), 0.25f, GGameUserSettingsIni);
	}
	AOrbitalManager* EarthManager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (EarthManager)
	{
		EarthManager->ApplyEarthShadowColors();
	}
	if (SliderR && SliderG && SliderB && SliderA && ColorPreviewBorder)
	{
		SliderR->SetValue(0.0f);
		SliderG->SetValue(0.0f);
		SliderB->SetValue(0.0f);
		SliderA->SetValue(0.5f);
		ColorPreviewBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		if (GConfig)
		{
			GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareR"), 0.0f, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareG"), 0.0f, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("CuloareB"), 0.0f, GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("SetariUmbra"), TEXT("Opacitate"), 0.5f, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		if (PenumbraSliderR && PenumbraSliderG && PenumbraSliderB && PenumbraSliderA && PenumbraColorPreviewBorder)
		{
			PenumbraSliderR->SetValue(0.0f); PenumbraSliderG->SetValue(0.0f); PenumbraSliderB->SetValue(0.0f); PenumbraSliderA->SetValue(0.3f);
			PenumbraColorPreviewBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
			if (GConfig)
			{
				GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareR"), 0.0f, GGameUserSettingsIni);
				GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareG"), 0.0f, GGameUserSettingsIni);
				GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("CuloareB"), 0.0f, GGameUserSettingsIni);
				GConfig->SetFloat(TEXT("SetariPenumbra"), TEXT("Opacitate"), 0.3f, GGameUserSettingsIni);
			}
		}
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager && Manager->DynUmbraMaterial)
		{
			Manager->DynUmbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
			Manager->DynUmbraMaterial->SetScalarParameterValue(FName("Opacitate"), 0.5f);
		}
		if (Manager->DynPenumbraMaterial)
		{
			Manager->DynPenumbraMaterial->SetVectorParameterValue(FName("Culoare"), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
			Manager->DynPenumbraMaterial->SetScalarParameterValue(FName("Opacitate"), 0.3f);
		}
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->ConsoleCommand(TEXT("r.MotionBlurQuality 4"));
		PC->ConsoleCommand(TEXT("gamma 2.2"));
		PC->ConsoleCommand(TEXT("s.Volume 1.0"));
		PC->ConsoleCommand(TEXT("r.VolumetricCloud.ViewRaySampleCountMax 128"));
	}
	if (GConfig)
	{
		GConfig->EmptySection(TEXT("SetariCustom"), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (PC)
	{
		PC->FOV(90.0f);
		ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
		if (Drona)
		{
			Drona->SensibilitateMouse = 1.0f;
			if (Drona->DroneMesh) Drona->DroneMesh->SetHiddenInGame(false);
			if (Drona->PropellerFL) Drona->PropellerFL->SetHiddenInGame(false);
			if (Drona->PropellerFR) Drona->PropellerFR->SetHiddenInGame(false);
			if (Drona->PropellerBL) Drona->PropellerBL->SetHiddenInGame(false);
			if (Drona->PropellerBR) Drona->PropellerBR->SetHiddenInGame(false);
		}
		ADroneHUD* MyHUD = Cast<ADroneHUD>(PC->GetHUD());
		if (MyHUD)
		{
			MyHUD->bDrawTelemetry = true;
		}
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("Setarile au fost resetate la default!"));
	}
	if (CesiumKeyTextBox)
	{
		CesiumKeyTextBox->SetText(FText::GetEmpty());
	}
	LoadCurrentSettings();
}
void USettingsMenuWidget::OnToggleCesiumKeyVisibilityClicked()
{
	if (CesiumKeyTextBox && ToggleKeyVisibilityText)
	{
		bool bIsCurrentlyPassword = CesiumKeyTextBox->GetIsPassword();
		CesiumKeyTextBox->SetIsPassword(!bIsCurrentlyPassword);
		if (bIsCurrentlyPassword)
		{
			ToggleKeyVisibilityText->SetText(AOrbitalManager::Tr(TEXT("Ascunde"), TEXT("Hide")));
		}
		else
		{
			ToggleKeyVisibilityText->SetText(AOrbitalManager::Tr(TEXT("Arata"), TEXT("Show")));
		}
	}
}
void USettingsMenuWidget::OnCesiumLinkClicked()
{
	FString CesiumURL = TEXT("https://ion.cesium.com/tokens");
	FPlatformProcess::LaunchURL(*CesiumURL, nullptr, nullptr);
}
void UMainMenuWidget::OpenBookmarksMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UBookmarksMenuWidget* BookmarksWidget = CreateWidget<UBookmarksMenuWidget>(PC, UBookmarksMenuWidget::StaticClass());
		if (BookmarksWidget)
		{
			BookmarksWidget->AddToViewport(100);
		}
	}
	ToggleMenu();
}
void UBookmarkActionBtn::HandleClick()
{
	if (ParentMenu)
	{
		if (bIsDeleteBtn) ParentMenu->OnDeleteClicked(TargetIndex);
		else ParentMenu->OnTeleportClicked(TargetIndex);
	}
}
TSharedRef<SWidget> UBookmarksMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.98f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(700.0f, 920.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(20.f, 15.f));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("LOCATII SALVATE (BOOKMARKS)"), TEXT("SAVED LOCATIONS (BOOKMARKS)")));
	Title->SetFont(TitleFont);
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(TitleFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UBookmarksMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	FSlateFontInfo SectionFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	auto StyleInputBox = [](UEditableTextBox* InputBox) {
		FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		TextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
		FSlateBrush BgBrush;
		BgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
		TextBoxStyle.BackgroundImageNormal = BgBrush;
		TextBoxStyle.BackgroundImageHovered = BgBrush;
		TextBoxStyle.BackgroundImageFocused = BgBrush;
		TextBoxStyle.BackgroundImageReadOnly = BgBrush;
		InputBox->SetWidgetStyle(TextBoxStyle);
		};
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	UTextBlock* CreateTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CreateTitle->SetText(AOrbitalManager::Tr(TEXT("ADAUGA UN NOU BOOKMARK"), TEXT("ADD A NEW BOOKMARK")));
	CreateTitle->SetFont(SectionFont);
	CreateTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.8f, 1.0f)));
	UVerticalBoxSlot* CreateTitleSlot = MainLayout->AddChildToVerticalBox(CreateTitle);
	if (CreateTitleSlot) CreateTitleSlot->SetPadding(FMargin(20.f, 20.f, 20.f, 5.f));
	UBorder* CreateZoneBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CreateZoneBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
	UVerticalBoxSlot* CreateZoneBgSlot = MainLayout->AddChildToVerticalBox(CreateZoneBg);
	if (CreateZoneBgSlot) CreateZoneBgSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	UVerticalBox* CreateBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CreateZoneBg->SetContent(CreateBox);
	CreateZoneBg->SetPadding(FMargin(15.f));
	TitleInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInputBox(TitleInput);
	TitleInput->SetHintText(AOrbitalManager::Tr(TEXT("Titlu Bookmark (ex: Eclipsa Totala Egipt 2027)"), TEXT("Bookmark Title (ex: Total Eclipse Egypt 2027)")));
	UVerticalBoxSlot* TitleInputSlot = CreateBox->AddChildToVerticalBox(TitleInput);
	if (TitleInputSlot) TitleInputSlot->SetPadding(FMargin(0, 0, 0, 10.f));
	DescInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInputBox(DescInput);
	DescInput->SetHintText(AOrbitalManager::Tr(TEXT("Descriere / Notite Personale (Optional)"), TEXT("Description / Personal Notes (Optional)")));
	UVerticalBoxSlot* DescInputSlot = CreateBox->AddChildToVerticalBox(DescInput);
	if (DescInputSlot) DescInputSlot->SetPadding(FMargin(0, 0, 0, 15.f));
	UTextBlock* TimeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TimeLabel->SetText(AOrbitalManager::Tr(TEXT("DATA SI ORA:"), TEXT("DATE AND TIME:")));
	TimeLabel->SetFont(LabelFont);
	TimeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	UVerticalBoxSlot* TimeLabelSlot = CreateBox->AddChildToVerticalBox(TimeLabel);
	if (TimeLabelSlot) TimeLabelSlot->SetPadding(FMargin(0, 0, 0, 5.f));
	UHorizontalBox* TimeBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* TimeBoxSlot = CreateBox->AddChildToVerticalBox(TimeBox);
	if (TimeBoxSlot) TimeBoxSlot->SetPadding(FMargin(0, 0, 0, 15.f));
	YearInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(YearInput); YearInput->SetHintText(AOrbitalManager::Tr(TEXT("An"), TEXT("Yr")));
	MonthInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(MonthInput); MonthInput->SetHintText(AOrbitalManager::Tr(TEXT("Luna"), TEXT("Mo")));
	DayInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(DayInput); DayInput->SetHintText(AOrbitalManager::Tr(TEXT("Zi"), TEXT("Day")));
	HourInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(HourInput); HourInput->SetHintText(AOrbitalManager::Tr(TEXT("Ora"), TEXT("Hr")));
	MinInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(MinInput); MinInput->SetHintText(AOrbitalManager::Tr(TEXT("Min"), TEXT("Min")));
	UHorizontalBoxSlot* YSlot = TimeBox->AddChildToHorizontalBox(YearInput); if (YSlot) { YSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); YSlot->SetPadding(FMargin(0, 0, 5.f, 0)); }
	UHorizontalBoxSlot* MSlot = TimeBox->AddChildToHorizontalBox(MonthInput); if (MSlot) { MSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MSlot->SetPadding(FMargin(0, 0, 5.f, 0)); }
	UHorizontalBoxSlot* DSlot = TimeBox->AddChildToHorizontalBox(DayInput); if (DSlot) { DSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); DSlot->SetPadding(FMargin(0, 0, 15.f, 0)); }
	UHorizontalBoxSlot* HSlot = TimeBox->AddChildToHorizontalBox(HourInput); if (HSlot) { HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HSlot->SetPadding(FMargin(0, 0, 5.f, 0)); }
	UHorizontalBoxSlot* MinSlot = TimeBox->AddChildToHorizontalBox(MinInput); if (MinSlot) MinSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* LocLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LocLabel->SetText(AOrbitalManager::Tr(TEXT("COORDONATE GPS:"), TEXT("GPS COORDINATES:")));
	LocLabel->SetFont(LabelFont);
	LocLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	UVerticalBoxSlot* LocLabelSlot = CreateBox->AddChildToVerticalBox(LocLabel);
	if (LocLabelSlot) LocLabelSlot->SetPadding(FMargin(0, 0, 0, 5.f));
	UHorizontalBox* LocBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* LocBoxSlot = CreateBox->AddChildToVerticalBox(LocBox);
	if (LocBoxSlot) LocBoxSlot->SetPadding(FMargin(0, 0, 0, 15.f));
	LatInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(LatInput); LatInput->SetHintText(AOrbitalManager::Tr(TEXT("Latitudine"), TEXT("Latitude")));
	LonInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(LonInput); LonInput->SetHintText(AOrbitalManager::Tr(TEXT("Longitudine"), TEXT("Longitude")));
	AltInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); StyleInputBox(AltInput); AltInput->SetHintText(AOrbitalManager::Tr(TEXT("Altitudine (m)"), TEXT("Altitude (m)")));
	UHorizontalBoxSlot* LatSlot = LocBox->AddChildToHorizontalBox(LatInput); if (LatSlot) { LatSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LatSlot->SetPadding(FMargin(0, 0, 10.f, 0)); }
	UHorizontalBoxSlot* LonSlot = LocBox->AddChildToHorizontalBox(LonInput); if (LonSlot) { LonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LonSlot->SetPadding(FMargin(0, 0, 10.f, 0)); }
	UHorizontalBoxSlot* AltSlot = LocBox->AddChildToHorizontalBox(AltInput); if (AltSlot) AltSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* OpticsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OpticsLabel->SetText(AOrbitalManager::Tr(TEXT("OPTICA (ZOOM & TRACKING):"), TEXT("OPTICS (ZOOM & TRACKING):")));
	OpticsLabel->SetFont(LabelFont);
	OpticsLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	UVerticalBoxSlot* OpticsLabelSlot = CreateBox->AddChildToVerticalBox(OpticsLabel);
	if (OpticsLabelSlot) OpticsLabelSlot->SetPadding(FMargin(0, 0, 0, 5.f));
	UHorizontalBox* OpticsBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* OpticsBoxSlot = CreateBox->AddChildToVerticalBox(OpticsBox);
	if (OpticsBoxSlot) OpticsBoxSlot->SetPadding(FMargin(0, 0, 0, 20.f));
	UVerticalBox* FovContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* FovContainerSlot = OpticsBox->AddChildToHorizontalBox(FovContainer);
	if (FovContainerSlot) { FovContainerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); FovContainerSlot->SetPadding(FMargin(0, 0, 15.f, 0)); }
	SavedFovSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	SavedFovSlider->SetMinValue(0.0001f);
	SavedFovSlider->SetMaxValue(135.0f);
	SavedFovSlider->SetValue(90.0f);
	SavedFovSlider->OnValueChanged.AddDynamic(this, &UBookmarksMenuWidget::OnFovSliderChanged);
	UVerticalBoxSlot* SliderSlot = FovContainer->AddChildToVerticalBox(SavedFovSlider);
	if (SliderSlot) SliderSlot->SetPadding(FMargin(0, 0, 0, 5.f));
	SavedFovInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	StyleInputBox(SavedFovInput);
	SavedFovInput->SetHintText(FText::FromString("FOV (Zoom)"));
	SavedFovInput->SetText(FText::FromString("90.0000"));
	SavedFovInput->SetJustification(ETextJustify::Center);
	SavedFovInput->OnTextCommitted.AddDynamic(this, &UBookmarksMenuWidget::OnFovInputCommitted);
	FovContainer->AddChildToVerticalBox(SavedFovInput);
	SavedTrackCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	SavedTrackCombo->SetWidgetStyle(ComboStyle);
	SavedTrackCombo->ItemStyle = ComboRowStyle;
	SavedTrackCombo->ForegroundColor = FSlateColor(FLinearColor::White);
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Venus"), TEXT("Venus")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Jupiter"), TEXT("Jupiter")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Saturn"), TEXT("Saturn")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Uranus"), TEXT("Uranus")));
	SavedTrackCombo->AddOption(AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune")));
	SavedTrackCombo->SetSelectedOption(AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)")));
	UHorizontalBoxSlot* TrackSlot = OpticsBox->AddChildToHorizontalBox(SavedTrackCombo);
	if (TrackSlot) TrackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* ActionBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	CreateBox->AddChildToVerticalBox(ActionBox);
	GetCurrentDataButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	GetCurrentDataButton->SetBackgroundColor(FLinearColor(0.2f, 0.4f, 0.8f));
	UTextBlock* GetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	GetTxt->SetText(AOrbitalManager::Tr(TEXT(" CITESTE TOATE DATELE CURENTE "), TEXT(" READ ALL CURRENT DATA ")));
	GetTxt->SetFont(ButtonFont);
	GetTxt->SetMargin(FMargin(0, 10.f));
	GetTxt->SetJustification(ETextJustify::Center);
	GetCurrentDataButton->AddChild(GetTxt);
	GetCurrentDataButton->OnClicked.AddDynamic(this, &UBookmarksMenuWidget::OnGetCurrentDataClicked);
	UHorizontalBoxSlot* GetBtnSlot = ActionBox->AddChildToHorizontalBox(GetCurrentDataButton);
	if (GetBtnSlot) { GetBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GetBtnSlot->SetPadding(FMargin(0, 0, 10.f, 0)); }
	SaveBookmarkButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	SaveBookmarkButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f));
	UTextBlock* SaveTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SaveTxt->SetText(AOrbitalManager::Tr(TEXT(" SALVEAZA BOOKMARK "), TEXT(" SAVE BOOKMARK ")));
	SaveTxt->SetFont(ButtonFont);
	SaveTxt->SetMargin(FMargin(0, 10.f));
	SaveTxt->SetJustification(ETextJustify::Center);
	SaveBookmarkButton->AddChild(SaveTxt);
	SaveBookmarkButton->OnClicked.AddDynamic(this, &UBookmarksMenuWidget::OnSaveBookmarkClicked);
	UHorizontalBoxSlot* SaveBtnSlot = ActionBox->AddChildToHorizontalBox(SaveBookmarkButton);
	if (SaveBtnSlot) SaveBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* ListTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ListTitle->SetText(AOrbitalManager::Tr(TEXT("LISTA LOCATII SALVATE"), TEXT("SAVED LOCATIONS LIST")));
	ListTitle->SetFont(SectionFont);
	ListTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.8f, 1.0f)));
	UVerticalBoxSlot* ListTitleSlot = MainLayout->AddChildToVerticalBox(ListTitle);
	if (ListTitleSlot) ListTitleSlot->SetPadding(FMargin(20.f, 10.f, 20.f, 5.f));
	BookmarksListScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBoxSlot* ScrollSlot = MainLayout->AddChildToVerticalBox(BookmarksListScroll);
	if (ScrollSlot)
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ScrollSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	}
	LoadBookmarksFromDisk();
	RefreshBookmarksList();
	return Super::RebuildWidget();
}
void UBookmarksMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
}
void UBookmarksMenuWidget::OnGetCurrentDataClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		YearInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Year)));
		MonthInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Month)));
		DayInput->SetText(FText::FromString(FString::FromInt(Manager->CurrentDate.Day)));
		int32 H = FMath::FloorToInt(Manager->CurrentDate.Hour);
		int32 M = FMath::FloorToInt((Manager->CurrentDate.Hour - H) * 60.0f);
		HourInput->SetText(FText::FromString(FString::FromInt(H)));
		MinInput->SetText(FText::FromString(FString::FromInt(M)));
	}
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	ADronaMea* Drona = Cast<ADronaMea>(UGameplayStatics::GetActorOfClass(GetWorld(), ADronaMea::StaticClass()));
	if (GeoRef && Drona)
	{
		FVector Coords = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(Drona->GetActorLocation());
		LatInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Coords.Y)));
		LonInput->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), Coords.X)));
		AltInput->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), Coords.Z)));
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerCameraManager)
	{
		float CurrentFOV = PC->PlayerCameraManager->GetFOVAngle();
		if (SavedFovInput) SavedFovInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), CurrentFOV)));
		if (SavedFovSlider) SavedFovSlider->SetValue(CurrentFOV);
	}
	if (Drona)
	{
		FString T = Drona->TrackingTarget;
		FString UIDisplayTarget = T;
		if (T == "Libera (Niciunul)") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Libera (Niciunul)"), TEXT("Free (None)"));
		else if (T == "Soare") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Soare"), TEXT("Sun"));
		else if (T == "Luna") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Luna"), TEXT("Moon"));
		else if (T == "Mercur") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Mercur"), TEXT("Mercury"));
		else if (T == "Marte") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Marte"), TEXT("Mars"));
		else if (T == "Neptun") UIDisplayTarget = AOrbitalManager::TrString(TEXT("Neptun"), TEXT("Neptune"));
		SavedTrackCombo->SetSelectedOption(UIDisplayTarget);
	}
}
void UBookmarksMenuWidget::OnSaveBookmarkClicked()
{
	FAstroBookmark NewBM;
	NewBM.Title = TitleInput->GetText().ToString();
	if (NewBM.Title.IsEmpty())
	{
		NewBM.Title = FString::Printf(TEXT("Bookmark %d"), SavedBookmarks.Num() + 1);
	}
	NewBM.Description = DescInput->GetText().ToString();
	NewBM.Year = FCString::Atoi(*YearInput->GetText().ToString());
	NewBM.Month = FCString::Atoi(*MonthInput->GetText().ToString());
	NewBM.Day = FCString::Atoi(*DayInput->GetText().ToString());
	NewBM.Hour = FCString::Atoi(*HourInput->GetText().ToString());
	NewBM.Minute = FCString::Atoi(*MinInput->GetText().ToString());
	NewBM.Latitude = FCString::Atod(*LatInput->GetText().ToString());
	NewBM.Longitude = FCString::Atod(*LonInput->GetText().ToString());
	NewBM.Altitude = FCString::Atod(*AltInput->GetText().ToString());
	NewBM.CameraFOV = FCString::Atof(*SavedFovInput->GetText().ToString());
	FString SelectedUI = SavedTrackCombo->GetSelectedOption();
	FString InternalTarget = SelectedUI;
	if (SelectedUI == "Free (None)") InternalTarget = "Libera (Niciunul)";
	else if (SelectedUI == "Sun") InternalTarget = "Soare";
	else if (SelectedUI == "Moon") InternalTarget = "Luna";
	else if (SelectedUI == "Mercury") InternalTarget = "Mercur";
	else if (SelectedUI == "Mars") InternalTarget = "Marte";
	else if (SelectedUI == "Neptune") InternalTarget = "Neptun";
	NewBM.TrackingTarget = InternalTarget;
	SavedBookmarks.Add(NewBM);
	SaveBookmarksToDisk();
	RefreshBookmarksList();
	TitleInput->SetText(FText::GetEmpty());
	DescInput->SetText(FText::GetEmpty());
}
void UBookmarksMenuWidget::RefreshBookmarksList()
{
	if (!BookmarksListScroll) return;
	BookmarksListScroll->ClearChildren();
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	FSlateFontInfo DescFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	for (int32 i = 0; i < SavedBookmarks.Num(); i++)
	{
		FAstroBookmark BM = SavedBookmarks[i];
		UBorder* RowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		RowBg->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.18f, 1.0f));
		BookmarksListScroll->AddChild(RowBg);
		UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		RowBg->SetContent(RowBox);
		RowBg->SetPadding(FMargin(10.f));
		UVerticalBox* TextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RowBox->AddChildToHorizontalBox(TextBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* TxtTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TxtTitle->SetText(FText::FromString(BM.Title));
		TxtTitle->SetFont(TitleFont);
		TextBox->AddChildToVerticalBox(TxtTitle);
		FString Details = AOrbitalManager::TrString(
			FString::Printf(TEXT("Data: %02d.%02d.%d %02d:%02d | Lat: %.3f Lon: %.3f"), BM.Day, BM.Month, BM.Year, BM.Hour, BM.Minute, BM.Latitude, BM.Longitude),
			FString::Printf(TEXT("Date: %02d.%02d.%d %02d:%02d | Lat: %.3f Lon: %.3f"), BM.Day, BM.Month, BM.Year, BM.Hour, BM.Minute, BM.Latitude, BM.Longitude)
		);
		UTextBlock* TxtDetails = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TxtDetails->SetText(FText::FromString(Details));
		TxtDetails->SetFont(DescFont);
		TxtDetails->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)));
		TextBox->AddChildToVerticalBox(TxtDetails);
		UBookmarkActionBtn* TeleportBtn = WidgetTree->ConstructWidget<UBookmarkActionBtn>(UBookmarkActionBtn::StaticClass());
		TeleportBtn->TargetIndex = i;
		TeleportBtn->bIsDeleteBtn = false;
		TeleportBtn->ParentMenu = this;
		TeleportBtn->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.1f));
		UTextBlock* TpTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TpTxt->SetText(AOrbitalManager::Tr(TEXT(" VIZITEAZA "), TEXT(" VISIT ")));
		TpTxt->SetFont(TitleFont);
		TeleportBtn->AddChild(TpTxt);
		TeleportBtn->OnClicked.AddDynamic(TeleportBtn, &UBookmarkActionBtn::HandleClick);
		RowBox->AddChildToHorizontalBox(TeleportBtn)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		UBookmarkActionBtn* DeleteBtn = WidgetTree->ConstructWidget<UBookmarkActionBtn>(UBookmarkActionBtn::StaticClass());
		DeleteBtn->TargetIndex = i;
		DeleteBtn->bIsDeleteBtn = true;
		DeleteBtn->ParentMenu = this;
		DeleteBtn->SetBackgroundColor(FLinearColor(0.6f, 0.1f, 0.1f));
		UTextBlock* DelTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DelTxt->SetText(FText::FromString(" X "));
		DelTxt->SetFont(TitleFont);
		DeleteBtn->AddChild(DelTxt);
		DeleteBtn->OnClicked.AddDynamic(DeleteBtn, &UBookmarkActionBtn::HandleClick);
		RowBox->AddChildToHorizontalBox(DeleteBtn)->SetPadding(FMargin(5.f, 0.f, 0.f, 0.f));
		USpacer* RowSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		RowSpacer->SetSize(FVector2D(1.0f, 5.0f));
		BookmarksListScroll->AddChild(RowSpacer);
	}
}
void UBookmarksMenuWidget::OnTeleportClicked(int32 BookmarkIndex)
{
	if (!SavedBookmarks.IsValidIndex(BookmarkIndex)) return;
	FAstroBookmark BM = SavedBookmarks[BookmarkIndex];
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		float FinalHour = (float)BM.Hour + ((float)BM.Minute / 60.0f);
		Manager->SetSimulationDateTime(BM.Day, BM.Month, BM.Year, FinalHour);
	}
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	ADronaMea* Drona = Cast<ADronaMea>(UGameplayStatics::GetActorOfClass(GetWorld(), ADronaMea::StaticClass()));
	if (GeoRef && Drona)
	{
		FVector TargetLLH(BM.Longitude, BM.Latitude, BM.Altitude);
		GeoRef->SetOriginLongitudeLatitudeHeight(TargetLLH);
		Drona->SetActorLocation(FVector(0, 0, BM.Altitude * 100.0f));
		Drona->CurrentVelocity = FVector::ZeroVector;
		Drona->TrackingTarget = BM.TrackingTarget;
	}
	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetFOV(BM.CameraFOV);
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor::Green, FString::Printf(TEXT("Vizita: %s | Track: %s | FOV: %.1f"), *BM.Title, *BM.TrackingTarget, BM.CameraFOV));
	OnCloseClicked();
}
void UBookmarksMenuWidget::OnDeleteClicked(int32 BookmarkIndex)
{
	if (SavedBookmarks.IsValidIndex(BookmarkIndex))
	{
		SavedBookmarks.RemoveAt(BookmarkIndex);
		SaveBookmarksToDisk();
		RefreshBookmarksList();
	}
}
void UBookmarksMenuWidget::SaveBookmarksToDisk()
{
	if (!GConfig) return;
	GConfig->EmptySection(TEXT("BookmarksData"), GGameUserSettingsIni);
	GConfig->SetInt(TEXT("BookmarksData"), TEXT("Count"), SavedBookmarks.Num(), GGameUserSettingsIni);
	for (int32 i = 0; i < SavedBookmarks.Num(); i++)
	{
		FString Pfx = FString::Printf(TEXT("BM_%d_"), i);
		GConfig->SetString(TEXT("BookmarksData"), *(Pfx + TEXT("Title")), *SavedBookmarks[i].Title, GGameUserSettingsIni);
		GConfig->SetString(TEXT("BookmarksData"), *(Pfx + TEXT("Desc")), *SavedBookmarks[i].Description, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("BookmarksData"), *(Pfx + TEXT("Y")), SavedBookmarks[i].Year, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("BookmarksData"), *(Pfx + TEXT("M")), SavedBookmarks[i].Month, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("BookmarksData"), *(Pfx + TEXT("D")), SavedBookmarks[i].Day, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("BookmarksData"), *(Pfx + TEXT("H")), SavedBookmarks[i].Hour, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("BookmarksData"), *(Pfx + TEXT("Min")), SavedBookmarks[i].Minute, GGameUserSettingsIni);
		GConfig->SetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Lat")), SavedBookmarks[i].Latitude, GGameUserSettingsIni);
		GConfig->SetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Lon")), SavedBookmarks[i].Longitude, GGameUserSettingsIni);
		GConfig->SetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Alt")), SavedBookmarks[i].Altitude, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("BookmarksData"), *(Pfx + TEXT("FOV")), SavedBookmarks[i].CameraFOV, GGameUserSettingsIni);
		GConfig->SetString(TEXT("BookmarksData"), *(Pfx + TEXT("Target")), *SavedBookmarks[i].TrackingTarget, GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}
void UBookmarksMenuWidget::LoadBookmarksFromDisk()
{
	if (!GConfig) return;
	SavedBookmarks.Empty();
	int32 Count = 0;
	GConfig->GetInt(TEXT("BookmarksData"), TEXT("Count"), Count, GGameUserSettingsIni);
	for (int32 i = 0; i < Count; i++)
	{
		FAstroBookmark BM;
		FString Pfx = FString::Printf(TEXT("BM_%d_"), i);
		GConfig->GetString(TEXT("BookmarksData"), *(Pfx + TEXT("Title")), BM.Title, GGameUserSettingsIni);
		GConfig->GetString(TEXT("BookmarksData"), *(Pfx + TEXT("Desc")), BM.Description, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("BookmarksData"), *(Pfx + TEXT("Y")), BM.Year, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("BookmarksData"), *(Pfx + TEXT("M")), BM.Month, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("BookmarksData"), *(Pfx + TEXT("D")), BM.Day, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("BookmarksData"), *(Pfx + TEXT("H")), BM.Hour, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("BookmarksData"), *(Pfx + TEXT("Min")), BM.Minute, GGameUserSettingsIni);
		GConfig->GetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Lat")), BM.Latitude, GGameUserSettingsIni);
		GConfig->GetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Lon")), BM.Longitude, GGameUserSettingsIni);
		GConfig->GetDouble(TEXT("BookmarksData"), *(Pfx + TEXT("Alt")), BM.Altitude, GGameUserSettingsIni);
		BM.CameraFOV = 90.0f;
		BM.TrackingTarget = TEXT("Libera (Niciunul)");
		GConfig->GetFloat(TEXT("BookmarksData"), *(Pfx + TEXT("FOV")), BM.CameraFOV, GGameUserSettingsIni);
		GConfig->GetString(TEXT("BookmarksData"), *(Pfx + TEXT("Target")), BM.TrackingTarget, GGameUserSettingsIni);
		SavedBookmarks.Add(BM);
	}
}
void UBookmarksMenuWidget::OnFovSliderChanged(float Value)
{
	if (SavedFovInput)
	{
		SavedFovInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), Value)));
	}
}
void UBookmarksMenuWidget::OnFovInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter || CommitMethod == ETextCommit::OnUserMovedFocus)
	{
		float Value = FCString::Atof(*Text.ToString());
		Value = FMath::Clamp(Value, 0.0001f, 135.0f);
		if (SavedFovSlider)
		{
			SavedFovSlider->SetValue(Value);
		}
		if (SavedFovInput)
		{
			SavedFovInput->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), Value)));
		}
	}
}
void UMainMenuWidget::OpenHelpMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UHelpMenuWidget* HelpWidget = CreateWidget<UHelpMenuWidget>(PC, UHelpMenuWidget::StaticClass());
		if (HelpWidget)
		{
			HelpWidget->AddToViewport(100);
		}
	}
	ToggleMenu();
}
void UHelpResultBtn::HandleClick()
{
	if (ParentMenu)
	{
		ParentMenu->OpenTargetMenu(TargetMenuToOpen);
	}
}
TSharedRef<SWidget> UHelpMenuWidget::RebuildWidget()
{
	BuildDatabase();
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	MainMenuContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MainMenuContainer->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.98f));
	UCanvasPanelSlot* MainSlot = RootCanvas->AddChildToCanvas(MainMenuContainer);
	MainSlot->SetAnchors(FAnchors(0.5f));
	MainSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MainSlot->SetSize(FVector2D(800.0f, 700.0f));
	ManualContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ManualContainer->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.98f));
	UCanvasPanelSlot* ManualSlot = RootCanvas->AddChildToCanvas(ManualContainer);
	ManualSlot->SetAnchors(FAnchors(0.5f));
	ManualSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ManualSlot->SetSize(FVector2D(800.0f, 700.0f));
	ManualSlot->SetZOrder(5);
	ManualContainer->SetVisibility(ESlateVisibility::Collapsed);
	BuildManualUI();
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MainMenuContainer->SetContent(MainLayout);
	MainMenuContainer->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(20.f, 15.f));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr("AJUTOR SI CAUTARE RAPIDA", "HELP AND QUICK SEARCH"));
	Title->SetFont(TitleFont);
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(TitleFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UHelpMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	UBorder* SearchBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SearchBg->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 1.0f));
	MainLayout->AddChildToVerticalBox(SearchBg)->SetPadding(FMargin(20.f));
	SearchInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	FEditableTextBoxStyle SearchStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	SearchStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(0.15f, 0.15f, 0.2f, 1.0f));
	SearchStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	SearchStyle.TextStyle.Font = FCoreStyle::GetDefaultFontStyle("Regular", 14);
	SearchInput->SetWidgetStyle(SearchStyle);
	SearchInput->SetHintText(AOrbitalManager::Tr("Cauta setari, cuvinte cheie, optiuni... (ex: FPS, Zoom, Timp)", "Search settings, keywords, options... (e.g. FPS, Zoom, Time)"));
	SearchInput->OnTextChanged.AddDynamic(this, &UHelpMenuWidget::OnSearchTextChanged);
	SearchBg->SetContent(SearchInput);
	SearchBg->SetPadding(FMargin(20.f, 20.f, 20.f, 10.f));
	ManualButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ManualButton->SetBackgroundColor(FLinearColor(0.15f, 0.25f, 0.35f, 1.0f));
	UTextBlock* ManualTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ManualTxt->SetText(AOrbitalManager::Tr("GHID DE UTILIZARE (INSTRUCTIUNI) ", "USER GUIDE (INSTRUCTIONS) "));
	ManualTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	ManualTxt->SetJustification(ETextJustify::Center);
	ManualButton->AddChild(ManualTxt);
	ManualButton->OnClicked.AddDynamic(this, &UHelpMenuWidget::OnManualClicked);
	MainLayout->AddChildToVerticalBox(ManualButton)->SetPadding(FMargin(20.f, 0.f, 20.f, 15.f));
	ResultsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBoxSlot* ScrollSlot = MainLayout->AddChildToVerticalBox(ResultsScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 20.f));
	UpdateResults("");
	return Super::RebuildWidget();
}
void UHelpMenuWidget::OnManualClicked()
{
	if (!ManualContainer || !MainMenuContainer) return;
	if (ManualContainer->GetVisibility() == ESlateVisibility::Visible)
	{
		ManualContainer->SetVisibility(ESlateVisibility::Collapsed);
		MainMenuContainer->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ManualContainer->SetVisibility(ESlateVisibility::Visible);
		MainMenuContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}
void UHelpMenuWidget::BuildManualUI()
{
	if (!ManualContainer) return;
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ManualContainer->SetContent(Layout);
	ManualContainer->SetPadding(FMargin(15.f));
	UHorizontalBox* HeadBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Layout->AddChildToVerticalBox(HeadBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));
	FSlateFontInfo BigFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr("MANUAL DE BAZA", "BASIC MANUAL"));
	Title->SetFont(BigFont);
	HeadBox->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* CloseManualBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseManualBtn->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(BigFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseManualBtn->AddChild(XTxt);
	CloseManualBtn->OnClicked.AddDynamic(this, &UHelpMenuWidget::OnManualClicked);
	HeadBox->AddChildToHorizontalBox(CloseManualBtn);
	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBoxSlot* ScrollSlot = Layout->AddChildToVerticalBox(ScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	auto AddSection = [&](FString TitleRo, FString TitleEn, FString BodyRo, FString BodyEn)
		{
			if (!TitleRo.IsEmpty())
			{
				UTextBlock* TxtTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				TxtTitle->SetText(AOrbitalManager::Tr(TitleRo, TitleEn));
				TxtTitle->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
				TxtTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.75f, 0.3f, 1.0f)));
				UScrollBoxSlot* TitleSlot = Cast<UScrollBoxSlot>(ScrollBox->AddChild(TxtTitle));
				if (TitleSlot) TitleSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 5.0f));
			}
			if (!BodyRo.IsEmpty())
			{
				UTextBlock* TxtBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				TxtBody->SetText(AOrbitalManager::Tr(BodyRo, BodyEn));
				TxtBody->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
				TxtBody->SetAutoWrapText(true);
				TxtBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
				UScrollBoxSlot* BodySlot = Cast<UScrollBoxSlot>(ScrollBox->AddChild(TxtBody));
				if (BodySlot) BodySlot->SetPadding(FMargin(15.0f, 0.0f, 0.0f, 0.0f));
			}
		};
	AddSection(
		"BINE AI VENIT IN SIMULATORUL 3D DE ECLIPSE SOLARE!",
		"WELCOME TO THE 3D SOLAR ECLIPSE SIMULATOR!",
		"", ""
	);
	AddSection(
		"CONTROALE DE ZBOR (DRONA)",
		"FLIGHT CONTROLS (DRONE)",
		"[ W ] [ A ] [ S ] [ D ]  : Deplasare\n"
		"[ Q ] / [ E ]            : Sus / Jos\n"
		"[ Z ] / [ X ]            : Rotire stanga / dreapta (Yaw)\n"
		"[ Scroll Mouse ]         : Zoom optic inainte / inapoi\n"
		"[ Click Dreapta ] (Hold) : Misca camera liber",
		"[ W ] [ A ] [ S ] [ D ]  : Movement\n"
		"[ Q ] / [ E ]            : Up / Down\n"
		"[ Z ] / [ X ]            : Rotate left / right (Yaw)\n"
		"[ Mouse Scroll ]         : Optical zoom in / out\n"
		"[ Right Click ] (Hold)   : Move camera freely"
	);
	AddSection(
		"NAVIGATIE GLOBALA",
		"GLOBAL NAVIGATION",
		"[ M ] : Deschide / Inchide Harta Globala de sus.",
		"[ M ] : Open / Close Top-Down Global Map."
	);
	AddSection(
		"CONTROALE HARTA GLOBALA",
		"GLOBAL MAP CONTROLS",
		"[ W ] [ S ] : Deplasare Sus / Jos (Latitudine)\n"
		"[ A ] [ D ] : Deplasare Stanga / Dreapta (Longitudine)\n"
		"[ Q ] [ E ] : Rotatie in jurul punctului curent\n"
		"[ Click Stanga ] + Drag  : Trage globul pamantesc.\n"
		"[ Click Stanga ] (Simplu): Teleportare instanta.",
		"[ W ] [ S ] : Move Up / Down (Latitude)\n"
		"[ A ] [ D ] : Move Left / Right (Longitude)\n"
		"[ Q ] [ E ] : Rotate around the current point\n"
		"[ Left Click ] + Drag    : Drag the globe to explore.\n"
		"[ Left Click ] (Simple)  : Instant teleport to location."
	);
	AddSection(
		"SISTEMUL ASTRONOMIC",
		"ASTRONOMICAL SYSTEM",
		"Catalog NASA : Cauta o eclipsa si apasa 'Vezi'. Simulatorul va seta automat timpul si coordonatele pentru faza maxima a eclipsei!",
		"NASA Catalog : Search for an eclipse and click 'GO'. The simulator will automatically set the time and coordinates for the maximum phase!"
	);
	AddSection(
		"BUTOANE RAPIDE (HUD SUS)",
		"QUICK BUTTONS (TOP HUD)",
		"[ Harta (Glob) ] : Comuta intre Harta Globala si zborul normal (la fel ca tasta M).\n"
		"[ Eclipsa ] : Deschide tabelul tip Xavier Jubier cu timpii exacti de contact (C1 - C4) pentru locatia curenta, incluzand eventual si apusul sau rasaritul\n"
		"[ Nori ] : Ascunde sau arata norii de pe cer (la fel ca tasta N).\n"
		"[ Play / Pauza ] : Opreste sau reia curgerea timpului in simulare (la fel ca tasta P).",
		"[ Map (Globe) ] : Toggle between Top-Down Global Map and normal flight (same as M key).\n"
		"[ Eclipse ] : Opens the Xavier Jubier table with exact contact times (C1 - C4) for the current location, including sunrise and sunset\n"
		"[ Clouds ] : Hide or show clouds in the sky (same as N key).\n"
		"[ Play / Pause ] : Stop or resume time flow in the simulation (same as P key)."
	);
	AddSection(
		"TASTE UTILE (SHORTCUTS)",
		"USEFUL HOTKEYS",
		"[ F10 ] : Deschide oricand acest meniu.\n"
		"[ N ]   : Ascunde / Arata norii (Cer senin).\n"
		"[ P ]   : Pauza in simulare.\n"
		"[ C ]   : Reduce viteza simularii la jumatate.\n"
		"[ V ]   : Dubleaza viteza simularii.\n",
		"[ F10 ] : Open this menu anytime.\n"
		"[ N ]   : Toggle white clouds on / off.\n"
		"[ P ]   : Pause the simulation.\n"
		"[ C ]   : Halve the simulation speed.\n"
		"[ V ]   : Double the simulation speed.\n"
	);
	AddSection(
		"CUM SA OBTII O CHEIE CESIUM (HARTA 3D)",
		"HOW TO GET A CESIUM API KEY (3D MAP)",
		"1. Intra pe site-ul oficial: ion.cesium.com\n"
		"2. Creeaza un cont gratuit (Sign Up).\n"
		"3. Din meniul din stanga, acceseaza sectiunea 'Access Tokens'.\n"
		"4. Apasa butonul 'Create Token' sau copiaza Token-ul Default.\n"
		"5. In simulator, mergi la Setari(din meniul derulant de sus) si lipeste cheia in sectiunea 'Cesium API Key'. Apasa Apply.\n"
		"Acest lucru este necesar o singura data pentru a descarca terenul global la rezolutie maxima in mod gratuit!",
		"1. Go to the official website: ion.cesium.com\n"
		"2. Create a free account (Sign Up).\n"
		"3. From the left menu, access the 'Access Tokens' section.\n"
		"4. Click 'Create Token' or copy the Default Token.\n"
		"5. In the simulator, go to Settings(from scrolling menu) and paste the key in the 'Cesium API Key' section. Click Apply.\n"
		"This is required only once to download the global terrain at maximum resolution for free!"
	);
}
void UHelpMenuWidget::BuildDatabase()
{
	HelpDatabase.Empty();
	auto AddEntry = [&](FString RoTitle, FString EnTitle, FString RoDesc, FString EnDesc, FString RoKeys, FString EnKeys, EMenuTarget Target) {
		HelpDatabase.Add({
			AOrbitalManager::TrString(RoTitle, EnTitle),
			AOrbitalManager::TrString(RoDesc, EnDesc),
			AOrbitalManager::TrString(RoKeys, EnKeys),
			Target
			});
		};
	AddEntry(
		"Setari Grafice & FPS", "Graphics Settings & FPS",
		"Ajusteaza rezolutia, limiteaza cadrele pe secunda (FPS), activeaza VSync si modifica calitatea norilor volumetrici.",
		"Adjust resolution, limit frames per second (FPS), enable VSync, and modify volumetric clouds quality.",
		"fps, lag, grafica, calitate, sacadeaza, rezolutie, vsync, performanta, nori", "fps, lag, graphics, quality, stutter, resolution, vsync, performance, clouds",
		EMenuTarget::Settings
	);
	AddEntry(
		"Cheie API Cesium (Teren 3D)", "Cesium API Key (3D Terrain)",
		"Daca harta nu se incarca sau e blurata, introdu propriul Token de pe ion.cesium.com. Gestioneaza calitatea si memoria cache Cesium.",
		"If the map doesn't load or is blurry, enter your own Token from ion.cesium.com. Manage Cesium quality and cache memory.",
		"cesium, cheie, token, api, harta nu merge, blur, eroare teren, net, cache, memorie", "cesium, key, token, api, map not working, blur, terrain error, internet, cache, memory",
		EMenuTarget::Settings
	);
	AddEntry(
		"Personalizare Umbre (Culori)", "Eclipse Shadow Colors",
		"Modifica nuanta si opacitatea (RGBA) pentru Umbra si Penumbra proiectate pe suprafata Pamantului.",
		"Modify the color and opacity (RGBA) for the Umbra and Penumbra projected on the Earth's surface.",
		"culoare, umbra, penumbra, rosu, opacitate, transparenta, design, rgba", "color, shadow, penumbra, red, opacity, transparency, design, rgba",
		EMenuTarget::Settings
	);
	AddEntry(
		"Control Drona, Audio & UI", "Drone Control, Audio & UI",
		"Regleaza sensibilitatea mouse-ului, FOV-ul standard, inversarea axei Y, volumul audio si ascunderea HUD-ului.",
		"Adjust mouse sensitivity, standard FOV, Y-axis inversion, audio volume, and hide the HUD.",
		"sensibilitate, mouse, fov, audio, volum, sunet, invert, ascunde hud, UI", "sensitivity, mouse, fov, audio, volume, sound, invert, hide hud, UI",
		EMenuTarget::Settings
	);
	AddEntry(
		"Limba si Unitati de Masura", "Language and Measurement Units",
		"Comuta limba simulatorului (Romana/Engleza) si sistemul metric (Kilometri) sau imperial (Mile).",
		"Switch simulator language (Romanian/English) and metric (Kilometers) or imperial (Miles) system.",
		"limba, engleza, romana, translate, unitati, km, mile, metric, imperial", "language, english, romanian, translate, units, km, miles, metric, imperial",
		EMenuTarget::Settings
	);
	AddEntry(
		"Control Timp (Viteza & Pauza)", "Time Control (Playback & Speed)",
		"Pune pe pauza simularea astrala sau deruleaza timpul inainte/inapoi multiplicand viteza de curgere.",
		"Pause the astral simulation or fast-forward/rewind time by multiplying the flow speed.",
		"timp, viteza, repede, play, pauza, deruleaza, multiplicator", "time, speed, fast, play, pause, rewind, fast-forward, multiplier",
		EMenuTarget::Time
	);
	AddEntry(
		"Timp UTC vs. Timp Solar", "UTC vs. Solar Time",
		"Seteaza ora exacta. UTC e ora globala standard, in timp ce Timpul Solar este calculat pur in functie de pozitia Soarelui local.",
		"Set the exact time. UTC is standard global time, while Solar Time is calculated purely based on local Sun position.",
		"ora, utc, solar, adevarat, fus orar, sincronizare, ceas, secunde", "time, utc, solar, true, timezone, sync, clock, seconds",
		EMenuTarget::Time
	);
	AddEntry(
		"Calendar Astronomic", "Astronomical Calendar",
		"Navigheaza printr-un calendar vizual si alege o data din trecut sau viitor pentru a actualiza cerul.",
		"Navigate through a visual calendar and pick a past or future date to update the sky.",
		"calendar, zi, luna, an, data, istorie, viitor, selectie", "calendar, day, month, year, date, history, future, selection",
		EMenuTarget::Time
	);
	AddEntry(
		"Vreme (Ceata, Ploaie, Ninsoare)", "Weather (Fog, Rain, Snow)",
		"Simuleaza conditii atmosferice. Trage de slider pentru ceata sau activeaza precipitatiile pentru a testa vizibilitatea.",
		"Simulate atmospheric conditions. Drag the slider for fog or enable precipitation to test visibility.",
		"vreme, weather, ploaie, ninsoare, ceata, fog, precipitatii, nori, atmosfera", "weather, rain, snow, fog, precipitation, clouds, atmosphere",
		EMenuTarget::Weather
	);
	AddEntry(
		"Zoom Optic si Auto-Fit", "Optical Zoom and Auto-Fit",
		"Controleaza precis lentila telescopului. Foloseste Auto-Fit Width/Height pentru a incadra planeta perfect pe ecran.",
		"Precisely control the telescope lens. Use Auto-Fit Width/Height to frame the planet perfectly on screen.",
		"zoom, telescop, mareste, fov, incadreaza, perfect, auto fit, latime, inaltime", "zoom, telescope, magnify, fov, frame, perfect, auto fit, width, height",
		EMenuTarget::Optics
	);
	AddEntry(
		"Urmarire Automata (Auto-Tracking)", "Automatic Tracking (Target)",
		"Selecteaza un astru (ex: Soare, Luna, Marte) dintr-o lista si drona il va urmari cu precizie cat timp se misca pe cer.",
		"Select a body (e.g. Sun, Moon, Mars) from a list and the drone will track it precisely as it moves across the sky.",
		"urmarire, tracking, blocare, tinta, auto, sistem solar, planete, urmareste", "tracking, lock, target, auto, solar system, planets, follow",
		EMenuTarget::Optics
	);
	AddEntry(
		"Umbre Eclipse 3D (Conuri & Penumbra)", "3D Eclipse Shadows (Cones & Penumbra)",
		"Deseneaza conurile fizice de umbra/penumbra in spatiu (Complete sau Trunchiate) si proiectia lor pe sol.",
		"Draw physical shadow/penumbra cones in space (Full or Truncated) and their projection on the ground.",
		"con, umbra, cilindru, trunchiat, spatiu, proiectie, penumbra, sol", "cone, shadow, cylinder, truncated, space, projection, penumbra, ground",
		EMenuTarget::Overlays
	);
	AddEntry(
		"Grile Ceresti (Azimut / Ecuator)", "Celestial Grids (Azimuth / Equator)",
		"Proiecteaza o plasa geometrica pe cer: Altitudine/Azimut pentru orientare locala si Declinatie/Ascensie Dreapta astronomica.",
		"Project a geometric net on the sky: Altitude/Azimuth for local orientation and astronomical Declination/Right Ascension.",
		"grila, linii, azimut, ecuator, coordonate, cer, latitudine, plasa", "grid, lines, azimuth, equator, coordinates, sky, latitude, net",
		EMenuTarget::Overlays
	);
	AddEntry(
		"Constelatii (Nume si Arta)", "Constellations (Names and Art)",
		"Uneste stelele intre ele (Zodiac), afiseaza numele lor stiintifice sau deseneaza arta antica a constelatiilor.",
		"Connect stars (Zodiac), display their scientific names, or draw ancient constellation art.",
		"constelatii, zodiac, linii stele, arta, desene, orion, carul mare", "constellations, zodiac, star lines, art, drawings, orion, big dipper",
		EMenuTarget::Overlays
	);
	AddEntry(
		"Etichete Planete, Luna si Drona", "Planet, Moon and Drone Labels",
		"Evidentiaza textul astrelor pe cer, arata locatiile de pe Luna (ex: Marea Linistii) sau activeaza modelul fizic al dronei.",
		"Highlight celestial text in the sky, show Moon locations (e.g. Sea of Tranquility), or enable the drone physical model.",
		"nume, planete, text, cratere luna, mari lunare, drona 3d, ascunde drona", "names, planets, text, moon craters, lunar maria, 3d drone, hide drone",
		EMenuTarget::Overlays
	);
	AddEntry(
		"Opreste Atmosphera (Cer Negru Ziua)", "Disable Atmosphere (Black Sky Day)",
		"Opreste simularea aerului Pamantului. Acest lucru face cerul negru in plina zi pentru a vedea cum arata stelele si Calea Lactee in jurul Soarelui.",
		"Stop Earth's air simulation. This turns the sky black during the day to see how stars and the Milky Way look around the Sun.",
		"atmosfera, nori, cer senin, fara aer, cer negru ziua, spatiu", "atmosphere, clouds, clear sky, no air, black sky day, space",
		EMenuTarget::Overlays
	);
	AddEntry(
		"Filtru Solar & Expunere", "Solar Filter & Exposure",
		"Actioneaza ca un ochelar de eclipsa. Scade luminozitatea Soarelui pentru a vedea detaliile discului sau creste expunerea noaptea.",
		"Acts as eclipse glasses. Lower Sun brightness to see disk details or increase exposure at night.",
		"soare orbitor, filtru solar, expunere, bloom, intunecare, lentila", "blinding sun, solar filter, exposure, bloom, darkening, lens",
		EMenuTarget::VisualFX
	);
	AddEntry(
		"Efecte Eclipsa (Inel Diamant / Corona)", "Eclipse Effects (Diamond Ring / Corona)",
		"Ajusteaza intensitatea halo-ului solar din timpul totalitatii si flash-ul luminos de la Marginile Lunii.",
		"Adjust the intensity of the solar halo during totality and the bright flash at the Moon's edges.",
		"diamant, inel, diamond ring, corona, totalitate, flash, lumina eclipsa", "diamond, ring, diamond ring, corona, totality, flash, eclipse light",
		EMenuTarget::VisualFX
	);
	AddEntry(
		"Export Video / Timelapse (MP4)", "Video / Timelapse Export (MP4)",
		"Programeaza o ora de start si stop. Simulatorul va inregistra automat un clip video la rezolutia si framerate-ul dorite.",
		"Program a start and stop time. The simulator will automatically record a video clip at the desired resolution and framerate.",
		"video, film, inregistrare, record, timelapse, mp4, framerate, render, animatie", "video, movie, recording, record, timelapse, mp4, framerate, render, animation",
		EMenuTarget::VideoExport
	);
	AddEntry(
		"Screenshot (Poza Rapida)", "Screenshot (Quick Photo)",
		"Salveaza un cadru perfect, fara elemente UI (Meniuri) care sa strice poza.",
		"Saves a perfect frame, without UI elements (Menus) ruining the photo.",
		"poza, imagine, captura de ecran, screenshot, png, fotografie, camera foto", "photo, image, screen capture, screenshot, png, photography, camera",
		EMenuTarget::VideoExport
	);
	AddEntry(
		"Harta 2D si Banda de Totalitate", "2D Map and Totality Band",
		"Vizualizeaza un plan 2D global. Deseneaza banda umbrei pe glob pentru eclipsa curenta si exporta graficul ca imagine PNG.",
		"View a global 2D plane. Draw the shadow band on the globe for the current eclipse and export the chart as a PNG image.",
		"harta 2d, map, png, export imagine, banda totalitate, salvare harta, desen eclipsa", "2d map, png, export image, totality band, save map, eclipse drawing",
		EMenuTarget::Map2D
	);
	AddEntry(
		"Harta Globala de Sus (Tasta M)", "Top-Down Global Map (M Key)",
		"Trage de glob cu mouse-ul si fa click stanga pentru a te teleporta intr-o fractiune de secunda.",
		"Drag the globe with the mouse and left-click to teleport in a fraction of a second.",
		"harta, map, tasta m, vedere de sus, satelit, click pamant, glob", "map, m key, top down view, satellite, click earth, globe",
		EMenuTarget::None
	);
	AddEntry(
		"Alegere Coordonate (Map Picker)", "Map Picker Coordinates",
		"Un meniu dedicat pentru a cauta sau introduce Latitudinea/Longitudinea si pentru a sincroniza timpul locului.",
		"A dedicated menu to search or enter Latitude/Longitude and synchronize the local time.",
		"gps, latitudine, longitudine, altitudine, teleport, manual, picker, harta precisa", "gps, latitude, longitude, altitude, teleport, manual, picker, precise map",
		EMenuTarget::Placement
	);
	AddEntry(
		"Locatii Salvate (Bookmarks)", "Saved Locations (Bookmarks)",
		"Salveaza un cadru exact (Timp, Locatie GPS, Tinta Camerei si FOV Optic) ca sa te poti reintoarce apasand un singur buton.",
		"Save an exact frame (Time, GPS Location, Camera Target, and Optical FOV) so you can return by pressing a single button.",
		"salveaza, locatie, favorit, marcaj, gps, teleportare, bookmark, amintire", "save, location, favorite, bookmark, gps, teleport, memory",
		EMenuTarget::Bookmarks
	);
	AddEntry(
		"Perspective Zbor (Spatiu / LEO / GEO)", "Flight Perspectives (Space / LEO / GEO)",
		"Iesi din drona First-Person si treci la vizualizari din satelit: Orbita Terestra Joasa (LEO), Orbita Geostationara (GEO) sau Cinematic.",
		"Exit the First-Person drone and switch to satellite views: Low Earth Orbit (LEO), Geostationary Orbit (GEO), or Cinematic.",
		"spatiu, leo, geo, orbita, deasupra pamantului, cinematografic, satelit, fpv, tpv, persoana a treia", "space, leo, geo, orbit, above earth, cinematic, satellite, fpv, tpv, third person",
		EMenuTarget::Cameras
	);
	AddEntry(
		"Zbor pe suprafata Lunii (Tasta L)", "Flight on Moon Surface (L Key)",
		"Te ataseaza fizic pe suprafata Lunii. De aici poti privi spre Pamant si poti observa cum o Eclipsa de Soare arata ca un punct negru pe suprafata Terrei.",
		"Physically attaches you to the Moon's surface. From here you can look at Earth and observe how a Solar Eclipse looks like a black dot on Earth.",
		"luna, moon, apollo, aselenizare, calatoreste pe luna, tasta l, spatiu", "moon, apollo, moon landing, travel to moon, l key, space",
		EMenuTarget::None
	);
	AddEntry(
		"Telemetrie Live si Distante (AU/Km)", "Live Telemetry and Distances (AU/Km)",
		"Arata calculele din spatele motorului: Distanta in km sau Unitati Astronomice (AU), Marimea Angulara pe cer, Altitudinea si Azimutul.",
		"Shows engine backend calculations: Distance in km or Astronomical Units (AU), Angular Size in the sky, Altitude, and Azimuth.",
		"distanta, km, unitati astronomice, lumina, altitudine, azimut, date, calcule, telemetrie", "distance, km, astronomical units, light, altitude, azimuth, data, calculations, telemetry",
		EMenuTarget::Telemetry
	);
	AddEntry(
		"Iluminare si Elongatie", "Illumination and Elongation",
		"Citeste procentul fazei corpului ceresc (Iluminarea) si distanta sa unghiulara fata de Soare (Elongatia) in meniul Telemetrie.",
		"Read the phase percentage of the celestial body (Illumination) and its angular distance from the Sun (Elongation) in the Telemetry menu.",
		"iluminare, faza, elongatie, unghi soare, matematica, procente", "illumination, phase, elongation, sun angle, math, percentages",
		EMenuTarget::Telemetry
	);
	AddEntry(
		"Catalogul de Eclipse NASA", "NASA Eclipse Catalog",
		"Baza de date suprema. Rasfoieste mii de eclipse trecute si viitoare. Filtreaza dupa Seria Saros sau intervalul de Ani si calatoreste instant la inceputul fenomenului.",
		"The ultimate database. Browse thousands of past and future eclipses. Filter by Saros Series or Year range and travel instantly to the event's start.",
		"catalog, baza de date, nasa, cauta eclipsa, saros, istorie, secole, an, filtru", "catalog, database, nasa, search eclipse, saros, history, centuries, year, filter",
		EMenuTarget::Browser
	);
	AddEntry(
		"Credite, Echipa si Licente API", "Credits, Team and API Licenses",
		"Afla cine a lucrat la proiect (Dezvoltatori, Profesori Coordonatori) si ce surse de date stiintifice (JPL Horizons) guverneaza fizica.",
		"Find out who worked on the project (Developers, Coordinating Professors) and what scientific data sources (JPL Horizons) govern the physics.",
		"credite, licenta, coordonator, echipa, cine a facut, nasa, jpl, autori, facultate", "credits, license, coordinator, team, who made this, nasa, jpl, authors, university",
		EMenuTarget::Credits
	);
	AddEntry(
		"Iesire Aplicatie (Exit)", "Exit Application",
		"Deschide meniul de confirmare pentru a inchide simulatorul in siguranta si a reveni la Desktop.",
		"Opens the confirmation menu to safely close the simulator and return to the Desktop.",
		"iesire, exit, inchide, quit, desktop, afara, inchidere joc", "exit, quit, close, desktop, out, close game",
		EMenuTarget::ExitApp
	);
}
void UHelpMenuWidget::OnSearchTextChanged(const FText& Text)
{
	FString Query = Text.ToString();
	UpdateResults(Query);
}
void UHelpMenuWidget::UpdateResults(FString Query)
{
	if (!ResultsScrollBox) return;
	ResultsScrollBox->ClearChildren();
	Query = Query.ToLower();
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	FSlateFontInfo DescFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	for (const FHelpEntry& Entry : HelpDatabase)
	{
		bool bIsMatch = false;
		if (Query.IsEmpty())
		{
			bIsMatch = true;
		}
		else
		{
			FString LTitle = Entry.Title.ToLower();
			FString LDesc = Entry.Description.ToLower();
			FString LKeys = Entry.Keywords.ToLower();
			if (LTitle.Contains(Query) || LDesc.Contains(Query) || LKeys.Contains(Query))
			{
				bIsMatch = true;
			}
		}
		if (bIsMatch)
		{
			UBorder* RowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			RowBg->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.15f, 1.0f));
			ResultsScrollBox->AddChild(RowBg);
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			RowBg->SetContent(RowBox);
			RowBg->SetPadding(FMargin(10.f));
			UVerticalBox* TextBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			RowBox->AddChildToHorizontalBox(TextBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			UTextBlock* TxtTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			TxtTitle->SetText(FText::FromString(Entry.Title));
			TxtTitle->SetFont(TitleFont);
			TextBox->AddChildToVerticalBox(TxtTitle);
			UTextBlock* TxtDetails = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			TxtDetails->SetText(FText::FromString(Entry.Description));
			TxtDetails->SetFont(DescFont);
			TxtDetails->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
			TxtDetails->SetAutoWrapText(true);
			TextBox->AddChildToVerticalBox(TxtDetails)->SetPadding(FMargin(0, 3.f, 0, 0));
			UHelpResultBtn* ActionBtn = WidgetTree->ConstructWidget<UHelpResultBtn>(UHelpResultBtn::StaticClass());
			ActionBtn->TargetMenuToOpen = Entry.TargetMenu;
			ActionBtn->ParentMenu = this;
			ActionBtn->SetBackgroundColor(FLinearColor(0.2f, 0.5f, 0.8f));
			UTextBlock* BtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			BtnTxt->SetText(AOrbitalManager::Tr(" MERGI ACOLO ", " GO THERE "));
			BtnTxt->SetFont(TitleFont);
			ActionBtn->AddChild(BtnTxt);
			ActionBtn->OnClicked.AddDynamic(ActionBtn, &UHelpResultBtn::HandleClick);
			RowBox->AddChildToHorizontalBox(ActionBtn)->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
			USpacer* RowSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			RowSpacer->SetSize(FVector2D(1.0f, 5.0f));
			ResultsScrollBox->AddChild(RowSpacer);
		}
	}
	if (ResultsScrollBox->GetChildrenCount() == 0)
	{
		UTextBlock* TxtNoRes = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TxtNoRes->SetText(AOrbitalManager::Tr("Nu s-au gasit rezultate. Incearca alte cuvinte!", "No results found. Try other keywords!"));
		TxtNoRes->SetFont(DescFont);
		TxtNoRes->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
		ResultsScrollBox->AddChild(TxtNoRes);
	}
}
void UHelpMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
}
void UHelpMenuWidget::OpenTargetMenu(EMenuTarget Target)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;
	OnCloseClicked();
	if (Target == EMenuTarget::None) return;
	switch (Target)
	{
	case EMenuTarget::Settings:
	{
		USettingsMenuWidget* Menu = CreateWidget<USettingsMenuWidget>(PC, USettingsMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Optics:
	{
		UOpticsMenuWidget* Menu = CreateWidget<UOpticsMenuWidget>(PC, UOpticsMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Time:
	{
		UTimeControlWidget* Menu = CreateWidget<UTimeControlWidget>(PC, UTimeControlWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::VisualFX:
	{
		UVisualEffectsMenuWidget* Menu = CreateWidget<UVisualEffectsMenuWidget>(PC, UVisualEffectsMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Bookmarks:
	{
		UBookmarksMenuWidget* Menu = CreateWidget<UBookmarksMenuWidget>(PC, UBookmarksMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Overlays:
	{
		UOverlayMenuWidget* Menu = CreateWidget<UOverlayMenuWidget>(PC, UOverlayMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Browser:
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UMainMenuWidget::StaticClass(), true);
		if (FoundWidgets.Num() > 0)
		{
			UMainMenuWidget* MainMenu = Cast<UMainMenuWidget>(FoundWidgets[0]);
			if (MainMenu)
			{
				MainMenu->OpenBrowser();
			}
		}
		break;
	}
	case EMenuTarget::Placement:
	{
		UPlacementMenuWidget* Menu = CreateWidget<UPlacementMenuWidget>(PC, UPlacementMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::VideoExport:
	{
		UVideoExportMenuWidget* Menu = CreateWidget<UVideoExportMenuWidget>(PC, UVideoExportMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Cameras:
	{
		UCameraMenuWidget* Menu = CreateWidget<UCameraMenuWidget>(PC, UCameraMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Telemetry:
	{
		UAstroTelemetryWidget* Menu = CreateWidget<UAstroTelemetryWidget>(PC, UAstroTelemetryWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Weather:
	{
		UWeatherMenuWidget* Menu = CreateWidget<UWeatherMenuWidget>(PC, UWeatherMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::Map2D:
	{
		UEclipseMapExportWidget* Menu = CreateWidget<UEclipseMapExportWidget>(PC, UEclipseMapExportWidget::StaticClass());
		if (Menu) Menu->AddToViewport(100);
		break;
	}
	case EMenuTarget::ExitApp:
	{
		UExitMenuWidget* Menu = CreateWidget<UExitMenuWidget>(PC, UExitMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(9999);
		break;
	}
	case EMenuTarget::Credits:
		UCreditsMenuWidget* Menu = CreateWidget<UCreditsMenuWidget>(PC, UCreditsMenuWidget::StaticClass());
		if (Menu) Menu->AddToViewport(9999);
		break;
	}
}
void UMainMenuWidget::OpenWeatherMenu()
{
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UWeatherMenuWidget* WeatherMenu = CreateWidget<UWeatherMenuWidget>(PC, UWeatherMenuWidget::StaticClass());
		if (WeatherMenu)
		{
			WeatherMenu->AddToViewport(100);
		}
	}
	ToggleMenu();
}
TSharedRef<SWidget> UWeatherMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.04f, 0.98f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(600.0f, 450.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	BgBorder->SetPadding(FMargin(0.f));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.12f, 1.0f));
	MainLayout->AddChildToVerticalBox(HeaderBg);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	HeaderBg->SetPadding(FMargin(20.f, 15.f));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
	FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("CONDITII ATMOSFERICE"), TEXT("ATMOSPHERIC CONDITIONS")));
	Title->SetFont(TitleFont);
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(TitleFont);
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UWeatherMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	UVerticalBox* SlidersContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBoxSlot* SlidersSlot = MainLayout->AddChildToVerticalBox(SlidersContainer);
	SlidersSlot->SetPadding(FMargin(30.f, 30.f, 30.f, 20.f));
	SlidersSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* CloudAltLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloudAltLabel->SetText(AOrbitalManager::Tr(TEXT("Altitudine Plafon Nori (km)"), TEXT("Cloud Altitude (km)")));
	CloudAltLabel->SetFont(RegularFont);
	SlidersContainer->AddChildToVerticalBox(CloudAltLabel)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	UHorizontalBox* CloudAltRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SlidersContainer->AddChildToVerticalBox(CloudAltRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	CloudAltSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	CloudAltSlider->SetMinValue(0.0f);
	CloudAltSlider->SetMaxValue(1.0f);
	CloudAltSlider->OnValueChanged.AddDynamic(this, &UWeatherMenuWidget::OnCloudAltitudeChanged);
	CloudAltRow->AddChildToHorizontalBox(CloudAltSlider)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloudAltValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloudAltValueText->SetText(FText::FromString("5.0"));
	CloudAltValueText->SetFont(RegularFont);
	CloudAltRow->AddChildToHorizontalBox(CloudAltValueText)->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
	UTextBlock* CloudLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloudLabel->SetText(AOrbitalManager::Tr(TEXT("Densitate Nori (Acoperire)"), TEXT("Cloud Coverage")));
	CloudLabel->SetFont(RegularFont);
	SlidersContainer->AddChildToVerticalBox(CloudLabel)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	UHorizontalBox* CloudRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SlidersContainer->AddChildToVerticalBox(CloudRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	CloudSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	CloudSlider->SetMinValue(0.0f);
	CloudSlider->SetMaxValue(1.0f);
	CloudSlider->OnValueChanged.AddDynamic(this, &UWeatherMenuWidget::OnCloudChanged);
	CloudRow->AddChildToHorizontalBox(CloudSlider)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloudValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloudValueText->SetText(FText::FromString("0.00"));
	CloudValueText->SetFont(RegularFont);
	CloudRow->AddChildToHorizontalBox(CloudValueText)->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
	UTextBlock* FogLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FogLabel->SetText(AOrbitalManager::Tr(TEXT("Densitate Ceata (Fog)"), TEXT("Fog Density")));
	FogLabel->SetFont(RegularFont);
	SlidersContainer->AddChildToVerticalBox(FogLabel)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	UHorizontalBox* FogRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SlidersContainer->AddChildToVerticalBox(FogRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	FogSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
	FogSlider->SetMinValue(0.0f);
	FogSlider->SetMaxValue(1.0f);
	FogSlider->OnValueChanged.AddDynamic(this, &UWeatherMenuWidget::OnFogChanged);
	FogRow->AddChildToHorizontalBox(FogSlider)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FogValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FogValueText->SetText(FText::FromString("0.00"));
	FogValueText->SetFont(RegularFont);
	FogRow->AddChildToHorizontalBox(FogValueText)->SetPadding(FMargin(15.f, 0.f, 0.f, 0.f));
	UHorizontalBox* RainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SlidersContainer->AddChildToVerticalBox(RainRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
	RainCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	RainCheckBox->OnCheckStateChanged.AddDynamic(this, &UWeatherMenuWidget::OnRainChanged);
	RainRow->AddChildToHorizontalBox(RainCheckBox)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* RainLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	RainLabel->SetText(AOrbitalManager::Tr(TEXT("Activeaza Ploaia"), TEXT("Enable Rain")));
	RainLabel->SetFont(RegularFont);
	RainRow->AddChildToHorizontalBox(RainLabel)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	UHorizontalBox* SnowRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SlidersContainer->AddChildToVerticalBox(SnowRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));
	SnowCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	SnowCheckBox->OnCheckStateChanged.AddDynamic(this, &UWeatherMenuWidget::OnSnowChanged);
	SnowRow->AddChildToHorizontalBox(SnowCheckBox)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* SnowLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SnowLabel->SetText(AOrbitalManager::Tr(TEXT("Activeaza Ninsoarea"), TEXT("Enable Snow")));
	SnowLabel->SetFont(RegularFont);
	SnowRow->AddChildToHorizontalBox(SnowLabel)->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ResetButton->SetBackgroundColor(FLinearColor(0.8f, 0.2f, 0.2f, 1.0f));
	ResetButton->OnClicked.AddDynamic(this, &UWeatherMenuWidget::OnResetClicked);
	UTextBlock* ResetTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ResetTxt->SetText(AOrbitalManager::Tr(TEXT("RESETEAZA VREMEA"), TEXT("RESET WEATHER")));
	ResetTxt->SetFont(TitleFont);
	ResetButton->AddChild(ResetTxt);
	SlidersContainer->AddChildToVerticalBox(ResetButton)->SetHorizontalAlignment(HAlign_Center);
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		if (FogSlider) FogSlider->SetValue(MyDrone->SavedFogValue);
		if (FogValueText) FogValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(MyDrone->SavedFogValue * 100.f) / 100.f));
		if (RainCheckBox) RainCheckBox->SetIsChecked(MyDrone->bIsRainActive);
		if (SnowCheckBox) SnowCheckBox->SetIsChecked(MyDrone->bIsSnowActive);
		if (CloudSlider) CloudSlider->SetValue(MyDrone->SavedCloudSliderValue);
		float MappedCloud = FMath::Lerp(-0.5f, 0.5f, MyDrone->SavedCloudSliderValue);
		if (CloudValueText) CloudValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(MappedCloud * 100.f) / 100.f));
		if (CloudAltSlider) CloudAltSlider->SetValue(MyDrone->SavedCloudAltSliderValue);
		float MappedAlt = FMath::Lerp(0.1f, 15.0f, MyDrone->SavedCloudAltSliderValue);
		if (CloudAltValueText) CloudAltValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(MappedAlt * 10.f) / 10.f));
	}
	return Super::RebuildWidget();
}
void UWeatherMenuWidget::OnCloudChanged(float Value)
{
	float MappedDensity = FMath::Lerp(-0.5f, 0.5f, Value);
	if (CloudValueText)
	{
		CloudValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(MappedDensity * 100.f) / 100.f));
	}
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		MyDrone->SavedCloudSliderValue = Value;
		MyDrone->CurrentCloudDensity = MappedDensity;
		if (GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* CurrentActor = *It;
				UVolumetricCloudComponent* CloudComp = CurrentActor->GetComponentByClass<UVolumetricCloudComponent>();
				if (CloudComp)
				{
					UMaterialInterface* CurrentMaterial = CloudComp->GetMaterial();
					if (CurrentMaterial)
					{
						UMaterialInstanceDynamic* DynamicMat = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
						if (!DynamicMat)
						{
							DynamicMat = UMaterialInstanceDynamic::Create(CurrentMaterial, this);
							CloudComp->SetMaterial(DynamicMat);
						}
						if (DynamicMat)
						{
							DynamicMat->SetScalarParameterValue(FName("Cloud_GlobalCoverage"), MappedDensity);
						}
					}
					break;
				}
			}
		}
	}
}
void UWeatherMenuWidget::OnCloudAltitudeChanged(float Value)
{
	float MappedAltitude = FMath::Lerp(0.1f, 15.0f, Value);
	if (CloudAltValueText)
	{
		CloudAltValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(MappedAltitude * 10.f) / 10.f));
	}
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		MyDrone->SavedCloudAltSliderValue = Value;
		if (GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* CurrentActor = *It;
				UVolumetricCloudComponent* CloudComp = CurrentActor->GetComponentByClass<UVolumetricCloudComponent>();
				if (CloudComp)
				{
					CloudComp->LayerBottomAltitude = MappedAltitude;
					CloudComp->MarkRenderStateDirty();
					break;
				}
			}
		}
	}
}
void UWeatherMenuWidget::OnFogChanged(float Value)
{
	if (FogValueText)
	{
		FogValueText->SetText(FText::AsNumber(FMath::RoundHalfToZero(Value * 100.f) / 100.f));
	}
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		MyDrone->SavedFogValue = Value;
		MyDrone->CurrentFogIntensity = Value * 0.05f;
	}
}
void UWeatherMenuWidget::OnRainChanged(bool bIsChecked)
{
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		MyDrone->bIsRainActive = bIsChecked;
		if (MyDrone->RainComponent)
		{
			if (bIsChecked)
			{
				MyDrone->RainComponent->SetVisibility(true);
				MyDrone->RainComponent->Activate(true);
				MyDrone->RainComponent->SetFloatParameter(FName("User.Intensity"), 1.0f);
			}
			else
			{
				MyDrone->RainComponent->Deactivate();
				MyDrone->RainComponent->SetVisibility(false);
			}
		}
	}
}
void UWeatherMenuWidget::OnSnowChanged(bool bIsChecked)
{
	ADronaMea* MyDrone = Cast<ADronaMea>(GetOwningPlayerPawn());
	if (MyDrone)
	{
		MyDrone->bIsSnowActive = bIsChecked;
		if (MyDrone->SnowComponent)
		{
			if (bIsChecked)
			{
				MyDrone->SnowComponent->SetVisibility(true);
				MyDrone->SnowComponent->Activate(true);
				MyDrone->SnowComponent->SetFloatParameter(FName("User.Intensity"), 1.0f);
			}
			else
			{
				MyDrone->SnowComponent->Deactivate();
				MyDrone->SnowComponent->SetVisibility(false);
			}
		}
	}
}
void UWeatherMenuWidget::OnResetClicked()
{
	if (FogSlider) FogSlider->SetValue(0.0f);
	OnFogChanged(0.0f);
	if (RainCheckBox) RainCheckBox->SetIsChecked(false);
	OnRainChanged(false);
	if (SnowCheckBox) SnowCheckBox->SetIsChecked(false);
	OnSnowChanged(false);
	if (CloudSlider) CloudSlider->SetValue(0.3f);
	OnCloudChanged(0.3f);
	if (CloudAltSlider) CloudAltSlider->SetValue(0.3f);
	OnCloudAltitudeChanged(0.3f);
}
void UWeatherMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
}
UEclipseMapExportWidget::UEclipseMapExportWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsVolatile = true;
	static ConstructorHelpers::FObjectFinder<UTexture2D> MapTexObj(TEXT("/Game/Textures/Pamant2D.Pamant2D"));
	if (MapTexObj.Succeeded())
	{
		WorldMapTexture = MapTexObj.Object;
	}
}
TSharedRef<SWidget> UEclipseMapExportWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.98f));
	BgBorder->SetPadding(FMargin(0.f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(1100.0f, 687.5f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.2f, 1.0f));
	HeaderBg->SetPadding(FMargin(15.f, 10.f));
	MainLayout->AddChildToVerticalBox(HeaderBg);
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("HARTA TRASEU ECLIPSA (2D)"), TEXT("ECLIPSE PATH MAP (2D)")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	HeaderBox->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	XTxt->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UEclipseMapExportWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBoxSlot* ContentSlot = MainLayout->AddChildToVerticalBox(ContentBox);
	ContentSlot->SetPadding(FMargin(20.f));
	ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UBorder* PreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PreviewBorder->SetBrushColor(FLinearColor::Black);
	PreviewBorder->SetPadding(FMargin(0.0f));
	UVerticalBoxSlot* PreviewSlot = ContentBox->AddChildToVerticalBox(PreviewBorder);
	PreviewSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UScaleBox* ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	ScaleBox->SetStretch(EStretch::ScaleToFit);
	PreviewBorder->SetContent(ScaleBox);
	MapPreviewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ScaleBox->AddChild(MapPreviewImage);
	if (WorldMapTexture)
	{
		MapPreviewImage->SetBrushFromTexture(WorldMapTexture);
		FSlateBrush NewBrush = MapPreviewImage->GetBrush();
		NewBrush.ImageSize = FVector2D(2048.0f, 1024.0f);
		MapPreviewImage->SetBrush(NewBrush);
	}
	UHorizontalBox* BottomControlsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentBox->AddChildToVerticalBox(BottomControlsRow)->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
	TotalityCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	TotalityCheckBox->OnCheckStateChanged.AddDynamic(this, &UEclipseMapExportWidget::OnTotalityCheckboxChanged);
	UHorizontalBoxSlot* CheckSlot = BottomControlsRow->AddChildToHorizontalBox(TotalityCheckBox);
	CheckSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock* CheckBoxText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CheckBoxText->SetText(AOrbitalManager::Tr(TEXT(" Afiseaza Banda de Totalitate"), TEXT(" Show Totality Path")));
	CheckBoxText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
	UHorizontalBoxSlot* CheckTxtSlot = BottomControlsRow->AddChildToHorizontalBox(CheckBoxText);
	CheckTxtSlot->SetVerticalAlignment(VAlign_Center);
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		TotalityCheckBox->SetIsChecked(Manager->bShowTotalityZone);
	}
	USpacer* MidSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	BottomControlsRow->AddChildToHorizontalBox(MidSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ExportImageButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ExportImageButton->SetBackgroundColor(FLinearColor(0.1f, 0.6f, 0.2f));
	UTextBlock* TxtExport = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtExport->SetText(AOrbitalManager::Tr(TEXT("Salveaza Harta in PC (.PNG)"), TEXT("Save Map to PC (.PNG)")));
	TxtExport->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	TxtExport->SetMargin(FMargin(20.f, 10.f));
	TxtExport->SetJustification(ETextJustify::Center);
	ExportImageButton->AddChild(TxtExport);
	ExportImageButton->OnClicked.AddDynamic(this, &UEclipseMapExportWidget::OnExportImageClicked);
	UHorizontalBoxSlot* BtnSlot = BottomControlsRow->AddChildToHorizontalBox(ExportImageButton);
	BtnSlot->SetVerticalAlignment(VAlign_Center);
	return Super::RebuildWidget();
}
void UEclipseMapExportWidget::OnCloseClicked() { this->RemoveFromParent(); }
void UEclipseMapExportWidget::OnToggleLinesClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn()))
		{
			UE_LOG(LogTemp, Warning, TEXT("WIDGET: Am schimbat bShowEclipsePath in: %s"),
				MyDrone->bShowEclipsePath ? TEXT("TRUE") : TEXT("FALSE"));
			MyDrone->bShowEclipsePath = !MyDrone->bShowEclipsePath;
		}
	}
}
void UEclipseMapExportWidget::OnExportImageClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager || Manager->SavedEclipsePath.Num() < 2) return;
	FString FinalSavePath = TEXT("");
#if PLATFORM_WINDOWS
	OPENFILENAME ofn;
	TCHAR szFile[512] = { 0 };
	FCString::Strcpy(szFile, TEXT("HartaEclipsa.png"));
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWindow().IsValid())
	{
		ofn.hwndOwner = (HWND)GEngine->GameViewport->GetWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("Imagini PNG (*.png)\0*.png\0Toate Fisierele (*.*)\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT("png");
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileName(&ofn))
	{
		FinalSavePath = FString(ofn.lpstrFile);
	}
#endif
	if (FinalSavePath.IsEmpty())
	{
		return;
	}
	if (!FinalSavePath.EndsWith(".png"))
	{
		FinalSavePath += ".png";
	}
	UTextureRenderTarget2D* ExportRT = NewObject<UTextureRenderTarget2D>(this);
	ExportRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
	ExportRT->SRGB = true;
	ExportRT->InitAutoFormat(2048, 1024);
	ExportRT->UpdateResourceImmediate(true);
	FLinearColor OceanColor = FLinearColor(0.1f, 0.4f, 1.0f, 1.0f);
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), ExportRT, OceanColor);
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), ExportRT, Canvas, Size, Context);
	if (Canvas)
	{
		if (WorldMapTexture)
		{
			Canvas->K2_DrawTexture(
				WorldMapTexture,
				FVector2D::ZeroVector,
				Size,
				FVector2D::ZeroVector,
				FVector2D(1.f, 1.f),
				FLinearColor::White,
				EBlendMode::BLEND_Translucent
			);
		}
		else
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, AOrbitalManager::TrString(TEXT("Eroare Critica: WorldMapTexture nu a fost gasita in constructor!"), TEXT("Critical Error: WorldMapTexture not found in constructor!")));
		}
		const double EarthCircumferenceKm = 40075.0;
		FLinearColor CuloareBanda = FLinearColor(0.0f, 0.0f, 0.0f, 0.7f);
		FLinearColor CuloareLinieSubtire = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		for (int32 i = 0; i < Manager->SavedEclipsePath.Num() - 1; i++)
		{
			double Lon1 = Manager->SavedEclipsePath[i].X;
			double Lat1 = Manager->SavedEclipsePath[i].Y;
			double WidthKm1 = Manager->SavedEclipsePath[i].Z;
			double Lon2 = Manager->SavedEclipsePath[i + 1].X;
			double Lat2 = Manager->SavedEclipsePath[i + 1].Y;
			double WidthKm2 = Manager->SavedEclipsePath[i + 1].Z;
			float X1 = ((Lon1 + 180.0) / 360.0) * Size.X;
			float Y1 = ((90.0 - Lat1) / 180.0) * Size.Y;
			float X2 = ((Lon2 + 180.0) / 360.0) * Size.X;
			float Y2 = ((90.0 - Lat2) / 180.0) * Size.Y;
			if (FMath::Abs(Lon1 - Lon2) > 180.0)
			{
				float X2_Shifted = (Lon1 > 0) ? X2 + Size.X : X2 - Size.X;
				float X1_Shifted = (Lon1 > 0) ? X1 - Size.X : X1 + Size.X;
				FCanvasLineItem Line1(FVector2D(X1, Y1), FVector2D(X2_Shifted, Y2));
				Line1.SetColor(CuloareBanda);
				Line1.LineThickness = (WidthKm1 / EarthCircumferenceKm) * Size.X;
				Line1.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Line1);
				FCanvasLineItem Line2(FVector2D(X1_Shifted, Y1), FVector2D(X2, Y2));
				Line2.SetColor(CuloareBanda);
				Line2.LineThickness = (WidthKm2 / EarthCircumferenceKm) * Size.X;
				Line2.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Line2);
			}
			else
			{
				if (Manager->bShowTotalityZone)
				{
					FVector2D P1(X1, Y1);
					FVector2D P2(X2, Y2);
					FVector2D Forward = (P2 - P1).GetSafeNormal();
					FVector2D Right = FVector2D(-Forward.Y, Forward.X);
					float HalfThick1 = ((WidthKm1 / EarthCircumferenceKm) * Size.X) / 2.0f;
					float HalfThick2 = ((WidthKm2 / EarthCircumferenceKm) * Size.X) / 2.0f;
					FVector2D TopLeft = P1 - Right * HalfThick1;
					FVector2D BottomLeft = P1 + Right * HalfThick1;
					FVector2D TopRight = P2 - Right * HalfThick2;
					FVector2D BottomRight = P2 + Right * HalfThick2;
					TArray<FCanvasUVTri> Triangles;
					FCanvasUVTri Tri1;
					Tri1.V0_Pos = TopLeft;    Tri1.V0_UV = FVector2D::ZeroVector; Tri1.V0_Color = CuloareBanda;
					Tri1.V1_Pos = BottomLeft; Tri1.V1_UV = FVector2D::ZeroVector; Tri1.V1_Color = CuloareBanda;
					Tri1.V2_Pos = TopRight;   Tri1.V2_UV = FVector2D::ZeroVector; Tri1.V2_Color = CuloareBanda;
					Triangles.Add(Tri1);
					FCanvasUVTri Tri2;
					Tri2.V0_Pos = TopRight;   Tri2.V0_UV = FVector2D::ZeroVector; Tri2.V0_Color = CuloareBanda;
					Tri2.V1_Pos = BottomLeft; Tri2.V1_UV = FVector2D::ZeroVector; Tri2.V1_Color = CuloareBanda;
					Tri2.V2_Pos = BottomRight; Tri2.V2_UV = FVector2D::ZeroVector; Tri2.V2_Color = CuloareBanda;
					Triangles.Add(Tri2);
					FCanvasTriangleItem TriItem(Triangles, GWhiteTexture);
					TriItem.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(TriItem);
				}
				else
				{
					FCanvasLineItem LineItem(FVector2D(X1, Y1), FVector2D(X2, Y2));
					LineItem.SetColor(CuloareLinieSubtire);
					LineItem.LineThickness = 2.0f;
					LineItem.BlendMode = SE_BLEND_Translucent;
					Canvas->DrawItem(LineItem);
				}
			}
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(GetWorld(), Context);
	FlushRenderingCommands();
	FString SaveDirectory = FPaths::GetPath(FinalSavePath);
	FString FileName = FPaths::GetCleanFilename(FinalSavePath);
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), ExportRT, SaveDirectory, FileName);
	if (GEngine)
	{
		FString SuccessStr = AOrbitalManager::TrString(
			FString::Printf(TEXT("Harta a fost salvata in: %s"), *FinalSavePath),
			FString::Printf(TEXT("Map successfully saved to: %s"), *FinalSavePath)
		);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, SuccessStr);
	}
}
void UMainMenuWidget::OpenMapMenu()
{
	if (MenuDropdownContainer)
	{
		MenuDropdownContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
	UEclipseMapExportWidget* MapWidget = CreateWidget<UEclipseMapExportWidget>(GetOwningPlayer(), UEclipseMapExportWidget::StaticClass());
	if (MapWidget)
	{
		MapWidget->AddToViewport(9999);
		MapWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Eroare: Nu am putut crea widget-ul Harta!"));
	}
}
void UEclipseMapExportWidget::OnTotalityCheckboxChanged(bool bIsChecked)
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		Manager->bShowTotalityZone = bIsChecked;
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ADronaMea* MyDrone = Cast<ADronaMea>(PC->GetPawn()))
		{
			if (MyDrone->MapCaptureComponent)
			{
				MyDrone->MapCaptureComponent->CaptureScene();
			}
		}
	}
}
int32 UEclipseMapExportWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (GEngine) GEngine->AddOnScreenDebugMessage(999, 0.1f, FColor::Cyan, TEXT("DEBUG UI: NativePaint se executa!"));
	if (!GetWorld()) return LayerId;
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(1000, 0.1f, FColor::Red, TEXT("DEBUG UI: EROARE - Nu gasesc AOrbitalManager in lume!"));
		return LayerId;
	}
	if (Manager->SavedEclipsePath.Num() <= 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(1001, 0.1f, FColor::Red, TEXT("DEBUG UI: EROARE - Traseul are 0 puncte! (Problema e in Manager, nu a calculat traseul!)"));
		return LayerId;
	}
	if (!MapPreviewImage) return LayerId;
	FGeometry MapGeo = MapPreviewImage->GetCachedGeometry();
	FVector2D MapSize = MapGeo.GetLocalSize();
	if (MapSize.X <= 0 || MapSize.Y <= 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(1002, 0.1f, FColor::Orange, TEXT("DEBUG UI: Astept incarcarea hartii (Dimensiune 0)..."));
		return LayerId;
	}
	if (GEngine) GEngine->AddOnScreenDebugMessage(1003, 0.1f, FColor::Green, TEXT("DEBUG UI: Totul e perfect! Desenez linia..."));
	TArray<FVector2D> PathPoints;
	double TotalWidthKm = 0.0;
	for (int32 i = 0; i < Manager->SavedEclipsePath.Num(); i++)
	{
		double Lon = Manager->SavedEclipsePath[i].X;
		double Lat = Manager->SavedEclipsePath[i].Y;
		TotalWidthKm += Manager->SavedEclipsePath[i].Z;
		float X = ((Lon + 180.0) / 360.0) * MapSize.X;
		float Y = ((90.0 - Lat) / 180.0) * MapSize.Y;
		FVector2D ScreenPos = MapGeo.LocalToAbsolute(FVector2D(X, Y));
		FVector2D LocalWidgetPos = AllottedGeometry.AbsoluteToLocal(ScreenPos);
		PathPoints.Add(LocalWidgetPos);
	}
	double AverageWidthKm = TotalWidthKm / Manager->SavedEclipsePath.Num();
	const double EarthCircumferenceKm = 40075.0;
	float CalculatedThickness = (AverageWidthKm / EarthCircumferenceKm) * MapSize.X;
	if (CalculatedThickness < 3.0f) { CalculatedThickness = 3.0f; }
	float GrosimeLinie = Manager->bShowTotalityZone ? CalculatedThickness : 2.0f;
	FLinearColor CuloareLinie = Manager->bShowTotalityZone ? FLinearColor(0.0f, 0.0f, 0.0f, 0.7f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
	if (PathPoints.Num() > 1)
	{
		TArray<FVector2D> CurrentSegment;
		CurrentSegment.Add(PathPoints[0]);
		for (int32 i = 1; i < PathPoints.Num(); i++)
		{
			if (FMath::Abs(PathPoints[i].X - PathPoints[i - 1].X) > (MapSize.X * 0.5f))
			{
				if (CurrentSegment.Num() >= 2)
				{
					FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), CurrentSegment, ESlateDrawEffect::None, CuloareLinie, true, GrosimeLinie);
				}
				CurrentSegment.Empty();
			}
			CurrentSegment.Add(PathPoints[i]);
		}
		if (CurrentSegment.Num() >= 2)
		{
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), CurrentSegment, ESlateDrawEffect::None, CuloareLinie, true, GrosimeLinie);
		}
	}
	return LayerId + 1;
}
UMapPickerWidget::UMapPickerWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	MapZoom = 1.0f;
	MapPan = FVector2D(0.f, 0.f);
	bIsPanning = false;
	bHasMarker = false;
	bIsUpdatingTime = false;
	static ConstructorHelpers::FObjectFinder<UTexture2D> MapTexObj(TEXT("/Game/Textures/Pamant2D.Pamant2D"));
	if (MapTexObj.Succeeded())
	{
		WorldMapTexture = MapTexObj.Object;
	}
}
TSharedRef<SWidget> UMapPickerWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UOverlay* MainOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UCanvasPanelSlot* MainOverlaySlot = RootCanvas->AddChildToCanvas(MainOverlay);
	MainOverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	MainOverlaySlot->SetOffsets(FMargin(0, 0, 0, 0));
	UBorder* FullScreenBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FullScreenBg->SetBrushColor(FLinearColor(0.0f, 0.01f, 0.03f, 1.0f));
	UOverlaySlot* BgSlot = MainOverlay->AddChildToOverlay(FullScreenBg);
	BgSlot->SetHorizontalAlignment(HAlign_Fill);
	BgSlot->SetVerticalAlignment(VAlign_Fill);
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* VBoxSlot = MainOverlay->AddChildToOverlay(MainVBox);
	VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	VBoxSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlay* MapCenterOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UVerticalBoxSlot* MapSlot = MainVBox->AddChildToVerticalBox(MapCenterOverlay);
	MapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MapSlot->SetPadding(FMargin(20.0f, 30.0f, 20.0f, 5.0f));
	UScaleBox* MapScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	MapScaleBox->SetStretch(EStretch::ScaleToFit);
	UOverlaySlot* ScaleSlot = MapCenterOverlay->AddChildToOverlay(MapScaleBox);
	ScaleSlot->SetHorizontalAlignment(HAlign_Center);
	ScaleSlot->SetVerticalAlignment(VAlign_Center);
	USizeBox* MapSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MapSizeBox->SetWidthOverride(2048.0f);
	MapSizeBox->SetHeightOverride(1024.0f);
	MapScaleBox->SetContent(MapSizeBox);
	MapClippingBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MapClippingBorder->SetBrushColor(FLinearColor(0.01f, 0.03f, 0.08f, 1.0f));
	MapClippingBorder->SetClipping(EWidgetClipping::ClipToBounds);
	MapSizeBox->SetContent(MapClippingBorder);
	MapImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (WorldMapTexture) MapImage->SetBrushFromTexture(WorldMapTexture);
	MapImage->SetRenderTransformPivot(FVector2D(0.f, 0.f));
	MapClippingBorder->SetContent(MapImage);
	UTextBlock* MapHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MapHint->SetText(AOrbitalManager::Tr(
		TEXT("Click Stanga = Alege Locatie | Click Dreapta = Muta Harta | Rotita = Zoom"),
		TEXT("Left Click = Pick Location | Right Click = Pan Map | Scroll = Zoom")));
	MapHint->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	MapHint->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)));
	UOverlaySlot* HintSlot = MapCenterOverlay->AddChildToOverlay(MapHint);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetVerticalAlignment(VAlign_Bottom);
	HintSlot->SetPadding(FMargin(0, 0, 0, 8.f));
	UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseBtn->SetBackgroundColor(FLinearColor(0.85f, 0.15f, 0.15f, 1.0f));
	UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CloseTxt->SetText(FText::FromString(" X "));
	CloseTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
	CloseBtn->AddChild(CloseTxt);
	CloseBtn->OnClicked.AddDynamic(this, &UMapPickerWidget::OnCancelClicked);
	UOverlaySlot* CloseSlotInfo = MainOverlay->AddChildToOverlay(CloseBtn);
	CloseSlotInfo->SetHorizontalAlignment(HAlign_Right);
	CloseSlotInfo->SetVerticalAlignment(VAlign_Top);
	CloseSlotInfo->SetPadding(FMargin(25.f, 25.f));
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 9);
	FSlateFontInfo InputFont = FCoreStyle::GetDefaultFontStyle("Bold", 12);
	FEditableTextBoxStyle TextBoxStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	FSlateBrush InputBgBrush;
	InputBgBrush.TintColor = FSlateColor(FLinearColor(0.07f, 0.07f, 0.10f, 1.0f));
	TextBoxStyle.BackgroundImageNormal = InputBgBrush;
	TextBoxStyle.BackgroundImageHovered = InputBgBrush;
	TextBoxStyle.BackgroundImageFocused = InputBgBrush;
	TextBoxStyle.BackgroundImageReadOnly = InputBgBrush;
	TextBoxStyle.TextStyle.Font = InputFont;
	TextBoxStyle.TextStyle.ColorAndOpacity = FSlateColor(FLinearColor::White);
	TextBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	FComboBoxStyle ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>("ComboBox");
	FTableRowStyle ComboRowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row");
	ComboRowStyle.TextColor = FSlateColor(FLinearColor::White);
	ComboRowStyle.SelectedTextColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f));
	auto MakeCardBorder = [&]() -> UBorder* {
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.07f, 0.95f));
		B->SetPadding(FMargin(14.f, 10.f));
		return B;
		};
	UBorder* BottomPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BottomPanel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.98f));
	BottomPanel->SetPadding(FMargin(25.0f, 12.0f, 25.0f, 12.0f));
	UVerticalBoxSlot* BottomSlotInfo = MainVBox->AddChildToVerticalBox(BottomPanel);
	BottomSlotInfo->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	UVerticalBox* BottomVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BottomPanel->SetContent(BottomVBox);
	UHorizontalBox* ColumnsHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* ColsSlot = BottomVBox->AddChildToVerticalBox(ColumnsHBox);
	ColsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	UBorder* Card1 = MakeCardBorder();
	UHorizontalBoxSlot* Card1Slot = ColumnsHBox->AddChildToHorizontalBox(Card1);
	Card1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card1Slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	UVerticalBox* Col1 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card1->SetContent(Col1);
	UTextBlock* Title1 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title1->SetText(AOrbitalManager::Tr(TEXT("CAUTARE ORAȘ"), TEXT("CITY SEARCH")));
	Title1->SetFont(TitleFont);
	Title1->SetColorAndOpacity(FLinearColor(0.2f, 0.75f, 1.0f));
	Col1->AddChildToVerticalBox(Title1)->SetPadding(FMargin(0, 0, 0, 6.0f));
	UTextBlock* CitySearchLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CitySearchLbl->SetText(AOrbitalManager::Tr(TEXT("Cauta Oras / Tara:"), TEXT("Search City / Country:")));
	CitySearchLbl->SetFont(LabelFont);
	Col1->AddChildToVerticalBox(CitySearchLbl)->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	CitySearchInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	CitySearchInput->SetWidgetStyle(TextBoxStyle);
	CitySearchInput->SetHintText(AOrbitalManager::Tr(TEXT("Tasteaza oras... (ex: Tokyo)"), TEXT("Type city... (e.g. Tokyo)")));
	CitySearchInput->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnCitySearchChanged);
	Col1->AddChildToVerticalBox(CitySearchInput)->SetPadding(FMargin(0.f, 0.f, 0.f, 5.f));
	CityResultsComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	CityResultsComboBox->SetWidgetStyle(ComboStyle);
	CityResultsComboBox->ItemStyle = ComboRowStyle;
	CityResultsComboBox->ForegroundColor = FSlateColor(FLinearColor::White);
	CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("Selecteaza din rezultate"), TEXT("Select from results")));
	CityResultsComboBox->SetSelectedIndex(0);
	CityResultsComboBox->OnSelectionChanged.AddDynamic(this, &UMapPickerWidget::OnCitySelected);
	Col1->AddChildToVerticalBox(CityResultsComboBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	GetCurrentLocButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	GetCurrentLocButton->SetBackgroundColor(FLinearColor(0.08f, 0.28f, 0.48f, 1.0f));
	UTextBlock* LocBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LocBtnTxt->SetText(AOrbitalManager::Tr(TEXT("PREIA LOCATIA CURENTA"), TEXT("GET CURRENT LOCATION")));
	LocBtnTxt->SetFont(LabelFont);
	LocBtnTxt->SetMargin(FMargin(0.f, 4.f));
	LocBtnTxt->SetJustification(ETextJustify::Center);
	GetCurrentLocButton->AddChild(LocBtnTxt);
	GetCurrentLocButton->OnClicked.AddDynamic(this, &UMapPickerWidget::OnGetCurrentLocationClicked);
	Col1->AddChildToVerticalBox(GetCurrentLocButton);
	UBorder* Card2 = MakeCardBorder();
	UHorizontalBoxSlot* Card2Slot = ColumnsHBox->AddChildToHorizontalBox(Card2);
	Card2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card2Slot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
	UVerticalBox* Col2 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card2->SetContent(Col2);
	UTextBlock* Title2 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title2->SetText(AOrbitalManager::Tr(TEXT("COORDONATE & ALTITUDINE"), TEXT("COORDINATES & ALTITUDE")));
	Title2->SetFont(TitleFont);
	Title2->SetColorAndOpacity(FLinearColor(0.4f, 0.9f, 0.5f));
	Col2->AddChildToVerticalBox(Title2)->SetPadding(FMargin(0, 0, 0, 6.0f));
	auto CreateGeoInput = [&](UVerticalBox* TargetCol, FString LabelRO, FString LabelEN, UEditableTextBox*& OutTextBox, UComboBoxString*& OutCombo, FString Opt1RO, FString Opt1EN, FString Opt2RO, FString Opt2EN)
		{
			UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Lbl->SetText(AOrbitalManager::Tr(LabelRO, LabelEN));
			Lbl->SetFont(LabelFont);
			TargetCol->AddChildToVerticalBox(Lbl)->SetPadding(FMargin(0.f, 1.f, 0.f, 1.f));
			UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			TargetCol->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			OutTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
			OutTextBox->SetWidgetStyle(TextBoxStyle);
			OutTextBox->SetJustification(ETextJustify::Center);
			OutTextBox->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnInputTextChanged);
			UHorizontalBoxSlot* TxtSlot = RowBox->AddChildToHorizontalBox(OutTextBox);
			TxtSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TxtSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			OutCombo = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
			OutCombo->SetWidgetStyle(ComboStyle);
			OutCombo->ItemStyle = ComboRowStyle;
			OutCombo->ForegroundColor = FSlateColor(FLinearColor::White);
			OutCombo->AddOption(AOrbitalManager::TrString(Opt1RO, Opt1EN));
			OutCombo->AddOption(AOrbitalManager::TrString(Opt2RO, Opt2EN));
			OutCombo->SetSelectedOption(AOrbitalManager::TrString(Opt1RO, Opt1EN));
			OutCombo->OnSelectionChanged.AddDynamic(this, &UMapPickerWidget::OnComboSelectionChanged);
			RowBox->AddChildToHorizontalBox(OutCombo);
		};
	CreateGeoInput(Col2, TEXT("Latitudine:"), TEXT("Latitude:"), InputLat, ComboLat, TEXT("N (Nord)"), TEXT("N (North)"), TEXT("S (Sud)"), TEXT("S (South)"));
	CreateGeoInput(Col2, TEXT("Longitudine:"), TEXT("Longitude:"), InputLon, ComboLon, TEXT("E (Est)"), TEXT("E (East)"), TEXT("V (Vest)"), TEXT("W (West)"));
	UTextBlock* LblAlt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LblAlt->SetText(AOrbitalManager::Tr(TEXT("Altitudine AGL (m):"), TEXT("Altitude AGL (m):")));
	LblAlt->SetFont(LabelFont);
	Col2->AddChildToVerticalBox(LblAlt)->SetPadding(FMargin(0.f, 1.f, 0.f, 1.f));
	InputAlt = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputAlt->SetWidgetStyle(TextBoxStyle);
	InputAlt->SetJustification(ETextJustify::Center);
	Col2->AddChildToVerticalBox(InputAlt);
	UBorder* Card3 = MakeCardBorder();
	UHorizontalBoxSlot* Card3Slot = ColumnsHBox->AddChildToHorizontalBox(Card3);
	Card3Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Card3Slot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
	UVerticalBox* Col3 = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card3->SetContent(Col3);
	UTextBlock* Title3 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title3->SetText(AOrbitalManager::Tr(TEXT("DATA SI ORA"), TEXT("DATE AND TIME")));
	Title3->SetFont(TitleFont);
	Title3->SetColorAndOpacity(FLinearColor(1.0f, 0.65f, 0.1f));
	Col3->AddChildToVerticalBox(Title3)->SetPadding(FMargin(0, 0, 0, 6.0f));
	auto CreateRowLabel = [&](UVerticalBox* TargetCol, FString TxtRO, FString TxtEN) {
		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Lbl->SetText(AOrbitalManager::Tr(TxtRO, TxtEN));
		Lbl->SetFont(LabelFont);
		TargetCol->AddChildToVerticalBox(Lbl)->SetPadding(FMargin(0.f, 1.f, 0.f, 1.f));
		};
	CreateRowLabel(Col3, TEXT("Data (AAAA / LL / ZZ):"), TEXT("Date (YYYY / MM / DD):"));
	UHorizontalBox* DateBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col3->AddChildToVerticalBox(DateBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	InputYear = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputYear->SetWidgetStyle(TextBoxStyle);
	InputYear->SetJustification(ETextJustify::Center);
	InputYear->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnDateChanged);
	DateBox->AddChildToHorizontalBox(InputYear)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputMonth = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputMonth->SetWidgetStyle(TextBoxStyle);
	InputMonth->SetJustification(ETextJustify::Center);
	InputMonth->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnDateChanged);
	UHorizontalBoxSlot* MSlot = DateBox->AddChildToHorizontalBox(InputMonth);
	MSlot->SetPadding(FMargin(4.f, 0));
	MSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputDay = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputDay->SetWidgetStyle(TextBoxStyle);
	InputDay->SetJustification(ETextJustify::Center);
	InputDay->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnDateChanged);
	DateBox->AddChildToHorizontalBox(InputDay)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CreateRowLabel(Col3, TEXT("Ora Locala (HH / MM / SS):"), TEXT("Local Time (HH / MM / SS):"));
	UHorizontalBox* LocalTimeBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col3->AddChildToVerticalBox(LocalTimeBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	InputLocalHour = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputLocalHour->SetWidgetStyle(TextBoxStyle);
	InputLocalHour->SetJustification(ETextJustify::Center);
	InputLocalHour->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnLocalTimeChanged);
	LocalTimeBox->AddChildToHorizontalBox(InputLocalHour)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputLocalMinute = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputLocalMinute->SetWidgetStyle(TextBoxStyle);
	InputLocalMinute->SetJustification(ETextJustify::Center);
	InputLocalMinute->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnLocalTimeChanged);
	UHorizontalBoxSlot* LMSlot = LocalTimeBox->AddChildToHorizontalBox(InputLocalMinute);
	LMSlot->SetPadding(FMargin(4.f, 0));
	LMSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputLocalSecond = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputLocalSecond->SetWidgetStyle(TextBoxStyle);
	InputLocalSecond->SetJustification(ETextJustify::Center);
	InputLocalSecond->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnLocalTimeChanged);
	LocalTimeBox->AddChildToHorizontalBox(InputLocalSecond)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CreateRowLabel(Col3, TEXT("Ora UTC (HH / MM / SS):"), TEXT("UTC Time (HH / MM / SS):"));
	UHorizontalBox* UTCTimeBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Col3->AddChildToVerticalBox(UTCTimeBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	InputUTCHour = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputUTCHour->SetWidgetStyle(TextBoxStyle);
	InputUTCHour->SetJustification(ETextJustify::Center);
	InputUTCHour->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnUTCTimeChanged);
	UTCTimeBox->AddChildToHorizontalBox(InputUTCHour)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputUTCMinute = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputUTCMinute->SetWidgetStyle(TextBoxStyle);
	InputUTCMinute->SetJustification(ETextJustify::Center);
	InputUTCMinute->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnUTCTimeChanged);
	UHorizontalBoxSlot* UMSlot = UTCTimeBox->AddChildToHorizontalBox(InputUTCMinute);
	UMSlot->SetPadding(FMargin(4.f, 0));
	UMSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InputUTCSecond = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	InputUTCSecond->SetWidgetStyle(TextBoxStyle);
	InputUTCSecond->SetJustification(ETextJustify::Center);
	InputUTCSecond->OnTextChanged.AddDynamic(this, &UMapPickerWidget::OnUTCTimeChanged);
	UTCTimeBox->AddChildToHorizontalBox(InputUTCSecond)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TxtTimeZoneInfo = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TxtTimeZoneInfo->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 8));
	TxtTimeZoneInfo->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
	Col3->AddChildToVerticalBox(TxtTimeZoneInfo)->SetPadding(FMargin(0, 1.f, 0, 4.f));
	GetCurrentTimeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	GetCurrentTimeButton->SetBackgroundColor(FLinearColor(0.45f, 0.22f, 0.05f));
	UTextBlock* TimeBtnTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TimeBtnTxt->SetText(AOrbitalManager::Tr(TEXT("PREIA TIMPUL CURENT"), TEXT("GET CURRENT TIME")));
	TimeBtnTxt->SetFont(LabelFont);
	TimeBtnTxt->SetMargin(FMargin(0.f, 3.f));
	TimeBtnTxt->SetJustification(ETextJustify::Center);
	GetCurrentTimeButton->AddChild(TimeBtnTxt);
	GetCurrentTimeButton->OnClicked.AddDynamic(this, &UMapPickerWidget::OnGetCurrentTimeClicked);
	Col3->AddChildToVerticalBox(GetCurrentTimeButton);
	UHorizontalBox* BtnHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BottomVBox->AddChildToVerticalBox(BtnHBox);
	SaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	SaveButton->SetBackgroundColor(FLinearColor(0.12f, 0.50f, 0.18f, 1.0f));
	UTextBlock* SaveTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	SaveTxt->SetText(AOrbitalManager::Tr(TEXT("TELEPORTEAZA DRONA"), TEXT("TELEPORT DRONE")));
	SaveTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	SaveTxt->SetMargin(FMargin(0.f, 7.f));
	SaveTxt->SetJustification(ETextJustify::Center);
	SaveButton->AddChild(SaveTxt);
	SaveButton->OnClicked.AddDynamic(this, &UMapPickerWidget::OnSaveClicked);
	BtnHBox->AddChildToHorizontalBox(SaveButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	BtnSpacer->SetSize(FVector2D(16.0f, 0.0f));
	BtnHBox->AddChildToHorizontalBox(BtnSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CancelButton->SetBackgroundColor(FLinearColor(0.35f, 0.08f, 0.08f, 1.0f));
	UTextBlock* CancelTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CancelTxt->SetText(AOrbitalManager::Tr(TEXT("ANULARE"), TEXT("CANCEL")));
	CancelTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
	CancelTxt->SetMargin(FMargin(0.f, 7.f));
	CancelTxt->SetJustification(ETextJustify::Center);
	CancelButton->AddChild(CancelTxt);
	CancelButton->OnClicked.AddDynamic(this, &UMapPickerWidget::OnCancelClicked);
	BtnHBox->AddChildToHorizontalBox(CancelButton)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LoadSavedCoordinates();
	return Super::RebuildWidget();
}
void UMapPickerWidget::OnCitySearchChanged(const FText& Text)
{
	if (!CityResultsComboBox) return;
	CityResultsComboBox->ClearOptions();
	FoundCitiesMap.Empty();
	FString Query = Text.ToString().TrimStartAndEnd();
	if (Query.Len() < 2)
	{
		CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("-- Scrie minim 2 caractere --"), TEXT("-- Type at least 2 chars --")));
		CityResultsComboBox->SetSelectedIndex(0);
		return;
	}
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (!Manager) return;
	if (Manager->CountryPoints.Num() == 0)
	{
		Manager->LoadCountryPoints();
	}
	int32 MatchesCount = 0;
	constexpr int32 MaxResults = 30;
	for (const auto& Pair : Manager->CountryPoints)
	{
		const FString& CountryName = Pair.Key;
		for (const FCountryPoint& P : Pair.Value)
		{
			if (P.CityName.Contains(Query, ESearchCase::IgnoreCase) || CountryName.Contains(Query, ESearchCase::IgnoreCase))
			{
				FString DisplayLabel3 = FString::Printf(TEXT("%s (%s)"), *P.CityName, *CountryName);
				FEclipseCityHit Hit;
				Hit.CityName = P.CityName;
				Hit.Country = CountryName;
				Hit.Latitude = P.Lat;
				Hit.Longitude = P.Lon;
				Hit.Altitude = P.Alt;
				FoundCitiesMap.Add(DisplayLabel3, Hit);
				CityResultsComboBox->AddOption(DisplayLabel3);
				MatchesCount++;
				if (MatchesCount >= MaxResults) break;
			}
		}
		if (MatchesCount >= MaxResults) break;
	}
	if (MatchesCount == 0)
	{
		CityResultsComboBox->AddOption(AOrbitalManager::TrString(TEXT("Niciun oras gasit"), TEXT("No cities found")));
	}
	CityResultsComboBox->SetSelectedIndex(0);
}
void UMapPickerWidget::OnCitySelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (const FEclipseCityHit* FoundCity = FoundCitiesMap.Find(SelectedItem))
	{
		ApplyCityCoordinates(*FoundCity);
	}
}
void UMapPickerWidget::ApplyCityCoordinates(const FEclipseCityHit& City)
{
	if (InputLat && ComboLat)
	{
		InputLat->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), FMath::Abs(City.Latitude))));
		FString LatOpt = City.Latitude >= 0.0f ? AOrbitalManager::TrString(TEXT("N (Nord)"), TEXT("N (North)"))
			: AOrbitalManager::TrString(TEXT("S (Sud)"), TEXT("S (South)"));
		ComboLat->SetSelectedOption(LatOpt);
	}
	if (InputLon && ComboLon)
	{
		InputLon->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), FMath::Abs(City.Longitude))));
		FString LonOpt = City.Longitude >= 0.0f ? AOrbitalManager::TrString(TEXT("E (Est)"), TEXT("E (East)"))
			: AOrbitalManager::TrString(TEXT("V (Vest)"), TEXT("W (West)"));
		ComboLon->SetSelectedOption(LonOpt);
	}
	if (InputAlt)
	{
		InputAlt->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), FMath::Max(0.0f, City.Altitude))));
	}
	OnInputTextChanged(FText::GetEmpty());
}
FReply UMapPickerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapClippingBorder && MapClippingBorder->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			bIsPanning = true;
			LastMousePosition = InMouseEvent.GetScreenSpacePosition();
			return FReply::Handled();
		}
		else if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FVector2D LocalPos = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			FVector2D MapSize = MapClippingBorder->GetCachedGeometry().GetLocalSize();
			FVector2D UnzoomedPos = (LocalPos - MapPan) / MapZoom;
			float U = FMath::Clamp(UnzoomedPos.X / MapSize.X, 0.0f, 1.0f);
			float V = FMath::Clamp(UnzoomedPos.Y / MapSize.Y, 0.0f, 1.0f);
			MarkerLatLon.Y = 90.0f - (V * 180.0f);
			MarkerLatLon.X = (U * 360.0f) - 180.0f;
			bHasMarker = true;
			UpdateInputsFromMarker();
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
FReply UMapPickerWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsPanning && MapClippingBorder)
	{
		FVector2D CurrentMousePos = InMouseEvent.GetScreenSpacePosition();
		FVector2D LocalCurrent = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(CurrentMousePos);
		FVector2D LocalLast = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(LastMousePosition);
		MapPan += (LocalCurrent - LocalLast);
		LastMousePosition = CurrentMousePos;
		if (MapImage) MapImage->SetRenderTranslation(MapPan);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}
FReply UMapPickerWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bIsPanning = false;
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
FReply UMapPickerWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapClippingBorder && MapClippingBorder->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		float ZoomDelta = InMouseEvent.GetWheelDelta() * 0.1f;
		float OldZoom = MapZoom;
		MapZoom = FMath::Clamp(MapZoom + ZoomDelta, 1.0f, 20.0f);
		FVector2D LocalMouse = MapClippingBorder->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		MapPan = LocalMouse - (LocalMouse - MapPan) * (MapZoom / OldZoom);
		if (MapZoom == 1.0f) MapPan = FVector2D::ZeroVector;
		if (MapImage)
		{
			MapImage->SetRenderScale(FVector2D(MapZoom));
			MapImage->SetRenderTranslation(MapPan);
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}
int32 UMapPickerWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bHasMarker && MapClippingBorder)
	{
		FGeometry MapGeo = MapClippingBorder->GetCachedGeometry();
		FVector2D MapSize = MapGeo.GetLocalSize();
		float U = (MarkerLatLon.X + 180.0f) / 360.0f;
		float V = (90.0f - MarkerLatLon.Y) / 180.0f;
		FVector2D UnzoomedPos(U * MapSize.X, V * MapSize.Y);
		FVector2D FinalLocalPos = (UnzoomedPos * MapZoom) + MapPan;
		if (FinalLocalPos.X >= 0 && FinalLocalPos.X <= MapSize.X && FinalLocalPos.Y >= 0 && FinalLocalPos.Y <= MapSize.Y)
		{
			float CrossSize = 10.0f;
			FVector2D AbsolutePos = MapGeo.LocalToAbsolute(FinalLocalPos);
			FVector2D PaintPos = AllottedGeometry.AbsoluteToLocal(AbsolutePos);
			TArray<FVector2D> P1, P2;
			P1.Add(PaintPos + FVector2D(-CrossSize, -CrossSize)); P1.Add(PaintPos + FVector2D(CrossSize, CrossSize));
			P2.Add(PaintPos + FVector2D(-CrossSize, CrossSize)); P2.Add(PaintPos + FVector2D(CrossSize, -CrossSize));
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), P1, ESlateDrawEffect::None, FLinearColor::Red, true, 3.0f);
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), P2, ESlateDrawEffect::None, FLinearColor::Red, true, 3.0f);
		}
	}
	return LayerId + 1;
}
void UMapPickerWidget::UpdateInputsFromMarker()
{
	float AbsLat = FMath::Abs(MarkerLatLon.Y);
	float AbsLon = FMath::Abs(MarkerLatLon.X);
	if (InputLat) InputLat->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), AbsLat)));
	if (InputLon) InputLon->SetText(FText::FromString(FString::Printf(TEXT("%.5f"), AbsLon)));
	if (ComboLat)
	{
		FString OptN = AOrbitalManager::TrString(TEXT("N (Nord)"), TEXT("N (North)"));
		FString OptS = AOrbitalManager::TrString(TEXT("S (Sud)"), TEXT("S (South)"));
		ComboLat->SetSelectedOption(MarkerLatLon.Y >= 0 ? OptN : OptS);
	}
	if (ComboLon)
	{
		FString OptE = AOrbitalManager::TrString(TEXT("E (Est)"), TEXT("E (East)"));
		FString OptW = AOrbitalManager::TrString(TEXT("V (Vest)"), TEXT("W (West)"));
		ComboLon->SetSelectedOption(MarkerLatLon.X >= 0 ? OptE : OptW);
	}
	SyncTime(true);
}
void UMapPickerWidget::UpdateMarkerFromInputs()
{
	if (!InputLat || !InputLon || !ComboLat || !ComboLon) return;
	float Lat = FCString::Atof(*InputLat->GetText().ToString());
	float Lon = FCString::Atof(*InputLon->GetText().ToString());
	FString SelLat = ComboLat->GetSelectedOption();
	FString SelLon = ComboLon->GetSelectedOption();
	if (SelLat.StartsWith("S")) Lat *= -1.0f;
	if (SelLon.StartsWith("W") || SelLon.StartsWith("V")) Lon *= -1.0f;
	MarkerLatLon.Y = FMath::Clamp(Lat, -90.0f, 90.0f);
	MarkerLatLon.X = FMath::Clamp(Lon, -180.0f, 180.0f);
	bHasMarker = true;
	SyncTime(true);
}
void UMapPickerWidget::OnInputTextChanged(const FText& Text) { UpdateMarkerFromInputs(); }
void UMapPickerWidget::OnComboSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType) { UpdateMarkerFromInputs(); }
int32 UMapPickerWidget::GetTimeZoneOffset()
{
	return FMath::RoundToInt(MarkerLatLon.X / 15.0f);
}
void UMapPickerWidget::OnDateChanged(const FText& Text) { SyncTime(true); }
void UMapPickerWidget::OnLocalTimeChanged(const FText& Text) { SyncTime(true); }
void UMapPickerWidget::OnUTCTimeChanged(const FText& Text) { SyncTime(false); }
void UMapPickerWidget::SyncTime(bool bFromLocal)
{
	if (bIsUpdatingTime) return;
	bIsUpdatingTime = true;
	int32 OffsetSeconds = FMath::RoundToInt(MarkerLatLon.X * 240.0f);
	if (TxtTimeZoneInfo)
	{
		FString Sign = MarkerLatLon.X >= 0 ? TEXT("+") : TEXT("-");
		int32 AbsOffset = FMath::Abs(OffsetSeconds);
		int32 OffH = AbsOffset / 3600;
		int32 OffM = (AbsOffset % 3600) / 60;
		int32 OffS = AbsOffset % 60;
		FString TxtRO = FString::Printf(TEXT("Timp Solar Mediu (Offset fata de UTC: %s%02d:%02d:%02d)"), *Sign, OffH, OffM, OffS);
		FString TxtEN = FString::Printf(TEXT("Mean Solar Time (UTC Offset: %s%02d:%02d:%02d)"), *Sign, OffH, OffM, OffS);
		TxtTimeZoneInfo->SetText(AOrbitalManager::Tr(TxtRO, TxtEN));
	}
	int32 Y = InputYear ? FCString::Atoi(*InputYear->GetText().ToString()) : 2024;
	int32 M = InputMonth ? FCString::Atoi(*InputMonth->GetText().ToString()) : 3;
	int32 D = InputDay ? FCString::Atoi(*InputDay->GetText().ToString()) : 21;
	Y = FMath::Clamp(Y, -13000, 17000);
	M = FMath::Clamp(M, 1, 12);
	int32 DaysInMonthArr[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	bool bIsLeapYear = (Y % 4 == 0 && (Y % 100 != 0 || Y % 400 == 0));
	if (Y < 0 && (FMath::Abs(Y) % 4 == 0)) { bIsLeapYear = true; }
	if (M == 2 && bIsLeapYear) DaysInMonthArr[2] = 29;
	int32 MaxDaysInMonth = DaysInMonthArr[M];
	D = FMath::Clamp(D, 1, MaxDaysInMonth);
	if (bFromLocal)
	{
		int32 LH = InputLocalHour ? FCString::Atoi(*InputLocalHour->GetText().ToString()) : 12;
		int32 LM = InputLocalMinute ? FCString::Atoi(*InputLocalMinute->GetText().ToString()) : 0;
		int32 LS = InputLocalSecond ? FCString::Atoi(*InputLocalSecond->GetText().ToString()) : 0;
		LH = FMath::Clamp(LH, 0, 23);
		LM = FMath::Clamp(LM, 0, 59);
		LS = FMath::Clamp(LS, 0, 59);
		int32 LocalTotalSecs = LH * 3600 + LM * 60 + LS;
		int32 UTCTotalSecs = LocalTotalSecs - OffsetSeconds;
		while (UTCTotalSecs < 0) UTCTotalSecs += 86400;
		UTCTotalSecs %= 86400;
		int32 UH = UTCTotalSecs / 3600;
		int32 UM = (UTCTotalSecs % 3600) / 60;
		int32 US = UTCTotalSecs % 60;
		if (InputUTCHour) InputUTCHour->SetText(FText::FromString(FString::Printf(TEXT("%02d"), UH)));
		if (InputUTCMinute) InputUTCMinute->SetText(FText::FromString(FString::Printf(TEXT("%02d"), UM)));
		if (InputUTCSecond) InputUTCSecond->SetText(FText::FromString(FString::Printf(TEXT("%02d"), US)));
	}
	else
	{
		int32 UH = InputUTCHour ? FCString::Atoi(*InputUTCHour->GetText().ToString()) : 10;
		int32 UM = InputUTCMinute ? FCString::Atoi(*InputUTCMinute->GetText().ToString()) : 0;
		int32 US = InputUTCSecond ? FCString::Atoi(*InputUTCSecond->GetText().ToString()) : 0;
		UH = FMath::Clamp(UH, 0, 23);
		UM = FMath::Clamp(UM, 0, 59);
		US = FMath::Clamp(US, 0, 59);
		int32 UTCTotalSecs = UH * 3600 + UM * 60 + US;
		int32 LocalTotalSecs = UTCTotalSecs + OffsetSeconds;
		while (LocalTotalSecs < 0) LocalTotalSecs += 86400;
		LocalTotalSecs %= 86400;
		int32 LH = LocalTotalSecs / 3600;
		int32 LM = (LocalTotalSecs % 3600) / 60;
		int32 LS = LocalTotalSecs % 60;
		if (InputLocalHour) InputLocalHour->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LH)));
		if (InputLocalMinute) InputLocalMinute->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LM)));
		if (InputLocalSecond) InputLocalSecond->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LS)));
	}
	bIsUpdatingTime = false;
}
void UMapPickerWidget::LoadSavedCoordinates()
{
	if (GConfig)
	{
		float SavedLat = 44.4268f, SavedLon = 26.1025f, SavedAlt = 200.0f;
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnLat"), SavedLat, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnLon"), SavedLon, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("SetariCustom"), TEXT("SpawnAlt"), SavedAlt, GGameUserSettingsIni);
		MarkerLatLon.Y = SavedLat;
		MarkerLatLon.X = SavedLon;
		bHasMarker = true;
		int32 Y = 2024, M = 3, D = 21, LH = 12, LM = 0, LS = 0;
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnYear"), Y, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMonth"), M, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnDay"), D, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnHourLocal"), LH, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnMinuteLocal"), LM, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("SetariCustom"), TEXT("SpawnSecondLocal"), LS, GGameUserSettingsIni);
		bIsUpdatingTime = true;
		if (InputYear) InputYear->SetText(FText::FromString(FString::Printf(TEXT("%d"), Y)));
		if (InputMonth) InputMonth->SetText(FText::FromString(FString::Printf(TEXT("%02d"), M)));
		if (InputDay) InputDay->SetText(FText::FromString(FString::Printf(TEXT("%02d"), D)));
		if (InputLocalHour) InputLocalHour->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LH)));
		if (InputLocalMinute) InputLocalMinute->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LM)));
		if (InputLocalSecond) InputLocalSecond->SetText(FText::FromString(FString::Printf(TEXT("%02d"), LS)));
		bIsUpdatingTime = false;
		UpdateInputsFromMarker();
		if (InputAlt) InputAlt->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), SavedAlt)));
		SyncTime(true);
	}
}
void UMapPickerWidget::OnGetCurrentLocationClicked()
{
	APlayerController* PC = GetOwningPlayer();
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	if (PC && GeoRef && PC->GetPawn())
	{
		FVector DronaPos = PC->GetPawn()->GetActorLocation();
		FVector LLH = GeoRef->TransformUnrealPositionToLongitudeLatitudeHeight(DronaPos);
		MarkerLatLon.X = LLH.X;
		MarkerLatLon.Y = LLH.Y;
		bHasMarker = true;
		UpdateInputsFromMarker();
		if (InputAlt) InputAlt->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), LLH.Z)));
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, AOrbitalManager::TrString(TEXT("Locatie preluata cu succes!"), TEXT("Location fetched successfully!")));
	}
}
void UMapPickerWidget::OnGetCurrentTimeClicked()
{
	AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
	if (Manager)
	{
		int32 Y = Manager->CurrentDate.Year;
		int32 M = Manager->CurrentDate.Month;
		int32 D = Manager->CurrentDate.Day;
		float H_Float = Manager->CurrentDate.Hour;
		int32 UH = FMath::FloorToInt(H_Float);
		int32 UMin = FMath::FloorToInt((H_Float - UH) * 60.0f);
		int32 USec = FMath::FloorToInt((((H_Float - UH) * 60.0f) - UMin) * 60.0f);
		if (InputYear) InputYear->SetText(FText::FromString(FString::FromInt(Y)));
		if (InputMonth) InputMonth->SetText(FText::FromString(FString::Printf(TEXT("%02d"), M)));
		if (InputDay) InputDay->SetText(FText::FromString(FString::Printf(TEXT("%02d"), D)));
		if (InputUTCHour) InputUTCHour->SetText(FText::FromString(FString::Printf(TEXT("%02d"), UH)));
		if (InputUTCMinute) InputUTCMinute->SetText(FText::FromString(FString::Printf(TEXT("%02d"), UMin)));
		if (InputUTCSecond) InputUTCSecond->SetText(FText::FromString(FString::Printf(TEXT("%02d"), USec)));
		SyncTime(false);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, AOrbitalManager::TrString(TEXT("Data si Ora preluate cu succes!"), TEXT("Date and Time fetched successfully!")));
	}
}
void UMapPickerWidget::OnSaveClicked()
{
	UpdateMarkerFromInputs();
	float Alt = InputAlt ? FCString::Atof(*InputAlt->GetText().ToString()) : 200.0f;
	int32 Y = InputYear ? FCString::Atoi(*InputYear->GetText().ToString()) : 2024;
	int32 M = InputMonth ? FCString::Atoi(*InputMonth->GetText().ToString()) : 3;
	int32 D = InputDay ? FCString::Atoi(*InputDay->GetText().ToString()) : 21;
	int32 LH = InputLocalHour ? FCString::Atoi(*InputLocalHour->GetText().ToString()) : 12;
	int32 LM = InputLocalMinute ? FCString::Atoi(*InputLocalMinute->GetText().ToString()) : 0;
	int32 LS = InputLocalSecond ? FCString::Atoi(*InputLocalSecond->GetText().ToString()) : 0;
	int32 UH = InputUTCHour ? FCString::Atoi(*InputUTCHour->GetText().ToString()) : 10;
	int32 UMin = InputUTCMinute ? FCString::Atoi(*InputUTCMinute->GetText().ToString()) : 0;
	int32 USec = InputUTCSecond ? FCString::Atoi(*InputUTCSecond->GetText().ToString()) : 0;
	Y = FMath::Clamp(Y, -13000, 17000);
	M = FMath::Clamp(M, 1, 12);
	int32 DaysInMonthArr[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	bool bIsLeapYear = (Y % 4 == 0 && (Y % 100 != 0 || Y % 400 == 0));
	if (Y < 0 && (FMath::Abs(Y) % 4 == 0)) { bIsLeapYear = true; }
	if (M == 2 && bIsLeapYear) DaysInMonthArr[2] = 29;
	int32 MaxDaysInMonth = DaysInMonthArr[M];
	D = FMath::Clamp(D, 1, MaxDaysInMonth);
	LH = FMath::Clamp(LH, 0, 23);
	LM = FMath::Clamp(LM, 0, 59);
	LS = FMath::Clamp(LS, 0, 59);
	UH = FMath::Clamp(UH, 0, 23);
	UMin = FMath::Clamp(UMin, 0, 59);
	USec = FMath::Clamp(USec, 0, 59);
	if (GConfig)
	{
		GConfig->SetFloat(TEXT("SetariCustom"), TEXT("SpawnLat"), MarkerLatLon.Y, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariCustom"), TEXT("SpawnLon"), MarkerLatLon.X, GGameUserSettingsIni);
		GConfig->SetFloat(TEXT("SetariCustom"), TEXT("SpawnAlt"), Alt, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnYear"), Y, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnMonth"), M, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnDay"), D, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnHourLocal"), LH, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnMinuteLocal"), LM, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnSecondLocal"), LS, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnHourUTC"), UH, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnMinuteUTC"), UMin, GGameUserSettingsIni);
		GConfig->SetInt(TEXT("SetariCustom"), TEXT("SpawnSecondUTC"), USec, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	ACesiumGeoreference* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
	APlayerController* PC = GetOwningPlayer();
	if (GeoRef && PC)
	{
		GeoRef->SetOriginLongitudeLatitudeHeight(FVector(MarkerLatLon.X, MarkerLatLon.Y, Alt));
		TArray<AActor*> FoundStarts;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), FoundStarts);
		for (AActor* StartActor : FoundStarts)
		{
			if (StartActor)
			{
				if (USceneComponent* RootComp = StartActor->GetRootComponent())
					RootComp->SetMobility(EComponentMobility::Movable);
				StartActor->SetActorLocation(FVector(0.f, 0.f, 0.f));
			}
		}
		ADronaMea* Drona = Cast<ADronaMea>(PC->GetPawn());
		if (Drona)
		{
			Drona->SetActorLocation(FVector(0.f, 0.f, 5000.0f));
			if (UPrimitiveComponent* PhysComp = Cast<UPrimitiveComponent>(Drona->GetRootComponent()))
			{
				PhysComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
				PhysComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
			Drona->bSearchingForGround = true;
			Drona->SearchTimeout = 5.0f;
		}
		AOrbitalManager* Manager = Cast<AOrbitalManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AOrbitalManager::StaticClass()));
		if (Manager)
		{
			float SelectedHourFloat = (float)UH + (UMin / 60.0f) + (USec / 3600.0f);
			Manager->SetSimulationDateTime(D, M, Y, SelectedHourFloat);
		}
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, AOrbitalManager::TrString(TEXT("Setari Default Salvate! Drona repozitionata."), TEXT("Default Settings Saved! Drone repositioned.")));
	}
	OnCancelClicked();
}
void UMapPickerWidget::OnCancelClicked()
{
	this->RemoveFromParent();
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		UUserWidget* SettingsMenu = CreateWidget<UUserWidget>(PC, USettingsMenuWidget::StaticClass());
		if (SettingsMenu) SettingsMenu->AddToViewport(100);
	}
}
TSharedRef<SWidget> UCreditsMenuWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* BgBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BgBorder->SetBrushColor(FLinearColor(0.015f, 0.015f, 0.03f, 0.95f));
	BgBorder->SetPadding(FMargin(0.0f));
	UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(BgBorder);
	BgSlot->SetAnchors(FAnchors(0.5f));
	BgSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BgSlot->SetSize(FVector2D(750.0f, 800.0f));
	UVerticalBox* MainLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BgBorder->SetContent(MainLayout);
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HeaderBg->SetBrushColor(FLinearColor(0.05f, 0.15f, 0.3f, 1.0f));
	HeaderBg->SetPadding(FMargin(15.f, 10.f, 15.f, 10.f));
	UVerticalBoxSlot* HeaderSlot = MainLayout->AddChildToVerticalBox(HeaderBg);
	HeaderSlot->SetPadding(FMargin(0.0f));
	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderBg->SetContent(HeaderBox);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("CREDITE SI LICENTE"), TEXT("CREDITS & LICENSES")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	HeaderBox->AddChildToHorizontalBox(Title)->SetVerticalAlignment(VAlign_Center);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	HeaderBox->AddChildToHorizontalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CloseButton->SetBackgroundColor(FLinearColor(0.8f, 0.1f, 0.1f, 1.0f));
	UTextBlock* XTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	XTxt->SetText(FText::FromString("X"));
	XTxt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	CloseButton->AddChild(XTxt);
	CloseButton->OnClicked.AddDynamic(this, &UCreditsMenuWidget::OnCloseClicked);
	HeaderBox->AddChildToHorizontalBox(CloseButton);
	UScrollBox* ScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UVerticalBoxSlot* ScrollSlot = MainLayout->AddChildToVerticalBox(ScrollBox);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetPadding(FMargin(25.f));
	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ScrollBox->AddChild(ContentBox);
	auto AddSectionHeader = [&](const FString& HeaderText) {
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Txt->SetText(FText::FromString(HeaderText));
		Txt->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
		Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.7f, 1.0f, 1.0f)));
		UVerticalBoxSlot* Slot = ContentBox->AddChildToVerticalBox(Txt);
		Slot->SetPadding(FMargin(0.f, 25.f, 0.f, 10.f));
		};
	auto AddEntry = [&](const FString& ItemName, const FString& Details, const FString& Url) {
		UVerticalBox* EntryBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TitleText->SetText(FText::FromString(TEXT("• ") + ItemName));
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		TitleRow->AddChildToHorizontalBox(TitleText)->SetVerticalAlignment(VAlign_Center);
		if (!Url.IsEmpty())
		{
			USpacer* RowSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			RowSpacer->SetSize(FVector2D(15.f, 1.f));
			TitleRow->AddChildToHorizontalBox(RowSpacer);
			FString LinkStr = AOrbitalManager::TrString(TEXT("Vezi Link"), TEXT("View Link"));
			UNativeWidgetHost* Host = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass());
			Host->SetContent(
				SNew(SHyperlink)
				.Text(FText::FromString(LinkStr))
				.OnNavigate_Lambda([Url]() {
					FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
					})
			);
			TitleRow->AddChildToHorizontalBox(Host)->SetVerticalAlignment(VAlign_Center);
		}
		EntryBox->AddChildToVerticalBox(TitleRow);
		if (!Details.IsEmpty())
		{
			UTextBlock* DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			DescText->SetText(FText::FromString(TEXT("  ") + Details));
			DescText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
			DescText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));
			DescText->SetAutoWrapText(true);
			EntryBox->AddChildToVerticalBox(DescText)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		UVerticalBoxSlot* Slot = ContentBox->AddChildToVerticalBox(EntryBox);
		Slot->SetPadding(FMargin(10.f, 5.f, 0.f, 12.f));
		};
	auto AddLegalText = [&](const FString& Text) {
		UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Txt->SetText(FText::FromString(Text));
		Txt->SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 10));
		Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f)));
		Txt->SetAutoWrapText(true);
		UVerticalBoxSlot* Slot = ContentBox->AddChildToVerticalBox(Txt);
		Slot->SetPadding(FMargin(0.f, 30.f, 0.f, 10.f));
		};
	AddSectionHeader(AOrbitalManager::TrString(TEXT("DEZVOLTARE"), TEXT("DEVELOPMENT")));
	AddEntry(
		AOrbitalManager::TrString(TEXT("Profesor Coordonator"), TEXT("Thesis Supervisor")),
		AOrbitalManager::TrString(TEXT("Conf. dr. ing. Victor-Iosif Asavei"), TEXT("Assoc. Prof. Victor-Iosif Asavei, PhD")),
		TEXT("")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Profesor Coordonator"), TEXT("Thesis Supervisor")),
		AOrbitalManager::TrString(TEXT("Ș. l. dr. ing. Florin-Cătălin Tudose"), TEXT("Lect. Florin-Catalin Tudose, PhD")),
		TEXT("")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Coordonator Științific"), TEXT("Scientific Coordinator")),
		AOrbitalManager::TrString(TEXT("Astronom Cătălin Beldea"), TEXT("Astronomer Catalin Beldea")),
		TEXT("")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Dezvoltator"), TEXT("Developer")),
		TEXT("Dragoș-Eduard-Gabriel Boncan"),
		TEXT("")
	);
	AddSectionHeader(AOrbitalManager::TrString(TEXT("SOFTWARE SI MOTOR ASTRONOMIC"), TEXT("SOFTWARE & ASTRONOMY ENGINE")));
	AddEntry(
		AOrbitalManager::TrString(TEXT("Randare Glob 3D"), TEXT("3D Globe Rendering")),
		AOrbitalManager::TrString(TEXT("Sustinut de Cesium pentru Unreal."), TEXT("Powered by Cesium for Unreal.")),
		TEXT("https://cesium.com/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Calcule Astronomice"), TEXT("Astronomical Calculations")),
		AOrbitalManager::TrString(TEXT("Realizat cu Skyfield (Brandon Rhodes)"), TEXT("Powered by Skyfield (Brandon Rhodes)")),
		TEXT("https://rhodesmill.org/skyfield/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Date Efemeride"), TEXT("Ephemeris Data")),
		AOrbitalManager::TrString(TEXT("Nuclee SPICE si DE441 NASA JPL furnizate de NASA NAIF."), TEXT("NASA JPL DE441 and SPICE kernels provided by NASA NAIF.")),
		TEXT("https://naif.jpl.nasa.gov/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Catalog Eclipse Solare"), TEXT("Solar Eclipses Catalog")),
		AOrbitalManager::TrString(TEXT("Baza de date istorica a eclipselor (13000 i.Hr. - 17000 d.Hr.) compilata de John Irwin."), TEXT("Historical eclipse database (13000 BC - 17000 AD) compiled by John Irwin.")),
		TEXT("http://www.jir1667.plus.com/eclipse/eclipsecats3/index.html")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Date Rotatia Pamantului"), TEXT("Earth Rotation Data")),
		AOrbitalManager::TrString(TEXT("finals2000A.all furnizat de IERS."), TEXT("finals2000A.all provided by IERS.")),
		TEXT("https://www.iers.org/IERS/EN/DataProducts/EarthOrientationData/eop.html")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Catalog Stele Fundal"), TEXT("Background Stars Catalog")),
		AOrbitalManager::TrString(TEXT("Baza de date HYG de David Nash / Astronexus (CC BY-SA 4.0)."), TEXT("HYG Database by David Nash / Astronexus (CC BY-SA 4.0).")),
		TEXT("https://www.astronexus.com/hyg")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Procesare Video (FFmpeg)"), TEXT("Video Processing (FFmpeg)")),
		AOrbitalManager::TrString(
			TEXT("Acest proiect utilizeaza FFmpeg, un software open-source licentiat sub GPLv3.0. Codul sursa este disponibil pe site-ul oficial."),
			TEXT("This software uses code of FFmpeg, an open-source software licensed under the GPLv3.0. Source code is available on the official website.")
		),
		TEXT("http://ffmpeg.org")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Serviciu de Geocodare"), TEXT("Geocoding Service")),
		AOrbitalManager::TrString(TEXT("Sustinut de Nominatim. Date (c) contribuitorii OpenStreetMap."), TEXT("Powered by Nominatim. Data (c) OpenStreetMap contributors.")),
		TEXT("https://www.openstreetmap.org/copyright")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Limbajul Python"), TEXT("Python Language")),
		AOrbitalManager::TrString(TEXT("Server de backend sustinut de Python. (c) Python Software Foundation."), TEXT("Backend server powered by Python. (c) Python Software Foundation.")),
		TEXT("https://www.python.org/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Libraria NumPy"), TEXT("NumPy Library")),
		AOrbitalManager::TrString(TEXT("Librarie de calcul stiintific pentru Python (Licenta BSD)."), TEXT("Scientific computing library for Python (BSD License).")),
		TEXT("https://numpy.org/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Libraria Geopy"), TEXT("Geopy Library")),
		AOrbitalManager::TrString(TEXT("Client de geocodare pentru Python (Licenta MIT)."), TEXT("Geocoding client for Python (MIT License).")),
		TEXT("https://geopy.readthedocs.io/")
	);
	AddSectionHeader(AOrbitalManager::TrString(TEXT("CONTINUT VIZUAL (TEXTURI SI MODELE 3D)"), TEXT("VISUAL CONTENT (TEXTURES & 3D MODELS)")));
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura 2D Pamant"), TEXT("Earth 2D Texture")),
		AOrbitalManager::TrString(TEXT("NASA Scientific Visualization Studio (Domeniu Public)"), TEXT("NASA Scientific Visualization Studio (Public Domain)")),
		TEXT("https://svs.gsfc.nasa.gov/2915")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Luna"), TEXT("Moon Texture")),
		AOrbitalManager::TrString(TEXT("NASA Scientific Visualization Studio (Domeniu Public)"), TEXT("NASA Scientific Visualization Studio (Public Domain)")),
		TEXT("https://svs.gsfc.nasa.gov/4720/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Fotografie Eclipsa Totala"), TEXT("Total Solar Eclipse Photo")),
		AOrbitalManager::TrString(TEXT("NASA Science (Domeniu Public)"), TEXT("NASA Science (Public Domain)")),
		TEXT("https://science.nasa.gov/solar-system/skywatching/april-8-total-solar-eclipse-through-the-eyes-of-nasa/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Texturi Planetare"), TEXT("Planetary Textures")),
		AOrbitalManager::TrString(TEXT("Furnizate de SolarSystemScope.com (CC BY 4.0)"), TEXT("Provided by SolarSystemScope.com (CC BY 4.0)")),
		TEXT("https://www.solarsystemscope.com/textures/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Europa"), TEXT("Europa Texture")),
		AOrbitalManager::TrString(TEXT("USGS Astrogeology Science Center (Domeniu Public)"), TEXT("USGS Astrogeology Science Center (Public Domain)")),
		TEXT("https://astrogeology.usgs.gov/search/map/europa_voyager_galileo_ssi_global_mosaic_500m")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Io"), TEXT("Io Texture")),
		AOrbitalManager::TrString(TEXT("USGS Astrogeology Science Center (Domeniu Public)"), TEXT("USGS Astrogeology Science Center (Public Domain)")),
		TEXT("https://astrogeology.usgs.gov/search/map/io_galileo_ssi_voyager_color_merged_global_mosaic_1km")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Ganymede"), TEXT("Ganymede Texture")),
		AOrbitalManager::TrString(TEXT("NASA Science 3D Resources (Domeniu Public)"), TEXT("NASA Science 3D Resources (Public Domain)")),
		TEXT("https://science.nasa.gov/3d-resources/jupiter-ganymede/")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Callisto"), TEXT("Callisto Texture")),
		AOrbitalManager::TrString(TEXT("De Askaniy via Wikimedia Commons (CC BY-SA)"), TEXT("By Askaniy via Wikimedia Commons (CC BY-SA)")),
		TEXT("https://commons.wikimedia.org/wiki/File:Callisto_map_by_Askaniy.png")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Triton"), TEXT("Triton Texture")),
		AOrbitalManager::TrString(TEXT("De Askaniy via DeviantArt (CC BY-SA)"), TEXT("By Askaniy via DeviantArt (CC BY-SA)")),
		TEXT("https://www.deviantart.com/askaniy/art/Triton-Texture-Map-14K-761940736")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Textura Atmosfera Titan"), TEXT("Titan Atmosphere Texture")),
		AOrbitalManager::TrString(TEXT("Bazata pe harta radar originala de FarGetaNik (Celestia Motherlode), procesata sub CC-BY-SA."), TEXT("Based on original radar work by FarGetaNik (Celestia Motherlode), processed under CC-BY-SA.")),
		TEXT("http://www.celestiamotherlode.net/addon/addon_1675.html")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Model 3D Drona"), TEXT("3D Drone Model")),
		AOrbitalManager::TrString(TEXT("\"DJI FPV by SDC\" de SDC PERFORMANCE (CC BY 4.0)"), TEXT("\"DJI FPV by SDC\" by SDC PERFORMANCE (CC BY 4.0)")),
		TEXT("https://sketchfab.com/3d-models/dji-fpv-by-sdc-high-performance-drone-d471ea8c6235457b8e131842e2cf3783")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Ilustratii Constelatii"), TEXT("Constellation Illustrations")),
		AOrbitalManager::TrString(TEXT("Culturi ceresti vestice din Stellarium (GPL)"), TEXT("Western Skycultures from Stellarium (GPL)")),
		TEXT("https://github.com/Stellarium/stellarium-skycultures/tree/master/western/illustrations")
	);
	AddSectionHeader(AOrbitalManager::TrString(TEXT("CERCETARE SI INSPIRATIE"), TEXT("RESEARCH & INSPIRATION")));
	AddEntry(
		AOrbitalManager::TrString(TEXT("Geometria si Conceptele Eclipselor"), TEXT("Eclipse Geometry & Concepts")),
		AOrbitalManager::TrString(TEXT("Inspirat de munca lui Xavier Jubier."), TEXT("Inspired by the work of Xavier Jubier.")),
		TEXT("http://xjubier.free.fr/en/site_pages/solar_eclipses/TSE_2026_GoogleMapFull.html")
	);
	AddEntry(
		AOrbitalManager::TrString(TEXT("Iconite Interfata (UI)"), TEXT("User Interface Icons")),
		AOrbitalManager::TrString(TEXT("Generate cu ajutorul inteligentei artificiale (Anthropic Claude)."), TEXT("Generated using artificial intelligence (Anthropic Claude).")),
		TEXT("https://www.anthropic.com/")
	);
	AddLegalText(AOrbitalManager::TrString(
		TEXT("Acest software necesita o conexiune activa la internet pentru a descarca datele efemeride actualizate.\nEclipse Simulator 3D uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere. Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.\nToate texturile si modelele 3D sunt utilizate in conformitate cu licentele lor respective. Toate drepturile asupra datelor astronomice apartin surselor mentionate."),
		TEXT("This software requires an active internet connection to fetch updated ephemeris data.\nEclipse Simulator 3D uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere. Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.\nAll textures and 3D models are used in accordance with their respective licenses. All rights to astronomical data belong to the mentioned sources.")
	));
	return Super::RebuildWidget();
}
void UCreditsMenuWidget::OnCloseClicked()
{
	this->RemoveFromParent();
}
TSharedRef<SWidget> UFirstRunSetupWidget::RebuildWidget()
{
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	UBorder* Overlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Overlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.82f));
	UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(Overlay);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrushColor(FLinearColor(0.09f, 0.1f, 0.12f, 1.0f));
	Box->SetPadding(FMargin(0.f));
	UCanvasPanelSlot* BoxSlot = RootCanvas->AddChildToCanvas(Box);
	BoxSlot->SetAnchors(FAnchors(0.5f));
	BoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BoxSlot->SetSize(FVector2D(620.0f, 400.0f));
	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Box->SetContent(Layout);
	FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	UBorder* AccentBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	AccentBar->SetBrushColor(FLinearColor(0.2f, 0.55f, 0.95f, 1.0f));
	UVerticalBoxSlot* AccentSlot = Layout->AddChildToVerticalBox(AccentBar);
	AccentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	USizeBox* AccentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AccentSizeBox->SetHeightOverride(4.0f);
	AccentBar->SetContent(AccentSizeBox);
	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Layout->AddChildToVerticalBox(ContentBox)->SetPadding(FMargin(35.f, 30.f, 35.f, 30.f));
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Title->SetText(AOrbitalManager::Tr(TEXT("Bine ai venit la Simulatorul de Eclipse 3D!"), TEXT("Welcome to Eclipse 3D Simulator!")));
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ContentBox->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	UTextBlock* Explain = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Explain->SetText(AOrbitalManager::Tr(
		TEXT("Pentru randarea 3D a terenului (Cesium) ai nevoie de un token gratuit. Il poti schimba oricand mai tarziu din Setari."),
		TEXT("3D terrain rendering (Cesium) requires a free token. You can change it anytime later from Settings.")));
	Explain->SetAutoWrapText(true);
	Explain->SetFont(LabelFont);
	Explain->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.78f, 1.0f)));
	ContentBox->AddChildToVerticalBox(Explain)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));
	UTextBlock* FieldLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FieldLabel->SetText(AOrbitalManager::Tr(TEXT("TOKEN CESIUM ION"), TEXT("CESIUM ION TOKEN")));
	FieldLabel->SetFont(SmallFont);
	FieldLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.55f, 0.6f, 1.0f)));
	ContentBox->AddChildToVerticalBox(FieldLabel)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	UHorizontalBox* KeyInputHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentBox->AddChildToVerticalBox(KeyInputHBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	CesiumKeyTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	CesiumKeyTextBox->SetHintText(AOrbitalManager::Tr(TEXT("Lipeste tokenul aici..."), TEXT("Paste your token here...")));
	CesiumKeyTextBox->SetIsPassword(true);
	FEditableTextBoxStyle KeyBoxStyle = CesiumKeyTextBox->WidgetStyle;
	FSlateColor DarkFieldBg(FLinearColor(0.04f, 0.04f, 0.05f, 1.0f));
	KeyBoxStyle.BackgroundImageNormal.TintColor = DarkFieldBg;
	KeyBoxStyle.BackgroundImageHovered.TintColor = DarkFieldBg;
	KeyBoxStyle.BackgroundImageFocused.TintColor = DarkFieldBg;
	KeyBoxStyle.BackgroundImageReadOnly.TintColor = DarkFieldBg;
	KeyBoxStyle.TextStyle.Font = LabelFont;
	KeyBoxStyle.ForegroundColor = FSlateColor(FLinearColor::White);
	KeyBoxStyle.Padding = FMargin(12.f, 10.f);
	CesiumKeyTextBox->WidgetStyle = KeyBoxStyle;
	UHorizontalBoxSlot* KeyInputSlot = KeyInputHBox->AddChildToHorizontalBox(CesiumKeyTextBox);
	KeyInputSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	KeyInputSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	ToggleKeyVisibilityButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ToggleKeyVisibilityButton->SetBackgroundColor(FLinearColor(0.16f, 0.17f, 0.19f, 1.0f));
	ToggleKeyVisibilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ToggleKeyVisibilityText->SetText(AOrbitalManager::Tr(TEXT("Arata"), TEXT("Show")));
	ToggleKeyVisibilityText->SetFont(LabelFont);
	ToggleKeyVisibilityText->SetMargin(FMargin(14.f, 10.f));
	ToggleKeyVisibilityButton->AddChild(ToggleKeyVisibilityText);
	ToggleKeyVisibilityButton->OnClicked.AddDynamic(this, &UFirstRunSetupWidget::OnToggleCesiumKeyVisibilityClicked);
	KeyInputHBox->AddChildToHorizontalBox(ToggleKeyVisibilityButton)->SetVerticalAlignment(VAlign_Fill);
	ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ErrorText->SetText(AOrbitalManager::Tr(TEXT("Introdu un token valid pentru a continua."), TEXT("Enter a valid token to continue.")));
	ErrorText->SetFont(SmallFont);
	ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.35f, 0.35f, 1.0f)));
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	ContentBox->AddChildToVerticalBox(ErrorText)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	CesiumLinkButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	CesiumLinkButton->SetBackgroundColor(FLinearColor::Transparent);
	UTextBlock* LinkText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LinkText->SetText(AOrbitalManager::Tr(TEXT("Ia un token gratuit de pe ion.cesium.com"), TEXT("Get a free token from ion.cesium.com")));
	LinkText->SetFont(SmallFont);
	LinkText->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.65f, 0.95f, 1.0f)));
	CesiumLinkButton->AddChild(LinkText);
	CesiumLinkButton->OnClicked.AddDynamic(this, &UFirstRunSetupWidget::OnCesiumLinkClicked);
	UVerticalBoxSlot* LinkSlot = ContentBox->AddChildToVerticalBox(CesiumLinkButton);
	LinkSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 26.f));
	LinkSlot->SetHorizontalAlignment(HAlign_Left);
	USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	ContentBox->AddChildToVerticalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	ConfirmButton->SetBackgroundColor(FLinearColor(0.15f, 0.5f, 0.3f, 1.0f));
	UTextBlock* ConfirmTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ConfirmTxt->SetText(AOrbitalManager::Tr(TEXT("CONFIRMA SI INTRA"), TEXT("CONFIRM & ENTER")));
	ConfirmTxt->SetFont(ButtonFont);
	ConfirmTxt->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmTxt->SetJustification(ETextJustify::Center);
	ConfirmTxt->SetMargin(FMargin(0.f, 13.f));
	ConfirmButton->AddChild(ConfirmTxt);
	ConfirmButton->OnClicked.AddDynamic(this, &UFirstRunSetupWidget::OnConfirmClicked);
	ContentBox->AddChildToVerticalBox(ConfirmButton);
	return Super::RebuildWidget();
}
void UFirstRunSetupWidget::OnToggleCesiumKeyVisibilityClicked()
{
	if (CesiumKeyTextBox && ToggleKeyVisibilityText)
	{
		bool bIsPassword = CesiumKeyTextBox->GetIsPassword();
		CesiumKeyTextBox->SetIsPassword(!bIsPassword);
		ToggleKeyVisibilityText->SetText(bIsPassword
			? AOrbitalManager::Tr(TEXT("Ascunde"), TEXT("Hide"))
			: AOrbitalManager::Tr(TEXT("Arata"), TEXT("Show")));
	}
}
void UFirstRunSetupWidget::OnCesiumLinkClicked()
{
	FPlatformProcess::LaunchURL(TEXT("https://ion.cesium.com/tokens"), nullptr, nullptr);
}
void UFirstRunSetupWidget::MarkFirstRunDone()
{
	if (GConfig)
	{
		GConfig->SetBool(TEXT("SetariCustom"), TEXT("PrimaRulare"), true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
void UFirstRunSetupWidget::OnConfirmClicked()
{
	if (!CesiumKeyTextBox) return;
	FString NewKey = CesiumKeyTextBox->GetText().ToString().TrimStartAndEnd();
	if (NewKey.IsEmpty())
	{
		if (ErrorText) ErrorText->SetVisibility(ESlateVisibility::Visible);
		return;
	}
	FString CaleFisierRulare = FPaths::Combine(FPaths::RootDir(), TEXT("rulare.dat"));
	FFileHelper::SaveStringToFile(NewKey, *CaleFisierRulare);
	if (GConfig)
	{
		GConfig->SetString(TEXT("SetariCustom"), TEXT("CesiumKey"), *NewKey, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	TArray<AActor*> CesiumTilesets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACesium3DTileset::StaticClass(), CesiumTilesets);
	for (AActor* Actor : CesiumTilesets)
	{
		if (ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(Actor))
		{
			Tileset->SetIonAccessToken(NewKey);
			Tileset->RefreshTileset();
		}
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		FInputModeGameAndUI GameAndUIMode;
		GameAndUIMode.SetHideCursorDuringCapture(false);
		GameAndUIMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(GameAndUIMode);
		PC->bShowMouseCursor = true;
	}
	MarkFirstRunDone();
	this->RemoveFromParent();
}
#include "SuitSelectWidget.h"

#include "ArenaUI.h"
#include "ArenaPlayerController.h"
#include "SuitTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "UObject/EnumProperty.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "SuitSelectWidget"

void USuitSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> USuitSelectWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildTree(); }
	return Super::RebuildWidget();
}

void USuitSelectWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Dim"));
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.02f, 0.8f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim)) { S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); S->SetOffsets(FMargin(0.f)); }

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Box))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}
	auto AddRow = [Box](UWidget* W, float PadBottom)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, PadBottom)); }
	};

	AddRow(ArenaUI::MakeText(WidgetTree, LOCTEXT("Title", "SELECT YOUR SUIT"), 38, ArenaUI::Red), 4.f);
	CounterText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 14, ArenaUI::Ice, "Regular");
	AddRow(CounterText, 10.f);

	// < NAME >
	UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* L = nullptr;
	UButton* Prev = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT("<")), 26, L);
	UButton* Next = ArenaUI::MakeButton(WidgetTree, FText::FromString(TEXT(">")), 26, L);
	Prev->OnClicked.AddDynamic(this, &USuitSelectWidget::HandlePrev);
	Next->OnClicked.AddDynamic(this, &USuitSelectWidget::HandleNext);
	USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	NameBox->SetWidthOverride(520.f);
	NameText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 34, ArenaUI::Gold);
	NameBox->SetContent(NameText);
	for (UWidget* W : TArray<UWidget*>{ Prev, NameBox, Next })
	{
		if (UHorizontalBoxSlot* S = NameRow->AddChildToHorizontalBox(W)) { S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(10.f, 0.f)); }
	}
	AddRow(NameRow, 2.f);
	MarkText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 18, ArenaUI::Ice, "Regular");
	AddRow(MarkText, 10.f);

	// Colour swatches (primary / secondary / accent / glow).
	UHorizontalBox* SwatchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 i = 0; i < 4; ++i)
	{
		USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SB->SetWidthOverride(60.f);
		SB->SetHeightOverride(24.f);
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SB->SetContent(B);
		Swatches.Add(B);
		if (UHorizontalBoxSlot* S = SwatchRow->AddChildToHorizontalBox(SB)) { S->SetPadding(FMargin(4.f, 0.f)); }
	}
	AddRow(SwatchRow, 12.f);

	USizeBox* DescBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DescBox->SetWidthOverride(640.f);
	DescText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor::White, "Regular");
	DescText->SetAutoWrapText(true);
	DescBox->SetContent(DescText);
	AddRow(DescBox, 10.f);

	AIText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 16, ArenaUI::Ice);
	AddRow(AIText, 2.f);
	WeaponText = ArenaUI::MakeText(WidgetTree, FText::GetEmpty(), 14, ArenaUI::Ice, "Regular");
	AddRow(WeaponText, 10.f);

	const FText StatNames[] = { LOCTEXT("Speed", "SPEED"), LOCTEXT("Armor", "ARMOR"), LOCTEXT("Energy", "ENERGY"), LOCTEXT("Regen", "REGEN"), LOCTEXT("Damage", "DAMAGE") };
	for (const FText& StatName : StatNames)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* LB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LB->SetWidthOverride(110.f);
		UTextBlock* T = ArenaUI::MakeText(WidgetTree, StatName, 14, ArenaUI::Gold);
		T->SetJustification(ETextJustify::Left);
		LB->SetContent(T);
		USizeBox* BB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BB->SetWidthOverride(300.f);
		BB->SetHeightOverride(14.f);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Bar->SetFillColorAndOpacity(ArenaUI::Gold);
		BB->SetContent(Bar);
		StatBars.Add(Bar);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LB)) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BB)) { S->SetVerticalAlignment(VAlign_Center); }
		AddRow(Row, 4.f);
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UButton* Confirm = ArenaUI::MakeButton(WidgetTree, LOCTEXT("SuitUp", "SUIT UP"), 22, L);
	UButton* Back = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Back", "BACK"), 22, L);
	Confirm->OnClicked.AddDynamic(this, &USuitSelectWidget::HandleConfirm);
	Back->OnClicked.AddDynamic(this, &USuitSelectWidget::HandleBack);
	for (UWidget* W : TArray<UWidget*>{ Confirm, Back })
	{
		if (UHorizontalBoxSlot* S = Buttons->AddChildToHorizontalBox(W)) { S->SetPadding(FMargin(10.f, 0.f)); }
	}
	AddRow(Buttons, 8.f);
	AddRow(ArenaUI::MakeText(WidgetTree, LOCTEXT("Hint", "Left / Right to browse  |  Enter to suit up  |  Esc back  |  In game: 1-5 / mouse wheel swap suits"), 13, ArenaUI::Ice, "Regular"), 0.f);

	Refresh();
}

void USuitSelectWidget::OpenAt(int32 SuitIndex)
{
	const int32 Num = USuitLibrary::GetNumSuits();
	Index = Num > 0 ? FMath::Clamp(SuitIndex, 0, Num - 1) : 0;
	Refresh();
}

void USuitSelectWidget::Step(int32 Delta)
{
	const int32 Num = USuitLibrary::GetNumSuits();
	if (Num <= 0) { return; }
	Index = ((Index + Delta) % Num + Num) % Num;
	Refresh();
}

namespace
{
	FText EnumText(const UEnum* E, int64 V)
	{
		return E ? E->GetDisplayNameTextByValue(V) : FText::GetEmpty();
	}
}

void USuitSelectWidget::Refresh()
{
	if (!NameText || USuitLibrary::GetNumSuits() <= 0) { return; }
	const FSuitDef& S = USuitLibrary::GetSuit(Index);
	const FAIPersonaDef& P = USuitLibrary::GetPersona(S.AI);

	CounterText->SetText(FText::Format(LOCTEXT("Counter", "{0} / {1}"), FText::AsNumber(Index + 1), FText::AsNumber(USuitLibrary::GetNumSuits())));
	NameText->SetText(S.Name);
	MarkText->SetText(S.Mark);
	DescText->SetText(S.Description);
	AIText->SetText(FText::Format(LOCTEXT("AI", "ON-BOARD AI: {0}"), P.Label));
	AIText->SetColorAndOpacity(FSlateColor(P.SubtitleColor));
	WeaponText->SetText(FText::Format(LOCTEXT("Weapons", "Primary: {0}   |   Special: {1}"),
		EnumText(StaticEnum<ESuitPrimaryWeapon>(), int64(S.PrimaryWeapon)),
		EnumText(StaticEnum<ESuitSpecialWeapon>(), int64(S.SpecialWeapon))));

	const int32 Ratings[] = { S.SpeedRating, S.ArmorRating, S.EnergyRating, S.RegenRating, S.DamageRating };
	for (int32 i = 0; i < StatBars.Num() && i < 5; ++i)
	{
		if (StatBars[i]) { StatBars[i]->SetPercent(FMath::Clamp(Ratings[i] / 10.f, 0.f, 1.f)); }
	}
	const FLinearColor Colors[] = { S.PrimaryColor, S.SecondaryColor, S.AccentColor, S.EmissiveColor };
	for (int32 i = 0; i < Swatches.Num() && i < 4; ++i)
	{
		if (Swatches[i]) { Swatches[i]->SetBrushColor(FLinearColor(Colors[i].R, Colors[i].G, Colors[i].B, 1.f)); }
	}
}

void USuitSelectWidget::HandlePrev() { Step(-1); }
void USuitSelectWidget::HandleNext() { Step(1); }

void USuitSelectWidget::HandleConfirm()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuConfirmSuit(Index); }
}

void USuitSelectWidget::HandleBack()
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer())) { PC->MenuBack(); }
}

FReply USuitSelectWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetOwningPlayer()))
	{
		if (PC->HandleGlobalKey(InKeyEvent)) { return FReply::Handled(); }
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left) { Step(-1); return FReply::Handled(); }
	if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right) { Step(1); return FReply::Handled(); }
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) { HandleConfirm(); return FReply::Handled(); }
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) { HandleBack(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE

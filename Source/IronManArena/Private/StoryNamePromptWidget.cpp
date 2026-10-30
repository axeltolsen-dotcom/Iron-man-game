#include "StoryNamePromptWidget.h"

#include "ArenaUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "StoryNamePrompt"

void UStoryNamePromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
}

TSharedRef<SWidget> UStoryNamePromptWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) { BuildTree(); }
	return Super::RebuildWidget();
}

void UStoryNamePromptWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	Panel->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.88f));
	Panel->SetPadding(FMargin(40.f, 28.f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Panel))
	{
		S->SetAnchors(FAnchors(0.5f, 0.5f));
		S->SetAlignment(FVector2D(0.5f, 0.5f));
		S->SetAutoSize(true);
	}

	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
	Panel->SetContent(Box);
	auto AddRow = [Box](UWidget* W, float Pad)
	{
		if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(W))
		{
			S->SetHorizontalAlignment(HAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Pad));
		}
	};

	TitleText = ArenaUI::MakeText(WidgetTree, PendingTitle, 30, ArenaUI::Gold);
	AddRow(TitleText, 6.f);
	PromptText = ArenaUI::MakeText(WidgetTree, PendingPrompt, 16, ArenaUI::Ice, "Regular");
	AddRow(PromptText, 16.f);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(420.f);
	NameBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("NameBox"));
	NameBox->SetHintText(FText::FromString(Default));
	NameBox->SetText(FText::FromString(Default));
	NameBox->OnTextCommitted.AddDynamic(this, &UStoryNamePromptWidget::HandleCommitted);
	Size->SetContent(NameBox);
	AddRow(Size, 16.f);

	UTextBlock* Label = nullptr;
	OkButton = ArenaUI::MakeButton(WidgetTree, LOCTEXT("Ok", "CONFIRM"), 20, Label);
	OkButton->OnClicked.AddDynamic(this, &UStoryNamePromptWidget::HandleOkClicked);
	AddRow(OkButton, 0.f);
}

void UStoryNamePromptWidget::Setup(const FText& Title, const FText& Prompt, const FString& DefaultName, int32 MaxLength)
{
	PendingTitle = Title;
	PendingPrompt = Prompt;
	Default = DefaultName;
	MaxLen = FMath::Max(1, MaxLength);
	bSubmitted = false;
	if (TitleText) { TitleText->SetText(Title); }
	if (PromptText) { PromptText->SetText(Prompt); }
	if (NameBox)
	{
		NameBox->SetHintText(FText::FromString(DefaultName));
		NameBox->SetText(FText::FromString(DefaultName));
	}
}

void UStoryNamePromptWidget::FocusInput(APlayerController* PC)
{
	if (NameBox && PC) { NameBox->SetUserFocus(PC); }
}

void UStoryNamePromptWidget::HandleCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter) { Submit(); }
}

void UStoryNamePromptWidget::HandleOkClicked()
{
	Submit();
}

void UStoryNamePromptWidget::SubmitName(const FString& Name)
{
	if (NameBox) { NameBox->SetText(FText::FromString(Name)); }
	else { Default = Name; }
	Submit();
}

void UStoryNamePromptWidget::Submit()
{
	if (bSubmitted) { return; }
	bSubmitted = true;
	const FString Clean = ArenaUI::SanitizeName(NameBox ? NameBox->GetText().ToString() : Default, Default, MaxLen);
	// Move the callback out first: the owner may clear OnSubmitted / drop this widget from inside it.
	TFunction<void(const FString&)> Callback = MoveTemp(OnSubmitted);
	OnSubmitted = nullptr;
	if (Callback) { Callback(Clean); }
}

#undef LOCTEXT_NAMESPACE

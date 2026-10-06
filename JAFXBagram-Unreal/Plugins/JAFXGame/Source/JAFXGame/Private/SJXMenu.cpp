#include "SJXMenu.h"
#include "JXPlayerController.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Gold(0.95f, 0.72f, 0.18f, 1.f);
	const FLinearColor Ink(0.04f, 0.04f, 0.05f, 1.f);
	const FLinearColor Soft(1.f, 1.f, 1.f, 0.75f);

	FSlateFontInfo Font(const char* Style, int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(Style, Size);
	}

	TSharedRef<SWidget> Label(const FString& Text, int32 Size, const FLinearColor& Color, const char* Style = "Regular")
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(Font(Style, Size))
			.ColorAndOpacity(FSlateColor(Color))
			.Justification(ETextJustify::Center);
	}
}

void SJXMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	bPauseMenu = InArgs._bPauseMenu;

	static FSlateColorBrush Backdrop(FLinearColor::White);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&Backdrop)
		.BorderBackgroundColor(FSlateColor(FLinearColor(0.02f, 0.03f, 0.05f, bPauseMenu ? 0.78f : 0.94f)))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(Switcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				MakeMainPage()
			]
			+ SWidgetSwitcher::Slot()
			[
				MakeSettingsPage()
			]
		]
	];
}

void SJXMenu::ShowPage(int32 Index)
{
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(Index);
	}
}

TSharedRef<SWidget> SJXMenu::MakeButton(const FText& Text, TFunction<void()> OnClick, float Width)
{
	return SNew(SBox)
		.WidthOverride(Width)
		.HeightOverride(66.f)
		.Padding(FMargin(4.f, 6.f))
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonColorAndOpacity(FSlateColor(Gold))
			.OnClicked(FOnClicked::CreateLambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			}))
			[
				SNew(STextBlock)
				.Text(Text)
				.Font(Font("Bold", 22))
				.ColorAndOpacity(FSlateColor(Ink))
			]
		];
}

TSharedRef<SWidget> SJXMenu::MakeMainPage()
{
	TWeakObjectPtr<AJXPlayerController> Weak = Owner;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			Label(TEXT("JAF X BAGRAM"), 72, Gold, "Bold")
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 44.f))
		[
			Label(bPauseMenu ? TEXT("PAUSED") : TEXT("BATTLE ROYALE  -  AFGHANISTAN"), 20, Soft)
		];

	if (bPauseMenu)
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			MakeButton(FText::FromString(TEXT("RESUME")), [Weak]() { if (Weak.IsValid()) Weak->CloseMenu(); })
		];
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			MakeButton(FText::FromString(TEXT("RESTART MATCH")), [Weak]() { if (Weak.IsValid()) Weak->RestartMatchFromMenu(); })
		];
	}
	else
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
		[
			MakeButton(FText::FromString(TEXT("PLAY")), [Weak]() { if (Weak.IsValid()) Weak->StartMatchFromMenu(); })
		];
	}

	Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		MakeButton(FText::FromString(TEXT("SETTINGS")), [this]() { ShowPage(1); })
	];

#if !PLATFORM_IOS
	// Apple does not allow apps to quit themselves, so there is no QUIT button on iPhone.
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		MakeButton(FText::FromString(TEXT("QUIT")), [Weak]() { if (Weak.IsValid()) Weak->QuitFromMenu(); })
	];
#endif

	const bool bTouch = Owner.IsValid() && Owner->bTouchControls;
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 36.f, 0.f, 0.f))
	[
		Label(bTouch
			? TEXT("Left stick: move   |   Right stick: look   |   FIRE / AIM / JUMP / USE buttons")
			: TEXT("WASD move  |  Mouse look  |  LMB fire  |  RMB aim  |  Space jump  |  F use  |  R reload  |  H heal  |  G grenade  |  P menu"),
			13, Soft)
	];
	return Box;
}

TSharedRef<SWidget> SJXMenu::MakeSettingsPage()
{
	TWeakObjectPtr<AJXPlayerController> Weak = Owner;

	// Graphics quality buttons, like the quality picker in mobile shooters.
	TSharedRef<SHorizontalBox> Quality = SNew(SHorizontalBox);
	static const TCHAR* Names[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
	for (int32 Level = 0; Level < 4; ++Level)
	{
		Quality->AddSlot().AutoWidth()
		[
			MakeButton(FText::FromString(Names[Level]), [Weak, Level]() { if (Weak.IsValid()) Weak->SetGraphicsQuality(Level); }, 170.f)
		];
	}

	const float StartSens = Owner.IsValid() ? Owner->LookSensitivity : 1.f;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 30.f))
		[
			Label(TEXT("SETTINGS"), 48, Gold, "Bold")
		]
		// Graphics
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Font(Font("Regular", 20))
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
			.Text_Lambda([Weak]()
			{
				static const TCHAR* Current[] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
				const int32 Q = Weak.IsValid() ? Weak->GetGraphicsQuality() : -1;
				return FText::FromString(FString::Printf(TEXT("Graphics quality: %s"), (Q >= 0 && Q < 4) ? Current[Q] : TEXT("CUSTOM")));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f, 0.f, 26.f))
		[
			Quality
		]
		// Look sensitivity
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Font(Font("Regular", 20))
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
			.Text_Lambda([Weak]()
			{
				const float S = Weak.IsValid() ? Weak->LookSensitivity : 1.f;
				return FText::FromString(FString::Printf(TEXT("Camera sensitivity: %.1f"), S));
			})
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f, 0.f, 26.f))
		[
			SNew(SBox)
			.WidthOverride(520.f)
			[
				SNew(SSlider)
				.Value((StartSens - 0.2f) / 1.8f)
				.SliderBarColor(FSlateColor(Soft))
				.SliderHandleColor(FSlateColor(Gold))
				.OnValueChanged_Lambda([Weak](float V)
				{
					if (Weak.IsValid()) Weak->SetLookSensitivity(0.2f + V * 1.8f);
				})
			]
		]
		// FPS counter
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			MakeButton(FText::FromString(TEXT("SHOW / HIDE FPS")), [Weak]() { if (Weak.IsValid()) Weak->ToggleFps(); })
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 20.f, 0.f, 0.f))
		[
			MakeButton(FText::FromString(TEXT("BACK")), [this]() { ShowPage(0); })
		];
}

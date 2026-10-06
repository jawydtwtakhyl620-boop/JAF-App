// JAF X BAGRAM - main menu, pause menu and settings, built with Slate (no UI assets needed).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AJXPlayerController;
class SWidgetSwitcher;

class SJXMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SJXMenu)
		: _bPauseMenu(false)
	{}
		SLATE_ARGUMENT(TWeakObjectPtr<AJXPlayerController>, Owner)
		SLATE_ARGUMENT(bool, bPauseMenu)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick, float Width = 380.f);
	TSharedRef<SWidget> MakeMainPage();
	TSharedRef<SWidget> MakeSettingsPage();
	void ShowPage(int32 Index);

	TWeakObjectPtr<AJXPlayerController> Owner;
	bool bPauseMenu = false;
	TSharedPtr<SWidgetSwitcher> Switcher;
};

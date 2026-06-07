// Copyright ProjectTetra. All Rights Reserved.

#include "TetrisPrimaryGameLayout.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"   // UCommonActivatableWidgetStack

UCommonActivatableWidget* UTetrisPrimaryGameLayout::PushWidgetToLayer(EUILayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	UCommonActivatableWidgetStack* Stack = GetLayerStack(Layer);
	if (!Stack)
	{
		return nullptr;
	}

	return Stack->AddWidget<UCommonActivatableWidget>(WidgetClass);
}

void UTetrisPrimaryGameLayout::RemoveWidget(UCommonActivatableWidget& Widget)
{
	// Why 소유 스택만 건드린다: 컨테이너의 RemoveWidget을 위젯이 없는 스택에 호출하면
	//   다른 스택의 슬레이트 위젯을 release하려다 깨질 수 있다. 활성 위젯 비교로 소유 스택만 식별.
	const UCommonActivatableWidgetStack* Stacks[] = { MenuStack, GameStack, GameMenuStack, ModalStack };
	for (const UCommonActivatableWidgetStack* ConstStack : Stacks)
	{
		UCommonActivatableWidgetStack* Stack = const_cast<UCommonActivatableWidgetStack*>(ConstStack);
		if (Stack && Stack->GetActiveWidget() == &Widget)
		{
			Stack->RemoveWidget(Widget);
			return;
		}
	}
}

UCommonActivatableWidgetStack* UTetrisPrimaryGameLayout::GetLayerStack(EUILayer Layer) const
{
	switch (Layer)
	{
	case EUILayer::Menu:     return MenuStack;
	case EUILayer::Game:     return GameStack;
	case EUILayer::GameMenu: return GameMenuStack;
	case EUILayer::Modal:    return ModalStack;
	default:                 return nullptr;
	}
}

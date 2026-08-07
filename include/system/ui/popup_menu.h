/**********************************************************************************************
*
POPUP MENU MODULE
*
**********************************************************************************************/
#ifndef POPUP_MENU_H
#define POPUP_MENU_H

#include "math/cvectors.h"
#include "ui/ui.h"

void InitPopupMenu(void);
void ShowPopupMenu(Vector2d position);
void HidePopupMenu(void);
bool IsPopupMenuVisible(void);
void DrawPopupMenu(void);

Frame2d *GetPopupMenuSpaceFrame(void);
UIElement *GetPopupMenuRoot(void);
UIElement *GetPopupMenuContainer(void);

#endif

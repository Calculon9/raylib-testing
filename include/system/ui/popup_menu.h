/**********************************************************************************************
*
POPUP MENU MODULE
*
**********************************************************************************************/
#ifndef POPUP_MENU_H
#define POPUP_MENU_H

#include "math/cvectors.h"
#include "system/panel_system.h"
#include "ui/ui.h"

void InitPopupMenu(void);
// Destroy the popup panel and clear its cached UI references.
void DestroyPopupMenu(void);
void ShowPopupMenu(Vector2d position);
void HidePopupMenu(void);
bool IsPopupMenuVisible(void);
void DrawPopupMenu(void);

UIElement *GetPopupMenuRoot(void);

// Get the panel system instance.
struct PanelSystem *GetPopupMenuSystem(void);

#endif

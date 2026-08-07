#include "system/ui/popup_menu.h"

#include "system/panel_system.h"
#include "system/ui_system.h"
#include "system/viewport_system.h"
#include "ui/ui_constructors.h"

static PanelSystem *popup_menu = NULL;
static UIElement *popup_menu_create_cont = NULL;
static View *popup_create_view = NULL;
static View popup_create_view_storage = {0};
static bool popup_menu_visible = false;

static const Size popup_menu_size = {{3.0f, 4.5f}, SIZE_FIXED};
static const Offset popup_menu_offset = {{0.0f, 0.0f}, OFFSET_FIXED};
static const Vector2d popup_menu_padding = {0.15f, 0.15f};
static const Spacing popup_menu_root_spacing = {{0.0f, 0.0f}, NONE, SPACING_INLINE};
static const Spacing popup_menu_child_spacing = {{0.0f, 0.05f}, PERCENT, SPACING_STACKED};

void InitPopupMenu(void)
{
    if (popup_menu)
    {
        return;
    }

    popup_menu = PanelSystem_Create(
        &game_viewport, 1.0f, ZERO_VECTOR_2D,
        &ui_default_palette, popup_menu_root_spacing);
    if (!popup_menu)
    {
        return;
    }

    PanelSystem_InitRoot(popup_menu);
    popup_menu->root->colour_fill = COLOURLESS_RGBA;
    popup_menu->root->is_enabled = false;

    popup_menu_create_cont = CreateUIContainer(
        popup_menu->root, popup_menu_size, popup_menu_offset,
        popup_menu_padding, popup_menu->palette,
        UI_PALETTE_SURFACE_CONTAINER, popup_menu_child_spacing,
        true, true);

    popup_create_view = &popup_create_view_storage;
    PanelSystem_AddView(popup_menu,popup_create_view,popup_menu_create_cont,POPUP_MENU_CREATE_VIEW);

    UpdateUISpace(popup_menu->root, popup_menu->seed_box);
}

void ShowPopupMenu(Vector2d position)
{
    if (!popup_menu)
    {
        InitPopupMenu();
    }

    if (!popup_menu || !popup_menu_create_cont)
    {
        return;
    }

    popup_menu_create_cont->authored_offset = (Offset){position, OFFSET_FIXED};
    popup_menu_create_cont->resolved_offset = popup_menu_create_cont->authored_offset;
    popup_menu_visible = true;
    popup_menu->root->is_enabled = true;
}

void HidePopupMenu(void)
{
    popup_menu_visible = false;
    if (popup_menu && popup_menu->root)
    {
        popup_menu->root->is_enabled = false;
    }
}

bool IsPopupMenuVisible(void)
{
    return popup_menu_visible;
}

void DrawPopupMenu(void)
{
    if (popup_menu && popup_menu_visible)
    {
        PanelSystem_Draw(popup_menu);
    }
}

Frame2d *GetPopupMenuSpaceFrame(void)
{
    return PanelSystem_GetSpaceFrame(popup_menu);
}

UIElement *GetPopupMenuRoot(void)
{
    return popup_menu ? popup_menu->root : NULL;
}

UIElement *GetPopupMenuContainer(void)
{
    return popup_menu_create_cont;
}

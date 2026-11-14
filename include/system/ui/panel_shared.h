#ifndef PANEL_SHARED_H
#define PANEL_SHARED_H

// Behaviour shared by the Entity Manager and the World Manager: flag families and the bulk-edit
// walker, the tinted header block, the multi-select field lock and small selection helpers.

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/view_host_system.h"
#include "ui/ui.h"
#include "ui/binding.h"
#include "world/target_resolver.h"
#include "physics/newtonoid.h"

// Which bitmask on a selection member a flag button targets (entity fields, world or cell).
typedef enum FlagFamily
{
    FLAG_FAMILY_ROLE,           // Exclusive entity role
    FLAG_FAMILY_CAPABILITY,
    FLAG_FAMILY_CONSTRAINT,
    FLAG_FAMILY_STATUS,
    FLAG_FAMILY_COLLISION_MASK,
    FLAG_FAMILY_WORLD,
    FLAG_FAMILY_CELL,
} FlagFamily;

// Flag button binding: associates a button control with its display label and underlying bitflag.
// `family` is assigned when the button is wired (PanelShared_WireFlags).
typedef struct FlagButton
{
    const char *label;
    uint32_t flag;
    UIElement *button;
    FlagFamily family;
} FlagButton;

// Tally of a bulk walk: how many targets the action applies to, and how many of those are ON.
typedef struct FlagTally
{
    int applicable;
    int on;
} FlagTally;

// The five exclusive entity roles OR'd together (the ROLE family clears this whole group on a click).
#define PANEL_SHARED_ROLE_MASK (ENTITY_ROLE_WALL | ENTITY_ROLE_NEWTONOID | \
                                ENTITY_ROLE_PROJECTILE | ENTITY_ROLE_EFFECT | ENTITY_ROLE_CAMERA)

// Pack a family and flag bit into the int key a flag button's query binding carries. Flag bits are
// all below 1 << 16, so the family lives in the upper half.
#define PANEL_FLAG_KEY(family, flag) (((int)(family) << 16) | (int)(flag))

// Thickness (UI pixels) of the coral rule along the bottom of every header block.
#define PANEL_HEADER_RULE_PX 2.0f

// Header block: a tinted container with a large title and a medium sub-line.
typedef struct PanelHeader
{
    UIElement *box;              // container whose fill is tinted in multi-select
    UIElement *title;            // FONT_LARGE title textbox
    UIElement *sub;              // FONT_MEDIUM sub-line textbox
    ColourRgba default_fill;     // box fill captured at bind time (restored when untinted)
    const UIPalette *palette;    // owning host's palette (tint and ink colours)
} PanelHeader;

// Resolve an ENTITY ref to its live entity, or NULL for a wrong-kind or stale ref.
Newtonoid2d *PanelShared_ResolveEntityRef(TargetRef ref);

// Collapse a tally to the toggle state: 0 off, 1 all on, else a packed "<on>/<applicable>" count.
int PanelShared_TriState(int applicable, int on);

// Read / write the bitmask a family targets on one selection member. False for a wrong-kind or stale ref.
bool PanelShared_ReadFlags(TargetRef ref, FlagFamily family, uint32_t *out);
bool PanelShared_WriteFlags(TargetRef ref, FlagFamily family, uint32_t value);

// Visit every applicable member of a family, tallying `bit`; when `apply` is true also writes `value`.
// The WORLD family falls back to the context world when the set holds no world and no cell.
FlagTally PanelShared_WalkFlag(FlagFamily family, uint32_t bit, bool apply, bool value);

// Is-set query shared by every flag family (key from PANEL_FLAG_KEY): 0 off, 1 on, else mixed pack.
BindingValue PanelShared_QueryFlag(int key);

// Acquire each id's button from the loaded tree and wire the click handler plus the "<label>: ON/OFF" query.
void PanelShared_WireFlags(UIElement *root, FlagButton *buttons, const char **ids, size_t count,
                           FlagFamily family);

// Click handler for every flag button: clears the flag when on for every applicable target, else sets it.
void PanelShared_HandleFlagClick(UIElement *button);

// Bind a header block to its three markup ids (FONT_LARGE title) and capture the default fill.
// `palette` is the owning host's palette (NULL uses the active palette).
void PanelShared_HeaderBind(PanelHeader *header, UIElement *root, const UIPalette *palette,
                            const char *box_id, const char *title_id, const char *sub_id);

// Tint the header amber (several targets) or restore the default fill; ink is the dark text colour.
void PanelShared_HeaderSetTint(PanelHeader *header, bool tinted);

// Lock (read-only) or unlock editable fields by swapping SAFE_IO <-> O. `cached` holds the last state.
void PanelShared_SetFieldsLocked(UIElement **fields, size_t count, bool *cached, bool locked);

// Set a button's authored and drawn text together (no binding on the button).
void PanelShared_SetButtonText(UIElement *button, const char *text);

// Show exactly one of a host's views by index (-1 shows none).
void PanelShared_ShowOnlyView(ViewHostSystem *host, int view_index);

// Last member of `kind` in the SelectionSet (the "primary of that kind"). False when none.
bool PanelShared_PrimaryOfKind(TargetKind kind, TargetRef *out);

// Number of SelectionSet members of `kind`.
int PanelShared_CountKind(TargetKind kind);

#endif

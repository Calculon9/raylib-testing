/**********************************************************************************************
*
DETAIL STRIP SYSTEM MODULE (implementation)
*
Loads detail_strip.xml, mounts it as a single-view ViewHostSystem against the detail-strip
viewport, and feeds four read-only BIND_STRING labels describing the CURRENT detail context. For
v1 the context is ALWAYS the active prefab, resolved via Catalogue_GetParams(UIState_GetActivePrefab()).
The single indirection DetailStrip_ActivePrefabParams is the seam where a future context (selected
tool, world prefab) would be added WITHOUT a provider framework — branch that one function and add
fields/shims. When no prefab context applies (empty catalogue) each field shows a neutral em-dash
placeholder, so the strip is always present with a stable height.
*
**********************************************************************************************/
#include "system/ui/detail_strip_system.h"

#include <string.h>
#include <stdio.h> // snprintf

#include "system/ui/ui_loader.h"
#include "system/view_host_system.h"
#include "system/viewport_system.h"
#include "system/ui_system.h"
#include "ui/ui.h"
#include "ui/binding.h"
#include "entities/entity_catalogue.h" // Catalogue_GetParams / Catalogue_GetId
#include "entities/entity_factory.h"   // EntityCreateParams / CREATION_COMPONENT_*
#include "math/geometry.h"             // ShapeType enum
#include "common/common.h"             // LOG_ERROR

// Em-dash placeholder shown when no prefab context applies (empty catalogue).
#define DETAIL_STRIP_EMPTY "\xE2\x80\x94"

// ============================================================================
// Panel state (module-owned)
// ============================================================================
static ViewHostSystem *detail_strip = NULL;

// Panel-owned backing buffers for the BIND_STRING query results (BIND_STRING lifetime — never
// stack locals). The binding refresh copies value.as.s during its call, so the pointer must stay
// valid; panel-owned storage mirrors the info strip's shims.
static char detail_id_text[64];
static char detail_type_text[64];
static char detail_components_text[96];
static char detail_physics_text[96];

// ============================================================================
// v1 context resolution (private seam)
// ============================================================================

// Resolve the current detail context. For now the context is ALWAYS the active prefab; this single
// indirection is the seam where a future context (selected tool, world prefab) would branch WITHOUT
// a provider framework. Returns the active prefab's params (NULL when the catalogue is empty) and,
// when out_id is non-NULL, writes the active prefab's id ("" when empty).
static const EntityCreateParams *DetailStrip_ActivePrefabParams(const char **out_id)
{
    int idx = UIState_GetActivePrefab();
    const EntityCreateParams *p = Catalogue_GetParams(idx); // NULL when catalogue empty.
    if (out_id)
    {
        *out_id = Catalogue_GetId(idx); // "" when empty.
    }
    return p;
}

// Map a ShapeType to a short label. There is NO existing ShapeType->string helper in the tree, so
// this file-local map covers every ShapeType value (SHAPE_AUTO .. SHAPE_GEAR) with a "?" default.
// It is a pure enum->string function, added here rather than to the physics/geometry module.
static const char *DetailStrip_ShapeName(ShapeType shape)
{
    switch (shape)
    {
        case SHAPE_AUTO:                 return "auto";
        case SHAPE_TRIANGLE:             return "triangle";
        case SHAPE_SQUARE:               return "square";
        case SHAPE_CIRCLE:               return "circle";
        case SHAPE_ELLIPSE:              return "ellipse";
        case SHAPE_POLYGON:              return "polygon";
        case SHAPE_RECTANGLE:            return "rectangle";
        case SHAPE_TRIANGLE_EQUILATERAL: return "triangle-eq";
        case SHAPE_TRIANGLE_ISOSCELES:   return "triangle-iso";
        case SHAPE_ARROW:                return "arrow";
        case SHAPE_ROTOR:                return "rotor";
        case SHAPE_GEAR:                 return "gear";
        default:                         return "?";
    }
}

// ============================================================================
// Binding query shims (return BIND_STRING, mirroring the info strip's shims)
// ============================================================================

// "Prefab: <id>" — the active prefab's id; em-dash placeholder when no context applies. Returns a
// BIND_STRING value pointing at the panel-owned buffer; the key is unused.
static BindingValue DetailStrip_QueryPrefabId(int key)
{
    (void)key;
    const char *id = NULL;
    const EntityCreateParams *p = DetailStrip_ActivePrefabParams(&id);
    if (!p)
    {
        snprintf(detail_id_text, sizeof(detail_id_text), "Prefab: " DETAIL_STRIP_EMPTY);
    }
    else
    {
        snprintf(detail_id_text, sizeof(detail_id_text), "Prefab: %s",
                 (id && *id) ? id : DETAIL_STRIP_EMPTY);
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = detail_id_text };
}

// "Type: <shape>" — the active prefab's physics.shape_type as a short label; em-dash when empty.
static BindingValue DetailStrip_QueryType(int key)
{
    (void)key;
    const EntityCreateParams *p = DetailStrip_ActivePrefabParams(NULL);
    if (!p)
    {
        snprintf(detail_type_text, sizeof(detail_type_text), "Type: " DETAIL_STRIP_EMPTY);
    }
    else
    {
        snprintf(detail_type_text, sizeof(detail_type_text), "Type: %s",
                 DetailStrip_ShapeName(p->physics.shape_type));
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = detail_type_text };
}

// "Has: portal rotor gear health" — the component flags present on the active prefab; "Has: —" when
// none or no context. Writes are length-checked so the panel-owned buffer never overflows.
static BindingValue DetailStrip_QueryComponents(int key)
{
    (void)key;
    const EntityCreateParams *p = DetailStrip_ActivePrefabParams(NULL);
    if (!p)
    {
        snprintf(detail_components_text, sizeof(detail_components_text), "Has: " DETAIL_STRIP_EMPTY);
        return (BindingValue){ .type = BIND_STRING, .as.s = detail_components_text };
    }

    int n = snprintf(detail_components_text, sizeof(detail_components_text), "Has:");
    if (p->component_flags & CREATION_COMPONENT_PORTAL)
    {
        n += snprintf(detail_components_text + n, sizeof(detail_components_text) - (size_t)n, " portal");
    }
    if (p->component_flags & CREATION_COMPONENT_ROTOR)
    {
        n += snprintf(detail_components_text + n, sizeof(detail_components_text) - (size_t)n, " rotor");
    }
    if (p->component_flags & CREATION_COMPONENT_GEAR)
    {
        n += snprintf(detail_components_text + n, sizeof(detail_components_text) - (size_t)n, " gear");
    }
    if (p->component_flags & CREATION_COMPONENT_HEALTH)
    {
        n += snprintf(detail_components_text + n, sizeof(detail_components_text) - (size_t)n, " health");
    }
    if (p->component_flags == CREATION_COMPONENT_NONE)
    {
        snprintf(detail_components_text + n, sizeof(detail_components_text) - (size_t)n, " " DETAIL_STRIP_EMPTY);
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = detail_components_text };
}

// "Size: w×h  m=<mass>" — a couple of key physics values from params.physics (width, height, mass).
// The remaining Newtonoid2dParams fields are intentionally not shown. Em-dash when no context.
static BindingValue DetailStrip_QueryPhysics(int key)
{
    (void)key;
    const EntityCreateParams *p = DetailStrip_ActivePrefabParams(NULL);
    if (!p)
    {
        snprintf(detail_physics_text, sizeof(detail_physics_text), "Size: " DETAIL_STRIP_EMPTY);
    }
    else
    {
        snprintf(detail_physics_text, sizeof(detail_physics_text), "Size: %.0f\xC3\x97%.0f  m=%.2f",
                 (double)p->physics.width, (double)p->physics.height, (double)p->physics.mass);
    }
    return (BindingValue){ .type = BIND_STRING, .as.s = detail_physics_text };
}

// ============================================================================
// UI loader resolvers (application-specific)
// ============================================================================

// Resolve the strip's binding names to query sources returning BIND_STRING values. Four names are
// recognised: "detailstrip.prefab_id", "detailstrip.type", "detailstrip.components" and
// "detailstrip.physics"; anything else is unresolved. Structure mirrors InfoStrip_ResolveBinding.
static UIBinding DetailStrip_ResolveBinding(const char *binding_string, UILoaderContext *ctx)
{
    (void)ctx;

    if (!binding_string)
    {
        return (UIBinding){ .kind = UI_BIND_SRC_NONE };
    }

    if (!strcmp(binding_string, "detailstrip.prefab_id"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = DetailStrip_QueryPrefabId,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "detailstrip.type"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = DetailStrip_QueryType,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "detailstrip.components"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = DetailStrip_QueryComponents,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    if (!strcmp(binding_string, "detailstrip.physics"))
    {
        return (UIBinding){
            .kind = UI_BIND_SRC_QUERY,
            .query = DetailStrip_QueryPhysics,
            .query_key = 0,
            .value_type = BIND_STRING,
        };
    }

    return (UIBinding){ .kind = UI_BIND_SRC_NONE };
}

// The strip has no command buttons; every action name resolves to none.
static int DetailStrip_ResolveCommand(const char *cmd_string, UILoaderContext *ctx)
{
    (void)cmd_string;
    (void)ctx;
    return 0;
}

// ============================================================================
// Lifecycle
// ============================================================================

// Load detail_strip.xml and mount it as a single-view strip against the detail-strip viewport.
void InitDetailStrip(void)
{
    if (detail_strip)
    {
        return;
    }

    // 1) Load the authoritative markup with the strip's own resolvers. The path is the main-repo
    //    component location, matching the convention used by info_strip_system.c / toolbar_system.c
    //    / state_manager_system.c (they all load from src/engine/ui/components/).
    UIElement *root = UILoader_LoadFromFileWithResolvers(
        "C:\\Projects\\raylib-testing\\src\\engine\\ui\\components\\detail_strip.xml",
        &ui_default_palette,
        DetailStrip_ResolveBinding,
        DetailStrip_ResolveCommand,
        NULL,
        NULL);

    if (!root)
    {
        // XML is the single authoritative source; a load failure is an asset/build error.
        LOG_ERROR("InitDetailStrip: failed to load detail_strip.xml; detail strip will not render");
        return;
    }

    // 2) Build the host against the dedicated detail-strip viewport band (carved from the bottom of
    //    the central column by InitViewportLayout), build its real root, and re-parent the XML
    //    container under it. Mounting against detail_strip_viewport keeps the strip in its own
    //    reserved screen region so it never overlaps the world render area.
    detail_strip = ViewHostSystem_Create(&detail_strip_viewport, 1.0f, ZERO_VECTOR_2D,
                                         &ui_default_palette, ui_standard_stack_spacing);
    if (!detail_strip)
    {
        DisposeUIElement(root);
        return;
    }

    ViewHostSystem_InitRoot(detail_strip);      // Real UI_ELEMENT_ROOT + space + seed_box.
    AddElementToTree(root, detail_strip->root);  // Re-parent XML container under the real root.

    // 3) Final layout update over the host's valid coordinate space.
    UpdateUISpace(detail_strip->root, detail_strip->seed_box);
}

// Draw the detail strip (null-guarded so a load failure degrades to no rendering).
void DrawDetailStrip(void)
{
    if (!detail_strip)
    {
        return;
    }

    ViewHostSystem_Draw(detail_strip);
}

// Destroy the detail strip and clear its cached references.
void DestroyDetailStrip(void)
{
    ViewHostSystem *panel = detail_strip;
    detail_strip = NULL;
    ViewHostSystem_Destroy(panel);

    // Reset the shim buffers so a later re-init starts clean (no stale label).
    detail_id_text[0] = '\0';
    detail_type_text[0] = '\0';
    detail_components_text[0] = '\0';
    detail_physics_text[0] = '\0';
}

/**
 * Generic XML UI tree loader implementation.
 * Factory-registry pattern for declarative UI markup.
 */

#include "system/ui/ui_loader.h"
#include "system/command_system.h"
#include "system/view_host_system.h"
#include "ui/ui_constructors.h"
#include "system/utility_system.h"
#include "memory/cmemory.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

// ============================================================================
// Element Builder Registry
// ============================================================================

// Maximum number of registered element types
#define MAX_REGISTERED_BUILDERS 64

// Simple error logging helper
#define LOADER_WARNING(ctx, msg) UILoader_LogWarning((ctx), (msg), __func__)
#define LOADER_ERROR(ctx, msg) UILoader_LogError((ctx), (msg), __func__)
#define LOADER_LOG(ctx, ...) UILoader_Log((ctx), __func__, __VA_ARGS__)

// Logging helpers (forward-declared so the per-load side-table helpers below, which use the
// LOADER_WARNING macro, resolve before their definitions appear further down).
static void UILoader_LogWarning(UILoaderContext *ctx, const char *msg, const char *func);
static void UILoader_LogError(UILoaderContext *ctx, const char *msg, const char *func);
static void UILoader_Log(UILoaderContext *ctx, const char *func, const char *format, ...);

static struct {
    UIElementBuilderRegistry entries[MAX_REGISTERED_BUILDERS];
    int count;
} g_builder_registry = {0};

// ============================================================================
// Per-Load Markup Side Table
// ============================================================================
//
// The <ViewSelector>/<Option>/<View> intent cannot be fully wired during the
// single parse pass: Options precede Views in document order and the host's view
// array does not exist until after the whole tree is built. During parsing the
// builders therefore record just enough metadata here (per View: its container
// pointer, resolved ViewType and id; per Option: its button pointer and target
// view id; plus the selector container and the ViewHost's initialView), and the
// post-process functions below consume it. The table is loader-internal,
// fixed-capacity, and cleared at the start of every load so no state leaks
// between loads.

// Maximum number of Views / Options captured per load.
#define MAX_LOADED_VIEWS 32

// Captured <View>: links a view container to its resolved type and id.
typedef struct {
    UIElement *container;           // The UI_ELEMENT_VIEW container (same pointer View->container holds)
    ViewType view_type;             // Resolved ViewType from type=
    char id[MAX_UI_ELEMENT_ID];     // Preserved id= for Option/initialView resolution
    bool scrollable_y;              // Vertical scrollability from scrollable= (default false)
} LoadedViewEntry;

// Captured <Option>: links an enumerate button to the View id it targets.
typedef struct {
    UIElement *button;              // The enumerate button built for this Option
    char view_id[MAX_UI_ELEMENT_ID]; // Target View id from view=
} LoadedOptionEntry;

static struct {
    LoadedViewEntry views[MAX_LOADED_VIEWS];
    size_t view_count;
    LoadedOptionEntry options[MAX_LOADED_VIEWS];
    size_t option_count;
    UIElement *selector_cont;                   // The <ViewSelector> toggle-bar container
    char initial_view_id[MAX_UI_ELEMENT_ID];    // The <ViewHost> initialView attribute
} g_markup_table = {0};

/**
 * Reset the per-load markup side table.
 * Called at the start of every load entry point so no cross-load state leaks.
 */
static void UILoader_ResetMarkupTable(void)
{
    memset(&g_markup_table, 0, sizeof(g_markup_table));
}

/**
 * Record a captured View (container, resolved type, id) in the side table.
 * Overflow beyond MAX_LOADED_VIEWS is warned and ignored.
 */
static void UILoader_RecordView(UILoaderContext *ctx, UIElement *container,
                                ViewType view_type, const char *id, bool scrollable_y)
{
    if (g_markup_table.view_count >= MAX_LOADED_VIEWS)
    {
        LOADER_WARNING(ctx, "View table overflow; ignoring excess view");
        return;
    }

    LoadedViewEntry *entry = &g_markup_table.views[g_markup_table.view_count++];
    entry->container = container;
    entry->view_type = view_type;
    safe_strncpy(entry->id, id ? id : "", MAX_UI_ELEMENT_ID);
    entry->scrollable_y = scrollable_y;
}

/**
 * Look up the recorded vertical scrollability for a given view container pointer.
 * Defaults to false if the container is not found in the side table.
 */
static bool UILoader_LookupViewScrollable(const UIElement *container)
{
    for (size_t i = 0; i < g_markup_table.view_count; i++)
    {
        if (g_markup_table.views[i].container == container)
        {
            return g_markup_table.views[i].scrollable_y;
        }
    }
    return false;
}

/**
 * Record a captured Option (button, target view id) in the side table.
 * Overflow beyond MAX_LOADED_VIEWS is warned and ignored.
 */
static void UILoader_RecordOption(UILoaderContext *ctx, UIElement *button,
                                  const char *view_id)
{
    if (g_markup_table.option_count >= MAX_LOADED_VIEWS)
    {
        LOADER_WARNING(ctx, "Option table overflow; ignoring excess option");
        return;
    }

    LoadedOptionEntry *entry = &g_markup_table.options[g_markup_table.option_count++];
    entry->button = button;
    safe_strncpy(entry->view_id, view_id ? view_id : "", MAX_UI_ELEMENT_ID);
}

/**
 * Look up the recorded ViewType for a given view container pointer.
 * Defaults to the first ViewType (0) with a warning if the container is not found.
 */
static ViewType UILoader_LookupViewType(UILoaderContext *ctx, const UIElement *container)
{
    for (size_t i = 0; i < g_markup_table.view_count; i++)
    {
        if (g_markup_table.views[i].container == container)
        {
            return g_markup_table.views[i].view_type;
        }
    }

    LOADER_WARNING(ctx, "View container not found in table; defaulting view type");
    return (ViewType)0;
}

// ============================================================================
// Forward Declarations
// ============================================================================

static UIElement *UILoader_ParseElementNode(mxml_node_t *node, UIElement *parent, UILoaderContext *ctx);

// Parser helper functions
static const Spacing *UILoader_ParseLayout(const char *layout_str);

// ============================================================================
// Logging and Validation
// ============================================================================

/**
 * Log a formatted informational message during XML loading.
 */
static void UILoader_Log(UILoaderContext *ctx, const char *func, const char *format, ...)
{
    if (!format)
        return;

    const char *source = ctx && ctx->source_file ? ctx->source_file : "<unknown>";
    char msg[512];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    LOG_INFO("UI Loader: %s (source: %s, function: %s)", msg, source, func);
}

/**
 * Log a warning message during XML loading.
 * Used to report issues like missing bindings, unknown actions, or unrecognized attributes.
 */
static void UILoader_LogWarning(UILoaderContext *ctx, const char *msg, const char *func)
{
    if (!msg)
        return;
    
    const char *source = ctx && ctx->source_file ? ctx->source_file : "<unknown>";
    // TODO: Replace with actual logging system (DEBUG_LOG, printf, etc.)
    
    LOG_WARN("UI Loader Warning: %s (source: %s, function: %s)", msg, source, func);

    (void)source;
    (void)func;
}

/**
 * Log an error message during XML loading.
 * Used to report fatal issues like missing required attributes or parse failures.
 */
static void UILoader_LogError(UILoaderContext *ctx, const char *msg, const char *func)
{
    if (!msg)
        return;
    
    const char *source = ctx && ctx->source_file ? ctx->source_file : "<unknown>";
    // TODO: Replace with actual logging system (DEBUG_LOG, fprintf(stderr, ...), etc.)

    LOG_ERROR("UI Loader Error: %s (source: %s, function: %s)", msg, source, func);
    
    (void)source;
    (void)func;
}

/**
 * Validate that a required attribute is present and non-empty.
 * Logs error if missing.
 * 
 * @return true if attribute exists and is non-empty, false otherwise
 */
static bool UILoader_ValidateRequired(mxml_node_t *node, const char *attr_name,
                                      UILoaderContext *ctx, const char *element_tag)
{
    const char *value = mxmlElementGetAttr(node, attr_name);
    if (!value || value[0] == '\0')
    {
        // Log error but don't fatal; allow builder to handle gracefully
        return false;
    }
    return true;
}

// ============================================================================
// Built-in Element Builders
// ============================================================================

/**
/**
 * Command-dispatch adapter matching BindingCommandFn (void(int, const void *)).
 *
 * The binding core is intentionally free of any command-system dependency: it stores a command
 * as an opaque int code plus a function pointer. This adapter is the UI-side bridge that casts
 * the opaque code back to a CommandType and dispatches it. Using a real adapter (rather than
 * casting ExecuteCommand to the fn-pointer type) keeps the call type-correct.
 */
static void UILoader_DispatchCommand(int code, const void *data)
{
    ExecuteCommand((CommandType)code, data);
}

/**
 * Generic click handler for declarative (XML) command buttons.
 *
 * Matches the UIEventHandler signature (void(UIElement *)) that the UI input dispatcher
 * invokes. Dispatches the button's command through its attached binding's command-sink,
 * the same symmetric binding path used for textbox read/write.
 */
static void UILoader_HandleCommandClick(UIElement *e)
{
    if (!e || !e->data.button.binding)
        return;

    // A command sink ignores the written value, so a BIND_NONE value is sufficient to trigger
    // dispatch; Binding_WriteSink routes it to the configured command fn + code.
    BindingValue none = {0};
    Binding_WriteSink(&e->data.button.binding->sink, none);
}

/**
 * Map a loader DataType to the canonical BindingValueType.
 *
 * This is the single intentional duplication of integration_system.c's file-static
 * ResolveBindingType: it keeps the loader self-contained and domain-free (it never
 * reaches across translation units for the mapping). Used only for an ADDRESS button
 * source, where the resolver reports a DataType rather than a BindingValueType.
 */
static BindingValueType UILoader_MapDataType(DataType type)
{
    switch (type)
    {
        case INT:       return BIND_INT;
        case FLOAT:     return BIND_FLOAT;
        case VECTOR2D:  return BIND_VECTOR2D;
        case STRING64:
        case STRING128:
        case STRING256: return BIND_STRING;
        default:        return BIND_NONE;
    }
}

/**
 * Builder for <Button> elements.
 * Attributes: text (required), type (optional: "simple"/"enumerate"/"submit"),
 *             binding (optional source), action (optional sink), size (optional),
 *             width/height (optional per-axis mode "fixed|percent|fill|hug").
 *
 * Assembles ONE Binding carrying BOTH halves: a display SOURCE from binding= (resolved
 * via resolve_binding into an address or query source) and a commit SINK from action=
 * (resolved via resolve_action, else the legacy resolve_command, else a direct integer
 * code). A button with only action= is byte-identical to the previous behaviour; a
 * button with only binding= becomes a read-only live label; a button with neither gets
 * no Binding and no click handler.
 */
static UIBuildResult BuildButton(mxml_node_t *node, UIElement *parent, const UIPalette *palette,
                                 UILoaderContext *ctx, UICommonAttrs *common)
{
    const char *text = mxmlElementGetAttr(node, "text");
    if (!text)
        text = "";
    
    const char *type_attr = mxmlElementGetAttr(node, "type");
    UIElementType btn_type = UI_ELEMENT_BUTTON_SIMPLE;
    if (type_attr)
    {
        if (!strcmp(type_attr, "enumerate"))
            btn_type = UI_ELEMENT_BUTTON_ENUMERATE;
        else if (!strcmp(type_attr, "submit"))
            btn_type = UI_ELEMENT_BUTTON_SUBMIT;
    }

    // Assemble a single Binding carrying both a display source (binding=) and a commit
    // sink (action=). have_source/have_sink gate whether a Binding is attached and whether
    // a click handler is wired, preserving today's "sink => handler" rule.
    Binding b = {0};
    bool have_source = false;
    bool have_sink = false;

    // SOURCE from binding= (new). Resolve to an address or query source descriptor.
    const char *binding_attr = mxmlElementGetAttr(node, "binding");
    if (binding_attr && ctx && ctx->resolve_binding)
    {
        UIBinding src = ctx->resolve_binding(binding_attr, ctx);

        // Honour the legacy implicit contract: a non-NULL address with kind NONE means an
        // address source (an unmodified address resolver zero-inits the appended tail).
        UIBindingSourceKind k = src.kind;
        if (k == UI_BIND_SRC_NONE && src.address)
            k = UI_BIND_SRC_ADDRESS;

        if (k == UI_BIND_SRC_ADDRESS && src.address)
        {
            b.source.kind = BIND_SRC_ADDRESS;
            b.source.value_type = UILoader_MapDataType(src.data_type);
            b.source.address = src.address;
            have_source = true;
        }
        else if (k == UI_BIND_SRC_QUERY && src.query)
        {
            b.source.kind = BIND_SRC_QUERY;
            b.source.value_type = src.value_type; // advisory; query owns the actual type
            b.source.query = src.query;
            b.source.query_key = src.query_key;
            have_source = true;
        }
        else
        {
            LOADER_WARNING(ctx, "Binding did not resolve");
        }
    }
    else if (binding_attr)
    {
        LOADER_WARNING(ctx, "Binding specified but resolver not available");
    }

    // SINK from action= (existing command path + new opt-in callback path).
    const char *action_attr = mxmlElementGetAttr(node, "action");
    if (action_attr && ctx && ctx->resolve_action)
    {
        // (1) Opt-in widened sink resolver: command OR callback.
        UIAction action = ctx->resolve_action(action_attr, ctx);
        if (action.kind == UI_BIND_SINK_COMMAND && action.command_code != 0)
        {
            b.sink.kind = BIND_SINK_COMMAND;
            b.sink.command = UILoader_DispatchCommand;
            b.sink.command_code = action.command_code;
            have_sink = true;
        }
        else if (action.kind == UI_BIND_SINK_CALLBACK && action.write)
        {
            // Callback sinks also use UILoader_HandleCommandClick; rename deferred - see design section 7.
            b.sink.kind = BIND_SINK_CALLBACK;
            b.sink.write = action.write;
            b.sink.write_key = action.write_key;
            b.sink.value_type = action.value_type;
            have_sink = true;
        }
        else
        {
            LOADER_WARNING(ctx, "Unrecognised action string");
        }
    }
    else if (action_attr && ctx && ctx->resolve_command)
    {
        // (2) Legacy command-sink resolver (unchanged path).
        int command_code = ctx->resolve_command(action_attr, ctx);
        if (command_code != 0)
        {
            b.sink.kind = BIND_SINK_COMMAND;
            b.sink.command = UILoader_DispatchCommand;
            b.sink.command_code = command_code;
            have_sink = true;
        }
        else
        {
            LOADER_WARNING(ctx, "Unrecognised action string");
        }
    }
    else if (action_attr)
    {
        // (3) No resolver available: try parsing the action as a direct integer code.
        char *endptr;
        long parsed = strtol(action_attr, &endptr, 10);

        if (*endptr == '\0' && parsed > 0)
        {
            b.sink.kind = BIND_SINK_COMMAND;
            b.sink.command = UILoader_DispatchCommand;
            b.sink.command_code = (int)parsed;
            have_sink = true;
        }
        else
        {
            LOADER_WARNING(ctx, "Action value not a valid integer");
        }
    }

    // Attach the generic command handler only when a sink exists; a source-only (read-only
    // label) or action-less button gets no handler, matching today's command-code != 0 rule.
    UIEventHandler handler = have_sink ? UILoader_HandleCommandClick : NULL;

    // Size comes from the common pass (seeded with ui_standard_button_size). The button does
    // not take an offset constructor arg, so offset stays applied POST by the skeleton
    // (offset_consumed left false); enabled is applied POST by the skeleton too.
    UIElement *button = CreateUIButtonDefault(parent, btn_type, text, common->size,
                                              ui_standard_button_padding, palette, handler, NULL, NULL);

    // Attach the assembled Binding when either half resolved. A command/callback sink is
    // fired by UILoader_HandleCommandClick; a source drives the generic refresh walk.
    // The binding is freed by DisposeUIElement's button-binding cleanup.
    if (button && (have_source || have_sink))
    {
        button->data.button.binding = Binding_Create(b);
    }

    // Single-element builder: control == outer (bind_target NULL). The skeleton stamps
    // id/enabled/offset on the button and recurses children into it, exactly as before.
    return (UIBuildResult){ .element = button };
}

/**
 * Builder for <Label> elements.
 * Attributes: text (required)
 */
static UIBuildResult BuildLabel(mxml_node_t *node, UIElement *parent, const UIPalette *palette,
                                UILoaderContext *ctx, UICommonAttrs *common)
{
    const char *text = mxmlElementGetAttr(node, "text");
    if (!text)
        text = "";

    // Size comes from the common pass (seeded with ui_standard_control_size). Offset is
    // applied POST by the skeleton (not consumed) and enabled by the skeleton too.
    UIElement *label_elem = CreateUILabelDefault(parent, text, common->size,
                               ui_standard_button_padding, palette);

    // Single-element builder: control == outer. id/offset/enabled/size land on the label.
    return (UIBuildResult){ .element = label_elem };
}

/**
 * Builder for <TextField> elements.
 * Attributes: label (required), type (integer/float/vector2), binding (optional source,
 *             optionally a dynamic write sink), io (optional: "readonly"/"safe"/"raw"),
 *             size (optional "w,h"), width/height (optional per-axis mode
 *             "fixed|percent|fill|hug"), offset (optional "x,y"),
 *             offset-mode (optional "fixed"/"percent"), enabled (optional).
 * size/width/height and offset/offset-mode default to ui_standard_control_size and a zero
 * fixed offset when absent, so attribute-less markup is unchanged.
 *
 * Resolves binding= through ctx->resolve_binding and ATTACHES the result to the created
 * textbox (0.1): an address source becomes a read-only live label (BIND_SRC_ADDRESS, no
 * sink - see design section 1 sink policy); a query source becomes a live display
 * (BIND_SRC_QUERY) that, when the resolver also returns a non-NULL write, becomes editable
 * via a BIND_SINK_CALLBACK (0.2). This mirrors BindTextboxStable / BindTextboxDynamic in
 * integration_system.c. The io= attribute (0.4) selects the textbox IO type, defaulting to
 * UI_ELEMENT_TEXTBOX_IO so existing markup is byte-for-byte unchanged. The loader stays
 * domain-free: it only ever handles function pointers, opaque int keys, and generic types.
 */
/**
 * Map a <TextField>/<Textbox> type= attribute to the DataType the field parses/formats with.
 * Absent/unrecognised => FLOAT (the historic default), matching the pre-refactor behaviour.
 */
static DataType UILoader_ParseFieldDataType(const char *type_attr)
{
    if (type_attr)
    {
        if (!strcmp(type_attr, "integer"))
            return INT;
        else if (!strcmp(type_attr, "float"))
            return FLOAT;
        else if (!strcmp(type_attr, "vector2"))
            return VECTOR2D;
    }
    return FLOAT;  // Default data type
}

/**
 * Map a <TextField>/<Textbox> io= attribute to the textbox IO element type.
 * Absent => UI_ELEMENT_TEXTBOX_IO (raw), the exact value hardcoded before the common-builder
 * refactor, so existing markup builds unchanged. An unrecognised value falls back to raw and
 * warns (consistent with BuildViewSelector). Shared by BuildTextField and BuildTextbox so the
 * two stay in lockstep.
 */
static UIElementType UILoader_ParseFieldIoType(UILoaderContext *ctx, const char *io_attr)
{
    UIElementType field_type = UI_ELEMENT_TEXTBOX_IO;
    if (io_attr)
    {
        if (!strcmp(io_attr, "readonly"))
            field_type = UI_ELEMENT_TEXTBOX_O;
        else if (!strcmp(io_attr, "safe"))
            field_type = UI_ELEMENT_TEXTBOX_SAFE_IO;
        else if (!strcmp(io_attr, "raw"))
            field_type = UI_ELEMENT_TEXTBOX_IO;
        else
            LOADER_WARNING(ctx, "Unrecognised io value");
    }
    return field_type;
}

/**
 * Resolve a binding= attribute and assemble the textbox source/sink into `b`.
 *
 * Shared verbatim by BuildTextField and BuildTextbox so the two never drift. A source
 * (address or query) drives the live display via the per-frame refresh walk; an optional
 * query-paired write callback makes the field editable (commit-on-ENTER through
 * Binding_Commit). Mirrors BuildButton's source logic exactly.
 *
 * @param node          XML element node to read binding= from
 * @param ctx           Loader context (supplies resolve_binding; may be NULL)
 * @param b             Output Binding to populate (source/sink written in place)
 * @param have_source   Output: set true when a display source resolved
 * @param have_sink     Output: set true when a dynamic write sink resolved
 * @param data_type     In/out: parse/format DataType, overwritten to match an address source
 */
static void UILoader_AssembleTextboxBinding(mxml_node_t *node, UILoaderContext *ctx,
                                            Binding *b, bool *have_source, bool *have_sink,
                                            DataType *data_type)
{
    *have_source = false;
    *have_sink = false;

    const char *binding_attr = mxmlElementGetAttr(node, "binding");
    if (binding_attr && ctx && ctx->resolve_binding)
    {
        UIBinding src = ctx->resolve_binding(binding_attr, ctx);

        // Honour the legacy implicit contract: a non-NULL address with kind NONE means an
        // address source (an unmodified address resolver zero-inits the appended tail).
        UIBindingSourceKind k = src.kind;
        if (k == UI_BIND_SRC_NONE && src.address)
            k = UI_BIND_SRC_ADDRESS;

        if (k == UI_BIND_SRC_ADDRESS && src.address)
        {
            // Address source: read-only live label. Interpret the deref via the loader's
            // DataType -> BindingValueType map. An unmapped DataType (BIND_NONE) attaches
            // nothing and warns, mirroring BindTextboxStable's BINDING_NONE early-out.
            BindingValueType vt = UILoader_MapDataType(src.data_type);
            if (vt == BIND_NONE)
            {
                LOADER_WARNING(ctx, "Binding did not resolve");
            }
            else
            {
                b->source.kind = BIND_SRC_ADDRESS;
                b->source.value_type = vt;
                b->source.address = src.address;
                *data_type = src.data_type; // keep parse/format in step with the source
                *have_source = true;
            }
        }
        else if (k == UI_BIND_SRC_QUERY && src.query)
        {
            // Query source: live display driven by the query fn. The query owns the real
            // type; value_type is advisory.
            b->source.kind = BIND_SRC_QUERY;
            b->source.value_type = src.value_type;
            b->source.query = src.query;
            b->source.query_key = src.query_key;
            *have_source = true;
        }
        else
        {
            LOADER_WARNING(ctx, "Binding did not resolve");
        }

        // 0.2 - Optional dynamic WRITE sink. When the resolver returns a non-NULL write,
        // complete the dynamic shape (BindTextboxDynamic) on the SAME Binding: the field
        // commits on ENTER through this callback. The sink's parse type is the source's
        // advisory value_type (a dynamic field reads and writes one type). When write is
        // NULL (every existing resolver, incl. lpanel's {NULL, FLOAT}) no sink is attached
        // and the field stays read-only, exactly as the source-only path leaves it.
        if (src.write)
        {
            b->sink.kind = BIND_SINK_CALLBACK;
            b->sink.value_type = src.value_type;
            b->sink.write = src.write;
            b->sink.write_key = src.write_key;
            *have_sink = true;
        }
    }
    else if (binding_attr)
    {
        // Binding specified but no resolver available.
        LOADER_WARNING(ctx, "Binding specified but resolver not available");
    }
}

static UIBuildResult BuildTextField(mxml_node_t *node, UIElement *parent, const UIPalette *palette,
                                    UILoaderContext *ctx, UICommonAttrs *common)
{
    const char *label = mxmlElementGetAttr(node, "label");
    if (!label)
        label = "";

    // Map type= to the parse/format DataType and io= to the textbox IO element type (shared
    // with BuildTextbox so the two primitives stay in lockstep).
    DataType data_type = UILoader_ParseFieldDataType(mxmlElementGetAttr(node, "type"));
    UIElementType field_type = UILoader_ParseFieldIoType(ctx, mxmlElementGetAttr(node, "io"));

    // 0.1/0.2 - Resolve binding= and assemble a single Binding (shared helper).
    Binding b = {0};
    bool have_source = false;
    bool have_sink = false;
    UILoader_AssembleTextboxBinding(node, ctx, &b, &have_source, &have_sink, &data_type);

    // Size comes from the common pass (seeded with ui_standard_control_size) and is passed as
    // the ROW size. CreateUILabeledFieldDefault builds the UI_ELEMENT_TEXTFIELD row with a
    // label child and a textbox child and returns the TEXTBOX child (its public return is
    // relied upon by InitUIFields, so it is left unchanged). The row is the OUTER element the
    // <TextField> tag represents; the textbox is the inner CONTROL.
    UIElement *textbox = CreateUILabeledFieldDefault(parent, label, field_type,
                                      common->size,
                                      ui_standard_field_padding, palette);
    if (!textbox)
        return (UIBuildResult){ NULL };

    // Derive the row (UI_ELEMENT_TEXTFIELD) as the textbox's parent. CreateLabeledFieldElement
    // always AddElementToTree(textbox, field) before returning, so parent is non-NULL on
    // success; null-check defensively anyway.
    UIElement *row = textbox->parent;
    if (!row)
        return (UIBuildResult){ NULL };

    // Attach the assembled Binding + data_type to the CONTROL (textbox) explicitly when a
    // source OR a sink resolved (mirrors BuildButton's have_source || have_sink gate). The row
    // is a container and has no meaningful data.textbox, so all data/binding writes target the
    // textbox. The precision uses the documented default sentinel (0). The binding is freed by
    // DisposeUIElement's textbox cleanup, as for BindTextboxStable/Dynamic bindings.
    if (have_source || have_sink)
    {
        b.precision = 0;
        textbox->data.textbox.data_type = data_type; // parse/format agrees with the source
        textbox->data.textbox.binding = Binding_Create(b);
    }

    // id-on-control (design section 7): stamp common.id onto the TEXTBOX (control) here so
    // FindById(id) returns the textbox exactly as external code (StateManager) expects. The
    // skeleton skips id-stamping for composites (control != outer), so the row carries no id.
    safe_strncpy(textbox->id, common->id, MAX_UI_ELEMENT_ID);

    // Return the ROW as outer and the TEXTBOX as the control. The skeleton applies
    // offset/enabled to the ROW and recurses children into it, so the textbox KEEPS its
    // constructor-authored {0.38,0} OFFSET_PERCENT and its AXIS_PERCENT split - the fix that
    // makes the label visible again. enabled= is applied POST by the skeleton on the row.
    return (UIBuildResult){ .element = row, .bind_target = textbox };
}

/**
 * Builder for <Textbox> elements (standalone input primitive).
 * Attributes: type (integer/float/vector2), io (optional: "readonly"/"safe"/"raw"),
 *             binding (optional source, optionally a dynamic write sink), size (optional
 *             "w,h"), width/height (optional per-axis mode), offset (optional),
 *             offset-mode (optional), enabled (optional).
 *
 * Constructs a BARE textbox - the SAME UI_ELEMENT_TEXTBOX_* control <TextField> uses
 * internally - with NO label and NO row wrapper, parented straight under `parent`. It is a
 * SINGLE-element builder (control == outer), so id/offset/enabled/size land on the textbox
 * naturally and its binding/data_type target it. io=/type=/binding= are read exactly as
 * BuildTextField does (shared helpers) so the two primitives stay in lockstep.
 */
static UIBuildResult BuildTextbox(mxml_node_t *node, UIElement *parent, const UIPalette *palette,
                                  UILoaderContext *ctx, UICommonAttrs *common)
{
    if (!palette)
        palette = &ui_default_palette;

    // Map type= and io= via the shared helpers (identical mapping to BuildTextField).
    DataType data_type = UILoader_ParseFieldDataType(mxmlElementGetAttr(node, "type"));
    UIElementType field_type = UILoader_ParseFieldIoType(ctx, mxmlElementGetAttr(node, "io"));

    // Resolve binding= and assemble the Binding using the SAME helper as BuildTextField.
    Binding b = {0};
    bool have_source = false;
    bool have_sink = false;
    UILoader_AssembleTextboxBinding(node, ctx, &b, &have_source, &have_sink, &data_type);

    // Derive the input-cell palette colours, mirroring how CreateUILabeledFieldDefault styles
    // the row's textbox child, so a standalone <Textbox> looks like a field input cell.
    ColourRgba input_border;
    ColourRgba input_fill;
    UIPalette_GetSurfaceColours(palette, UI_PALETTE_SURFACE_INPUT, &input_border, &input_fill);

    // Construct a bare textbox under `parent`, mirroring how CreateLabeledFieldElement makes
    // its textbox child (same UI_ELEMENT_TEXTBOX_* type) but with NO row and NO percent split.
    // Size comes from the common pass (seeded with ui_standard_control_size); offset is applied
    // POST by the skeleton (offset_consumed left false) so a zero fixed offset is the default.
    UIElement *textbox = CreateUIElementInTree(field_type, common->size, parent,
                                               (Offset){ZERO_VECTOR_2D, OFFSET_FIXED},
                                               ui_standard_field_padding,
                                               input_border, input_fill);
    if (!textbox)
        return (UIBuildResult){ NULL };

    // Match the row child's cosmetics: medium font in the palette's text colour and
    // vertically-centred text.
    textbox->data.textbox.font = FONT_MEDIUM_WITH_COLOUR(palette->text);
    SetUIElementTextVerticalAlignment(textbox, UI_TEXT_VERTICAL_ALIGN_CENTRE);

    // Attach the Binding + data_type to the textbox (which IS outer here, so the data lands on
    // the live union member and FindById returns this element naturally).
    if (have_source || have_sink)
    {
        b.precision = 0;
        textbox->data.textbox.data_type = data_type;
        textbox->data.textbox.binding = Binding_Create(b);
    }

    // Single-element builder: control == outer. id/offset/enabled/size land on the textbox.
    return (UIBuildResult){ .element = textbox };
}

/**
 * Builder for <Section> elements.
 * Attributes: title (required), id (optional), size (optional "w,h"),
 *             width/height (optional per-axis mode "fixed|percent|fill|hug"),
 *             layout (optional: "stack"/"stack_wrap"/"inline_wrap"),
 *             offset (optional "x,y"), offset-mode (optional "fixed"/"percent")
 * Children are added to the section's container.
 */
static UIBuildResult BuildSection(mxml_node_t *node, UIElement *parent,
                                  const UIPalette *palette, UILoaderContext *ctx,
                                  UICommonAttrs *common)
{
    const char *title = mxmlElementGetAttr(node, "title");
    if (!title)
        title = "";

    // Size, offset and layout all come from the common pass (size seeded with
    // ui_standard_container_size). The offset is passed into CreateViewSection, so mark it
    // consumed and let the skeleton skip the post-apply (exactly-once offset). enabled is
    // applied POST by the skeleton.
    UIElement *section = CreateViewSection(parent, title, common->size, common->offset,
                                           common->layout, palette);
    common->offset_consumed = true;

    // Single-element builder: control == outer (offset consumed at construct time).
    return (UIBuildResult){ .element = section };
}

/**
 * Builder for <Container> elements.
 * Attributes: size (optional "w,h"), width/height (optional per-axis mode
 *             "fixed|percent|fill|hug"), spacing (optional)
 * Children are added to the container.
 */
static UIBuildResult BuildContainer(mxml_node_t *node, UIElement *parent,
                                    const UIPalette *palette, UILoaderContext *ctx,
                                    UICommonAttrs *common)
{
    // Size, offset, layout and enabled all come from the common pass (size seeded with
    // ui_standard_container_size). The offset is passed into CreateUIContainer, so mark it
    // consumed; the skeleton re-asserts the same enabled value afterwards (idempotent).
    UIElement *container = CreateUIContainer(parent, common->size, common->offset,
                            ui_standard_container_padding, palette,
                            UI_PALETTE_SURFACE_CONTAINER, *common->layout,
                            false, common->enabled);
    common->offset_consumed = true;

    // Single-element builder: control == outer (offset consumed at construct time).
    return (UIBuildResult){ .element = container };
}

/**
 * Builder for <View> elements.
 * Attributes: id (optional), type (optional), scrollable (optional), layout (optional),
 *             width/height (optional: "fixed"/"percent"/"fill"/"hug"), size (optional "w,h"),
 *             offset (optional "x,y"), offset-mode (optional), enabled (optional).
 * Children (Sections) are added to the view container.
 *
 * width/height/size and offset default to a fill/fill container and a zero fixed offset
 * when absent (a bare <View> fills its host). The View is independent of any panel and can
 * be registered with a panel later if needed.
 */
static UIBuildResult BuildView(mxml_node_t *node, UIElement *parent,
                               const UIPalette *palette, UILoaderContext *ctx,
                               UICommonAttrs *common)
{
    // The id comes from the common pass (preserved so Options and the ViewHost's initialView
    // can resolve to this View's index after the tree is built). Size/offset/layout/enabled
    // likewise come from common (size seeded with ui_fill_container_size), so a bare <View>
    // keeps its historic "fills the host" behaviour.
    const char *view_id = common->id;

    const char *view_type_str = mxmlElementGetAttr(node, "type");
    const char *scrollable_attr = mxmlElementGetAttr(node, "scrollable");
    // Honour scrollable= when present (default false); applied to the View struct
    // in UILoader_CollectViewContainers once the View wrapper exists.
    bool is_scrollable = scrollable_attr && strcmp(scrollable_attr, "true") == 0;

    // Resolve the type= string to a ViewType through the application-supplied
    // resolver. The generic loader never names application ViewType enums, so an
    // absent resolver or an unrecognised string defaults to the first ViewType (0)
    // with a warning.
    ViewType view_type = (ViewType)0;
    if (view_type_str && ctx && ctx->resolve_view_type)
    {
        bool resolved = false;
        ViewType resolved_type = ctx->resolve_view_type(view_type_str, &resolved, ctx);
        if (resolved)
        {
            view_type = resolved_type;
        }
        else
        {
            LOADER_WARNING(ctx, "Unrecognised view type");
        }
    }
    else if (view_type_str)
    {
        LOADER_WARNING(ctx, "View type specified but resolver not available");
    }

    LOADER_LOG(ctx, "Creating View: id=%s, type=%s, scrollable=%s",
               view_id, view_type_str ? view_type_str : "<none>",
               is_scrollable ? "true" : "false");
    
    // Create container with UI_ELEMENT_VIEW type so post-processor can find it. Size/offset/
    // layout/enabled all come from the common pass; the offset is consumed at construct time
    // so the skeleton does not re-apply it.
    UIElement *container = CreateUIContainer(
        parent,
        common->size,     // honour width=/height=/size= (defaults to fill/fill)
        common->offset,   // honour offset=/offset-mode= (defaults to zero fixed)
        ui_standard_container_padding,
        palette,
        UI_PALETTE_SURFACE_CONTAINER,
        *common->layout,  // honour layout= (defaults to stack)
        false,
        common->enabled   // honour enabled= (skeleton re-asserts the same value)
    );
    common->offset_consumed = true;
    
    if (container)
    {
        container->type = UI_ELEMENT_VIEW;  // Mark for post-processor
        // Record the resolved type, id and scrollability against this exact container
        // pointer so UILoader_CollectViewContainers can stamp the correct View->type
        // and scroll flag, and the post-process pass can resolve Option/initialView
        // ids to view indices.
        UILoader_RecordView(ctx, container, view_type, view_id, is_scrollable);
    }
    
    // Single-element builder: control == outer (offset consumed at construct time).
    return (UIBuildResult){ .element = container };
}

/**
 * Builder for <ViewSelector> elements.
 * Attributes: id (optional), type (optional)
 * Children (Options) are converted to selector buttons.
 */
static UIBuildResult BuildViewSelector(mxml_node_t *node, UIElement *parent,
                                       const UIPalette *palette, UILoaderContext *ctx,
                                       UICommonAttrs *common)
{
    // Build the toggle-bar container, mirroring the styling of
    // ViewHostSystem_CreateStandardViewSelector (transparent surface, container
    // border colour, zero inline spacing). The Option children become enumerate
    // buttons; the actual ViewSelector struct is allocated later by
    // UILoader_BuildSelectorFromMarkup once all Views are known.
    const char *type_attr = mxmlElementGetAttr(node, "type");
    if (type_attr && strcmp(type_attr, "enumerate") != 0)
    {
        // Only "enumerate" selectors are wired; "hover" is reserved for later.
        LOADER_WARNING(ctx, "Unrecognised view selector type; defaulting to enumerate");
    }

    // Size and offset now come from the common pass (folded into the common-owns-it model).
    // The registry seeds common.size with ui_standard_selector_container_size and the skeleton
    // seeds the zero fixed offset, so attribute-less <ViewSelector> markup is byte-for-byte
    // unchanged; only newly-authored size/offset/width/height take effect. The child spacing
    // stays the hardcoded ui_zero_inline_spacing (a fixed horizontal toggle bar - layout
    // authorability for the selector bar is intentionally NOT part of this change). The
    // transparent surface, zero padding and colour_border styling are preserved exactly.
    UIElement *toggle_cont = CreateUIContainer(
        parent, common->size,
        common->offset, ZERO_VECTOR_2D,
        palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_zero_inline_spacing, false, common->enabled);
    // Offset is passed into the constructor, so mark it consumed (no skeleton double-apply).
    common->offset_consumed = true;

    if (toggle_cont)
    {
        toggle_cont->colour_border = palette->container_border;
        // Record the selector container so the post-process pass can locate it.
        g_markup_table.selector_cont = toggle_cont;
    }

    // Single-element builder: control == outer (offset consumed at construct time).
    return (UIBuildResult){ .element = toggle_cont };
}

/**
 * Builder for <Option> elements (inside ViewSelector).
 * Attributes: text (required), view (optional)
 */
static UIBuildResult BuildOption(mxml_node_t *node, UIElement *parent,
                                 const UIPalette *palette, UILoaderContext *ctx,
                                 UICommonAttrs *common)
{
    const char *text = mxmlElementGetAttr(node, "text");
    if (!text)
        text = "";

    // The target View id is recorded for later index resolution. The click
    // handler / data_bind / user_data are intentionally left NULL here: they are
    // wired by UILoader_BuildSelectorFromMarkup once the ViewSelector and its
    // view_indices backing array exist.
    const char *view_attr = mxmlElementGetAttr(node, "view");
    if (!view_attr || view_attr[0] == '\0')
    {
        LOADER_WARNING(ctx, "Option missing view target");
    }

    // Size now comes from the common pass (folded into the common-owns-it model). The
    // registry seeds common.size with ui_standard_selector_button_size, so attribute-less
    // <Option> markup is byte-for-byte unchanged; only newly-authored size/width/height take
    // effect. A button takes no offset constructor arg, so offset is applied POST by the
    // skeleton (offset_consumed left false) - identical to Button/TextField/Label. enabled is
    // likewise applied POST by the skeleton.
    UIElement *button = CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_ENUMERATE, text,
                                             common->size,
                                             ui_standard_button_padding, palette,
                                             NULL, NULL, NULL);

    if (button)
    {
        UILoader_RecordOption(ctx, button, view_attr);
    }

    // Single-element builder: control == outer (offset applied POST by the skeleton).
    return (UIBuildResult){ .element = button };
}

/**
 * Builder for <ViewHost> elements (root of a multi-view panel markup).
 * Attributes: id (optional), layout (optional: "stacked"/"stack"/...),
 *             initial-view (optional), enabled (optional, default true),
 *             width/height (optional: "fixed"/"percent"/"fill"/"hug"), size (optional "w,h"),
 *             offset (optional "x,y"), offset-mode (optional). width/height/size and offset
 *             default to a fill/fill surface and a zero fixed offset when absent.
 *
 * Produces a plain fill-size container (NOT a UI_ELEMENT_ROOT) that becomes the
 * content host re-parented under the real ViewHostSystem root in InitLPanel. The
 * viewport and scale are deliberately NOT expressed in markup: they are
 * application-specific and supplied by the application (InitLPanel), keeping the
 * generic loader free of viewport knowledge. The initial-view id is recorded for
 * the application to resolve to a view index.
 */
static UIBuildResult BuildViewHost(mxml_node_t *node, UIElement *parent,
                                   const UIPalette *palette, UILoaderContext *ctx,
                                   UICommonAttrs *common)
{
    // id, size, offset, layout and enabled all come from the common pass (size seeded with
    // ui_fill_container_size), so a bare <ViewHost> keeps its historic behaviour of filling
    // the application-built root. The offset is consumed at construct time.
    const char *host_id = common->id;

    // Record the initial-view attribute so InitLPanel can resolve it to an index.
    const char *initial_view = mxmlElementGetAttr(node, "initial-view");
    safe_strncpy(g_markup_table.initial_view_id, initial_view ? initial_view : "",
                 MAX_UI_ELEMENT_ID);

    LOADER_LOG(ctx, "Creating ViewHost: id=%s, initial-view=%s",
               host_id, initial_view ? initial_view : "<none>");

    // A plain fill-size container; the real UI_ELEMENT_ROOT is built by
    // ViewHostSystem_InitRoot in InitLPanel and this container is re-parented under
    // it. parent is NULL at the document root.
    UIElement *host = CreateUIContainer(parent, common->size,  // honour width=/height=/size= (defaults fill/fill)
                             common->offset,                    // honour offset=/offset-mode= (defaults zero fixed)
                             ui_standard_container_padding, palette,
                             UI_PALETTE_SURFACE_CONTAINER, *common->layout,
                             false, common->enabled);           // skeleton re-asserts the same enabled value
    common->offset_consumed = true;

    // Single-element builder: control == outer (offset consumed at construct time).
    return (UIBuildResult){ .element = host };
}

// ============================================================================
// Builder Registration
// ============================================================================

void UILoader_RegisterDefaultBuilders(void)
{
    // Register all built-in element type builders. Each builder's per-type size default
    // (the exact constant it previously hardcoded) is seeded into the registry so the
    // skeleton can place it in common.size before extraction, keeping attribute-less markup
    // byte-for-byte unchanged.
    UILoader_RegisterBuilder("ViewHost", BuildViewHost, ui_fill_container_size);
    UILoader_RegisterBuilder("ViewSelector", BuildViewSelector, ui_standard_selector_container_size);
    UILoader_RegisterBuilder("Option", BuildOption, ui_standard_selector_button_size);
    UILoader_RegisterBuilder("View", BuildView, ui_fill_container_size);
    UILoader_RegisterBuilder("Section", BuildSection, ui_standard_container_size);
    UILoader_RegisterBuilder("Container", BuildContainer, ui_standard_container_size);
    UILoader_RegisterBuilder("Button", BuildButton, ui_standard_button_size);
    UILoader_RegisterBuilder("Label", BuildLabel, ui_standard_control_size);
    UILoader_RegisterBuilder("Textbox", BuildTextbox, ui_standard_control_size);
    UILoader_RegisterBuilder("TextField", BuildTextField, ui_standard_control_size);
}

// ============================================================================
// Registry Management
// ============================================================================

bool UILoader_RegisterBuilder(const char *tag_name, UIElementBuilder builder,
                              Size default_size)
{
    if (!tag_name || !builder)
        return false;
    
    if (g_builder_registry.count >= MAX_REGISTERED_BUILDERS)
        return false;
    
    // Check for duplicate
    for (int i = 0; i < g_builder_registry.count; i++)
    {
        if (!strcmp(g_builder_registry.entries[i].tag_name, tag_name))
            return false;  // Already registered
    }
    
    g_builder_registry.entries[g_builder_registry.count].tag_name = tag_name;
    g_builder_registry.entries[g_builder_registry.count].builder = builder;
    // Store the per-type size default; the skeleton seeds common.size with this before
    // extraction so attribute-less markup keeps each element's historic default size.
    g_builder_registry.entries[g_builder_registry.count].default_size = default_size;
    g_builder_registry.count++;
    
    return true;
}

/**
 * Look up a builder by tag name.
 * @return Registry entry, or NULL if not found
 */
static const UIElementBuilderRegistry *UILoader_FindBuilder(const char *tag_name)
{
    if (!tag_name)
        return NULL;
    
    for (int i = 0; i < g_builder_registry.count; i++)
    {
        if (!strcmp(g_builder_registry.entries[i].tag_name, tag_name))
            return &g_builder_registry.entries[i];
    }
    
    return NULL;
}

// ============================================================================
// Common Attribute Extraction
// ============================================================================

// Helper: Parse layout attribute and return corresponding Spacing constant
// Supported values: "stack", "stack_wrap", "inline_wrap"
static const Spacing *UILoader_ParseLayout(const char *layout_str)
{
    if (!layout_str)
        return &ui_standard_stack_spacing;  // Default
    
    if (!strcmp(layout_str, "stack"))
        return &ui_standard_stack_spacing;
    else if (!strcmp(layout_str, "stacked"))
        // Alias of "stack". This is a GLOBAL alias in the shared layout parser, so
        // layout="stacked" now resolves for every element that reads layout=
        // (previously it silently defaulted to stack anyway).
        return &ui_standard_stack_spacing;
    else if (!strcmp(layout_str, "stack_wrap"))
        return &ui_standard_stack_wrap_spacing;
    else if (!strcmp(layout_str, "inline_wrap"))
        return &ui_standard_inline_wrap_spacing;
    
    return &ui_standard_stack_spacing;  // Default
}

// Parse one per-axis mode keyword ("fixed"|"percent"|"fill"|"hug") into an AxisSizeMode.
// *present is set true only when a recognised keyword was supplied; a NULL attribute or an
// unrecognised value leaves it false (so the caller keeps the builder's default for that
// axis). An unrecognised non-NULL value also emits a loader warning.
static AxisSizeMode UILoader_ParseAxisMode(UILoaderContext *ctx, const char *mode_str, bool *present)
{
    if (present)
        *present = false;

    if (!mode_str)
        return AXIS_FIXED;

    if (!strcmp(mode_str, "fixed"))
    {
        if (present) *present = true;
        return AXIS_FIXED;
    }
    else if (!strcmp(mode_str, "percent"))
    {
        if (present) *present = true;
        return AXIS_PERCENT;
    }
    else if (!strcmp(mode_str, "fill"))
    {
        if (present) *present = true;
        return AXIS_FILL;
    }
    else if (!strcmp(mode_str, "hug"))
    {
        if (present) *present = true;
        return AXIS_HUG;
    }

    LOADER_WARNING(ctx, "Unrecognised width/height mode; using element default");
    return AXIS_FIXED;
}

// Helper: Parse OffsetMode from string (e.g., "fixed", "percent")
static OffsetMode UILoader_ParseOffsetMode(const char *mode_str)
{
    if (!mode_str)
        return OFFSET_FIXED;  // Default
    
    if (!strcmp(mode_str, "percent"))
        return OFFSET_PERCENT;
    
    return OFFSET_FIXED;  // Default
}

// Helper: Parse size attribute (e.g., "100,50" or "100" for square)
static Vector2d UILoader_ParseSizeValues(const char *size_str)
{
    Vector2d result = {0.0f, 0.0f};
    
    if (!size_str)
        return result;
    
    // Try to parse "width,height" format
    float width = 0.0f, height = 0.0f;
    int parsed = sscanf(size_str, "%f,%f", &width, &height);
    
    if (parsed == 2)
    {
        result.x = width;
        result.y = height;
    }
    else if (parsed == 1)
    {
        // Single value: use for both width and height (square)
        result.x = width;
        result.y = width;
    }
    
    return result;
}

void UILoader_ExtractCommonAttrs(mxml_node_t *node, UILoaderContext *ctx, char *id_out,
                                 Size *size_out, Offset *offset_out,
                                 bool *enabled_out, const Spacing **layout_out)
{
    if (!node)
        return;
    
    // Extract id
    if (id_out)
    {
        const char *id_attr = mxmlElementGetAttr(node, "id");
        if (id_attr)
            safe_strncpy(id_out, id_attr, MAX_UI_ELEMENT_ID);
        else
            id_out[0] = '\0';
    }
    
    // Extract per-axis size. Numeric size="w,h" supplies authored dimensions; width=/height=
    // supply the per-axis mode keyword. Inference per axis: explicit mode wins; else a
    // provided size= value implies AXIS_FIXED on that axis; else leave the caller's seeded
    // builder default untouched.
    if (size_out)
    {
        const char *size_attr = mxmlElementGetAttr(node, "size");
        const char *width_attr = mxmlElementGetAttr(node, "width");
        const char *height_attr = mxmlElementGetAttr(node, "height");

        // A provided size= supplies BOTH axes (ParseSizeValues mirrors a lone value to both).
        Vector2d dims = size_attr ? UILoader_ParseSizeValues(size_attr) : (Vector2d){0.0f, 0.0f};
        bool has_size = (size_attr != NULL);

        bool wset = false, hset = false;
        AxisSizeMode wmode = UILoader_ParseAxisMode(ctx, width_attr, &wset);
        AxisSizeMode hmode = UILoader_ParseAxisMode(ctx, height_attr, &hset);

        // Per-axis mode inference (width and height resolved independently).
        if (wset)
            size_out->width_mode = wmode;
        else if (has_size)
            size_out->width_mode = AXIS_FIXED;
        // else: leave size_out->width_mode as the caller's builder default.

        if (hset)
            size_out->height_mode = hmode;
        else if (has_size)
            size_out->height_mode = AXIS_FIXED;
        // else: leave size_out->height_mode as the caller's builder default.

        // Only overwrite dimensions when size= actually provided numbers; otherwise keep the
        // builder default dimensions (so e.g. a Button with only width=hug keeps its authored
        // height for the FIXED axis).
        if (has_size)
            size_out->dimensions = dims;

        // A HUG axis treats dimensions > 0 as a MAX cap (see ResolveAxisExtent). A cap must
        // only exist when the author EXPLICITLY supplied one via size=; otherwise a leftover
        // builder-default dimension (e.g. ui_standard_container_size's {1.0, 0.5}) would leak
        // in as an accidental cap and clamp the hug far below its content. So when an axis
        // resolves to AXIS_HUG and no size= was authored, zero that axis's dimension to make
        // the hug uncapped (matching the old UI_SIZE_HUG_CAPPED(0, ...) width behaviour).
        if (!has_size && size_out->width_mode == AXIS_HUG)
            size_out->dimensions.x = 0.0f;
        if (!has_size && size_out->height_mode == AXIS_HUG)
            size_out->dimensions.y = 0.0f;

        // Warn when size= supplies a value for an axis whose RESOLVED mode is FILL (dead
        // value). LOADER_WARNING takes a fixed string (no varargs), so no value interpolation.
        if (has_size && size_out->width_mode == AXIS_FILL)
            LOADER_WARNING(ctx, "size= width value ignored: width mode is fill");
        if (has_size && size_out->height_mode == AXIS_FILL)
            LOADER_WARNING(ctx, "size= height value ignored: height mode is fill");

        // Warn when a PERCENT fraction falls outside 0..1.
        if (size_out->width_mode == AXIS_PERCENT && (dims.x < 0.0f || dims.x > 1.0f))
            LOADER_WARNING(ctx, "width percent value outside 0..1");
        if (size_out->height_mode == AXIS_PERCENT && (dims.y < 0.0f || dims.y > 1.0f))
            LOADER_WARNING(ctx, "height percent value outside 0..1");
    }
    
    // Extract offset and offset-mode. The preferred syntax mirrors size=: a single
    // offset="x,y" (a lone "offset=v" applies to both axes) plus offset-mode="fixed|percent".
    // The older x=/y=/offset-type= attributes remain accepted as a fallback so existing
    // markup keeps working; the unified attributes win when both are present.
    if (offset_out)
    {
        float x = 0.0f, y = 0.0f;

        // Fallback: legacy per-axis x=/y=.
        const char *x_attr = mxmlElementGetAttr(node, "x");
        const char *y_attr = mxmlElementGetAttr(node, "y");
        if (x_attr)
            sscanf(x_attr, "%f", &x);
        if (y_attr)
            sscanf(y_attr, "%f", &y);

        // Preferred: unified offset="x,y" (reuses the size value parser for identical
        // "x,y" / single-value-for-both semantics). Overrides the legacy attributes.
        const char *offset_attr = mxmlElementGetAttr(node, "offset");
        if (offset_attr)
        {
            Vector2d parsed = UILoader_ParseSizeValues(offset_attr);
            x = parsed.x;
            y = parsed.y;
        }

        // offset-mode= is preferred; offset-type= is the legacy spelling.
        const char *offset_mode_attr = mxmlElementGetAttr(node, "offset-mode");
        if (!offset_mode_attr)
            offset_mode_attr = mxmlElementGetAttr(node, "offset-type");

        OffsetMode mode = UILoader_ParseOffsetMode(offset_mode_attr);
        *offset_out = (Offset){{x, y}, mode};
    }
    
    // Extract enabled state
    if (enabled_out)
    {
        const char *enabled_attr = mxmlElementGetAttr(node, "enabled");
        *enabled_out = !enabled_attr || strcmp(enabled_attr, "false") != 0;  // Default true
    }

    // Extract child spacing from layout=. Folding layout into the common pass makes it a
    // single common concern; UILoader_ParseLayout returns a shared Spacing constant (never
    // NULL, defaulting to &ui_standard_stack_spacing), so the field is always populated.
    if (layout_out)
    {
        *layout_out = UILoader_ParseLayout(mxmlElementGetAttr(node, "layout"));
    }
}

// ============================================================================
// Tree Parsing
// ============================================================================

/**
 * Recursively parse an XML element and its children, building the UIElement tree.
 */
static UIElement *UILoader_ParseElementNode(mxml_node_t *node, UIElement *parent, UILoaderContext *ctx)
{
    if (!node || mxmlGetType(node) != MXML_TYPE_ELEMENT)
        return NULL;
    
    const char *tag = mxmlGetElement(node);
    if (!tag)
        return NULL;
    
    // Look up builder for this tag
    const UIElementBuilderRegistry *builder_entry = UILoader_FindBuilder(tag);
    if (!builder_entry)
    {
        // Log warning for unknown element type
        LOADER_WARNING(ctx, "Unknown XML element type");
        return NULL;
    }

    // --- COMMON PRE-EXTRACT (once, for every element) ------------------------
    // Seed the per-type size default from the registry, the universal zero fixed offset
    // default, and the enabled default before extraction. ExtractCommonAttrs then overwrites
    // only what the markup actually authors (and always populates layout), so attribute-less
    // markup keeps each element's historic defaults.
    UICommonAttrs common = {0};
    common.enabled = true;                                   // default; ExtractCommonAttrs re-affirms
    common.size    = builder_entry->default_size;            // per-type size default seed
    common.offset  = (Offset){{0.0f, 0.0f}, OFFSET_FIXED};   // historic universal offset default
    common.offset_consumed = false;
    UILoader_ExtractCommonAttrs(node, ctx, common.id, &common.size,
                                &common.offset, &common.enabled, &common.layout);

    // --- DISPATCH the variable step ------------------------------------------
    // The builder reads its own special attrs, calls the right constructor (passing size
    // always; offset+layout where it needs them at construct time), sets union data /
    // bindings / side-table records, and sets common.offset_consumed if it consumed offset.
    UIBuildResult built = builder_entry->builder(node, parent, ctx->palette, ctx, &common);

    // outer: the element the tag represents (the skeleton stamps common attrs onto it and
    // recurses children into it). A NULL outer means the builder failed; abort this node.
    UIElement *outer = built.element;
    if (!outer)
    {
        // Builder returned NULL; may have logged its own errors
        return NULL;
    }

    // control: the inner element where type-specific data/bindings live. A NULL bind_target
    // means "same as outer" (every single-element builder). Only a composite (e.g.
    // TextField) sets a distinct control; the skeleton uses this to decide id placement.
    UIElement *control = built.bind_target ? built.bind_target : outer;

    // --- COMMON POST-APPLY (the remainder the builder did not consume) --------
    // id: stamp on the OUTER element for single-element builders (control == outer) at this
    // single generic choke point, so the app can later hand any element back by id via
    // UILoader_FindById. For a composite (control != outer) the builder has already stamped
    // the id on its control itself (see BuildTextField), so the skeleton must NOT also stamp
    // the row - stamping both would make FindById ambiguous (the ancestor row wins pre-order
    // and would reintroduce the TextField label bug). This is independent of the
    // <View>/<Option> side-table recording (still needed for index resolution).
    if (control == outer)
    {
        safe_strncpy(outer->id, common.id, MAX_UI_ELEMENT_ID);
    }
    // enabled: single owner, applied uniformly to OUTER here (idempotent for containers that
    // also received enabled at construct time).
    outer->is_enabled = common.enabled;
    // offset: apply to OUTER ONLY when the builder did not consume it at construct time, so
    // offset is applied exactly once (no double application - e.g. Section passes it into its
    // ctor). For a composite the outer is the row, so the inner control keeps its own
    // constructor-authored offset untouched.
    if (!common.offset_consumed)
    {
        outer->authored_offset = common.offset;
        outer->resolved_offset = common.offset;
    }
    // size: NOT re-applied here; it was handled at construct time via common.size.

    // Recursively parse children into the OUTER element (never the control).
    for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child))
    {
        if (mxmlGetType(child) == MXML_TYPE_ELEMENT)
        {
            UIElement *child_elem = UILoader_ParseElementNode(child, outer, ctx);
            // Child parse failures are non-fatal; continue with siblings
            (void)child_elem;
        }
    }
    
    return outer;
}

// ============================================================================
// Loader Entry Points
// ============================================================================

UIElement *UILoader_LoadFromFile(const char *filepath, const UIPalette *palette)
{
    LOADER_LOG(NULL, "Loading UI from file: %s", filepath);

    if (!filepath)
    {
        LOADER_ERROR(NULL, "File path is NULL");
        return NULL;
    }

    // Clear per-load markup metadata before parsing a fresh tree.
    UILoader_ResetMarkupTable();

    mxml_node_t *tree = mxmlLoadFilename(NULL, NULL, filepath);
    if (!tree)
    {
        // mxmlLoadFilename returns NULL for file-not-found or parse errors
        LOADER_ERROR(NULL, "Failed to load or parse XML file");
        return NULL;
    }
    
    // Find document root element (skip XML declaration and text nodes)
    mxml_node_t *root_elem = mxmlGetFirstChild(tree);
    while (root_elem && mxmlGetType(root_elem) != MXML_TYPE_ELEMENT)
    {
        root_elem = mxmlGetNextSibling(root_elem);
    }
    
    if (!root_elem)
    {
        // XML file is empty or contains only non-element nodes
        LOADER_ERROR(NULL, "No root element found in XML file");
        mxmlDelete(tree);
        return NULL;
    }
    
    UILoaderContext ctx = {
        .root = root_elem,
        .palette = palette ? palette : &ui_default_palette,
        .source_file = filepath,
        .resolve_binding = NULL,  // Application to provide resolver
        .resolve_command = NULL,   // Application to provide resolver
        .user_data = NULL,
    };
    
    UIElement *root = UILoader_ParseElementNode(root_elem, NULL, &ctx);
    mxmlDelete(tree);
    
    return root;
}

UIElement *UILoader_LoadFromString(const char *xml_string, const UIPalette *palette)
{
    if (!xml_string)
    {
        LOADER_ERROR(NULL, "XML string is NULL");
        return NULL;
    }

    // Clear per-load markup metadata before parsing a fresh tree.
    UILoader_ResetMarkupTable();

    mxml_node_t *tree = mxmlLoadString(NULL, NULL, xml_string);
    if (!tree)
    {
        // XML parse error or empty string
        LOADER_ERROR(NULL, "Failed to parse XML string");
        return NULL;
    }
    
    // Find document root element (skip XML declaration and text nodes)
    mxml_node_t *root_elem = mxmlGetFirstChild(tree);
    while (root_elem && mxmlGetType(root_elem) != MXML_TYPE_ELEMENT)
    {
        root_elem = mxmlGetNextSibling(root_elem);
    }
    
    if (!root_elem)
    {
        // XML string is empty or contains only non-element nodes
        LOADER_ERROR(NULL, "No root element found in XML string");
        mxmlDelete(tree);
        return NULL;
    }
    
    UILoaderContext ctx = {
        .root = root_elem,
        .palette = palette ? palette : &ui_default_palette,
        .source_file = "<string>",
        .resolve_binding = NULL,  // Application to provide resolver
        .resolve_command = NULL,   // Application to provide resolver
        .user_data = NULL,
    };
    
    UIElement *root = UILoader_ParseElementNode(root_elem, NULL, &ctx);
    mxmlDelete(tree);
    
    return root;
}

// ============================================================================
// Loader Entry Points with Custom Resolvers
// ============================================================================

UIElement *UILoader_LoadFromFileWithResolvers(const char *filepath, const UIPalette *palette, UIBindingResolver resolve_binding,
                                               UICommandResolver resolve_command, UIViewTypeResolver resolve_view_type,
                                               void *user_data)
{
    LOADER_LOG(NULL, "Loading UI from file: %s", filepath);
    
    if (!filepath)
    {
        LOADER_ERROR(NULL, "File path is NULL");
        return NULL;
    }

    // Clear per-load markup metadata before parsing a fresh tree.
    UILoader_ResetMarkupTable();

    mxml_node_t *tree = mxmlLoadFilename(NULL, NULL, filepath);
    if (!tree)
    {
        LOADER_ERROR(NULL, "Failed to load or parse XML file");
        return NULL;
    }
    
    // Find document root element
    mxml_node_t *root_elem = mxmlGetFirstChild(tree);
    while (root_elem && mxmlGetType(root_elem) != MXML_TYPE_ELEMENT)
    {
        root_elem = mxmlGetNextSibling(root_elem);
    }
    
    if (!root_elem)
    {
        LOADER_ERROR(NULL, "No root element found in XML file");
        mxmlDelete(tree);
        return NULL;
    }
    
    UILoaderContext ctx = {
        .root = root_elem,
        .palette = palette ? palette : &ui_default_palette,
        .source_file = filepath,
        .resolve_binding = resolve_binding,
        .resolve_command = resolve_command,
        .resolve_view_type = resolve_view_type,
        .user_data = user_data,
    };
    
    UIElement *root = UILoader_ParseElementNode(root_elem, NULL, &ctx);
    mxmlDelete(tree);
    
    return root;
}

UIElement *UILoader_LoadFromStringWithResolvers(const char *xml_string, const UIPalette *palette,
                                                 UIBindingResolver resolve_binding,
                                                 UICommandResolver resolve_command,
                                                 UIViewTypeResolver resolve_view_type,
                                                 void *user_data)
{
    if (!xml_string)
    {
        LOADER_ERROR(NULL, "XML string is NULL");
        return NULL;
    }

    // Clear per-load markup metadata before parsing a fresh tree.
    UILoader_ResetMarkupTable();

    mxml_node_t *tree = mxmlLoadString(NULL, NULL, xml_string);
    if (!tree)
    {
        LOADER_ERROR(NULL, "Failed to parse XML string");
        return NULL;
    }
    
    // Find document root element
    mxml_node_t *root_elem = mxmlGetFirstChild(tree);
    while (root_elem && mxmlGetType(root_elem) != MXML_TYPE_ELEMENT)
    {
        root_elem = mxmlGetNextSibling(root_elem);
    }
    
    if (!root_elem)
    {
        LOADER_ERROR(NULL, "No root element found in XML string");
        mxmlDelete(tree);
        return NULL;
    }
    
    UILoaderContext ctx = {
        .root = root_elem,
        .palette = palette ? palette : &ui_default_palette,
        .source_file = "<string>",
        .resolve_binding = resolve_binding,
        .resolve_command = resolve_command,
        .resolve_view_type = resolve_view_type,
        .user_data = user_data,
    };
    
    UIElement *root = UILoader_ParseElementNode(root_elem, NULL, &ctx);
    mxmlDelete(tree);
    
    return root;
}

// ============================================================================
// Post-Load Processing: View Extraction
// ============================================================================

/**
 * Recursive helper: collect all View containers in the tree.
 * Finds UIElements with type==UI_ELEMENT_VIEW and creates View structs for each.
 */
static void UILoader_CollectViewContainers(UIElement *elem, View ***views_array,
                                           size_t *count, size_t *capacity)
{
    if (!elem)
        return;

    // If this element is a View container, create a View struct for it
    if (elem->type == UI_ELEMENT_VIEW)
    {
        // Resize array if needed
        if (*count >= *capacity)
        {
            *capacity = (*capacity == 0) ? 4 : *capacity * 2;
            *views_array = (View **)realloc(*views_array, sizeof(View *) * (*capacity));
        }

        // Create View wrapping this container
        View *view = AllocateBytes(sizeof(View));
        if (view)
        {
            view->container = elem;
            // Resolve the correct ViewType recorded during parsing (BuildView) by
            // looking the container pointer up in the per-load side table. The
            // container pointer stored there is this exact `elem` (no copy), so the
            // lookup is a direct pointer match. Defaults to the first ViewType with
            // a warning if the container is somehow absent.
            view->type = UILoader_LookupViewType(NULL, elem);
            view->scroll_x = 0.0f;
            view->max_scroll_x = 0.0f;
            view->content_width = 0.0f;
            view->scroll_y = 0.0f;
            view->max_scroll_y = 0.0f;
            view->content_height = 0.0f;
            view->is_scrollable_x = false;
            // Apply the recorded scrollable= flag (vertical) from the markup, syncing
            // the container flag the same way View_SetScrollableY does.
            view->is_scrollable_y = UILoader_LookupViewScrollable(elem);
            elem->is_scrollable_y = view->is_scrollable_y;
            
            (*views_array)[(*count)++] = view;
        }
    }

    // Recursively process children
    ForEachChild(elem, child)
    {
        UILoader_CollectViewContainers(child, views_array, count, capacity);
    }
}

/**
 * Extract all View containers from XML tree and create View structs.
 */
View **UILoader_ExtractViews(UIElement *root, size_t *out_count)
{
    if (!root || !out_count)
        return NULL;

    View **views = NULL;
    size_t count = 0;
    size_t capacity = 0;

    UILoader_CollectViewContainers(root, &views, &count, &capacity);

    *out_count = count;
    return views;
}

// ============================================================================
// Post-Load Processing: Selector / View-Index Resolution
// ============================================================================

/**
 * Find the index in host->views of the View whose source container has the given
 * id, using the per-load side table. Returns -1 when no View matches.
 *
 * The side-table view-container pointers are identical to host->views[k]->container
 * (UILoader_CollectViewContainers stores the exact tree pointer without copying),
 * so this is a direct pointer-keyed id comparison. An id-less View (empty recorded
 * id) can never match a non-empty id_string.
 */
int UILoader_ResolveViewIndexById(ViewHostSystem *host, const char *id_string)
{
    if (!host || !id_string || id_string[0] == '\0')
    {
        return -1;
    }

    for (int k = 0; k < host->views.count; k++)
    {
        View *view = *((View **)LArray_Get(&host->views, k));
        if (!view)
        {
            continue;
        }

        // Find the side-table row for this view's container, then compare its id.
        for (size_t t = 0; t < g_markup_table.view_count; t++)
        {
            if (g_markup_table.views[t].container == view->container)
            {
                if (!strcmp(g_markup_table.views[t].id, id_string))
                {
                    return k;
                }
                break;  // Container matched but id did not; no other row shares it.
            }
        }
    }

    return -1;
}

/**
 * Return the initialView id recorded from the most recent load's <ViewHost>, or
 * NULL when none was recorded.
 */
const char *UILoader_GetInitialViewId(void)
{
    return g_markup_table.initial_view_id[0] != '\0' ? g_markup_table.initial_view_id : NULL;
}

/**
 * Find the first element at or below `root` whose id= equals `id`.
 *
 * Depth-first, pre-order, first-match-wins walk mirroring the
 * UILoader_CollectViewContainers recursion. Domain-free: it compares only the
 * generic UIElement.id, so it works on any UIElement tree. Returns NULL when
 * root/id is NULL/empty or no element matches.
 */
UIElement *UILoader_FindById(UIElement *root, const char *id)
{
    // Guard NULL root and a NULL/empty id; an empty id can never name an element.
    if (!root || !id || id[0] == '\0')
        return NULL;

    // Pre-order: test this element before descending so the first match wins.
    if (strcmp(root->id, id) == 0)
        return root;

    // Recurse over children, returning the first non-NULL hit.
    ForEachChild(root, child)
    {
        UIElement *found = UILoader_FindById(child, id);
        if (found)
            return found;
    }

    return NULL;
}

/**
 * Build and register a ViewSelector on `host` from the captured <Option> metadata.
 *
 * Each Option's target view id is resolved to the index of the matching View in
 * host->views; unmatched targets default to index 0 with a warning. The selector's
 * buttons[]/view_indices[] arrays are allocated with the exact element sizes used
 * by the C-built path so DestroyPanelViewSelector's frees stay correct, and each
 * enumerate button is wired exactly like ViewHostSystem_CreateViewSelector so
 * clicks drive ViewHostSystem_SelectView. Styling / initial selection is left to
 * the single ViewHostSystem_SelectView call the application makes afterwards.
 */
ViewSelector *UILoader_BuildSelectorFromMarkup(ViewHostSystem *host,
                                               ViewSelectionCallback callback)
{
    if (!host || g_markup_table.option_count == 0)
    {
        return NULL;
    }

    size_t count = g_markup_table.option_count;

    // Allocate the selector and its backing arrays. Element sizes match
    // AllocatePanelViewSelector so DestroyPanelViewSelector frees them correctly.
    ViewSelector *selector = AllocateBytes(sizeof(ViewSelector));
    if (!selector)
    {
        return NULL;
    }

    selector->buttons = AllocateBytes(sizeof(UIElement *) * count);
    selector->view_indices = AllocateBytes(sizeof(int) * count);
    if (!selector->buttons || !selector->view_indices)
    {
        // Mirror AllocatePanelViewSelector's cleanup on partial allocation failure.
        Deallocate((void **)&selector->buttons, sizeof(UIElement *) * count);
        Deallocate((void **)&selector->view_indices, sizeof(int) * count);
        Deallocate((void **)&selector, sizeof(ViewSelector));
        return NULL;
    }

    selector->panel = host;
    selector->count = count;
    selector->active_index = count;  // Unselected sentinel; overwritten by SelectView.
    selector->on_view_selected = callback;

    // Resolve each Option to a view index and wire its button.
    for (size_t i = 0; i < count; i++)
    {
        LoadedOptionEntry *option = &g_markup_table.options[i];

        // Resolve the Option's target view id to a view index; default to 0 + warn.
        int index = UILoader_ResolveViewIndexById(host, option->view_id);
        if (index < 0)
        {
            LOADER_WARNING(NULL, "Option view target did not resolve; defaulting to first view");
            index = 0;
        }

        selector->buttons[i] = option->button;
        selector->view_indices[i] = index;  // Resolved index (NOT a trivial 0..count-1).

        // Wire the enumerate button exactly like ViewHostSystem_CreateViewSelector.
        if (option->button)
        {
            option->button->data.button.on_click = HandleViewHostSelectorClick;
            option->button->data.button.user_data = &selector->view_indices[i];
            option->button->data.button.data_bind = selector;
        }
    }

    // Register the selector so the host owns it (and frees it on destroy).
    if (!LArray_Push(&host->selectors, &selector))
    {
        Deallocate((void **)&selector->buttons, sizeof(UIElement *) * count);
        Deallocate((void **)&selector->view_indices, sizeof(int) * count);
        Deallocate((void **)&selector, sizeof(ViewSelector));
        return NULL;
    }

    return selector;
}
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
 *             size-mode (optional).
 *
 * Assembles ONE Binding carrying BOTH halves: a display SOURCE from binding= (resolved
 * via resolve_binding into an address or query source) and a commit SINK from action=
 * (resolved via resolve_action, else the legacy resolve_command, else a direct integer
 * code). A button with only action= is byte-identical to the previous behaviour; a
 * button with only binding= becomes a read-only live label; a button with neither gets
 * no Binding and no click handler.
 */
static UIElement *BuildButton(mxml_node_t *node, UIElement *parent, const UIPalette *palette, UILoaderContext *ctx)
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
    
    // Extract size and enabled attributes (enabled defaults to true when absent).
    Size size = ui_standard_button_size;
    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, NULL, &size, NULL, &enabled);

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

    UIElement *button = CreateUIButtonDefault(parent, btn_type, text, size,
                                              ui_standard_button_padding, palette, handler, NULL, NULL);

    // Apply enabled= (defaults to true).
    if (button)
    {
        button->is_enabled = enabled;
    }

    // Attach the assembled Binding when either half resolved. A command/callback sink is
    // fired by UILoader_HandleCommandClick; a source drives the generic refresh walk.
    // The binding is freed by DisposeUIElement's button-binding cleanup.
    if (button && (have_source || have_sink))
    {
        button->data.button.binding = Binding_Create(b);
    }

    return button;
}

/**
 * Builder for <Label> elements.
 * Attributes: text (required)
 */
static UIElement *BuildLabel(mxml_node_t *node, UIElement *parent, const UIPalette *palette, UILoaderContext *ctx)
{
    const char *text = mxmlElementGetAttr(node, "text");
    if (!text)
        text = "";
    
    UIElement *label_elem = CreateUILabelDefault(parent, text, ui_standard_control_size,
                               ui_standard_button_padding, palette);

    // Apply enabled= (defaults to true when absent).
    const char *enabled_attr = mxmlElementGetAttr(node, "enabled");
    if (label_elem && enabled_attr)
    {
        label_elem->is_enabled = strcmp(enabled_attr, "false") != 0;
    }

    return label_elem;
}

/**
 * Builder for <TextField> elements.
 * Attributes: label (required), type (integer/float/vector2), binding (optional), default (optional)
 */
static UIElement *BuildTextField(mxml_node_t *node, UIElement *parent, const UIPalette *palette, UILoaderContext *ctx)
{
    const char *label = mxmlElementGetAttr(node, "label");
    if (!label)
        label = "";
    
    const char *type_attr = mxmlElementGetAttr(node, "type");
    UIElementType field_type = UI_ELEMENT_TEXTBOX_IO;
    DataType data_type = FLOAT;  // Default data type
    
    // Map type attribute to UIElementType and DataType
    if (type_attr)
    {
        if (!strcmp(type_attr, "integer"))
        {
            data_type = INT;
        }
        else if (!strcmp(type_attr, "float"))
        {
            data_type = FLOAT;
        }
        else if (!strcmp(type_attr, "vector2"))
        {
            data_type = VECTOR2D;
        }
    }
    
    // Extract binding and resolve to data address
    void *data_bind = NULL;
    const char *binding_attr = mxmlElementGetAttr(node, "binding");
    if (binding_attr && ctx && ctx->resolve_binding)
    {
        UIBinding binding = ctx->resolve_binding(binding_attr, ctx);
        data_bind = binding.address;
        // Use resolved data_type if binding was found
        if (binding.address)
        {
            data_type = binding.data_type;
        }
        else
        {
            // Warn if binding did not resolve
            LOADER_WARNING(ctx, "Binding did not resolve");
        }
    }
    else if (binding_attr)
    {
        // Binding specified but no resolver available
        LOADER_WARNING(ctx, "Binding specified but resolver not available");
    }
    
    UIElement *field = CreateUILabeledFieldDefault(parent, label, field_type,
                                      ui_standard_control_size,
                                      ui_standard_field_padding, palette);

    // Apply enabled= (defaults to true when absent).
    const char *enabled_attr = mxmlElementGetAttr(node, "enabled");
    if (field && enabled_attr)
    {
        field->is_enabled = strcmp(enabled_attr, "false") != 0;
    }

    return field;
}

/**
 * Builder for <Section> elements.
 * Attributes: title (required), id (optional), size (optional), size-mode (optional),
 *             layout (optional: "stack"/"stack_wrap"/"inline_wrap"), 
 *             offset (optional), offset-mode (optional)
 * Children are added to the section's container.
 */
static UIElement *BuildSection(mxml_node_t *node, UIElement *parent,
                               const UIPalette *palette, UILoaderContext *ctx)
{
    const char *title = mxmlElementGetAttr(node, "title");
    if (!title)
        title = "";
    
    // Extract size, offset and enabled attributes (enabled defaults to true).
    Size size = ui_standard_container_size;
    Offset offset = {{0.0f, 0.0f}, OFFSET_FIXED};
    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, &enabled);
    
    // Extract layout attribute to determine child spacing
    const char *layout_attr = mxmlElementGetAttr(node, "layout");
    const Spacing *layout = UILoader_ParseLayout(layout_attr);
    
    // Create section with explicit size, offset, and layout
    UIElement *section = CreateViewSection(parent, title, size, offset, layout, palette);

    // Apply enabled= (defaults to true).
    if (section)
    {
        section->is_enabled = enabled;
    }

    return section;
}

/**
 * Builder for <Container> elements.
 * Attributes: size (optional), spacing (optional)
 * Children are added to the container.
 */
static UIElement *BuildContainer(mxml_node_t *node, UIElement *parent,
                                 const UIPalette *palette, UILoaderContext *ctx)
{
    // Extract size, offset and layout exactly as BuildSection does, defaulting to
    // the standard container size when the attributes are absent. This only honours
    // attributes when present, so it is not a behavioural change for existing
    // attribute-less <Container> markup.
    Size size = ui_standard_container_size;
    Offset offset = {{0.0f, 0.0f}, OFFSET_FIXED};
    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, &enabled);

    const char *layout_attr = mxmlElementGetAttr(node, "layout");
    const Spacing *layout = UILoader_ParseLayout(layout_attr);

    return CreateUIContainer(parent, size, offset,
                            ui_standard_container_padding, palette,
                            UI_PALETTE_SURFACE_CONTAINER, *layout,
                            false, enabled);  // honour enabled= (defaults to true)
}

/**
 * Builder for <View> elements.
 * Attributes: id (optional), type (optional), scrollable (optional)
 * Children (Sections) are added to the view container.
 * 
 * Creates a View struct with a fill-size container. The View is independent
 * of any panel and can be registered with a panel later if needed.
 */
static UIElement *BuildView(mxml_node_t *node, UIElement *parent,
                            const UIPalette *palette, UILoaderContext *ctx)
{
    // Extract view attributes. The id is preserved so Options and the ViewHost's
    // initialView can resolve to this View's index after the tree is built. The
    // enabled flag defaults to true when the attribute is absent.
    char view_id[MAX_UI_ELEMENT_ID] = {0};
    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, view_id, NULL, NULL, &enabled);
    
    const char *view_type_str = mxmlElementGetAttr(node, "type");
    const char *scrollable_attr = mxmlElementGetAttr(node, "scrollable");
    // Honour scrollable= when present (default false); applied to the View struct
    // in UILoader_CollectViewContainers once the View wrapper exists.
    bool is_scrollable = scrollable_attr && strcmp(scrollable_attr, "true") == 0;

    // Honour layout= for the view's child spacing (defaults to stack when absent).
    const char *layout_attr = mxmlElementGetAttr(node, "layout");
    const Spacing *layout = UILoader_ParseLayout(layout_attr);

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
    
    // Create container with UI_ELEMENT_VIEW type so post-processor can find it
    UIElement *container = CreateUIContainer(
        parent,
        ui_fill_container_size,
        (Offset){{0, 0}, OFFSET_FIXED},
        ui_standard_container_padding,
        palette,
        UI_PALETTE_SURFACE_CONTAINER,
        *layout,          // honour layout= (defaults to stack)
        false,
        enabled           // honour enabled= (defaults to true)
    );
    
    if (container)
    {
        container->type = UI_ELEMENT_VIEW;  // Mark for post-processor
        // Record the resolved type, id and scrollability against this exact container
        // pointer so UILoader_CollectViewContainers can stamp the correct View->type
        // and scroll flag, and the post-process pass can resolve Option/initialView
        // ids to view indices.
        UILoader_RecordView(ctx, container, view_type, view_id, is_scrollable);
    }
    
    return container;
}

/**
 * Builder for <ViewSelector> elements.
 * Attributes: id (optional), type (optional)
 * Children (Options) are converted to selector buttons.
 */
static UIElement *BuildViewSelector(mxml_node_t *node, UIElement *parent,
                                    const UIPalette *palette, UILoaderContext *ctx)
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

    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, NULL, NULL, NULL, &enabled);

    UIElement *toggle_cont = CreateUIContainer(
        parent, ui_standard_selector_container_size,
        (Offset){{0.0f, 0.0f}, OFFSET_FIXED}, ZERO_VECTOR_2D,
        palette, UI_PALETTE_SURFACE_TRANSPARENT,
        ui_zero_inline_spacing, false, enabled);  // honour enabled= (defaults to true)

    if (toggle_cont)
    {
        toggle_cont->colour_border = palette->container_border;
        // Record the selector container so the post-process pass can locate it.
        g_markup_table.selector_cont = toggle_cont;
    }

    return toggle_cont;
}

/**
 * Builder for <Option> elements (inside ViewSelector).
 * Attributes: text (required), view (optional)
 */
static UIElement *BuildOption(mxml_node_t *node, UIElement *parent,
                              const UIPalette *palette, UILoaderContext *ctx)
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

    UIElement *button = CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_ENUMERATE, text,
                                             ui_standard_selector_button_size,
                                             ui_standard_button_padding, palette,
                                             NULL, NULL, NULL);

    if (button)
    {
        // Apply enabled= (defaults to true when absent).
        const char *enabled_attr = mxmlElementGetAttr(node, "enabled");
        if (enabled_attr)
        {
            button->is_enabled = strcmp(enabled_attr, "false") != 0;
        }
        UILoader_RecordOption(ctx, button, view_attr);
    }

    return button;
}

/**
 * Builder for <ViewHost> elements (root of a multi-view panel markup).
 * Attributes: id (optional), layout (optional: "stacked"/"stack"/...),
 *             initial-view (optional), enabled (optional, default true).
 *
 * Produces a plain fill-size container (NOT a UI_ELEMENT_ROOT) that becomes the
 * content host re-parented under the real ViewHostSystem root in InitLPanel. The
 * viewport and scale are deliberately NOT expressed in markup: they are
 * application-specific and supplied by the application (InitLPanel), keeping the
 * generic loader free of viewport knowledge. The initial-view id is recorded for
 * the application to resolve to a view index.
 */
static UIElement *BuildViewHost(mxml_node_t *node, UIElement *parent,
                                const UIPalette *palette, UILoaderContext *ctx)
{
    // Extract the host id (documentation / future lookup), enabled flag and layout spacing.
    char host_id[MAX_UI_ELEMENT_ID] = {0};
    bool enabled = true;
    UILoader_ExtractCommonAttrs(node, host_id, NULL, NULL, &enabled);

    const char *layout_attr = mxmlElementGetAttr(node, "layout");
    const Spacing *layout = UILoader_ParseLayout(layout_attr);

    // Record the initial-view attribute so InitLPanel can resolve it to an index.
    const char *initial_view = mxmlElementGetAttr(node, "initial-view");
    safe_strncpy(g_markup_table.initial_view_id, initial_view ? initial_view : "",
                 MAX_UI_ELEMENT_ID);

    LOADER_LOG(ctx, "Creating ViewHost: id=%s, initial-view=%s",
               host_id, initial_view ? initial_view : "<none>");

    // A plain fill-size container; the real UI_ELEMENT_ROOT is built by
    // ViewHostSystem_InitRoot in InitLPanel and this container is re-parented under
    // it. parent is NULL at the document root.
    return CreateUIContainer(parent, ui_fill_container_size,
                             (Offset){{0, 0}, OFFSET_FIXED},
                             ui_standard_container_padding, palette,
                             UI_PALETTE_SURFACE_CONTAINER, *layout,
                             false, enabled);  // honour enabled= (defaults to true)
}

// ============================================================================
// Builder Registration
// ============================================================================

void UILoader_RegisterDefaultBuilders(void)
{
    // Register all built-in element type builders
    UILoader_RegisterBuilder("ViewHost", BuildViewHost);
    UILoader_RegisterBuilder("ViewSelector", BuildViewSelector);
    UILoader_RegisterBuilder("Option", BuildOption);
    UILoader_RegisterBuilder("View", BuildView);
    UILoader_RegisterBuilder("Section", BuildSection);
    UILoader_RegisterBuilder("Container", BuildContainer);
    UILoader_RegisterBuilder("Button", BuildButton);
    UILoader_RegisterBuilder("Label", BuildLabel);
    UILoader_RegisterBuilder("TextField", BuildTextField);
}

// ============================================================================
// Registry Management
// ============================================================================

bool UILoader_RegisterBuilder(const char *tag_name, UIElementBuilder builder)
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

// Helper: Parse SizeMode from string (e.g., "fixed", "fill", "content_fill")
static SizeMode UILoader_ParseSizeMode(const char *mode_str)
{
    if (!mode_str)
        return SIZE_CONTENT;  // Default
    
    if (!strcmp(mode_str, "fixed"))
        return SIZE_FIXED;
    else if (!strcmp(mode_str, "percent"))
        return SIZE_PERCENT;
    else if (!strcmp(mode_str, "fill"))
        return SIZE_FILL;
    else if (!strcmp(mode_str, "content"))
        return SIZE_CONTENT;
    else if (!strcmp(mode_str, "content_fill"))
        return SIZE_CONTENT_FILL;
    else if (!strcmp(mode_str, "content_max"))
        return SIZE_CONTENT_MAX;
    
    return SIZE_CONTENT;  // Default
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

void UILoader_ExtractCommonAttrs(mxml_node_t *node, char *id_out,
                                 Size *size_out, Offset *offset_out,
                                 bool *enabled_out)
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
    
    // Extract size and size-mode
    if (size_out)
    {
        const char *size_attr = mxmlElementGetAttr(node, "size");
        const char *size_mode_attr = mxmlElementGetAttr(node, "size-mode");
        
        if (size_attr)
        {
            Vector2d dimensions = UILoader_ParseSizeValues(size_attr);
            SizeMode mode = UILoader_ParseSizeMode(size_mode_attr);
            *size_out = (Size){{dimensions.x, dimensions.y}, mode};
        }
        else if (size_mode_attr)
        {
            // Honour a size-mode even when no explicit size is given. Modes like
            // "content", "content_fill", and "fill" derive their dimensions from
            // children/parent, so zeroed dimensions are correct here.
            SizeMode mode = UILoader_ParseSizeMode(size_mode_attr);
            *size_out = (Size){{0.0f, 0.0f}, mode};
        }
        else
        {
            *size_out = ui_standard_button_size;  // Default
        }
    }
    
    // Extract offset (x, y) and offset-type
    if (offset_out)
    {
        const char *x_attr = mxmlElementGetAttr(node, "x");
        const char *y_attr = mxmlElementGetAttr(node, "y");
        const char *offset_type_attr = mxmlElementGetAttr(node, "offset-type");
        
        float x = 0.0f, y = 0.0f;
        if (x_attr)
            sscanf(x_attr, "%f", &x);
        if (y_attr)
            sscanf(y_attr, "%f", &y);
        
        OffsetMode mode = UILoader_ParseOffsetMode(offset_type_attr);
        *offset_out = (Offset){{x, y}, mode};
    }
    
    // Extract enabled state
    if (enabled_out)
    {
        const char *enabled_attr = mxmlElementGetAttr(node, "enabled");
        *enabled_out = !enabled_attr || strcmp(enabled_attr, "false") != 0;  // Default true
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
    
    // Call tag-specific builder to construct this element
    UIElement *elem = builder_entry->builder(node, parent, ctx->palette, ctx);
    if (!elem)
    {
        // Builder returned NULL; may have logged its own errors
        return NULL;
    }
    
    // Recursively parse children
    for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child))
    {
        if (mxmlGetType(child) == MXML_TYPE_ELEMENT)
        {
            UIElement *child_elem = UILoader_ParseElementNode(child, elem, ctx);
            // Child parse failures are non-fatal; continue with siblings
            (void)child_elem;
        }
    }
    
    return elem;
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
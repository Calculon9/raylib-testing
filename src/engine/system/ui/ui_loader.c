/**
 * Generic XML UI tree loader implementation.
 * Factory-registry pattern for declarative UI markup.
 */

#include "system/ui/ui_loader.h"
#include "system/command_system.h"
#include "system/panel_system.h"
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

static struct {
    UIElementBuilderRegistry entries[MAX_REGISTERED_BUILDERS];
    int count;
} g_builder_registry = {0};

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
 * Builder for <Button> elements.
 * Attributes: text (required), type (optional: "simple"/"enumerate"/"submit"), action (optional),
 *             size (optional), size-mode (optional)
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
    
    // Extract size attribute
    Size size = ui_standard_button_size;
    UILoader_ExtractCommonAttrs(node, NULL, &size, NULL, NULL);
    
    // Extract action attribute and resolve to CommandType code
    UIEventHandler handler = NULL;
    void *user_data = NULL;
    const char *action_attr = mxmlElementGetAttr(node, "action");
    if (action_attr && ctx && ctx->resolve_command)
    {
        int command_code = ctx->resolve_command(action_attr, ctx);
        
        // If action resolved to a valid command code, use ExecuteCommand as the handler
        if (command_code != 0)
        {
            handler = (UIEventHandler)ExecuteCommand;
            user_data = (void *)(intptr_t)command_code;
        }
        else
        {
            LOADER_WARNING(ctx, "Unrecognised action string");
        }
    }
    else if (action_attr)
    {
        // Try parsing as direct integer code
        char *endptr;
        long command_code = strtol(action_attr, &endptr, 10);
        
        if (*endptr == '\0' && command_code > 0)
        {
            handler = (UIEventHandler)ExecuteCommand;
            user_data = (void *)(intptr_t)command_code;
        }
        else
        {
            LOADER_WARNING(ctx, "Action value not a valid integer");
        }
    }
    
    return CreateUIButtonDefault(parent, btn_type, text, size,
                                ui_standard_button_padding, palette, handler, user_data, NULL);
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
    
    return CreateUILabelDefault(parent, text, ui_standard_control_size,
                               ui_standard_button_padding, palette);
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
    
    return CreateUILabeledFieldDefault(parent, label, field_type,
                                      ui_standard_control_size,
                                      ui_standard_field_padding, palette);
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
    
    // Extract size and offset attributes
    Size size = ui_standard_container_size;
    Offset offset = {{0.0f, 0.0f}, OFFSET_FIXED};
    UILoader_ExtractCommonAttrs(node, NULL, &size, &offset, NULL);
    
    // Extract layout attribute to determine child spacing
    const char *layout_attr = mxmlElementGetAttr(node, "layout");
    const Spacing *layout = UILoader_ParseLayout(layout_attr);
    
    // Create section with explicit size, offset, and layout
    return CreateViewSection(parent, title, size, offset, layout, palette);
}

/**
 * Builder for <Container> elements.
 * Attributes: size (optional), spacing (optional)
 * Children are added to the container.
 */
static UIElement *BuildContainer(mxml_node_t *node, UIElement *parent,
                                 const UIPalette *palette, UILoaderContext *ctx)
{
    // TODO: Extract size and spacing attributes
    return CreateUIContainer(parent, ui_standard_container_size, (Offset){{0, 0}, OFFSET_FIXED},
                            ui_standard_container_padding, palette,
                            UI_PALETTE_SURFACE_CONTAINER, ui_standard_stack_spacing,
                            false, true);
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
    // Extract view attributes
    char view_id[MAX_UI_ELEMENT_ID] = {0};
    UILoader_ExtractCommonAttrs(node, view_id, NULL, NULL, NULL);
    
    const char *view_type_str = mxmlElementGetAttr(node, "type");
    const char *scrollable_attr = mxmlElementGetAttr(node, "scrollable");
    bool is_scrollable = scrollable_attr && strcmp(scrollable_attr, "true") == 0;
    
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
        ui_standard_stack_spacing,
        false,
        true
    );
    
    if (container)
    {
        container->type = UI_ELEMENT_VIEW;  // Mark for post-processor
    }
    
    LOADER_LOG(ctx, "Created View container: id=%s, type=%s",
               view_id, view_type_str ? view_type_str : "<none>");
    
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
    // TODO: Create a button group container for view selection
    // Parse Option children and create buttons for each
    return CreateUIContainer(parent, ui_standard_selector_container_size,
                            (Offset){{0, 0}, OFFSET_FIXED},
                            ui_standard_button_padding, palette,
                            UI_PALETTE_SURFACE_CONTAINER, ui_standard_inline_spacing,
                            false, true);
}

/**
 * Builder for <Panel> elements (root container).
 * Attributes: id (optional), layout (optional)
 * Children (ViewSelector, Views) are added.
 * 
 * When parent is NULL, this Panel is a root element (e.g., loaded from XML file).
 * The UI constructor should handle NULL parent gracefully for root elements.
 */
static UIElement *BuildPanel(mxml_node_t *node, UIElement *parent,
                             const UIPalette *palette, UILoaderContext *ctx)
{
    // TODO: Extract panel-specific attributes
    // Pass parent as-is; NULL indicates root element, non-NULL indicates nested panel
    return CreateUIContainer(parent, ui_fill_container_size, (Offset){{0, 0}, OFFSET_FIXED},
                            ui_standard_container_padding, palette,
                            UI_PALETTE_SURFACE_CONTAINER, ui_standard_stack_spacing,
                            false, true);
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
    
    // TODO: Extract view attribute for tab switching
    return CreateUIButtonDefault(parent, UI_ELEMENT_BUTTON_ENUMERATE, text,
                                ui_standard_selector_button_size,
                                ui_standard_button_padding, palette,
                                NULL, NULL, NULL);
}

// ============================================================================
// Builder Registration
// ============================================================================

void UILoader_RegisterDefaultBuilders(void)
{
    // Register all built-in element type builders
    UILoader_RegisterBuilder("Panel", BuildPanel);
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
                                               UICommandResolver resolve_command, void *user_data)
{
    LOADER_LOG(NULL, "Loading UI from file: %s", filepath);
    
    if (!filepath)
    {
        LOADER_ERROR(NULL, "File path is NULL");
        return NULL;
    }
    
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
        .user_data = user_data,
    };
    
    UIElement *root = UILoader_ParseElementNode(root_elem, NULL, &ctx);
    mxmlDelete(tree);
    
    return root;
}

UIElement *UILoader_LoadFromStringWithResolvers(const char *xml_string, const UIPalette *palette,
                                                 UIBindingResolver resolve_binding,
                                                 UICommandResolver resolve_command,
                                                 void *user_data)
{
    if (!xml_string)
    {
        LOADER_ERROR(NULL, "XML string is NULL");
        return NULL;
    }
    
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
            view->type = LPANEL_STATE_VIEW;  // Default; could be extended to parse from attributes
            view->scroll_x = 0.0f;
            view->max_scroll_x = 0.0f;
            view->content_width = 0.0f;
            view->scroll_y = 0.0f;
            view->max_scroll_y = 0.0f;
            view->content_height = 0.0f;
            view->is_scrollable_x = false;
            view->is_scrollable_y = false;
            
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
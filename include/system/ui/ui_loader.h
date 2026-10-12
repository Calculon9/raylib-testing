/**
 * Generic XML UI tree loader.
 * Parses XML markup files and constructs UIElement trees using a factory-registry pattern.
 * 
 * Supports declarative UI markup in XML with element-type dispatch to element-specific builders.
 * Registry-based: add new element types by implementing a builder function and registering it.
 * 
 * Error Handling:
 * - Logs warnings for missing/invalid bindings and unrecognized actions
 * - Logs errors for missing required attributes and malformed XML
 * - Continues parsing on non-fatal errors; only fails on critical issues
 * - Returns NULL on file-not-found, parse errors, or missing root element
 * 
 * Schema Validation:
 * - Required attributes validated per element type
 * - Type attributes mapped to correct enum values
 * - Optional attributes handled gracefully with defaults
 * - Unrecognized element types logged and skipped
 */

#ifndef UI_LOADER_H
#define UI_LOADER_H

#include "ui/ui.h"
#include "system/ui_system.h"
#include "system/view_host_system.h"
#include <mxml.h>

// Maximum length of an element ID from XML markup
#define MAX_UI_ELEMENT_ID (int)sizeof(String64)

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct UILoaderContext UILoaderContext;

// ============================================================================
// Data Binding Resolution
// ============================================================================

/**
 * Kind of source a binding= attribute resolved to.
 * Mirrors BindingSourceKind but stays loader-local so the header does not force
 * every resolver to adopt the full core enum semantics; the loader maps 1:1.
 */
typedef enum {
    UI_BIND_SRC_NONE = 0,   // unresolved / absent
    UI_BIND_SRC_ADDRESS,    // *(T*)address
    UI_BIND_SRC_QUERY,      // query(query_key) -> BindingValue
} UIBindingSourceKind;

/**
 * Binding (source) metadata: how a binding= attribute reads its display value.
 *
 * CRITICAL LAYOUT CONSTRAINT (design section 1.3): `address` and `data_type` MUST
 * remain the FIRST TWO members, in that order. The new members (`kind`, `query`,
 * `query_key`, `value_type`, `write`, `write_key`) are APPENDED after `data_type`
 * and must never be moved before it. LPanel_ResolveBinding initialises its result
 * with the POSITIONAL initialiser `UIBinding binding = {NULL, FLOAT};` (NULL ->
 * slot 0 `address`, FLOAT -> slot 1 `data_type`); ALL appended members
 * value-initialise to zero so `kind == UI_BIND_SRC_NONE`, `query == NULL`,
 * `write == NULL`, and `write_key == 0`. Reordering would break that init.
 *
 * The optional dynamic WRITE sink (`write` + `write_key`) is appended strictly
 * after the existing tail (0.2): a resolver that returns a query SOURCE plus a
 * non-NULL `write` makes a `<TextField>` an editable field that commits on ENTER
 * through a BIND_SINK_CALLBACK. Existing address resolvers (e.g. the `{NULL, FLOAT}`
 * one) leave `write == NULL` and so remain read-only, unchanged. The resolver
 * typedef signature is NOT changed; only the returned struct grows (additive).
 */
typedef struct {
    void *address;              // slot 0 (unchanged): UI_BIND_SRC_ADDRESS data address
    DataType data_type;         // slot 1 (unchanged): address deref interpretation
    UIBindingSourceKind kind;   // appended: which payload is valid; 0 == UI_BIND_SRC_NONE
    BindingQueryFn query;       // appended: UI_BIND_SRC_QUERY display read fn
    int query_key;              // appended: UI_BIND_SRC_QUERY opaque key (e.g. DebugOverlayId)
    BindingValueType value_type;// appended: query advisory value type (query owns the actual type)
    BindingSinkFn write;        // appended (0.2): optional dynamic write callback (NULL => read-only)
    int write_key;              // appended (0.2): opaque key for the write callback
} UIBinding;

/**
 * Binding resolver callback.
 * Resolves a binding string (e.g., "physics.width") to a UIBinding describing an
 * address source OR a query source. Returns a UIBinding with kind=UI_BIND_SRC_NONE
 * and address=NULL if not resolved. The signature is unchanged; only the returned
 * struct grew (additive) - existing address resolvers compile without edits.
 * 
 * @param binding_string XML binding attribute value (e.g., "physics.width")
 * @param ctx Shared loader context
 * @return UIBinding describing the resolved source, or an unresolved UIBinding
 */
typedef UIBinding (*UIBindingResolver)(const char *binding_string, UILoaderContext *ctx);

// ============================================================================
// Action Resolution
// ============================================================================

/**
 * Action resolver callback.
 * Resolves an action string (e.g., "toggle-debug-dashboard") to a CommandType code.
 * Returns 0 (CMD_NONE) if action is not resolved.
 * 
 * @param action_string XML action attribute value (e.g., "toggle-debug-dashboard")
 * @param ctx Shared loader context
 * @return CommandType code (see command_system.h), or 0 (CMD_NONE) if not resolved
 */
typedef int (*UICommandResolver)(const char *action_string, UILoaderContext *ctx);

/**
 * Kind of sink an action= attribute resolved to.
 */
typedef enum {
    UI_BIND_SINK_NONE = 0,  // unresolved / absent
    UI_BIND_SINK_COMMAND,   // command(command_code, NULL)
    UI_BIND_SINK_CALLBACK,  // write(write_key, value)
} UIBindingSinkKind;

/**
 * Widened action (sink) descriptor returned by the action resolver.
 * Carries a command code OR a callback write, tagged by kind.
 */
typedef struct {
    UIBindingSinkKind kind;     // which payload below is valid
    int command_code;           // UI_BIND_SINK_COMMAND: CommandType code (0 => none)
    BindingSinkFn write;        // UI_BIND_SINK_CALLBACK: panel store fn
    int write_key;              // UI_BIND_SINK_CALLBACK: opaque key
    BindingValueType value_type;// callback: parse/commit value type
} UIAction;

/**
 * Action (sink) resolver callback.
 * NEW, opt-in: resolves an action= string to a sink descriptor (command OR callback).
 * Hosts that only need command sinks keep using resolve_command instead; this is an
 * additive opt-in for hosts that need callback sinks.
 *
 * @param action_string XML action attribute value
 * @param ctx Shared loader context
 * @return UIAction describing the resolved sink, or an UI_BIND_SINK_NONE action
 */
typedef UIAction (*UIActionResolver)(const char *action_string, UILoaderContext *ctx);

// ============================================================================
// View-Type Resolution
// ============================================================================

/**
 * View-type resolver callback.
 * Resolves a view-type string (e.g. "LPANEL_DRAW_VIEW") to a ViewType code.
 * Kept as an application-supplied callback so the generic loader never names
 * application-specific ViewType enum values.
 *
 * @param type_string XML type attribute value (e.g. "LPANEL_STATE_VIEW")
 * @param resolved Output: set true when the string was recognised, false otherwise
 * @param ctx Shared loader context
 * @return Resolved ViewType; a defaulted/zero ViewType when unrecognised
 */
typedef ViewType (*UIViewTypeResolver)(const char *type_string, bool *resolved,
                                       UILoaderContext *ctx);

/**
 * Shared context during XML tree parsing.
 * Holds palette reference and callbacks for resolving bindings/actions.
 */
typedef struct UILoaderContext {
    mxml_node_t *root;              // Root XML node (document element)
    const UIPalette *palette;       // UI palette for element styling
    const char *source_file;        // Source filename (for debug messages)
    UIBindingResolver resolve_binding;  // Callback to resolve binding strings (can be NULL)
    UICommandResolver resolve_command;    // Callback to resolve action strings (legacy command sink; can be NULL)
    UIActionResolver resolve_action;      // Callback to resolve action strings to a sink descriptor (opt-in; can be NULL)
    UIViewTypeResolver resolve_view_type; // Callback to resolve view-type strings (can be NULL)
    void *user_data;                // Optional user data for callbacks
} UILoaderContext;

// ============================================================================
// Common Element Attributes
// ============================================================================

/**
 * Common attributes extracted ONCE by the skeleton for EVERY element, then handed
 * to the per-type builder. The builder consumes the parts it needs at construct
 * time (e.g. a container passes size/offset/layout into its constructor); the
 * skeleton applies the universally-safe-after parts (id, enabled, and offset where
 * the builder did not consume it) once the builder returns.
 *
 * `size` is SEEDED by the skeleton from the registry's per-type default_size BEFORE
 * extraction, because UILoader_ExtractCommonAttrs only overwrites an axis mode /
 * dimensions when the markup actually authors it; the per-type default must already
 * be in place for the "attribute absent" case to reproduce historic behaviour.
 *
 * `offset_consumed` is the double-application guard: a builder that passed `offset`
 * into its constructor sets it true, so the skeleton skips the post-apply.
 */
typedef struct {
    char id[MAX_UI_ELEMENT_ID];   // id= (empty string when absent)
    Size size;                    // size=/width=/height= with per-axis mode inference (seeded with the per-type default)
    Offset offset;                // offset=/offset-mode= (+ legacy x/y/offset-type); default zero fixed
    bool enabled;                 // enabled= ("false" => false; default true)
    const Spacing *layout;        // layout= -> shared Spacing constant (never NULL; defaults to &ui_standard_stack_spacing)
    bool offset_consumed;         // set by the builder when it passed `offset` into its constructor (skeleton must not re-apply)
} UICommonAttrs;

// ============================================================================
// Element Builder Registry
// ============================================================================

/**
 * Result of a builder: the element the tag represents (`element`) plus the optional
 * inner control where type-specific data/bindings attach (`bind_target`).
 *
 * CONTRACT:
 *  - `element` is the OUTER element: what the skeleton stamps common attrs onto and
 *    recurses children into. NULL => the builder failed (skeleton aborts this node).
 *  - `bind_target` is the inner CONTROL. A builder that is backed by a single element
 *    leaves it NULL; the skeleton then treats bind_target as == element. Only a
 *    composite builder (e.g. TextField) sets it to a descendant of `element`.
 */
typedef struct {
    UIElement *element;      // outer element (NULL on failure)
    UIElement *bind_target;  // inner control; NULL means "same as element"
} UIBuildResult;

/**
 * Element type builder callback (template-method variable step).
 *
 * The skeleton has ALREADY extracted all common attributes into `common` (and
 * applied nothing yet). The builder:
 *   (1) reads its OWN special attributes from `node`,
 *   (2) calls the correct constructor, passing the common fields it needs at
 *       construct time (size always; offset+layout for containers),
 *   (3) sets type-specific union data / assembles bindings / records side-table
 *       entries,
 *   (4) sets common->offset_consumed = true IF it passed common->offset into its
 *       constructor (so the skeleton does not re-apply offset).
 * The skeleton then applies the common remainder (id, enabled, and offset unless
 * consumed) to the OUTER element.
 *
 * The builder returns a UIBuildResult: `element` is the outer element the tag
 * represents (NULL on error), and `bind_target` is the optional inner control. A
 * single-element builder leaves `bind_target` NULL (the skeleton then treats the
 * control as the element itself); a composite builder (e.g. TextField) sets it to
 * the descendant control that owns the type-specific data/bindings.
 *
 * @param node XML element node (mxml_node_t with type MXML_TYPE_ELEMENT)
 * @param parent Parent UIElement (NULL if root)
 * @param palette UI palette for styling
 * @param ctx Shared loader context
 * @param common Common attributes pre-extracted by the skeleton (never NULL)
 * @return UIBuildResult with the outer element (NULL on error) and optional control
 */
typedef UIBuildResult (*UIElementBuilder)(mxml_node_t *node, UIElement *parent,
                                          const UIPalette *palette,
                                          UILoaderContext *ctx,
                                          UICommonAttrs *common);

/**
 * Registry entry: maps XML tag names to builder functions.
 */
typedef struct {
    const char *tag_name;           // XML element tag (e.g., "Button", "TextField")
    UIElementBuilder builder;       // Construction callback
    Size default_size;              // Per-type size default; seeds common->size before extraction
} UIElementBuilderRegistry;

// ============================================================================
// Loader API
// ============================================================================

/**
 * Load and parse a UI tree from an XML file with custom resolvers.
 * 
 * Reads the XML file, creates a tree of UIElements by dispatching to registered builders,
 * and returns the root element. Uses provided resolvers for bindings and commands.
 * Caller owns the returned tree and must dispose it.
 * 
 * @param filepath Path to XML file (absolute or relative to cwd)
 * @param palette UI palette for element styling (NULL = use default)
 * @param resolve_binding Optional callback to resolve binding strings (can be NULL)
 * @param resolve_command Optional callback to resolve command strings (can be NULL)
 * @param resolve_view_type Optional callback to resolve view-type strings (can be NULL)
 * @param user_data Optional user data passed to callbacks
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromFileWithResolvers(const char *filepath, const UIPalette *palette,
                                               UIBindingResolver resolve_binding,
                                               UICommandResolver resolve_command,
                                               UIViewTypeResolver resolve_view_type,
                                               void *user_data);

/**
 * Load and parse a UI tree from an XML string with custom resolvers.
 * 
 * Parses the XML string, creates a tree of UIElements by dispatching to registered builders,
 * and returns the root element. Uses provided resolvers for bindings and commands.
 * 
 * @param xml_string XML markup as string
 * @param palette UI palette for element styling (NULL = use default)
 * @param resolve_binding Optional callback to resolve binding strings (can be NULL)
 * @param resolve_command Optional callback to resolve command strings (can be NULL)
 * @param resolve_view_type Optional callback to resolve view-type strings (can be NULL)
 * @param user_data Optional user data passed to callbacks
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromStringWithResolvers(const char *xml_string, const UIPalette *palette,
                                                 UIBindingResolver resolve_binding,
                                                 UICommandResolver resolve_command,
                                                 UIViewTypeResolver resolve_view_type,
                                                 void *user_data);

/**
 * Load and parse a UI tree from an XML file.
 * 
 * Reads the XML file, creates a tree of UIElements by dispatching to registered builders,
 * and returns the root element. Caller owns the returned tree and must dispose it.
 * No bindings or actions will be resolved (resolvers are NULL).
 * 
 * @param filepath Path to XML file (absolute or relative to cwd)
 * @param palette UI palette for element styling (NULL = use default)
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromFile(const char *filepath, const UIPalette *palette);

/**
 * Load and parse a UI tree from an XML string.
 * 
 * Parses the XML string, creates a tree of UIElements by dispatching to registered builders,
 * and returns the root element. No bindings or actions will be resolved (resolvers are NULL).
 * 
 * @param xml_string XML markup as string
 * @param palette UI palette for element styling (NULL = use default)
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromString(const char *xml_string, const UIPalette *palette);

/**
 * Register a new element type builder.
 * 
 * Must be called before loading any XML that uses the element type.
 * Does not take ownership of tag_name or builder; caller is responsible.
 * 
 * @param tag_name XML tag to map (e.g., "CustomElement")
 * @param builder Construction callback
 * @param default_size Per-type size default (seeds common->size before extraction)
 * @return true on success, false if tag already registered or out of space
 */
bool UILoader_RegisterBuilder(const char *tag_name, UIElementBuilder builder,
                              Size default_size);

// ============================================================================
// Common Attribute Extraction
// ============================================================================

/**
 * Extract common element attributes from an XML node.
 *
 * Fills in id, size, offset, and enabled state from standard XML attributes.
 * Attributes:
 *   - id: element identifier (string)
 *   - size: "W,H" numeric authored dimensions (optional; a lone value mirrors to both axes)
 *   - width/height: per-axis mode keyword "fixed|percent|fill|hug" (optional)
 *   - offset / offset-mode (and legacy x/y/offset-type): position (optional)
 *   - enabled: "true"/"false" (optional, default true)
 * Per-axis inference: an explicit width=/height= wins; else a provided size= value implies
 * fixed on that axis; else the caller's seeded builder default is left untouched.
 *
 * @param node XML element node
 * @param ctx Loader context (for source-aware warnings; may be NULL)
 * @param id_out[MAX_UI_ELEMENT_ID] Output element ID (empty string if not present)
 * @param size_out[optional] Output size spec
 * @param offset_out[optional] Output position/offset
 * @param enabled_out[optional] Output enabled flag (default true)
 * @param layout_out[optional] Output child spacing from layout= (shared Spacing constant, never NULL when set)
 */
void UILoader_ExtractCommonAttrs(mxml_node_t *node, UILoaderContext *ctx, char *id_out, Size *size_out, Offset *offset_out, bool *enabled_out, const Spacing **layout_out);

// ============================================================================
// Default Builder Registration
// ============================================================================

/**
 * Register all default element type builders (Button, Label, TextField, Section, etc.).
 * Must be called once before loading any XML files.
 */
void UILoader_RegisterDefaultBuilders(void);

// ============================================================================
// Post-Load Processing
// ============================================================================

/**
 * Extract all View containers from an XML-loaded UI tree.
 * 
 * Recursively searches the tree for UIElements with type==UI_ELEMENT_VIEW,
 * creates View structs wrapping each container, and returns an array of Views.
 * 
 * The caller must free the returned array but NOT the View structs (they are owned by caller).
 * The View structs reference the containers from the tree.
 * 
 * @param root Root UIElement from UILoader_LoadFromFile*
 * @param out_count Output: number of Views found
 * @return Allocated array of View pointers, or NULL if no Views found or error
 */
View **UILoader_ExtractViews(UIElement *root, size_t *out_count);

/**
 * Build and register a ViewSelector on `host` from the <ViewSelector>/<Option>
 * metadata captured during the most recent load.
 *
 * Resolves each Option's target view id to the index of the matching View in
 * host->views (two-pass, since Options precede Views in document order),
 * allocates the ViewSelector plus its buttons[]/view_indices[] arrays, and wires
 * each enumerate button exactly like ViewHostSystem_CreateViewSelector:
 *   button->on_click              = HandleViewHostSelectorClick
 *   button->interaction_user_data = &selector->view_indices[i]
 *   button->data.button.data_bind = selector
 * so Option clicks drive ViewHostSystem_SelectView.
 *
 * @param host Host already populated with the extracted Views
 * @param callback on_view_selected callback for the selector
 * @return The created selector (also pushed to host->selectors), or NULL on error
 */
ViewSelector *UILoader_BuildSelectorFromMarkup(ViewHostSystem *host,
                                               ViewSelectionCallback callback);

/**
 * Resolve a View id string to the index of the matching View in host->views,
 * using the view-id metadata captured during the most recent load.
 *
 * @param host Host populated with the extracted Views
 * @param id_string View id to look up (e.g. "state_view")
 * @return Index of the matching View, or -1 when unresolved
 */
int UILoader_ResolveViewIndexById(ViewHostSystem *host, const char *id_string);

/**
 * Return the initialView id recorded from the most recent load's <ViewHost>.
 *
 * @return The initialView id string, or NULL when none was recorded
 */
const char *UILoader_GetInitialViewId(void);

/**
 * Find the first element at or below `root` whose id= equals `id`.
 *
 * Depth-first, pre-order, first-match-wins tree walk (mirrors the
 * UILoader_CollectViewContainers recursion). Domain-free: compares only the
 * generic UIElement.id string, so it works on any UIElement tree and names no
 * application concept. Duplicate ids are not diagnosed (first in pre-order wins).
 *
 * @param root Root UIElement to search at and below (may be NULL)
 * @param id   The id string to match (NULL or empty => no match)
 * @return The matching element, or NULL when root/id is NULL/empty or no match
 */
UIElement *UILoader_FindById(UIElement *root, const char *id);

#ifdef __cplusplus
}
#endif

#endif // !UI_LOADER_H

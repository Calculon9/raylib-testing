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
 * Binding metadata: address and type of a data member to bind a UI element to.
 */
typedef struct {
    void *address;          // Address of the bound data member
    DataType data_type;     // Type of the data (INT, FLOAT, VECTOR2D, etc.)
} UIBinding;

/**
 * Binding resolver callback.
 * Resolves a binding string (e.g., "physics.width") to a UIBinding with the
 * actual data address and type. Returns a UIBinding with address=NULL if not resolved.
 * 
 * @param binding_string XML binding attribute value (e.g., "physics.width")
 * @param ctx Shared loader context
 * @return UIBinding with resolved address and type, or {NULL, ...} if not resolved
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
 * Shared context during XML tree parsing.
 * Holds palette reference and callbacks for resolving bindings/actions.
 */
typedef struct UILoaderContext {
    mxml_node_t *root;              // Root XML node (document element)
    const UIPalette *palette;       // UI palette for element styling
    const char *source_file;        // Source filename (for debug messages)
    UIBindingResolver resolve_binding;  // Callback to resolve binding strings (can be NULL)
    UICommandResolver resolve_command;    // Callback to resolve action strings (can be NULL)
    void *user_data;                // Optional user data for callbacks
} UILoaderContext;

// ============================================================================
// Element Builder Registry
// ============================================================================

/**
 * Element type builder callback.
 * Called to construct a UIElement from an XML node.
 * 
 * @param node XML element node (mxml_node_t with type MXML_TYPE_ELEMENT)
 * @param parent Parent UIElement (NULL if root)
 * @param palette UI palette for styling
 * @param ctx Shared loader context
 * @return Constructed UIElement, or NULL on error
 */
typedef UIElement *(*UIElementBuilder)(mxml_node_t *node, UIElement *parent,
                                       const UIPalette *palette,
                                       UILoaderContext *ctx);

/**
 * Registry entry: maps XML tag names to builder functions.
 */
typedef struct {
    const char *tag_name;           // XML element tag (e.g., "Button", "TextField")
    UIElementBuilder builder;       // Construction callback
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
 * @param user_data Optional user data passed to callbacks
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromFileWithResolvers(const char *filepath, const UIPalette *palette,
                                               UIBindingResolver resolve_binding,
                                               UICommandResolver resolve_command,
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
 * @param user_data Optional user data passed to callbacks
 * @return Root UIElement, or NULL on error
 */
UIElement *UILoader_LoadFromStringWithResolvers(const char *xml_string, const UIPalette *palette,
                                                 UIBindingResolver resolve_binding,
                                                 UICommandResolver resolve_command,
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
 * @return true on success, false if tag already registered or out of space
 */
bool UILoader_RegisterBuilder(const char *tag_name, UIElementBuilder builder);

// ============================================================================
// Common Attribute Extraction
// ============================================================================

/**
 * Extract common element attributes from an XML node.
 * 
 * Fills in id, size, offset, and enabled state from standard XML attributes.
 * Attributes:
 *   - id: element identifier (string)
 *   - size: "WIDTHxHEIGHT" or "W,H" (fixed) or "W%,H%" (percent) (optional)
 *   - x, y: position in pixels or percent (optional)
 *   - enabled: "true"/"false" (optional, default true)
 * 
 * @param node XML element node
 * @param id_out[MAX_UI_ELEMENT_ID] Output element ID (empty string if not present)
 * @param size_out[optional] Output size spec
 * @param offset_out[optional] Output position/offset
 * @param enabled_out[optional] Output enabled flag (default true)
 */
void UILoader_ExtractCommonAttrs(mxml_node_t *node, char *id_out, Size *size_out, Offset *offset_out, bool *enabled_out);

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

#ifdef __cplusplus
}
#endif

#endif // !UI_LOADER_H

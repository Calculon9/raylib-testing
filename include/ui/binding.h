#ifndef UI_BINDING_H
#define UI_BINDING_H

#include <stdbool.h>
#include <stddef.h>
#include "math/cvectors.h" // Vector2d is embedded by value in BindingValue (a forward decl will NOT compile)

// Simple data-binding and validation API for UI text inputs.
// A Binder holds a pointer to target data and a converter/validator function.

// ---- Canonical value-type enum ----------------------------------------------------------
// BindingValueType is the ONE canonical enum used throughout the symmetric binding core.
// The historic BindingType / BINDING_* names are retained as back-compat aliases (identical
// integer values 0..4) so existing callers (ResolveBindingType, Binder.type) compile
// unchanged. They are aliases, NOT deprecated - no deprecation annotation is applied.
typedef enum BindingValueType
{
    BIND_NONE = 0,
    BIND_INT,
    BIND_FLOAT,
    BIND_VECTOR2D,
    BIND_STRING,
} BindingValueType;

// ---- Back-compat aliases (identical integer values 0..4; no behaviour change) -----------
typedef BindingValueType BindingType; // for the existing Binder.type field + callers
#define BINDING_NONE     BIND_NONE
#define BINDING_INT      BIND_INT
#define BINDING_FLOAT    BIND_FLOAT
#define BINDING_VECTOR2D BIND_VECTOR2D
#define BINDING_STRING   BIND_STRING

typedef bool (*ValidatorFn)(const char *text, void *out_value, void *user_data);

typedef struct Binder
{
    BindingType type;
    void *target; // pointer to bound storage
    ValidatorFn validator; // returns true if valid and writes parsed value to out_value
    void *user_data; // optional validator context
} Binder;

Binder *Binder_Create(BindingType type, void *target, ValidatorFn validator, void *user_data);
void Binder_Destroy(Binder *b);

// Validate text and, on success, write to the target and return true.
bool Binder_ValidateAndWrite(Binder *b, const char *text);

// Common validators for specific constraints
// IntRange: validates that parsed int is within [min, max] inclusive
bool ValidatorIntRange(const char *text, void *out_value, void *user_data);

// IntPositive: validates that parsed int is > 0
bool ValidatorIntPositive(const char *text, void *out_value, void *user_data);

// FloatRange: validates that parsed float is within [min, max] inclusive
bool ValidatorFloatRange(const char *text, void *out_value, void *user_data);

//------------------------------------------------------------------------------------------
// Symmetric binding core: source x sink x format/parse
//
// A binding reads a value INTO a widget (source -> format -> text) and writes a value OUT of
// a widget (text -> parse -> sink) through one generic abstraction. The core is deliberately
// free of game-subsystem dependencies: the debug query and command dispatch are supplied by
// the caller as plain function pointers keyed on opaque ints.
//------------------------------------------------------------------------------------------

// VALUE: a small tagged union, the common currency between source/sink/codec.
typedef struct BindingValue
{
    BindingValueType type;
    union
    {
        int i;
        float f;
        Vector2d v;    // complete type (binding.h includes math/cvectors.h)
        const char *s; // non-owning; points into caller storage for the duration of a call
    } as;
} BindingValue;

// SOURCE: how to READ the current value for display.
typedef enum BindingSourceKind
{
    BIND_SRC_NONE = 0,
    BIND_SRC_ADDRESS,
    BIND_SRC_QUERY,
} BindingSourceKind;

// Opaque query: caller supplies fn + int key. The layer never interprets the key
// (e.g. an IsDebugEnabled shim keyed on a DebugOverlayId cast to int).
typedef int (*BindingQueryFn)(int key);

typedef struct BindingSource
{
    BindingSourceKind kind;
    BindingValueType value_type; // how to interpret the address / query result
    void *address;               // BIND_SRC_ADDRESS: *(T*)address
    BindingQueryFn query;        // BIND_SRC_QUERY: query(key)
    int query_key;               // opaque key for query
} BindingSource;

// SINK: how to WRITE a changed value back.
typedef enum BindingSinkKind
{
    BIND_SINK_NONE = 0,
    BIND_SINK_ADDRESS,
    BIND_SINK_COMMAND,
} BindingSinkKind;

// Opaque command dispatch: caller supplies fn + int code. The default binding to the
// command system is provided by the integration/consumer layer, NOT by binding.c.
typedef void (*BindingCommandFn)(int code, const void *data);

typedef struct BindingSink
{
    BindingSinkKind kind;
    BindingValueType value_type; // address sink: type to write; command sink: ignored
    void *address;               // BIND_SINK_ADDRESS: *(T*)address = value
    ValidatorFn validator;       // optional, same contract as the Binder validators
    void *validator_ctx;         // == the old Binder.user_data
    BindingCommandFn command;    // BIND_SINK_COMMAND: command(code, NULL)
    int command_code;            // CommandType code, kept opaque here (0 => no dispatch)
} BindingSink;

// The whole binding: a display read, a commit write, and the float formatting precision.
typedef struct Binding
{
    BindingSource source; // display read
    BindingSink sink;      // commit write (may be BIND_SINK_NONE for read-only)
    int precision;         // float formatting precision; 0 => default
} Binding;

// FORMAT: value -> text buffer. Returns false on unknown type or NULL out.
bool Binding_FormatValue(BindingValue value, int precision, char *out, size_t out_bytes);

// PARSE: text -> value, honouring value_type and the optional validator.
// Returns false on parse/validation failure (caller reverts); *out left type BIND_NONE.
bool Binding_ParseText(const char *text, BindingValueType type,
                       ValidatorFn validator, void *validator_ctx, BindingValue *out);

// READ: resolve the source to a current BindingValue (address deref or query call).
BindingValue Binding_ReadSource(const BindingSource *src);

// WRITE: push a BindingValue to the sink (address store or command dispatch).
// Returns false if the sink rejects (e.g. NULL address) - caller reverts.
bool Binding_WriteSink(const BindingSink *sink, BindingValue value);

// COMMIT: the full UI->data path for an editable widget. text -> parse -> validate -> sink.
bool Binding_Commit(const Binding *b, const char *text);

// REFRESH: the full data->UI path. read source -> format -> write into out buffer.
// NOTE: focused-skip is the CALLER's responsibility; this fn always formats.
bool Binding_RefreshText(const Binding *b, char *out, size_t out_bytes);

// Free a heap-allocated Binding and NULL the caller's pointer (symmetric with Binder_Destroy).
void Binding_Destroy(Binding **b);

#endif // UI_BINDING_H

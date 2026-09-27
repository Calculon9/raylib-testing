#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "cJSON.h"
#include "entities/entity_prefab.h"
#include "raylib.h"

// Read a finite JSON number in range and convert it to a float.
static bool ReadPrefabFloat(const cJSON *value, double minimum, double maximum,
							float *out_value)
{
	if (!cJSON_IsNumber(value) || !out_value || !isfinite(value->valuedouble) ||
		value->valuedouble < minimum || value->valuedouble > maximum ||
		value->valuedouble < -FLT_MAX || value->valuedouble > FLT_MAX)
	{
		return false;
	}

	*out_value = (float)value->valuedouble;
	return isfinite(*out_value);
}

// Read one named numeric member from a JSON object.
static bool ReadPrefabFloatMember(const cJSON *object, const char *name,
								  double minimum, double maximum, float *out_value)
{
	return cJSON_IsObject(object) &&
		   ReadPrefabFloat(cJSON_GetObjectItemCaseSensitive(object, name),
						   minimum, maximum, out_value);
}

// Read a two-element numeric array as a vector.
static bool ReadPrefabVector2(const cJSON *object, const char *name, Vector2d *out_vector)
{
	const cJSON *array = cJSON_GetObjectItemCaseSensitive(object, name);
	if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != 2 || !out_vector)
	{
		return false;
	}

	float x = 0.0f;
	float y = 0.0f;
	if (!ReadPrefabFloat(cJSON_GetArrayItem(array, 0), -FLT_MAX, FLT_MAX, &x) ||
		!ReadPrefabFloat(cJSON_GetArrayItem(array, 1), -FLT_MAX, FLT_MAX, &y))
	{
		return false;
	}

	*out_vector = (Vector2d){x, y};
	return true;
}

// Map stable prefab role names to the engine's composable role bits.
static bool ReadEntityRoles(const cJSON *array, EntityRoleFlags *out_roles)
{
	if (!cJSON_IsArray(array) || !out_roles)
	{
		return false;
	}

	EntityRoleFlags roles = ENTITY_ROLE_NONE;
	int role_count = cJSON_GetArraySize(array);
	for (int role_index = 0; role_index < role_count; role_index++)
	{
		const char *role_name = cJSON_GetStringValue(cJSON_GetArrayItem(array, role_index));
		if (!role_name)
		{
			return false;
		}

		if (strcmp(role_name, "wall") == 0)
		{
			roles |= ENTITY_ROLE_WALL;
		}
		else if (strcmp(role_name, "newtonoid") == 0)
		{
			roles |= ENTITY_ROLE_NEWTONOID;
		}
		else if (strcmp(role_name, "projectile") == 0)
		{
			roles |= ENTITY_ROLE_PROJECTILE;
		}
		else if (strcmp(role_name, "effect") == 0)
		{
			roles |= ENTITY_ROLE_EFFECT;
		}
		else if (strcmp(role_name, "camera") == 0)
		{
			roles |= ENTITY_ROLE_CAMERA;
		}
		else
		{
			return false;
		}
	}

	*out_roles = roles;
	return true;
}

// Read a non-negative integral cooldown value from the portal component.
static bool ReadPrefabCooldown(const cJSON *portal, int *out_cooldown_frames)
{
	const cJSON *value = cJSON_GetObjectItemCaseSensitive(portal, "cooldown_frames");
	if (!cJSON_IsNumber(value) || !out_cooldown_frames ||
		!isfinite(value->valuedouble) || value->valuedouble < 0.0 ||
		value->valuedouble > INT_MAX)
	{
		return false;
	}

	int cooldown_frames = (int)value->valuedouble;
	if ((double)cooldown_frames != value->valuedouble)
	{
		return false;
	}

	*out_cooldown_frames = cooldown_frames;
	return true;
}

// Parse physics parameters.
static bool ParsePhysicsComponent(const cJSON *physics, Newtonoid2dParams *out_physics)
{
	if (!cJSON_IsObject(physics) || !out_physics)
	{
		return false;
	}

	Newtonoid2dParams result = {0};
	result.shape_type = SHAPE_AUTO;
	result.vertice_count = MAX_SHAPE_VERTICES;
	result.anchor_position = ZERO_VECTOR_2D;

	if (!ReadPrefabFloatMember(physics, "width", 0.0, FLT_MAX, &result.width) ||
		!ReadPrefabFloatMember(physics, "height", 0.0, FLT_MAX, &result.height) ||
		result.width <= 0.0f || result.height <= 0.0f ||
		!ReadPrefabFloatMember(physics, "mass", 0.0, FLT_MAX, &result.mass) ||
		!ReadPrefabFloatMember(physics, "restitution", 0.0, 1.0, &result.restitution) ||
		!ReadPrefabFloatMember(physics, "friction", 0.0, FLT_MAX, &result.friction) ||
		!ReadPrefabVector2(physics, "velocity", &result.velocity) ||
		!ReadPrefabVector2(physics, "acceleration", &result.acceleration))
	{
		return false;
	}

	*out_physics = result;
	return true;
}

// Parse portal component from prefab JSON.
static bool ParsePortalComponent(const cJSON *portal, PortalComponentParams *out_params)
{
	if (!cJSON_IsObject(portal) || !out_params)
	{
		return false;
	}

	PortalComponentParams result = {0};
	if (!ReadEntityRoles(cJSON_GetObjectItemCaseSensitive(portal, "entrant_roles"),
						 &result.entrant_roles) || !ReadPrefabCooldown(portal, &result.cooldown_frames))
	{
		return false;
	}

	*out_params = result;
	return true;
}

// Parse rotor component from prefab JSON.
static bool ParseRotorComponent(const cJSON *rotor, RotorComponentParams *out_params)
{
	if (!cJSON_IsObject(rotor) || !out_params)
	{
		return false;
	}

	// Rotor component is currently a placeholder; initialise to zero.
	*out_params = (RotorComponentParams){0};
	return true;
}

// Parse gear component from prefab JSON.
static bool ParseGearComponent(const cJSON *gear, GearComponentParams *out_params)
{
	if (!cJSON_IsObject(gear) || !out_params)
	{
		return false;
	}

	// Gear component is currently a placeholder; initialise to zero.
	*out_params = (GearComponentParams){0};
	return true;
}

// Parse health configuration from prefab JSON.
static bool ParseHealthComponent(const cJSON *health, HealthComponentParams *out_params)
{
	if (!cJSON_IsObject(health) || !out_params)
	{
		return false;
	}

	HealthComponentParams result = {0};
	if (!ReadPrefabFloatMember(health, "max_health", 0.0, FLT_MAX, &result.max_health) ||
		result.max_health <= 0.0f)
	{
		return false;
	}

	*out_params = result;
	return true;
}

// Decode a generic prefab into entity creation parameters by dispatching component parsers.
static bool DecodePrefab(const cJSON *root, EntityCreateParams *out_params)
{
	if (!cJSON_IsObject(root) || !out_params)
	{
		return false;
	}

    // Resolve all top-level fields.
	const cJSON *schema_version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
	const cJSON *prefab_id = cJSON_GetObjectItemCaseSensitive(root, "prefab_id");
	const cJSON *components = cJSON_GetObjectItemCaseSensitive(root, "components");

	// Validate schema and top-level fields.
	if (!cJSON_IsNumber(schema_version) || schema_version->valuedouble != 1.0 ||
		!cJSON_IsString(prefab_id) || !prefab_id->valuestring ||
		prefab_id->valuestring[0] == '\0' || !cJSON_IsObject(components))
	{
		return false;
	}

	const cJSON *physics = cJSON_GetObjectItemCaseSensitive(components, "physics");
	if (!cJSON_IsObject(physics))
	{
		return false;
	}

	EntityCreateParams params = {0};

	// Parse physics (common to all entity types).
	if (!ParsePhysicsComponent(physics, &params.physics))
	{
		return false;
	}

	// Detect and parse portal component if present.
	const cJSON *portal = cJSON_GetObjectItemCaseSensitive(components, "portal");
	if (cJSON_IsObject(portal))
	{
		if (!ParsePortalComponent(portal, &params.portal_params))
		{
			return false;
		}
		params.component_flags |= CREATION_COMPONENT_PORTAL;
	}

	// Detect and parse rotor component if present.
	const cJSON *rotor = cJSON_GetObjectItemCaseSensitive(components, "rotor");
	if (cJSON_IsObject(rotor))
	{
		if (!ParseRotorComponent(rotor, &params.rotor_params))
		{
			return false;
		}
		params.component_flags |= CREATION_COMPONENT_ROTOR;
	}

	// Detect and parse gear component if present.
	const cJSON *gear = cJSON_GetObjectItemCaseSensitive(components, "gear");
	if (cJSON_IsObject(gear))
	{
		if (!ParseGearComponent(gear, &params.gear_params))
		{
			return false;
		}
		params.component_flags |= CREATION_COMPONENT_GEAR;
	}

	// Parse health only when the prefab declares the component.
	const cJSON *health = cJSON_GetObjectItemCaseSensitive(components, "health");
	if (health)
	{
		if (!ParseHealthComponent(health, &params.health_params))
		{
			return false;
		}
		params.component_flags |= CREATION_COMPONENT_HEALTH;
	}

	*out_params = params;
	return true;
}

// Load a JSON prefab without modifying the output parameters on failure.
bool EntityPrefab_LoadFile(const char *file_path, EntityCreateParams *out_params)
{
	if (!file_path || !out_params)
	{
		return false;
	}

	char *file_text = LoadFileText(file_path);
	if (!file_text)
	{
		LOG_WARN("Cannot read entity prefab: %s\n", file_path);
		return false;
	}

	cJSON *root = cJSON_Parse(file_text);
	bool is_valid = DecodePrefab(root, out_params);
	if (root)
	{
		cJSON_Delete(root);
	}
	UnloadFileText(file_text);

	if (!is_valid)
	{
		LOG_WARN("Invalid or unsupported entity prefab: %s\n", file_path);
	}

	return is_valid;
}

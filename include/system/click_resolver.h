/**********************************************************************************************
*
CLICK RESOLVER MODULE
*
Generic hierarchical click resolution across nested coordinate spaces.
*
**********************************************************************************************/
#ifndef CLICK_RESOLVER_H
#define CLICK_RESOLVER_H

#include "common/common.h"
#include "math/cvectors.h"

typedef void *ClickNodeHandle;

typedef struct ClickResolveResult
{
    bool hit;
    ClickNodeHandle node;
    Vector2d local_point;
    int depth;
} ClickResolveResult;

typedef struct ClickHierarchyAdapter
{
    bool (*IsNodeInteractive)(void *context, ClickNodeHandle node);
    bool (*ContainsPointInParent)(void *context, ClickNodeHandle node, Vector2d point_in_parent);
    Vector2d (*PointParentToLocal)(void *context, ClickNodeHandle node, Vector2d point_in_parent);
    ClickNodeHandle (*GetFirstChild)(void *context, ClickNodeHandle node);
    ClickNodeHandle (*GetNextSibling)(void *context, ClickNodeHandle parent, ClickNodeHandle node);
    bool (*AcceptNodeHit)(void *context, ClickNodeHandle node, Vector2d local_point);
} ClickHierarchyAdapter;

bool ClickResolver_ResolveDeepest(void *context,
                                  ClickNodeHandle root,
                                  Vector2d point_in_root_parent,
                                  const ClickHierarchyAdapter *adapter,
                                  ClickResolveResult *out_result);

#endif
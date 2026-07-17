/**********************************************************************************************
*
CLICK RESOLVER MODULE
*
**********************************************************************************************/

#include "system/click_resolver.h"

static ClickResolveResult ResolveDeepestRecursive(void *context,
                                                  ClickNodeHandle node,
                                                  Vector2d point_in_parent,
                                                  const ClickHierarchyAdapter *adapter,
                                                  int depth)
{
    ClickResolveResult miss = {0};
    if (!adapter || !node)
    {
        return miss;
    }

    if (adapter->IsNodeInteractive && !adapter->IsNodeInteractive(context, node))
    {
        return miss;
    }

    if (adapter->ContainsPointInParent && !adapter->ContainsPointInParent(context, node, point_in_parent))
    {
        return miss;
    }

    Vector2d local_point = point_in_parent;
    if (adapter->PointParentToLocal)
    {
        local_point = adapter->PointParentToLocal(context, node, point_in_parent);
    }

    ClickResolveResult deepest_child_hit = {0};
    if (adapter->GetFirstChild && adapter->GetNextSibling)
    {
        ClickNodeHandle child = adapter->GetFirstChild(context, node);
        while (child)
        {
            ClickResolveResult child_hit = ResolveDeepestRecursive(context, child, local_point, adapter, depth + 1);
            if (child_hit.hit)
            {
                // Later siblings replace earlier ones so top-most/last-drawn wins.
                deepest_child_hit = child_hit;
            }

            child = adapter->GetNextSibling(context, node, child);
        }
    }

    if (deepest_child_hit.hit)
    {
        return deepest_child_hit;
    }

    bool accept_node_hit = true;
    if (adapter->AcceptNodeHit)
    {
        accept_node_hit = adapter->AcceptNodeHit(context, node, local_point);
    }

    if (!accept_node_hit)
    {
        return miss;
    }

    ClickResolveResult hit = {0};
    hit.hit = true;
    hit.node = node;
    hit.local_point = local_point;
    hit.depth = depth;
    return hit;
}

bool ClickResolver_ResolveDeepest(void *context,
                                  ClickNodeHandle root,
                                  Vector2d point_in_root_parent,
                                  const ClickHierarchyAdapter *adapter,
                                  ClickResolveResult *out_result)
{
    if (!out_result)
    {
        return false;
    }

    *out_result = (ClickResolveResult){0};

    if (!root || !adapter)
    {
        return false;
    }

    ClickResolveResult result = ResolveDeepestRecursive(context, root, point_in_root_parent, adapter, 0);
    *out_result = result;
    return result.hit;
}
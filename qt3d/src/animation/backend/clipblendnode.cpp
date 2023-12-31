// Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "clipblendnode_p.h"
#include <Qt3DAnimation/qabstractanimationclip.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {

namespace Animation {

ClipBlendNode::ClipBlendNode(BlendType blendType)
    : BackendNode(ReadOnly) // Makes sense for now at least
    , m_manager(nullptr)
    , m_blendType(blendType)
{
}

ClipBlendNode::~ClipBlendNode()
{
}

void ClipBlendNode::setClipBlendNodeManager(ClipBlendNodeManager *manager)
{
    m_manager = manager;
}

ClipBlendNode::BlendType Animation::ClipBlendNode::blendType() const
{
    return m_blendType;
}

void ClipBlendNode::setClipResults(Qt3DCore::QNodeId animatorId, const ClipResults &clipResults)
{
    // Do we already have an entry for this animator?
    const qsizetype animatorIndex = m_animatorIds.indexOf(animatorId);
    if (animatorIndex == -1) {
        // Nope, add it
        m_animatorIds.push_back(animatorId);
        m_clipResults.push_back(clipResults);
    } else {
        m_clipResults[animatorIndex] = clipResults;
    }
}

ClipResults ClipBlendNode::clipResults(Qt3DCore::QNodeId animatorId) const
{
    const qsizetype animatorIndex = m_animatorIds.indexOf(animatorId);
    if (animatorIndex != -1)
        return m_clipResults[animatorIndex];
    return ClipResults();
}

/*
    \fn QList<Qt3DCore::QNodeId> ClipBlendNode::currentDependencyIds() const
    \internal

    Each subclass of ClipBlendNode must implement this function such that it
    returns a vector of the ids of ClipBlendNodes upon which is it dependent
    in order to be able to evaluate given its current internal state.

    For example, a subclass implementing a simple lerp blend between two
    other nodes, would always return the ids of the nodes between which it
    is lerping.

    A more generalised lerp node that is capable of lerping between a
    series of nodes would return the ids of the two nodes that correspond
    to the blend values which sandwich the currently set blend value.

    The animation handler will submit a job that uses this function to
    build a list of clips that must be evaluated in order to later
    evaluate the entire blend tree. In this way, the clips can all be
    evaluated in one pass, and the tree in a subsequent pass.
*/

/*
    \fn QList<Qt3DCore::QNodeId> ClipBlendNode::allDependencyIds() const
    \internal

    Similar to currentDependencyIds() but returns the ids of all potential
    dependency nodes, not just those that are dependencies given the current
    internal state. For example a generalised lerp node would return the ids
    of all nodes that can participate in the lerp for any value of the blend
    parameter. Not just those bounding the current blend value.
*/

/*
    \internal

    Fetches the ClipResults from the nodes listed in the dependencyIds
    and passes them to the doBlend() virtual function which should be
    implemented in subclasses to perform the actual blend operation.
    The results are then inserted into the clip results for this blend
    node indexed by the \a animatorId.
*/
void ClipBlendNode::blend(Qt3DCore::QNodeId animatorId)
{
    // Obtain the clip results from each of the dependencies
    const QList<Qt3DCore::QNodeId> dependencyNodeIds = currentDependencyIds();
    const qsizetype dependencyCount = dependencyNodeIds.size();
    QList<ClipResults> blendData;
    blendData.reserve(dependencyCount);
    for (const auto &dependencyId : dependencyNodeIds) {
        ClipBlendNode *dependencyNode = clipBlendNodeManager()->lookupNode(dependencyId);
        ClipResults blendDataElement = dependencyNode->clipResults(animatorId);
        blendData.push_back(blendDataElement);
    }

    // Ask the blend node to perform the actual blend operation on the data
    // from the dependencies
    ClipResults blendedResults = doBlend(blendData);
    setClipResults(animatorId, blendedResults);
}

} // Animation

} // Qt3DAnimation

QT_END_NAMESPACE

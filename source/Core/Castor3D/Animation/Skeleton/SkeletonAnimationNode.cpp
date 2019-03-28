#include "Castor3D/Animation/Skeleton/SkeletonAnimationNode.hpp"

#include "Castor3D/Animation/Skeleton/SkeletonAnimation.hpp"

using namespace castor;

namespace castor3d
{
	SkeletonAnimationNode::SkeletonAnimationNode( SkeletonAnimation & animation
		, String const & name )
		: SkeletonAnimationObject{ animation, SkeletonAnimationObjectType::eNode }
		, m_name{ name }
	{
	}

	SkeletonAnimationNode :: ~SkeletonAnimationNode()
	{
	}
}

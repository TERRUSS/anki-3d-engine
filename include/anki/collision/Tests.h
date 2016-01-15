// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/collision/Common.h>

namespace anki
{

/// @addtogroup collision
/// @{

/// Test if two collision shapes collide.
Bool testCollisionShapes(const CollisionShape& a, const CollisionShape& b);
/// @}

} // end namespace anki

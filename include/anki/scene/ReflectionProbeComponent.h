// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/scene/SceneComponent.h>

namespace anki
{

/// @addtogroup scene
/// @{

/// Reflection probe component.
class ReflectionProbeComponent : public SceneComponent
{
public:
	ReflectionProbeComponent(SceneNode* node)
		: SceneComponent(SceneComponent::Type::REFLECTION_PROBE, node)
	{
	}

	static Bool classof(const SceneComponent& c)
	{
		return c.getType() == Type::REFLECTION_PROBE;
	}

	const Vec4& getPosition() const
	{
		return m_pos;
	}

	void setPosition(const Vec4& pos)
	{
		m_pos = pos.xyz0();
	}

	F32 getRadius() const
	{
		ANKI_ASSERT(m_radius > 0.0);
		return m_radius;
	}

	void setRadius(F32 radius)
	{
		ANKI_ASSERT(radius > 0.0);
		m_radius = radius;
	}

	Bool getMarkedForRendering() const
	{
		return m_markedForRendering;
	}

	void setMarkedForRendering(Bool render)
	{
		m_markedForRendering = render;
	}

private:
	Vec4 m_pos = Vec4(0.0);
	F32 m_radius = 0.0;
	Bool8 m_markedForRendering = false;
};
/// @}

} // end namespace anki

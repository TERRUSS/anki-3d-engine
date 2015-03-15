// Copyright (C) 2009-2015, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_GR_GL_GR_MANAGER_IMPL_H
#define ANKI_GR_GL_GR_MANAGER_IMPL_H

#include "anki/gr/Common.h"
#include "anki/util/List.h"

namespace anki {

/// @addtogroup opengl
/// @{

/// Graphics manager backend specific.
class GrManagerImpl
{
public:
	GrManagerImpl(GrManager* manager)
	:	m_manager(manager)
	{
		ANKI_ASSERT(manager);
	}

	ANKI_USE_RESULT Error create(GrManagerInitializer& init);

	GrAllocator<U8> getAllocator() const;

	const RenderingThread& getRenderingThread() const
	{
		return *m_thread;
	}

	RenderingThread& getRenderingThread()
	{
		return *m_thread;
	}

private:
	GrManager* m_manager;
	RenderingThread* m_thread = nullptr;
	List<GrObject*> m_objectsForDeletion;
};
/// @}

} // end namespace anki

#endif

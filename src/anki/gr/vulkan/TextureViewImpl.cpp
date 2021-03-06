// Copyright (C) 2009-2018, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/vulkan/TextureViewImpl.h>
#include <anki/gr/vulkan/TextureImpl.h>

namespace anki
{

TextureViewImpl::~TextureViewImpl()
{
}

Error TextureViewImpl::init(const TextureViewInitInfo& inf)
{
	ANKI_ASSERT(inf.isValid());

	// Store some stuff
	m_subresource = inf;

	m_tex = inf.m_texture;
	const TextureImpl& tex = static_cast<const TextureImpl&>(*m_tex);
	ANKI_ASSERT(tex.isSubresourceValid(inf));

	m_texType = tex.getTextureType();

	// Ask the texture for a view
	m_handle = tex.getOrCreateView(inf, m_texType);

	// Create the hash
	Array<U64, 2> toHash = {{tex.getUuid(), ptrToNumber(m_handle)}};
	m_hash = computeHash(&toHash[0], sizeof(toHash));

	return Error::NONE;
}

} // end namespace anki

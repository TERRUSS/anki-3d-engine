// Copyright (C) 2009-2017, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/gr/GrObject.h>
#include <anki/gr/Enums.h>
#include <anki/util/HashMap.h>

namespace anki
{

/// @addtogroup graphics
/// @{

/// XXX
using RenderGraphHandle = U32;

/// XXX
using RenderGraphWorkCallback = void (*)(
	void* userData, CommandBufferPtr cmdb, U32 secondLevelCmdbIdx, U32 secondLevelCmdbCount);

/// XXX
class RenderGraphDependency
{
public:
	RenderGraphHandle m_handle; ///< Buffer or render target handle.
	union
	{
		TextureUsageBit m_textureUsage;
		BufferUsageBit m_bufferUsage;
	};
};

/// XXX
class RenderTargetInfo
{
public:
	RenderGraphHandle m_renderTarget;
	TextureSurfaceInfo m_surface;
	AttachmentLoadOperation m_loadOperation = AttachmentLoadOperation::CLEAR;
	AttachmentStoreOperation m_storeOperation = AttachmentStoreOperation::STORE;
	ClearValue m_clearValue;

	AttachmentLoadOperation m_stencilLoadOperation = AttachmentLoadOperation::CLEAR;
	AttachmentStoreOperation m_stencilStoreOperation = AttachmentStoreOperation::STORE;

	DepthStencilAspectBit m_aspect = DepthStencilAspectBit::NONE; ///< Relevant only for depth stencil textures.
};

/// XXX
class RenderGraph final : public GrObject
{
	ANKI_GR_OBJECT

public:
	static const GrObjectType CLASS_TYPE = GrObjectType::RENDER_GRAPH;

	RenderGraph(GrManager* manager, U64 hash, GrObjectCache* cache);

	// Non-copyable
	RenderGraph(const RenderGraph&) = delete;

	~RenderGraph();

	// Non-copyable
	RenderGraph& operator=(const RenderGraph&) = delete;

	void init()
	{
		// Do nothing, implement the method for the interface
	}

	/// @name 1st step methods
	/// @{
	RenderGraphHandle importRenderTarget(CString name, TexturePtr tex);

	RenderGraphHandle newRenderTarget(CString name, const TextureInitInfo& texInf);

	RenderGraphHandle importBuffer(CString name, BufferPtr buff);

	RenderGraphHandle newBuffer(CString name, PtrSize size, BufferUsageBit usage, BufferMapAccessBit access);

	RenderGraphHandle registerRenderPass(CString name,
		WeakArray<RenderTargetInfo> colorAttachments,
		RenderTargetInfo depthStencilAttachment,
		WeakArray<RenderGraphDependency> consumers,
		WeakArray<RenderGraphDependency> producers,
		RenderGraphWorkCallback callback,
		void* userData,
		U32 secondLevelCmdbsCount);

	/// For compute or other work (mipmap gen).
	RenderGraphHandle registerNonRenderPass(CString name,
		WeakArray<RenderGraphDependency> consumers,
		WeakArray<RenderGraphDependency> producers,
		RenderGraphWorkCallback callback,
		void* userData);
	/// @}

	/// @name 2nd step methods
	/// @{
	void bake();
	/// @}

	/// @name 3rd step methods
	/// @{

	/// Will call a number of RenderGraphWorkCallback that populate 2nd level command buffers.
	void runSecondLevel();
	/// @}

	/// @name 4th step methods
	/// @{

	/// Will call a number of RenderGraphWorkCallback that populate 1st level command buffers.
	void run();
	/// @}

	/// @name 3rd and 4th step methods
	/// @{
	TexturePtr getTexture(RenderGraphHandle handle);
	BufferPtr getBuffer(RenderGraphHandle handle);
	/// @}

	/// @name 5th step methods
	/// @{

	/// Reset the graph for a new frame. All previously created RenderGraphHandle are invalid after that call.
	void reset();
	/// @}

private:
	static constexpr U MAX_PASSES = 128;
	static constexpr U32 TEXTURE_TYPE = 0x10000000;
	static constexpr U32 BUFFER_TYPE = 0x20000000;
	static constexpr U32 RT_TYPE = 0x40000000;

	GrManager* m_gr;
	StackAllocator<U8> m_tmpAlloc;

	class PassBatch;
	class BakeContext;

	/// Render targets of the same type+size+format.
	class RenderTargetCacheEntry
	{
	public:
		DynamicArray<TexturePtr> m_textures;
		U32 m_texturesInUse = 0;
	};

	HashMap<TextureInitInfo, RenderTargetCacheEntry*> m_renderTargetCache; ///< Imported render targets.
	HashMap<FramebufferInitInfo, FramebufferPtr> m_framebufferCache;

	class RenderTarget
	{
	public:
		TexturePtr m_tex;
		Bool8 m_imported = false;
		Array<char, MAX_GR_OBJECT_NAME_LENGTH + 1> m_name;
	};

	RenderTarget* m_renderTargets = nullptr;
	U32 m_renderTargetsCount = 0;
	U32 m_renderTargetsStorage = 0;

	/// Render pass or compute job.
	class Pass
	{
	public:
		FramebufferPtr m_framebuffer;
		DynamicArray<RenderGraphDependency> m_consumers;
		DynamicArray<RenderGraphDependency> m_producers;
		DynamicArray<Pass*> m_dependsOn;
		U32 m_index;
		Array<char, MAX_GR_OBJECT_NAME_LENGTH + 1> m_name;

		void destroy(StackAllocator<U8>& alloc);
	};

	DynamicArray<Pass*> m_passes;

	RenderGraphHandle pushRenderTarget(CString name, TexturePtr tex, Bool imported);

	Bool passHasUnmetDependencies(const BakeContext& ctx, const Pass& pass) const;

	static Bool passADependsOnB(const Pass& a, const Pass& b);

	/// Dump the dependency graph into a file.
	ANKI_USE_RESULT Error dumpDependencyDotFile(const BakeContext& ctx, CString path) const;

	template<typename T>
	void increaseStorage(T*& oldStorage, U32& count, U32& storage);
};
/// @}

} // end namespace anki

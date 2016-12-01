// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/CommandBuffer.h>
#include <anki/gr/gl/CommandBufferImpl.h>
#include <anki/gr/GrManager.h>
#include <anki/gr/gl/GrManagerImpl.h>
#include <anki/gr/gl/RenderingThread.h>
#include <anki/gr/gl/GlState.h>
#include <anki/gr/gl/TransientMemoryManager.h>

#include <anki/gr/Framebuffer.h>
#include <anki/gr/gl/FramebufferImpl.h>
#include <anki/gr/OcclusionQuery.h>
#include <anki/gr/gl/OcclusionQueryImpl.h>
#include <anki/gr/Texture.h>
#include <anki/gr/gl/TextureImpl.h>
#include <anki/gr/Buffer.h>
#include <anki/gr/gl/BufferImpl.h>
#include <anki/gr/Sampler.h>
#include <anki/gr/gl/SamplerImpl.h>
#include <anki/gr/ShaderProgram.h>
#include <anki/gr/gl/ShaderProgramImpl.h>

#include <anki/core/Trace.h>

namespace anki
{

CommandBuffer::CommandBuffer(GrManager* manager, U64 hash, GrObjectCache* cache)
	: GrObject(manager, CLASS_TYPE, hash, cache)
{
}

CommandBuffer::~CommandBuffer()
{
}

void CommandBuffer::init(CommandBufferInitInfo& inf)
{
	m_impl.reset(getAllocator().newInstance<CommandBufferImpl>(&getManager()));
	m_impl->init(inf);

#if ANKI_ASSERTIONS
	m_impl->m_state.m_secondLevel = !!(inf.m_flags & CommandBufferFlag::SECOND_LEVEL);
#endif
}

void CommandBuffer::flush()
{
	if(!m_impl->isSecondLevel())
	{
		ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	}

	if(!m_impl->isSecondLevel())
	{
		getManager().getImplementation().getRenderingThread().flushCommandBuffer(CommandBufferPtr(this));
	}
}

void CommandBuffer::finish()
{
	if(!m_impl->isSecondLevel())
	{
		ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	}

	getManager().getImplementation().getRenderingThread().finishCommandBuffer(CommandBufferPtr(this));
}

void CommandBuffer::bindVertexBuffer(U32 binding, BufferPtr buff, PtrSize offset, PtrSize stride)
{
	class Cmd final : public GlCommand
	{
	public:
		BufferPtr m_buff;
		U32 m_binding;
		PtrSize m_offset;
		PtrSize m_stride;

		Cmd(U32 binding, BufferPtr buff, PtrSize offset, PtrSize stride)
			: m_buff(buff)
			, m_binding(binding)
			, m_offset(offset)
			, m_stride(stride)
		{
		}

		Error operator()(GlState& state)
		{
			glBindVertexBuffer(m_binding, m_buff->m_impl->getGlName(), m_offset, m_stride);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindVertexBuffer(binding, buff, offset, stride))
	{
		m_impl->pushBackNewCommand<Cmd>(binding, buff, offset, stride);
	}
}

void CommandBuffer::bindVertexBuffer(U32 binding, const TransientMemoryToken& token, PtrSize stride)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_binding;
		TransientMemoryToken m_token;
		PtrSize m_stride;
		GLuint m_name;

		Cmd(U32 binding, const TransientMemoryToken& token, PtrSize stride, GLuint name)
			: m_binding(binding)
			, m_token(token)
			, m_stride(stride)
			, m_name(name)
		{
		}

		Error operator()(GlState& state)
		{
			glBindVertexBuffer(m_binding, m_name, m_token.m_offset, m_stride);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(token.m_usage == BufferUsageBit::VERTEX);
	GLuint name = getManager().getImplementation().getTransientMemoryManager().getGlName(token);

	if(m_impl->m_state.bindVertexBuffer(binding, token, stride))
	{
		m_impl->pushBackNewCommand<Cmd>(binding, token, stride, name);
	}
}

void CommandBuffer::setVertexAttribute(U32 location, U32 buffBinding, const PixelFormat& fmt, PtrSize relativeOffset)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_location;
		U32 m_buffBinding;
		U8 m_compSize;
		GLenum m_fmt;
		Bool8 m_normalized;
		PtrSize m_relativeOffset;

		Cmd(U32 location, U32 buffBinding, U8 compSize, GLenum fmt, Bool normalized, PtrSize relativeOffset)
			: m_location(location)
			, m_buffBinding(buffBinding)
			, m_compSize(compSize)
			, m_fmt(fmt)
			, m_normalized(normalized)
			, m_relativeOffset(relativeOffset)
		{
		}

		Error operator()(GlState& state)
		{
			glVertexAttribFormat(m_location, m_compSize, m_fmt, m_normalized, m_relativeOffset);
			glVertexAttribBinding(m_location, m_buffBinding);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setVertexAttribute(location, buffBinding, fmt, relativeOffset))
	{
		U compCount;
		GLenum type;
		Bool normalized;

		convertVertexFormat(fmt, compCount, type, normalized);

		m_impl->pushBackNewCommand<Cmd>(location, buffBinding, compCount, type, normalized, relativeOffset);
	}
}

void CommandBuffer::bindIndexBuffer(BufferPtr buff, PtrSize offset, IndexType type)
{
	class Cmd final : public GlCommand
	{
	public:
		BufferPtr m_buff;

		Cmd(BufferPtr buff)
			: m_buff(buff)
		{
		}

		Error operator()(GlState& state)
		{
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buff->m_impl->getGlName());
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindIndexBuffer(buff, offset, type))
	{
		m_impl->pushBackNewCommand<Cmd>(buff);
	}
}

void CommandBuffer::bindIndexBuffer(const TransientMemoryToken& token, IndexType type)
{
	ANKI_ASSERT(!"TODO");
}

void CommandBuffer::setPrimitiveRestart(Bool enable)
{
	class Cmd final : public GlCommand
	{
	public:
		Bool8 m_enable;

		Cmd(Bool enable)
			: m_enable(enable)
		{
		}

		Error operator()(GlState& state)
		{
			if(m_enable)
			{
				glEnable(GL_PRIMITIVE_RESTART);
			}
			else
			{
				glDisable(GL_PRIMITIVE_RESTART);
			}

			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setPrimitiveRestart(enable))
	{
		m_impl->pushBackNewCommand<Cmd>(enable);
	}
}

void CommandBuffer::setViewport(U16 minx, U16 miny, U16 maxx, U16 maxy)
{
	class ViewportCommand final : public GlCommand
	{
	public:
		Array<U16, 4> m_value;

		ViewportCommand(U16 a, U16 b, U16 c, U16 d)
		{
			m_value = {{a, b, c, d}};
		}

		Error operator()(GlState& state)
		{
			glViewport(m_value[0], m_value[1], m_value[2], m_value[3]);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setViewport(minx, miny, maxx, maxy))
	{
		m_impl->pushBackNewCommand<ViewportCommand>(minx, miny, maxx, maxy);
	}
}

void CommandBuffer::setScissorRect(U16 minx, U16 miny, U16 maxx, U16 maxy)
{
	ANKI_ASSERT(!"TODO");
}

void CommandBuffer::setFillMode(FillMode mode)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_fillMode;

		Cmd(GLenum fillMode)
			: m_fillMode(fillMode)
		{
		}

		Error operator()(GlState& state)
		{
			glPolygonMode(GL_FRONT_AND_BACK, m_fillMode);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setFillMode(mode))
	{
		m_impl->pushBackNewCommand<Cmd>(convertFillMode(mode));
	}
}

void CommandBuffer::setCullMode(FaceSelectionMask mode)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_mode;

		Cmd(GLenum mode)
			: m_mode(mode)
		{
		}

		Error operator()(GlState& state)
		{
			glCullFace(m_mode);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setCullMode(mode))
	{
		m_impl->pushBackNewCommand<Cmd>(convertFaceMode(mode));
	}
}

void CommandBuffer::setPolygonOffset(F32 factor, F32 units)
{
	class Cmd final : public GlCommand
	{
	public:
		F32 m_factor;
		F32 m_units;

		Cmd(F32 factor, F32 units)
			: m_factor(factor)
			, m_units(units)
		{
		}

		Error operator()(GlState& state)
		{
			if(m_factor == 0.0 && m_units == 0.0)
			{
				glDisable(GL_POLYGON_OFFSET_FILL);
			}
			else
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(m_factor, m_units);
			}

			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setPolygonOffset(factor, units))
	{
		m_impl->pushBackNewCommand<Cmd>(factor, units);
	}
}

void CommandBuffer::setStencilOperations(FaceSelectionMask face,
	StencilOperation stencilFail,
	StencilOperation stencilPassDepthFail,
	StencilOperation stencilPassDepthPass)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_face;
		GLenum m_stencilFail;
		GLenum m_stencilPassDepthFail;
		GLenum m_stencilPassDepthPass;

		Cmd(GLenum face, GLenum stencilFail, GLenum stencilPassDepthFail, GLenum stencilPassDepthPass)
			: m_face(face)
			, m_stencilFail(stencilFail)
			, m_stencilPassDepthFail(stencilPassDepthFail)
			, m_stencilPassDepthPass(stencilPassDepthPass)
		{
		}

		Error operator()(GlState& state)
		{
			glStencilOpSeparate(m_face, m_stencilFail, m_stencilPassDepthFail, m_stencilPassDepthPass);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setStencilOperations(face, stencilFail, stencilPassDepthFail, stencilPassDepthPass))
	{
		m_impl->pushBackNewCommand<Cmd>(convertFaceMode(face),
			convertStencilOperation(stencilFail),
			convertStencilOperation(stencilPassDepthFail),
			convertStencilOperation(stencilPassDepthPass));
	}
}

void CommandBuffer::setStencilCompareFunction(FaceSelectionMask face, CompareOperation comp)
{
	m_impl->m_state.setStencilCompareFunction(face, comp);
}

void CommandBuffer::setStencilCompareMask(FaceSelectionMask face, U32 mask)
{
	m_impl->m_state.setStencilCompareMask(face, mask);
}

void CommandBuffer::setStencilWriteMask(FaceSelectionMask face, U32 mask)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_face;
		U32 m_mask;

		Cmd(GLenum face, U32 mask)
			: m_face(face)
			, m_mask(mask)
		{
		}

		Error operator()(GlState& state)
		{
			glStencilMaskSeparate(m_face, m_mask);

			if(m_face == GL_FRONT)
			{
				state.m_stencilWriteMask[0] = m_mask;
			}
			else if(m_face == GL_BACK)
			{
				state.m_stencilWriteMask[1] = m_mask;
			}
			else
			{
				ANKI_ASSERT(m_face == GL_FRONT_AND_BACK);
				state.m_stencilWriteMask[0] = state.m_stencilWriteMask[1] = m_mask;
			}

			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setStencilWriteMask(face, mask))
	{
		m_impl->pushBackNewCommand<Cmd>(convertFaceMode(face), mask);
	}
}

void CommandBuffer::setStencilReference(FaceSelectionMask face, U32 ref)
{
	m_impl->m_state.setStencilReference(face, ref);
}

void CommandBuffer::setDepthWrite(Bool enable)
{
	class Cmd final : public GlCommand
	{
	public:
		Bool8 m_enable;

		Cmd(Bool enable)
			: m_enable(enable)
		{
		}

		Error operator()(GlState& state)
		{
			glDepthMask(m_enable);
			state.m_depthWriteMask = m_enable;
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setDepthWrite(enable))
	{
		m_impl->pushBackNewCommand<Cmd>(enable);
	}
}

void CommandBuffer::setDepthCompareFunction(CompareOperation op)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_op;

		Cmd(GLenum op)
			: m_op(op)
		{
		}

		Error operator()(GlState& state)
		{
			glDepthFunc(m_op);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setDepthCompareFunction(op))
	{
		m_impl->pushBackNewCommand<Cmd>(convertCompareOperation(op));
	}
}

void CommandBuffer::setAlphaToCoverage(Bool enable)
{
	ANKI_ASSERT(!"TODO");
}

void CommandBuffer::setColorChannelWriteMask(U32 attachment, ColorBit mask)
{
	class Cmd final : public GlCommand
	{
	public:
		U8 m_attachment;
		ColorBit m_mask;

		Cmd(U8 attachment, ColorBit mask)
			: m_attachment(attachment)
			, m_mask(mask)
		{
		}

		Error operator()(GlState& state)
		{
			const Bool r = !!(m_mask & ColorBit::RED);
			const Bool g = !!(m_mask & ColorBit::GREEN);
			const Bool b = !!(m_mask & ColorBit::BLUE);
			const Bool a = !!(m_mask & ColorBit::ALPHA);

			glColorMaski(m_attachment, r, g, b, a);

			state.m_colorWriteMasks[m_attachment] = {{r, g, b, a}};

			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setColorChannelWriteMask(attachment, mask))
	{
		m_impl->pushBackNewCommand<Cmd>(attachment, mask);
	}
}

void CommandBuffer::setBlendMethods(U32 attachment, BlendMethod src, BlendMethod dst)
{
	class Cmd final : public GlCommand
	{
	public:
		U8 m_attachment;
		GLenum m_src;
		GLenum m_dst;

		Cmd(U8 att, GLenum src, GLenum dst)
			: m_attachment(att)
			, m_src(src)
			, m_dst(dst)
		{
		}

		Error operator()(GlState&)
		{
			glBlendFunci(m_attachment, m_src, m_dst);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setBlendMethods(attachment, src, dst))
	{
		m_impl->pushBackNewCommand<Cmd>(attachment, convertBlendMethod(src), convertBlendMethod(dst));
	}
}

void CommandBuffer::setBlendFunction(U32 attachment, BlendFunction func)
{
	class Cmd final : public GlCommand
	{
	public:
		U8 m_attachment;
		GLenum m_func;

		Cmd(U8 att, GLenum func)
			: m_attachment(att)
			, m_func(func)
		{
		}

		Error operator()(GlState&)
		{
			glBlendEquationi(m_attachment, m_func);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.setBlendFunction(attachment, func))
	{
		m_impl->pushBackNewCommand<Cmd>(attachment, convertBlendFunction(func));
	}
}

void CommandBuffer::bindTexture(U32 set, U32 binding, TexturePtr tex, DepthStencilAspectMask aspect)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_unit;
		TexturePtr m_tex;

		Cmd(U32 unit, TexturePtr tex)
			: m_unit(unit)
			, m_tex(tex)
		{
		}

		Error operator()(GlState&)
		{
			glBindTextureUnit(m_unit, m_tex->m_impl->getGlName());
			glBindSampler(m_unit, 0);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindTexture(set, binding, tex, aspect))
	{
		U unit = binding + MAX_TEXTURE_BINDINGS * set;
		m_impl->pushBackNewCommand<Cmd>(unit, tex);
	}
}

void CommandBuffer::bindTextureAndSampler(
	U32 set, U32 binding, TexturePtr tex, SamplerPtr sampler, DepthStencilAspectMask aspect)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_unit;
		TexturePtr m_tex;
		SamplerPtr m_sampler;

		Cmd(U32 unit, TexturePtr tex, SamplerPtr sampler)
			: m_unit(unit)
			, m_tex(tex)
			, m_sampler(sampler)
		{
		}

		Error operator()(GlState&)
		{
			glBindTextureUnit(m_unit, m_tex->m_impl->getGlName());
			glBindSampler(m_unit, m_sampler->m_impl->getGlName());
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindTextureAndSampler(set, binding, tex, sampler, aspect))
	{
		U unit = binding + MAX_TEXTURE_BINDINGS * set;
		m_impl->pushBackNewCommand<Cmd>(unit, tex, sampler);
	}
}

void CommandBuffer::bindUniformBuffer(U32 set, U32 binding, BufferPtr buff, PtrSize offset)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_binding;
		BufferPtr m_buff;
		PtrSize m_offset;

		Cmd(U32 binding, BufferPtr buff, PtrSize offset)
			: m_binding(binding)
			, m_buff(buff)
			, m_offset(offset)
		{
		}

		Error operator()(GlState&)
		{
			m_buff->m_impl->bind(GL_UNIFORM_BUFFER, m_binding, m_offset);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindUniformBuffer(set, binding, buff, offset))
	{
		binding = binding + MAX_UNIFORM_BUFFER_BINDINGS * set;
		m_impl->pushBackNewCommand<Cmd>(binding, buff, offset);
	}
}

void CommandBuffer::bindUniformBuffer(U32 set, U32 binding, const TransientMemoryToken& token)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_binding;
		TransientMemoryToken m_token;
		GLuint m_name;

		Cmd(U32 binding, const TransientMemoryToken& token, GLuint name)
			: m_binding(binding)
			, m_token(token)
			, m_name(name)
		{
		}

		Error operator()(GlState& state)
		{
			glBindBufferRange(GL_UNIFORM_BUFFER, m_binding, m_name, m_token.m_offset, m_token.m_range);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!!(token.m_usage & BufferUsageBit::UNIFORM_ALL));

	if(m_impl->m_state.bindUniformBuffer(set, binding, token))
	{
		binding = binding + MAX_UNIFORM_BUFFER_BINDINGS * set;
		GLuint name = getManager().getImplementation().getTransientMemoryManager().getGlName(token);
		m_impl->pushBackNewCommand<Cmd>(binding, token, name);
	}
}

void CommandBuffer::bindStorageBuffer(U32 set, U32 binding, BufferPtr buff, PtrSize offset)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_binding;
		BufferPtr m_buff;
		PtrSize m_offset;

		Cmd(U32 binding, BufferPtr buff, PtrSize offset)
			: m_binding(binding)
			, m_buff(buff)
			, m_offset(offset)
		{
		}

		Error operator()(GlState&)
		{
			m_buff->m_impl->bind(GL_SHADER_STORAGE_BUFFER, m_binding, m_offset);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindStorageBuffer(set, binding, buff, offset))
	{
		binding = binding + MAX_STORAGE_BUFFER_BINDINGS * set;
		m_impl->pushBackNewCommand<Cmd>(binding, buff, offset);
	}
}

void CommandBuffer::bindStorageBuffer(U32 set, U32 binding, const TransientMemoryToken& token)
{
	class Cmd final : public GlCommand
	{
	public:
		U32 m_binding;
		TransientMemoryToken m_token;
		GLuint m_name;

		Cmd(U32 binding, const TransientMemoryToken& token, GLuint name)
			: m_binding(binding)
			, m_token(token)
			, m_name(name)
		{
		}

		Error operator()(GlState& state)
		{
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, m_binding, m_name, m_token.m_offset, m_token.m_range);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!!(token.m_usage & BufferUsageBit::STORAGE_ALL));

	if(m_impl->m_state.bindStorageBuffer(set, binding, token))
	{
		binding = binding + MAX_STORAGE_BUFFER_BINDINGS * set;
		GLuint name = getManager().getImplementation().getTransientMemoryManager().getGlName(token);
		m_impl->pushBackNewCommand<Cmd>(binding, token, name);
	}
}

void CommandBuffer::bindImage(U32 set, U32 binding, TexturePtr img, U32 level)
{
	class Cmd final : public GlCommand
	{
	public:
		TexturePtr m_img;
		U16 m_unit;
		U8 m_level;

		Cmd(U32 unit, TexturePtr img, U32 level)
			: m_img(img)
			, m_unit(unit)
			, m_level(level)
		{
		}

		Error operator()(GlState&)
		{
			glBindImageTexture(m_unit,
				m_img->m_impl->getGlName(),
				m_level,
				GL_TRUE,
				0,
				GL_READ_WRITE,
				m_img->m_impl->m_internalFormat);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindImage(set, binding, img, level))
	{
		binding = binding + set * MAX_IMAGE_BINDINGS;
		m_impl->pushBackNewCommand<Cmd>(binding, img, level);
	}
}

void CommandBuffer::bindShaderProgram(ShaderProgramPtr prog)
{
	class Cmd final : public GlCommand
	{
	public:
		ShaderProgramPtr m_prog;

		Cmd(const ShaderProgramPtr& prog)
			: m_prog(prog)
		{
		}

		Error operator()(GlState&)
		{
			glUseProgram(m_prog->m_impl->getGlName());
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.bindShaderProgram(prog))
	{
		m_impl->pushBackNewCommand<Cmd>(prog);
	}
}

void CommandBuffer::beginRenderPass(FramebufferPtr fb)
{
	class BindFramebufferCommand final : public GlCommand
	{
	public:
		FramebufferPtr m_fb;

		BindFramebufferCommand(FramebufferPtr fb)
			: m_fb(fb)
		{
		}

		Error operator()(GlState& state)
		{
			m_fb->m_impl->bind(state);
			return ErrorCode::NONE;
		}
	};

	if(m_impl->m_state.beginRenderPass(fb))
	{
		m_impl->pushBackNewCommand<BindFramebufferCommand>(fb);
	}
}

void CommandBuffer::endRenderPass()
{
	// Nothing for GL
	m_impl->m_state.endRenderPass();
}

void CommandBuffer::drawElements(
	PrimitiveTopology topology, U32 count, U32 instanceCount, U32 firstIndex, U32 baseVertex, U32 baseInstance)
{
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_topology;
		GLenum m_indexType;
		DrawElementsIndirectInfo m_info;

		Cmd(GLenum topology, GLenum indexType, const DrawElementsIndirectInfo& info)
			: m_topology(topology)
			, m_indexType(indexType)
			, m_info(info)
		{
		}

		Error operator()(GlState&)
		{
			glDrawElementsInstancedBaseVertexBaseInstance(m_topology,
				m_info.m_count,
				m_indexType,
				numberToPtr<void*>(m_info.m_firstIndex),
				m_info.m_instanceCount,
				m_info.m_baseVertex,
				m_info.m_baseInstance);

			ANKI_TRACE_INC_COUNTER(GR_DRAWCALLS, 1);
			ANKI_TRACE_INC_COUNTER(GR_VERTICES, m_info.m_instanceCount * m_info.m_count);
			return ErrorCode::NONE;
		}
	};

	m_impl->m_state.checkIndexedDracall();
	m_impl->flushDrawcall(*this);

	U idxBytes;
	if(m_impl->m_state.m_indexType == GL_UNSIGNED_SHORT)
	{
		idxBytes = sizeof(U16);
	}
	else
	{
		ANKI_ASSERT(m_impl->m_state.m_indexType == GL_UNSIGNED_INT);
		idxBytes = sizeof(U32);
	}

	firstIndex = firstIndex + m_impl->m_state.m_indexBuffOffset / idxBytes;

	DrawElementsIndirectInfo info(count, instanceCount, firstIndex, baseVertex, baseInstance);
	m_impl->pushBackNewCommand<Cmd>(convertPrimitiveTopology(topology), m_impl->m_state.m_indexType, info);
}

void CommandBuffer::drawArrays(PrimitiveTopology topology, U32 count, U32 instanceCount, U32 first, U32 baseInstance)
{
	class DrawArraysCommand final : public GlCommand
	{
	public:
		GLenum m_topology;
		DrawArraysIndirectInfo m_info;

		DrawArraysCommand(GLenum topology, const DrawArraysIndirectInfo& info)
			: m_topology(topology)
			, m_info(info)
		{
		}

		Error operator()(GlState& state)
		{
			glDrawArraysInstancedBaseInstance(
				m_topology, m_info.m_first, m_info.m_count, m_info.m_instanceCount, m_info.m_baseInstance);

			ANKI_TRACE_INC_COUNTER(GR_DRAWCALLS, 1);
			ANKI_TRACE_INC_COUNTER(GR_VERTICES, m_info.m_instanceCount * m_info.m_count);
			return ErrorCode::NONE;
		}
	};

	m_impl->m_state.checkNonIndexedDrawcall();
	m_impl->flushDrawcall(*this);

	DrawArraysIndirectInfo info(count, instanceCount, first, baseInstance);
	m_impl->pushBackNewCommand<DrawArraysCommand>(convertPrimitiveTopology(topology), info);
}

void CommandBuffer::drawElementsIndirect(
	PrimitiveTopology topology, U32 drawCount, PtrSize offset, BufferPtr indirectBuff)
{
	class DrawElementsIndirectCommand final : public GlCommand
	{
	public:
		GLenum m_topology;
		GLenum m_indexType;
		U32 m_drawCount;
		PtrSize m_offset;
		BufferPtr m_buff;

		DrawElementsIndirectCommand(GLenum topology, GLenum indexType, U32 drawCount, PtrSize offset, BufferPtr buff)
			: m_topology(topology)
			, m_indexType(indexType)
			, m_drawCount(drawCount)
			, m_offset(offset)
			, m_buff(buff)
		{
			ANKI_ASSERT(drawCount > 0);
			ANKI_ASSERT((m_offset % 4) == 0);
		}

		Error operator()(GlState&)
		{
			const BufferImpl& buff = *m_buff->m_impl;

			ANKI_ASSERT(m_offset + sizeof(DrawElementsIndirectInfo) * m_drawCount <= buff.m_size);

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buff.getGlName());

			glMultiDrawElementsIndirect(
				m_topology, m_indexType, numberToPtr<void*>(m_offset), m_drawCount, sizeof(DrawElementsIndirectInfo));

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			return ErrorCode::NONE;
		}
	};

	m_impl->m_state.checkIndexedDracall();
	m_impl->flushDrawcall(*this);
	m_impl->pushBackNewCommand<DrawElementsIndirectCommand>(
		convertPrimitiveTopology(topology), m_impl->m_state.m_indexType, drawCount, offset, indirectBuff);
}

void CommandBuffer::drawArraysIndirect(
	PrimitiveTopology topology, U32 drawCount, PtrSize offset, BufferPtr indirectBuff)
{
	class DrawArraysIndirectCommand final : public GlCommand
	{
	public:
		GLenum m_topology;
		U32 m_drawCount;
		PtrSize m_offset;
		BufferPtr m_buff;

		DrawArraysIndirectCommand(GLenum topology, U32 drawCount, PtrSize offset, BufferPtr buff)
			: m_topology(topology)
			, m_drawCount(drawCount)
			, m_offset(offset)
			, m_buff(buff)
		{
			ANKI_ASSERT(drawCount > 0);
			ANKI_ASSERT((m_offset % 4) == 0);
		}

		Error operator()(GlState& state)
		{
			const BufferImpl& buff = *m_buff->m_impl;

			ANKI_ASSERT(m_offset + sizeof(DrawArraysIndirectInfo) * m_drawCount <= buff.m_size);

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buff.getGlName());

			glMultiDrawArraysIndirect(
				m_topology, numberToPtr<void*>(m_offset), m_drawCount, sizeof(DrawArraysIndirectInfo));

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			return ErrorCode::NONE;
		}
	};

	m_impl->m_state.checkNonIndexedDrawcall();
	m_impl->flushDrawcall(*this);
	m_impl->pushBackNewCommand<DrawArraysIndirectCommand>(
		convertPrimitiveTopology(topology), drawCount, offset, indirectBuff);
}

void CommandBuffer::dispatchCompute(U32 groupCountX, U32 groupCountY, U32 groupCountZ)
{
	class DispatchCommand final : public GlCommand
	{
	public:
		Array<U32, 3> m_size;

		DispatchCommand(U32 x, U32 y, U32 z)
			: m_size({{x, y, z}})
		{
		}

		Error operator()(GlState&)
		{
			glDispatchCompute(m_size[0], m_size[1], m_size[2]);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!!(m_impl->m_flags & CommandBufferFlag::COMPUTE_WORK));
	m_impl->m_state.checkDispatch();
	m_impl->pushBackNewCommand<DispatchCommand>(groupCountX, groupCountY, groupCountZ);
}

void CommandBuffer::resetOcclusionQuery(OcclusionQueryPtr query)
{
	// Nothing for GL
}

void CommandBuffer::beginOcclusionQuery(OcclusionQueryPtr query)
{
	class OqBeginCommand final : public GlCommand
	{
	public:
		OcclusionQueryPtr m_handle;

		OqBeginCommand(const OcclusionQueryPtr& handle)
			: m_handle(handle)
		{
		}

		Error operator()(GlState&)
		{
			m_handle->m_impl->begin();
			return ErrorCode::NONE;
		}
	};

	m_impl->pushBackNewCommand<OqBeginCommand>(query);
}

void CommandBuffer::endOcclusionQuery(OcclusionQueryPtr query)
{
	class OqEndCommand final : public GlCommand
	{
	public:
		OcclusionQueryPtr m_handle;

		OqEndCommand(const OcclusionQueryPtr& handle)
			: m_handle(handle)
		{
		}

		Error operator()(GlState&)
		{
			m_handle->m_impl->end();
			return ErrorCode::NONE;
		}
	};

	m_impl->pushBackNewCommand<OqEndCommand>(query);
}

void CommandBuffer::uploadTextureSurface(
	TexturePtr tex, const TextureSurfaceInfo& surf, const TransientMemoryToken& token)
{
	class TexSurfUploadCommand final : public GlCommand
	{
	public:
		TexturePtr m_handle;
		TextureSurfaceInfo m_surf;
		TransientMemoryToken m_token;

		TexSurfUploadCommand(const TexturePtr& handle, TextureSurfaceInfo surf, const TransientMemoryToken& token)
			: m_handle(handle)
			, m_surf(surf)
			, m_token(token)
		{
		}

		Error operator()(GlState& state)
		{
			GLuint pbo = state.m_manager->getImplementation().getTransientMemoryManager().getGlName(m_token);
			m_handle->m_impl->writeSurface(m_surf, pbo, m_token.m_offset, m_token.m_range);

			if(m_token.m_lifetime == TransientMemoryTokenLifetime::PERSISTENT)
			{
				state.m_manager->getImplementation().getTransientMemoryManager().free(m_token);
			}

			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(tex);
	ANKI_ASSERT(token.m_range > 0);
	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());

	m_impl->pushBackNewCommand<TexSurfUploadCommand>(tex, surf, token);
}

void CommandBuffer::uploadTextureVolume(TexturePtr tex, const TextureVolumeInfo& vol, const TransientMemoryToken& token)
{
	class TexVolUploadCommand final : public GlCommand
	{
	public:
		TexturePtr m_handle;
		TextureVolumeInfo m_vol;
		TransientMemoryToken m_token;

		TexVolUploadCommand(const TexturePtr& handle, TextureVolumeInfo vol, const TransientMemoryToken& token)
			: m_handle(handle)
			, m_vol(vol)
			, m_token(token)
		{
		}

		Error operator()(GlState& state)
		{
			GLuint pbo = state.m_manager->getImplementation().getTransientMemoryManager().getGlName(m_token);
			m_handle->m_impl->writeVolume(m_vol, pbo, m_token.m_offset, m_token.m_range);

			if(m_token.m_lifetime == TransientMemoryTokenLifetime::PERSISTENT)
			{
				state.m_manager->getImplementation().getTransientMemoryManager().free(m_token);
			}

			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(tex);
	ANKI_ASSERT(token.m_range > 0);
	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());

	m_impl->pushBackNewCommand<TexVolUploadCommand>(tex, vol, token);
}

void CommandBuffer::uploadBuffer(BufferPtr buff, PtrSize offset, const TransientMemoryToken& token)
{
	class BuffWriteCommand final : public GlCommand
	{
	public:
		BufferPtr m_handle;
		PtrSize m_offset;
		TransientMemoryToken m_token;

		BuffWriteCommand(const BufferPtr& handle, PtrSize offset, const TransientMemoryToken& token)
			: m_handle(handle)
			, m_offset(offset)
			, m_token(token)
		{
		}

		Error operator()(GlState& state)
		{
			GLuint pbo = state.m_manager->getImplementation().getTransientMemoryManager().getGlName(m_token);
			m_handle->m_impl->write(pbo, m_token.m_offset, m_offset, m_token.m_range);

			if(m_token.m_lifetime == TransientMemoryTokenLifetime::PERSISTENT)
			{
				state.m_manager->getImplementation().getTransientMemoryManager().free(m_token);
			}

			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(token.m_range > 0);
	ANKI_ASSERT(buff);
	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());

	m_impl->pushBackNewCommand<BuffWriteCommand>(buff, offset, token);
}

void CommandBuffer::generateMipmaps2d(TexturePtr tex, U face, U layer)
{
	class GenMipsCommand final : public GlCommand
	{
	public:
		TexturePtr m_tex;
		U8 m_face;
		U32 m_layer;

		GenMipsCommand(const TexturePtr& tex, U face, U layer)
			: m_tex(tex)
			, m_face(face)
			, m_layer(layer)
		{
		}

		Error operator()(GlState&)
		{
			m_tex->m_impl->generateMipmaps2d(m_face, m_layer);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	m_impl->pushBackNewCommand<GenMipsCommand>(tex, face, layer);
}

void CommandBuffer::generateMipmaps3d(TexturePtr tex)
{
	ANKI_ASSERT(!!"TODO");
}

CommandBufferInitHints CommandBuffer::computeInitHints() const
{
	return m_impl->computeInitHints();
}

void CommandBuffer::pushSecondLevelCommandBuffer(CommandBufferPtr cmdb)
{
	class ExecCmdbCommand final : public GlCommand
	{
	public:
		CommandBufferPtr m_cmdb;

		ExecCmdbCommand(const CommandBufferPtr& cmdb)
			: m_cmdb(cmdb)
		{
		}

		Error operator()(GlState&)
		{
			ANKI_TRACE_START_EVENT(GL_2ND_LEVEL_CMD_BUFFER);
			Error err = m_cmdb->m_impl->executeAllCommands();
			ANKI_TRACE_STOP_EVENT(GL_2ND_LEVEL_CMD_BUFFER);
			return err;
		}
	};

	m_impl->pushBackNewCommand<ExecCmdbCommand>(cmdb);
}

Bool CommandBuffer::isEmpty() const
{
	return m_impl->isEmpty();
}

void CommandBuffer::copyTextureSurfaceToTextureSurface(
	TexturePtr src, const TextureSurfaceInfo& srcSurf, TexturePtr dest, const TextureSurfaceInfo& destSurf)
{
	class CopyTexCommand final : public GlCommand
	{
	public:
		TexturePtr m_src;
		TextureSurfaceInfo m_srcSurf;
		TexturePtr m_dest;
		TextureSurfaceInfo m_destSurf;

		CopyTexCommand(
			TexturePtr src, const TextureSurfaceInfo& srcSurf, TexturePtr dest, const TextureSurfaceInfo& destSurf)
			: m_src(src)
			, m_srcSurf(srcSurf)
			, m_dest(dest)
			, m_destSurf(destSurf)
		{
		}

		Error operator()(GlState&)
		{
			TextureImpl::copy(*m_src->m_impl, m_srcSurf, *m_dest->m_impl, m_destSurf);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	m_impl->pushBackNewCommand<CopyTexCommand>(src, srcSurf, dest, destSurf);
}

void CommandBuffer::setBufferBarrier(
	BufferPtr buff, BufferUsageBit prevUsage, BufferUsageBit nextUsage, PtrSize offset, PtrSize size)
{
	class SetBufferMemBarrierCommand final : public GlCommand
	{
	public:
		GLenum m_barrier;

		SetBufferMemBarrierCommand(GLenum barrier)
			: m_barrier(barrier)
		{
		}

		Error operator()(GlState&)
		{
			glMemoryBarrier(m_barrier);
			return ErrorCode::NONE;
		}
	};

	GLenum d = GL_NONE;
	BufferUsageBit all = prevUsage | nextUsage;

	if(!!(all & BufferUsageBit::UNIFORM_ALL))
	{
		d |= GL_UNIFORM_BARRIER_BIT;
	}

	if(!!(all & BufferUsageBit::STORAGE_ALL))
	{
		d |= GL_SHADER_STORAGE_BARRIER_BIT;
	}

	if(!!(all & BufferUsageBit::INDEX))
	{
		d |= GL_ELEMENT_ARRAY_BARRIER_BIT;
	}

	if(!!(all & BufferUsageBit::VERTEX))
	{
		d |= GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
	}

	if(!!(all & BufferUsageBit::INDIRECT))
	{
		d |= GL_COMMAND_BARRIER_BIT;
	}

	if(!!(all
		   & (BufferUsageBit::FILL | BufferUsageBit::BUFFER_UPLOAD_SOURCE | BufferUsageBit::BUFFER_UPLOAD_DESTINATION)))
	{
		d |= GL_BUFFER_UPDATE_BARRIER_BIT;
	}

	if(!!(all & BufferUsageBit::QUERY_RESULT))
	{
		d |= GL_QUERY_BUFFER_BARRIER_BIT;
	}

	ANKI_ASSERT(d);
	m_impl->pushBackNewCommand<SetBufferMemBarrierCommand>(d);
}

void CommandBuffer::setTextureSurfaceBarrier(
	TexturePtr tex, TextureUsageBit prevUsage, TextureUsageBit nextUsage, const TextureSurfaceInfo& surf)
{
	// Do nothing
}

void CommandBuffer::setTextureVolumeBarrier(
	TexturePtr tex, TextureUsageBit prevUsage, TextureUsageBit nextUsage, const TextureVolumeInfo& vol)
{
	// Do nothing
}

void CommandBuffer::clearTextureSurface(
	TexturePtr tex, const TextureSurfaceInfo& surf, const ClearValue& clearValue, DepthStencilAspectMask aspect)
{
	class ClearTextCommand final : public GlCommand
	{
	public:
		TexturePtr m_tex;
		ClearValue m_val;
		TextureSurfaceInfo m_surf;
		DepthStencilAspectMask m_aspect;

		ClearTextCommand(
			TexturePtr tex, const TextureSurfaceInfo& surf, const ClearValue& val, DepthStencilAspectMask aspect)
			: m_tex(tex)
			, m_val(val)
			, m_surf(surf)
			, m_aspect(aspect)
		{
		}

		Error operator()(GlState&)
		{
			m_tex->m_impl->clear(m_surf, m_val, m_aspect);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	m_impl->pushBackNewCommand<ClearTextCommand>(tex, surf, clearValue, aspect);
}

void CommandBuffer::fillBuffer(BufferPtr buff, PtrSize offset, PtrSize size, U32 value)
{
	class FillBufferCommand final : public GlCommand
	{
	public:
		BufferPtr m_buff;
		PtrSize m_offset;
		PtrSize m_size;
		U32 m_value;

		FillBufferCommand(BufferPtr buff, PtrSize offset, PtrSize size, U32 value)
			: m_buff(buff)
			, m_offset(offset)
			, m_size(size)
			, m_value(value)
		{
		}

		Error operator()(GlState&)
		{
			m_buff->m_impl->fill(m_offset, m_size, m_value);
			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	m_impl->pushBackNewCommand<FillBufferCommand>(buff, offset, size, value);
}

void CommandBuffer::writeOcclusionQueryResultToBuffer(OcclusionQueryPtr query, PtrSize offset, BufferPtr buff)
{
	class WriteOcclResultToBuff final : public GlCommand
	{
	public:
		OcclusionQueryPtr m_query;
		PtrSize m_offset;
		BufferPtr m_buff;

		WriteOcclResultToBuff(OcclusionQueryPtr query, PtrSize offset, BufferPtr buff)
			: m_query(query)
			, m_offset(offset)
			, m_buff(buff)
		{
			ANKI_ASSERT((m_offset % 4) == 0);
		}

		Error operator()(GlState&)
		{
			const BufferImpl& buff = *m_buff->m_impl;
			ANKI_ASSERT(m_offset + 4 <= buff.m_size);

			glBindBuffer(GL_QUERY_BUFFER, buff.getGlName());
			glGetQueryObjectuiv(m_query->m_impl->getGlName(), GL_QUERY_RESULT, numberToPtr<GLuint*>(m_offset));
			glBindBuffer(GL_QUERY_BUFFER, 0);

			return ErrorCode::NONE;
		}
	};

	ANKI_ASSERT(!m_impl->m_state.insideRenderPass());
	m_impl->pushBackNewCommand<WriteOcclResultToBuff>(query, offset, buff);
}

} // end namespace anki

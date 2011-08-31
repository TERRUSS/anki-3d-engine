#include "Common.h"
#include "r/Renderer.h"


WRAP(Renderer)
{
	typedef Pps& (Renderer::* getPpsAccessor)();

	class_<Renderer, noncopyable>("Renderer", no_init)
		.def("getPps", (getPpsAccessor)(&Renderer::getPps),
			return_value_policy<reference_existing_object>())
		.def("getMsTime", &Renderer::getMsTime)
		.def("getIsTime", &Renderer::getIsTime)
		.def("getPpsTime", &Renderer::getPpsTime)
		.def("getBsTime", &Renderer::getBsTime)
		.def("isStagesProfilingEnabled",
			(bool (Renderer::*)() const)(&Renderer::isStagesProfilingEnabled))
		.def("setEnableStagesProfiling", &Renderer::setEnableStagesProfiling)
	;
}

#include "ScriptingCommon.h"
#include "Events/MainRendererPpsHdr.h"


WRAP(EventMainRendererPpsHdr)
{
	class_<Event::MainRendererPpsHdr>("EventMainRendererPpsHdr", no_init)
		.def(init<float, float, float, uint, float>())
		.def(init<const Event::MainRendererPpsHdr&>())
	;
}
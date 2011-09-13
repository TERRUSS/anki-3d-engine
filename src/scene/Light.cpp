#include "Light.h"
#include "rsrc/LightRsrc.h"


//==============================================================================
// Destructor                                                                  =
//==============================================================================
Light::~Light()
{}


//==============================================================================
// init                                                                        =
//==============================================================================
void Light::init(const char* filename)
{
	lightData.loadRsrc(filename);

	diffuseCol = lightData->getDiffuseCol();
	specularCol = lightData->getSpecularCol();
	castsShadowFlag = lightData->castsShadow();
}
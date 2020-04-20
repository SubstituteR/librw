#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"

namespace rw {
namespace d3d {

#ifdef RW_D3D9
IDirect3DDevice9 *d3ddevice = nil;

#define MAX_LIGHTS 8


#define VS_NAME g_vs20_main
#define PS_NAME g_ps20_main
void *default_amb_VS;
void *default_amb_dir_VS;
void *default_all_VS;
void *default_color_PS;
void *default_color_tex_PS;

void
createDefaultShaders(void)
{
	{
		static
#include "shaders/default_amb_VS.h"
		default_amb_VS = createVertexShader((void*)VS_NAME);
		assert(default_amb_VS);
	}
	{
		static
#include "shaders/default_amb_dir_VS.h"
		default_amb_dir_VS = createVertexShader((void*)VS_NAME);
		assert(default_amb_dir_VS);
	}
	{
		static
#include "shaders/default_all_VS.h"
		default_all_VS = createVertexShader((void*)VS_NAME);
		assert(default_all_VS);
	}

	{
		static
#include "shaders/default_color_PS.h"
		default_color_PS = createPixelShader((void*)PS_NAME);
		assert(default_color_PS);
	}

	{
		static
#include "shaders/default_color_tex_PS.h"
		default_color_tex_PS = createPixelShader((void*)PS_NAME);
		assert(default_color_tex_PS);
	}
}

void
destroyDefaultShaders(void)
{
	destroyVertexShader(default_amb_VS);
	default_amb_VS = nil;
	destroyVertexShader(default_amb_dir_VS);
	default_amb_dir_VS = nil;
	destroyVertexShader(default_all_VS);
	default_all_VS = nil;

	destroyPixelShader(default_color_PS);
	default_color_PS = nil;
	destroyPixelShader(default_color_tex_PS);
	default_color_tex_PS = nil;
}


void
lightingCB_Fix(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;

	((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);

	int i, n;
	RGBA amb;
	D3DLIGHT9 light;
	light.Type = D3DLIGHT_DIRECTIONAL;
	//light.Diffuse =  { 0.8f, 0.8f, 0.8f, 1.0f };
	light.Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Ambient =  { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Position = { 0.0f, 0.0f, 0.0f };
	//light.Direction = { 0.0f, 0.0f, -1.0f };
	light.Range = 0.0f;
	light.Falloff = 0.0f;
	light.Attenuation0 = 0.0f;
	light.Attenuation1 = 0.0f;
	light.Attenuation2 = 0.0f;
	light.Theta = 0.0f;
	light.Phi = 0.0f;

	convColor(&amb, &lightData.ambient);
	d3d::setRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(amb.red, amb.green, amb.blue, amb.alpha));

	n = 0;
	for(i = 0; i < lightData.numDirectionals; i++){
		if(n >= MAX_LIGHTS)
			return;
		Light *l = lightData.directionals[i];
		light.Type = D3DLIGHT_DIRECTIONAL;
		light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
		light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
		d3ddevice->SetLight(n, &light);
		d3ddevice->LightEnable(n, TRUE);
		n++;
	}

	for(i = 0; i < lightData.numLocals; i++){
		if(n >= MAX_LIGHTS)
			return;
		Light *l = lightData.locals[i];
		switch(l->getType()){
		case Light::POINT:
			light.Type = D3DLIGHT_POINT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction.x = 0.0f;
			light.Direction.y = 0.0f;
			light.Direction.z = 0.0f;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;

		case Light::SPOT:
			light.Type = D3DLIGHT_SPOT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			light.Theta = l->getAngle()*2.0f;
			light.Phi = light.Theta;
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;

		case Light::SOFTSPOT:
			light.Type = D3DLIGHT_SPOT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			light.Theta = 0.0f;
			light.Phi = l->getAngle()*2.0f;
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;
		}
	}

	for(; n < MAX_LIGHTS; n++)
		d3ddevice->LightEnable(n, FALSE);
}


struct LightVS
{
	V3d color; float param0;
	V3d position; float param1;
	V3d direction; float param2;
};

int32
uploadLights(WorldLights *lightData)
{
	int i;
	int bits = 0;
	int32 numLights[4*3];
	float32 firstLight[4];
	firstLight[0] = 0;	// directional
	firstLight[1] = 0;	// point
	firstLight[2] = 0;	// spot
	firstLight[3] = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	LightVS directionals[8];
	LightVS points[8];
	LightVS spots[8];
	for(i = 0; i < lightData->numDirectionals; i++){
		Light *l = lightData->directionals[i];
		directionals[i].color.x = l->color.red;
		directionals[i].color.y = l->color.green;
		directionals[i].color.z = l->color.blue;
		directionals[i].direction = l->getFrame()->getLTM()->at;
		bits |= VSLIGHT_DIRECT;
	}

	int np = 0;
	int ns = 0;
	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			points[np].color.x = l->color.red;
			points[np].color.y = l->color.green;
			points[np].color.z = l->color.blue;
			points[np].param0 = l->radius;
			points[np].position = l->getFrame()->getLTM()->pos;
			np++;
			bits |= VSLIGHT_POINT;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			spots[ns].color.x = l->color.red;
			spots[ns].color.y = l->color.green;
			spots[ns].color.z = l->color.blue;
			spots[ns].param0 = l->radius;
			spots[ns].position = l->getFrame()->getLTM()->pos;
			spots[ns].direction = l->getFrame()->getLTM()->at;
			spots[ns].param1 = l->minusCosAngle;
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				spots[ns].param2 = 0.0f;
			else
				spots[ns].param2 = 1.0f;
			bits |= VSLIGHT_SPOT;
			ns++;
			break;
		}
	}

	firstLight[0] = 0;
	numLights[0] = lightData->numDirectionals;
	firstLight[1] = numLights[0] + firstLight[0];
	numLights[4] = np;
	firstLight[2] = numLights[4] + firstLight[1];
	numLights[8] = ns;

	d3ddevice->SetVertexShaderConstantI(VSLOC_numLights, numLights, 3);
	d3ddevice->SetVertexShaderConstantF(VSLOC_lightOffset, firstLight, 1);

	int32 off = VSLOC_lights;
	if(numLights[0])
		d3ddevice->SetVertexShaderConstantF(off, (float*)&directionals, numLights[0]*3);
	off += numLights[0]*3;

	if(numLights[4])
		d3ddevice->SetVertexShaderConstantF(off, (float*)&points, numLights[4]*3);
	off += numLights[4]*3;

	if(numLights[8])
		d3ddevice->SetVertexShaderConstantF(off, (float*)&spots, numLights[8]*3);

	return bits;
}

int32
lightingCB_Shader(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;

	int lighting = !!(atomic->geometry->flags & rw::Geometry::LIGHT);
	if(lighting){
		((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
		d3ddevice->SetVertexShaderConstantF(VSLOC_ambLight, (float*)&lightData.ambient, 1);
		return uploadLights(&lightData);
	}else{
		static const float zeroF[4];
		static const int32 zeroI[4];
		d3ddevice->SetVertexShaderConstantF(VSLOC_ambLight, zeroF, 1);
		d3ddevice->SetVertexShaderConstantI(VSLOC_numLights, zeroI, 1);
		return 0;
	}
}

static RawMatrix identityXform = {
	{ 1.0f, 0.0f, 0.0f }, 0.0f,
	{ 0.0f, 1.0f, 0.0f }, 0.0f,
	{ 0.0f, 0.0f, 1.0f }, 0.0f,
	{ 0.0f, 0.0f, 0.0f }, 1.0f
};

void
uploadMatrices(void)
{
	RawMatrix combined;
	Camera *cam = engine->currentCamera;
	d3ddevice->SetVertexShaderConstantF(VSLOC_world, (float*)&identityXform, 4);
	d3ddevice->SetVertexShaderConstantF(VSLOC_normal, (float*)&identityXform, 4);

	RawMatrix::mult(&combined, &cam->devView, &cam->devProj);
	d3ddevice->SetVertexShaderConstantF(VSLOC_combined, (float*)&combined, 4);
}

void
uploadMatrices(Matrix *worldMat)
{
	RawMatrix combined, world, worldview;
	Camera *cam = engine->currentCamera;
	convMatrix(&world, worldMat);
	d3ddevice->SetVertexShaderConstantF(VSLOC_world, (float*)&world, 4);
	// TODO: inverse transpose
	d3ddevice->SetVertexShaderConstantF(VSLOC_normal, (float*)&world, 4);

	RawMatrix::mult(&worldview, &world, &cam->devView);
	RawMatrix::mult(&combined, &worldview, &cam->devProj);
	d3ddevice->SetVertexShaderConstantF(VSLOC_combined, (float*)&combined, 4);
}



#endif

}
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwplugins.h"
#include "ps2/rwps2.h"
#include "ps2/rwps2plg.h"
#include "d3d/rwxbox.h"
#include "d3d/rwd3d8.h"
#include "d3d/rwd3d9.h"
#include "gl/rwwdgl.h"
#include "gl/rwgl3.h"

#define PLUGIN_ID ID_HANIM

namespace rw {

int32 hAnimOffset;
bool32 hAnimDoStream = 1;

HAnimHierarchy*
HAnimHierarchy::create(int32 numNodes, int32 *nodeFlags, int32 *nodeIDs, int32 flags, int32 maxKeySize)
{
	HAnimHierarchy *hier = (HAnimHierarchy*)malloc(sizeof(*hier));

	hier->numNodes = numNodes;
	hier->flags = flags;
	hier->maxInterpKeyFrameSize = maxKeySize;
	hier->parentFrame = nil;
	hier->parentHierarchy = hier;
	if(hier->flags & 2)
		hier->matrices = hier->matricesUnaligned = nil;
	else{
		hier->matricesUnaligned =
		  (float*) new uint8[hier->numNodes*64 + 15];
		hier->matrices =
		  (float*)((uintptr)hier->matricesUnaligned & ~0xF);
	}
	hier->nodeInfo = new HAnimNodeInfo[hier->numNodes];
	for(int32 i = 0; i < hier->numNodes; i++){
		hier->nodeInfo[i].id = nodeIDs[i];
		hier->nodeInfo[i].index = i;
		hier->nodeInfo[i].flags = nodeFlags[i];
		hier->nodeInfo[i].frame = nil;
	}
	return hier;
}

void
HAnimHierarchy::destroy(void)
{
	delete[] (uint8*)this->matricesUnaligned;
	delete[] this->nodeInfo;
	free(this);
}

static Frame*
findById(Frame *f, int32 id)
{
	if(f == nil) return nil;
	HAnimData *hanim = HAnimData::get(f);
	if(hanim->id >= 0 && hanim->id == id) return f;
	Frame *ff = findById(f->next, id);
	if(ff) return ff;
	return findById(f->child, id);
}

void
HAnimHierarchy::attachByIndex(int32 idx)
{
	int32 id = this->nodeInfo[idx].id;
	Frame *f = findById(this->parentFrame, id);
	this->nodeInfo[idx].frame = f;
}

void
HAnimHierarchy::attach(void)
{
	for(int32 i = 0; i < this->numNodes; i++)
		this->attachByIndex(i);
}

int32
HAnimHierarchy::getIndex(int32 id)
{
	for(int32 i = 0; i < this->numNodes; i++)
		if(this->nodeInfo[i].id == id)
			return i;
	return -1;
}

HAnimHierarchy*
HAnimHierarchy::get(Frame *f)
{
	return HAnimData::get(f)->hierarchy;
}

HAnimHierarchy*
HAnimHierarchy::find(Frame *f)
{
	if(f == nil) return nil;
	HAnimHierarchy *hier = HAnimHierarchy::get(f);
	if(hier) return hier;
	hier = HAnimHierarchy::find(f->next);
	if(hier) return hier;
	return HAnimHierarchy::find(f->child);
}

HAnimData*
HAnimData::get(Frame *f)
{
	return PLUGINOFFSET(HAnimData, f, hAnimOffset);
}

static void*
createHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	hanim->id = -1;
	hanim->hierarchy = nil;
	return object;
}

static void*
destroyHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(hanim->hierarchy)
		hanim->hierarchy->destroy();
	hanim->id = -1;
	hanim->hierarchy = nil;
	return object;
}

static void*
copyHAnim(void *dst, void *src, int32 offset, int32)
{
	HAnimData *dsthanim = PLUGINOFFSET(HAnimData, dst, offset);
	HAnimData *srchanim = PLUGINOFFSET(HAnimData, src, offset);
	dsthanim->id = srchanim->id;
	// TODO
	dsthanim->hierarchy = nil;
	return dst;
}

static Stream*
readHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	int32 ver, numNodes;
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	ver = stream->readI32();
	assert(ver == 0x100);
	hanim->id = stream->readI32();
	numNodes = stream->readI32();
	if(numNodes != 0){
		int32 flags = stream->readI32();
		int32 maxKeySize = stream->readI32();
		int32 *nodeFlags = new int32[numNodes];
		int32 *nodeIDs = new int32[numNodes];
		for(int32 i = 0; i < numNodes; i++){
			nodeIDs[i] = stream->readI32();
			stream->readI32();	// index...unused
			nodeFlags[i] = stream->readI32();
		}
		hanim->hierarchy = HAnimHierarchy::create(numNodes,
			nodeFlags, nodeIDs, flags, maxKeySize);
		hanim->hierarchy->parentFrame = (Frame*)object;
		delete[] nodeFlags;
		delete[] nodeIDs;
	}
	return stream;
}

static Stream*
writeHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	stream->writeI32(256);
	stream->writeI32(hanim->id);
	if(hanim->hierarchy == nil){
		stream->writeI32(0);
		return stream;
	}
	HAnimHierarchy *hier = hanim->hierarchy;
	stream->writeI32(hier->numNodes);
	stream->writeI32(hier->flags);
	stream->writeI32(hier->maxInterpKeyFrameSize);
	for(int32 i = 0; i < hier->numNodes; i++){
		stream->writeI32(hier->nodeInfo[i].id);
		stream->writeI32(hier->nodeInfo[i].index);
		stream->writeI32(hier->nodeInfo[i].flags);
	}
	return stream;
}

static int32
getSizeHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(!hAnimDoStream ||
	   version >= 0x35000 && hanim->id == -1 && hanim->hierarchy == nil)
		return 0;
	if(hanim->hierarchy)
		return 12 + 8 + hanim->hierarchy->numNodes*12;
	return 12;
}

static void
hAnimFrameRead(Stream *stream, Animation *anim)
{
	HAnimKeyFrame *frames = (HAnimKeyFrame*)anim->keyframes;
	for(int32 i = 0; i < anim->numFrames; i++){
		frames[i].time = stream->readF32();
		stream->read(frames[i].q, 4*4);
		stream->read(frames[i].t, 3*4);
		int32 prev = stream->readI32();
		frames[i].prev = &frames[prev];
	}
}

static void
hAnimFrameWrite(Stream *stream, Animation *anim)
{
	HAnimKeyFrame *frames = (HAnimKeyFrame*)anim->keyframes;
	for(int32 i = 0; i < anim->numFrames; i++){
		stream->writeF32(frames[i].time);
		stream->write(frames[i].q, 4*4);
		stream->write(frames[i].t, 3*4);
		stream->writeI32(frames[i].prev - frames);
	}
}

static uint32
hAnimFrameGetSize(Animation *anim)
{
	return anim->numFrames*(4 + 4*4 + 3*4 + 4);
}

void
registerHAnimPlugin(void)
{
	hAnimOffset = Frame::registerPlugin(sizeof(HAnimData), ID_HANIMPLUGIN,
	                                    createHAnim,
	                                    destroyHAnim, copyHAnim);
	Frame::registerPluginStream(ID_HANIMPLUGIN,
	                            readHAnim,
	                            writeHAnim,
	                            getSizeHAnim);

	AnimInterpolatorInfo *info = new AnimInterpolatorInfo;
	info->id = 1;
	info->keyFrameSize = sizeof(HAnimKeyFrame);
	info->customDataSize = sizeof(HAnimKeyFrame);
	info->streamRead = hAnimFrameRead;
	info->streamWrite = hAnimFrameWrite;
	info->streamGetSize = hAnimFrameGetSize;
	registerAnimInterpolatorInfo(info);
}

}
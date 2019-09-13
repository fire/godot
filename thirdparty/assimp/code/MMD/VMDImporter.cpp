/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2018, assimp team


All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.
r
* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/

#ifndef ASSIMP_BUILD_NO_VMD_IMPORTER

#include "VMDImporter.h"

#include <assimp/CreateAnimMesh.h>
#include <assimp/MemoryIOWrapper.h>
#include <assimp/SkeletonMeshBuilder.h>
#include <assimp/StreamReader.h>
#include <assimp/StringComparison.h>
#include <assimp/importerdesc.h>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include "PostProcessing/ConvertToLHProcess.h"

#include <iterator>
#include <set>

#include <Saba/Base/File.h>
#include <Saba/Model/MMD/VMDAnimation.h>
#include <Saba/Model/MMD/VMDFile.h>

using namespace Assimp;

namespace {
static const aiImporterDesc desc = {
	"VMD Importer",
	"",
	"",
	"",
	aiImporterFlags_SupportTextFlavour,
	0,
	0,
	0,
	0,
	"vmd"
};
}

bool VMDImporter::CanRead(const std::string &pFile, IOSystem *pIOHandler,
		bool checkSig) const {
	const std::string extension = GetExtension(pFile);
	saba::VMDFile vmd;
	if (saba::ReadVMDFile(&vmd, pFile.c_str())) {
		return true;
	}
	if (!extension.length() || checkSig) {
		// no extension given, or we're called a second time because no
		// suitable loader was found yet. This means, we're trying to open
		// the file and look for and hints to identify the file format.
		// #Assimp::BaseImporter provides some utilities:
		//
		// #Assimp::BaseImporter::SearchFileHeaderForToken - for text files.
		// It reads the first lines of the file and does a substring check
		// against a given list of 'magic' strings.
		//
		// #Assimp::BaseImporter::CheckMagicToken - for binary files. It goes
		// to a particular offset in the file and and compares the next words
		// against a given list of 'magic' tokens.
		// These checks MUST be done (even if !checkSig) if the file extension
		// is not exclusive to your format. For example, .xml is very common
		// and (co)used by many formats.
	}
	return false;
}

// -------------------------------------------------------------------------------
// Get list of file extensions handled by this loader
void VMDImporter::GetExtensionList(std::set<std::string> &extensions) {
	extensions.insert("vmd");
}

// -------------------------------------------------------------------------------
void VMDImporter::InternReadFile(const std::string &pFile,
		aiScene *pScene, IOSystem *pIOHandler) {
	std::unique_ptr<IOStream> file(pIOHandler->Open(pFile, "rb"));

	if (file.get() == NULL) {
		throw DeadlyImportError("Failed to open vmd file " + pFile + ".");
	}
	saba::VMDFile vmd;
	saba::ReadVMDFile(&vmd, pFile.c_str());
	const ai_real ticksPerSecond = 30.f;
	pScene->mRootNode = new aiNode();
	pScene->mRootNode->mNumChildren = 1;
	pScene->mRootNode->mChildren = new aiNode *[1];
	pScene->mRootNode->mChildren[0] = new aiNode();
	aiNode *aiChild = pScene->mRootNode->mChildren[0];
	aiChild->mParent = pScene->mRootNode;
	SkeletonMeshBuilder meshBuilder(pScene);
	pScene->mMeshes[0]->mName = vmd.m_header.m_modelName.ToUtf8String();

	std::map<BoneAnim, std::string> bone_anim;
	ai_real frameCount = 0.f;
	std::set<std::string> bones;

	for (std::vector<int32_t>::size_type i = 0; i != vmd.m_motions.size(); i++) {

		glm::vec3 pos = vmd.m_motions[i].m_translate;
		aiVectorKey pos_key;
		pos_key.mTime = vmd.m_motions[i].m_frame;
		pos_key.mValue = aiVector3D(pos.x, pos.y, pos.z);

		glm::quat quat = vmd.m_motions[i].m_quaternion;
		aiQuatKey rot_key;
		rot_key.mTime = vmd.m_motions[i].m_frame;
		rot_key.mValue = aiQuaternion(quat.w, quat.x, quat.y, quat.z);
		BoneAnim bone = {
			vmd.m_motions[i].m_frame,
			pos_key,
			rot_key
		};
		bone_anim.insert({ bone, vmd.m_motions[i].m_boneName.ToUtf8String() });
		bones.insert(vmd.m_motions[i].m_boneName.ToUtf8String());
		if (vmd.m_motions[i].m_frame > frameCount) {
			frameCount = vmd.m_motions[i].m_frame;
		}
	}

	pScene->mNumAnimations = 1;
	pScene->mAnimations = new aiAnimation *[1];
	aiAnimation *anim = new aiAnimation();
	pScene->mAnimations[0] = anim;
	pScene->mAnimations[0]->mDuration = frameCount;
	anim->mTicksPerSecond = ticksPerSecond;
	anim->mNumChannels = bones.size();
	anim->mChannels = new aiNodeAnim *[bones.size()]();
	int32_t track_count = 0;

	for (std::string bone : bones) {
		std::vector<aiVector3D> pos_values;
		std::vector<float> pos_times;
		std::vector<aiQuaternion> rot_values;
		std::vector<float> rot_times;

		for (const auto &frame : bone_anim) {
			if (bone != frame.second) {
				continue;
			}
			pos_times.push_back(frame.first.position.mTime);
			pos_values.push_back(frame.first.position.mValue);
			rot_times.push_back(frame.first.rotation.mTime);
			rot_values.push_back(frame.first.rotation.mValue);
		}
		bool last = false;
		double time = 0.0f;
		float increment = 1.0;

		std::vector<aiVectorKey> pos_track;
		std::vector<aiQuatKey> rot_track;

		while (true) {
			aiVector3D pos;
			aiQuaternion rot;
			aiVector3D scale(1, 1, 1);

			if (pos_values.size()) {
				pos = _interpolate_track_vec3(pos_times, pos_values, time);
			}

			if (rot_values.size()) {
				rot = _interpolate_track_quat(rot_times, rot_values, time);
			}

			aiVectorKey position_key;
			position_key.mTime = time;
			position_key.mValue = pos;
			pos_track.push_back(position_key);

			aiQuatKey rotation_key;
			rotation_key.mTime = time;
			rotation_key.mValue = rot;
			rot_track.push_back(rotation_key);

			if (last) { //done this way so a key is always inserted past the end (for proper interpolation)
				break;
			}

			time += increment;
			if (time >= frameCount) {
				last = true;
			}
		}

		aiNodeAnim *node = new aiNodeAnim;
		anim->mChannels[track_count] = node;
		node->mNodeName.Set(bone);
		node->mNumPositionKeys = pos_track.size();
		node->mPositionKeys = new aiVectorKey[node->mNumPositionKeys]();
		for (size_t j = 0; j < pos_track.size(); j++) {
			node->mPositionKeys[j] = pos_track[j];
		}
		node->mNumRotationKeys = rot_track.size();
		node->mRotationKeys = new aiQuatKey[node->mNumRotationKeys]();
		for (size_t j = 0; j < rot_track.size(); j++) {
			node->mRotationKeys[j] = rot_track[j];
		}
		node->mNumScalingKeys = 1;
		node->mScalingKeys = new aiVectorKey[node->mNumScalingKeys]();
		aiVectorKey identity_scale;
		identity_scale.mTime = 0;
		identity_scale.mValue = aiVector3D(1.0f, 1.0f, 1.0f);
		node->mScalingKeys[0] = identity_scale;

		track_count++;
	}

	// IK Controllers
	// Morph Controllers

	// Convert everything to OpenGL space
	MakeLeftHandedProcess convertProcess;
	convertProcess.Execute(pScene);

	FlipUVsProcess uvFlipper;
	uvFlipper.Execute(pScene);

	FlipWindingOrderProcess windingFlipper;
	windingFlipper.Execute(pScene);
}

const aiImporterDesc *VMDImporter::GetInfo() const {
	return &desc;
}

#endif

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

#include <iterator>
#include <set>

#include "Saba/Model/MMD/VMDFile.h"
#include <Saba/Base/File.h>

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

	pScene->mRootNode = new aiNode();
	pScene->mRootNode->mNumChildren = 1;
	pScene->mRootNode->mChildren = new aiNode*[1];
	pScene->mRootNode->mChildren[0] = new aiNode();
	aiNode *aiChild = pScene->mRootNode->mChildren[0];
	SkeletonMeshBuilder meshBuilder(pScene);
	pScene->mMeshes[0]->mName = vmd.m_header.m_modelName.ToCString();
	struct BoneAnim {
		int32_t index;
		std::vector<uint32_t> frame;
		std::vector<aiVector3D> position;
		std::vector<aiQuaternion> rotation;
	};
	std::map<std::string, BoneAnim> bones;
	int32_t bone_count = 0;
	for (std::vector<int32_t>::size_type i = 0; i != vmd.m_motions.size(); i++) {
		const std::string nodeName = vmd.m_motions[i].m_boneName.ToUtf8String();
		if (bones.find(nodeName) == bones.end()) {
			pScene->mMeshes[0]->mBones[bone_count] = new aiBone();
			pScene->mMeshes[0]->mBones[bone_count]->mName = nodeName; 
			BoneAnim bone;
			bone.frame.push_back(vmd.m_motions[i].m_frame);
			glm::vec3 vector3 = vmd.m_motions[i].m_translate;
			bone.position.push_back(aiVector3D(vector3.x, vector3.y, vector3.z));
			glm::quat quat = vmd.m_motions[i].m_quaternion;
			bone.rotation.push_back(aiQuaternion(quat.w, quat.x, quat.y, quat.z));
			bone.index = bone_count;
			bones.insert({nodeName, bone});
			bone_count++;
		} else {
			BoneAnim &bone_anim = bones.find(nodeName)->second;
			bone_anim.frame.push_back(vmd.m_motions[i].m_frame);
			glm::vec3 vector3 = vmd.m_motions[i].m_translate;
			bone_anim.position.push_back(aiVector3D(vector3.x, vector3.y, vector3.z));
			glm::quat old_quat = vmd.m_motions[i].m_quaternion;
			bone_anim.rotation.push_back(aiQuaternion(old_quat.w, old_quat.x, old_quat.y, old_quat.z));
		}
	}
	pScene->mMeshes[0]->mNumBones = bones.size();
	pScene->mMeshes[0]->mBones = new aiBone*[pScene->mMeshes[0]->mNumBones];
	for (auto bone : bones) {
		pScene->mMeshes[0]->mBones[bone.second.index] = new aiBone();
		pScene->mMeshes[0]->mBones[bone.second.index]->mName = bone.first; 
	}
	pScene->mNumAnimations = 1;
	pScene->mAnimations = new aiAnimation*[1];
	pScene->mAnimations[0] = new aiAnimation();
	pScene->mAnimations[0]->mTicksPerSecond = 30;
	pScene->mAnimations[0]->mNumChannels = bones.size();
	pScene->mAnimations[0]->mChannels = new aiNodeAnim *[bones.size()];
	for (std::vector<int32_t>::size_type i = 0; i != vmd.m_motions.size(); i++) {
		if( bones.find(vmd.m_motions[i].m_boneName.ToUtf8String()) != bones.end()) {
			BoneAnim bone_anim = bones.find(vmd.m_motions[i].m_boneName.ToCString())->second;
			int32_t channelIndex = bone_anim.index;
			aiNodeAnim *node = new aiNodeAnim();
			// pScene->mAnimations[0]->mChannels[channelIndex] = node;
			// node->mNodeName = vmd.m_motions[i].m_boneName.ToUtf8String();
			// node->mNumPositionKeys = bone_anim.position.size();
			// node->mPositionKeys = bone_anim.position.data;
			// node->mNumRotationKeys = bone_anim.rotation.size();
			// node->mRotationKeys = bone_anim.rotation.data;
			// node->mNumScalingKeys = bone_anim.rotation.size();
			// std::vector<aiVector3D> scale; 
			// for(int32_t i =0; i<  bone_anim.rotation.size(); i++) {
			// 	aiVector3D ai_scale = aiVector3D(1.0f, 1.0f, 1.0f);
			// 	scale.push_back(ai_scale);
			// }
			// node->mScalingKeys = scale.data;
		}
	}
	// IK Controllers
	// Morph Controllers
}

const aiImporterDesc *VMDImporter::GetInfo() const {
	return &desc;
}

#endif

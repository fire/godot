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

#include <Saba/Model/MMD/VMDFile.h>
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
	pScene->mRootNode->mChildren = new aiNode *[1];
	pScene->mRootNode->mChildren[0] = new aiNode();
	aiNode *aiChild = pScene->mRootNode->mChildren[0];
	aiChild->mParent = pScene->mRootNode;
	SkeletonMeshBuilder meshBuilder(pScene);
	pScene->mMeshes[0]->mName = vmd.m_header.m_modelName.ToString();
	std::map<std::string, BoneAnim> bone_anim;
	int32_t bone_count = 0;
	for (std::vector<int32_t>::size_type i = 0; i != vmd.m_motions.size(); i++) {
		std::string name = vmd.m_motions[i].m_boneName.ToUtf8String();
		if (bone_anim.find(name) == bone_anim.end()) {
			BoneAnim bone;
			glm::vec3 source_position = vmd.m_motions[i].m_translate;
			aiVectorKey position;
			position.mTime = vmd.m_motions[i].m_frame;
			position.mValue = aiVector3D(source_position.x, -source_position.z, source_position.y);
			bone.position.push_back(position);
			aiVectorKey scale;
			scale.mTime = vmd.m_motions[i].m_frame;
			scale.mValue = aiVector3D(1.0f, 1.0f, 1.0f);
			bone.scale.push_back(scale);
			aiQuatKey rotation;
			rotation.mTime = vmd.m_motions[i].m_frame;
			glm::quat source_quat = vmd.m_motions[i].m_quaternion;
			rotation.mValue = aiQuaternion(source_quat.w, source_quat.x, source_quat.y, source_quat.z);
			bone.rotation.push_back(rotation);
			bone.index = bone_count;
			bone_anim.insert(std::pair<std::string, BoneAnim>(name, bone));
			bone_count++;
		} else {
			BoneAnim &bone = bone_anim.find(name)->second;
			glm::vec3 source_position = vmd.m_motions[i].m_translate;
			aiVectorKey position;
			position.mTime = vmd.m_motions[i].m_frame;
			position.mValue = aiVector3D(source_position.x, -source_position.y, source_position.z);
			bone.position.push_back(position);
			aiVectorKey scale;
			scale.mTime = vmd.m_motions[i].m_frame;
			scale.mValue = aiVector3D(1.0f, 1.0f, 1.0f);
			bone.scale.push_back(scale);
			aiQuatKey rotation;
			rotation.mTime = vmd.m_motions[i].m_frame;
			glm::quat source_quat = vmd.m_motions[i].m_quaternion;
			rotation.mValue = aiQuaternion(source_quat.w, source_quat.x, source_quat.y, source_quat.z).Conjugate;
			bone.rotation.push_back(rotation);
		}
	}
	pScene->mNumAnimations = 1;
	pScene->mAnimations = new aiAnimation *[1];
	aiAnimation *anim = new aiAnimation();
	pScene->mAnimations[0] = anim;
	pScene->mAnimations[0]->mDuration = vmd.m_motions.size();
	anim->mTicksPerSecond = 30;
	anim->mNumChannels = bone_anim.size();
	anim->mChannels = new aiNodeAnim *[bone_anim.size()]();
	for (std::vector<int32_t>::size_type i = 0; i != bone_anim.size(); i++) {
		aiNodeAnim *node = new aiNodeAnim;
		anim->mChannels[i] = node;
		std::string name = vmd.m_motions[i].m_boneName.ToUtf8String();
		node->mNodeName = name;
		if (bone_anim.find(name) != bone_anim.end()) {
			BoneAnim bone = bone_anim.find(name)->second;
			int32_t channelIndex = bone.index;
			node->mNumPositionKeys = bone.position.size();
			node->mPositionKeys = new aiVectorKey[node->mNumPositionKeys]();
			for (size_t j = 0; j < bone.position.size(); j++) {
				node->mPositionKeys[j] = bone.position[j];
			}
			node->mNumRotationKeys = bone.rotation.size();
			node->mRotationKeys = new aiQuatKey[node->mNumRotationKeys]();
			for (size_t j = 0; j < bone.position.size(); j++) {
				node->mRotationKeys[j] = bone.rotation[j];
			}
			node->mNumScalingKeys = bone.scale.size();
			node->mScalingKeys = new aiVectorKey[node->mNumRotationKeys]();
			for (size_t j = 0; j < bone.scale.size(); j++) {
				node->mScalingKeys[j] = bone.scale[j];
			}
		}
	}
	// IK Controllers
	// Morph Controllers
}

const aiImporterDesc *VMDImporter::GetInfo() const {
	return &desc;
}

#endif

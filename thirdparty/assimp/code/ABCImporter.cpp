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

//-*****************************************************************************
//
// Copyright (c) 2013,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#ifndef ASSIMP_BUILD_NO_ABC_IMPORTER

#include "ABCImporter.h"

#include <assimp/CreateAnimMesh.h>
#include <assimp/MemoryIOWrapper.h>
#include <assimp/StreamReader.h>
#include <assimp/StringComparison.h>
#include <assimp/importerdesc.h>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <Alembic/Abc/ErrorHandler.h>
#include <Alembic/AbcCoreAbstract/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcGeom/IPolyMesh.h>

#include <iterator>

using namespace Alembic::AbcGeom;

#include <set>

using namespace Assimp;
//using namespace Assimp::Formatter;
//using namespace Assimp::ABC;

#define RESETCOLOR "\033[0m"
#define GREENCOLOR "\033[1;32m"
#define BLUECOLOR "\033[1;34m"
#define BOLD "\033[1m"

namespace {
static const aiImporterDesc desc = {
	"ABC Importer",
	"",
	"",
	"",
	aiImporterFlags_SupportTextFlavour,
	0,
	0,
	0,
	0,
	"abc"
};
}

bool ABCImporter::CanRead(const std::string &pFile, IOSystem *pIOHandler,
		bool checkSig) const {
	const std::string extension = GetExtension(pFile);
	if (extension == "abc") {
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
void ABCImporter::GetExtensionList(std::set<std::string> &extensions) {
	extensions.insert("abc");
}

// -------------------------------------------------------------------------------
void ABCImporter::InternReadFile(const std::string &pFile,
		aiScene *pScene, IOSystem *pIOHandler) {
	std::unique_ptr<IOStream> file(pIOHandler->Open(pFile, "rb"));
	// Referenced http://jonmacey.blogspot.com/2011/12/getting-started-with-alembic.html
	IArchive archive(Alembic::AbcCoreOgawa::ReadArchive(), pFile);

	// Check whether we can read from the file
	if (file.get() == NULL) {
		throw DeadlyImportError("Failed to open abc file " + pFile + ".");
	}
	// Based on AbcTree/AbcTree.cpp

	std::vector<std::string> seglist;
	bool opt_all = false;

	// walk object hierarchy and find valid objects
	AbcG::IObject test = archive.getTop();
	AbcG::IObject iObj = test;
	while (test.valid() && seglist.size() > 0) {
		test = test.getChild(seglist.front());
		if (test.valid()) {
			iObj = test;
			seglist.erase(seglist.begin());
		}
	}

	// walk property hierarchy for most recent valid object
	Abc::ICompoundProperty props = iObj.getProperties();
	const Abc::PropertyHeader *header;
	bool found = false;
	for (std::size_t i = 0; i < seglist.size(); ++i) {
		header = props.getPropertyHeader(seglist[i]);
		if (header && header->isCompound()) {
			Abc::ICompoundProperty ptest(props, header->getName());
			if (ptest.valid()) {
				props = ptest;
				found = true;
			}
		} else if (header && header->isSimple()) {
			found = true;
		} else {
			throw DeadlyImportError("Invalid object or property " + seglist[i]);
		}
	}

	// walk the archive tree
	if (found) {
		if (header->isCompound()) {
			tree(props, pScene);
		} else {
			tree(Abc::IScalarProperty(props, header->getName()), pScene);
		}
	} else {
		pScene->mRootNode = new aiNode;
		tree(iObj, pScene, pScene->mRootNode, opt_all);
	}
	if (morphs.size()) {
		aiAnimation *const anim = new aiAnimation();
		const float framesPerSecond = 24.0f;
		anim->mTicksPerSecond = framesPerSecond;
		anim->mDuration = archive.getMaxNumSamplesForTimeSamplingIndex(1);
		anim->mNumMorphMeshChannels = morphs.size();
		anim->mMorphMeshChannels = new aiMeshMorphAnim *[anim->mNumMorphMeshChannels];
		std::copy(morphs.begin(), morphs.end(), anim->mMorphMeshChannels);
		animations.push_back(anim);
	}
	TransferDataToScene(pScene);
}

const aiImporterDesc *Assimp::ABCImporter::GetInfo() const {
	return &desc;
}

bool Assimp::ABCImporter::is_leaf(AbcG::IObject iObj) {
	if (!iObj.getParent().valid()) {
		return true;
	}

	Abc::IObject parent = iObj.getParent();
	int numChildren = parent.getNumChildren();

	Abc::IObject test = parent.getChild(numChildren - 1);
	if (test.valid() && test.getName() != iObj.getName()) {
		return false;
	}
	return true;
}

bool Assimp::ABCImporter::is_leaf(Abc::ICompoundProperty iProp, Abc::PropertyHeader iHeader) {
	if (!iProp.valid()) {
		return true;
	}

	int last = iProp.getNumProperties() - 1;
	Abc::PropertyHeader header = iProp.getPropertyHeader(last);
	if (header.getName() == iHeader.getName())
		return true;

	return false;
}

int Assimp::ABCImporter::index(Abc::ICompoundProperty iProp, Abc::PropertyHeader iHeader) {
	for (size_t i = 0; i < iProp.getNumProperties(); i++) {
		Abc::PropertyHeader header = iProp.getPropertyHeader(i);
		if (header.getName() == iHeader.getName()) {
			return i;
		}
	}
	return -1;
}

void Assimp::ABCImporter::tree(Abc::IScalarProperty iProp, aiScene *pScene, std::string prefix) {
	if (iProp.getObject().getFullName() != "/") {
		prefix = prefix + "   ";
	}
	if (is_leaf(iProp.getParent(), iProp.getHeader()) &&
			(iProp.getObject().getNumChildren() == 0 ||
					iProp.getParent().getName() != "")) {
		std::cout << prefix << " `--";
	} else {
		std::cout << prefix << " :--";
		prefix = prefix + " :";
	}

	std::cout << iProp.getName() << "\r" << std::endl;
}

unsigned int Assimp::ABCImporter::ConvertMeshSingleMaterial(AbcG::IPolyMesh polymesh, std::string faceSetName, aiNode *current) {
	IPolyMeshSchema schema = polymesh.getSchema();
	IPolyMeshSchema::Sample mesh_samp;
	schema.get(mesh_samp);
	aiMesh *const mesh = new aiMesh();
	if (faceSetName.length()) {
		mesh->mName.Set(faceSetName);
	} else {
		mesh->mName = current->mName;
	}
	std::vector<aiVector3D> vertices;
	std::vector<aiVector3D> normals;
	std::vector<aiVector2D> uvs;
	std::vector<int> faces;
	const size_t polyCount = mesh_samp.getFaceCounts()->size();
	{
		size_t begIndex = 0;
		const Imath::Vec3<float> *abc_positions = mesh_samp.getPositions()->get();
		for (int i = 0; i < polyCount; i++) {
			const int *face_indices = mesh_samp.getFaceIndices()->get();
			int faceCount = mesh_samp.getFaceCounts()->get()[i];
			if (faceCount > 2) {
				for (int j = faceCount - 1; j >= 0; --j) {
					int face_index = face_indices[begIndex + j];
					aiVector3D pos;
					pos.x = abc_positions[face_index].x;
					pos.y = abc_positions[face_index].y;
					pos.z = abc_positions[face_index].z;
					vertices.push_back(pos);
				}
			}
			begIndex += faceCount;
			faces.push_back(faceCount);
		}

		// copy vertices
		mesh->mNumVertices = static_cast<unsigned int>(vertices.size());
		mesh->mVertices = new aiVector3D[vertices.size()];
		std::copy(vertices.begin(), vertices.end(), mesh->mVertices);

		// generate dummy faces
		mesh->mNumFaces = static_cast<unsigned int>(faces.size());
		aiFace *fac = mesh->mFaces = new aiFace[faces.size()]();

		unsigned int cursor = 0;
		for (unsigned int pcount : faces) {
			aiFace &f = *fac++;
			f.mNumIndices = pcount;
			f.mIndices = new unsigned int[pcount];
			switch (pcount) {
				case 1:
					mesh->mPrimitiveTypes |= aiPrimitiveType_POINT;
					break;
				case 2:
					mesh->mPrimitiveTypes |= aiPrimitiveType_LINE;
					break;
				case 3:
					mesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
					break;
				default:
					mesh->mPrimitiveTypes |= aiPrimitiveType_POLYGON;
					break;
			}
			for (unsigned int i = 0; i < pcount; ++i) {
				f.mIndices[i] = cursor++;
			}
		}
	}

	//if (schema.getNormalsParam().getNumSamples() > 1) {
	//	const IN3fGeomParam iNormals = schema.getNormalsParam();
	//	IN3fGeomParam::Sample normalSamp;
	//	if (iNormals) {
	//		iNormals.getExpanded(normalSamp);
	//	}
	//	const Imath::Vec3<float> *abc_normals = normalSamp.getVals()->get();
	//	size_t begIndex = 0;
	//	for (int i = 0; i < polyCount; i++) {
	//		const int *face_indices = mesh_samp.getFaceIndices()->get();
	//		int faceCount = mesh_samp.getFaceCounts()->get()[i];
	//		if (faceCount > 2) {
	//			for (int j = 0; j < faceCount; j++) {
	//				int face_index = face_indices[begIndex + j];
	//				if (abc_normals) {
	//					aiVector3D ai_normal;
	//					ai_normal.x = abc_normals[face_index].x;
	//					ai_normal.y = abc_normals[face_index].y;
	//					ai_normal.z = abc_normals[face_index].z;
	//					normals.push_back(ai_normal);
	//				}
	//			}
	//		}
	//		begIndex += faceCount;
	//		faces.push_back(faceCount);
	//	}
	//	// copy normals
	//	if (normals.size()) {
	//		ai_assert(normals.size() == vertices.size());

	//		mesh->mNormals = new aiVector3D[vertices.size()];
	//		std::copy(normals.begin(), normals.end(), mesh->mNormals);
	//	}
	//}
	if (true) {
		std::vector<std::string> faceSetNames;
		schema.getFaceSetNames(faceSetNames);
		{
			std::vector<int32_t> facesetFaces;
			// TODO(Ernest) multi material
			for (int i = 0; i < faceSetNames.size(); i++) {
				if (schema.hasFaceSet(faceSetNames[i]) == false) {
					continue;
				}
				IFaceSetSchema faceSet = schema.getFaceSet(faceSetNames[i]).getSchema();
				IFaceSetSchema::Sample faceSetSamp;
				faceSet.get(faceSetSamp);
				const int *abcFacesetFaces = faceSetSamp.getFaces()->get();
				for (size_t j = 0; j < faceSetSamp.getFaces()->size(); j++) {
					facesetFaces.push_back(abcFacesetFaces[j]);
				}
			}

			if (schema.getUVsParam().getNumSamples() > 0) {
				const IV2fGeomParam iUVs = schema.getUVsParam();
				IV2fGeomParam::Sample uvSamp;
				if (iUVs) {
					iUVs.getExpanded(uvSamp);
				}
				const Imath::Vec2<float> *abc_uvs = uvSamp.getVals()->get();
				for (size_t i = 0; i < mesh->mNumVertices; i++) {
					aiVector2D ai_uv;
					ai_uv.x = abc_uvs[i].x;
					ai_uv.y = 1.0f - abc_uvs[i].y;
					uvs.push_back(ai_uv);
				}
				std::reverse(uvs.begin(), uvs.end());
				const size_t texture_coords = 1;
				for (unsigned int i = 0; i < texture_coords; ++i) {
					if (uvs.empty()) {
						break;
					}
					aiVector3D *out_uv = new aiVector3D[vertices.size()];
					for (size_t j = 0; j < uvs.size(); j++) {
						out_uv[j] = aiVector3D(uvs[j].x, uvs[j].y, 0.0f);
					}
					mesh->mTextureCoords[i] = out_uv;
					mesh->mNumUVComponents[i] = 2;
				}
			}
		}
	}

	//// copy tangents - assimp requires both tangents and bitangents (binormals)
	//// to be present, or neither of them. Compute binormals from normals
	//// and tangents if needed.
	//const std::vector<aiVector3D> &tangents = mesh.GetTangents();
	//const std::vector<aiVector3D> *binormals = &mesh.GetBinormals();

	//if (tangents.size()) {
	//	std::vector<aiVector3D> tempBinormals;
	//	if (!binormals->size()) {
	//		if (normals.size()) {
	//			tempBinormals.resize(normals.size());
	//			for (unsigned int i = 0; i < tangents.size(); ++i) {
	//				tempBinormals[i] = normals[i] ^ tangents[i];
	//			}

	//			binormals = &tempBinormals;
	//		} else {
	//			binormals = NULL;
	//		}
	//	}

	//	if (binormals) {
	//		ai_assert(tangents.size() == vertices.size());
	//		ai_assert(binormals->size() == vertices.size());

	//		out_mesh->mTangents = new aiVector3D[vertices.size()];
	//		std::copy(tangents.begin(), tangents.end(), out_mesh->mTangents);

	//		out_mesh->mBitangents = new aiVector3D[vertices.size()];
	//		std::copy(binormals->begin(), binormals->end(), out_mesh->mBitangents);
	//	}
	//}

	//// copy vertex colors
	//for (unsigned int i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++i) {
	//	const std::vector<aiColor4D> &colors = mesh.GetVertexColors(i);
	//	if (colors.empty()) {
	//		break;
	//	}

	//	out_mesh->mColors[i] = new aiColor4D[vertices.size()];
	//	std::copy(colors.begin(), colors.end(), out_mesh->mColors[i]);
	//}

	//if (!doc.Settings().readMaterials || mindices.empty()) {
	//	FBXImporter::LogError("no material assigned to mesh, setting default material");
	//	out_mesh->mMaterialIndex = GetDefaultMaterial();
	//} else {
	//	ConvertMaterialForMesh(out_mesh, model, mesh, mindices[0]);
	//}

	//if (doc.Settings().readWeights && mesh.DeformerSkin() != NULL) {
	//	ConvertWeights(out_mesh, model, mesh, node_global_transform, NO_MATERIAL_SEPARATION);
	//}

	std::vector<aiAnimMesh *> animMeshes;
	if (schema.getTopologyVariance() == kHomogenousTopology && schema.isConstant() == false) {

		TimeSamplingPtr ts = schema.getTimeSampling();
		size_t numChannels = schema.getNumSamples();
		for (size_t i = 0; i < numChannels; i++) {
			SampleTimeSet sampleTimes;
			MatrixSampleMap xformSamples;
			GetRelevantSampleTimes(i, 12.0, 0.0, 0.0, ts, numChannels, sampleTimes);

			//MatrixSampleMap localXformSamples;
			//MatrixSampleMap *localXformSamplesToFill = 0;
			//if (!xformSamples) {
			//	// If we don't have parent xform samples, we can fill
			//	// in the map directly.
			//	localXformSamplesToFill = concatenatedXformSamples.get();
			//} else {
			//	//otherwise we need to fill in a temporary map
			//	localXformSamplesToFill = &localXformSamples;
			//}

			for (SampleTimeSet::iterator I = sampleTimes.begin();
					I != sampleTimes.end(); ++I) {
				IPolyMeshSchema::Sample animMeshSamp;
				schema.get(animMeshSamp, Abc::ISampleSelector(*I));
				aiAnimMesh *animMesh = aiCreateAnimMesh(mesh);
				animMesh->mName = std::string("animation_") + std::to_string(animMeshes.size());
				const Imath::Vec3<float> *animPositions = animMeshSamp.getPositions()->get();
				size_t polyCount = animMeshSamp.getFaceCounts()->size();
				size_t begIndex = 0;
				std::vector<aiVector3D> animVertices;
				for (int i = 0; i < polyCount; i++) {
					const int *animFaceIndices = animMeshSamp.getFaceIndices()->get();
					int faceCount = animMeshSamp.getFaceCounts()->get()[i];
					if (faceCount > 2) {
						for (int j = faceCount - 1; j >= 0; --j) {
							int face_index = animFaceIndices[begIndex + j];
							aiVector3D pos;
							pos.x = animPositions[face_index].x;
							pos.y = animPositions[face_index].y;
							pos.z = animPositions[face_index].z;
							animVertices.push_back(pos);
						}
					}
					begIndex += faceCount;
				}
				animMesh->mNumVertices = static_cast<unsigned int>(animVertices.size());
				animMesh->mVertices = new aiVector3D[animVertices.size()];
				std::copy(animVertices.begin(), animVertices.end(), animMesh->mVertices);
				animMesh->mWeight = 1.0f;
				animMeshes.push_back(animMesh);
			}
		}
		{
			size_t keys = 6;
			for (size_t j = 0; j < numChannels; j++) {
				aiMeshMorphAnim *meshMorphAnim = new aiMeshMorphAnim();
				aiString name = current->mName;
				name.Append("*");
				name.length = 1 + ASSIMP_itoa10(name.data + name.length, MAXLEN - 1, morphs.size());
				meshMorphAnim->mName.Set(name.C_Str());
				meshMorphAnim->mNumKeys = keys;
				meshMorphAnim->mKeys = new aiMeshMorphKey[keys];

				// Bracket the playing frame with weights of 0, during with 1 and after with 0.

				meshMorphAnim->mKeys[0].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[0].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[0].mWeights = new double[1];

				meshMorphAnim->mKeys[0].mValues[0] = j;
				meshMorphAnim->mKeys[0].mWeights[0] = 0.0f;
				meshMorphAnim->mKeys[0].mTime = 0.0f;

				meshMorphAnim->mKeys[1].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[1].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[1].mWeights = new double[1];

				meshMorphAnim->mKeys[1].mValues[0] = j;
				meshMorphAnim->mKeys[1].mWeights[0] = 0.0f;
				meshMorphAnim->mKeys[1].mTime = j - 0.01f;

				meshMorphAnim->mKeys[2].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[2].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[2].mWeights = new double[1];

				meshMorphAnim->mKeys[2].mValues[0] = j;
				meshMorphAnim->mKeys[2].mWeights[0] = 1.0f;
				meshMorphAnim->mKeys[2].mTime = j;

				meshMorphAnim->mKeys[3].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[3].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[3].mWeights = new double[1];

				meshMorphAnim->mKeys[3].mValues[0] = j;
				meshMorphAnim->mKeys[3].mWeights[0] = 1.0f;
				meshMorphAnim->mKeys[3].mTime = j + 1;

				meshMorphAnim->mKeys[4].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[4].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[4].mWeights = new double[1];

				meshMorphAnim->mKeys[4].mValues[0] = j;
				meshMorphAnim->mKeys[4].mWeights[0] = 0.0f;
				meshMorphAnim->mKeys[4].mTime = j + 1.01;

				meshMorphAnim->mKeys[5].mNumValuesAndWeights = 1;
				meshMorphAnim->mKeys[5].mValues = new unsigned int[1];
				meshMorphAnim->mKeys[5].mWeights = new double[1];

				meshMorphAnim->mKeys[5].mValues[0] = j;
				meshMorphAnim->mKeys[5].mWeights[0] = 0.0f;
				meshMorphAnim->mKeys[5].mTime = numChannels;

				morphs.push_back(meshMorphAnim);
			}
		}
	}
	size_t numAnimMeshes = animMeshes.size();
	if (numAnimMeshes > 0) {
		mesh->mNumAnimMeshes = static_cast<unsigned int>(numAnimMeshes);
		mesh->mAnimMeshes = new aiAnimMesh *[numAnimMeshes];
		for (size_t i = 0; i < numAnimMeshes; i++) {
			mesh->mAnimMeshes[i] = animMeshes.at(i);
		}
	}
	meshes.push_back(mesh);
	return static_cast<unsigned int>(meshes.size() - 1);
}

void Assimp::ABCImporter::TransferDataToScene(aiScene *pScene) {
	ai_assert(!pScene->mMeshes);
	ai_assert(!pScene->mNumMeshes);

	// note: the trailing () ensures initialization with NULL - not
	// many C++ users seem to know this, so pointing it out to avoid
	// confusion why this code works.

	if (meshes.size()) {
		pScene->mMeshes = new aiMesh *[meshes.size()]();
		pScene->mNumMeshes = static_cast<unsigned int>(meshes.size());

		std::swap_ranges(meshes.begin(), meshes.end(), pScene->mMeshes);
	}

	if (materials.size()) {
		pScene->mMaterials = new aiMaterial *[materials.size()]();
		pScene->mNumMaterials = static_cast<unsigned int>(materials.size());

		std::swap_ranges(materials.begin(), materials.end(), pScene->mMaterials);
	}

	if (animations.size()) {
		pScene->mAnimations = new aiAnimation *[animations.size()]();
		pScene->mNumAnimations = static_cast<unsigned int>(animations.size());

		std::swap_ranges(animations.begin(), animations.end(), pScene->mAnimations);
	}

	if (lights.size()) {
		pScene->mLights = new aiLight *[lights.size()]();
		pScene->mNumLights = static_cast<unsigned int>(lights.size());

		std::swap_ranges(lights.begin(), lights.end(), pScene->mLights);
	}

	if (cameras.size()) {
		pScene->mCameras = new aiCamera *[cameras.size()]();
		pScene->mNumCameras = static_cast<unsigned int>(cameras.size());

		std::swap_ranges(cameras.begin(), cameras.end(), pScene->mCameras);
	}

	if (textures.size()) {
		pScene->mTextures = new aiTexture *[textures.size()]();
		pScene->mNumTextures = static_cast<unsigned int>(textures.size());

		std::swap_ranges(textures.begin(), textures.end(), pScene->mTextures);
	}
}

void Assimp::ABCImporter::GetRelevantSampleTimes(double frame, double fps, double shutterOpen, double shutterClose, AbcA::TimeSamplingPtr timeSampling, size_t numSamples, SampleTimeSet &output) {
	if (numSamples < 2) {
		output.insert(0.0);
		return;
	}

	chrono_t frameTime = frame / fps;

	chrono_t shutterOpenTime = (frame + shutterOpen) / fps;

	chrono_t shutterCloseTime = (frame + shutterClose) / fps;

	std::pair<index_t, chrono_t> shutterOpenFloor =
			timeSampling->getFloorIndex(shutterOpenTime, numSamples);

	std::pair<index_t, chrono_t> shutterCloseCeil =
			timeSampling->getCeilIndex(shutterCloseTime, numSamples);

	//TODO, what's a reasonable episilon?
	static const chrono_t epsilon = 1.0 / 10000.0;

	//check to see if our second sample is really the
	//floor that we want due to floating point slop
	//first make sure that we have at least two samples to work with
	if (shutterOpenFloor.first < shutterCloseCeil.first) {
		//if our open sample is less than open time,
		//look at the next index time
		if (shutterOpenFloor.second < shutterOpenTime) {
			chrono_t nextSampleTime =
					timeSampling->getSampleTime(shutterOpenFloor.first + 1);

			if (fabs(nextSampleTime - shutterOpenTime) < epsilon) {
				shutterOpenFloor.first += 1;
				shutterOpenFloor.second = nextSampleTime;
			}
		}
	}

	for (index_t i = shutterOpenFloor.first; i < shutterCloseCeil.first; ++i) {
		output.insert(timeSampling->getSampleTime(i));
	}

	//no samples above? put frame time in there and get out
	if (output.size() == 0) {
		output.insert(frameTime);
		return;
	}

	chrono_t lastSample = *(output.rbegin());

	//determine whether we need the extra sample at the end
	if ((fabs(lastSample - shutterCloseTime) > epsilon) && lastSample < shutterCloseTime) {
		output.insert(shutterCloseCeil.second);
	}
}

void Assimp::ABCImporter::tree(Abc::IArrayProperty iProp, aiScene *pScene, std::string prefix) {
	if (iProp.getObject().getFullName() != "/") {
		prefix = prefix + "   ";
	}
	if (is_leaf(iProp.getParent(), iProp.getHeader()) &&
			(iProp.getObject().getNumChildren() == 0 ||
					iProp.getParent().getName() != "")) {
		std::cout << prefix << " `--";
	} else {
		std::cout << prefix << " :--";
		prefix = prefix + " :";
	}

	std::cout << iProp.getName() << "\r" << std::endl;
}

void Assimp::ABCImporter::tree(Abc::ICompoundProperty iProp, aiScene *pScene, std::string prefix) {
	if (iProp.getObject().getFullName() != "/") {
		prefix = prefix + "   ";
	}
	if (is_leaf(iProp.getParent(), iProp.getHeader()) &&
			iProp.getObject().getNumChildren() == 0) {
		std::cout << prefix << " `--";
		prefix = prefix + " ";
	} else {
		if (is_leaf(iProp.getParent(), iProp.getHeader())) {
			std::cout << prefix << " | `--";
			prefix = prefix + " |";
		} else if (iProp.getObject().getNumChildren() == 0) {
			std::cout << prefix << " :--";
			prefix = prefix + " :";
		} else if (is_leaf(iProp, iProp.getHeader())) {
			std::cout << prefix << " | `--";
			prefix = prefix + " |";
		} else {
			std::cout << prefix << " | :--";
			prefix = prefix + " | :";
		}
	}

	std::cout << iProp.getName() << "\r" << std::endl;

	for (size_t i = 0; i < iProp.getNumProperties(); i++) {
		Abc::PropertyHeader header = iProp.getPropertyHeader(i);
		if (header.isScalar()) {
			tree(Abc::IScalarProperty(iProp, header.getName()), pScene, prefix);
		} else if (header.isArray()) {
			tree(Abc::IArrayProperty(iProp, header.getName()), pScene, prefix);
		} else {
			tree(Abc::ICompoundProperty(iProp, header.getName()), pScene, prefix);
		}
	}
}

void Assimp::ABCImporter::tree(AbcG::IObject iObj, aiScene *pScene, aiNode *current, bool showProps, std::string prefix) {
	std::string path = iObj.getFullName();

	if (path == "/") {
		prefix = "";
	} else {
		if (iObj.getParent().getFullName() != "/") {
			prefix = prefix + "   ";
		}
		if (is_leaf(iObj)) {
			std::cout << prefix << " `--";
			prefix = prefix + " ";
		} else {
			std::cout << prefix << " |--";
			prefix = prefix + " |";
		}
	};

	if (showProps) {
		std::cout << GREENCOLOR;
	}

	std::cout << iObj.getName();
	current->mName = iObj.getName();

	if (IXform::matches(iObj.getHeader())) {
		IXform xform(iObj.getParent(), iObj.getHeader().getName());
		IXformSchema &xs = xform.getSchema();
		XformSample xformSample;
		xs.get(xformSample);
		if (xs.getNumOps() > 0) {
			TimeSamplingPtr ts = xs.getTimeSampling();
			Abc::M44d abc_mat = xformSample.getMatrix();
			aiMatrix4x4t<ai_real> ai_mat(
					(ai_real)abc_mat[0][0],
					(ai_real)abc_mat[0][1],
					(ai_real)abc_mat[0][2],
					(ai_real)abc_mat[0][3],
					(ai_real)abc_mat[1][0],
					(ai_real)abc_mat[1][1],
					(ai_real)abc_mat[1][2],
					(ai_real)abc_mat[1][3],
					(ai_real)abc_mat[2][0],
					(ai_real)abc_mat[2][1],
					(ai_real)abc_mat[2][2],
					(ai_real)abc_mat[2][3],
					(ai_real)abc_mat[3][0],
					(ai_real)abc_mat[3][1],
					(ai_real)abc_mat[3][2],
					(ai_real)abc_mat[3][3]);
			ai_mat.Transpose();
			current->mTransformation = ai_mat;
		}
	} else if (IPolyMesh::matches(iObj.getHeader())) {
		IPolyMesh polymesh(iObj.getParent(), iObj.getHeader().getName());
		std::string faceSetName;

		std::vector<unsigned int> meshIndexes;
		//TODO(Ernest) Multimesh?
		//TODO(Ernest) reserve number of meshes

		std::vector<unsigned int> indices;
		const unsigned int index = ConvertMeshSingleMaterial(polymesh, faceSetName, current);
		indices.push_back(index);
		std::copy(indices.begin(), indices.end(), std::back_inserter(meshIndexes));

		if (meshIndexes.size()) {
			current->mMeshes = new unsigned int[meshIndexes.size()]();
			current->mNumMeshes = static_cast<unsigned int>(meshIndexes.size());
			std::swap_ranges(meshIndexes.begin(), meshIndexes.end(), current->mMeshes);
		}
	}

	if (iObj.getNumChildren()) {
		const unsigned int numChildren = iObj.getNumChildren();
		aiNode **nodes = new aiNode *[numChildren]();
		for (unsigned int i = 0; i < numChildren; i++) {
			nodes[i] = new aiNode;
		}
		current->mChildren = nodes;
		current->mNumChildren = static_cast<unsigned int>(numChildren);
	}

	if (showProps) {
		std::cout << RESETCOLOR;
	}
	std::cout << "\r" << std::endl;

	// property tree
	if (showProps) {
		Abc::ICompoundProperty props = iObj.getProperties();
		for (size_t i = 0; i < props.getNumProperties(); i++) {
			Abc::PropertyHeader header = props.getPropertyHeader(i);
			if (header.isScalar()) {
				tree(Abc::IScalarProperty(props, header.getName()), pScene, prefix);
			} else if (header.isArray()) {
				tree(Abc::IArrayProperty(props, header.getName()), pScene, prefix);
			} else {
				tree(Abc::ICompoundProperty(props, header.getName()), pScene, prefix);
			}
		}
	}

	// object tree
	for (size_t i = 0; i < iObj.getNumChildren(); i++) {
		tree(AbcG::IObject(iObj, iObj.getChildHeader(i).getName()), pScene, current->mChildren[i], showProps, prefix);
	};
}

#endif

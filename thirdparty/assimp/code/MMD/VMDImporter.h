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

// Based on Godot's interpolation code

#ifndef INCLUDED_AI_VMD_IMPORTER_H
#define INCLUDED_AI_VMD_IMPORTER_H

#include <assimp/BaseImporter.h>
#include <assimp/ai_assert.h>
#include <assimp/scene.h>

#include <algorithm>
#include <set>
#include <string>

namespace Assimp {
class VMDImporter : public BaseImporter {
public:
	bool CanRead(const std::string &pFile, IOSystem *pIOHandler,
			bool checkSig) const;

protected:
	aiQuaternion _interpolate_track_quat(const std::vector<float> &p_times, const std::vector<aiQuaternion> &p_values, float p_time) {
		//could use binary search, worth it?
		int idx = -1;
		for (int i = 0; i < p_times.size(); i++) {
			if (p_times[i] > p_time)
				break;
			idx++;
		}
		if (idx == -1) {
			return p_values[0];
		} else if (idx >= p_times.size() - 1) {
			return p_values[p_times.size() - 1];
		}

		float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

		aiQuaternion out;
		aiQuaternion::Interpolate(out, p_values[idx], p_values[idx + 1], c);
		return out.Normalize();
	}

	aiVector3D _interpolate_track_vec3(const std::vector<float> &p_times, const std::vector<aiVector3D> &p_values, float p_time) {
		//could use binary search, worth it?
		int idx = -1;
		for (int i = 0; i < p_times.size(); i++) {
			if (p_times[i] > p_time)
				break;
			idx++;
		}

		if (idx == -1) {
			return p_values[0];
		} else if (idx >= p_times.size() - 1) {
			return p_values[p_times.size() - 1];
		}

		float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

		return p_values[idx] + (p_values[idx + 1] - p_values[idx]) * c;
	}

	struct BoneAnim {
		uint32_t frame;
		aiVectorKey position;
		aiQuatKey rotation;
		BoneAnim(uint32_t first, aiVectorKey second, aiQuatKey third) :
				frame(first),
				position(second),
				rotation(third){};
		bool operator<(const BoneAnim &rhs) const {
			return (frame < rhs.frame);
		}
	};
	const aiImporterDesc *GetInfo() const;
	void GetExtensionList(std::set<std::string> &extensions);
	void InternReadFile(const std::string &pFile,
			aiScene *pScene, IOSystem *pIOHandler);
};
} // namespace Assimp
#endif

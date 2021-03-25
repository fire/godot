#include "etcpak_wrap.h"

// thresholds for the early compression-mode decision scheme in QuickETC2
// which can be changed by the option -e
float ecmd_threshold[3] = {0.03f, 0.09f, 0.38f};

#include "thirdparty/etcpak/BlockData.hpp"
#include "thirdparty/etcpak/Math.hpp"
#include "thirdparty/etcpak/Vector.hpp"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <future>
#include <limits>
#include <memory>

void etcpak_wrap_etc2(float p_imgw, float p_imgh, bool p_dither, bool p_etc2, float p_x, float p_y, bool p_mipmap, const uint8_t *p_src, int32_t p_target_size, uint8_t *r_dst) {
	BlockDataPtr bd = std::make_shared<BlockData>(v2i(p_x, p_y), p_mipmap, p_etc2 == true ? BlockData::Etc2_RGBA : BlockData::Etc1);
	if (p_etc2) {
		bd->ProcessRGBA((uint32_t *)p_src, p_imgw * p_imgh / 16, 0, p_imgw);
	} else {
		bd->Process((uint32_t *)p_src, p_imgw * p_imgh / 16, 0, p_imgw, Channels::RGB, p_dither);
	}
	memcpy(r_dst, bd->Decode()->Data(), p_target_size);
	bd.reset();
}

void etcpak_wrap_bc(float p_imgw, float p_imgh, bool p_mipmap, const uint8_t *p_src, int32_t p_target_size, uint8_t *r_dst) {
	BlockDataPtr bd = std::make_shared<BlockData>(v2i(p_imgw, p_imgh), p_mipmap, BlockData::Dxt5);
	bd->ProcessRGBA((uint32_t *)p_src, p_imgw * p_imgh / 16, 0, p_imgw);
	int wofs = 0;
	memcpy(r_dst, bd->Decode()->Data(), p_target_size);
	bd.reset();
}
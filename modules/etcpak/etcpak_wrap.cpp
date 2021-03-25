#include "etcpak_wrap.h"

// thresholds for the early compression-mode decision scheme in QuickETC2
// which can be changed by the option -e
float ecmd_threshold[3] = { 0.03f, 0.09f, 0.38f };

#include "thirdparty/etcpak/BlockData.hpp"
#include "thirdparty/etcpak/Math.hpp"
#include "thirdparty/etcpak/ProcessDxtc.hpp"
#include "thirdparty/etcpak/ProcessRGB.hpp"
#include "thirdparty/etcpak/Vector.hpp"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <future>
#include <limits>
#include <memory>

void etcpak_wrap_etc2(float p_imgw, float p_imgh, bool p_dither, bool p_etc2, const uint32_t *p_src, int32_t p_target_size, uint64_t *r_dst) {
	CompressEtc2Rgba(p_src, r_dst, p_imgw * p_imgh / 16, p_imgw);
	// if (p_etc2) {
	// 	CompressEtc2Rgba(p_src, r_dst, p_imgw * p_imgh / 16, p_imgw);
	// } else {
	// 	if (p_dither) {
	// 		CompressEtc1RgbDither(p_src, r_dst, p_imgw * p_imgh / 16, p_imgw);
	// 	} else {
	// 		CompressEtc1Rgb(p_src, r_dst, p_imgw * p_imgh / 16, p_imgw);
	// 	}
	// }
}

void etcpak_wrap_bc(float p_imgw, float p_imgh, const uint32_t *p_src, int32_t p_target_size, uint64_t *r_dst) {
	CompressDxt5(p_src, r_dst, p_imgw * p_imgh / 16, p_imgw);
}
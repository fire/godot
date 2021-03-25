#include "image_etc.h"
#include "core/io/image.h"
#include "core/os/copymem.h"
#include "core/os/os.h"
#include "core/string/print_string.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <future>
#include <limits>
#include <memory>

#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#include <unistd.h>
#endif

#include "thirdparty/etcpak/ProcessDxtc.hpp"
#include "thirdparty/etcpak/ProcessRGB.hpp"

// thresholds for the early compression-mode decision scheme in QuickETC2
// which can be changed by the option -e
float ecmd_threshold[3] = { 0.03f, 0.09f, 0.38f };

void _register_etc_compress_func() {
	Image::_image_compress_etc1_func = _compress_etc1;
	Image::_image_compress_etc2_func = _compress_etc2;
	Image::_image_compress_bc_func = _compress_bc;
}
static void _compress_etc2(Image *p_img, float p_lossy_quality, Image::UsedChannels p_source) {
	_compress_etcpak(EtcpakType::ETCPAK_TYPE_ETC2, p_img, p_lossy_quality, false, p_source);
}
static void _compress_bc(Image *p_img, float p_lossy_quality, Image::UsedChannels p_source) {
	_compress_etcpak(EtcpakType::ETCPAK_TYPE_DXT5, p_img, p_lossy_quality, false, p_source);
}
static void _compress_etc1(Image *p_img, float p_lossy_quality) {
	_compress_etcpak(EtcpakType::ETCPAK_TYPE_ETC1, p_img, p_lossy_quality, true, Image::USED_CHANNELS_RGB);
}
static void _compress_etcpak(EtcpakType p_compresstype, Image *p_img, float p_lossy_quality, bool force_etc1_format, Image::UsedChannels p_channels) {
	Image::Format img_format = p_img->get_format();

	if (img_format >= Image::FORMAT_DXT1) {
		return; //do not compress, already compressed
	}

	if (img_format > Image::FORMAT_RGBA8) {
		// TODO: we should be able to handle FORMAT_RGBA4444 and FORMAT_RGBA5551 eventually
		return;
	}

	uint32_t imgw = p_img->get_width(), imgh = p_img->get_height();

	Image::Format format = Image::FORMAT_ETC2_RGBA8;

	if (p_compresstype == EtcpakType::ETCPAK_TYPE_ETC1) {
		format = Image::FORMAT_ETC2_RGBA8;
	} else if (p_compresstype == EtcpakType::ETCPAK_TYPE_DXT1) {
		format = Image::FORMAT_DXT1;
	} else if (p_compresstype == EtcpakType::ETCPAK_TYPE_DXT5) {
		format = Image::FORMAT_DXT5;
	}

	const bool mipmap = p_img->has_mipmaps();
	print_verbose("Encoding format: " + Image::get_format_name(format));
	uint64_t t = OS::get_singleton()->get_ticks_msec();

	if (p_img->get_format() != Image::FORMAT_RGBA8) {
		p_img->convert(Image::FORMAT_RGBA8);
	}

	Ref<Image> new_img;
	new_img.instance();
	new_img->create(p_img->get_width(), p_img->get_height(), mipmap, format);
	Vector<uint8_t> data = new_img->get_data();
	uint8_t *wr = data.ptrw();

	Ref<Image> image = p_img->duplicate();
	int mmc = 1 + (mipmap ? Image::get_image_required_mipmaps(new_img->get_width(), new_img->get_height(), format) : 0);
	for (int i = 0; i < mmc; i++) {
		int ofs, size, mip_w, mip_h;
		new_img->get_mipmap_offset_size_and_dimensions(i, ofs, size, mip_w, mip_h);
		mip_w = (mip_w + 3) & ~3;
		mip_h = (mip_h + 3) & ~3;
		image->resize(mip_w, mip_h);
		Vector<uint8_t> dst_data;
		dst_data.resize(size);
		if (p_compresstype == EtcpakType::ETCPAK_TYPE_ETC2) {
			CompressEtc2Rgba((uint32_t *)image->get_data().ptr(), (uint64_t *)dst_data.ptrw(), mip_w * mip_h / 16, mip_w);
		} else if (p_compresstype == EtcpakType::ETCPAK_TYPE_ETC1) {
			CompressEtc1RgbDither((uint32_t *)image->get_data().ptr(), (uint64_t *)dst_data.ptrw(), mip_w * mip_h / 16, mip_w);
		} else if (p_compresstype == EtcpakType::ETCPAK_TYPE_DXT1) {
			CompressDxt1Dither((uint32_t *)image->get_data().ptr(), (uint64_t *)dst_data.ptrw(), mip_w * mip_h / 16, mip_w);
		} else if (p_compresstype == EtcpakType::ETCPAK_TYPE_DXT5) {
			CompressDxt5((uint32_t *)image->get_data().ptr(), (uint64_t *)dst_data.ptrw(), mip_w * mip_h / 16, mip_w);
		} else {
			ERR_FAIL();
		}
		copymem(&wr[ofs], dst_data.ptr(), size);
	}
	p_img->create(new_img->get_width(), new_img->get_height(), mipmap, format, data);

	print_line(vformat("ETCPAK encode took %s ms", rtos(OS::get_singleton()->get_ticks_msec() - t)));
}

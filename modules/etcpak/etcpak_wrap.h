#pragma once
#include <stdint.h>

void etcpak_wrap_etc2(float p_imgw, float p_imgh, bool p_dither, bool p_etc2, float p_x, float p_y, bool p_mipmap, const uint8_t *p_src, int32_t p_target_size, uint8_t *r_dst);
void etcpak_wrap_bc(float p_imgw, float p_imgh, bool p_mipmap, const uint8_t *p_src, int32_t p_target_size, uint8_t *r_dst);
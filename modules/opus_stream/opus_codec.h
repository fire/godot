/*************************************************************************/
/*  opus_codec.h                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef OPUS_CODEC_H
#define OPUS_CODEC_H

#include "core/int_types.h"
#include "core/variant.h"
#include "thirdparty/opus/opus/opus.h"

// TODO: always assumes little endian
template <uint32_t SAMPLE_RATE, uint32_t CHANNEL_COUNT, uint32_t MILLISECONDS_PER_PACKET>
class OpusCodec {
private:
	static const uint32_t APPLICATION = OPUS_APPLICATION_VOIP;

	static const int BUFFER_FRAME_COUNT = SAMPLE_RATE / MILLISECONDS_PER_PACKET;

	static const int INTERNAL_BUFFER_SIZE = (3 * 1276);
	unsigned char internal_buffer[INTERNAL_BUFFER_SIZE];

	OpusEncoder *encoder = NULL;
	OpusDecoder *decoder = NULL;

protected:
	void print_opus_error(int error_code) {
		switch (error_code) {
			case OPUS_OK:
				print_line(String("OpusCodec::OPUS_OK"));
				break;
			case OPUS_BAD_ARG:
				print_line(String("OpusCodec::OPUS_BAD_ARG"));
				break;
			case OPUS_BUFFER_TOO_SMALL:
				print_line(String("OpusCodec::OPUS_BUFFER_TOO_SMALL"));
				break;
			case OPUS_INTERNAL_ERROR:
				print_line(String("OpusCodec::OPUS_INTERNAL_ERROR"));
				break;
			case OPUS_INVALID_PACKET:
				print_line(String("OpusCodec::OPUS_INVALID_PACKET"));
				break;
			case OPUS_UNIMPLEMENTED:
				print_line(String("OpusCodec::OPUS_UNIMPLEMENTED"));
				break;
			case OPUS_INVALID_STATE:
				print_line(String("OpusCodec::OPUS_INVALID_STATE"));
				break;
			case OPUS_ALLOC_FAIL:
				print_line(String("OpusCodec::OPUS_ALLOC_FAIL"));
				break;
		}
	}

public:
	PoolByteArray encode_buffer(const PoolByteArray &p_pcm_buffer) {
		PoolByteArray opus_buffer;

		if (encoder) {
			const opus_int16 *pcm_buffer_pointer = reinterpret_cast<const opus_int16 *>(p_pcm_buffer.read().ptr());

			opus_int32 ret_value = opus_encode(encoder, pcm_buffer_pointer, BUFFER_FRAME_COUNT, internal_buffer, INTERNAL_BUFFER_SIZE);
			if (ret_value >= 0) {
				int number_of_bytes = ret_value;

				opus_buffer.resize(number_of_bytes);
				if (number_of_bytes > 0) {
					unsigned char *opus_buffer_pointer = reinterpret_cast<unsigned char *>(opus_buffer.write().ptr());
					memcpy(opus_buffer_pointer, internal_buffer, number_of_bytes);
				}
			} else {
				print_opus_error(ret_value);
			}
		}

		return opus_buffer;
	}

	PoolByteArray decode_buffer(const PoolByteArray &p_opus_buffer) {
		PoolByteArray pcm_buffer;
		pcm_buffer.resize(BUFFER_FRAME_COUNT * sizeof(opus_int16) * CHANNEL_COUNT);

		if (decoder) {
			opus_int16 *pcm_buffer_pointer = reinterpret_cast<opus_int16 *>(pcm_buffer.write().ptr());
			const unsigned char *opus_buffer_pointer = reinterpret_cast<const unsigned char *>(p_opus_buffer.read().ptr());

			opus_int32 ret_value = opus_decode(decoder, opus_buffer_pointer, p_opus_buffer.size(), pcm_buffer_pointer, BUFFER_FRAME_COUNT, 0);
		}

		return pcm_buffer;
	}

	OpusCodec() {
		int error = 0;
		encoder = opus_encoder_create(SAMPLE_RATE, CHANNEL_COUNT, APPLICATION, &error);
		if (error != OPUS_OK) {
			print_error(String("OpusCodec: could not create Opus encoder!"));
		}

		decoder = opus_decoder_create(SAMPLE_RATE, CHANNEL_COUNT, &error);
		if (error != OPUS_OK) {
			print_error(String("OpusCodec: could not create Opus decoder!"));
		}
	}

	~OpusCodec() {
		opus_encoder_destroy(encoder);
		opus_decoder_destroy(decoder);
	}
};

#endif // OPUS_CODEC_HPP

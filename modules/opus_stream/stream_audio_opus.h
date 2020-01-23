/*************************************************************************/
/*  stream_audio_opus.h                                                  */
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

#ifndef STREAM_AUDIO_OPUS_H
#define STREAM_AUDIO_OPUS_H

#include "opus_codec.h"
#include "modules/audio_effect_stream/stream_audio.h"
#include "modules/opus_stream/libsamplerate/src/samplerate.h"

class StreamAudioOpus : public StreamAudio {
	GDCLASS(StreamAudioOpus, StreamAudio)

private:
	static const uint32_t VOICE_SAMPLE_RATE = 48000;
	static const uint32_t CHANNEL_COUNT = 2;
	static const uint32_t MILLISECONDS_PER_PACKET = 100;
	static const uint32_t BUFFER_FRAME_COUNT = VOICE_SAMPLE_RATE / MILLISECONDS_PER_PACKET;

	OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET> *opus_codec;

	const uint32_t BUFFER_BYTE_COUNT = sizeof(uint16_t);

	static uint32_t _resample_audio_buffer(
			const float *p_src,
			const uint32_t p_src_frame_count,
			const uint32_t p_src_samplerate,
			const uint32_t p_target_samplerate,
			float *p_dst);

protected:
	static void _bind_methods();

public:
	static PoolVector2Array _16_pcm_to_real_stereo(const PoolByteArray p_src_buffer);
	PoolVector2Array decompress_opus(const PoolByteArray p_compressed_buffer);
	PoolVector<uint8_t> _audio_frame_to_pcm(const PoolRealArray p_src_buffer);
	PoolVector<uint8_t> compress_audio_frames(Vector<float> p_frames);
	PoolVector<uint8_t> compress_audio();
	StreamAudioOpus() {
		opus_codec = new OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET>();
	}
	~StreamAudioOpus() {
		delete opus_codec;
	}
};

#endif

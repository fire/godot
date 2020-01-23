/*************************************************************************/
/*  stream_audio_opus.cpp                                                */
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

#include "stream_audio_opus.h"
#include "editor/import/resource_importer_wav.h"
#include "opus_codec.h"

PoolVector2Array StreamAudioOpus::_16_pcm_to_real_stereo(const PoolByteArray p_src_buffer) {
	PoolVector2Array real_audio_frames;
	uint32_t buffer_size = p_src_buffer.size();

	ERR_FAIL_COND_V(buffer_size % 2, real_audio_frames);

	uint32_t frame_count = buffer_size / 2;
	real_audio_frames.resize(frame_count);

	const int16_t *src_buffer_ptr = reinterpret_cast<const int16_t *>(p_src_buffer.read().ptr());
	real_t *real_buffer_ptr = reinterpret_cast<real_t *>(real_audio_frames.write().ptr());

	for (int i = 0; i < frame_count; i++) {
		float value =
				*(real_buffer_ptr + 0) = ((float)*(src_buffer_ptr + 0)) / 32768.0f;
		*(real_buffer_ptr + 1) = ((float)*(src_buffer_ptr + 1)) / 32768.0f;

		real_buffer_ptr += 2;
		src_buffer_ptr += 2;
	}

	return real_audio_frames;
}

PoolVector2Array StreamAudioOpus::decompress_opus(const PoolByteArray p_compressed_buffer) {
	return _16_pcm_to_real_stereo(opus_codec->decode_buffer(p_compressed_buffer));
}

PoolVector<uint8_t> StreamAudioOpus::_audio_frame_to_pcm(const PoolRealArray p_src_buffer) {
	PoolRealArray::Read source_read = p_src_buffer.read();
	Vector<float> resampled_src_buffer;
	resampled_src_buffer.resize(p_src_buffer.size() * 4);
	int32_t resampled_size = _resample_audio_buffer(source_read.ptr(),
			p_src_buffer.size(),
			AudioServer::get_singleton()->get_mix_rate(),
			VOICE_SAMPLE_RATE,
			resampled_src_buffer.ptrw());
	resampled_src_buffer.resize(resampled_size);
	PoolVector<uint8_t> dst_data;

	//byte interleave
	Vector<float> left;
	Vector<float> right;

	int tframes = resampled_src_buffer.size() / 2;
	left.resize(tframes);
	right.resize(tframes);

	for (int i = 0; i < tframes; i++) {
		left.set(i, resampled_src_buffer[i * 2 + 0]);
		right.set(i, resampled_src_buffer[i * 2 + 1]);
	}

	PoolVector<uint8_t> bleft;
	PoolVector<uint8_t> bright;

	ResourceImporterWAV::_compress_ima_adpcm(left, bleft);
	ResourceImporterWAV::_compress_ima_adpcm(right, bright);

	int dl = bleft.size();
	dst_data.resize(dl * 2);

	PoolVector<uint8_t>::Write w = dst_data.write();
	PoolVector<uint8_t>::Read rl = bleft.read();
	PoolVector<uint8_t>::Read rr = bright.read();

	for (int i = 0; i < dl; i++) {
		w[i * 2 + 0] = rl[i];
		w[i * 2 + 1] = rr[i];
	}
	return dst_data;
}

PoolVector<uint8_t> StreamAudioOpus::compress_audio_frames(Vector<float> p_frames) {
	Vector<float> resampled_src_buffer;
	resampled_src_buffer.resize(p_frames.size() * 4);
	int32_t resampled_size = _resample_audio_buffer(p_frames.ptr(),
			p_frames.size(),
			AudioServer::get_singleton()->get_mix_rate(),
			VOICE_SAMPLE_RATE,
			resampled_src_buffer.ptrw());
	resampled_src_buffer.resize(resampled_size);
	PoolVector<uint8_t> dst_data;

	//byte interleave
	Vector<float> left;
	Vector<float> right;

	int tframes = resampled_src_buffer.size() / 2;
	left.resize(tframes);
	right.resize(tframes);

	for (int i = 0; i < tframes; i++) {
		left.set(i, resampled_src_buffer[i * 2 + 0]);
		right.set(i, resampled_src_buffer[i * 2 + 1]);
	}

	PoolVector<uint8_t> bleft;
	PoolVector<uint8_t> bright;

	ResourceImporterWAV::_compress_ima_adpcm(left, bleft);
	ResourceImporterWAV::_compress_ima_adpcm(right, bright);

	int dl = bleft.size();
	dst_data.resize(dl * 2);

	PoolVector<uint8_t>::Write w = dst_data.write();
	PoolVector<uint8_t>::Read rl = bleft.read();
	PoolVector<uint8_t>::Read rr = bright.read();

	for (int i = 0; i < dl; i++) {
		w[i * 2 + 0] = rl[i];
		w[i * 2 + 1] = rr[i];
	}
	return opus_codec->encode_buffer(dst_data);
}

PoolVector<uint8_t> StreamAudioOpus::compress_audio(){
	int32_t size = float(AudioServer::get_singleton()->get_mix_rate()) * 0.1f;
	return compress_audio_frames(get_audio_frames(size));
}

uint32_t StreamAudioOpus::_resample_audio_buffer(const float *p_src, const uint32_t p_src_frame_count, const uint32_t p_src_samplerate, const uint32_t p_target_samplerate, float *p_dst) {
	if (p_src_samplerate != p_target_samplerate) {
		int error = 0;
		SRC_STATE *libresample_state = libresample_state = src_new(SRC_SINC_BEST_QUALITY, CHANNEL_COUNT, &error);
		SRC_DATA src_data;

		src_data.data_in = p_src;
		src_data.data_out = p_dst;

		src_data.src_ratio = (double)p_target_samplerate / (double)p_src_samplerate;

		src_data.input_frames = p_src_frame_count;
		src_data.output_frames = p_src_frame_count * src_data.src_ratio;

		src_data.end_of_input = 0;

		error = src_process(libresample_state, &src_data);
		ERR_FAIL_COND_V_MSG(error != 0, 0, "Resample_error.");
		libresample_state = src_delete(libresample_state);
		return src_data.output_frames_gen;
	} else {
		copymem(p_dst, p_src, p_src_frame_count * sizeof(float));
		return p_src_frame_count;
	}
}

void StreamAudioOpus::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_audio_frame_to_pcm"), &StreamAudioOpus::_audio_frame_to_pcm);
	ClassDB::bind_method(D_METHOD("compress_audio"), &StreamAudioOpus::compress_audio);
	ClassDB::bind_method(D_METHOD("compress_audio_frames"), &StreamAudioOpus::compress_audio_frames);
	ClassDB::bind_method(D_METHOD("decompress_buffer", "opus_compressed_buffer"), &StreamAudioOpus::decompress_opus);
}

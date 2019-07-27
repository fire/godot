#include "audio_effect_opus.h"

#include <algorithm>

#define SET_BUFFER_16_BIT(buffer, buffer_pos, sample) ((int16_t *)buffer)[buffer_pos] = sample >> 16;

void AudioEffectOpus::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_mix_audio"), &AudioEffectOpus::_mix_audio);
	ClassDB::bind_method(D_METHOD("start"), &AudioEffectOpus::start);
	ClassDB::bind_method(D_METHOD("stop"), &AudioEffectOpus::stop);
	ClassDB::bind_method(D_METHOD("decompress_buffer"), &AudioEffectOpus::decompress_buffer);
    ADD_SIGNAL(MethodInfo("audio_packet_processed", PropertyInfo(Variant::POOL_BYTE_ARRAY, "packet")));
}

uint32_t AudioEffectOpus::get_audio_server_mix_frames() {
	return 1024; // TODO: expose this
}

void AudioEffectOpus::_mix_audio() {
	mutex->lock();
	if (active) {
		int8_t *write_buffer = reinterpret_cast<int8_t *>(mix_buffer.write().ptr());
		if (audio_server) {
			uint32_t remaining_frames = get_audio_server_mix_frames();

			while (remaining_frames > 0) {
				audio_server->lock();
				remaining_frames = _mix_internal(audio_server, remaining_frames, BUFFER_FRAME_COUNT, write_buffer, current_mix_buffer_position, capture_ofs);
				audio_server->unlock();

				if (current_mix_buffer_position == 0) {
					emit_signal("audio_packet_processed", compress_buffer(mix_buffer));
				}
			}
		}
	}
	mutex->unlock();
}

// returns remaining processed frames
uint32_t AudioEffectOpus::_mix_internal(AudioServer *p_audio_server, const uint32_t &p_frame_count, const uint32_t p_buffer_size, int8_t *p_buffer_out, uint32_t &p_buffer_position_out, uint32_t &p_capture_offset_out) {
	//PoolIntArray capture_buffer = p_audio_server->get_capture_buffer();
	//uint32_t capture_size = p_audio_server->get_capture_size();
	//uint32_t mix_rate = p_audio_server->get_mix_rate();

	//uint32_t playback_delay = std::min<uint32_t>(((50 * mix_rate) / 1000) * 2, capture_buffer.size() >> 1);

	//if (playback_delay > capture_size) {
	//	p_capture_offset_out = 0;
	//	for (int i = 0; i < p_frame_count; i++) {
	//		SET_BUFFER_16_BIT(p_buffer_out, p_buffer_position_out, 0);
	//		p_buffer_position_out++;

	//		if (p_buffer_position_out >= p_buffer_size) {
	//			p_buffer_position_out = 0;
	//			return p_frame_count - (i + 1);
	//		}
	//	}
	//} else {
	//	for (int i = 0; i < p_frame_count; i++) {
	//		if (capture_size > p_capture_offset_out && (int)p_capture_offset_out < capture_buffer.size()) {
	//			int32_t l = (capture_buffer[p_capture_offset_out++]);
	//			if ((int)p_capture_offset_out >= capture_buffer.size()) {
	//				p_capture_offset_out = 0;
	//			}
	//			int32_t r = (capture_buffer[p_capture_offset_out++]);
	//			if ((int)p_capture_offset_out >= capture_buffer.size()) {
	//				p_capture_offset_out = 0;
	//			}

	//			int32_t mono = (l * 0.5) + (r * 0.5);

	//			SET_BUFFER_16_BIT(p_buffer_out, p_buffer_position_out, mono)
	//			p_buffer_position_out++;
	//		} else {
	//			SET_BUFFER_16_BIT(p_buffer_out, p_buffer_position_out, 0);
	//			p_buffer_position_out++;
	//		}

	//		if (p_buffer_position_out >= p_buffer_size) {
	//			p_buffer_position_out = 0;
	//			return p_frame_count - (i + 1);
	//		}
	//	}
	//}

	return 0;
}

PoolByteArray AudioEffectOpus::_get_buffer_copy(const PoolByteArray p_mix_buffer) {
	PoolByteArray out;

	uint32_t mix_buffer_size = p_mix_buffer.size();
	if (mix_buffer_size > 0) {
		out.resize(mix_buffer_size);
		memcpy(out.write().ptr(), p_mix_buffer.read().ptr(), mix_buffer_size);
	}

	return out;
}

void AudioEffectOpus::start() {
	//if (!ProjectSettings::get_singleton()->get("audio/enable_audio_input")) {
	//	print_error("Need to enable Project settings > Audio > Enable Audio Input option to use capturing.");
	//	return;
	//}

	//mutex->lock();
	//if (!active && is_inside_tree()) {
	//	if (audio_server) {
	//		capture_ofs = 0;

	//		if (audio_server->capture_start() == Error::OK) {
	//			active = true;
	//		}
	//	}
	//}
	//mutex->unlock();
}

void AudioEffectOpus::stop() {
	//mutex->lock();
	//if (active) {
	//	AudioServer::get_singleton()->capture_stop();
	//	active = false;
	//}
	//mutex->unlock();
}

PoolVector2Array AudioEffectOpus::_16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer) {
	PoolVector2Array real_audio_frames;
	uint32_t buffer_size = p_src_buffer.size();

	ERR_FAIL_COND_V(buffer_size % 2, real_audio_frames);

	uint32_t frame_count = buffer_size / 2;
	real_audio_frames.resize(frame_count);

	const int16_t *src_buffer_ptr = reinterpret_cast<const int16_t *>(p_src_buffer.read().ptr());
	real_t *real_buffer_ptr = reinterpret_cast<real_t *>(real_audio_frames.write().ptr());

	for (int i = 0; i < frame_count; i++) {
		float value = ((float)*src_buffer_ptr) / 32768.0f;

		*real_buffer_ptr = value;
		*(real_buffer_ptr + 1) = value;

		real_buffer_ptr += 2;
		src_buffer_ptr++;
	}

	return real_audio_frames;
}

PoolByteArray AudioEffectOpus::compress_buffer(const PoolByteArray p_pcm_buffer) {
	return opus_codec->encode_buffer(p_pcm_buffer);
}

PoolVector2Array AudioEffectOpus::decompress_buffer(const PoolByteArray p_compressed_buffer) {
	return _16_pcm_mono_to_real_stereo(opus_codec->decode_buffer(p_compressed_buffer));
}

void AudioEffectOpus::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE:
			opus_codec = new OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET>(); // ???
			//mutex = new Mutex;
			break;
		case NOTIFICATION_READY:
			audio_server = AudioServer::get_singleton();
			if (audio_server != NULL) {
				mutex->lock();
				mix_buffer.resize(BUFFER_FRAME_COUNT * BUFFER_BYTE_COUNT);
				mutex->unlock();

				audio_server->lock();
				audio_server->connect("audio_mix_callback", this, "_mix_audio");
				audio_server->unlock();

				start();
			}
			break;
		case NOTIFICATION_EXIT_TREE:
			if (!Engine::get_singleton()->is_editor_hint()) {
				print_line(String("VoiceManager::_notification::exit_tree"));
				if (audio_server != NULL) {
					stop();

					audio_server->lock();
					audio_server->disconnect("audio_mix_callback", this, "_mix_audio");
					audio_server->unlock();

					mutex->lock();
					mix_buffer.resize(BUFFER_FRAME_COUNT * BUFFER_BYTE_COUNT);
					mutex->unlock();

					audio_server = NULL;
				}
			}
			break;
		case NOTIFICATION_PREDELETE:
			memdelete(opus_codec);
			//memdelete(mutex);
		break;
	}
}

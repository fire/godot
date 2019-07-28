#ifndef AUDIO_EFFECT_RECORD_STREAM
#define AUDIO_EFFECT_RECORD_STREAM

#include <scene/main/node.h>
#include <core/engine.h>
#include <servers/audio_server.h>
#include <core/project_settings.h>
#include <core/bind/core_bind.h>

#include "opus_codec.h"

#define SET_BUFFER_16_BIT(buffer, buffer_pos, sample) ((int16_t *)buffer)[buffer_pos] = sample >> 16;
class OpusCoder : public Reference {
	GDCLASS(OpusCoder, Reference);

	friend class AudioEffectRecordStreamInstance;
	bool recording_active = false;

	static const uint32_t MIX_BUFFER_COUNT = 4;

	static const uint32_t VOICE_SAMPLE_RATE = 48000;
	static const uint32_t CHANNEL_COUNT = 1;
	static const uint32_t MILLISECONDS_PER_PACKET = 100;
	static const uint32_t BUFFER_FRAME_COUNT = VOICE_SAMPLE_RATE / MILLISECONDS_PER_PACKET;

	const uint32_t BUFFER_BYTE_COUNT = sizeof(uint16_t);

	OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET> *opus_codec = NULL; // Ditto  

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("decompress_buffer"), &OpusCoder::decompress_buffer);
	}
	Ref<OpusCoder> current_instance;

public:
	PoolByteArray compress_buffer(const PoolByteArray p_pcm_buffer) {
		return opus_codec->encode_buffer(p_pcm_buffer);
	}
	
	PoolVector2Array decompress_buffer(const PoolByteArray p_compressed_buffer) {
		return _16_pcm_mono_to_real_stereo(opus_codec->decode_buffer(p_compressed_buffer));
	}

	PoolVector2Array _16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer) {
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

	OpusCoder() {
		opus_codec = new OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET>(); // ???
	}

	~OpusCoder() {
		memdelete(opus_codec);
	}

	OpusCoder();
	~OpusCoder();
};


#endif

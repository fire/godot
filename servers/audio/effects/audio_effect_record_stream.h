#ifndef AUDIO_EFFECT_RECORD_STREAM
#define AUDIO_EFFECT_RECORD_STREAM

#include <scene/main/node.h>
#include <core/engine.h>
#include <servers/audio_server.h>
#include <core/project_settings.h>
#include <core/bind/core_bind.h>

#include "opus_codec.h"

class AudioEffectRecordStreamInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectRecordStreamInstance, AudioEffectInstance);

	//friend class AudioEffectRecordStream;
    Mutex* mutex;
    static const uint32_t MIX_BUFFER_COUNT = 4;

    static const uint32_t VOICE_SAMPLE_RATE = 48000;
    static const uint32_t CHANNEL_COUNT = 1;
    static const uint32_t MILLISECONDS_PER_PACKET = 100;
    static const uint32_t BUFFER_FRAME_COUNT = VOICE_SAMPLE_RATE / MILLISECONDS_PER_PACKET;

    OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET> *opus_codec = NULL; // Ditto
    
    const uint32_t BUFFER_BYTE_COUNT = sizeof(uint16_t);
    bool active = false;
private:
    AudioServer *audio_server = NULL;
    PoolByteArray mix_buffer;
    uint32_t current_mix_buffer_position = 0;

    uint32_t capture_ofs = 0;
	bool is_recording = false;

public:
	static void _bind_methods();
    static PoolByteArray _get_buffer_copy(const PoolByteArray p_mix_buffer);

    void start();
    void stop();

    uint32_t get_audio_server_mix_frames();

    void _mix_audio();
    static uint32_t _mix_internal(AudioServer *p_audio_server, const uint32_t &p_frame_count, const uint32_t p_buffer_size, int8_t *p_buffer_out, uint32_t &p_buffer_position_out, uint32_t &p_capture_offset_out);

    static PoolVector2Array _16_pcm_mono_to_real_stereo(const PoolByteArray p_src_buffer);

    PoolByteArray compress_buffer(const PoolByteArray p_pcm_buffer);
    PoolVector2Array decompress_buffer(const PoolByteArray p_compressed_buffer);

	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {}
	
    void _notification(int p_what);
};

class AudioEffectRecordStream : public AudioEffect {
	GDCLASS(AudioEffectRecordStream, AudioEffect);

	friend class AudioEffectRecordStreamInstance;
	bool recording_active = false;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_recording_active", "record"), &AudioEffectRecordStream::set_recording_active);
		ClassDB::bind_method(D_METHOD("is_recording_active"), &AudioEffectRecordStream::is_recording_active);
	}
	Ref<AudioEffectRecordStream> current_instance;

public:
	Ref<AudioEffectInstance> instance() {
		Ref<AudioEffectRecordStreamInstance> ins;
		ins.instance();
		return ins;
	}

	void set_recording_active(bool p_record) {
		if (p_record) {
			if (current_instance.is_null()) {
				WARN_PRINTS("Recording should not be set as active before Godot has initialized.");
				recording_active = false;
				return;
			}

			//ensure_thread_stopped();
			//current_instance->init();
		}

		recording_active = p_record;
	}

	bool is_recording_active() const {
		return recording_active;
	}

	AudioEffectRecordStream() {}
};


#endif

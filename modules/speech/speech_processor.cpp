#include "speech_processor.h"

#include <algorithm>

#define SET_BUFFER_16_BIT(buffer, buffer_pos, sample) ((int16_t *)buffer)[buffer_pos] = sample >> 16;
#define STEREO_CHANNEL_COUNT 2

#define SIGNED_32_BIT_SIZE 2147483647
#define UNSIGNED_32_BIT_SIZE 4294967295
#define SIGNED_16_BIT_SIZE 32767
#define UNSIGNED_16_BIT_SIZE 65536

#define RECORD_MIX_FRAMES 1024 * 2
#define RESAMPLED_BUFFER_FACTOR sizeof(int)

void SpeechProcessor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_init"), &SpeechProcessor::_init);
	ClassDB::bind_method(D_METHOD("_ready"), &SpeechProcessor::_ready);
	ClassDB::bind_method(D_METHOD("_notification"), &SpeechProcessor::_notification);

	ClassDB::bind_method(D_METHOD("start"), &SpeechProcessor::start);
	ClassDB::bind_method(D_METHOD("stop"), &SpeechProcessor::stop);

	ClassDB::bind_method(D_METHOD("compress_buffer"), &SpeechProcessor::compress_buffer);
	ClassDB::bind_method(D_METHOD("decompress_buffer"), &SpeechProcessor::decompress_buffer);

	ClassDB::bind_method(D_METHOD("set_streaming_bus"), &SpeechProcessor::set_streaming_bus);
	ClassDB::bind_method(D_METHOD("set_audio_input_stream_player"), &SpeechProcessor::set_audio_input_stream_player);

	ADD_SIGNAL(MethodInfo("speech_processed", PropertyInfo(Variant::DICTIONARY, "packet")));
}

uint32_t SpeechProcessor::_resample_audio_buffer(
		const float *p_src,
		const uint32_t p_src_frame_count,
		const uint32_t p_src_samplerate,
		const uint32_t p_target_samplerate,
		float *p_dst) {
	if (p_src_samplerate != p_target_samplerate) {
		SRC_DATA src_data;

		src_data.data_in = p_src;
		src_data.data_out = p_dst;

		src_data.input_frames = p_src_frame_count;
		src_data.output_frames = p_src_frame_count * RESAMPLED_BUFFER_FACTOR;

		src_data.src_ratio = (double)p_target_samplerate / (double)p_src_samplerate;
		src_data.end_of_input = 0;

		int error = src_process(libresample_state, &src_data);
		if (error != 0) {
			ERR_PRINT("resample_error!");
			return 0;
		}
		return src_data.output_frames_gen;
	} else {
		memcpy(p_dst, p_src, static_cast<size_t>(p_src_frame_count) * sizeof(float));
		return p_src_frame_count;
	}
}

void SpeechProcessor::_get_capture_block(AudioServer *p_audio_server,
		const uint32_t &p_mix_frame_count,
		const float *p_process_buffer_in,
		float *p_process_buffer_out) {
	// 0.1 second based on the internal sample rate
	//uint32_t playback_delay = std::min<uint32_t>(((50 * mix_rate) / 1000) * 2, capture_buffer.size() >> 1);

	size_t capture_offset = 0;
	{
		for (size_t i = 0; i < p_mix_frame_count; i++) {
			{
				float mono = 0.0f;
				for (size_t j = 0; j < STEREO_CHANNEL_COUNT; j++) {
					mono += p_process_buffer_in[capture_offset] * 0.5f;
					capture_offset++;
				}
				p_process_buffer_out[i] = mono;
			}
		}
	}
}

void SpeechProcessor::_mix_audio(const float *p_incoming_buffer) {
	int8_t *write_buffer = reinterpret_cast<int8_t *>(mix_byte_array.ptrw());
	if (audio_server) {
		_get_capture_block(audio_server, RECORD_MIX_FRAMES, p_incoming_buffer, mono_real_array.ptrw());
		uint32_t resampled_frame_count = resampled_real_array_offset + _resample_audio_buffer(
																			   mono_real_array.ptr(), // Pointer to source buffer
																			   RECORD_MIX_FRAMES, // Size of source buffer * sizeof(float)
																			   mix_rate, // Source sample rate
																			   VOICE_SAMPLE_RATE, // Target sample rate
																			   resampled_real_array.ptrw() + static_cast<size_t>(resampled_real_array_offset));

		resampled_real_array_offset = 0;

		const float *resampled_real_array_read_ptr = resampled_real_array.ptr();
		double_t sum = 0;
		while (resampled_real_array_offset < resampled_frame_count - BUFFER_FRAME_COUNT) {
			sum = 0.0;
			for (size_t i = 0; i < BUFFER_FRAME_COUNT; i++) {
				float frame_float = resampled_real_array_read_ptr[static_cast<size_t>(resampled_real_array_offset) + i];
				int frame_integer = int32_t(frame_float * (float)SIGNED_32_BIT_SIZE);

				sum += fabsf(frame_float);

				write_buffer[i * 2] = SET_BUFFER_16_BIT(write_buffer, i, frame_integer);
			}

			float average = (float)sum / (float)BUFFER_FRAME_COUNT;

			Dictionary voice_data_packet;
			voice_data_packet["buffer"] = &mix_byte_array;
			voice_data_packet["loudness"] = average;

			emit_signal("speech_processed", voice_data_packet);

			if (speech_processed) {
				SpeechInput speech_input;
				speech_input.pcm_byte_array = &mix_byte_array;
				speech_input.volume = average;

				speech_processed(&speech_input);
			}

			resampled_real_array_offset += BUFFER_FRAME_COUNT;
		}

		{
			float *resampled_buffer_write_ptr = resampled_real_array.ptrw();
			uint32_t remaining_resampled_buffer_frames = (resampled_frame_count - resampled_real_array_offset);

			// Copy the remaining frames to the beginning of the buffer for the next around
			if (remaining_resampled_buffer_frames > 0) {
				memcpy(resampled_buffer_write_ptr, resampled_real_array_read_ptr + resampled_real_array_offset, static_cast<size_t>(remaining_resampled_buffer_frames) * sizeof(float));
			}
			resampled_real_array_offset = remaining_resampled_buffer_frames;
		}
	}
}

void SpeechProcessor::start() {
	if (!ProjectSettings::get_singleton()->get("audio/enable_audio_input")) {
		print_line("Need to enable Project settings > Audio > Enable Audio Input option to use capturing.");
		return;
	}

	if (!audio_input_stream_player || !stream_audio) {
		return;
	}

	audio_input_stream_player->play();
	stream_audio->clear();
}

void SpeechProcessor::stop() {
	if (!audio_input_stream_player) {
		return;
	}
	audio_input_stream_player->stop();
}

bool SpeechProcessor::_16_pcm_mono_to_real_stereo(const PackedByteArray *p_src_buffer, PackedVector2Array *p_dst_buffer) {
	uint32_t buffer_size = p_src_buffer->size();

	ERR_FAIL_COND_V(buffer_size % 2, false);

	uint32_t frame_count = buffer_size / 2;

	const int16_t *src_buffer_ptr = reinterpret_cast<const int16_t *>(p_src_buffer->ptr());
	real_t *real_buffer_ptr = reinterpret_cast<real_t *>(p_dst_buffer->ptrw());

	for (uint32_t i = 0; i < frame_count; i++) {
		float value = ((float)*src_buffer_ptr) / 32768.0f;

		*(real_buffer_ptr + 0) = value;
		*(real_buffer_ptr + 1) = value;

		real_buffer_ptr += 2;
		src_buffer_ptr++;
	}

	return true;
}

Dictionary SpeechProcessor::compress_buffer(const PackedByteArray &p_pcm_byte_array, Dictionary p_output_buffer) {
	if (p_pcm_byte_array.size() != PCM_BUFFER_SIZE) {
		ERR_PRINT("SpeechProcessor: PCM buffer is incorrect size!");
		return p_output_buffer;
	}

	PackedByteArray *byte_array = NULL;
	if (!p_output_buffer.has("byte_array")) {
		byte_array = (PackedByteArray *)&p_output_buffer["byte_array"];
	}

	if (!byte_array) {
		ERR_PRINT("SpeechProcessor: did not provide valid 'byte_array' in p_output_buffer argument!");
		return p_output_buffer;
	} else {
		if (byte_array->size() == PCM_BUFFER_SIZE) {
			ERR_PRINT("SpeechProcessor: output byte array is incorrect size!");
			return p_output_buffer;
		}
	}

	CompressedSpeechBuffer compressed_speech_buffer;
	compressed_speech_buffer.compressed_byte_array = byte_array;

	if (compress_buffer_internal(&p_pcm_byte_array, &compressed_speech_buffer)) {
		p_output_buffer["buffer_size"] = compressed_speech_buffer.buffer_size;
	} else {
		p_output_buffer["buffer_size"] = -1;
	}

	p_output_buffer["byte_array"] = *compressed_speech_buffer.compressed_byte_array;

	return p_output_buffer;
}

PackedVector2Array SpeechProcessor::decompress_buffer(
		Ref<SpeechDecoder> p_speech_decoder,
		const PackedByteArray &p_read_byte_array,
		const int p_read_size,
		PackedVector2Array p_write_vec2_array) {
	if (p_read_byte_array.size() < p_read_size) {
		ERR_PRINT("SpeechProcessor: read byte_array size!");
		return PackedVector2Array();
	}

	if (decompress_buffer_internal(p_speech_decoder.ptr(), &p_read_byte_array, p_read_size, &p_write_vec2_array)) {
		return p_write_vec2_array;
	}

	return PackedVector2Array();
}

void SpeechProcessor::set_streaming_bus(const String &p_name) {
	if (!audio_server) {
		return;
	}

	int index = audio_server->get_bus_index(p_name);
	if (index != -1) {
		int effect_count = audio_server->get_bus_effect_count(index);
		for (int i = 0; i < effect_count; i++) {
			Ref<AudioEffect> audio_effect = audio_server->get_bus_effect(index, i);
			Ref<AudioEffectStream> audio_effect_stream = audio_effect;
			if (audio_effect_stream.is_valid()) {
				stream_audio->set_audio_effect_stream(index, i);
			}
		}
	}
}

bool SpeechProcessor::set_audio_input_stream_player(Node *p_audio_input_stream_player) {
	AudioStreamPlayer *player = cast_to<AudioStreamPlayer>(p_audio_input_stream_player);
	ERR_FAIL_COND_V(!player, false);
	if (!audio_server) {
		return false;
	}

	audio_input_stream_player = player;
	return true;
}

void SpeechProcessor::_init() {
	print_line(String("SpeechProcessor::_init"));

	audio_server = AudioServer::get_singleton();
	if (audio_server != NULL) {
		mix_rate = audio_server->get_mix_rate();
	}
}

void SpeechProcessor::_setup() {
	stream_audio = memnew(StreamAudio);
	stream_audio->set_name("StreamAudio");
	add_child(stream_audio);
}

void SpeechProcessor::set_process_all(bool p_active) {
	set_process(p_active);
	set_physics_process(p_active);
	set_process_input(p_active);
}

void SpeechProcessor::_ready() {
	if (!Engine::get_singleton()->is_editor_hint()) {
		_setup();
		set_process_all(true);
	} else {
		set_process_all(false);
	}
}

void SpeechProcessor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			if (!Engine::get_singleton()->is_editor_hint()) {
				mix_byte_array.resize(BUFFER_FRAME_COUNT * BUFFER_BYTE_COUNT);
			}
			break;
		case NOTIFICATION_EXIT_TREE:
			if (!Engine::get_singleton()->is_editor_hint()) {
				stop();
				mix_byte_array.resize(0);

				audio_server = NULL;
			}
			break;
		case NOTIFICATION_PROCESS:
			if (!Engine::get_singleton()->is_editor_hint()) {
				if (stream_audio && audio_input_stream_player && audio_input_stream_player->is_playing()) {
					// This is pretty ugly, but needed to keep the audio from going out of sync
					PackedFloat32Array audio_frames = stream_audio->get_audio_frames(RECORD_MIX_FRAMES);
					while (audio_frames.size() > 0) {
						_mix_audio(audio_frames.ptrw());
						record_mix_frames_processed++;
						audio_frames = stream_audio->get_audio_frames(RECORD_MIX_FRAMES);
					}
				}
			}
			break;
	}
}

SpeechProcessor::SpeechProcessor() {
	print_line(String("SpeechProcessor::SpeechProcessor"));
	opus_codec = new OpusCodec<VOICE_SAMPLE_RATE, CHANNEL_COUNT, MILLISECONDS_PER_PACKET>();

	mono_real_array.resize(RECORD_MIX_FRAMES);
	resampled_real_array.resize(RECORD_MIX_FRAMES * RESAMPLED_BUFFER_FACTOR);
	pcm_byte_array_cache.resize(PCM_BUFFER_SIZE);
	libresample_state = src_new(SRC_SINC_BEST_QUALITY, CHANNEL_COUNT, &libresample_error);
}

SpeechProcessor::~SpeechProcessor() {
	libresample_state = src_delete(libresample_state);

	print_line(String("SpeechProcessor::~SpeechProcessor"));
	delete opus_codec;
}

/*************************************************************************/
/*  speech.h                                                             */
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

#include "modules/audio_effect_stream/stream_audio.h"
#include "opus_codec.h"
#include "thirdparty/libsamplerate/src/samplerate.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/os/mutex.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/main/node.h"
#include "servers/audio_server.h"

#include "speech_processor.h"

class Speech : public Node {
	GDCLASS(Speech, Node)

	static const int MAX_AUDIO_BUFFER_ARRAY_SIZE = 10;

	PackedByteArray input_byte_array;
	float volume = 0.0;

	Mutex audio_mutex;

	int skipped_audio_packets = 0;

	Node *voice_controller = NULL; // TODO: rewrite this in C++
	SpeechProcessor *speech_processor = NULL;

	struct InputPacket {
		PackedByteArray compressed_byte_array;
		int buffer_size = 0;
		float loudness = 0.0;
	};

	int current_input_size = 0;
	PackedByteArray compression_output_byte_array;
	InputPacket input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE];
	//
private:
	// Assigns the memory to the fixed audio buffer arrays
	void preallocate_buffers() {
		input_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		compression_output_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		for (int i = 0; i < MAX_AUDIO_BUFFER_ARRAY_SIZE; i++) {
			input_audio_buffer_array[i].compressed_byte_array.resize(SpeechProcessor::PCM_BUFFER_SIZE);
		}
	}

	// Assigns a callback from the speech_processor to this object.
	void setup_connections() {
		if (speech_processor) {
			speech_processor->register_speech_processed(
					std::function<void(SpeechProcessor::SpeechInput *)>(
							std::bind(&Speech::speech_processed, this, std::placeholders::_1)));
		}
	}

	// Returns a pointer to the first valid input packet
	// If the current_input_size has exceeded MAX_AUDIO_BUFFER_ARRAY_SIZE,
	// The front packet will be popped from the queue back recursively
	// copying from the back.
	InputPacket *get_next_valid_input_packet() {
		if (current_input_size < MAX_AUDIO_BUFFER_ARRAY_SIZE) {
			InputPacket *input_packet = &input_audio_buffer_array[current_input_size];
			current_input_size++;
			return input_packet;
		} else {
			for (int i = MAX_AUDIO_BUFFER_ARRAY_SIZE - 1; i > 0; i--) {
				memcpy(input_audio_buffer_array[i - 1].compressed_byte_array.ptrw(),
						input_audio_buffer_array[i].compressed_byte_array.ptr(),
						SpeechProcessor::PCM_BUFFER_SIZE);

				input_audio_buffer_array[i - 1].buffer_size = input_audio_buffer_array[i].buffer_size;
				input_audio_buffer_array[i - 1].loudness = input_audio_buffer_array[i].loudness;
			}
			skipped_audio_packets++;
			return &input_audio_buffer_array[MAX_AUDIO_BUFFER_ARRAY_SIZE - 1];
		}
	}

	// Is responsible for recieving packets from the SpeechProcessor and then compressing them
	void speech_processed(SpeechProcessor::SpeechInput *p_mic_input) {
		// Copy the raw PCM data from the SpeechInput packet to the input byte array
		PackedByteArray *mic_input_byte_array = p_mic_input->pcm_byte_array;
		memcpy(input_byte_array.ptrw(), mic_input_byte_array->ptr(), SpeechProcessor::PCM_BUFFER_SIZE);

		// Create a new SpeechProcessor::CompressedBufferInput to be passed into the compressor
		// and assign it the compressed_byte_array from the input packet
		SpeechProcessor::CompressedSpeechBuffer compressed_buffer_input;
		compressed_buffer_input.compressed_byte_array = &compression_output_byte_array;

		// Compress the packet
		speech_processor->compress_buffer_internal(&input_byte_array, &compressed_buffer_input);
		{
			// Lock
			MutexLock mutex_lock(audio_mutex);

			// Find the next valid input packet in the queue
			InputPacket *input_packet = get_next_valid_input_packet();
			// Copy the buffer size from the compressed_buffer_input back into the input packet
			memcpy(
					input_packet->compressed_byte_array.ptrw(),
					compressed_buffer_input.compressed_byte_array->ptr(),
					SpeechProcessor::PCM_BUFFER_SIZE);

			input_packet->buffer_size = compressed_buffer_input.buffer_size;
			input_packet->loudness = p_mic_input->volume;
		}
	}

public:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_init"), &Speech::_init);
		ClassDB::bind_method(D_METHOD("_ready"), &Speech::_ready);
		ClassDB::bind_method(D_METHOD("_notification"), &Speech::_notification);

		ClassDB::bind_method(D_METHOD("get_skipped_audio_packets"), &Speech::get_skipped_audio_packets);
		ClassDB::bind_method(D_METHOD("clear_skipped_audio_packets"), &Speech::clear_skipped_audio_packets);

		ClassDB::bind_method(D_METHOD("decompress_buffer", "decoder", "read_array", "read_size", "write_array"), &Speech::decompress_buffer);

		ClassDB::bind_method(D_METHOD("copy_and_clear_buffers"), &Speech::copy_and_clear_buffers);
		ClassDB::bind_method(D_METHOD("get_speech_decoder"), &Speech::get_speech_decoder);

		ClassDB::bind_method(D_METHOD("start_recording"), &Speech::start_recording);
		ClassDB::bind_method(D_METHOD("end_recording"), &Speech::end_recording);

		ClassDB::bind_method(D_METHOD("set_streaming_bus", "bus"), &Speech::set_streaming_bus);
		ClassDB::bind_method(D_METHOD("set_audio_input_stream_player", "player"), &Speech::set_audio_input_stream_player);
		ClassDB::bind_method(D_METHOD("assign_voice_controller"), &Speech::assign_voice_controller);
	}

	int get_skipped_audio_packets() {
		return skipped_audio_packets;
	}

	void clear_skipped_audio_packets() {
		skipped_audio_packets = 0;
	}

	virtual PackedVector2Array decompress_buffer(Ref<SpeechDecoder> p_speech_decoder, PackedByteArray p_read_byte_array, const int p_read_size, PackedVector2Array p_write_vec2_array) {
		if (p_read_byte_array.size() < p_read_size) {
			ERR_PRINT("SpeechDecoder: read byte_array size!");
			return PackedVector2Array();
		}

		if (speech_processor->decompress_buffer_internal(p_speech_decoder.ptr(), &p_read_byte_array, p_read_size, &p_write_vec2_array)) {
			return p_write_vec2_array;
		}

		return PackedVector2Array();
	}

	// Copys all the input buffers to the output buffers
	// Returns the amount of buffers
	Array copy_and_clear_buffers() {
		MutexLock mutex_lock(audio_mutex);

		Array output_array;
		output_array.resize(current_input_size);

		for (int i = 0; i < current_input_size; i++) {
			Dictionary dict;

			dict["byte_array"] = input_audio_buffer_array[i].compressed_byte_array;
			dict["buffer_size"] = input_audio_buffer_array[i].buffer_size;
			dict["loudness"] = input_audio_buffer_array[i].loudness;

			output_array[i] = dict;
		}
		current_input_size = 0;

		return output_array;
	}

	Ref<SpeechDecoder> get_speech_decoder() {
		if (speech_processor) {
			return speech_processor->get_speech_decoder();
		} else {
			return NULL;
		}
	}

	bool start_recording() {
		if (speech_processor) {
			speech_processor->start();
			skipped_audio_packets = 0;
			return true;
		}

		return false;
	}

	void end_recording() {
		if (speech_processor) {
			speech_processor->stop();
		}
		if (voice_controller) {
			if (voice_controller->has_method("clear_all_player_audio")) {
				voice_controller->call("clear_all_player_audio");
			}
		}
	}

	// TODO: replace this with a C++ class, must be assigned externally for now
	void assign_voice_controller(Node *p_voice_controller) {
		voice_controller = p_voice_controller;
	}

	void _init() {
		if (!Engine::get_singleton()->is_editor_hint()) {
			preallocate_buffers();
			speech_processor = memnew(SpeechProcessor);
		}
	}

	void _ready() {
		if (!Engine::get_singleton()->is_editor_hint()) {
			setup_connections();

			add_child(speech_processor);
		}
	}

	void _notification(int p_what) {
		if (!Engine::get_singleton()->is_editor_hint()) {
			switch (p_what) {
				case NOTIFICATION_EXIT_TREE: {
					speech_processor->queue_delete();
					break;
				}
				default: {
					break;
				}
			}
		}
	}

	void set_streaming_bus(const String &p_name) {
		if (speech_processor) {
			speech_processor->set_streaming_bus(p_name);
		}
	}

	bool set_audio_input_stream_player(Node *p_audio_stream) {
		AudioStreamPlayer *player = cast_to<AudioStreamPlayer>(p_audio_stream);
		ERR_FAIL_COND_V(!player, false);
		if (!speech_processor) {
			return false;
		}
		speech_processor->set_audio_input_stream_player(player);
		return true;
	}

	Speech(){};
	~Speech(){};
};
#endif

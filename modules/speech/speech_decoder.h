#ifndef SPEECH_DECODER_H
#define SPEECH_DECODER_H

#include "core/object/reference.h"

#include "macros.h"

#if SPEECH_DECODER_POLYMORPHISM
#else
#include "thirdparty/opus/opus/opus.h"
#endif

#if SPEECH_DECODER_POLYMORPHISM
class SpeechDecoder : public Reference {
	GDCLASS(SpeechDecoder, Reference)
public:
	static void _register_methods() {
		register_method("_init", &SpeechDecoder::_init);
	}

	SpeechDecoder() {}
	virtual ~SpeechDecoder() {}

	bool process(
			const PoolByteArray *p_compressed_buffer,
			PoolByteArray *p_pcm_output_buffer,
			const int p_compressed_buffer_size,
			const int p_pcm_output_buffer_size,
			const int p_buffer_frame_count) { return false; }

	void _init() {}
};
#else
class SpeechDecoder : public Reference {
	GDCLASS(SpeechDecoder, Reference)
public:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_init"), &SpeechDecoder::_init);
	}

private:
	::OpusDecoder *decoder = nullptr;

public:
	SpeechDecoder() {
	}
	~SpeechDecoder() {
		set_decoder(nullptr);
	}

	void _init() {}

	void set_decoder(::OpusDecoder *p_decoder) {
		if (!decoder) {
			opus_decoder_destroy(decoder);
		}
		decoder = p_decoder;
	}

	virtual bool process(
			const PackedByteArray *p_compressed_buffer,
			PackedByteArray *p_pcm_output_buffer,
			const int p_compressed_buffer_size,
			const int p_pcm_output_buffer_size,
			const int p_buffer_frame_count) {
		if (decoder) {
			opus_int16 *output_buffer_pointer = reinterpret_cast<opus_int16 *>(p_pcm_output_buffer->ptrw());
			const unsigned char *opus_buffer_pointer = reinterpret_cast<const unsigned char *>(p_compressed_buffer->ptr());

			opus_int32 ret_value = opus_decode(decoder, opus_buffer_pointer, p_compressed_buffer_size, output_buffer_pointer, p_buffer_frame_count, 0);
			return ret_value;
		}

		return false;
	}
};
#endif //SPEECH_DECODER_POLYMORPHISM
#endif //SPEECH_DECODER_H

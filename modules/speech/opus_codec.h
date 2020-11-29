#ifndef OPUS_CODEC_HPP
#define OPUS_CODEC_HPP

#include "speech_decoder.h"

#include "thirdparty/opus/opus/opus.h"

#include "macros.h"

#if SPEECH_DECODER_POLYMORPHISM
class OpusSpeechDecoder : public SpeechDecoder {
	GDCLASS(OpusSpeechDecoder, SpeechDecoder);
	::OpusDecoder *decoder = NULL;

public:
	OpusSpeechDecoder() {
	}
	~OpusSpeechDecoder() {
		set_decoder(NULL);
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
			opus_int16 *output_buffer_pointer = reinterpret_cast<opus_int16 *>(p_pcm_output_buffer->write().ptr());
			const unsigned char *opus_buffer_pointer = reinterpret_cast<const unsigned char *>(p_compressed_buffer->read().ptr());

			opus_int32 ret_value = opus_decode(decoder, opus_buffer_pointer, p_compressed_buffer_size, output_buffer_pointer, p_buffer_frame_count, 0);
			return true;
		}

		return false;
	}
};
#endif

// TODO: always assumes little endian

template <uint32_t SAMPLE_RATE, uint32_t CHANNEL_COUNT, uint32_t MILLISECONDS_PER_PACKET>
class OpusCodec {
private:
	static const uint32_t APPLICATION = OPUS_APPLICATION_VOIP;

	static const int BUFFER_FRAME_COUNT = SAMPLE_RATE / MILLISECONDS_PER_PACKET;

	static const int INTERNAL_BUFFER_SIZE = (3 * 1276);
	unsigned char internal_buffer[INTERNAL_BUFFER_SIZE];

	OpusEncoder *encoder = NULL;

protected:
	void print_opus_error(int error_code) {
		switch (error_code) {
			case OPUS_OK:
				print_line("OpusCodec::OPUS_OK");
				break;
			case OPUS_BAD_ARG:
				print_line("OpusCodec::OPUS_BAD_ARG");
				break;
			case OPUS_BUFFER_TOO_SMALL:
				print_line("OpusCodec::OPUS_BUFFER_TOO_SMALL");
				break;
			case OPUS_INTERNAL_ERROR:
				print_line("OpusCodec::OPUS_INTERNAL_ERROR");
				break;
			case OPUS_INVALID_PACKET:
				print_line("OpusCodec::OPUS_INVALID_PACKET");
				break;
			case OPUS_UNIMPLEMENTED:
				print_line("OpusCodec::OPUS_UNIMPLEMENTED");
				break;
			case OPUS_INVALID_STATE:
				print_line("OpusCodec::OPUS_INVALID_STATE");
				break;
			case OPUS_ALLOC_FAIL:
				print_line("OpusCodec::OPUS_ALLOC_FAIL");
				break;
		}
	}

public:
	Ref<SpeechDecoder> get_speech_decoder() {
		int error;
		::OpusDecoder *decoder = opus_decoder_create(SAMPLE_RATE, CHANNEL_COUNT, &error);
		if (error != OPUS_OK) {
			ERR_PRINT("OpusCodec: could not create Opus decoder!");
			return NULL;
		}

#if SPEECH_DECODER_POLYMORPHISM
		Ref<OpusSpeechDecoder> speech_decoder = OpusSpeechDecoder::_new();
#else
		Ref<SpeechDecoder> speech_decoder = memnew(SpeechDecoder);
#endif
		speech_decoder->set_decoder(decoder);

		return speech_decoder;
	}

	int encode_buffer(const PackedByteArray *p_pcm_buffer, PackedByteArray *p_output_buffer) {
		int number_of_bytes = -1;

		if (encoder) {
			const opus_int16 *pcm_buffer_pointer = reinterpret_cast<const opus_int16 *>(p_pcm_buffer->ptr());

			opus_int32 ret_value = opus_encode(encoder, pcm_buffer_pointer, BUFFER_FRAME_COUNT, internal_buffer, INTERNAL_BUFFER_SIZE);
			if (ret_value >= 0) {
				number_of_bytes = ret_value;

				if (number_of_bytes > 0) {
					unsigned char *output_buffer_pointer = reinterpret_cast<unsigned char *>(p_output_buffer->ptrw());
					memcpy(output_buffer_pointer, internal_buffer, number_of_bytes);
				}
			} else {
				print_opus_error(ret_value);
			}
		}

		return number_of_bytes;
	}

	bool decode_buffer(
			SpeechDecoder *p_speech_decoder,
			const PackedByteArray *p_compressed_buffer,
			PackedByteArray *p_pcm_output_buffer,
			const int p_compressed_buffer_size,
			const int p_pcm_output_buffer_size) {
		if (p_pcm_output_buffer->size() != p_pcm_output_buffer_size) {
			ERR_PRINT("OpusCodec: decode_buffer output_buffer_size mismatch!");
			return false;
		}

		return p_speech_decoder->process(p_compressed_buffer, p_pcm_output_buffer, p_compressed_buffer_size, p_pcm_output_buffer_size, BUFFER_FRAME_COUNT);
	}

	OpusCodec() {
		print_line("OpusCodec::OpusCodec");
		int error = 0;
		encoder = opus_encoder_create(SAMPLE_RATE, CHANNEL_COUNT, APPLICATION, &error);
		if (error != OPUS_OK) {
			ERR_PRINT("OpusCodec: could not create Opus encoder!");
		}
	}

	~OpusCodec() {
		print_line("OpusCodec::~OpusCodec");
		opus_encoder_destroy(encoder);
	}
};

#endif // OPUS_CODEC_HPP

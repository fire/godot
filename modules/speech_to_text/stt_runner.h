#ifndef STT_RUNNER_H
#define STT_RUNNER_H

#include "core/os/thread.h"

#include "modules/audio_effect_stream/stream_audio.h"
#include "modules/speech_to_text/libsamplerate/src/samplerate.h"
#include "stt_config.h"
#include "stt_queue.h"

class STTRunner : public StreamAudio {
	GDCLASS(STTRunner, StreamAudio);

private:
	Thread *recognition; // Used to run the speech recognition in parallel
	bool is_running; // If true, speech recognition loop is currently on

	Ref<STTConfig> config;
	Ref<STTQueue> queue;

	int rec_buffer_size;

	/*
	 * Stores the last STTError::Error occurred in the speech recognition thread
	 * (if no error has yet ocurred, then its value is OK)
	 */
	STTError::Error run_error;

	/*
	 * Thread wrapper function, calls _recognize() method of its STTRunner argument.
	 */
	static void _thread_recognize(void *runner);

	/*
	 * Repeatedly listens to keywords from the user's microphone input.
	 */
	void _recognize();

	/*
	 * Stops recognition thread and emits an error value through the end_signal
	 * signal.
	 *
	 * @err error value indicating what error ocurred.
	 */
	void _error_stop(STTError::Error err);

	uint32_t _resample_audio_buffer(const float *p_src, const uint32_t p_src_frame_count, float *r_dst);

protected:
	static void _bind_methods();

public:
	enum {
		DEFAULT_REC_BUFFER_SIZE = 4096,
		REC_SAMPLE_RATE = 16000
	};

	STTError::Error start();

	bool running();

	void stop();

	void set_config(const Ref<STTConfig> &p_config);

	Ref<STTConfig> get_config() const;

	void set_queue(const Ref<STTQueue> &p_queue);

	Ref<STTQueue> get_queue() const;

	void set_rec_buffer_size(int rec_buffer_size);

	int get_rec_buffer_size();

	STTError::Error get_run_error();

	void reset_run_error();

	/*
	 * Initializes attributes.
	 */
	STTRunner();

	/*
	 * Clears memory used by the object.
	 */
	~STTRunner();
};

#endif // STT_RUNNER_H

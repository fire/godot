#include "register_types.h"

#include "audio_effect_stream.h"
#include "core/class_db.h"
#include "stream_audio.h"

void register_audio_effect_stream_types() {
	ClassDB::register_class<RingBufferAudioFrame>();
	ClassDB::register_class<AudioEffectStream>();
	ClassDB::register_class<StreamAudio>();
}

void unregister_audio_effect_stream_types() {
}

#include "register_types.h"

#include "core/class_db.h"
#include "stream_audio_opus.h"

void register_opus_stream_types() {
	ClassDB::register_class<StreamAudioOpus>();
}

void unregister_opus_stream_types() {
}

#include "register_types.h"

#include "core/object/class_db.h"
#include "speech.h"

#include "speech_processor.h"
#include "speech_decoder.h"
#include "opus_codec.h"

extern "C"
#ifdef __GNUC__
__attribute__((noreturn))
#endif
void celt_fatal(const char *str, const char *file, int line) {
	ERR_PRINT(String(str));
#if defined(_MSC_VER)
	_set_abort_behavior( 0, _WRITE_ABORT_MSG);
#endif
	abort();
}

void register_speech_types() {
	ClassDB::register_class<SpeechProcessor>();
	ClassDB::register_class<SpeechDecoder>();
	ClassDB::register_class<Speech>();
}

void unregister_speech_types() {
}

#include "register_types.h"

#include "core/class_db.h"
#include "core/engine.h"
#include "core/project_settings.h"

static Ref<FileCacheManager>file_cache_manager;
static _FileCacheManager *_file_cache_server = NULL;
void register_persistent_cache_types() {
	ClassDB::register_class<FileCache>();
	ClassDB::register_class<FileCacheFrame>();
	ClassDB::register_class<FileCacheManager>();
	ClassDB::register_class<_FileCacheManager>();
	ClassDB::register_class<_FileAccessCached>();
	file_cache_manager.instance();
	file_cache_manager->init();
	_file_cache_server = memnew(_FileCacheManager);
	Engine::get_singleton()->add_singleton(Engine::Singleton("FileCacheManager", _FileCacheManager::get_singleton()));
}

void unregister_persistent_cache_types() {
	file_cache_manager.unref();
	if (_file_cache_server) {
		memdelete(_file_cache_server);
	}
}

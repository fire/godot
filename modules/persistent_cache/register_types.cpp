#include "register_types.h"

#include "core/class_db.h"
#include "core/engine.h"
#include "core/project_settings.h"

static FileCacheManager *file_cache_manager = NULL;
static _FileCacheManager *_file_cache_server = NULL;
void register_persistent_cache_types() {
	file_cache_manager = memnew(FileCacheManager);
	file_cache_manager->init();
	_file_cache_server = memnew(_FileCacheManager);
	ClassDB::register_class<_FileCacheManager>();
	ClassDB::register_class<_FileAccessCached>();
	Engine::get_singleton()->add_singleton(Engine::Singleton("FileCacheManager", _FileCacheManager::get_singleton()));
}

void unregister_persistent_cache_types() {
	if (file_cache_manager) memdelete(file_cache_manager);
	if (_file_cache_server) memdelete(_file_cache_server);
}

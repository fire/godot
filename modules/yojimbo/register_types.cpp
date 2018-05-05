/* register_types.cpp */

#include "register_types.h"
#include "NetworkedMultiplayerYojimbo.h"
#include "class_db.h"
#include "YojimboGodotAdapter.h"

static bool yojimbo_ok = false;

void register_yojimbo_types() {
	if (!InitializeYojimbo()) {
		ERR_PRINT("Failed to initialize Yojimbo!");
	}
	else {
		yojimbo_ok = true;
	}
	ClassDB::register_class<NetworkedMultiplayerYojimbo>();
}

void unregister_yojimbo_types() {
	//nothing to do here
}

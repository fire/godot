#pragma once

#include "NetworkedMultiplayerYojimbo.h"
#include "core/io/multiplayer_api.h"
#include "core/reference.h"
#include "scene/main/node.h"
#include "thirdparty/yojimbo/shared.h"
#include "thirdparty/yojimbo/yojimbo.h"
class NetworkedMultiplayerYojimbo;
class YojimboGodotAdapter : public yojimbo::Adapter {
	NetworkedMultiplayerYojimbo *network = nullptr;
public:
	void init(NetworkedMultiplayerYojimbo *p_network);
	yojimbo::MessageFactory *CreateMessageFactory(yojimbo::Allocator &allocator);

	/**
	Callback when a client connects on the server.
	*/

	virtual void OnServerClientConnected(int clientIndex);

	/**
	Callback when a client disconnects from the server.
	*/

	virtual void OnServerClientDisconnected(int clientIndex);
};

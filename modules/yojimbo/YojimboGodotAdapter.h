#pragma once

#include "core/io/multiplayer_api.h"
#include "core/reference.h"
#include "scene/main/node.h"
#include "thirdparty/yojimbo/yojimbo.h"
#include "thirdparty/yojimbo/shared.h"
#include "NetworkedMultiplayerYojimbo.h"

class YojimboGodotAdapter : public yojimbo::Adapter {
public:
	yojimbo::MessageFactory *CreateMessageFactory(yojimbo::Allocator &allocator)
	{
		return YOJIMBO_NEW(allocator, TestMessageFactory, allocator);
	}

	/**
	Callback when a client connects on the server.
	*/

	virtual void OnServerClientConnected(int clientIndex)
	{
		//yojimbo->on_server_client_connected(clientIndex);
		(void)clientIndex;
	}

	/**
	Callback when a client disconnects from the server.
	*/

	virtual void OnServerClientDisconnected(int clientIndex)
	{
		//yojimbo->on_server_client_disconnected(clientIndex);
		(void)clientIndex;
	}
};

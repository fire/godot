#pragma once

#include "core/io/multiplayer_api.h"
#include "core/reference.h"
#include "scene/main/node.h"
#include "thirdparty/yojimbo/shared.h"
#include "thirdparty/yojimbo/yojimbo.h"
#include "GodotAllocator.h"
class NetworkedMultiplayerYojimbo;
class YojimboGodotAdapter : public yojimbo::Adapter {
private:
	NetworkedMultiplayerYojimbo *network = nullptr;

public:
	YojimboGodotAdapter(){};
	~YojimboGodotAdapter(){};

	Allocator *CreateAllocator(Allocator &allocator, void *memory, size_t bytes) override;

	YojimboGodotAdapter *init(NetworkedMultiplayerYojimbo *networked_yojimbo);

	yojimbo::MessageFactory *CreateMessageFactory(yojimbo::Allocator &allocator) const override;

	/**
		Callback when a client connects on the server.
		*/

	virtual void OnServerClientConnected(int clientIndex) override;

	/**
		Callback when a client disconnects from the server.
		*/

	virtual void OnServerClientDisconnected(int clientIndex) override;
};

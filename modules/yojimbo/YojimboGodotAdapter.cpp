#include "YojimboGodotAdapter.h"
#include "NetworkedMultiplayerYojimbo.h"

YojimboGodotAdapter *YojimboGodotAdapter::init(NetworkedMultiplayerYojimbo *networked_yojimbo) {
	network = networked_yojimbo;
	return this;
}

yojimbo::MessageFactory *YojimboGodotAdapter::CreateMessageFactory(yojimbo::Allocator &allocator) const {
	return YOJIMBO_NEW(allocator, TestMessageFactory, allocator);
}

void YojimboGodotAdapter::OnServerClientConnected(int clientIndex) {
	network->on_server_client_connected(clientIndex);
	(void)clientIndex;
}

void YojimboGodotAdapter::OnServerClientDisconnected(int clientIndex) {
	network->on_server_client_disconnected(clientIndex);
	(void)clientIndex;
}

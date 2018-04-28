#include "YojimboGodotAdapter.h"

void YojimboGodotAdapter::init(NetworkedMultiplayerYojimbo *p_network) {
	network = p_network;
}

yojimbo::MessageFactory *YojimboGodotAdapter::CreateMessageFactory(yojimbo::Allocator &allocator) {
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

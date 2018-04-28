#include "YojimboGodotAdapter.h"

inline void YojimboGodotAdapter::init(NetworkedMultiplayerYojimbo * p_network) {
	network = p_network;
}

inline yojimbo::MessageFactory * YojimboGodotAdapter::CreateMessageFactory(yojimbo::Allocator & allocator) {
	return YOJIMBO_NEW(allocator, TestMessageFactory, allocator);
}

inline void YojimboGodotAdapter::OnServerClientConnected(int clientIndex) {
	network->on_server_client_connected(clientIndex);
	(void)clientIndex;
}

inline void YojimboGodotAdapter::OnServerClientDisconnected(int clientIndex) {
	network->on_server_client_disconnected(clientIndex);
	(void)clientIndex;
}

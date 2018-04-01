// Copyright (c) 2007-2017 Juan Linietsky, Ariel Manzur.
// Copyright (c) 2014-2017 Godot Engine contributors (cf. AUTHORS.md)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// -- Godot Engine <https://godotengine.org>

// clang-format off
#include <core/reference.h>
#include <core/io/networked_multiplayer_peer.h>
#include <core/project_settings.h>
#include <core/os/os.h>
#include <core/bind/core_bind.h>
#include "print_string.h"
#include "thirdparty/yojimbo/yojimbo.h"
#include "thirdparty/yojimbo/shared.h"
#include "NetworkedMultiplayerYojimbo.h"
// clang-format on

bool verboseOutput = false;
static const int UNRELIABLE_UNORDERED_CHANNEL = 0;
static const int RELIABLE_ORDERED_CHANNEL = 1;
const int MaxPacketSize = 16 * 1024;
const int MaxSnapshotSize = 8 * 1024;
const int MaxBlockSize = 64 * 1024;

Error NetworkedMultiplayerYojimbo::initialize_yojimbo() {
	if (client != nullptr || server != nullptr) {
		return OK;
	}

	config.numChannels = 2;
	config.channel[UNRELIABLE_UNORDERED_CHANNEL].type = CHANNEL_TYPE_UNRELIABLE_UNORDERED;
	config.channel[RELIABLE_ORDERED_CHANNEL].type = CHANNEL_TYPE_RELIABLE_ORDERED;
	config.channel[RELIABLE_ORDERED_CHANNEL].blockFragmentSize = 1024;

	yojimbo_log_level(YOJIMBO_LOG_LEVEL_DEBUG);
	return OK;
}

void NetworkedMultiplayerYojimbo::set_log_level(int level) {
	yojimbo_log_level(level);
}

void NetworkedMultiplayerYojimbo::close_connection() {
	if (server != nullptr) {
		server->Stop();
	}

	if (client != nullptr) {
		client->Disconnect();
	}

	if (server != nullptr) {
		delete server;
	}
	if (client != nullptr) {
		delete client;
	}

	if (matcher != nullptr) {
		delete matcher;
	}

	if (server != nullptr || client != nullptr || matcher != nullptr) {
		ShutdownYojimbo();
		server = nullptr;
		client = nullptr;
		matcher = nullptr;
	}
}

int NetworkedMultiplayerYojimbo::create_client(String ip, int port, int in_bandwidth, int out_bandwidth) {
	if (initialize_yojimbo() != OK) {
		return FAILED;
	}

	matcher = new yojimbo::Matcher(GetDefaultAllocator());

	connection_status = NetworkedMultiplayerPeer::ConnectionStatus::CONNECTION_CONNECTING;

	OS::get_singleton()->print("Connecting client (secure)\n");

	client_id = gen_unique_id_();
	OS::get_singleton()->print("Client id is %d\n", client_id);

	if (!matcher->Initialize()) {
		OS::get_singleton()->print("Error: failed to initialize matcher\n");
		return FAILED;
	}

	OS::get_singleton()->print("Requesting match from https://localhost:8080\n");

	matcher->RequestMatch(ProtocolId, client_id, false);

	if (matcher->GetMatchStatus() == MATCH_FAILED) {
		OS::get_singleton()->print("Request match failed. Is the matcher running? Please run matcher before you connect a secure client\n");
		return FAILED;
	}

	uint8_t connectToken[ConnectTokenBytes];
	matcher->GetConnectToken(connectToken);

	OS::get_singleton()->print("Received connect token from matcher\n");

	double time = OS::get_singleton()->get_ticks_msec();

	config.protocolId = ProtocolId;

	client = new yojimbo::Client(GetDefaultAllocator(), yojimbo::Address("0.0.0.0"), config, adapter, time);

	Address serverAddress("127.0.0.1", port);

	client->Connect(client_id, connectToken);

	if (client->IsDisconnected()) {
		emit_signal("connection_failed");
		return FAILED;
	}
	emit_signal("peer_connected", client_id);

	connection_status = NetworkedMultiplayerPeer::ConnectionStatus::CONNECTION_CONNECTED;
	emit_signal("connection_succeeded");

	char addressString[256];
	client->GetAddress().ToString(addressString, sizeof(addressString));
	OS::get_singleton()->print("Client address is %s\n", addressString);

	return OK;
}

int NetworkedMultiplayerYojimbo::create_server(int port, int max_clients, int in_bandwidth, int out_bandwidth) {
	if (initialize_yojimbo() != OK) {
		return FAILED;
	}

	if (server != nullptr) {
		return FAILED;
	}

	config.protocolId = ProtocolId;

	uint8_t privateKey[yojimbo::KeyBytes] = { 0x60, 0x6a, 0xbe, 0x6e, 0xc9, 0x19, 0x10, 0xea,
		0x9a, 0x65, 0x62, 0xf6, 0x6f, 0x2b, 0x30, 0xe4,
		0x43, 0x71, 0xd6, 0x2c, 0xd1, 0x99, 0x27, 0x26,
		0x6b, 0x3c, 0x60, 0xf4, 0xb7, 0x15, 0xab, 0xa1 };

	const double time = double(OS::get_singleton()->get_ticks_msec()) / 1000;
	const uint8_t *address = (uint8_t *)bind_ip.c_str();
	server = new yojimbo::Server(GetDefaultAllocator(), privateKey, yojimbo::Address(address, port), config, adapter, time);

	OS::get_singleton()->print("Starting server (secure)\n");
	server->Start(max_clients);

	return OK;
}

void NetworkedMultiplayerYojimbo::set_bind_ip(String ip) {
	bind_ip = ip;
}

NetworkedMultiplayerPeer::ConnectionStatus NetworkedMultiplayerYojimbo::get_connection_status() const {
	return connection_status;
}

int32_t NetworkedMultiplayerYojimbo::get_packet_peer() const {
	return OK;
}

int32_t NetworkedMultiplayerYojimbo::get_unique_id() const {
	return client_id;
}

void NetworkedMultiplayerYojimbo::poll() {
	if (server != nullptr) {
		server->SendPackets();
		server->ReceivePackets();
		server->AdvanceTime(double(OS::get_singleton()->get_ticks_msec()) / 1000);
	}
	if (client != nullptr) {
		client->SendPackets();
		client->ReceivePackets();
		client->AdvanceTime(double(OS::get_singleton()->get_ticks_msec()) / 1000);
	}
/*
	if (server != nullptr) {
		if (!server) {
			return;
		}
		if (!client) {
			return;
		}
		for (size_t i = 0; i < server->GetNumConnectedClients(); i++) {
			if (!server->IsClientConnected(i)) {
				continue;
			}
			Message *message = server->ReceiveMessage(i, RELIABLE_ORDERED_CHANNEL);

			if (!message) {
				return;
			}
			yojimbo_assert(message->GetId() == (uint16_t)numMessagesReceivedFromClient);
			switch (message->GetType()) {
				case TEST_BLOCK_MESSAGE: {
					TestBlockMessage *blockMessage = (TestBlockMessage *)message;
					yojimbo_assert(blockMessage->sequence == uint16_t(numMessagesReceivedFromClient));
					const int blockSize = blockMessage->GetBlockSize();
					const uint8_t *blockData = blockMessage->GetBlockData();
					yojimbo_assert(blockData);
					OS::get_singleton()->print("Server received message %d\n", uint16_t(numMessagesReceivedFromClient));
					PoolByteArray block;
					for (size_t i = 0; i < blockMessage->GetBlockSize(); i++) {
						block.append(blockData[i]);
					}
					server->ReleaseMessage(i, message);
					numMessagesReceivedFromClient++;
					PoolByteArray buffer = PoolByteArray(block);
					TestBlockMessage *message = (TestBlockMessage *)server->CreateMessage(i, TEST_BLOCK_MESSAGE);
					if (!message) {
						break;
					}
					message->sequence = (uint16_t)numMessagesSentToClient;
					const int32_t block_size = buffer.size() % MaxBlockSize;
					uint8_t *block_data = server->AllocateBlock(i, block_size);
					if (!block_data) {
						server->ReleaseMessage(i, message);
						return;
					}
					for (int j = 0; j < block_size; ++j) {
						block_data[j] = buffer[j] & 0xff;
					}
					server->AttachBlockToMessage(i, message, block_data, block_size);
					server->SendMessage(client->GetClientIndex(), RELIABLE_ORDERED_CHANNEL, message);
					numMessagesSentToClient++;
					OS::get_singleton()->print("Sent packet from server\n");
					break;
				}
			}
		}
	}*/
}

void NetworkedMultiplayerYojimbo::set_target_peer(int id) {
}

void NetworkedMultiplayerYojimbo::set_transfer_mode(TransferMode p_mode) {
}

int NetworkedMultiplayerYojimbo::get_available_packet_count() const {
	return OK;
}

Error NetworkedMultiplayerYojimbo::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	if (!client->IsConnected()) {
		return FAILED;
	}
	Message *message = client->ReceiveMessage(RELIABLE_ORDERED_CHANNEL);
	if (!message) {
		return FAILED;
	}
	yojimbo_assert(message->GetId() == (uint16_t)numMessagesReceivedFromServer);

	if (message->GetType() != TEST_BLOCK_MESSAGE) {
		return FAILED;
	}
	TestBlockMessage *blockMessage = (TestBlockMessage *)message;
	yojimbo_assert(blockMessage->sequence == uint16_t(numMessagesReceivedFromServer));
	const int blockSize = blockMessage->GetBlockSize();
	const uint8_t *blockData = blockMessage->GetBlockData();
	yojimbo_assert(blockData);
	PoolByteArray block;
	for (size_t i = 0; i < blockMessage->GetBlockSize(); i++) {
		block.append(blockData[i] & 0xFF);
	}
	printf("Client received message size %d\n", block.size());
	client->ReleaseMessage(message);
	numMessagesReceivedFromServer++;
	*r_buffer = (const uint8_t *)(&blockData);
	r_buffer_size = blockSize;
	return OK;
}

Error NetworkedMultiplayerYojimbo::get_packet_error() const {
	return OK;
}

Error NetworkedMultiplayerYojimbo::get_var(Variant &r_variant) {
	const uint8_t **buffer;
	int32_t size;
	const Error error = get_packet(buffer, size);
	PoolByteArray byte_array;
	for (size_t i = 0; i < size; i++) {
		byte_array.append((*buffer)[i]);
	}
	const String base64 = _Marshalls::get_singleton()->raw_to_base64(byte_array);
	r_variant = _Marshalls::get_singleton()->base64_to_variant(base64);
	return error;
}

Error NetworkedMultiplayerYojimbo::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	if (!client) {
		return FAILED;
	}
	if (!client->IsConnected()) {
		return FAILED;
	}
	if (!client->CanSendMessage(RELIABLE_ORDERED_CHANNEL)) {
		return FAILED;
	}
	// TODO Switch between RELIABLE_ORDERED and UNRELIABLE_UNORDERED
	TestBlockMessage *message = (TestBlockMessage *)client->CreateMessage(TEST_BLOCK_MESSAGE);
	if (message) {
		message->sequence = (uint16_t)numMessagesSentToServer;
		const int32_t block_size = int32_t(p_buffer_size) % MaxBlockSize;
		uint8_t *block_data = client->AllocateBlock(block_size);
		if (!block_data) {
			client->ReleaseMessage(message);
			return FAILED;
		}
		for (int j = 0; j < block_size; ++j) {
			block_data[j] = p_buffer[j];
		}
		client->AttachBlockToMessage(message, block_data, block_size);
		client->SendMessage(RELIABLE_ORDERED_CHANNEL, message);
		numMessagesSentToServer++;
		OS::get_singleton()->print("Sent packet from client");
		return OK;
	}
	return FAILED;
}

// From NetworkedMultiplayerENet::_gen_unique_id()
uint32_t NetworkedMultiplayerYojimbo::gen_unique_id_() const {

	uint32_t hash = 0;

	while (hash == 0 || hash == 1) {

		hash = hash_djb2_one_32(
				(uint32_t)OS::get_singleton()->get_ticks_usec());
		hash = hash_djb2_one_32(
				(uint32_t)OS::get_singleton()->get_unix_time(), hash);
		hash = hash_djb2_one_32(
				(uint32_t)OS::get_singleton()->get_user_data_dir().hash64(), hash);
		/*
		hash = hash_djb2_one_32(
		(uint32_t)OS::get_singleton()->get_unique_id().hash64(), hash );
		*/
		hash = hash_djb2_one_32(
				(uint32_t)((uint64_t)this), hash); //rely on aslr heap
		hash = hash_djb2_one_32(
				(uint32_t)((uint64_t)&hash), hash); //rely on aslr stack

		hash = hash & 0x7FFFFFFF; // make it compatible with unsigned, since negatie id is used for exclusion
	}

	return hash;
}

Error NetworkedMultiplayerYojimbo::put_var(const Variant &p_packet) {
	PoolByteArray c = _Marshalls::get_singleton()->base64_to_raw(_Marshalls::get_singleton()->variant_to_base64(p_packet));
	PoolVector<uint8_t>::Read r = c.read();
	return put_packet(r.ptr(), c.size());
}

void NetworkedMultiplayerYojimbo::set_refuse_new_connections(bool p_enable) {
}

bool NetworkedMultiplayerYojimbo::is_refusing_new_connections() const {
	return false;
}

int32_t NetworkedMultiplayerYojimbo::get_max_packet_size() const {
	return int32_t();
}

NetworkedMultiplayerPeer::TransferMode NetworkedMultiplayerYojimbo::get_transfer_mode() const {
	return TransferMode();
}

bool NetworkedMultiplayerYojimbo::is_server() const {
	return false;
}

void NetworkedMultiplayerYojimbo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_server", "port", "max_clients", "in_bandwidth", "out_bandwidth"), &NetworkedMultiplayerYojimbo::create_server, DEFVAL(40000), DEFVAL(32), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("create_client", "ip", "port", "in_bandwidth", "out_bandwidth"), &NetworkedMultiplayerYojimbo::create_client, DEFVAL("127.0.0.1"), DEFVAL(40000), DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("close_connection"), &NetworkedMultiplayerYojimbo::close_connection);
	//ClassDB::bind_method(D_METHOD("set_compression_mode", "mode"), &NetworkedMultiplayerYojimbo::set_compression_mode);
	//ClassDB::bind_method(D_METHOD("get_compression_mode"), &NetworkedMultiplayerYojimbo::get_compression_mode);
	ClassDB::bind_method(D_METHOD("set_bind_ip", "ip"), &NetworkedMultiplayerYojimbo::set_bind_ip);
	ClassDB::bind_method(D_METHOD("set_log_level"), &NetworkedMultiplayerYojimbo::set_log_level);
}

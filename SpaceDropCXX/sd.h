#pragma once

#include <iostream>
#include <string>
#include <regex>

#pragma comment(lib, "Ws2_32.lib")

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_RANDOM_DEVICE_
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <json.hpp> 

#ifdef _WIN32
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define PROGRESS_INDICATOR "============================================================"
#define PROGRESS_WIDTH 60

#define SD_URL "http://vldr.org/ws/worker.html"
#define SD_SERVER "ws://vldr.org/sd"

using nlohmann::json;

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
 
void join_room(client * c, websocketpp::connection_hdl hdl, std::string pin = std::string());
void create_room(client * c, websocketpp::connection_hdl hdl);


const size_t BUFFERING_LIMIT = 100000000;
const int BLOCK_SIZE = 65527;
const int BLOCK_SIZE_UPDATE = BLOCK_SIZE * 4;

enum Operation {
	ERROR_OCCURRED = 0,
	CREATE_ROOM = 1,
	JOIN_ROOM = 2,
	SEND_PIN = 3,
	UPDATE_ROOM = 4,

	FILE_INFO = 5,
	READY_FOR_TRANSFER = 6,
	TRANSFER_STATUS = 7
};
 
enum State {
	S_IDLE = 0,
	S_UPLOADING = 1,
	S_DOWNLOADING = 2
};

struct SendState {
	size_t max_size = 0;
	size_t size = 0;

	size_t block_size = BLOCK_SIZE;
	unsigned char buffer[BLOCK_SIZE];

	long progress = 0;

	bool should_be_flushed = false;
};

State state = S_IDLE;

size_t bytes_written = 0;

bool is_transmitting = false;

long file_size = 0;

int file_queue = -1;

SendState send_state = SendState();

json file_info;

std::string room_id = std::string();

FILE * shared_file = nullptr;



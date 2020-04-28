#include "sd.h"


void reset()
{
	bytes_written = 0;
	file_queue = -1;
	is_transmitting = false;
}

void display_error(std::string error)
{
	printf("Error occurred: %s\n", error.c_str());
	printf("Exiting program\n");
	exit(0);
}


void handle_room_created(json & message)
{
	room_id = message["id"];

	/////////////////////////////////////////////////

#if defined(_WIN32)
	system("cls");
#endif

	std::string file_name = file_info["name"];
	long file_size = file_info["size"];

	printf("Room created - #%s\n", room_id.c_str());
	printf("File Name - %s\n", file_name.c_str());
	printf("File Size - %lu\n", file_size);
	printf("__________________________\n");
}

void handle_error_occurred(json & message) 
{
	std::string error_message = message["msg"];

	display_error(message["msg"]);
}


void send_file_info(client * c, websocketpp::connection_hdl hdl)
{
	json file_info_obj;
	file_info_obj["status"] = FILE_INFO;
	file_info_obj["fileInfo"] = file_info;

	try
	{
		// Serialize our json object.
		auto string = file_info_obj.dump();

		// Debug our object that
		printf("Sending file info.\n", string.c_str());

		// Send our serialized string.
		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	// Catch any json exceptions.
	catch (json::exception const & e)
	{
		display_error(std::string("Error in send_file_info (json::exception), ") + e.what());
	}
	// Catch any websocketpp exceptions.
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in send_file_info (websocketpp::exception), ") + e.what());
	}
}

void handle_ready_for_transfer(client * c, websocketpp::connection_hdl hdl, json & message)
{
	if (!shared_file)
	{
		display_error("Invalid file handle in handle_ready_for_transfer");
		return;
	}

	/////////////////////////////////////////////////////

	printf("Starting transfer\n");

	/////////////////////////////////////////////////////
	
	size_t size = file_info["size"];
	size_t block_size = BLOCK_SIZE;
	unsigned char buffer[BLOCK_SIZE];

	rewind(shared_file); 

	while (size != 0)
	{
		if (size < block_size)
			block_size = size;

		size_t bytes_read = fread(buffer, sizeof(unsigned char), block_size, shared_file);

		c->send(hdl, buffer, bytes_read, websocketpp::frame::opcode::value::binary);
		c->poll();

		size -= bytes_read;
	}
}

void handle_update_room(client * c, websocketpp::connection_hdl hdl, json & message)
{
	is_transmitting = message["isTransmitting"];

	///////////////////////////////////////////////////

	if (!is_transmitting)
	{
		printf("Client has left the room\n");
		return;
	}

	///////////////////////////////////////////////////

	if (state == S_UPLOADING)
	{
		printf("Client has conncted to the room.\n");

		send_file_info(c, hdl);
	}

	if (state == S_IDLE)
	{
		// TODO: implement this.
		//setStatus("Successfully joined the room.");

		//postMessage({ status: R_JOINED_ROOM });
	}
}

void on_open(client * c, websocketpp::connection_hdl hdl) 
{
	reset();

	if (state == S_UPLOADING)
	{
		printf("Connected as sender.\n");

		//////////////////////////////////////////////
	
		create_room(c, hdl);
	}
	else
	{
		printf("Connected as receiver.\n");
	}

}

void create_room(client * c, websocketpp::connection_hdl hdl)
{
	char room_pin[128] = { 0 };
	printf("Enter a PIN for the room (leave blank if you don't care): ");
	scanf_s("%[^\n]s", room_pin, sizeof room_pin);

#if defined(_WIN32)
	system("cls");
#endif

	//////////////////////////////////////////////

	json create_room_obj;
	create_room_obj["status"] = CREATE_ROOM;
	create_room_obj["pin"] = room_pin;

	//////////////////////////////////////////////

	try
	{
		// Serialize our json object.
		auto string = create_room_obj.dump();

		printf("Sending create room.\n", string.c_str());

		// Send our serialized string.
		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	// Catch any json exceptions.
	catch (json::exception const & e)
	{
		display_error(std::string("Error in create_room (json::exception), ") + e.what());
	}
	// Catch any websocketpp exceptions.
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in create_room (websocketpp::exception), ") + e.what());
	}
}

void on_message(client * c, websocketpp::connection_hdl hdl, message_ptr msg)
{
	// Process binary operations.
	if (msg->get_opcode() == websocketpp::frame::opcode::value::binary)
	{

	}
	// Process text operations
	else if (msg->get_opcode() == websocketpp::frame::opcode::value::text)
	{
		try
		{
			// Parse our message.
			auto message = json::parse(msg->get_payload());

			// Get our status code.
			Operation status = message["status"];

			switch (status)
			{
			case ERROR_OCCURRED:
			{
				handle_error_occurred(message);
				break;
			}
			case CREATE_ROOM:
			{
				handle_room_created(message);
				break;
			}
			case UPDATE_ROOM:
			{
				handle_update_room(c, hdl, message);
				break;
			}
			case READY_FOR_TRANSFER:
			{
				handle_ready_for_transfer(c, hdl, message);
				break;
			}
			}

		}
		catch (json::exception const & e)
		{
			display_error(std::string("Error in on_message, ") + e.what());
		}

	}


	/*std::cout << "on_message called with hdl: " << hdl.lock().get()
		<< " and message: " << msg->get_payload()
		<< std::endl;*/


	/*websocketpp::lib::error_code ec;

	c->send(hdl, msg->get_payload(), msg->get_opcode(), ec);
	if (ec) {
		std::cout << "Echo failed because: " << ec.message() << std::endl;
	}*/
}

std::string base_name(std::string const & path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

int main(int argc, char* argv[]) 
{
	//////////////////////////////////////////////

#ifdef _DEBUG
	std::string command = "-c";
	std::string parameter = "C:\\Users\\vlad\\Desktop\\bigblack.mp4";
#else
	if (argc != 3)
	{
		printf("Not enough arguments provided.\n");
		return 0;
	}


	std::string command = argv[1];
	std::string parameter = argv[2];
#endif

	// Handle the connect command.
	if (command == "-c" || command == "-connect")
	{
		auto err = fopen_s(&shared_file, parameter.c_str(), "rb");

		if (err)
		{
			printf("Invalid file path provided.\n");
			return 0;
		}

		state = S_UPLOADING;

		///////////////////////////////////////////

		int fd = _fileno(shared_file);

		if (fd < 0)
		{
			display_error("Unable to obtain file descriptor.");
			return 0;
		}

		////////////////////////////////////////////

		struct stat file_stat;
		if (fstat(fd, &file_stat) < 0)
		{
			display_error("Unable to obtain stat on file.");
			return 0;
		}

		////////////////////////////////////////////

		file_info.clear();
		file_info["name"] = base_name(parameter);
		file_info["size"] = file_stat.st_size;
		file_info["queue"] = -1;

	}
	// Handle the join command.
	else if (command == "-j" || command == "-join")
	{

		// Set our room id with the parameter provided.
		room_id = parameter;
	}
	else
	{
		printf("Invalid command provided.\n");
	}

	//////////////////////////////////////////////

	client c;

	//////////////////////////////////////////////

	std::string uri = "ws://my.vldr.org/sd";

	//////////////////////////////////////////////

	try 
	{
		c.set_access_channels(websocketpp::log::alevel::none);
		c.clear_access_channels(websocketpp::log::alevel::none);

		c.init_asio();

		c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));
		c.set_open_handler(bind(&on_open, &c, ::_1));

		websocketpp::lib::error_code ec;
		client::connection_ptr con = c.get_connection(uri, ec);

		if (ec) 
		{
			printf("Could not create connection because: %s\n", ec.message().c_str());
			return 0;
		}

		c.connect(con);
		c.run();
	}
	catch (websocketpp::exception const & e) 
	{
		std::cout << e.what() << std::endl;
	}
}
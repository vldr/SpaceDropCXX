#include "sd.h"

/*
* Displays an error and exits the program.
*/
void display_error(std::string error)
{
	printf("Error occurred: %s\n", error.c_str());
	printf("Exiting program\n");
	exit(0);
}

/*
* Handles with the creation of a room.
*/
void handle_room_created(json & message)
{
	room_id = message["id"];

	/////////////////////////////////////////////////

#if defined(_WIN32)
	system("cls");
#endif

	std::string file_name = file_info["name"];

	printf("Room created - #%s\n", room_id.c_str());
	printf("File Name - %s\n", file_name.c_str());
	printf("File Size - %lu\n", file_size);
	printf("__________________________\n");
}

/*
* Handles any error that may occur.
*/
void handle_error_occurred(json & message) 
{
	std::string error_message = message["msg"];

	display_error(message["msg"]);
}

/*
* Updates the sender of the download progress.
*/
void update_transfer_status(client * c, websocketpp::connection_hdl hdl)
{
	json transfer_status_obj;
	transfer_status_obj["status"] = TRANSFER_STATUS;
	transfer_status_obj["progress"] = bytes_written;

	try
	{
		auto string = transfer_status_obj.dump();

		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	catch (json::exception const & e)
	{
		display_error(std::string("Error in send_file_info (json::exception), ") + e.what());
	}
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in send_file_info (websocketpp::exception), ") + e.what());
	}
}

/*
* Handles sending file information to the other client.
*/
void send_file_info(client * c, websocketpp::connection_hdl hdl)
{
	json file_info_obj;
	file_info_obj["status"] = FILE_INFO;
	file_info_obj["fileInfo"] = file_info;

	try
	{
		auto string = file_info_obj.dump();

		printf("Sending file info.\n");

		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	catch (json::exception const & e)
	{
		display_error(std::string("Error in send_file_info (json::exception), ") + e.what());
	}
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in send_file_info (websocketpp::exception), ") + e.what());
	}
}

/*
* Handles sending the pin for the room inorder to enter it.
*/
void handle_send_pin(client * c, websocketpp::connection_hdl hdl)
{
	char room_pin[128] = { 0 };
	printf("This room requires a PIN: ");

	scanf("%127[^\n]s", room_pin);

	//////////////////////////////////////////

	join_room(c, hdl, room_pin);
}

/*
* Handles sending the file contents to the client.
*/
void handle_ready_for_transfer(client * c, websocketpp::connection_hdl hdl, json & message)
{
	if (!shared_file)
	{
		display_error("Invalid file handle in handle_ready_for_transfer");
		return;
	}

	/////////////////////////////////////////////////////

	printf("Starting transfer.\n");

	/////////////////////////////////////////////////////
	
	const size_t max_size = file_info["size"];
	size_t size = file_info["size"];
	size_t block_size = BLOCK_SIZE;
	unsigned char buffer[BLOCK_SIZE];

	/////////////////////////////////////////////////////

	rewind(shared_file);  

	/////////////////////////////////////////////////////

	auto con = c->get_con_from_hdl(hdl);

	while (size != 0)
	{  
		if (size < block_size)
			block_size = size;

		size_t bytes_read = fread(buffer, sizeof(unsigned char), block_size, shared_file);

		c->send(hdl, buffer, bytes_read, websocketpp::frame::opcode::value::binary);
		c->poll();

		size -= bytes_read;  

		/////////////////////////////////////

		if (con->get_buffered_amount() >= BUFFERING_LIMIT) 
		{
			c->interrupt(hdl);
		}
	} 
} 

/*
* Handles recieving file information.
*/
void handle_file_info(client * c, websocketpp::connection_hdl hdl, json & message)
{
#if defined(_WIN32)
	system("cls");
#endif

	std::string file_name = message["fileInfo"]["name"];

	//////////////////////////////////////

	// Check if the file name matches our regex.
	if (!std::regex_match(file_name, std::regex("[0-9a-zA-Z-._() ]+")))
	{
		display_error("Improper file name.");
		return;
	}

	//////////////////////////////////////

	file_info = message["fileInfo"];
	file_size = file_info["size"];

	//////////////////////////////////////

	printf("Room - #%s\n", room_id.c_str());
	printf("File Name - %s\n", file_name.c_str());
	printf("File Size - %lu\n", file_size);
	printf("__________________________\n");

	//////////////////////////////////////

	// Close our file handle if it is open.
	if (shared_file)
	{
		fclose(shared_file);
	}

	//////////////////////////////////////

	shared_file = fopen(file_name.c_str(), "w+b");

	if (!shared_file)
	{
		display_error("Unable to open/create file, file may already exist.");
		return;
	}

	//////////////////////////////////////

	rewind(shared_file);

	//////////////////////////////////////

	state = S_DOWNLOADING;
	bytes_written = 0;

	//////////////////////////////////////

	json ready_for_transfer_obj;
	ready_for_transfer_obj["status"] = READY_FOR_TRANSFER;

	//////////////////////////////////////

	try
	{
		// Serialize our json object.
		auto string = ready_for_transfer_obj.dump();

		printf("Sending ready for transfer.\n");

		// Send our serialized string.
		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	catch (json::exception const & e)
	{
		display_error(std::string("Error in handle_file_info (json::exception), ") + e.what());
	}
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in handle_file_info (websocketpp::exception), ") + e.what());
	}
}

/*
* Handles whenever a client joins or leaves the room.
*/
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
		printf("Successfully joined the room.\n");
	}
}

/*
* Send a JOIN_ROOM signal inorder to join a room.
*/
void join_room(client * c, websocketpp::connection_hdl hdl, std::string pin)
{
	//////////////////////////////////////////////

	json join_room_obj;
	join_room_obj["status"] = JOIN_ROOM;
	join_room_obj["id"] = room_id;
	join_room_obj["pin"] = pin;

	//////////////////////////////////////////////

	try
	{
		// Serialize our json object.
		auto string = join_room_obj.dump();

		printf("Sending join room.\n");

		// Send our serialized string.
		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	catch (json::exception const & e)
	{
		display_error(std::string("Error in join_room (json::exception), ") + e.what());
	}
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in join_room (websocketpp::exception), ") + e.what());
	}
}

/*
* Attempts to create a new room.
*/
void create_room(client * c, websocketpp::connection_hdl hdl)
{
	char room_pin[128] = { 0 };
	printf("Enter a PIN for the room (leave blank if you don't care): ");
	scanf("%127[^\n]s", room_pin);

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
		auto string = create_room_obj.dump();

		printf("Sending create room.\n");

		c->send(hdl, string, websocketpp::frame::opcode::value::text);
	}
	catch (json::exception const & e)
	{
		display_error(std::string("Error in create_room (json::exception), ") + e.what());
	}
	catch (websocketpp::exception const & e)
	{
		display_error(std::string("Error in create_room (websocketpp::exception), ") + e.what());
	}
}

//////////////////////////////////////////////

void on_interrupt(client * c, websocketpp::connection_hdl hdl)
{
	auto con = c->get_con_from_hdl(hdl); 

	if (con->get_buffered_amount() == 0)
	{
		return;
	}

	c->interrupt(hdl);
}


void on_open(client * c, websocketpp::connection_hdl hdl) 
{
	/////////////////////////////////////

	if (state == S_UPLOADING)
	{
		printf("Connected as sender.\n");

		//////////////////////////////////////////////
	
		create_room(c, hdl);
	}
	else
	{
		printf("Connected as receiver, attempting to join room.\n");

		//////////////////////////////////////////////

		join_room(c, hdl);
	}
} 

void on_message(client * c, websocketpp::connection_hdl hdl, message_ptr msg)
{
	// The following conditions must be met:
	// - We are being sent binary data.
	// - We are in the downloading state.
	// - We have a valid file handle. (a valid pointer)
	if (
		msg->get_opcode() == websocketpp::frame::opcode::value::binary 
		&& state == S_DOWNLOADING
		&& shared_file
	)
	{
		auto payload = msg->get_payload(); 
		 
		bytes_written += fwrite(
			(const void*)payload.data(),
			sizeof(unsigned char),
			payload.size(),
			shared_file
		); 
		 
		/////////////////////////////////

		if (bytes_written % BLOCK_SIZE_UPDATE == 0)
		{
			update_transfer_status(c, hdl);
		}

		/////////////////////////////////

		if (bytes_written == file_size)
		{
			printf("Transfer completed.\n");

			state = S_IDLE;

			///////////////////////////////////////////

			fclose(shared_file);
			shared_file = nullptr;

			///////////////////////////////////////////
			
			update_transfer_status(c, hdl);
		}
	}
	// Process text operations.
	else if (msg->get_opcode() == websocketpp::frame::opcode::value::text)
	{
		try
		{
			auto message = json::parse(msg->get_payload());
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
			case FILE_INFO:
			{
				handle_file_info(c, hdl, message);
				break;
			}
			case READY_FOR_TRANSFER:
			{
				handle_ready_for_transfer(c, hdl, message);
				break;
			}
			case SEND_PIN:
			{
				handle_send_pin(c, hdl);
				break;
			}
			case TRANSFER_STATUS:
			{
				long progress = message["progress"];

				if (progress == file_size)
				{
					printf("File transfer completed.\n");

				}

				break;
			}
			}
		}
		catch (json::exception const & e)
		{
			display_error(std::string("Error in on_message, ") + e.what());
		}
	}
}
 
//////////////////////////////////////////////

std::string base_name(std::string const & path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

int main(int argc, char* argv[]) 
{
	//////////////////////////////////////////////

#ifdef _DEBUG
	std::string command = "-c";
	std::string parameter = "C:\\Users\\vlad\\Downloads\\tut.mp4";
	
	//std::string command = "-j";
	//std::string parameter = "06og";
#else
	if (argc != 3)
	{
		printf("Not enough arguments provided.\n");
		return 0;
	}


	std::string command = argv[1];
	std::string parameter = argv[2];
#endif 

	// Handle the create command.
	if (command == "-c" || command == "-create")
	{
		shared_file = fopen(parameter.c_str(), "rb");

		if (!shared_file)
		{
			printf("Invalid file path provided.\n");
			return 0;
		}

		state = S_UPLOADING;

		///////////////////////////////////////////

#ifdef _WIN32
		int fd = _fileno(shared_file);
#else
		int fd = fileno(shared_file);
#endif

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

		file_size = file_stat.st_size;;

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

	std::string uri = "ws://vldr.org/sd";

	//////////////////////////////////////////////

	try 
	{
		c.set_access_channels(websocketpp::log::alevel::none);
		c.clear_access_channels(websocketpp::log::alevel::none);

		c.init_asio();

		c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));
		c.set_open_handler(bind(&on_open, &c, ::_1));
		c.set_interrupt_handler(bind(&on_interrupt, &c, ::_1));

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
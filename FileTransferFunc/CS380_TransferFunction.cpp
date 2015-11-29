
#include "stdafx.h"
#include <iostream>
#include <iomanip>
//#include <fstream>
//#include <sstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iterator>								// For reading directory
#include <vector>								// For reading directory
#include <boost/filesystem.hpp>
//#include <boost/bind.hpp>

using boost::asio::ip::tcp;
const std::string port = "1234";					// Default port
const std::string uploadPath = "../data/";			// Location of file to upload
const std::string downloadPath = "../downloads/";	// Location to put downloaded file
const int chunk_size = 9999;						// Chunk size used. 9999Bytes


/*
This is for reading inside the directory to find what available files can be used.
Leave this commented out in case we need it in the future

std::string workingDir() {					
	std::string message;
	boost::system::error_code ignored_error;
	boost::filesystem::path path(uploadPath);
	try {
		if (boost::filesystem::exists(path)) {
			if (boost::filesystem::is_regular_file(path)) {
				std::cout << path << " , size: " << boost::filesystem::file_size(path) << std::endl;
			}
			else if (boost::filesystem::is_directory(path)) {
				//std::cout << "  Contents of " << path << std::endl;
				message = "  Contents of " + path.string() + "\n";

				std::vector<std::string> v;
				for (auto&& x : boost::filesystem::directory_iterator(path)) 
				{
					v.push_back(x.path().filename().string());
				}
				std::sort(v.begin(), v.end());
				for (auto&& x : v) {
					//std::cout << "    " << x << '\n';
					message += "    " + x + "\n";
				}
			}
			else {
				std::cout << path << " exists, but is neither a regular file nor a directory\n";
			}
		}
		else {
			std::cout << path << " does not exist\n";
		}
	}
	catch (boost::filesystem::filesystem_error& ex) {
		std::cout << ex.what() << std::endl;
	}
	return message;
}
*/

// Server sends file...
void startServer() {
	std::cout << "SERVER RUNNING... listening on port " << port << ".\n";

	boost::asio::io_service io_service;
	//tcp::resolver resolver(io_service);
	tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), stoi(port)));
	boost::system::error_code error;
	tcp::socket socket(io_service);
	acceptor.accept(socket);

	//This is using the above function. Not needed.
	//boost::asio::write(socket, boost::asio::buffer(workingDir()));

	std::cout << "\nA client has connected.\n";


	boost::asio::streambuf request_buf;

	boost::asio::read_until(socket, request_buf, "\n");							// Read until newline char
	std::istream request_fileName(&request_buf);								// Read in requested filename
	std::string fileName;
	request_fileName >> fileName;
	boost::array<char, chunk_size> buf;
	request_fileName.read(buf.c_array(), 1);									// Eat "\n"


																				// Get file
	boost::asio::read_until(socket, request_buf, "\n\n");
	std::istream in_stream(&request_buf);
	std::string path;
	size_t fileSize = 0;
	in_stream >> path;
	in_stream >> fileSize;
	in_stream.read(buf.c_array(), 2);							// Eat "\n\n"

																//std::cout << "\nfile: " << path << " size: " << fileSize << std::endl;
	std::cout << "\nfile: " << fileName << " size: " << fileSize << " bytes\n" << std::endl;
	size_t pos = path.find_last_of("/");
	if (pos != std::string::npos)
		path = downloadPath + path.substr(pos + 1);
	std::ofstream outputFile(path.c_str(), std::ios_base::binary);
	if (!outputFile)
	{
		std::cout << "Failed to open " << path << std::endl;
	}

	// Purpose is to write extra bytes, possibly omit?
	do {
		in_stream.read(buf.c_array(), (std::streamsize)buf.size());
		std::cout << __FUNCTION__ << " write " << in_stream.gcount() << " bytes.\n";
		outputFile.write(buf.c_array(), in_stream.gcount());
	} while (in_stream.gcount()>0);

	std::cout << std::endl;


	//Chunk info.
	int currSize = 0;
	int count = 0;

	// Writing the file
	while (true) {
		size_t len = socket.read_some(boost::asio::buffer(buf), error);

		//Print out Chunk info.
		currSize += len;
		count++;
		std::cout << "Receiving Chunk #" << std::setw(5) << std::left << count << ": ";
		std::cout << "Length: " << std::setw(10) << std::left << len << " ";
		std::cout << "Cur Size: " << std::setw(10) << std::left << currSize << std::endl;
		


		if (len>0)
			outputFile.write(buf.c_array(), (std::streamsize)len);
		if (outputFile.tellp() == (std::fstream::pos_type)(std::streamsize)fileSize)
			break; // file was received
		if (error)
		{
			std::cout << error << std::endl;
			break;
		}
	}
	std::cout << "Received " << outputFile.tellp() << " bytes.\n" << std::endl;

}

/*
void login() {

if () {
startClient();
}
else {
std::cout << "Login failed, exiting program...";
exit(1); }
}
*/

// Client receives file...
void startClient(std::string serverIP) {
	boost::asio::io_service io_service;
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(serverIP, port);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;
	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	// Magical
	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}

	std::cout << "Connected to " << serverIP << ":" << port << std::endl;
	boost::array<char, chunk_size> buf;
	boost::asio::streambuf request_buf;

	/*
	This waits for the server to send a message to continue...
	Used because the server sends back the folder contents, but I removed it so we don't need it anymore.

	size_t len = socket.read_some(boost::asio::buffer(buf), error);
	std::cout.write(buf.data(), len);
	*/


	//Selecting a file to upload
	std::cout << "\nSelect a file: ";
	std::string fileName;
	std::getline(std::cin, fileName);
	std::ostream out_stream(&request_buf);						// Send filename for request
	out_stream << fileName << "\n";
	boost::asio::write(socket, request_buf);


	std::string filePath = uploadPath + fileName;								// Concatenate path and filename

	std::ifstream source(filePath, std::ios_base::binary);						// Read in file
	if (!source) {
		std::cout << "Failed to open.\n" << filePath << std::endl;
	}
	size_t fileSize = boost::filesystem::file_size(filePath);					// Get file size
																				//boost::asio::streambuf request;
	std::ostream request_stream(&request_buf);
	request_stream << filePath << "\n" << fileSize << "\n\n";					// Send filename and size
	boost::asio::write(socket, request_buf);

	//Sending file
	std::cout << "Sending file...\n";

	//Chunk info counting.
	int currSize = 0;
	int count = 0;

	while (true) {
		if (source.eof() == false) {
			source.read(buf.c_array(), (std::streamsize)buf.size());
			if (source.gcount() <= 0) {
				std::cout << "Read error.\n";
			}
			boost::asio::write(socket,
				boost::asio::buffer(buf.c_array(), source.gcount()),
				boost::asio::transfer_all(), error);

			//Print out Chunk info.
			currSize += source.gcount();
			count++;
			std::cout << "Sending Chunk #" << std::setw(5) << std::left << count << ": ";
			std::cout << "Length: " << std::setw(10) << std::left << source.gcount() << " ";
			std::cout << "Cur Size: " << std::setw(10) << std::left << currSize << std::endl;



			if (error) {
				std::cout << "Error: " << error << std::endl;
			}
		}
		else { break; }
	}
	std::cout << "Task complete. " << fileName << " sent.\n" << std::endl;
}

int _tmain(int argc, _TCHAR* argv[])
{
	std::cout << "\nWelcome to the File Transfer Program.\n" << std::endl;

	bool run = true;
	while (run) {
		std::cout << "1) Start Server\n2) Start Client\n3) Exit Program." << std::endl;
		std::cout << "Please select: ";

		std::string input = "";
		std::getline(std::cin, input);
		std::cout << std::endl;
		if (input[0] == '1') {
			startServer();
		}
		else if (input[0] == '2') {
			//login();
			std::cout << "Enter IP address of server: ";
			std::getline(std::cin, input);
			startClient(input);
		}
		else if (input[0] == '3') {
			std::cout << "Program exited. Goodbye.";
			run = false;
		}
		else {
			std::cout << "Invalid input.\n" << std::endl;
		}
	}
	return 0;
}
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include <iostream>
using namespace std;
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <string.h>
#include <string>
#include <time.h>
#include <fstream>
#include <sstream>
#include <fstream>
#include <ios>
#pragma warning(disable : 4996)

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[128];
	char sendBuff[255];
	int statusCode;
	int method;
	int len;
};

const int TIME_PORT = 27015;
const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_TIME = 1;
const int SEND_SECONDS = 2;

bool addSocket(SOCKET id, int what);
void removeSocket(int index);
void acceptConnection(int index);
void receiveMessage(int index);
void sendMessage(int index);
int requestMethod(string method);
bool validation(string fileName, string fileSuffix, string language);
void splitRequest(string& method, string& resource, int index);
void handleHeadAndGet(int index, int reqMethod);
string getStatusLine(int index);
void getFileNameAndSuffix(string& fileName, string& suffix);


struct SocketState sockets[MAX_SOCKETS] = { 0 };
int socketsCount = 0;

string requestBuffer, method, resource;

#define GET 100
#define TRACE 101
#define DELETE 102
#define PUT 103
#define POST 104
#define HEAD 105
#define OPTIONS 106
#define NOTFOUND 404
#define BADREQUEST 400
#define OK 200


void main()
{
	// Initialize Winsock (Windows Sockets).

	// Create a WSADATA object called wsaData.
	// The WSADATA structure contains information about the Windows 
	// Sockets implementation.
	WSAData wsaData;

	// Call WSAStartup and return its value as an integer and check for errors.
	// The WSAStartup function initiates the use of WS2_32.DLL by a process.
	// First parameter is the version number 2.2.
	// The WSACleanup function destructs the use of WS2_32.DLL by a process.
	if (NO_ERROR != WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "Time Server: Error at WSAStartup()\n";
		return;
	}

	// Server side:
	// Create and bind a socket to an internet address.
	// Listen through the socket for incoming connections.

	// After initialization, a SOCKET object is ready to be instantiated.

	// Create a SOCKET object called listenSocket. 
	// For this application:	use the Internet address family (AF_INET), 
	//							streaming sockets (SOCK_STREAM), 
	//							and the TCP/IP protocol (IPPROTO_TCP).
	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	// Error detection is a key part of successful networking code. 
	// If the socket call fails, it returns INVALID_SOCKET. 
	// The if statement in the previous code is used to catch any errors that
	// may have occurred while creating the socket. WSAGetLastError returns an 
	// error number associated with the last error that occurred.
	if (INVALID_SOCKET == listenSocket)
	{
		cout << "Time Server: Error at socket(): " << WSAGetLastError() << endl;
		WSACleanup();
		return;
	}

	// For a server to communicate on a network, it must bind the socket to 
	// a network address.

	// Need to assemble the required data for connection in sockaddr structure.

	// Create a sockaddr_in object called serverService. 
	sockaddr_in serverService;
	// Address family (must be AF_INET - Internet address family).
	serverService.sin_family = AF_INET;
	// IP address. The sin_addr is a union (s_addr is a unsigned long 
	// (4 bytes) data type).
	// inet_addr (Iternet address) is used to convert a string (char *) 
	// into unsigned long.
	// The IP address is INADDR_ANY to accept connections on all interfaces.
	serverService.sin_addr.s_addr = INADDR_ANY;
	// IP Port. The htons (host to network - short) function converts an
	// unsigned short from host to TCP/IP network byte order 
	// (which is big-endian).
	serverService.sin_port = htons(TIME_PORT);

	// Bind the socket for client's requests.

	// The bind function establishes a connection to a specified socket.
	// The function uses the socket handler, the sockaddr structure (which
	// defines properties of the desired connection) and the length of the
	// sockaddr structure (in bytes).
	if (SOCKET_ERROR == bind(listenSocket, (SOCKADDR*)&serverService, sizeof(serverService)))
	{
		cout << "Time Server: Error at bind(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}

	// Listen on the Socket for incoming connections.
	// This socket accepts only one connection (no pending connections 
	// from other clients). This sets the backlog parameter.
	if (SOCKET_ERROR == listen(listenSocket, 5))
	{
		cout << "Time Server: Error at listen(): " << WSAGetLastError() << endl;
		closesocket(listenSocket);
		WSACleanup();
		return;
	}
	addSocket(listenSocket, LISTEN);

	// Accept connections and handles them one by one.
	while (true)
	{
		// The select function determines the status of one or more sockets,
		// waiting if necessary, to perform asynchronous I/O. Use fd_sets for
		// sets of handles for reading, writing and exceptions. select gets "timeout" for waiting
		// and still performing other operations (Use NULL for blocking). Finally,
		// select returns the number of descriptors which are ready for use (use FD_ISSET
		// macro to check which descriptor in each set is ready to be used).
		fd_set waitRecv;
		FD_ZERO(&waitRecv);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if ((sockets[i].recv == LISTEN) || (sockets[i].recv == RECEIVE) && (sockets[i].send != SEND))
				FD_SET(sockets[i].id, &waitRecv);
		}

		fd_set waitSend;
		FD_ZERO(&waitSend);
		for (int i = 0; i < MAX_SOCKETS; i++)
		{
			if (sockets[i].send == SEND)
				FD_SET(sockets[i].id, &waitSend);
		}

		//
		// Wait for interesting event.
		// Note: First argument is ignored. The fourth is for exceptions.
		// And as written above the last is a timeout, hence we are blocked if nothing happens.
		struct timeval timeToExpire;
		timeToExpire.tv_sec = 120;
		timeToExpire.tv_usec = 0;

		int nfd;
		nfd = select(0, &waitRecv, &waitSend, NULL, &timeToExpire);
		if (nfd == SOCKET_ERROR)
		{
			cout << "Time Server: Error at select(): " << WSAGetLastError() << endl;
			WSACleanup();
			return;
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitRecv))
			{
				nfd--;
				switch (sockets[i].recv)
				{
				case LISTEN:
					acceptConnection(i);
					break;

				case RECEIVE:
					receiveMessage(i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_SOCKETS && nfd > 0; i++)
		{
			if (FD_ISSET(sockets[i].id, &waitSend))
			{
				nfd--;
				switch (sockets[i].send)
				{
				case SEND:
					sendMessage(i);
					break;
				}
			}
		}
	}

	// Closing connections and Winsock.
	cout << "Time Server: Closing Connection.\n";
	closesocket(listenSocket);
	WSACleanup();
}

bool addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			socketsCount++;
			return (true);
		}
	}
	return (false);
}

void removeSocket(int index)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void acceptConnection(int index)
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "Time Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "Time Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	//
	// Set the socket to be in non-blocking mode.
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "Time Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "Time Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "Time Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";

		sockets[index].len += bytesRecv;
		bool isValid;
		string fileName, fileSuffix, language;

		if (sockets[index].len > 0)
		{
			splitRequest(method, resource, index);
			sockets[index].send = SEND;

			sockets[index].method = requestMethod(method);


			//if (strncmp(sockets[index].buffer, "TimeString", 10) == 0)
			//{
			//	sockets[index].send = SEND;
			//	sockets[index].sendSubType = SEND_TIME;
			//	memcpy(sockets[index].buffer, &sockets[index].buffer[10], sockets[index].len - 10);
			//	sockets[index].len -= 10;
			//	return;
			//}
			//else if (strncmp(sockets[index].buffer, "SecondsSince1970", 16) == 0)
			//{
			//	sockets[index].send = SEND;
			//	sockets[index].sendSubType = SEND_SECONDS;
			//	memcpy(sockets[index].buffer, &sockets[index].buffer[16], sockets[index].len - 16);
			//	sockets[index].len -= 16;
			//	return;
			//}
			//else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
			//{
			//	closesocket(msgSocket);
			//	removeSocket(index);
			//	return;
			//}
		}
	}

}

void sendMessage(int index)
{
	int bytesSent = 0;
	char sendBuff[255];
	int reqMethod = sockets[index].method;
	SOCKET msgSocket = sockets[index].id;
	FILE * file;
	string response;

	string statusLine, nowTime, contentType, contentLen, body;

	time_t timer;
	time(&timer);
	

	//if (sockets[index].sendSubType == SEND_TIME)
	//{
	//	// Answer client's request by the current time string.

	//	// Get the current time.
	//	time_t timer;
	//	time(&timer);
	//	// Parse the current time to printable string.
	//	strcpy(sendBuff, ctime(&timer));
	//	sendBuff[strlen(sendBuff) - 1] = 0; //to remove the new-line from the created string
	//}
	//else if (sockets[index].sendSubType == SEND_SECONDS)
	//{
	//	// Answer client's request by the current time in seconds.

	//	// Get the current time.
	//	time_t timer;
	//	time(&timer);
	//	// Convert the number to string.
	//	itoa((int)timer, sendBuff, 10);
	//}

	switch (reqMethod)
	{
	case GET:
	case HEAD:
		handleHeadAndGet(index, reqMethod);
		/*statusLine = getStatusLine(index);
		response << statusLine;
		response << "Content-Type: "<< contentType << "\r\n";
		response << "\r\n";*/
		//response << "test";//////////change
		statusLine = getStatusLine(index);
		//response = "HTTP/1.1 200 OK\r\n";
		response = statusLine;

		if (reqMethod == GET)
		{
			contentLen = "13";
			response+= "Content-Length: 13\r\n";
		}
		response += "Content-Type: text/html\r\n";
		//response += "\r\n";

		//response = "HTTP/1.1 200 OK\r\n";
		//response += "Content-Length: 13\r\n";
		//response += "Content-Type: text/plain\r\n";
		response += "\r\n";
		response += "Hello, world!";

	case TRACE:

	case DELETE:

	case PUT:

	case POST:

	case OPTIONS:

	default:
		break;
	}

	strcpy(sendBuff, (response).c_str());
	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "Time Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "Time Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \"" << sendBuff << "\" message.\n";

	sockets[index].send = IDLE;
}

string getStatusLine(int index)
{
	if (sockets[index].statusCode == OK)
		return "HTTP/1.1 200 OK\r\n";
	if (sockets[index].statusCode == BADREQUEST)
		return "HTTP/1.1 400 Bad Request\r\n";
	if (sockets[index].statusCode == NOTFOUND)
		return "HTTP/1.1 404 Not Found\r\n";
}

int requestMethod(string method)
{
	if (method == "GET")
		return GET;
	if (method == "GTRACEET")
		return TRACE;
	if (method == "DELETE")
		return DELETE;
	if (method == "PUT")
		return PUT;
	if (method == "POST")
		return POST;
	if (method == "HEAD")
		return HEAD;
	if (method == "OPTIONS")
		return OPTIONS;
}

void getFileNameAndSuffix(string &fileName, string &suffix)
{
	fileName = resource.substr(1, resource.find('.') - 1);
	suffix = resource.substr(6, resource.find('?') - resource.find('.') - 1);
}

bool validation(string fileName, string fileSuffix, string language)
{
	bool isValid =true;

	if (fileName != "someFacts")
	{
		isValid = false;
	}
	else if (fileSuffix != "html" && fileSuffix != "txt")
	{
		isValid = false;
	}
	else if (language != "lang=he" && language != "lang=fr" && language != "lang=en")
	{
		isValid = false;
	}
	else
		return isValid;
}

void splitRequest(string &method,string &resource, int index)
{
	string requestBuffer = sockets[index].buffer;
	method = requestBuffer.substr(0, requestBuffer.find(' '));
	requestBuffer = requestBuffer.substr(requestBuffer.find(' ') + 1, string::npos);
	resource = requestBuffer.substr(0, requestBuffer.find(' '));
}

void handleHeadAndGet(int index, int reqMethod)
{
	string fileName, fileSuffix;
	getFileNameAndSuffix(fileName, fileSuffix);
	string language = resource.substr(resource.find('?') + 1, string::npos);
	bool isValid = validation(fileName, fileSuffix, language);

	if (isValid == false)
	{
		sockets[index].statusCode = NOTFOUND;
	}

	else
	{
		sockets[index].statusCode = OK;
		FILE* file=nullptr;
		if (strcmp(fileSuffix.c_str(), "html") == 0)
		{
			const char* englishRouth = "C:\\Temp\\html\\en.html";
			const char* hebrewRouth = "C:\\Temp\\html\\he.html";
			const char* frenchRouth = "C:\\Temp\\html\\fr.html";

			if (strcmp(language.c_str(),"lang=he")==0)
			{
				file = fopen(hebrewRouth, "r");
				strcpy(sockets[index].sendBuff, hebrewRouth);
			}

			else if (strcmp(language.c_str(), "lang=fr")==0)
			{
				file = fopen(frenchRouth, "r");
				strcpy(sockets[index].sendBuff, frenchRouth);
			}
			else {

				file = fopen(englishRouth, "r");
				strcpy(sockets[index].sendBuff, englishRouth);

			}
		}
		else if (strcmp(fileSuffix.c_str(), "txt")==0)
		{
			const char* englishRouth = "C:\\Temp\\txt\\en.txt";
			const char* hebrewRouth = "C:\\Temp\\txt\\he.txt";
			const char* frenchRouth = "C:\\Temp\\txt\\fr.txt";

			if (strcmp(language.c_str(), "lang=he")==0)
			{
				file = fopen(hebrewRouth, "r");
				strcpy(sockets[index].sendBuff, hebrewRouth);
			}

			else if (strcmp(language.c_str(), "lang=fr")==0)
			{
				file = fopen(frenchRouth, "r");
				strcpy(sockets[index].sendBuff, frenchRouth);
			}
			else {
				file = fopen(englishRouth, "r");
				strcpy(sockets[index].sendBuff, englishRouth);
			}
		}

		if (reqMethod == 100)//GET
		{
			sockets[index].sendSubType = GET;
		}
		else
		{
			sockets[index].sendSubType = HEAD;
		}
		fclose(file);
	}
}
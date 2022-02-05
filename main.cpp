#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <chrono>

using namespace std;
using namespace std::chrono;

#pragma warning(disable : 4996)

enum QueryTypes
{
	A2S_INFO = 0,
	A2S_PLAYER = 1,
	A2S_RULES = 2,
	A2A_PING = 3,
	A2S_SERVERQUERY_GETCHALLENGE = 4
};

const int PACKET_SIZE = 1390;
static const int TIMEOUT = 500;

string readStr(void* buffer, int offset)
{
	string result;

	char curChar = *((char*)((char*)buffer + offset++));
	while (curChar != '\0')
	{
		result += curChar;
		curChar = *((char*)((char*)buffer + offset++));
	}

	return result;
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		cout << "Usage: csnz-ping-dedis <ip> <port_start> [port_end=None] [query_type=A2A_PING]\n";
	}
	else
	{
		string ip = argv[1];
		int port_start = atoi(argv[2]);

		if (port_start <= 0)
		{
			cout << "Invalid port!\n";
			return 0;
		}

		int port_end = argc >= 4 ? atoi(argv[3]) : port_start;

		if (argc >= 4 && port_end <= 0)
			port_end = port_start;

		int query_type = argc >= 5 ? atoi(argv[4]) : A2A_PING;

		if (query_type < 0 || query_type > 4)
		{
			cout << "Invalid query type!\n";
			return 0;
		}

		cout << "Request started.\n\n";
		cout << "IP: " << ip << "\n";
		cout << "Port Start: " << port_start << "\n";
		cout << "Port End: " << port_end << "\n";
		cout << "Query Type: " << query_type << "\n";

		for (int port = port_start; port <= port_end; port++)
		{
			cout << "\nRequesting Port " << port << "\n";

			SOCKET sock = -1;
			WSADATA data = {};
			sockaddr_in addrRemote = {};
			int result;

			if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
			{
				cout << "Could not initialize winsock: " << WSAGetLastError() << "\n";
				continue;
			}

			sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

			if (sock != -1)
			{
				setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&TIMEOUT), sizeof(TIMEOUT));

				addrRemote.sin_family = AF_INET;
				addrRemote.sin_port = htons(port);
				addrRemote.sin_addr.s_addr = inet_addr(ip.c_str());
				result = connect(sock, (sockaddr*)&addrRemote, sizeof(addrRemote));

				if (result != -1)
				{
					vector<unsigned char> packetBuffer;

					// CSO Header
					packetBuffer.push_back(0x56);
					packetBuffer.push_back(0x8F);
					packetBuffer.push_back(0x76);
					packetBuffer.push_back(0x7C);
					packetBuffer.push_back(0x01);
					packetBuffer.push_back(0x00);

					for (int i = 0; i < 4; i++)
						packetBuffer.push_back(0xFF); // GoldSrc Header

					switch (query_type)
					{
					case A2S_INFO:
					{
						packetBuffer.push_back(0x54); // 'T'

						string str = "Source Engine Query";
						packetBuffer.insert(packetBuffer.end(), str.begin(), str.end());
						break;
					}
					case A2S_PLAYER:
					{
						packetBuffer.push_back(0x55); // 'U'
						for (int i = 0; i < 4; i++)
							packetBuffer.push_back(0xFF); // request challenge number

						result = send(sock, (const char*)packetBuffer.data(), (int)packetBuffer.size(), 0);
						if (result == SOCKET_ERROR)
						{
							cout << "Error on sending: " << WSAGetLastError() << "\n";
							continue;
						}

						char responseBuffer[PACKET_SIZE];
						memset(responseBuffer, 0, PACKET_SIZE);

						result = recv(sock, responseBuffer, PACKET_SIZE, 0);
						if (result == SOCKET_ERROR)
						{
							cout << "Error on receiving: " << WSAGetLastError() << "\n";
							continue;
						}

						for (int i = 0; i < 4; i++)
							packetBuffer.pop_back(); // remove request challenge number

						// Insert challenge number
						packetBuffer.push_back(responseBuffer[11]);
						packetBuffer.push_back(responseBuffer[12]);
						packetBuffer.push_back(responseBuffer[13]);
						packetBuffer.push_back(responseBuffer[14]);
						break;
					}
					case A2S_RULES:
					{
						packetBuffer.push_back(0x56); // 'V'
						for (int i = 0; i < 4; i++)
							packetBuffer.push_back(0xFF); // request challenge number

						result = send(sock, (const char*)packetBuffer.data(), (int)packetBuffer.size(), 0);
						if (result == SOCKET_ERROR)
						{
							cout << "Error on sending: " << WSAGetLastError() << "\n";
							continue;
						}

						char responseBuffer[PACKET_SIZE];
						memset(responseBuffer, 0, PACKET_SIZE);

						result = recv(sock, responseBuffer, PACKET_SIZE, 0);
						if (result == SOCKET_ERROR)
						{
							cout << "Error on receiving: " << WSAGetLastError() << "\n";
							continue;
						}

						for (int i = 0; i < 4; i++)
							packetBuffer.pop_back(); // remove request challenge number

						// Insert challenge number
						packetBuffer.push_back(responseBuffer[11]);
						packetBuffer.push_back(responseBuffer[12]);
						packetBuffer.push_back(responseBuffer[13]);
						packetBuffer.push_back(responseBuffer[14]);
						break;
					}
					case A2A_PING:
					{
						packetBuffer.push_back(0x69); // 'i'
						break;
					}
					case A2S_SERVERQUERY_GETCHALLENGE:
					{
						packetBuffer.push_back(0x57); // 'W'
						break;
					}
					}

					result = send(sock, (const char*)packetBuffer.data(), (int)packetBuffer.size(), 0);
					if (result == SOCKET_ERROR)
					{
						cout << "Error on sending: " << WSAGetLastError() << "\n";
						continue;
					}

					milliseconds start_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

					char buffer[PACKET_SIZE];
					memset(buffer, 0, PACKET_SIZE);

					result = recv(sock, buffer, PACKET_SIZE, 0);
					if (result == SOCKET_ERROR)
					{
						cout << "Error on receiving: " << WSAGetLastError() << "\n";
						continue;
					}

					milliseconds end_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

					void* responseBuffer = buffer;
					
					char name[MAX_PATH];

					sprintf(name, "ServerMessage_%d.bin", query_type);

					FILE* file = NULL;
					file = fopen(name, "wb");
					if (!file)
					{
						printf("Can't open '%s' file to write server message\n", name);
					}
					else
					{
						fwrite(responseBuffer, PACKET_SIZE, 1, file);
						fclose(file);
					}

					switch (query_type)
					{
					case A2S_INFO:
					{
						int offset = 11;

						string address = readStr(responseBuffer, offset);
						string name = readStr(responseBuffer, offset += (int)address.size() + 1);
						string map = readStr(responseBuffer, offset += (int)name.size() + 1);
						string folder = readStr(responseBuffer, offset += (int)map.size() + 1);
						string game = readStr(responseBuffer, offset += (int)folder.size() + 1);
						unsigned char players = *((unsigned char*)((char*)responseBuffer + (offset += (int)game.size() + 1))); offset++;
						unsigned char maxplayers = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char protocol = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char servertype = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char operatingsystem = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char passworded = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char mod = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						string websiteurl;
						string downloadurl;
						unsigned long version = 0;
						unsigned long size = 0;
						unsigned char type = 0;
						unsigned char dll = 0;

						if ((mod & 0xFF))
						{
							websiteurl = readStr(responseBuffer, offset);
							downloadurl = readStr(responseBuffer, offset += (int)websiteurl.size() + 1);
							version = *((unsigned long*)((char*)responseBuffer + (offset += (int)downloadurl.size() + 2))); offset += 4;
							size = *((unsigned long*)((char*)responseBuffer + offset)); offset += 4;
							type = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
							dll = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						}

						unsigned char vac = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
						unsigned char bots = *((unsigned char*)((char*)responseBuffer + offset)); offset++;

						
						cout << "\nAddress: " << address << "\n";
						cout << "Name: " << name << "\n";
						cout << "Map: " << map << "\n";
						cout << "Folder: " << folder << "\n";
						cout << "Game: " << game << "\n";
						cout << "Players: " << (players & 0xFF) << "/" << (maxplayers & 0xFF) << "\n";
						cout << "Protocol: " << (protocol & 0xFF) << "\n";
						cout << "Server type: " << ((servertype & 0xFF) == 'd' ? "Dedicated" : (servertype & 0xFF) == 'l' ? "Listen" : "HLTV") << "\n";
						cout << "Operating System: " << ((operatingsystem & 0xFF) == 'l' ? "Linux" : "Windows") << "\n";
						cout << "Passworded: " << ((passworded & 0xFF) == 0 ? "No" : "Yes") << "\n";
						cout << "Half-life Mod: " << ((mod & 0xFF) == 0 ? "No" : "Yes") << "\n";

						if ((mod & 0xFF))
						{
							cout << "Mod Website: " << websiteurl << "\n";
							cout << "Mod Download: " << downloadurl << "\n";
							cout << "Mod Version: " << version << "\n";
							cout << "Mod Size: " << size << "\n";
							cout << "Mod Type: " << ((type & 0xFF) == 0 ? "Single and Multiplayer" : "Multiplayer Only") << "\n";
							cout << "Mod DLL: " << ((dll & 0xFF) == 0 ? "Half-life DLL" : "Custom DLL") << "\n";
						}

						cout << "VAC: " << ((vac & 0xFF) == 0 ? "No" : "Yes") << "\n";
						cout << "Number of bots: " << (bots & 0xFF) << "\n";

						break;
					}
					case A2S_PLAYER:
					{
						int offset = 11;

						unsigned char numplayers = *((unsigned char*)((char*)responseBuffer + offset)); offset++;

						cout << "\nNumber of Players: " << (numplayers & 0xFF) << "\n";

						for (unsigned char player = 0; player < (numplayers & 0xFF); player++)
						{
							unsigned char index = *((unsigned char*)((char*)responseBuffer + offset)); offset++;
							string name = readStr(responseBuffer, offset);
							unsigned long score = *((unsigned long*)((char*)responseBuffer + (offset += (int)name.size() + 1))); offset += 4;
							float duration = *((float*)((char*)responseBuffer + offset)); offset += 4;

							cout << "\nIndex: " << (index & 0xFF) << "\n";
							cout << "Name: " << name << "\n";
							cout << "Score: " << score << "\n";
							cout << "Duration: " << duration << "\n";
						}

						break;
					}
					case A2S_RULES:
					{
						char buffer3[PACKET_SIZE * 2];
						memset(buffer3, 0, PACKET_SIZE * 2);

						void* responseBuffer3 = buffer3;
						memcpy(responseBuffer3, responseBuffer, PACKET_SIZE);

						if (*((unsigned char*)((char*)responseBuffer + 6)) == 0xFE)
						{
							char buffer2[PACKET_SIZE];
							memset(buffer2, 0, PACKET_SIZE);

							result = recv(sock, buffer2, PACKET_SIZE, 0);
							if (result == SOCKET_ERROR)
							{
								cout << "Error on receiving: " << WSAGetLastError() << "\n";
								continue;
							}

							sprintf(name, "ServerMessage_2_2.bin");

							FILE* file = NULL;
							file = fopen(name, "wb");
							if (!file)
							{
								printf("Can't open '%s' file to write server message\n", name);
							}
							else
							{
								fwrite(buffer2, PACKET_SIZE, 1, file);
								fclose(file);
							}

							memcpy((char*)responseBuffer3+PACKET_SIZE, (char*)buffer2+15, PACKET_SIZE);
						}

						int offset = 20;

						unsigned short numrules = *((unsigned short*)((char*)responseBuffer3 + offset)); offset += 2;

						cout << "\nNumber of Rules: " << numrules << "\n";

						for (unsigned short rule = 0; rule < numrules; rule++)
						{
							string name = readStr(responseBuffer3, offset);
							string value = readStr(responseBuffer3, offset += (int)name.size() + 1); offset += (int)value.size() + 1;

							cout << "\nName: " << name << "\n";
							cout << "Value: " << value << "\n";
						}

						break;
					}
					case A2A_PING:
					{
						cout << "\nPing: " << end_time.count() - start_time.count() << " ms\n";

						break;
					}
					case A2S_SERVERQUERY_GETCHALLENGE:
					{
						int offset = 11;

						unsigned long challenge = *((unsigned long*)((char*)responseBuffer + offset));

						cout << "\nChallenge number: " << challenge << "\n";
					}
					}
				}
				else
				{
					cout << "Error on connect: " << WSAGetLastError() << "\n";
					continue;
				}
			}
			else
			{
				cout << "Error on socket: " << WSAGetLastError() << "\n";
				continue;
			}

			if (sock != -1)
			{
				closesocket(sock);
				WSACleanup();
			}
		}
		cout << "\nRequest finished.\n";
	}
}
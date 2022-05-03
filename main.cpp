#include <iostream>
#include <chrono>
#include <Ws2tcpip.h>

using namespace std;
using namespace chrono;

enum QueryTypes
{
	A2S_INFO = 0,
	A2S_PLAYER = 1,
	A2S_RULES = 2,
	A2A_PING = 3,
	A2S_SERVERQUERY_GETCHALLENGE = 4
};

struct A2S_INFO_s
{
	string server_address;
	string host_name;
	string map_name;
	string game_dir;
	string game_name;
	unsigned char players = 0;
	unsigned char max_players = 0;
	unsigned char protocol = 0;

	unsigned char server_type = 0;
	unsigned char server_os = 0;
	unsigned char password = 0;

	unsigned char mod = 0;

	string url_info;
	string url_dl;
	string hl_version;
	unsigned long version = 0;
	unsigned long size = 0;
	unsigned char svonly = 0;
	unsigned char cldll = 0;

	unsigned char vac = 0;
	unsigned char bots = 0;
};

struct A2S_PLAYER_s
{
	unsigned char players = 0;

	unsigned char index = 0;
	string name;
	unsigned long score = 0;
	float duration = 0;
};

struct A2S_RULES_s
{
	unsigned short rules = 0;

	string name;
	string value;
};

struct A2S_SERVERQUERY_GETCHALLENGE_s
{
	unsigned long challenge = 0;
};

static const int PACKET_SIZE = 1400;
static const int TIMEOUT = 3000;

int udp_sendnrecv(vector<unsigned char> out_buffer, int out_len, char* in_buffer, int in_len, const char* host, int port, int query_type)
{
	WSADATA data = {};
	if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
		return -1; // failed to initialize winsock

	sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, host, &servaddr.sin_addr) == 0)
		return -2; // failed to parse the host address
	servaddr.sin_port = htons(port);
	int servaddr_len = sizeof(servaddr);

	SOCKET fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (fd < 0)
		return -3; // failed to open a socket, permission issues?

	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&TIMEOUT), sizeof(TIMEOUT));

	if (sendto(fd, reinterpret_cast<const char*>(out_buffer.data()), out_len, 0, (sockaddr*)&servaddr, sizeof(servaddr)) < 0)
		return closesocket(fd), -4; // send failed

	if (recvfrom(fd, in_buffer, in_len, 0, (sockaddr*)&servaddr, &servaddr_len) < 0)
		return closesocket(fd), -5; // receive failed

	if (query_type == A2S_PLAYER || query_type == A2S_RULES)
	{
		for (int i = 0; i < 4; i++)
			out_buffer.pop_back(); // remove request challenge number

		for (int i = 21; i <= 24; i++)
			out_buffer.push_back(in_buffer[i]); // insert challenge number

		if (sendto(fd, reinterpret_cast<const char*>(out_buffer.data()), out_len, 0, (sockaddr*)&servaddr, sizeof(servaddr)) < 0)
			return closesocket(fd), -4; // send failed

		memset(in_buffer, 0, in_len);

		if (recvfrom(fd, in_buffer, in_len, 0, (sockaddr*)&servaddr, &servaddr_len) < 0)
			return closesocket(fd), -5; // receive failed

		if (query_type == A2S_RULES)
		{
			if (recvfrom(fd, &in_buffer[in_len], in_len, 0, (sockaddr*)&servaddr, &servaddr_len) < 0)
				return closesocket(fd), -5; // receive failed
		}
	}

	closesocket(fd);
	return 1;
}

int log_servermessage(char* buffer, int len, int query_type)
{
	char name[MAX_PATH];
	sprintf_s(name, "ServerMessage_%d.bin", query_type);

	FILE* file = NULL;
	fopen_s(&file, name, "wb");
	if (!file)
	{
		printf("\nCan't open file '%s' to write server message\n", name);
		return -1;
	}
	
	fwrite(buffer, len, 1, file);
	fclose(file);
	return 1;
}

string readStr(char* buffer, int offset)
{
	std::string result;

	char curChar = buffer[offset]; offset++;
	while (curChar != '\0')
	{
		result += curChar;
		curChar = buffer[offset]; offset++;
	}

	return result;
}

void display_servermessage(char* buffer, int query_type, long long ping)
{
	int offset = query_type == A2S_RULES ? 30 : 21;

	switch (query_type)
	{
	case A2S_INFO:
	{
		A2S_INFO_s a2s_info;

		a2s_info.server_address = readStr(buffer, offset); offset += static_cast<int>(a2s_info.server_address.size()) + 1;
		a2s_info.host_name = readStr(buffer, offset); offset += static_cast<int>(a2s_info.host_name.size()) + 1;
		a2s_info.map_name = readStr(buffer, offset); offset += static_cast<int>(a2s_info.map_name.size()) + 1;
		a2s_info.game_dir = readStr(buffer, offset); offset += static_cast<int>(a2s_info.game_dir.size()) + 1;
		a2s_info.game_name = readStr(buffer, offset); offset += static_cast<int>(a2s_info.game_name.size()) + 1;
		a2s_info.players = buffer[offset]; offset++;
		a2s_info.max_players = buffer[offset]; offset++;
		a2s_info.protocol = buffer[offset]; offset++;
		a2s_info.server_type = buffer[offset]; offset++;
		a2s_info.server_os = buffer[offset]; offset++;
		a2s_info.password = buffer[offset]; offset++;
		a2s_info.mod = buffer[offset]; offset++;

		if ((a2s_info.mod))
		{
			a2s_info.url_info = readStr(buffer, offset); offset += static_cast<int>(a2s_info.url_info.size()) + 1;
			a2s_info.url_dl = readStr(buffer, offset); offset += static_cast<int>(a2s_info.url_dl.size()) + 1;
			a2s_info.hl_version = readStr(buffer, offset); offset += static_cast<int>(a2s_info.hl_version.size()) + 1;
			a2s_info.version = *(unsigned long*)(buffer + offset); offset += 4;
			a2s_info.size = *(unsigned long*)(buffer + offset); offset += 4;
			a2s_info.svonly = buffer[offset]; offset++;
			a2s_info.cldll = buffer[offset]; offset++;
		}

		a2s_info.vac = buffer[offset]; offset++;
		a2s_info.bots = buffer[offset]; offset++;

		cout << "\nAddress: " << a2s_info.server_address << "\n";
		cout << "Name: " << a2s_info.host_name << "\n";
		cout << "Map: " << a2s_info.map_name << "\n";
		cout << "Folder: ";
		for (int i = 0; i < static_cast<int>(a2s_info.game_dir.size()); i++)
		{
			printf("\\x%X", a2s_info.game_dir[i]);
		}
		cout << "\n";
		cout << "Game: " << a2s_info.game_name << "\n";
		cout << "Players: " << (a2s_info.players & 0xFF) << "/" << (a2s_info.max_players & 0xFF) << "\n";
		cout << "Protocol: " << (a2s_info.protocol & 0xFF) << "\n";
		cout << "Server type: " << (a2s_info.server_type == 'd' ? "Dedicated" : a2s_info.server_type == 'l' ? "Listen" : "HLTV") << "\n";
		cout << "Operating System: " << (a2s_info.server_os == 'l' ? "Linux" : "Windows") << "\n";
		cout << "Passworded: " << (a2s_info.password == 0 ? "No" : "Yes") << "\n";
		cout << "Half-life Mod: " << (a2s_info.mod == 0 ? "No" : "Yes") << "\n";

		if ((a2s_info.mod))
		{
			cout << "Mod Website: " << a2s_info.url_info << "\n";
			cout << "Mod Download: " << a2s_info.url_dl << "\n";
			cout << "Mod Version: " << a2s_info.version << "\n";
			cout << "Mod Size: " << a2s_info.size << "\n";
			cout << "Mod Type: " << (a2s_info.svonly == 0 ? "Singleplayer and Multiplayer" : "Multiplayer Only") << "\n";
			cout << "Mod DLL: " << (a2s_info.cldll == 0 ? "Half-life DLL" : "Custom DLL") << "\n";
		}

		cout << "VAC: " << (a2s_info.vac == 0 ? "No" : "Yes") << "\n";
		cout << "Number of bots: " << (a2s_info.bots & 0xFF) << "\n";

		break;
	}
	case A2S_PLAYER:
	{
		A2S_PLAYER_s a2s_player;

		a2s_player.players = buffer[offset]; offset++;

		cout << "\nNumber of Players: " << (a2s_player.players & 0xFF) << "\n";

		for (unsigned char player = 0; player < a2s_player.players; player++)
		{
			a2s_player.index = buffer[offset]; offset++;
			a2s_player.name = readStr(buffer, offset); offset += static_cast<int>(a2s_player.name.size()) + 1;
			a2s_player.score = *(unsigned long*)(buffer + offset); offset += 4;
			a2s_player.duration = *(float*)(buffer + offset); offset += 4;

			cout << "\nIndex: " << (a2s_player.index & 0xFF) << "\n";
			cout << "Name: " << a2s_player.name << "\n";
			cout << "Score: " << a2s_player.score << "\n";
			cout << "Duration: " << a2s_player.duration << "\n";
		}

		break;
	}
	case A2S_RULES:
	{
		memcpy(&buffer[PACKET_SIZE], &buffer[PACKET_SIZE + 25], PACKET_SIZE - 25);

		A2S_RULES_s a2s_rules;

		a2s_rules.rules = *(unsigned short*)(buffer + offset); offset += 2;

		cout << "\nNumber of Rules: " << a2s_rules.rules << "\n";

		for (unsigned short rule = 0; rule < a2s_rules.rules; rule++)
		{
			a2s_rules.name = readStr(buffer, offset); offset += static_cast<int>(a2s_rules.name.size()) + 1;
			a2s_rules.value = readStr(buffer, offset); offset += static_cast<int>(a2s_rules.value.size()) + 1;

			cout << "\nName: " << a2s_rules.name << "\n";
			cout << "Value: " << a2s_rules.value << "\n";
		}

		break;
	}
	case A2A_PING: cout << "\nPing: " << ping << " ms\n"; break;
	case A2S_SERVERQUERY_GETCHALLENGE:
	{
		A2S_SERVERQUERY_GETCHALLENGE_s a2s_serverquery_getchallenge;

		a2s_serverquery_getchallenge.challenge = *(unsigned long*)(buffer + offset);

		cout << "\nChallenge number: " << a2s_serverquery_getchallenge.challenge << "\n";
	}
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3 || argc > 4)
	{
		cout << "Usage: csnz-ping-dedis <ip> <port> [query_type=A2A_PING]\n";
	}
	else
	{
		string ip = argv[1];
		int port = clamp(atoi(argv[2]), 0, 65535);
		int query_type = argc == 4 ? clamp(atoi(argv[3]), 0, 4) : A2A_PING;

		cout << "Request started.\n\n";
		cout << "IP: " << ip << "\n";
		cout << "Port: " << port << "\n";
		cout << "Query Type: " << (query_type == 4 ? "A2S_SERVERQUERY_GETCHALLENGE" : (query_type == 3 ? "A2A_PING" : (query_type == 2 ? "A2S_RULES" : (query_type == 1 ? "A2S_PLAYER" : "A2S_INFO")))) << "\n";

		vector<unsigned char> out_buffer;

		switch (query_type)
		{
		case A2S_INFO:
		{
			out_buffer = { 0x56, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'T' };

			string str = "Source Engine Query";
			out_buffer.insert(out_buffer.end(), str.begin(), str.end());
			break;
		}
		case A2S_PLAYER: out_buffer = { 0x56, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'U', 0xFF, 0xFF, 0xFF, 0xFF }; break;
		case A2S_RULES: out_buffer = { 0x56, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'V', 0xFF, 0xFF, 0xFF, 0xFF }; break;
		case A2A_PING: out_buffer = { 0x56, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'i' }; break;
		case A2S_SERVERQUERY_GETCHALLENGE: out_buffer = { 0x56, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'W' }; break;
		}

		char in_buffer[PACKET_SIZE * 2];
		memset(in_buffer, 0, PACKET_SIZE * 2);

		auto start = high_resolution_clock::now();

		int result = udp_sendnrecv(out_buffer, static_cast<int>(out_buffer.size()), in_buffer, PACKET_SIZE, ip.c_str(), port, query_type);
		if (result < 0)
		{
			cout << "\nFailed to " << (result == -5 ? "receive the packet.\n" : (result == -4 ? "send the packet.\n" : (result == -3 ? "open the socket.\n" : (result == -2 ? "parse the host address.\n" : "initialize winsock.\n"))));
			return 0;
		}

		auto end = high_resolution_clock::now();

		log_servermessage(in_buffer, query_type == A2S_RULES ? PACKET_SIZE * 2 : PACKET_SIZE, query_type);
		display_servermessage(in_buffer, query_type, duration_cast<milliseconds>(end - start).count());

		cout << "\nRequest finished.\n";
		cin.get();
	}
}
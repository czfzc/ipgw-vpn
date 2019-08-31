#pragma once

extern "C" {
#include <pcap.h>
}

#include <winsock2.h>
#include <tchar.h>
#include <queue>
#include <list>
#include <map>

#include "data.h"

#pragma comment(lib,"DelayImp.lib")
#pragma comment(lib, "ws2_32.lib")
using namespace std;

static pcap_if_t* alldevs;
static char errbuf[PCAP_ERRBUF_SIZE];

typedef struct {
	pcap_t* Handle;
	HANDLE ThreadCap;
	pcap_if_t* Device;
} stInterface;

static list<stInterface> PcapHandle;

typedef struct {
	uint32_t Length;
	char* Data;
}Packet;

constexpr auto OFFSET_IPPROTO = 12;
constexpr auto OFFSET_NETLAYER = 14;
constexpr auto OFFSET_IPSRC = OFFSET_NETLAYER + 12;
constexpr auto OFFSET_IPDST = OFFSET_NETLAYER + 16;

static BOOL SelectSurvive = 1;

static HANDLE hIpgwCapture = INVALID_HANDLE_VALUE;
static HANDLE hIpgwSend = INVALID_HANDLE_VALUE;
static HANDLE hServerCapture = INVALID_HANDLE_VALUE;
static HANDLE hServerSend = INVALID_HANDLE_VALUE;
static HANDLE hTunnelUDP = INVALID_HANDLE_VALUE;

static SOCKET SockUDP;
static sockaddr_in AddrUDP;
static sockaddr_in SelfAddrUDP;

static pcap_t* ValidInterfaceHandle = nullptr;
static stInterface ValidInterface;

static pcap_handler __PacketHandler;
void PacketHandler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data);
void SelectInterface_Server(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data);

static queue<Packet*>IpgwToServerPacket;
CRITICAL_SECTION csIpgwToServerCode;
CONDITION_VARIABLE cvSendToServer;
static queue<Packet*>ServerToIpgwPacket;
CRITICAL_SECTION csServerToIpgwCode;
CONDITION_VARIABLE cvSendToIpgw;

static unsigned char EtherHeader[14];

constexpr auto MAX_QUEUE_SIZE = 1000;

inline BOOL IfFromIpgw(char *_pack)
{
	return (*(DWORD*)(_pack + OFFSET_IPSRC) == inet_addr(IPGW));
}

BOOL LoadNpcapDlls()
{
	_TCHAR npcap_dir[512];
	UINT len;
	len = GetSystemDirectory(npcap_dir, 480);
	if (!len) {
		fprintf(stderr, "Error in GetSystemDirectory: %x", GetLastError());
		return FALSE;
	}
	_tcscat_s(npcap_dir, 512, _T("\\Npcap"));
	if (SetDllDirectory(npcap_dir) == 0) {
		fprintf(stderr, "Error in SetDllDirectory: %x", GetLastError());
		return FALSE;
	}
	return TRUE;
}

void InitClient()
{
	if (!LoadNpcapDlls())
		_asm int 3

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		fprintf(stderr, "Error in pcap_findalldevs: %s\n", errbuf);
		_asm int 3
	}

	stInterface temp;
	for (pcap_if_t* d = alldevs; d; d = d->next)
	{
		temp.Handle = pcap_open_live(d->name, 65536, 0, 2, errbuf); //timeout = 0
		if (!temp.Handle)
			_asm int 3
		temp.Device = d;
		PcapHandle.push_front(temp);
	}
	InitializeCriticalSection(&csIpgwToServerCode);
	InitializeCriticalSection(&csServerToIpgwCode);

	SockUDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (SockUDP == INVALID_SOCKET)
		_asm int 3
	memset(&AddrUDP, 0, sizeof(sockaddr_in));
	AddrUDP.sin_family = AF_INET;
	inet_pton(AF_INET, SERVER_A_IP_ADDR, &AddrUDP.sin_addr);
	AddrUDP.sin_port = htons(SERVER_A_UDP_PORT);

	memset(&SelfAddrUDP, 0, sizeof(sockaddr_in));
	SelfAddrUDP.sin_family = AF_INET;
	SelfAddrUDP.sin_port = htons(DEFAULT_UDP_PORT);
	if (bind(SockUDP, (sockaddr*)& SelfAddrUDP, sizeof(sockaddr_in)) == SOCKET_ERROR)
		_asm int 3
}

DWORD WINAPI ThreadTunnel(LPVOID h)
{
	UNREFERENCED_PARAMETER(h);
	int count = 0;
	const char BeatStr[32] = "UDP HEART BEAT No.%d!!";
	char SendBuff[32];
	while (1)
	{
		sprintf_s(SendBuff, BeatStr, ++count);
		sendto(SockUDP, SendBuff, 32, 0, (sockaddr*)& AddrUDP, sizeof(sockaddr_in));
		Sleep(2000);
	}
}

DWORD WINAPI ThreadCap(LPVOID h)
{
	if (h)
		pcap_loop((pcap_t*)h, 0, __PacketHandler, (unsigned char*)h);
	else
		pcap_loop(ValidInterfaceHandle, 0, __PacketHandler, NULL);
	return 0;
}

BOOL SelectInterface()
{
	map<pcap_t*, HANDLE>Handles;
	__PacketHandler = (pcap_handler)SelectInterface_Server;

	for (auto& p : PcapHandle)
	{
		HANDLE TempHandle = CreateThread(NULL, 0, ThreadCap, p.Handle, 0, NULL);
		Handles.insert(map<pcap_t*, HANDLE>::value_type(p.Handle, TempHandle));
	}
	
	SOCKET SockIpgw = socket(AF_INET, SOCK_STREAM, 0);
	if (SockIpgw == INVALID_SOCKET)
		_asm int 3
	sockaddr_in AddrIpgw;
	memset(&AddrIpgw, 0, sizeof(sockaddr_in));
	AddrIpgw.sin_family = AF_INET;
	inet_pton(AF_INET, IPGW, &AddrIpgw.sin_addr);
	AddrIpgw.sin_port = htons(IPGW_DEFAULT_PORT);

	Sleep(2000);
	if (connect(SockIpgw, (sockaddr*)&AddrIpgw, sizeof(sockaddr_in)) == SOCKET_ERROR)
		_asm int 3

	Sleep(1000);
	
	if (ValidInterfaceHandle == nullptr)
		_asm int 3
	else {
		for (list<stInterface>::iterator p = PcapHandle.begin(); p != PcapHandle.end(); p++)
		{
			map<pcap_t*, HANDLE>::iterator iter = Handles.find(p->Handle);
			if (iter == Handles.end())
				_asm int 3
			else {
				TerminateThread(iter->second, 0);
				if (p->Handle == ValidInterfaceHandle)
					ValidInterface = *p;

			}
		}
	}

	printf("Interface Selected as %s", ValidInterface.Device->description);
	return 0;
}

void SelectInterface_Server(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data)
{
	if (SelectSurvive)
		if (IfFromIpgw((char *)pkt_data))
		{
			SelectSurvive = 0;

			ValidInterfaceHandle = (pcap_t*)param;
			memcpy(EtherHeader, pkt_data + 6, 6);
			memcpy(EtherHeader + 6, pkt_data, 6);
			*(WORD*)(EtherHeader + OFFSET_IPPROTO) = 0x0008;
		
		}
}


void PacketHandler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data)
{
	Packet* TempPack = new Packet;

	if (!IfFromIpgw((char*)pkt_data))
		return;
	
	TempPack->Length = header->len;
	TempPack->Data = new char[TempPack->Length];
	memcpy(TempPack->Data, pkt_data, TempPack->Length);
	EnterCriticalSection(&csIpgwToServerCode);
	if (IpgwToServerPacket.size() <= MAX_QUEUE_SIZE)
		IpgwToServerPacket.push(TempPack);
	
	WakeAllConditionVariable(&cvSendToServer);
	LeaveCriticalSection(&csIpgwToServerCode);
}

DWORD WINAPI ThreadSendToServer(LPVOID)
{
	Packet* TempPacket;

	while (1)
	{
		EnterCriticalSection(&csIpgwToServerCode);
		if (IpgwToServerPacket.empty())
		{
			SleepConditionVariableCS(&cvSendToServer, &csIpgwToServerCode, INFINITE);
			LeaveCriticalSection(&csIpgwToServerCode);
			continue;
		}
		printf("<%d>", IpgwToServerPacket.size());
		TempPacket = IpgwToServerPacket.front();
		IpgwToServerPacket.pop();
		LeaveCriticalSection(&csIpgwToServerCode);


		if (*(WORD*)(TempPacket->Data+OFFSET_IPPROTO) == 0x0008)
			if (IfFromIpgw(TempPacket->Data))
			{
				if (sendto(SockUDP, TempPacket->Data + OFFSET_NETLAYER, TempPacket->Length - OFFSET_NETLAYER, 0, (sockaddr*)& AddrUDP, sizeof(sockaddr_in)) == SOCKET_ERROR)
					_asm int 3
			}
		delete TempPacket->Data;
		delete TempPacket;
	}
}


DWORD WINAPI ThreadListenFromServer(LPVOID)
{
	Packet* TempPack = new Packet;
	char* Buff = new char[1500];
	sockaddr_in TempAddr;
	socklen_t TempLen = sizeof(sockaddr_in);
	while (1)
	{

		TempPack->Length = recvfrom(SockUDP, Buff, 1500, 0, (sockaddr*)&TempAddr, &TempLen);
		if (TempPack->Length == SOCKET_ERROR)
			_asm int 3

		TempPack->Data = new char[TempPack->Length + OFFSET_NETLAYER];
		memcpy(TempPack->Data + OFFSET_NETLAYER, Buff, TempPack->Length);
		EnterCriticalSection(&csServerToIpgwCode);
		if (ServerToIpgwPacket.size() <= MAX_QUEUE_SIZE)
			ServerToIpgwPacket.push(TempPack);
		
		WakeConditionVariable(&cvSendToIpgw);
		LeaveCriticalSection(&csServerToIpgwCode);
	}
}

DWORD WINAPI ThreadSendToIpgw(LPVOID)
{
	Packet* TempPack;
	while (1)
	{
		EnterCriticalSection(&csServerToIpgwCode);
		if (ServerToIpgwPacket.empty())
		{
			SleepConditionVariableCS(&cvSendToIpgw, &csServerToIpgwCode, INFINITE);
			LeaveCriticalSection(&csServerToIpgwCode);
			continue;
		}
		printf("(%d)", ServerToIpgwPacket.size());
		TempPack = ServerToIpgwPacket.front();
		ServerToIpgwPacket.pop();
		LeaveCriticalSection(&csServerToIpgwCode);

		TempPack->Length += OFFSET_NETLAYER;
		memcpy(TempPack->Data, EtherHeader, OFFSET_NETLAYER);

//		printf("\n");
//		for (int i = 0; i < TempPack->Length; i++) {
//			if (!(i % 8))
//				printf("\n");
//			printf("%02x ", (unsigned char) * (TempPack->Data + i));
//		}
//		printf("\n");

		if (pcap_sendpacket(ValidInterfaceHandle,(unsigned char*)TempPack->Data,TempPack->Length))
			_asm int 3
	}
}

void StartClient()
{
	__PacketHandler = PacketHandler;

	hTunnelUDP = CreateThread(NULL, 0, ThreadTunnel, NULL, 0, NULL);
	
	hIpgwSend = CreateThread(NULL, 0, ThreadSendToIpgw, NULL, 0, NULL);
	hServerSend = CreateThread(NULL, 0, ThreadSendToServer, NULL, 0, NULL);
	hIpgwCapture = CreateThread(NULL, 0, ThreadCap, NULL, 0, NULL);
	hServerCapture = CreateThread(NULL, 0, ThreadListenFromServer, NULL, 0, NULL);
}

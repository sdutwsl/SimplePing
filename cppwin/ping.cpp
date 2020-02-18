#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <iostream>
#include <Windows.h>
#include <stdio.h>

using namespace std;

#pragma comment(lib,"user32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

HANDLE hIcmpFile=0;
double iSendTimes=0;
double iLostTimes=0;

int iMinRTT=9999;
int iMaxRTT=0;
int iSumRTT=0;

char* sSendBuf="ABCDEFGHIJKLMNOPQRSTUVWXYZQWERT";
char* hostname;

HANDLE GetIcmpFileHandle()
{	
;	HANDLE hIcmpFile;
	hIcmpFile = IcmpCreateFile();
	if (hIcmpFile == INVALID_HANDLE_VALUE) {
		cout<<"Unable to open handle."<<endl;
 	}
	else {
		return hIcmpFile;
 	}  
	return 0;
}

void SendEcho2(HANDLE icmp,unsigned long IP,int time)
{
	int ReplySize = sizeof (ICMP_ECHO_REPLY) + 32+ 8;
	void* ReplyBuffer = (VOID *) malloc(ReplySize);
	IcmpSendEcho2(icmp,NULL, NULL, NULL,
                             IP, sSendBuf, 32, NULL,
                             ReplyBuffer, ReplySize, time);
	iSendTimes++;
	PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY) ReplyBuffer;
	in_addr ReplyAddr;
	ReplyAddr.S_un.S_addr = pEchoReply->Address;
	switch (pEchoReply->Status) {
		case IP_DEST_HOST_UNREACHABLE:
		case IP_DEST_NET_UNREACHABLE:
        		case IP_REQ_TIMED_OUT:
			cout<<"Request time out"<<endl;
			iLostTimes++;
            			break;
		default:
			cout<<"Reply from "<<inet_ntoa(ReplyAddr)<<": "<<"Bytes="<<pEchoReply->DataSize<<" RTT="<<pEchoReply->RoundTripTime<<"ms TTL="<<(int)pEchoReply->Options.Ttl<<endl;
			int rtt=pEchoReply->RoundTripTime;
			iSumRTT+=rtt;
			rtt>iMaxRTT?iMaxRTT=rtt:rtt=rtt;
			rtt<iMinRTT?iMinRTT=rtt:rtt=rtt;
            			break;
       	}
}

unsigned long GetHostByName(char *name)
{
	in_addr addr;
	WSADATA wsaData;
	hostent *remoteHost;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	remoteHost = gethostbyname(name);
	addr.s_addr = *(u_long *) remoteHost->h_addr_list[0];
	hostname=inet_ntoa(addr);
	cout<<"Ping "<<name<<" ["<<inet_ntoa(addr)<<"] with "<<32<<" bytes data: "<<endl;
	return inet_addr(inet_ntoa(addr));
	
}

void DoStatics()
{
	if(iSendTimes==0) return;
	cout<<endl<<"The static data of ping of "<<hostname<<": "<<endl;
	cout<<"\tpackages: Send = "<<iSendTimes<<", Received = "<<iSendTimes-iLostTimes<<", Lost = "<<iLostTimes;
	cout<<" ("<<int((double)iLostTimes/(double)iSendTimes*100)<<"% lost)"<<endl;
	cout<<"RTTS(ms):"<<endl;
	cout<<"\tMin = "<<iMinRTT<<"ms Max = "<<iMaxRTT<<"ms Ave = "<<iSumRTT/(iSendTimes-iLostTimes)<<"ms"<<endl;
}

BOOL CtrlHandler(DWORD fdwCtrlType) 
{
	switch( fdwCtrlType ) 
	{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT: 
 			DoStatics();
			exit(0);
			break;
		default:
			return FALSE;
	} 
} 

int main (int argc,char **argvs)
{
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
	unsigned long addr=GetHostByName(argvs[1]);
	while(true)
	{
		SendEcho2(GetIcmpFileHandle(),addr,4000);
		Sleep(1000);
	}
	return 0;
}
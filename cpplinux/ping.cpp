#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

using namespace std;

int numsend=0,numrecv=0;


void gethost(const string hostname, string& host, string& addr)
{
	hostent* hp=nullptr;
	if ((hp = gethostbyname(hostname.c_str())) == NULL)
	{
		cout << "Error get host by name" << endl;
		exit(0);
	}

	host=hp->h_name;
	addr=inet_ntoa(*reinterpret_cast<in_addr*>(hp->h_addr_list[0]));
}

unsigned short icmp_checksum(unsigned char* i, int len)
{
	unsigned long sum = 0;
	auto pbuf = reinterpret_cast<unsigned short*>(i);
	while (1 < len)
	{
		sum += *pbuf++;
		len -= 2;
	}
	if (1==len)
	{
		sum += *reinterpret_cast<unsigned short*>(i);
	}
	sum = (sum >> 16) + (sum & 0xffff);//
	sum += (sum >> 16);//上一步可能产生溢出
	return (unsigned short)(~sum);
}


void icmp_pack(icmp& i,int seq)
{
	memset(&i, 0, sizeof(i));
	i.icmp_type = ICMP_ECHO;	//ICMP 类型
	i.icmp_code = 0;
	i.icmp_cksum = 0;
	i.icmp_hun.ih_idseq.icd_id = getpid();
	i.icmp_hun.ih_idseq.icd_seq = seq;
	auto t = reinterpret_cast<timeval*>(i.icmp_dun.id_data);
	gettimeofday(t, NULL);
	i.icmp_cksum = icmp_checksum(reinterpret_cast<unsigned char*>(&i),sizeof(i));
}

void send_pack(int sock,int seq, string addr)
{
	icmp i;
	icmp_pack(i, seq);
	sockaddr_in si;
	memset(&si, 0, sizeof(si));
	si.sin_family = AF_INET;
	in_addr in;
	memset(&in, 0, sizeof(in));
	inet_aton(addr.c_str(), &in);
	si.sin_addr.s_addr = in.s_addr;
	sendto(sock, &i, sizeof(i), 0, reinterpret_cast<sockaddr*>(&si), sizeof(si));
}

void recv_pack(int sock)
{
	char buf[1500];
	auto n = recvfrom(sock, buf, 1500, 0, NULL, NULL);
	if (0 > n)
	{
		//cout << "recv_pack failed!" << endl;
		return ;
	}

	auto pip = reinterpret_cast<ip*>(buf);
	auto iplen = pip->ip_hl * 4;
	auto picmp = reinterpret_cast<icmp*>(buf + iplen);
	switch (picmp->icmp_type)
	{
	case ICMP_ECHOREPLY:
		{
			numrecv++;
			auto ot = reinterpret_cast<timeval*>(&picmp->icmp_dun.id_data);
			timeval t;
			gettimeofday(&t, NULL);
			auto rtt = (t.tv_sec - ot->tv_sec) * 1000 + (t.tv_usec - ot->tv_usec) / 1000;
			cout << "Recv an ICMP reply from ("<<inet_ntoa(pip->ip_src)<< "): Seq=" << picmp->icmp_hun.ih_idseq.icd_seq << "    TTL=" << pip->ip_ttl << "    RTT="<<rtt<<"ms"<<endl;
		}
		break;
	default:
		break;
	}

}


void statics()
{
	float f1=numsend;
	float f2=numrecv;
	float f3=f2/f1*100;
	f3=100.0f-f3;	
	cout<<endl<<"----------------------statics------------------"<<endl;
	cout<<"Total send "<<numsend<<" package(s), received "<<numrecv<<" package(s). "<<static_cast<int>(f3)<<" percent is lost"<<endl;
}

void icmp_sigint(int signo)
{
	statics();
	exit(0);
}


int main(int argc,char **argvs)
{
	signal(SIGINT, icmp_sigint);
	const string hostname = argvs[1];
	string host, addr;
	gethost(hostname, host, addr);
	cout<<"Ping "<<host<<" at "<<addr<<" ..."<<endl;
	auto sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (0 > sockfd)
	{
		cout << "Raw socket init failed!" << endl;
	}
	
	thread tsend([sockfd,addr]()
		{
			auto i = 1;
			while (true)
			{
				if(i>=128)
				{
					statics();
					exit(0);
				}
				sleep(1);
				send_pack(sockfd, i, addr);
				numsend++;
				i++;
			}
		});
	thread trecv([sockfd] 
		{
			while (true)
			{
				recv_pack(sockfd);
			}
		});
	tsend.join();
	trecv.join();
	return 0;
}


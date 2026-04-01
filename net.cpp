/************************************************************
	Obs³uga sieci - multicast, unicast
*************************************************************/

#define __NET_CPP_
//#include <windows.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <time.h>
//#include <unistd.h>     /* for close() */

#ifdef WIN32// VERSION FOR WINDOWS
//#include <ws2tcpip.h>
//#include <winsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else // VERSION FOR LINUX
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#endif


#include "net.h"

extern FILE *f;
extern bool if_delays;

HANDLE threadSend;

long send_moments[20];       // czasy wyslania ostatnich 20 ramek 
long number_of_sent_frames = 0;          // liczba wyslanych ramek
long czas_start = clock();                // czas od poczatku dzialania aplikacji  
float sr_czestosc = 1.0;           // srednia czestosc wysylania ramek w [1/s]

void DieWithError(char *errorMessage)
{
	perror(errorMessage);
	//exit(1);
}

// 
DWORD WINAPI SendThread(void *ptr)
{
	multicast_net *pmt_net = (multicast_net*)ptr;  // poiner to class multicast_net
	while (1)
	{
		SEND_QUEUE *qn = pmt_net->q, *qn_prv = 0;

		while (qn)                       // while not NULL member of queue 
		{
			if (qn->delay <= clock())   // timeout:  
			{
				int sendsize;
				if (!pmt_net->initS) pmt_net->init_send();
				bool no_problem = ((float)rand() / RAND_MAX > pmt_net->lost_prob);

				if (no_problem)
					if ((sendsize = sendto(pmt_net->sock, qn->buf, qn->size, 0, (struct sockaddr *)
						&pmt_net->multicastAddr, sizeof(pmt_net->multicastAddr))) != qn->size)
						DieWithError("sendto() sent a different number of bytes than expected");

				//return sendsize;
				SEND_QUEUE *do_remove = qn;

				if (qn == pmt_net->q)             // if 1-st member  
				{
					pmt_net->q = qn->next;
					qn = pmt_net->q;
				}
				else
				{
					qn_prv->next = qn->next;
					qn = qn_prv;
				}
				delete do_remove->buf;                   // buffer and queue member are delated
				delete do_remove;
				break;
			}
			qn_prv = qn;
			qn = qn->next;              // next queue member

		} // while(qn)

	}  // while(1)
	return 1;
}


multicast_net::multicast_net(char* ipAdress, unsigned short port)
{

#ifdef WIN32
	if (WSAStartup(MAKEWORD(2, 2), &localWSA) != 0)
	{
		printf("WSAStartup error");
	}
#endif
	// wpisanie adresu i portu ( zgodne juz z formatem )
	multicastIP = inet_addr(ipAdress);
	multicastPort = htons(port);
	multicastTTL = 1;


	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	q = NULL;
	mean_delay = var_delay = 0;
	lost_prob = 0;// 0.3;

	if (if_delays)
		PrepareDelay(0.3, 0.4);

	initS = 0;
	initR = 0;
	printf("socket ok\n");
}

multicast_net::~multicast_net()
{
	//close(sock);
#ifdef WIN32
	WSACleanup(); // ********************* For Windows
#endif
	if (q)
	{
		SEND_QUEUE *qn = q;
		while (q)
		{
			qn = q->next;
			delete q->buf;
			delete q;
			q = qn;
		}
	}
}

int multicast_net::init_recive()
{
	int on = 1;
	fprintf(stderr, "init receiver ");
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));

	if (!initS)
	{
		memset(&multicastAddr, 0, sizeof(multicastAddr));
		multicastAddr.sin_family = AF_INET;
		multicastAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		multicastAddr.sin_port = multicastPort;

		// Bind to the multicast port 
		if (bind(sock, (struct sockaddr *) &multicastAddr, sizeof(multicastAddr)) < 0)
			DieWithError("bind() failed");
	}

	// Specify the multicast group 
	multicastRequest.imr_multiaddr.s_addr = multicastIP;
	// Accept multicast from any interface 
	multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
	// Join the multicast address 
	if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multicastRequest,
		sizeof(multicastRequest)) < 0)
		DieWithError("setsockopt() failed");
	//if (setsockopt(rsock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof (mreq)) != 0)
	fprintf(stderr, " ok\n");
	initR = 1;
	return 1;

}

int multicast_net::init_send()
{
	fprintf(stderr, "init sender");

	// Set TTL of multicast packet 
	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&multicastTTL,
		sizeof(multicastTTL)) < 0)
		DieWithError("setsockopt() failed");


	// Construct local address structure 
	memset(&multicastAddr, 0, sizeof(multicastAddr));
	multicastAddr.sin_family = AF_INET;
	multicastAddr.sin_addr.s_addr = multicastIP;
	multicastAddr.sin_port = multicastPort;

	fprintf(stderr, " ok\n");

	initS = 1;


	return 1;

}

int multicast_net::send(char* buffer, int size)
{
	// average frame sending velocity computation:
	int index_curr = number_of_sent_frames % 20;
	long time_curr = send_moments[index_curr] = clock();
	long time_prev = (index_curr + 1 == 20 ? send_moments[0] : send_moments[index_curr + 1]);
	if (czas_start == 0) czas_start = clock();
	if (number_of_sent_frames > 0)
	{
		int num_w = (number_of_sent_frames >= 20 ? 20 : number_of_sent_frames);
		//sr_czestosc = (float)num_w/((float)(time_curr-time_prev)/CLOCKS_PER_SEC );
		sr_czestosc = (float)number_of_sent_frames / ((float)(time_curr - czas_start) / CLOCKS_PER_SEC);
	}
	else
	{
		for (long i = 0; i<20; i++) send_moments[i] = clock();
	}

	if (sr_czestosc > 1.0) return 0;

	number_of_sent_frames++;


	if ((mean_delay == 0) && (var_delay == 0))
	{
		int sendsize = 0;
		if (!initS) init_send();
		bool no_problem = ((float)rand() / RAND_MAX > lost_prob);
		if (no_problem)
			if ((sendsize = sendto(sock, buffer, size, 0, (struct sockaddr *)
				&multicastAddr, sizeof(multicastAddr))) != size)
				DieWithError("sendto() sent a different number of bytes than expected");

		return sendsize;
	}
	else
	{
		float delay = var_delay * 2 * ((float)rand() / RAND_MAX - 0.5) + mean_delay;  // delay in [sec]
		SEND_QUEUE *new_elem = new SEND_QUEUE;
		new_elem->buf = new char[size];
		memcpy(new_elem->buf, buffer, size);
		new_elem->next = NULL;
		new_elem->prev = NULL;
		new_elem->delay = clock() + (long)((float)delay*CLOCKS_PER_SEC);
		new_elem->size = size;
		//fprintf(f,"czas = %d, umieszczono ramke %d z czasem %d\n",clock(),new_elem,new_elem->delay);
		//fclose(f);
		//f = fopen("plik.txt","a");
		if (q == NULL)
			q = new_elem;
		else
		{
			SEND_QUEUE *qn = q;
			while (qn->next)
				qn = qn->next;
			qn->next = new_elem;
		}

	}


	return 1;
}

void multicast_net::PrepareDelay(float mean, float var)
{
	mean_delay = mean;
	var_delay = var;

	// thread for sending messages from queue
	DWORD dwThreadId;
	threadSend = CreateThread(
		NULL,                        // no security attributes
		0,                           // use default stack size
		SendThread,                // thread function
		(void *)this,               // argument to thread function
		0,                           // use default creation flags
		&dwThreadId);                // returns the thread identifier
}

int multicast_net::reciv(char* buffer, int maxsize)
{
	int recvLen;

	if (!initR) init_recive();


	if ((recvLen = recvfrom(sock, buffer, maxsize, 0, NULL, 0)) < 0)    DieWithError("recvfrom() failed");

	return recvLen;

}
// #########################################
// ##############   UNICAST  ###############
// #########################################

unicast_net::unicast_net(unsigned short usPort)
{
	printf("UDP socket ");
#ifdef WIN32
	if (WSAStartup(MAKEWORD(2, 2), &localWSA) != 0)
	{
		printf("WSAStartup error");
	}
#endif
	/* Create a datagram/UDP socket */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket() failed");

	memset(&udpServAddr, 0, sizeof(udpServAddr));   /* Zero out structure */
	udpServAddr.sin_family = AF_INET;                /* Internet address family */
	udpServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
	udpServAddr.sin_port = usPort;      /* Local port */

	if (bind(sock, (struct sockaddr *) &udpServAddr, sizeof(udpServAddr)) < 0)
		DieWithError("bind() failed");
	printf("OK\n");
}

unicast_net::~unicast_net()
{
	//close(sock);
#ifdef WIN32
	WSACleanup(); // ********************* For Windows
#endif
}


int unicast_net::send(char *buffer, char *servIP, unsigned short size)
{
	udpServAddr.sin_addr.s_addr = inet_addr(servIP);

	if (sendto(sock, buffer, size, 0, (struct sockaddr *)
		&udpServAddr, sizeof(udpServAddr)) == size) return size;
	else DieWithError("sendto() sent a different number of bytes than expected");
}

int unicast_net::send(char *buffer, unsigned long servIP, unsigned short size)
{
	udpServAddr.sin_addr.s_addr = servIP;

	if (sendto(sock, buffer, size, 0, (struct sockaddr *)
		&udpServAddr, sizeof(udpServAddr)) == size) return size;
	else DieWithError("sendto() sent a different number of bytes than expected");
}


int unicast_net::reciv(char *buffer, unsigned long *IP_Sender, unsigned short size)
{
	int ucleng = sizeof(udpClntAddr);

	if ((sSize = recvfrom(sock, buffer, size, 0,
		(struct sockaddr *) &udpClntAddr, (socklen_t*)&ucleng)) < 0)
		DieWithError("recvfrom() failed ");
	else
	{
		*IP_Sender = udpClntAddr.sin_addr.s_addr;
		return sSize;
	}
}

// end

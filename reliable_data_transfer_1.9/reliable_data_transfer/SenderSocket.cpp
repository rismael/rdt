/*
Name: Ismael Rodriguez
Class: CSCE 463-500
Semester: Fall 2019
*/
#include "pch.h"
float SenderSocket::calculate_estimated_rtt(float sample_rtt)
{
	float a = 0.125;
	estimated_rtt = a * sample_rtt + (1 - a) * estimated_rtt;
	return estimated_rtt;
}

float SenderSocket::calculate_dev_rtt(float sample_rtt)
{
	float b = 0.25;
	dev_rtt = b * abs(sample_rtt - estimated_rtt) + (1 - b) * dev_rtt;
	return dev_rtt;
}

float SenderSocket::calculate_rto()
{
	rto = estimated_rtt+ 4 * max(dev_rtt, 0.010);
	return rto;
}

int SenderSocket::Open(char* host, unsigned int port, int window, LinkProperties* lp)
{
	start = clock();
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);
	//API handler
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		printf("WSAStartup error %d\n", WSAGetLastError());
		return -1;
	}
	//create socket
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET)
	{
		printf("socket() error %d\n", WSAGetLastError());
		return -1;
	}
	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(0);
	if (bind(sock, (struct sockaddr*) & local, sizeof(local)) == SOCKET_ERROR)
	{
		// handle errors
		printf("bind() error %d\n", WSAGetLastError());
		return -1;
	}
	Flags flags_;
	flags_.SYN = 1;
	SenderDataHeader sender_data_header;
	sender_data_header.flags = flags_;
	sender_data_header.seq = 0;
	SenderSynHeader sender_syn_header;
	sender_syn_header.sdh = sender_data_header;
	lp->buffer_size = window + 3;
	sender_window = 1;
	sender_syn_header.lp = *lp;
	SenderSynHeader* packet = &sender_syn_header;

	//in_addr used to contain address information
	struct in_addr addr;
	//inet_addr used for storing ip-address
	DWORD address = inet_addr(host);
	addr.s_addr = address;
	//if inet_addr() fails use gethostbyname()
	if (address == INADDR_NONE)
	{
		struct hostent* remote = gethostbyname(host);
		//API hanlder
		if (remote == NULL) {
			int error = WSAGetLastError();
			/*
			if (error != NULL) {
				if (error == WSAHOST_NOT_FOUND) {
					printf("host not found\n");
					return INVALID_NAME;
				}
				else if (error == WSANO_DATA) {
					printf("no data record found\n");
					return -1;
				}
				else {
					printf("function failed with error: %ld\n", error);
					return -1;
				}
			}
			*/
			printf("Target %s is invalid\n", host);
			return INVALID_NAME;
		}
		//add ip-addresses to addr
		int i = 0;
		while (remote->h_addr_list[i] != 0) {
			address = (DWORD)remote->h_addr_list[i];
			addr.s_addr = *(DWORD*)remote->h_addr_list[i];
			//printf("\tIP Address: %s\n", inet_ntoa(addr));
			i++;
		}
	} 
	remote.sin_family = AF_INET; // IPv4
	remote.sin_addr = addr; // from inet_addr or gethostbyname
	remote.sin_port = htons(MAGIC_PORT); // port #
	rtt_start = clock();
	for (int i = 0; i < MAX_SYN_ATTEMPTS; i++)
	{
		//printf("[ %.3f] --> SYN %lu (attempt %d of 3, RTO %.3f) to %s\n", (rtt_start) / (double)CLOCKS_PER_SEC, packet->sdh.seq, i + 1, rto, inet_ntoa(remote.sin_addr));
		if (sendto(sock, (char*)packet, sizeof(SenderSynHeader), 0, (struct sockaddr*) & remote, sizeof(remote)) == SOCKET_ERROR)
		{
			// handle errors
			printf("Failed sendto()\n", WSAGetLastError());
			return FAILED_SEND;
		}
		float usec = (rto - int(rto)) * 1000000;
		struct timeval timeout;
		timeout.tv_sec = (int)rto;
		timeout.tv_usec = usec;
		fd_set fd;
		FD_ZERO(&fd); // clear the set
		FD_SET(sock, &fd); // add your socket to the set
		int available = select(0, &fd, NULL, NULL, &timeout);
		if (available > 0)
		{
			break;
		}
		else if (available == 0 && i != 2) continue;
		else
		{
			return TIMEOUT;
		}
	}
	char* recv_buf = new char[sizeof(ReceiverHeader)];
	struct sockaddr_in response;
	int response_size = sizeof(response);
	int result = 0;
	result = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*) & response, &response_size);
	if (result == SOCKET_ERROR)
	{
		// handle errors
		rtt_start = clock() - start;
		printf("Failed recvfrom with %d\n", WSAGetLastError());
		return FAILED_RECV;
	}
	ReceiverHeader* response_packet = (ReceiverHeader*)recv_buf;
	rtt_end = clock();
	rtt = (rtt_end - rtt_start) / (double)CLOCKS_PER_SEC;
	estimated_rtt = rtt;
	//printf("RTT %.3f, EST %.3f, DEV %.3f, RTO %.3f\n", rtt, estimated_rtt, dev_rtt, rto);
	//printf("[ %.3f] <-- SYN-ACK %lu window %lu; setting initial RTO to %.3f\n", rtt_end / (double)CLOCKS_PER_SEC, response_packet->ackSeq, response_packet->recvWnd,  rto);
	
	return STATUS_OK;
}
int SenderSocket::Send(char* data_buf, int data_size) 
{
	//printf("Sending Data Packet...");
	Flags flags_;
	SenderDataHeader sender_data_header;
	sender_data_header.flags = flags_;
	sender_data_header.seq = next_seq;
	DataPacket pkt = DataPacket();
	pkt.sdh = sender_data_header;
	memcpy(pkt.data_buffer, data_buf, data_size);
	DataPacket* data_packet = &pkt;
	rtt_start = clock();
	if(first_data_packet)
	{
		transfer_start = clock();
		first_data_packet = false;
	}
	bool was_retransmitted = false;
	//printf("Packet Seq: %d, RTO: %f\n", data_packet->sdh.seq, rto);
	for (int i = 0; i < MAX_ATTEMPTS; i++)
	{
		//printf("Attempt %d", i);
		//printf("[ %.3f] --> %lu (attempt %d of 5, RTO %.3f)\n", clock() / (float)CLOCKS_PER_SEC, data_packet->sdh.seq, i+1, rto);
		if (sendto(sock, (char*)data_packet, sizeof(SenderDataHeader) + data_size, 0, (struct sockaddr*) & remote, sizeof(remote)) == SOCKET_ERROR)
		{
			// handle errors
			printf("Failed sendto() with %d\n", WSAGetLastError());
			return FAILED_SEND;
		}
		//printf("...sent\n");
		recv_again:
		float usec = (rto - int(rto)) * 1000000;
		struct timeval timeout;
		timeout.tv_sec = (int)rto;
		timeout.tv_usec = usec;
		fd_set fd;
		FD_ZERO(&fd); // clear the set
		FD_SET(sock, &fd); // add your socket to the set
		int available = select(0, &fd, NULL, NULL, &timeout);
		//printf("Available: %d\n", available);
		if (available > 0)
		{
			//printf("Packet Received...\n");
			if (available > 0)
			{
				//printf("Packet Received...\n");
				sender_base++;
				char* recv_buf = new char[sizeof(ReceiverHeader)];
				struct sockaddr_in response;
				int response_size = sizeof(response);
				int result = 0;

				result = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*) & response, &response_size);
				if (result == SOCKET_ERROR)
				{
					// handle errors
					//rtt_start = clock() - start;
					printf("Failed recvfrom with %d\n", WSAGetLastError());
					return FAILED_RECV;
				}
				//acked_data += result;
				rtt_end = clock();
				float sample_rtt = (rtt_end - rtt_start) / (float)CLOCKS_PER_SEC;
				ReceiverHeader* response_packet = (ReceiverHeader*)recv_buf;
				//printf("Data Packet receieved...%lu\n", response_packet->ackSeq);
				DWORD current_seq = next_seq;
				if (current_seq == response_packet->ackSeq)
				{
					//printf("Duplicate...\n");
					//printf("[ %.3f] <-- DUP ACK %lu (RTT = %.3f, setting RTO %.3f)\n", clock() / (float)CLOCKS_PER_SEC, response_packet->ackSeq, sample_rtt, rto);
					goto recv_again;
				}
				else
				{
					next_seq = response_packet->ackSeq;
				}
				if (!was_retransmitted)
				{
					//printf("RTT %.3f, EST %.3f, DEV %.3f, RTO %.3f\n", rtt, estimated_rtt, dev_rtt, rto);
					calculate_estimated_rtt(sample_rtt);
					//printf("RTT %.3f, EST %.3f, DEV %.3f, RTO %.3f\n", rtt, estimated_rtt, dev_rtt, rto);
					calculate_dev_rtt(sample_rtt);
					//printf("RTT %.3f, EST %.3f, DEV %.3f, RTO %.3f\n", rtt, estimated_rtt, dev_rtt, rto);
					calculate_rto();
					//printf("RTT %.3f, EST %.3f, DEV %.3f, RTO %.3f\n", rtt, estimated_rtt, dev_rtt, rto);
				}

				acked_data += data_size;
				//printf("[ %.3f] <-- ACK %lu (RTT = %.3f, setting RTO %.3f)\n", clock() / (float)CLOCKS_PER_SEC, response_packet->ackSeq, sample_rtt, rto);
				receiver_window = response_packet->recvWnd;
				effective_window_size = min(sender_window, receiver_window);
				transfer_end = clock();

				return 0;
			}
			break;
		}
		else if (available == 0 && i != MAX_ATTEMPTS - 1)
		{
			//printf("Packet Timeout...\n");
			//printf("[ %.3f] timeout, retx pkt %d, (RTO = %.3f)\n", clock() / (float)CLOCKS_PER_SEC, data_packet->sdh.seq, rto);
			packet_timeouts++;
			was_retransmitted = true;
			continue;
		}
		else
		{
			//printf("TIMEOUT");
			return TIMEOUT;
		}
	}
}
int SenderSocket::Close() 
{
	Flags flags_;
	flags_.FIN = 1;
	SenderDataHeader sender_data_header;
	sender_data_header.flags = flags_;
	sender_data_header.seq = next_seq;
	SenderSynHeader sender_syn_header;
	sender_syn_header.sdh = sender_data_header;
	SenderSynHeader* packet = &sender_syn_header;
	for (int i = 0; i < MAX_ATTEMPTS; i++)
	{
		//printf("[ %.3f] --> FIN %lu (attempt %d of 5, RTO %.3f)\n", (clock() - start) / (double)CLOCKS_PER_SEC, packet->sdh.seq, i + 1, rto);
		if (sendto(sock, (char*)packet, sizeof(SenderSynHeader), 0, (struct sockaddr*) & remote, sizeof(remote)) == SOCKET_ERROR)
		{
			// handle errors
			printf("Failed sendto with %d\n", WSAGetLastError());
			return FAILED_SEND;
		}
		float usec = (rto - int(rto)) * 1000000;
		struct timeval timeout;
		timeout.tv_sec = (int)rto;
		timeout.tv_usec = usec;
		fd_set fd;
		FD_ZERO(&fd); // clear the set
		FD_SET(sock, &fd); // add your socket to the set
		int available = select(0, &fd, NULL, NULL, &timeout);
		if (available > 0)
		{
			break;
		}
		else if (available == 0 && i != MAX_ATTEMPTS-1) continue;
		else
		{
			return TIMEOUT;
		}
	}
	char* recv_buf = new char[sizeof(ReceiverHeader)];
	struct sockaddr_in response;
	int response_size = sizeof(response);
	int result = 0;
	result = recvfrom(sock, recv_buf, sizeof(ReceiverHeader), 0, (struct sockaddr*) & response, &response_size);
	if (result == SOCKET_ERROR)
	{
		// handle errors
		printf("Failed recvfrom with %d\n", WSAGetLastError());
		return FAILED_RECV;
	}
	ReceiverHeader* response_packet = (ReceiverHeader*)recv_buf;
	printf("[ %.3f] <-- FIN-ACK %lu window %X\n", (clock() - start) / (double)CLOCKS_PER_SEC, response_packet->ackSeq, response_packet->recvWnd);

	return STATUS_OK;
}
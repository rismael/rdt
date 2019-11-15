/*
Name: Ismael Rodriguez
Class: CSCE 463-500
Semester: Fall 2019
*/
#include "pch.h"
#pragma once
// SenderSocket.h
#define MAGIC_PORT 22345 // receiver listens on this port
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver 


class DataPacket {
public:
	SenderDataHeader sdh;
	char* data_buffer[MAX_PKT_SIZE];
};


class SenderSocket {
private:
	struct sockaddr_in remote;
	SOCKET sock;
	clock_t start;

	float calculate_estimated_rtt(float sample_rtt);
	float calculate_dev_rtt(float sample_rtt);
	float calculate_rto();
	
public:
	float rtt_start = 0;
	float rtt_end = 0;
	float rtt = 0;
	float rto = 1.000;
	float estimated_rtt = 0;
	float dev_rtt = 0;
	float transfer_start = 0;
	float transfer_end = 0;
	DWORD sender_base = 0;
	float acked_data = 0;
	DWORD next_seq = 0;
	int packet_timeouts = 0;
	int fast_retransmissions = 0;
	int effective_window_size = 0;
	int sender_window = 0;
	int receiver_window = 0;
	bool first_data_packet = true;



	int Open(char* host, unsigned int port, int window, LinkProperties* lp);
	int Send(char* data_buf, int data_size);
	int Close();
};
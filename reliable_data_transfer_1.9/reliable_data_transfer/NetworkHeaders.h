/*
Name: Ismael Rodriguez
Class: CSCE 463-500
Semester: Fall 2019
*/
#include "pch.h"
#pragma once
#define MAGIC_PROTOCOL 0x8311AA
#define FORWARD_PATH 0
#define RETURN_PATH 1 

#pragma pack(push, 1)
class Flags {
public:
	DWORD reserved : 5; // must be zero
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
	Flags()
	{
		memset(this, 0, sizeof(*this));
		magic = MAGIC_PROTOCOL;
	}
};

class SenderDataHeader {
public:
	Flags flags;
	DWORD seq; // must begin from 0
};

class SenderSynHeader {
public:
	SenderDataHeader sdh;
	LinkProperties lp;
};

class ReceiverHeader {
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in pkts)
	DWORD ackSeq; // ack value = next expected sequence
};
#pragma pack(pop)
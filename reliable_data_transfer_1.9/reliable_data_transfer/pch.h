/*
Name: Ismael Rodriguez
Class: CSCE 463-500
Semester: Fall 2019
*/
#pragma once

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel
#define MAX_SYN_ATTEMPTS 3
#define MAX_ATTEMPTS 5

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <math.h>
#include <chrono>
#define CLOCKS_PER_MS (CLOCKS_PER_SEC / 1000)
#include "windows.h"
#pragma comment(lib, "Ws2_32.lib")

#include "LinkProperties.h"
#include "NetworkHeaders.h"
#include "SenderSocket.h"
#include "Checksum.h"





/*
Name: Ismael Rodriguez
Class: CSCE 463-500
Semester: Fall 2019
*/
#include "pch.h"
struct stat_arg
{
	HANDLE event_;
	SenderSocket* sender;
	float* total_speed;
	float* number_of_points;

};

UINT stat_thread_function(void* arg)
{
	stat_arg* args = (stat_arg*)arg;
	float previous_base_num = args->sender->next_seq;
	while (true)
	{

		DWORD wait_err = WaitForSingleObject(args->event_, 2000);
		//Print details once 2 seconds pass
		if (wait_err == WAIT_TIMEOUT)
		{
			float current_base_num = args->sender->next_seq;
			float speed = ((current_base_num - previous_base_num) * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader)))/(1e6 * 2);
			
			printf("[%3d] B\t\t%d ( %.1f MB) N\t\t%d T %d F %d W %d S %.3f Mbps RTT %.3f\n",
				clock() / CLOCKS_PER_SEC,
				args->sender->sender_base,
				args->sender->acked_data / (float)1e6,
				args->sender->next_seq + 1,
				args->sender->packet_timeouts,
				args->sender->fast_retransmissions,
				args->sender->effective_window_size,
				speed,
				args->sender->estimated_rtt
				);
			
			previous_base_num = current_base_num;
			*(args->total_speed) += speed;
			*(args->number_of_points) += 1;
			
		}
		//Print final details when exiting event
		else if (wait_err == WAIT_OBJECT_0)
		{
			//printf("Event: exiting...\n");
			return 1;
		}
		else if (wait_err == WAIT_ABANDONED)
		{
			printf("WaitForSingleObject: WAIT_ABANDONED\n");
			exit(-1);
		}
		else if (wait_err == WAIT_FAILED)
		{
			printf("WaitForSingleObject: WAIT_FAILED, %d\n", GetLastError());
			exit(-1);
		}
	}
}

int main(unsigned int argc, char* arg[]) {
	if (argc != 8)
	{
		printf("Invalid amount of arguments\n");
		return -1;
	}

	char* target_host = arg[1];
	int power = atoi(arg[2]);
	int sender_window = atoi(arg[3]);
	float rtt = atof(arg[4]);
	float forward_loss_rate = atof(arg[5]);
	float return_loss_rate = atof(arg[6]);
	int bottleneck_speed = atoi(arg[7]);
	HANDLE stat_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!stat_event)
	{
		printf("CreateEvent failed: %d\n", GetLastError());
		exit(-1);
	}

	printf("Main:\t sender W = %d, RTT %.3f sec, loss %g / %g, link %d Mbps\n", sender_window, rtt, forward_loss_rate, return_loss_rate, bottleneck_speed);

	UINT64 dword_buf_size = (UINT64)1 << power;
	DWORD* dword_buf = new DWORD[dword_buf_size]; // user-requested buffer
	printf("Main:\t initializing DWORD array with 2^%d elements... ", power);
	clock_t start = clock();
	for (UINT64 i = 0; i < dword_buf_size; i++) // required initialization
	{
		dword_buf[i] = i;
	}
	printf("done in %d ms\n", (clock() - start) / CLOCKS_PER_MS);
	SenderSocket ss; // instance of your class 
	SenderSocket* ss_data = &ss;
	stat_arg stat_thread_arg;
	float speed = 0;
	float points = 0;
	stat_thread_arg.event_ = stat_event;
	stat_thread_arg.sender = &ss;
	stat_thread_arg.total_speed = &speed;
	stat_thread_arg.number_of_points = &points;
	HANDLE stat_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)stat_thread_function, &stat_thread_arg, 0, NULL);
	int status = 0;
	LinkProperties lp;
	lp.RTT = rtt;
	lp.speed = 1e6 * bottleneck_speed; // convert to megabits
	lp.pLoss[FORWARD_PATH] = forward_loss_rate;
	lp.pLoss[RETURN_PATH] = return_loss_rate;
	if ((status = ss.Open(target_host, MAGIC_PORT, sender_window, &lp)) != STATUS_OK)
	{
		// error handling: print status and quit
		printf("Main:\t connect failed with status %d\n", status);
		return -1;
	}
	else
	{
		printf("Main:\t connected to %s in %.3f sec, pkt size %d bytes\n", target_host, ss_data->rtt, MAX_PKT_SIZE);
	}
	char* char_buf = (char*)dword_buf; // this buffer goes into socket
	UINT64 byte_buffer_size = dword_buf_size << 2; // convert to bytes
	UINT64 off = 0; // current position in buffer
	while (off < byte_buffer_size)
	{
		// decide the size of next chunk
		int bytes = min(byte_buffer_size - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));
		// send chunk into socket
		if ((status = ss.Send(char_buf + off, bytes)) != STATUS_OK)
		{
			// error handing: print status and quit
			printf("Main:\t send failed with status %d\n", status);
			return -1;
		}
		off += bytes;
	}
	SetEvent(stat_event);	//Signals event to exit
	CloseHandle(stat_event);	//Close event
	CloseHandle(stat_thread);	//Close stat thread	
	if ((status = ss.Close()) != STATUS_OK)
	{
		// error handing: print status and quit 
		printf("Main:\t connect failed with status %d\n", status);
		return -1;
	}
	else
	{
		Checksum chksum;
		printf("Main:\t transfer finished in %.3f sec, %.2f Kbps, checksum %X\n",
			(ss_data->transfer_end - ss_data->transfer_start) / (double)CLOCKS_PER_SEC,
			(*stat_thread_arg.total_speed * 1e3)/ *stat_thread_arg.number_of_points,
			chksum.CRC32((unsigned char*)char_buf, byte_buffer_size)
		);
		printf("Main:\t estRTT %.3f, ideal rate %.2f Kbps\n",
			ss_data->estimated_rtt,
			(sender_window * 8 * (MAX_PKT_SIZE - sizeof(SenderDataHeader))) / (1e3 * ss_data->estimated_rtt)
			);
	}
	return 0;
}
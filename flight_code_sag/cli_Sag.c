#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include "file_io_Sag.h"
#include "cli_Sag.h"
#include "gps.h"
#include <math.h>
int exiting = 0;
int spec_running = 0;
int spec_479khz = 0; // Flag to track if 479kHz spectrometer is running
int spec_120khz = 0; // Flag to track if 120kHz spectrometer is running
pid_t python_pid;
enum commands input;
Packet pkt;
Packet fb;



bool verify_packet(const Packet* pkt) {
	// 1. Check start and end markers
	if (pkt->start != START_BYTE || pkt->end != END_BYTE) {
		printf("Invalid start/end byte\n");
		return false;
	}

	// 2. Validate number of data points
	if (pkt->num_data > MAX_DATA) {
		printf("Invalid num_data: %d\n", pkt->num_data);
		return false;
	}

	// 3. Compute expected checksum
	size_t header_size = 9 + pkt->num_data * sizeof(int16_t); // fixed + data
	uint8_t expected = compute_checksum((uint8_t*)pkt, header_size);

	if (expected != pkt->checksum) {
		printf("ERROR: Checksum mismatch: expected 0x%02X, got 0x%02X\n", expected, pkt->checksum);
		return false;
	}

	return true;
}



//send the packet to the server code
void send(Packet pkt){

}

//listen for feedback from the server code
Packet listen(){

	//return a packet
	return pkt;
}



//XOR checksum
uint8_t compute_checksum(const uint8_t* bytes, const size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum ^= bytes[i];
    }
    return sum;
}

//create packets for transmission
void create_packet(Packet* pkt, const uint8_t cmd1, const int16_t* data, const uint8_t small_len, const double_t* bigpack, const uint8_t big_len, const uint8_t dest) {
    pkt->start = START_BYTE;
    pkt->num_data = small_len;		//payload length
	pkt->destination = dest;
    pkt->cmd_primary = cmd1;
	pkt->utc = (uint32_t)time(NULL);
    memcpy(pkt->data, data, small_len * sizeof(int16_t));
	memcpy(pkt->bigpack, bigpack, big_len * sizeof(double_t));
    const size_t base_size = 1 + 1 + 1 + 1 + + 1 + 4 + small_len * sizeof(int16_t) + big_len * sizeof(double_t); // fields before checksum
    const uint8_t* byte_ptr = (uint8_t*)pkt;
    pkt->checksum = compute_checksum(byte_ptr, base_size);
    pkt->end = END_BYTE;
}



void run_python_script(const char* script_name, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {
    char interval_str[20];
    snprintf(interval_str, sizeof(interval_str), "%d", data_save_interval);
    execlp("python3", "python3", script_name, hostname, logpath, mode, "-i", interval_str, "-p", data_save_path, (char*)NULL);
    perror("execlp failed");
    exit(1);
}

void exec_command(uint8_t input, FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {

/*
    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            printf("print is missing argument usage is print <string>\n");
        } else {
            printf("%s\n", arg);
        }
    }
*/
     if (input == exit_both) {
        //printf("Exiting BCP\n");
        if (spec_running) {
            spec_running = 0;
            kill(python_pid, SIGTERM);
            waitpid(python_pid, NULL, 0);
           // printf("Stopped spec script\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
        }
        if (gps_is_logging()) {
            gps_stop_logging();
            //printf("Stopped GPS logging\n");
            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
        }
        exiting = 1;
    } else if (input == start_spec) {
	if (!spec_running) {
                spec_running = 1;
		 int16_t payload[1];
                // Check if we're starting the 120khz version
                if (pkt.data[0] == 1) {
                    spec_120khz = 1;
                    spec_479khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process for 120khz spectrometer
                        run_python_script("rfsoc_spec_120khz.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        //perror("fork failed");
			payload[0] = 0;
                        spec_running = 0;
                        spec_120khz = 0;
                    } else {
                        // Parent process
                        //printf("Started 120kHz spec script\n");
			payload[0] = 1;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started 120kHz spec script");
                    }
                }
                // Check if we're starting the 479khz version
                else if (pkt.data[0] == 2) {
                    spec_479khz = 1;
                    spec_120khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process for 479khz spectrometer
                        run_python_script("rfsoc_spec_479khz.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        //perror("fork failed");
			payload[0] = 0;
                        spec_running = 0;
                        spec_479khz = 0;
                    } else {
                        // Parent process
                        //printf("Started 479kHz spec script\n");
			payload[0] = 1;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started 479kHz spec script");
                    }
                } else {
                    // Standard spectrometer (960kHz)
                    spec_479khz = 0;
                    spec_120khz = 0;
                    python_pid = fork();
                    if (python_pid == 0) {
                        // Child process
                        run_python_script("rfsoc_spec.py", logpath, hostname, mode, data_save_interval, data_save_path);
                        exit(0);  // Should never reach here
                    } else if (python_pid < 0) {
                        //perror("fork failed");
			payload[0] = 0;
                        spec_running = 0;
                    } else {
                        // Parent process
                        //printf("Started spec script\n");
			payload[0] = 2;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started spec script");
                    }
		    double_t bigpayload[];
		    create_packet(&pkt, 0, payload, sizeof(payload) / sizeof(payload[0]), big_payload, 0, 0);
		    send(pkt);
		   
                }
            } else {
                //printf("Spec script is already running\n");
		int16_t payload[];
		double_t bigpayload[];
		create_packet(&pkt, 1, payload, 0, big_payload, 0, 0);
		send(pkt);
            }

    } else if (input == start_gps) {
	 uint8_t com = 0;
	 int16_t payload[];
	 double_t big_payload[];
	 if (!gps_is_logging()) {
                if (gps_start_logging()) {
                   // printf("Started GPS logging\n");
		    com = 1;
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Started GPS logging");
                } else {
                    //printf("Failed to start GPS logging\n");
		    com = 0;
                    write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Failed to start GPS logging");
                }
            } else {
                //printf("GPS logging is already active\n");
		com = 2;
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to start GPS logging, but it was already active");
            }
	 create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
	 send(pkt);
a
    } else if (input == stop_spec) {
	uint8_t prim_cmd = 0;
	int16_t payload[3];
	double_t big_payload[];
	if (spec_running) {
                spec_running = 0;
                
                // Send SIGTERM first
                kill(python_pid, SIGTERM);
                
                // Wait for up to 5 seconds for the process to terminate
                int status;
                int timeout = 5;
                while (timeout > 0) {
                    pid_t result = waitpid(python_pid, &status, WNOHANG);
                    if (result == python_pid) {
                        if (spec_479khz) {
                            payload[0] = 1;
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped 479kHz spec script");
                        } else if (spec_120khz) {
                            payload[1] = 1;
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped 120kHz spec script");
                        } else {
                            payload[2] = 1;
                            write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped spec script");
                        }
                        spec_479khz = 0; // Reset the spectrometer type flags
                        spec_120khz = 0;
			prim_cmd = 1;
                        break;
                    } else if (result == -1) {
                        prim_cmd = 2;
                        break;
                    }
                    sleep(1);
                    timeout--;
                }
                
                // If the process hasn't terminated, use SIGKILL
                if (timeout == 0) {
                    kill(python_pid, SIGKILL);
                    waitpid(python_pid, NULL, 0);
		    prim_cmd = 3;
                    if (spec_479khz) {
                        payload[0] = 1;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped 479kHz spec script");
                    } else if (spec_120khz) {
                        payload[1] = 1;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped 120kHz spec script");
                    } else {
                        payload[2] = 1;
                        write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Forcefully stopped spec script");
                    }
                    spec_479khz = 0; // Reset the spectrometer type flags
                    spec_120khz = 0;
                }
            } else {
                prim_cmd = 0;
            }
	    create_packet(&pkt, prim_cmd, payload, sizeof(payload) / sizeof(payload[0]), big_payload, 0, 0);
	    send(pkt);

    } else if (input == stop_gps) {
	uint8_t prim_cmd = 0;
	int16_t payload[];
	double_t big_payload[];
	if (gps_is_logging()) {
                gps_stop_logging();
                prim_cmd = 1;
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Stopped GPS logging");
            } else {
                write_to_log(cmdlog, "cli_Sag.c", "exec_command", "Attempted to stop GPS logging, but it was not active");
            }
	create_packet(&pkt, prim_cmd, payload, 0, big_payload, 0, 0);
	send(pkt);

    } else if (input == gps_status) {
	int16_t payload[2];
	double_t big_payload[];
	payload[0] = gps_is_logging();
	payload[1] = gps_is_status_active();
	create_packet(&pkt, 1, payload, sizeof(payload) / sizeof(payload[0]), big_payload, 0, 0);
	send(pkt);

	//get print gps status output here and transmit
    } else if (input == gps_display) {
	int16_t gps_info[4];
	double_t gps_loc[10];
	gps_data_t data;
	if (gps_get_data(&data)) {
            gps_info[0] = 1;
            if (data.valid_position) {
                gps_info[1] = 1;
                if (data.valid_heading) {
                    gps_info[2] = 1;
                    gps_info[3] = data.year;
                    gps_info[4] = data.month;
                    gps_info[5] = data.day;
                    gps_info[6] = data.hour;
                    gps_info[7] = data.minute;
                    gps_info[8] = data.second;
                    gps_loc[0] = data.latitude;
                    gps_loc[1] = data.longtitude;
                    gps_loc[2] = data.altitude;
                    gps_loc[3] = data.heading;
                /*
                    snprintf(status_buffer, sizeof(status_buffer),
                             "GPS Status: %04d-%02d-%02d %02d:%02d:%02d | Lat: %.6f | Lon:>
                             data.year, data.month, data.day,
                             data.hour, data.minute, data.second,
                             data.latitude, data.longitude, data.altitude,
                             data.heading);
                */
                }else {
                    gps_info[2] = 0;
                    gps_info[3] = data.year;
                    gps_info[4] = data.month;
                    gps_info[5] = data.day;
                    gps_info[6] = data.hour;
                    gps_info[7] = data.minute;
                    gps_info[8] = data.second;
                    gps_loc[0] = data.latitude;
                    gps_loc[1] = data.longtitude;
                    gps_loc[2] = data.altitude;
/*
                    snprintf(status_buffer, sizeof(status_buffer),
                             "GPS Status: %04d-%02d-%02d %02d:%02d:%02d | Lat: %.6f | Lon:>
                             data.year, data.month, data.day,
                             data.hour, data.minute, data.second,
                             data.latitude, data.longitude, data.altitude);
*/
                }
	    } else if (data.valid_heading) {
                gps_info[1] = 2;
                gps_loc[0] = data.heading;
/*
                snprintf(status_buffer, sizeof(status_buffer),
                         "GPS Status: No valid position fix | Heading: %.1f° | Press 'q' t>
                         data.heading);
*/
            } else {
                gps_info[1] = 0;
            }
        } else {
            gps_info[0] = 0;
            clear_line();
        }
	
	create_packet(&pkt, gps_info, sizeof(gps_info)/sizeof(gps_info[0]), gps_loc, sizeof(gps_loc)/sizeof(gps_loc[0]), 0);
	send(pkt);
    } 


     else {
        printf("%s: Unknown command\n", cmd);
    }

}


void cmdprompt(FILE* cmdlog, const char* logpath, const char* hostname, const char* mode, int data_save_interval, const char* data_save_path) {

    while (exiting != 1) {

	pkt = listen();

	if (verify_packet(&pkt)) {
	    input = pkt.cmd_primary;
	    exec_command(input, cmdlog, logpath, hostname, mode, data_save_interval, data_save_path);

	}

    }
}

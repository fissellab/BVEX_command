/*

Last Updated: 2025-06-25, evening

A few things:
1. Add delay between send and listen
2. Decision to be made: Apply start, stop, exit commands to both computers? See line 175 (or about there) for how exit command is currently handled
3. Currently, the code is NOT logging any command, but the architecture to do so has been preserved
4. Although each transmission contains a timestamp, the program currently does not check it after receiving a packet
5. It is assumed that the server (not implemented here) handles the uplink and downlink, the sole responsibility of this program is to pass the packet to the server for transmission, and listen for feedback from the server code

Key ideas:

To issue commands to the balloon, there are three pieces of code: cli_ground, cli_Oph, cli_sag
cli_ground functions in a similar fashion as before it was modified (as far as the user is concerned):
	1. It takes in a string command
	Here is where it differs:
	2. It translates the string command into a number according to the enum list in the .h file
	3. The translated commands, and any subcommands, are packaged into a packet by create_packet (more on this later)
	4. The packet is then passed onto send(), which in turn passes the packet to the server
	5. If the command requires a feedback, the program evokes listen(), which listen for an incoming packet from the server
	6. Once received, it verifies the integrity of the packet (also more later)
	7. Once verified, it either displays the feedback, or does some computation with it before displaying
	8. Upon termination of step 7 (if no loops, the "else if" block should terminate after displaying the feedback), the program is free to listen to another user command

Packet creation, transfer, and verification
	- A packet (any packet) has the following parts, whether a part is used or left empty is up to the type of command/feedback:
		- Start byte, same for all packet, to indicate the beginning of a packet
		- num_data, the number of integer parameters associated with the command itself, NOT associated with the packet (i.e.  not the start byte, checksum, etc)
		- num_data, the number of double parameters associated with the command itself
		- destination, 0 to oph, 1 to sag, 2 to both
		- utc, the time when the packet is created
		- cmd_primary, primary command, as listed in the enum list
		- data[], to hold integers
		- bigpack[], to hold doubles
		- checksum, using xor logic to verify packet integrity
		- end byte, to indicate the end of a packet, same for all packet

	- To transfer a packet
		- It is passed onto a function named send(), the specific of the function has NOT been coded as of the last update
		- The listen() function listens for an incoming packet from the server

	- To verify a packet
		- For each packet creation, a check sum is computed using xor logic
		- Upon receiving a feedback, it computes the checksum of the received packet, then compares it against the received checksum
		- If there is a discrepancy between the computed and the received checksum, the packet is invalid

	IMPORTANT NOTICE
		For each uplinking command, the balloon expects a strict format that varies per command, but in general:
			- cmd_primary is a number in the enum list
			- any integers are transmitted in the integer array. when the balloon opens the packet, it expects to find a specific parameter in a particular spot. This should be documented as comments under each "else if" and as it creates a packet
			- the same goes for parameters as doubles
		For each command from the downlink channel, the ballon expects a strict format that varies per command, but in general:
			- Whenever possible, the cmd_primary has been used to indicate if the subsystem is activated. But this might not be the case for all
			- The program to find specific parameters in specific spots in the integer and/or double arrays, see comments within each "else if"

		Key takeaway: when changing the uplink command format, remember to change the way oph/sag decodes the message. The same applies for when the feedback format from oph/sag is changed. Same logic when creating "else if" for new commands

The ideas above are applicable to the codes in oph and sag, just in the opposite fashion
(i.e. it receives the commands from the onboard uplink server, then creates the necessary feedback packets to be passed on to the onboard downlink server)


GOOD LUCK!
*/



#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cli_ground.h"
#include <curses.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
int exiting = 0;
Packet pkt;
Packet fb;
enum oph_commands com;


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
	printf("Sending packet...\n");
	printf("%d\n %d\n %d\n %d\n %d\n %d\n %d\n", pkt.cmd_primary, pkt.checksum, pkt.start, pkt.end, pkt.data[0], pkt.destination, pkt.utc);

}

//listen for feedback from the server code
Packet listen(){
	//recommend adding a delay here

	printf("Waiting...\n");
	//return a packet
	return pkt;
}

//listen for

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


// This executes the commands, we can simply add commands by adding if statements
void exec_command(char* input) {
    char* arg;
    char* cmd;
    int scan;
    arg = (char*)malloc(strlen(input) * sizeof(char));
    cmd = (char*)malloc(strlen(input) * sizeof(char));
	char* sub_arg = (char*) malloc(strlen(input) * sizeof(char));
	sub_arg[0] = '\0'; // Initialize to empty string
    scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    if (strcmp(cmd, "print") == 0) {
        if (scan == 1) {
            printf("print is missing argument usage is print <string>\n");
        } else {
            printf("%s\n", arg);
        }
    } else if (strcmp(cmd, "exit") == 0) {
        exiting = 1;
        printf("Exiting BCP\n");

        com = exit_both;
        int16_t payload[] = {};
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, 0, big_payload, 0, 2);
        send(pkt);
        printf("Shutting down bvexcam\n"); 
/*
        com = accl_stop;
        int16_t data[] = {};
        uint8_t payload_len = sizeof(data) / sizeof(data[0]);
    	double_t big_data[] = {};
        create_packet(&pkt, com, data, payload_len, big_data, 0, 0);
        //code to pass the pkt to server
    	send(pkt);
*/
        printf("Shutting down accelerometer\n");
        free(arg);
        free(cmd);
        return; // Exit the command loop immediately
    } else if (strcmp(cmd, "bvexcam_status") == 0) {
		//expected feedback format: primary cmd-> system enable,
		//int return payload: 0 mode (1/2/3), 1 auto focus start, 2 auto stop, 3 step, 4 foco pos
    	//double return payload: 0 EXPOSURE, 1 raw time, 2 RA, 3 DEC, 4 FR, 5 IR, 6 PS, 7 ALT, 8 AZ,
        com = bvexcam_status;
        int16_t payload[] = {};
    	double_t big_payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);

    	SCREEN* s;
    	char input = 0;
    	s = newterm(NULL, stdin, stdout);
    	noecho();
    	timeout(500);
		while (input != '\n'){
			send(pkt);
			fb = listen();
			if (verify_packet(&fb)){
				if (fb.cmd_primary == 1){
					if (fb.data[0] == 1) {
						mvprintw(0, 0, "bvexcam mode: Autofocus\n");
						mvprintw(1, 0, "Auto-focus start: %d\n", fb.data[1]);
						mvprintw(2, 0, "Auto-focus stop: %d\n", fb.data[2]);
						mvprintw(3, 0, "Auto-focus step: %d\n", fb.data[3]);
						mvprintw(4, 0, "Current focus position: %d\n", fb.data[4]);
						mvprintw(5, 0, "Exposure Time (msec): %f\n", fb.bigpack[0]);
					} else {
						if(fb.data[0] == 2){
							mvprintw(0, 0, "bvexcam mode: Solving  \b\b\n");
						}else{
							mvprintw(0, 0, "bvexcam mode: Taking Images\n");
						}
						mvprintw(1, 0, "Raw time (sec): %.1f\n", fb.bigpack[1]);
						mvprintw(2, 0, "Observed RA (deg): %lf\n", fb.bigpack[2]);
						mvprintw(3, 0, "Observed DEC (deg): %lf\n", fb.bigpack[3]);
						mvprintw(4, 0, "Field rotation (deg): %f\n", fb.bigpack[4]);
						mvprintw(5, 0, "Image rotation (deg): %lf\n", fb.bigpack[5]);
						mvprintw(6, 0, "Pixel scale (arcsec/px): %lf\n", fb.bigpack[6]);
						mvprintw(7, 0, "Altitude (deg): %.15f\n", fb.bigpack[7]);
						mvprintw(8, 0, "Azimuth (deg): %.15f\n", fb.bigpack[8]);
						mvprintw(9, 0, "Exposure Time (msec): %f\n", fb.bigpack[0]);
					}
				} else {
					printf("BVEXCAM is not enabled.\n");
				}
			}

			input = getch();
		}
    	endwin();
    	delscreen(s);

    } else if (strcmp(cmd, "focus_bvexcam") == 0) {
        com = focus_bvexcam;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
     	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
        //code to pass the pkt to server
    	send(pkt);
		//code to listen for feedback
		fb = listen();
		if (verify_packet(&fb)){
			if (fb.cmd_primary == 0){
				printf("bvexcam is not enabled.\n");
			}
		}

    } else if (strcmp(cmd, "bvexcam_solve_start")==0){
        com = bvexcam_solve_start;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
        //code to pass the pkt to server
		send(pkt);
		//code to listen for feedback
		fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 0){
    			printf("bvexcam is not enabled.\n");
    		}
    	}


    } else if (strcmp(cmd, "bvexcam_solve_stop")==0){
        com = bvexcam_solve_stop;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 0){
    			printf("bvexcam is not enabled.\n");
    		}
    	}


	//if (config.bvexcam.enabled){
	//   all_camera_params.solve_img = 0;
	//}else{
	//   printf("bvexcam is not enabled. \n");
	//}
    } else if (strcmp(cmd, "accl_status") == 0) {
		//expected return: cmd-> system enable,
		//param0-> is running, param1-> num of acc, param2-> sample0, param3->chunk0, param4->start_time0, param5->chunk_start_time0
        com = accl_status;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
        //code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 0){
    			printf("ACCEL is NOT enabled.\n");
    		} else {
    			printf("Accelerometer Status:\n");
    			for (int i = 0; i < fb.data[1]; i++) {
    				printf("Accelerometer %d:\n", i + 1);
    				printf("  Samples received: %d\n", fb.data[i*4 +2]);
    				printf("  Current chunk: %d\n", fb.data[i*4 +3]);
    				printf("  Start time: %d\n", fb.data[i*4 +4]);
    				printf("  Current chunk start time: %d\n", fb.data[i*4 +5]);
    				printf("\n");
				}
    			printf("Accelerometer is %s\n", fb.data[0] == 1 ? "running" : "stopped");
			}
    	}

    } else if (strcmp(cmd, "accl_start") == 0) {
		//expeted feedback format from the command: primary command-> enabled/not enabled, param0-> if running (1/0), param1-> if (accelerometer_init() == 0)
		com = accl_start;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload,0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 1){
    			if (fb.data[0] == 1){
    				printf("Accelerometer is already running.\n");
				} else {
					if (fb.data[1] == 0){
						printf("Accelerometer data collection started.\n");
					} else {
						printf("Failed to start accelerometer data collection.\n");
					}
				}
    		} else {
    			printf("Accelerometer is not enabled.\n");
			}
    	}

    } else if (strcmp(cmd, "accl_stop") == 0) {
    	//expeted feedback format from the command: primary command-> enabled/not enabled, param0-> if running (1/0)
        com = accl_stop;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 1){
    			if (fb.data[0] == 1){
    				printf("Accelerometer data collection is stopped.\n");
    			} else {
    				printf("Accelerometer is not currently running.\n");

    			}
    		} else {
    			printf("Accelerometer is not enabled.\n");
    		}
    	}

    } else if (strcmp(cmd,"motor_start")==0){
    	//expeted feedback format from the command: primary command-> enabled/not enabled, param0-> 1=started, 2->already running, 0=error starting

		com = motor_start;
        int16_t payload[] = {};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
        create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary){
    			if (fb.data[0] == 2){
    				printf("Motor is already running.\n");
    			} else if (fb.data[0] == 1) {
    				printf("Motor is started.\n");
    			} else {
    				printf("Failed to start motor.\n");
    			}
    		} else {
    			printf("Motor is not enabled.\n");
    		}
    	}

    } else if (strcmp(cmd,"motor_stop")==0) {
	    //expeted feedback format from the command: primary command-> enabled/not enabled, param0-> 1=shutdown, 2=already shutdown
    	com = motor_stop;
    	int16_t payload[] = {1, 0, 0};
    	uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
    	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary){
    			if (fb.data[0] == 2){
    				printf("Motor already shutdown\n");
    			} else if (fb.data[0] == 1){
    				printf("Motor shutdown complete\n");
    			}
    		} else {
    			printf("Motor is not enabled.\n");
    		}
    	}
    }

    else if (strcmp(cmd,"motor_control")==0) {
	    SCREEN* s;
    	char c =0;
    	// char* input;
    	// char* cmd;
    	// char* arg;
    	// int motor_i = 0;
    	int inputlen = 0;
    	int ret = 0;

    	s=newterm(NULL,stdin,stdout);
    	noecho();
    	timeout(100);
    	input = (char*)malloc(sizeof(char));
    	mvprintw(33,0,"Command:_");

    	while(!ret) {
    		// motor_i = GETREADINDEX(motor_index);
    		//
    		// print_motor_data();
    		// print_motor_PID();

    		com = print_m_data;
    		int16_t smalload[];
    		double_t big_payload[];
    		create_packet(&pkt, com, smalload, 0, big_payload, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();
    		if (verify_packet(&fb)) {
    			mvprintw(0,0,"Current sent to controller (mA): %d\n",fb.data[0]);
    			mvprintw(1,0,"Motor Current(A): %lf\n",fb.bigpack[0]);
    			mvprintw(2,0,"Motor Status Word: %d\n", fb.data[1]);
    			mvprintw(3,0,"Motor Latched Fault: %d\n",fb.data[2]);
    			mvprintw(4,0,"Motor Status register: %d\n",fb.data[3]);
    			mvprintw(5,0,"Motor Position(deg): %lf\n", fb.bigpack[1]);
    			mvprintw(6,0,"Motor Temperature: %d\n", fb.data[5]);
    			mvprintw(7,0,"Motor Velocity(dps): %lf\n", fb.bigpack[2]);
    			mvprintw(8,0,"Motor Control Word read: %d\n", fb.data[6]);
    			mvprintw(9,0,"Motor Control Word write: %d\n", fb.data[7]);
    			mvprintw(10,0,"Motor network_problem: %d\n", fb.data[8]);

    		}

    		com = print_m_pid;
    		create_packet(&pkt, com, smalload, 0, big_payload, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();
			if (verify_packet(&fb)) {
				mvprintw(11,0,"Motor P: %lf\n",fb.bigpack[0]);
				mvprintw(12,0,"Motor I: %lf\n",fb.bigpack[1]);
				mvprintw(13,0,"Motor D: %lf\n",fb.bigpack[2]);
			}


    		//request axes mode, expected feedback primary_cmd-> 1 = POS, 2 = VEL, double param0 dest/vel
    		com = axe_mode;
    		int16_t payload[];
    		uint8_t data_len = 0;
    		double_t big_payload[];
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();
    		if (verify_packet(&fb)){
    			if (fb.cmd_primary == 1){
    				mvprintw(14,0,"Commanded pointing(deg): %lf\n", fb.bigpack[0]);
    			} else if (fb.cmd_primary == 2) {
    				mvprintw(14,0,"Commanded velocity(dps): %lf\n", fb.bigpack[0]);
    			}else {
    				printf("NO AXES MODE INFORMATION\n");
    			}
    		}

    		//request axes mode, expected feedback primary_cmd-> 1=scanning /0, int param0~2 mode, nscan, scan; float param 0~2 start_el, stop_el, dither_s
    		com = scanning_mode;
    		int16_t scanload[] = {};
    		uint8_t load_len = 0;
    		double_t big_load[] = {};
    		create_packet(&pkt, com, scanload, load_len, big_load, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();

    		if (verify_packet(&fb)){
    			if (fb.cmd_primary == 1){
    				mvprintw(15,0,"Scan mode: %d\n", fb.data[0]);
    				mvprintw(16,0,"Start elevation(deg): %lf\n", fb.bigpack[0]);
    				mvprintw(17,0,"Stop elevation(deg): %lf\n", fb.bigpack[1]);
    				mvprintw(18,0,"Dither speed(dps): %lf\n", fb.bigpack[2]);
    				mvprintw(19,0,"Number of scans: %d\n", fb.data[1]);
    				mvprintw(20,0,"Current Scan: %d\n", fb.data[2]);

    			}else {
    				mvprintw(15,0,"                                      ");
    				mvprintw(16,0,"                                      ");
    				mvprintw(17,0,"                                      ");
    				mvprintw(18,0,"                                      ");
    				mvprintw(19,0,"                                      ");
    				mvprintw(20,0,"                                      ");
    			}
    		}

    		//request susan mode, expected feedback primary_cmd-> if susan enabled  1/0,
    		//int parano = axe mode
    		//double param0-1 get_act_angle, dest_az/vel_az
    		com = susan_mode;
    		int16_t susanload[] = {};
    		uint8_t susan_len = 0;
    		double_t big_susan[] = {};
    		create_packet(&pkt, com, susanload, susan_len, big_susan, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();


    		if (verify_packet(&fb)){
    			if (fb.cmd_primary == 1){
    				mvprintw(21,0,"Lazisusan angle (degrees): %lf\n",fb.bigpack[0]);
    				if (fb.data[0] == 1){
    					mvprintw(22,0,"Lazisusan commanded angle (degrees):%lf\n", fb.bigpack[1]);
    				}else if (fb.data[0] == 1){
    					mvprintw(22,0,"Lazisusan commanded velocity (degrees):%lf\n", fb.bigpack[1]);
    				}
    			}
    		}

    		//request susan mode, expected feedback primary_cmd-> 1=enabled /0,
    		//int param0~4: focus mode(1 or 2), sfp, efp, fs, fp
    		//OR int param0 mode, double param 0~7: raw, ra, dec, fr, ir, ps, alt, az
    		com = motor_control_cam;
    		int16_t camload[] = {};
    		uint8_t cam_len = 0;
    		double_t big_cam[] = {};
    		create_packet(&pkt, com, camload, cam_len, big_cam, 0, 0);
    		//code to pass the pkt to server
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();


    		if (verify_packet(&fb)){
    			if(fb.cmd_primary == 1){
    				if (fb.data[0] == 1) {
    					mvprintw(23, 0, "bvexcam mode: Autofocus\n");
    					mvprintw(24, 0, "Auto-focus start: %d\n", fb.data[1]);
    					mvprintw(25, 0, "Auto-focus stop: %d\n", fb.data[2]);
    					mvprintw(26, 0, "Auto-focus step: %d\n", fb.data[3]);
    					mvprintw(27, 0, "Current focus position: %d\n", fb.data[4]);
    				} else {
    					mvprintw(23, 0, "bvexcam mode: Solving  \b\b\n");
    					mvprintw(24, 0, "Raw time (sec): %.1f\n", fb.bigpack[0]);
    					mvprintw(25, 0, "Observed RA (deg): %lf\n", fb.bigpack[1]);
    					mvprintw(26, 0, "Observed DEC (deg): %lf\n", fb.bigpack[2]);
    					mvprintw(27, 0, "Field rotation (deg): %fdata\n", fb.bigpack[3]);
    					mvprintw(28, 0, "Image rotation (deg): %lf\n", fb.bigpack[4]);
    					mvprintw(29, 0, "Pixel scale (arcsec/px): %lf\n", fb.bigpack[5]);
    					mvprintw(30, 0, "Altitude (deg): %.15f\n", fb.bigpack[6]);
    					mvprintw(31, 0, "Azimuth (deg): %.15f\n", fb.bigpack[7]);
    				}
    			}
    		}



    		c = getch();
    		if((c != '\n') && (c != EOF)){
    			mvprintw(33,8+inputlen,"%c_",c);
    			input[inputlen++] = c;
    			input = (char*)realloc(input,inputlen+1);
    		}else if(c == '\n'){
    			input[inputlen] = '\0';
    			mvprintw(33,8,"_");
    			for(int i = 1; i<inputlen+1; i++){
    				mvprintw(33,8+i," ");
    			}

    			cmd = (char*) malloc(strlen(input) * sizeof(char));
    			arg = (char*) malloc(strlen(input) * sizeof(char));

    			sscanf(input, "%s %[^\t\n]", cmd, arg);

    			if(strcmp(cmd,"exit") == 0){
    				ret = 1;
    			}else if(strcmp(cmd,"gotoenc") == 0){
    				int16_t encload[] = {};
    				double_t big_encload[3];
    				big_encload[2] = atof(arg);
    				sscanf(arg, "%lf,%lf",&big_encload[0],&big_encload[1]);
    				com = gotoenc;
    				uint8_t big_len = sizeof(big_encload) / sizeof(big_encload[0]);
    				create_packet(&pkt, com, encload, 0, big_encload, big_len, 0);
    				//code to pass the pkt to server
					send(pkt);

    			}else if(strcmp(cmd,"encdither") == 0){
    				double_t big_ditherload[3];
    				int16_t ditherload[1];
    				sscanf(arg, "%lf,%lf,%lf,%d",&big_ditherload[0], &big_ditherload[1], &big_ditherload[2], &ditherload[0]);


    				com = encdither;
    				uint8_t small_len = sizeof(ditherload) / sizeof(ditherload[0]);
    				uint8_t large_len = sizeof(big_ditherload) / sizeof(big_ditherload[0]);
    				create_packet(&pkt, com, ditherload, small_len, big_ditherload, large_len, 0);
    				//code to pass the pkt to server
					send(pkt);

    				//Hardcode below to balloon
    				//scan_mode.mode = ENC_DITHER;
    				//scan_mode.scanning = 1;
    			}else if(strcmp(cmd,"enctrack") == 0){
    				double_t big_trackload[2];
    				sscanf(arg,"%lf,%lf",&big_trackload[0],&big_trackload[1]);
    				int16_t small_trackload[] = {};
    				com = enctrack;
    				uint8_t len_bigtrack = sizeof(big_trackload) / sizeof(big_trackload[0]);
    				create_packet(&pkt, com, small_trackload, 0, big_trackload, len_bigtrack, 0);
    				//code to pass the pkt to server
					send(pkt);

    				//Hardcode below to balloon
    				//scan_mode.mode = ENC_TRACK;
    				//scan_mode.scanning = 1;
    			}else if(strcmp(cmd,"enconoff")==0){
    				double_t big_onoffload[4];
    				sscanf(arg,"%lf,%lf,%lf,%lf",&big_onoffload[0],&big_onoffload[1],&big_onoffload[2], &big_onoffload[3]);
    				int16_t small_onoffload[] = {};
    				com = enconoff;
    				uint8_t len_bigonoff = sizeof(big_onoffload) / sizeof(big_onoffload[0]);
    				create_packet(&pkt, com, small_onoffload, 0, big_onoffload, len_bigonoff, 0);
    				//code to pass the pkt to server
					send(pkt);

    			}else if(strcmp(cmd,"stop") == 0){
    				com = stop_scan;
    				int16_t stopload[] = {};
    				double_t b_stopload[] = {};
    				create_packet(&pkt, com, stopload, 0, b_stopload, 0, 0);
    				send(pkt);

    			}else if(strcmp(cmd,"set_offset") == 0){
    				double_t big_offsetload[3];
    				big_offsetload[2] = atof(arg);
    				sscanf(arg,"%lf,%lf",&big_offsetload[0],&big_offsetload[1]);
    				int16_t s_offsetload[] = {};
    				com = setting_offset;
    				uint8_t len_bigoffset = sizeof(big_offsetload) / sizeof(big_offsetload[0]);
    				create_packet(&pkt, com, s_offsetload, 0, big_offsetload, len_bigoffset, 0);
    				//code to pass the pkt to server
    				send(pkt);

    			}

    			input = (char*)malloc(sizeof(char));
    			inputlen = 0;
    		}
    	}
    	endwin();
    	delscreen(s);
    } else if(strcmp(cmd,"lock_tel")==0){
		//expected feedback format: cmd_primary-> system enable, param0 -> lock state
		com = lock;
        int16_t payload[] = {1000};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
    	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	//code to pass the pkt to server
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 1){
    			if (fb.data[0] == 1) {
    				printf("Locked\n");
    			} else if (fb.data[0] == 2){
    				printf("Lock aleady locked\n");
    			} else {
    				printf("Lock did not report locked after 3 seconds\n");
    			}
    		} else {
    			printf("Lockpin is not enabled.\n");
    		}
    	}

    }else if(strcmp(cmd,"unlock_tel")==0){
    	//expected feedback format: cmd_primary-> system enable, param0 -> lock state
	 	com = unlock;
        int16_t payload[] = {1000};
        uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
    	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 1){
    			if (fb.data[0] == 0){
    				printf("Lock did not report unlocked after 3 seconds\n");
    			} else if (fb.data[0] == 1) {
    				printf("Unlocked\n");
    			} else {
    				printf("Lock already unlocked\n");
    			}
    		} else {
    			printf("Lockpin is not enabled.\n");
    		}
    	}
 
    }else if (strcmp(cmd,"reset_tel")==0){
    	//expected feedback format: cmd_primary-> system enable, param0 -> execution completetion state
		com = reset_lock;
		int16_t payload[] = {1000};
		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    	double_t big_payload[] = {};
    	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    	send(pkt);
    	//code to listen for feedback
    	fb = listen();
    	if (verify_packet(&fb)){
    		if (fb.cmd_primary == 1){
    			if (fb.data[0] == 1){
    				printf("Reset complete\n");
    			} else {
					printf("Reset NOT complete\n");
				}
    		} else {
    			printf("Lockpin is not enabled.\n");
    		}
    	}

	//saggitarius commands
    } else if (strcmp(cmd, "gps") == 0) {
    	// Handle GPS sub-commands
    	if (strcmp(arg, "status") == 0) {
			//expected feedback format: primary cmd-> dont care,
			//param0-> gps_is_logging, param1-> gps_is_status_active
    		com = gps_status;
    		int16_t payload[] = {};
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		double_t big_payload[] = {};
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();

    		// Command for GPS status display
    		if (fb.data[0] == 0) {
    			printf("GPS logging is not active. Start GPS logging first.\n");
    		} else if (fb.data[1] == 1) {
    			printf("GPS status display is already active.\n");

    		} else {
    			printf("Starting GPS status display. Press 'q' to exit.\n");

				//gps display here
    			// Save terminal settings
    			struct termios old_term;
    			tcgetattr(STDIN_FILENO, &old_term);
    			struct termios new_term = old_term;
    			new_term.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    			tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    			// Display GPS status (blocks until user exits)


    			// Restore terminal settings
    			tcsetattr(STDIN_FILENO, TCSANOW, &old_term);



    			// Flush any remaining input to prevent interference with CLI
    			fflush(stdin);

    		}
    	} else {
    		printf("Unknown GPS command: %s\n", arg);
    	}
    } else if (strcmp(cmd, "stop_sag") == 0) {
    	sscanf(arg, "%s %s", sub_arg, arg + strlen(sub_arg) + 1);
    	printf("HELLO\n");
        if (strcmp(sub_arg, "spec") == 0) {
            //expected feedback format: primary cmd-> 1 if waitpid work  2 if waitpid failed  3 if process timedout and force-killed,
            //param0-> spec_479khz, param1-> spec_120khz, param2-> stopped spec script
    		com = stop_spec;
    		int16_t payload[] = {};
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        	double_t big_payload[] = {};
        	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();

    		if (fb.cmd_primary == 1){
    		    if (fb.data[0] == 1) {
    		    	printf("Stopped 479kHz spec script\n");
    		    } else if (fb.data[1] == 1) {
    		    	printf("Stopped 120kHz spec script\n");

    		    } else if (fb.data[2] == 1) {
                    printf("Stopped spec script\n");
                }
            } else if ( fb.cmd_primary == 2) {
                printf("waitpid failed\n");
            } else if ( fb.cmd_primary == 3) {
                printf("Force kill triggered\n");
                if (fb.data[0] == 1) {
    		    	printf("Stoped 479kHz spec script\n");
    		    } else if (fb.data[1] == 1) {
    		    	printf("Stopped 120kHz spec script\n");
    		    } else if (fb.data[2] == 1) {
                    printf("Stopped spec script\n");
                }
            } else {
	            printf("Spec script was never started\n");
            }
        } else if (strcmp(sub_arg, "gps") == 0) {
            //expected feedback format: primary cmd-> 1 if gps stopped  0 if gps never active
    		com = stop_gps;
    		int16_t payload[] = {};
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        	double_t big_payload[] = {};
        	create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    		//code to listen for feedback
    		fb = listen();

            if (fb.cmd_primary == 1) {
                printf("Stopped GPS logging\n");
            } else {
                printf("GPS logging is not active\n");
            }
        } else {
            printf("Unknown stop_sag command: %s\n", sub_arg);
        }
	}

    else {
        printf("%s: Unknown command\n", cmd);
    }

    free(arg);
    free(cmd);
}

// This gets an input of arbitrary length from the command line
char* get_input() {
    char* input;
    char c;
    int i = 0;

    input = (char*)malloc(1 * sizeof(char));

    while ((c = getchar()) != '\n' && c != EOF) {
        input[i++] = c;
        input = (char*)realloc(input, i + 1);
    }

    input[i] = '\0';
    return input;
}

// This is the main function for the command line
void cmdprompt() {
    int count = 1;
    char* input;

    while (exiting != 1) {
        printf("[BCP]<%d>$ ", count);
        input = get_input();
        if (strlen(input) != 0) {
            exec_command(input);
        }
        free(input);
        count++;
    }
}

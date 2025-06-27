#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "file_io_Oph.h"
#include "cli_Oph.h"
#include "bvexcam.h"
#include "lens_adapter.h"
#include "astrometry.h"
#include "accelerometer.h"
#include "ec_motor.h"
#include "motor_control.h"
#include "lazisusan.h"
#include "coords.h"


#include <math.h>
#include <time.h>
int exiting = 0;
Packet pkt;
Packet fb;
enum oph_commands input;

int exiting = 0;
extern int shutting_down; // set this to one to shutdown star camera
extern struct camera_params all_camera_params;
extern struct astrometry all_astro_params;
extern AccelerometerData accel_data;
extern int stop;//flag that determines on/off state of motor
extern pthread_t motors;
extern AxesModeStruct axes_mode;
extern ScanModeStruct scan_mode;
extern int motor_index;
extern int ready; //This determines if the motor setup has completed
extern int comms_ok; //This determines if the motor setup was successful
extern SkyCoord target;


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



// This executes the commands, we can simply add commands by adding if statements
void exec_command(enum oph_commands cmd) {
    // char* arg;
    // char* cmd;
    // int scan;
    // arg = (char*)malloc(strlen(input) * sizeof(char));
    // cmd = (char*)malloc(strlen(input) * sizeof(char));
    // scan = sscanf(input, "%s %[^\t\n]", cmd, arg);

    // if (strcmp(cmd, "print") == 0) {
    //     if (scan == 1) {
    //         printf("print is missing argument usage is print <string>\n");
    //     } else {
    //         printf("%s\n", arg);
    //     }
    // } else
	if (cmd == exit_both) {
        exiting = 1;
        // printf("Exiting BCP\n");

        if (config.bvexcam.enabled) {
            shutting_down = 1;
            //printf("Shutting down bvexcam\n");
        }
        
        if (config.accelerometer.enabled) {
            //printf("Shutting down accelerometer\n");
            accelerometer_shutdown();
        }
        //free(arg);
        //free(cmd);
        return; // Exit the command loop immediately

    } else if (cmd == bvexcam_status) {

    	//expected feedback format: primary cmd-> system enable,
    	//int return payload: 0 mode (1/2/3), 1 auto focus start, 2 auto stop, 3 step, 4 foco pos
    	//double return payload: 0 EXPOSURE, 1 raw time, 2 RA, 3 DEC, 4 FR, 5 IR, 6 PS, 7 ALT, 8 AZ,

		int com;
        if (config.bvexcam.enabled) {
        	com = 1;
        	if (all_camera_params.focus_mode == 1) {
        		int8_t payload[5];
        		double_t big_payload[1];
        		payload[0] = all_camera_params.focus_mode;
        		payload[1] = all_camera_params.start_focus_pos;
        		payload[2] = all_camera_params.end_focus_pos;
        		payload[3] = all_camera_params.focus_step;
        		payload[4] = all_camera_params.focus_position;
        		big_payload[0] = all_camera_params.exposure_time;
        		// mvprintw(0, 0, "bvexcam mode: Autofocus\n");
        		// mvprintw(1, 0, "Auto-focus start: %d\n", all_camera_params.start_focus_pos);
        		// mvprintw(2, 0, "Auto-focus stop: %d\n", all_camera_params.end_focus_pos);
        		// mvprintw(3, 0, "Auto-focus step: %d\n", all_camera_params.focus_step);
        		// mvprintw(4, 0, "Current focus position: %d\n", all_camera_params.focus_position);
        		// mvprintw(5, 0, "Exposure Time (msec): %f\n", all_camera_params.exposure_time);
				uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        		uint8_t len_bigdata = sizeof(big_payload) / sizeof(big_payload[0]);
        		create_packet(&pkt, com, payload, data_len, big_payload, len_bigdata, 0);
        	} else {
        		int8_t payload[1];
        		double_t big_payload[9];
        		if(all_camera_params.solve_img){
        			payload[0] = 2;
        		}else{
        			payload[0] = 3;
        		}
        		big_payload[0] = all_camera_params.exposure_time;
        		big_payload[1] = all_astro_params.rawtime;
        		big_payload[2] = all_astro_params.ra;
        		big_payload[3] = all_astro_params.dec;
        		big_payload[4] = all_astro_params.fr;
        		big_payload[5] = all_astro_params.ir;
        		big_payload[6] = all_astro_params.ps;
        		big_payload[7] = all_astro_params.alt;
        		big_payload[8] = all_astro_params.az;
        		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        		uint8_t len_bigdata = sizeof(big_payload) / sizeof(big_payload[0]);
        		create_packet(&pkt, com, payload, data_len, big_payload, len_bigdata, 0);
        		// mvprintw(1, 0, "Raw time (sec): %.1f\n", all_astro_params.rawtime);
        		// mvprintw(2, 0, "Observed RA (deg): %lf\n", all_astro_params.ra);
        		// mvprintw(3, 0, "Observed DEC (deg): %lf\n", all_astro_params.dec);
        		// mvprintw(4, 0, "Field rotation (deg): %f\n", all_astro_params.fr);
        		// mvprintw(5, 0, "Image rotation (deg): %lf\n", all_astro_params.ir);
        		// mvprintw(6, 0, "Pixel scale (arcsec/px): %lf\n", all_astro_params.ps);
        		// mvprintw(7, 0, "Altitude (deg): %.15f\n", all_astro_params.alt);
        		// mvprintw(8, 0, "Azimuth (deg): %.15f\n", all_astro_params.az);
        		// mvprintw(9, 0, "Exposure Time (msec): %f\n", all_camera_params.exposure_time);
        	}


        } else {
        	int16_t payload[];
        	double_t big_payload[];
        	create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
        	send(pkt);
        }
    } else if (cmd == focus_bvexcam) {


        if (config.bvexcam.enabled) {
            all_camera_params.focus_mode = 1;
            all_camera_params.begin_auto_focus = 1;

        	int16_t payload[] = {};
        	double_t big_payload[] = {};
        	create_packet(&pkt, 1, payload, 0, big_payload, 0, 0);
        	send(pkt);
        } else {
        	int16_t payload[] = {};
        	double_t big_payload[] = {};
        	create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
        	send(pkt);
        }
    } else if (cmd == accl_status) {
        if (config.accelerometer.enabled) {
            AccelerometerStatus status;
            accelerometer_get_status(&status);

			int n_accel = config.accelerometer.num_accelerometers;
        	int n_intslot = n_accel * 2 + 2;
        	int n_floatslot = n_accel * 2;
        	int16_t payload[n_intslot];
        	double_t big_payload[n_floatslot];
        	payload[0] = status.is_running;
			payload[1] = n_accel;
        	int slot_int = 2;
        	int slot_double = 0;
        	
            for (int i = 0; n_accel; i++) {
            	payload[slot_int] = status.samples_received[i];
            	slot_int++;
            	payload[i] = status.chunk_numbers[i];
            	slot_int++;
            	big_payload[slot_double] = status.start_times[i];
            	slot_double++;
            	big_payload[slot_double] = status.chunk_start_times[i];
            	slot_double++;
                // printf("Accelerometer %d:\n", i + 1);
                // printf("  Samples received: %ld\n", status.samples_received[i]);
                // printf("  Current chunk: %d\n", status.chunk_numbers[i]);
                // printf("  Start time: %.6f\n", status.start_times[i]);
                // printf("  Current chunk start time: %.6f\n", status.chunk_start_times[i]);
                // printf("\n");
            }
            // printf("Accelerometer is %s\n", status.is_running ? "running" : "stopped");
        	uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        	uint8_t big_payload_len = sizeof(big_payload) / sizeof(big_payload[0]);
        	create_packet(&pkt, 1, payload, data_len, big_payload, big_payload_len, 0);
        	send(pkt);
        } else {
        	int16_t payload[] = {};
        	double_t big_payload[] = {};
        	create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
        	send(pkt);
        }
    } else if (cmd == accl_start) {
        if (config.accelerometer.enabled) {

        	int16_t payload[2];
        	double_t big_payload[] = {};
        	
            AccelerometerStatus status;
            accelerometer_get_status(&status);
            if (status.is_running) {
                // printf("Accelerometer is already running.\n");
            	payload[0] = 1;
            } else {
            	payload[0] = 0;
                if (accelerometer_init() == 0) {
                    // printf("Accelerometer data collection started.\n");
                	payload[1] = 1;
                } else {
                    // printf("Failed to start accelerometer data collection.\n");
                	payload[1] = 0;
                }
            }
        	uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        	create_packet(&pkt, 1, payload, data_len, big_payload,0, 0);
        	//code to pass the pkt to server
        	send(pkt);
        } else {
            // printf("Accelerometer is not enabled in configuration.\n");
        	int16_t payload[] = {};
        	double_t big_payload[] = {};
        	create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
        	send(pkt);
        }
    } else if (cmd == accl_stop) {
        if (config.accelerometer.enabled) {

        	int16_t payload[1];
        	double_t big_payload[] = {};
            AccelerometerStatus status;
            accelerometer_get_status(&status);
            if (status.is_running) {
                accelerometer_shutdown();
                payload[0] = 1;
            } else {
                payload[0] = 0;
            }

        	uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
        	create_packet(&pkt, 1, payload, data_len, big_payload, 0, 0);
        	send(pkt);
        } else {
            // printf("Accelerometer is not enabled in configuration.\n");
        	int16_t payload[] = {};
        	double_t big_payload[] = {};
        	create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
        	send(pkt);
        }
    } else if (cmd == motor_start){
		if(config.motor.enabled){
			int16_t payload[1];
			double_t big_payload[] = {};
		    if(stop){
				stop = 0;
				// printf("Starting motor\n");
				if (start_motor()){
					// printf("Successfully started motor\n");
					payload[0] = 1;
				}else{
					// printf("Error starting motor please see motor log\n");
					payload[0] = 0;
				}
            }else{
                // printf("Motor is already running\n");
            	payload[0] = 2;

            }

			uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
			create_packet(&pkt, 1, payload, data_len, big_payload, 0, 0);
			send(pkt);
			
		}else{
			// printf("Motor is not enabled in configuration.\n");
			int16_t payload[] = {};
			double_t big_payload[] = {};
			create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
			send(pkt);
		}
    } else if (cmd == motor_stop){
		if(config.motor.enabled){
			int16_t payload[0];
			double_t big_payload[] = {};
			if(!stop){
				stop = 1;
				ready = 0;
				comms_ok = 0;
           		// printf("Shutting down motor.\n");
				pthread_join(motors,NULL);
           		// printf("Motor shutdown complete.\n");
				payload[0] = 1;
		    }else{
				// printf("Motor already shutdown.\n");
		    	payload[0] = 2;
		    }
			uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
			create_packet(&pkt, 1, payload, data_len, big_payload, 0, 0);
			send(pkt);
		}else{
		    // printf("Motor is not enabled in configuration.\n");
			int16_t payload[] = {};
			double_t big_payload[] = {};
			create_packet(&pkt, 0, payload, 0, big_payload, 0, 0);
			send(pkt);
		}
    } else if (cmd == axe_mode) {
    	int com;
    	double_t big_payload[1];
    	if(axes_mode.mode == POS){
    		// mvprintw(14,0,"Commanded pointing(deg): %lf\n", axes_mode.dest);
    		com = 1;
    		big_payload[0] = axes_mode.dest;
    	}else if(axes_mode.mode == VEL){
    		com = 2;
    		big_payload[0] = axes_mode.vel;
    	}

    	uint8_t bigdata_len = sizeof(big_payload) / sizeof(big_payload[0]);
    	int8_t payload[] = {};
    	create_packet(&pkt, com, payload, 0, big_payload, bigdata_len, 0);
    	send(pkt);
    } else if (cmd == scanning_mode) {
    	int com;
    	if(scan_mode.scanning){
    		int8_t payload[3];
    		double_t big_payload[3];
    		com = 1;
    		payload[0] = scan_mode.mode;
    		payload[1] = scan_mode.nscans;
    		payload[2] = scan_mode.scan+1;
    		big_payload[0] = scan_mode.start_el;
    		big_payload[1] = scan_mode.stop_el;
    		big_payload[2] = scan_mode.vel;
    		uint8_t bigdata_len = sizeof(big_payload) / sizeof(big_payload[0]);
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, bigdata_len, 0);
    		send(pkt);
    		// mvprintw(15,0,"Scan mode: %d\n", scan_mode.mode);
    		// mvprintw(16,0,"Start elevation(deg): %lf\n", scan_mode.start_el);
    		// mvprintw(17,0,"Stop elevation(deg): %lf\n", scan_mode.stop_el);
    		// mvprintw(18,0,"Dither speed(dps): %lf\n", scan_mode.vel);
    		// mvprintw(19,0,"Number of scans: %d\n", scan_mode.nscans);
    		// mvprintw(20,0,"Current Scan: %d\n", scan_mode.scan+1);
    	}else{
    		com = 0;
    		int8_t payload[] = {};
    		double_t big_payload[] = {};
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}
    } else if (cmd == susan_mode) {
    	int com;
    	if(config.lazisusan.enabled){
    		int8_t payload[1];
    		double_t big_payload[2];
    		com = 1;
			payload[0] = axes_mode.mode;
    		big_payload[0] = get_act_angle();
    		// mvprintw(21,0,"Lazisusan angle (degrees): %lf\n",get_act_angle());
    		if (axes_mode.mode==POS){
    			big_payload[1] = axes_mode.dest_az;
    			// mvprintw(22,0,"Lazisusan commanded angle (degrees):%lf\n",axes_mode.dest_az);
    		}else if (axes_mode.mode==VEL){
    			big_payload[1] = axes_mode.vel_az;
    			// mvprintw(22,0,"Lazisusan commanded velocity (degrees):%lf\n",axes_mode.vel_az);
    		}
    		uint8_t bigdata_len = sizeof(big_payload) / sizeof(big_payload[0]);
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, bigdata_len, 0);
    		send(pkt);
    	} else {
    		com = 0;
    		int8_t payload[] = {};
    		double_t big_payload[] = {};
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}
    } else if (cmd == motor_control_cam) {
    	int com;
    	if(config.bvexcam.enabled){
    		com = 1;
    		if (all_camera_params.focus_mode == 1) {
				int16_t payload[5];
    			double_t big_payload[];
    			payload[0] = all_camera_params.focus_mode;
    			payload[1] = all_camera_params.start_focus_pos;
    			payload[2] = all_camera_params.end_focus_pos;
    			payload[3] = all_camera_params.focus_step;
    			payload[4] = all_camera_params.focus_position;
    			uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    			create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    			send(pkt);

    			// mvprintw(23, 0, "bvexcam mode: Autofocus\n");
    			// mvprintw(24, 0, "Auto-focus start: %d\n", all_camera_params.start_focus_pos);
    			// mvprintw(25, 0, "Auto-focus stop: %d\n", all_camera_params.end_focus_pos);
    			// mvprintw(26, 0, "Auto-focus step: %d\n", all_camera_params.focus_step);
    			// mvprintw(27, 0, "Current focus position: %d\n", all_camera_params.focus_position);
    		} else {
    			int16_t payload[1];
    			double_t big_payload[8];
    			big_payload[0] = all_astro_params.rawtime;
    			big_payload[1] = all_astro_params.ra;
    			big_payload[2] = all_astro_params.dec;
    			big_payload[3] = all_astro_params.fr;
    			big_payload[4] = all_astro_params.ir;
    			big_payload[5] = all_astro_params.ps;
    			big_payload[6] = all_astro_params.alt;
    			big_payload[7] = all_astro_params.az;
				uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    			uint8_t bigdata_len = sizeof(big_payload) / sizeof(big_payload[0]);
    			create_packet(&pkt, com, payload, data_len, big_payload, bigdata_len, 0);
    			send(pkt);

    			// mvprintw(23, 0, "bvexcam mode: Solving  \b\b\n");
    			// mvprintw(24, 0, "Raw time (sec): %.1f\n", all_astro_params.rawtime);
    			// mvprintw(25, 0, "Observed RA (deg): %lf\n", all_astro_params.ra);
    			// mvprintw(26, 0, "Observed DEC (deg): %lf\n", all_astro_params.dec);
    			// mvprintw(27, 0, "Field rotation (deg): %f\n", all_astro_params.fr);
    			// mvprintw(28, 0, "Image rotation (deg): %lf\n", all_astro_params.ir);
    			// mvprintw(29, 0, "Pixel scale (arcsec/px): %lf\n", all_astro_params.ps);
    			// mvprintw(30, 0, "Altitude (deg): %.15f\n", all_astro_params.alt);
    			// mvprintw(31, 0, "Azimuth (deg): %.15f\n", all_astro_params.az);
    		}
    	} else {
    		com = 0;
    		int8_t payload[];
    		double_t big_payload[];
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}

    } else if (cmd == gotoenc) {
    	//verify package
    	scan_mode.scanning = 0;
    	if(config.lazisusan.enabled && config.motor.enabled){
    		double az = pkt.bigpack[0];
    		double el = pkt.bigpack[1];
    		go_to_enc(el);
    		move_to(az);
    	}else if(!config.lazisusan.enabled){
    		go_to_enc(pkt.bigpack[2]);
    	}else if(!config.motor.enabled){
    		move_to(pkt.bigpack[2]);
    	}
    } else if(cmd = encdither){
    	scan_mode.start_el = pkt.bigpack[0];
    	scan_mode.stop_el = pkt.bigpack[1];
    	scan_mode.vel = pkt.bigpack[2];
    	scan_mode.nscans = pkt.data[0];
    	// sscanf(arg, "%lf,%lf,%lf,%d",&scan_mode.start_el, &scan_mode.stop_el, &scan_mode.vel, &scan_mode.nscans);
    	scan_mode.mode = ENC_DITHER;
    	scan_mode.scanning = 1;
    }else if(cmd == enctrack){
    	target.lon = pkt.bigpack[0];
    	target.lat = pkt.bigpack[1];
    	// sscanf(arg,"%lf,%lf",&target.lon,&target.lat);
    	scan_mode.mode = ENC_TRACK;
    	scan_mode.scanning = 1;
    }else if(cmd == enconoff){
    	target.lon = pkt.bigpack[0];
    	target.lat = pkt.bigpack[1];
    	scan_mode.offset = pkt.bigpack[2];
    	scan_mode.time = pkt.bigpack[3];
    	// sscanf(arg,"%lf,%lf,%lf,%lf",&target.lon,&target.lat,&scan_mode.offset,&scan_mode.time);
    	scan_mode.mode = EL_ONOFF;
    	scan_mode.scanning = 1;
    }else if(cmd == stop_scan){
    	scan_mode.scanning = 0;
    	scan_mode.nscans = 0;
    	scan_mode.scan = 0;
    	scan_mode.turnaround = 1;
    	scan_mode.time = 0;
    	scan_mode.on_position = 0;
    	scan_mode.offset = 0;
    	axes_mode.mode = VEL;
    	axes_mode.vel = 0.0;
    	axes_mode.vel_az = 0.0;
    	axes_mode.on_target = 0;
    }else if(cmd == setting_offset) {
	    if(config.lazisusan.enabled && config.motor.enabled){
	    	// sscanf(arg, "%lf,%lf",&az,&el);
	    	set_offset(pkt.bigpack[0]);
	    	set_el_offset(pkt.bigpack[1]);
	    }else if(!config.lazisusan.enabled){
	    	set_el_offset(pkt.bigpack[2]);
	    }else if(!config.motor.enabled){
	    	set_offset(pkt.bigpack[2]);
	    }
    }else if (cmd == print_m_pid){
    	double_t* big_payload = malloc(3 * sizeof(double_t));
    	double_t payload[];
    	print_motor_PID(&big_payload);
    	uint8_t len_payload = 0;
    	uint8_t len_big_payload = 3;
    	create_packet(&pkt, 1, payload, len_payload, big_payload, len_big_payload, 0);
    	send(pkt);

    } else if (cmd == print_m_data) {
    	double_t* big_payload = malloc(3 * sizeof(double_t));
    	int16_t * payload = malloc(8 * sizeof(int16_t));
    	print_motor_data(&big_payload, &payload);
    	uint8_t len_payload = 8;
    	uint8_t len_big_payload = 3;
    	create_packet(&pkt, 1, payload, len_payload, big_payload, len_big_payload, 0);
    	send(pkt);
    }else if (cmd == lock) {
    	int com;
    	if(config.lockpin.enabled&& !is_locked){
    		com = 1;
    		int8_t payload[1];
    		lock_tel = 1;
    		sleep(3);
    		if(is_locked){
    			payload[0] = 1;
    		} else {
    			payload[0] = 0;
    		}
    		double_t big_payload[];
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    	}else if(config.lockpin.enabled && is_locked){
    		com = 1;
    		int8_t payload[1];
    		payload[0] = 2;
    		double_t big_payload[];
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    	}else{
    		com = 0;
    		int8_t payload[];
    		double_t big_payload[];
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}


    } else if (cmd == unlock) {
    	int com;
    	if(config.lockpin.enabled && is_locked){
    		com = 1;
    		int8_t payload[1];
    		unlock_tel = 1;
    		sleep(3);
    		if(!is_locked){
    			payload[0] = 1;
    		} else {
    			payload[0] = 0;
    		}
    		double_t big_payload[];
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    	}else if(config.lockpin.enabled && !is_locked){
    		com = 1;
    		int8_t payload[1];
    		payload[0] = 2;
    		double_t big_payload[];
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    	}else{
    		com = 0;
    		int8_t payload[];
    		double_t big_payload[];
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}

    } else if (cmd == reset_lock) {
    	int com;
    	if(config.lockpin.enabled){
    		com = 1;
    		int8_t payload[1];
    		reset = 1;
    		sleep(3);
    		if(!reset){
    			payload[0] = 0;
    		} else {
    			payload[0] = 1;
    		}
    		double_t big_payload[];
    		uint8_t data_len = sizeof(payload) / sizeof(payload[0]);
    		create_packet(&pkt, com, payload, data_len, big_payload, 0, 0);
    		send(pkt);
    	} else {
    		com = 0;
    		int8_t payload[];
    		double_t big_payload[];
    		create_packet(&pkt, com, payload, 0, big_payload, 0, 0);
    		send(pkt);
    	}

    }

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
void cmdprompt(FILE* cmdlog) {
    // int input;

    while (exiting != 1) {

		pkt = listen();

        if (verify_packet(&pkt)) {
        	input = pkt.cmd_primary;
            // write_to_log(cmdlog, "cli.c", "cmdprompt", input);
            exec_command(input);
        }

    }
}

#ifndef CLI_OPH_H
#define CLI_OPH_H

#define START_BYTE 0xAA
#define END_BYTE 0x55
#define MAX_DATA 20
#define FIXED_POINT_SCALE 1000

#include <math.h>
#include <stdint.h>


void cmdprompt(FILE* cmdlog);
void exec_command(char* input);
char* get_input();

enum oph_commands {
    lock,
    unlock,
    reset_lock,
    motor_control,
    motor_stop,
    motor_start,
    accl_stop,
    accl_start,
    accl_status,
    bvexcam_solve_stop,
    bvexcam_solve_start,
    focus_bvexcam,
    bvexcam_status,
    bvexcam_shutdown,
    gotoenc,
    encdither,
    enctrack,
    enconoff,
    stop_scan,
    axe_mode,
    scanning_mode,
    susan_mode,
    motor_control_cam,
    motor_status,
    setting_offset,
    gps_status,
    stop_gps,
    stop_spec,
    start_gps,
    start_spec,
    print_m_data,
    print_m_pid,
    exit_both

};


typedef struct {
    uint8_t start;
    uint8_t num_data;
    uint8_t num_bigpack;
    uint8_t destination; //0 to send to ophiuchus, 1 to saggitarius, 2 to both
    uint32_t utc;
    uint8_t cmd_primary;
    int16_t data[MAX_DATA];  // or int32_t, if needed
    double_t bigpack[MAX_DATA];
    uint8_t checksum;
    uint8_t end;
} Packet;

void cmdprompt();
void exec_command(int input);
char* get_input();
uint8_t compute_checksum(const uint8_t* bytes, size_t length);
void create_packet(Packet* pkt, const uint8_t cmd1, const int16_t* data, const uint8_t small_len, const double_t* bigpack, const uint8_t big_len, const uint8_t dest);
void send(Packet pkt);
Packet listen();


#endif


/*
         APM2EX Version 1.0 
         based on Mav2DupEx Version 0.1 2014, by DevFor8.com, info@devfor8.com
	 
         schiwo1@gmail.com March 2015
	 
	 part of code is based on ArduCAM OSD
*/

#include "../GCS_MAVLink/include/mavlink/v1.0/mavlink_types.h"
#include "../GCS_MAVLink/include/mavlink/v1.0/ardupilotmega/mavlink.h"

// true when we have received at least 1 MAVLink packet
static bool mavlink_active;
static uint8_t crlf_count = 0;

static int packet_drops = 0;
static int parse_error = 0;

boolean getBit(byte Reg, byte whichBit) {
    boolean State;
    State = Reg & (1 << whichBit);
    return State;
}  

void request_mavlink_rates()
{
    const int  maxStreams = 6;
    const uint8_t MAVStreams[maxStreams] = {MAV_DATA_STREAM_RAW_SENSORS,
        MAV_DATA_STREAM_EXTENDED_STATUS,
        MAV_DATA_STREAM_RC_CHANNELS,
        MAV_DATA_STREAM_POSITION,
        MAV_DATA_STREAM_EXTRA1, 
        MAV_DATA_STREAM_EXTRA2};
    const uint16_t MAVRates[maxStreams] = {0x02, 0x02, 0x05, 0x02, 0x05, 0x02};
    for (int i=0; i < maxStreams; i++) {
        mavlink_msg_request_data_stream_send(MAVLINK_COMM_0,
            apm_mav_system, apm_mav_component,
            MAVStreams[i], MAVRates[i], 1);
    }
}

void read_mavlink(){
    mavlink_message_t msg; 
    mavlink_status_t status;

    //grabing data 
    while(Serial.available() > 0) { 
        uint8_t c = Serial.read();

        //trying to grab msg  
        if(mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
            mavlink_active = 1;
            //handle msg
            switch(msg.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT:
                {
                  //Serial.println("beat");
                    mavbeat = 1;
                    apm_mav_system    = msg.sysid;
                    apm_mav_component = msg.compid;
                    apm_mav_type      = mavlink_msg_heartbeat_get_type(&msg);            
                 //   osd_mode = mavlink_msg_heartbeat_get_custom_mode(&msg);
                    osd_mode = (uint8_t)mavlink_msg_heartbeat_get_custom_mode(&msg);
                    //Mode (arducoper armed/disarmed)
                    base_mode = mavlink_msg_heartbeat_get_base_mode(&msg);
                    if(getBit(base_mode,7)) motor_armed = 1;
                    else motor_armed = 0;

                    osd_nav_mode = 0;          
                    lastMAVBeat = millis();
                    if(waitingMAVBeats == 1){
                        enable_mav_request = 1;
                    }
                }
                break;
            case MAVLINK_MSG_ID_SYS_STATUS:
                {
                  //serial.println("status");
                    osd_vbat_A = (mavlink_msg_sys_status_get_voltage_battery(&msg) / 1000.0f); //Battery voltage, in millivolts (1 = 1 millivolt)
                    osd_curr_A = mavlink_msg_sys_status_get_current_battery(&msg); //Battery current, in 10*milliamperes (1 = 10 milliampere)         
                    osd_battery_remaining_A = mavlink_msg_sys_status_get_battery_remaining(&msg); //Remaining battery energy: (0%: 0, 100%: 100)
                    osd_curr_A = osd_curr_A / 100; //Fix current representation in Amperes
                }
                break;

            case MAVLINK_MSG_ID_GPS_RAW_INT:
                {
                 // fix by rosewhite
                   osd_lat_org = mavlink_msg_gps_raw_int_get_lat(&msg); // store the orignal data
                   osd_lon_org = mavlink_msg_gps_raw_int_get_lon(&msg);
                   osd_lat = osd_lat_org / 10000000.0f;
                   osd_lon = osd_lon_org / 10000000.0f;
                   osd_fix_type = mavlink_msg_gps_raw_int_get_fix_type(&msg);
                    // 0 = No GPS, 1 =No Fix, 2 = 2D Fix, 3 = 3D Fix 
                    osd_satellites_visible = mavlink_msg_gps_raw_int_get_satellites_visible(&msg);
                    ap_gps_hdop = mavlink_msg_gps_raw_int_get_eph(&msg)/100.f;
                }
                break; 
            case MAVLINK_MSG_ID_VFR_HUD:
                {
                  //Serial.print("hud:");
                    osd_airspeed = mavlink_msg_vfr_hud_get_airspeed(&msg);
                    osd_groundspeed = mavlink_msg_vfr_hud_get_groundspeed(&msg);
                    osd_heading = mavlink_msg_vfr_hud_get_heading(&msg); // 0..360 deg, 0=north
                  //Serial.println(osd_heading);  
                    osd_throttle = mavlink_msg_vfr_hud_get_throttle(&msg);
                    //if(osd_throttle > 100 && osd_throttle < 150) osd_throttle = 100;//Temporary fix for ArduPlane 2.28
                    //if(osd_throttle < 0 || osd_throttle > 150) osd_throttle = 0;//Temporary fix for ArduPlane 2.28
                    osd_alt = mavlink_msg_vfr_hud_get_alt(&msg);
                    osd_climb = mavlink_msg_vfr_hud_get_climb(&msg);
                }
                break;
            case MAVLINK_MSG_ID_ATTITUDE:
                {
                  //Serial.print("attitude:");
                    osd_pitch = ToDeg(mavlink_msg_attitude_get_pitch(&msg));
                    osd_roll = ToDeg(mavlink_msg_attitude_get_roll(&msg));
                    osd_yaw = ToDeg(mavlink_msg_attitude_get_yaw(&msg));
                  //Serial.print(osd_pitch);  Serial.print("  / ");Serial.print(osd_roll);  Serial.print("  / ");Serial.println(osd_yaw); 
                    
                }
                break;
/*             case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
                {
                  nav_roll = mavlink_msg_nav_controller_output_get_nav_roll(&msg);
                  nav_pitch = mavlink_msg_nav_controller_output_get_nav_pitch(&msg);
                  nav_bearing = mavlink_msg_nav_controller_output_get_nav_bearing(&msg);
                  alt_error = mavlink_msg_nav_controller_output_get_alt_error(&msg);
                  aspd_error = mavlink_msg_nav_controller_output_get_aspd_error(&msg);
                  xtrack_error = mavlink_msg_nav_controller_output_get_xtrack_error(&msg);
                }
                break;
           case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
                {
                    chan1_raw = mavlink_msg_rc_channels_raw_get_chan1_raw(&msg);
                    chan2_raw = mavlink_msg_rc_channels_raw_get_chan2_raw(&msg);
                    osd_chan5_raw = mavlink_msg_rc_channels_raw_get_chan5_raw(&msg);
                    osd_chan6_raw = mavlink_msg_rc_channels_raw_get_chan6_raw(&msg);
                    osd_chan7_raw = mavlink_msg_rc_channels_raw_get_chan7_raw(&msg);
                    osd_chan8_raw = mavlink_msg_rc_channels_raw_get_chan8_raw(&msg);
                    osd_rssi = mavlink_msg_rc_channels_raw_get_rssi(&msg);
                }
                break;
            case MAVLINK_MSG_ID_WIND:
                {
                    osd_winddirection = mavlink_msg_wind_get_direction(&msg); // 0..360 deg, 0=north
                    osd_windspeed = mavlink_msg_wind_get_speed(&msg); //m/s
                    osd_windspeedz = mavlink_msg_wind_get_speed_z(&msg); //m/s
                }
                break;*/
            default:
                //Do nothing
                break;
            }
        }
        delayMicroseconds(138);
        //next one
    }
    // Update global packet drops counter
    packet_drops += status.packet_rx_drop_count;
    parse_error += status.parse_error;

}

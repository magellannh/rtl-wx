/*
 * rtl-433fm-decode
 *
 * Message Decoding  to support reception from
 * 433Mhz weather and energy sensors by using a 
 * RealTek RTL2832 DVB usb dongle.
 * Code supports demodulation/decoding of OOK_PCM,
 * Manchester, and FSK sensor messages.
 *
 * This file in conjunction with rtl-433fm-demod combines
 *  work from rtl_433 and rtl_fm. to support Oregon scientific 
 * weather sensors (v2.1 and 3 using manchester encoding) and
 * Efergy and Owl energy monitors. 
 *
 * Based on rtl_sdr , rtl_433, and rtl_fm
 *   Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 *   Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 *   Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 *   Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 *   Copyright (C) 2012 by Benjamin Larsson <benjamin@southpole.se>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <libusb.h>
#include <stdint.h>
#include <time.h>

#include "rtl-433fm.h"

// Callback routines can optionally notify rtl-wx code on receive of messages from sensors .
static void (*os_msg_error_callback)(unsigned char *, int)=NULL;
void rtl_decode_register_os_msg_error_callback(void (*callback_function)(unsigned char *, int)) {
  os_msg_error_callback = callback_function;
}
static void (*os_msg_ok_callback)(unsigned char *,int, int)=NULL;
void rtl_decode_register_os_msg_ok_callback(void (*callback_function)(unsigned char *,int, int)) {
  os_msg_ok_callback = callback_function;
}

static void (*efergy_msg_error_callback)(unsigned char *, int)=NULL;
void rtl_decode_register_efergy_msg_error_callback(void (*callback_function)(unsigned char *, int)) {
  efergy_msg_error_callback = callback_function;
}
static void (*efergy_msg_ok_callback)(unsigned char *,int, float)=NULL;
void rtl_decode_register_efergy_msg_ok_callback(void (*callback_function)(unsigned char *,int, float)) {
  efergy_msg_ok_callback = callback_function;
}

static void (*owl_msg_error_callback)(unsigned char *, int, float, float)=NULL;
void rtl_decode_register_owl_msg_error_callback(void (*callback_function)(unsigned char *, int, float, float)) {
  owl_msg_error_callback = callback_function;
}
static void (*owl_msg_ok_callback)(unsigned char *,int, float, float)=NULL;
void rtl_decode_register_owl_msg_ok_callback(void (*callback_function)(unsigned char *,int, float, float)) {
  owl_msg_ok_callback = callback_function;
}

float get_os_temperature(unsigned char *message, unsigned int sensor_id) {
  // sensor ID included  to support sensors with temp in different position
  float temp_c = 0;
  temp_c = (((message[5]>>4)*100)+((message[4]&0x0f)*10) + ((message[4]>>4)&0x0f)) /10.0F;
  if (message[5] & 0x0f)
       temp_c = -temp_c;
  return temp_c;
}
unsigned int get_os_humidity(unsigned char *message, unsigned int sensor_id) {
 // sensor ID included to support sensors with temp in different position
 int humidity = 0;
    humidity = ((message[6]&0x0f)*10)+(message[6]>>4);
 return humidity;
}

static unsigned int swap_nibbles(unsigned char byte) {
 return (((byte&0xf)<<4) | (byte>>4));
}
unsigned int get_owl_current(unsigned char *msg) {
 unsigned int watts = swap_nibbles(msg[3]) + (msg[4]<<8);
 watts = (unsigned int) ((float) watts * 4.2);
 // need more work here - likely more bytes of current but not sure where they are...
 return watts;
}

double get_owl_total_current(unsigned char *msg) {
 double total = ((uint64_t) swap_nibbles(msg[10]) << 36) | ((uint64_t) swap_nibbles(msg[9]) << 28) |
                   (swap_nibbles(msg[8]) << 20) | (swap_nibbles(msg[7]) << 12) | 
		   (swap_nibbles(msg[6]) << 4) | (msg[5]&0xf);
//fprintf(stderr, "Raw KWH Total: %x (%d)  ",(int) total, (int) total);

 total /= ((double) 230195/4.2);
 return total;
}
static int validate_os_checksum(unsigned char *msg, int checksum_nibble_idx) {
  // Oregon Scientific v2.1 and v3 checksum is a  1 byte  'sum of nibbles' checksum.  
  // with the 2 nibbles of the checksum byte  swapped.
  int i;
  unsigned int checksum, sum_of_nibbles=0;
  for (i=0; i<(checksum_nibble_idx-1);i+=2) {
    unsigned char val=msg[i>>1];
	sum_of_nibbles += ((val>>4) + (val &0x0f));
  }
  if (checksum_nibble_idx & 1) {
     sum_of_nibbles += (msg[checksum_nibble_idx>>1]>>4);
     checksum = (msg[checksum_nibble_idx>>1] & 0x0f) | (msg[(checksum_nibble_idx+1)>>1]&0xf0);
  } else
     checksum = (msg[checksum_nibble_idx>>1]>>4) | ((msg[checksum_nibble_idx>>1]&0x0f)<<4);
  sum_of_nibbles &= 0xff;
  
  if (sum_of_nibbles == checksum)
    return 0;
  else {
	if (os_msg_error_callback != NULL)
		os_msg_error_callback(msg, (checksum_nibble_idx>>1)+1);
	else {
//             fprintf(stderr, "Checksum error in Oregon Scientific message.  Expected: %02x  Calculated: %02x\n", checksum, sum_of_nibbles);	
             fprintf(stderr, "Checksum error, nibbleSum: %02x  ", sum_of_nibbles);	
	     fprintf(stderr, "Msg: "); int i; for (i=0 ;i<((checksum_nibble_idx+4)>>1) ; i++) fprintf(stderr, "%02x ", msg[i]); fprintf(stderr, "\n\n");
	}
	return 1;
  }
}

static int validate_os_v2_message(unsigned char * msg, int bits_expected, int valid_v2_bits_received, 
                                int nibbles_in_checksum) {
  // Oregon scientific v2.1 protocol sends each bit using the complement of the bit, then the bit  for better error checking.  Compare number of valid bits processed vs number expected
  if (bits_expected == valid_v2_bits_received) {
    return (validate_os_checksum(msg, nibbles_in_checksum));	
  } else {
    if (os_msg_error_callback != NULL)
      os_msg_error_callback(msg, (valid_v2_bits_received+7)>>3);
//    fprintf(stderr, "Bit validation error on Oregon Scientific message.  Expected %d bits, received error after bit %d \n",        bits_expected, valid_v2_bits_received);	
//    fprintf(stderr, "Message: "); int i; for (i=0 ;i<(bits_expected+7)/8 ; i++) fprintf(stderr, "%02x ", msg[i]); fprintf(stderr, "\n\n");
  }
  return 1;
}

static int oregon_scientific_v2_1_parser(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
   // Check 2nd and 3rd bytes of stream for possible Oregon Scientific v2.1 sensor data (skip first byte to get past sync/startup bit errors)
   if ( ((bb[0][1] == 0x55) && (bb[0][2] == 0x55)) ||
	    ((bb[0][1] == 0xAA) && (bb[0][2] == 0xAA))) {
	  int i,j;
	  unsigned char msg[BITBUF_COLS] = {0};
	   
	  // Possible  v2.1 Protocol message
	  int num_valid_v2_bits = 0;
	  
	  unsigned int sync_test_val = (bb[0][3]<<24) | (bb[0][4]<<16) | (bb[0][5]<<8) | (bb[0][6]);
	  int dest_bit = 0;
	  int pattern_index;
	  // Could be extra/dropped bits in stream.  Look for sync byte at expected position +/- some bits in either direction
      for(pattern_index=0; pattern_index<8; pattern_index++) {
        unsigned int mask     = (unsigned int) (0xffff0000>>pattern_index);
		unsigned int pattern  = (unsigned int)(0x55990000>>pattern_index);
        unsigned int pattern2 = (unsigned int)(0xaa990000>>pattern_index);

	//fprintf(stderr, "OS v2.1 sync byte search - test_val=%08x pattern=%08x  mask=%08x\n", sync_test_val, pattern, mask);

	    if (((sync_test_val & mask) == pattern) || 
		    ((sync_test_val & mask) == pattern2)) {
		  //  Found sync byte - start working on decoding the stream data.
		  // pattern_index indicates  where sync nibble starts, so now we can find the start of the payload
	      int start_byte = 5 + (pattern_index>>3);
	      int start_bit = pattern_index & 0x07;
	//fprintf(stderr, "OS v2.1 Sync test val %08x found, starting decode at byte index %d bit %d\n", sync_test_val, start_byte, start_bit);
	      int bits_processed = 0;
		  unsigned char last_bit_val = 0;
		  j=start_bit;
	      for (i=start_byte;i<BITBUF_COLS;i++) {
	        while (j<8) {
			   if (bits_processed & 0x01) {
			     unsigned char bit_val = ((bb[0][i] & (0x80 >> j)) >> (7-j));
				 
				 // check if last bit received was the complement of the current bit
				 if ((num_valid_v2_bits == 0) && (last_bit_val == bit_val))
				   num_valid_v2_bits = bits_processed; // record position of first bit in stream that doesn't verify correctly
				 last_bit_val = bit_val;
				   
			     // copy every other bit from source stream to dest packet
				 msg[dest_bit>>3] |= (((bb[0][i] & (0x80 >> j)) >> (7-j)) << (7-(dest_bit & 0x07)));
				 
	//fprintf(stderr,"i=%d j=%d dest_bit=%02x bb=%02x msg=%02x\n",i, j, dest_bit, bb[0][i], msg[dest_bit>>3]); 
				 if ((dest_bit & 0x07) == 0x07) {
				    // after assembling each dest byte, flip bits in each nibble to convert from lsb to msb bit ordering
				    int k = (dest_bit>>3);
                    unsigned char indata = msg[k];
	                // flip the 4 bits in the upper and lower nibbles
	                msg[k] = ((indata & 0x11) << 3) | ((indata & 0x22) << 1) |
	   	                     ((indata & 0x44) >> 1) | ((indata & 0x88) >> 3);
		            }
				 dest_bit++;
			     }
				 else 
				   last_bit_val = ((bb[0][i] & (0x80 >> j)) >> (7-j)); // used for v2.1 bit error detection
			   bits_processed++;
			   j++;
	        }
		    j=0;
		  }
		  break;
	    } //if (sync_test_val...
      } // for (pattern...
	  

    int sensor_id = (msg[0] << 8) | msg[1];
	if ((sensor_id == 0x1d20) || (sensor_id == 0x1d30))	{
	   if (validate_os_v2_message(msg, 153, num_valid_v2_bits, 15) == 0) {
	     if (os_msg_ok_callback != NULL) {
		os_msg_ok_callback(msg, (num_valid_v2_bits+7)>>3, sensor_id);
		return 1;
	     }
	   int  channel = ((msg[2] >> 4)&0x0f);
	   if (channel == 4)
	       channel = 3; // sensor 3 channel number is 0x04
		float temp_c = get_os_temperature(msg, sensor_id);
		 if (sensor_id == 0x1d20) fprintf(stderr, "Weather Sensor THGR122N Channel %d ", channel);
		 else fprintf(stderr, "Weather Sensor THGR968  Outdoor   ");
		 fprintf(stderr, "Temp: %3.1f°C  %3.1f°F   Humidity: %d%%\n", temp_c, ((temp_c*9)/5)+32,get_os_humidity(msg, sensor_id));
	   }
	   return 1;  
    } else if (sensor_id == 0x5d60) {
	   if (validate_os_v2_message(msg, 185, num_valid_v2_bits, 19) == 0) {
	     if (os_msg_ok_callback != NULL) {
		os_msg_ok_callback(msg, (num_valid_v2_bits+7)>>3, sensor_id);
		return 1;
	     }
	     unsigned int comfort = msg[7] >>4;
	     char *comfort_str="Normal";
	     if      (comfort == 4)   comfort_str = "Comfortable";
	     else if (comfort == 8)   comfort_str = "Dry";
	     else if (comfort == 0xc) comfort_str = "Humid";
	     unsigned int forecast = msg[9]>>4;
	     char *forecast_str="Cloudy";
	     if      (forecast == 3)   forecast_str = "Rainy";
	     else if (forecast == 6)   forecast_str = "Partly Cloudy";
	     else if (forecast == 0xc) forecast_str = "Sunny";
         float temp_c = get_os_temperature(msg, 0x5d60);
	     fprintf(stderr,"Weather Sensor BHTR968  Indoor    Temp: %3.1f°C  %3.1f°F   Humidity: %d%%", temp_c, ((temp_c*9)/5)+32, get_os_humidity(msg, 0x5d60));  
	     fprintf(stderr, " (%s) Pressure: %dmbar (%s)\n", comfort_str, ((msg[7] & 0x0f) | (msg[8] & 0xf0))+856, forecast_str);  
	   }
	   return 1;
	} else if (sensor_id == 0x2d10) {
	   if (validate_os_v2_message(msg, 161, num_valid_v2_bits, 16) == 0) {
	     if (os_msg_ok_callback != NULL) {
		os_msg_ok_callback(msg,  (num_valid_v2_bits+7)>>3, sensor_id);
		return 1;
	     }
	   float rain_rate = (((msg[4] &0x0f)*100)+((msg[4]>>4)*10) + ((msg[5]>>4)&0x0f)) /10.0F;
       float total_rain = (((msg[7]&0xf)*10000)+((msg[7]>>4)*1000) + ((msg[6]&0xf)*100)+((msg[6]>>4)*10) + (msg[5]&0xf))/10.0F;
	   fprintf(stderr, "Weather Sensor RGR968   Rain Gauge  Rain Rate: %2.0fmm/hr Total Rain %3.0fmm\n", rain_rate, total_rain);
	   }
	   return 1;
	} else if (num_valid_v2_bits > 16) {
//fprintf(stderr, "%d bit message received from unrecognized Oregon Scientific v2.1 sensor.\n", num_valid_v2_bits);
//fprintf(stderr, "Message: "); for (i=0 ; i<20 ; i++) fprintf(stderr, "%02x ", msg[i]); fprintf(stderr,"\n\n");
    } else {
//fprintf(stderr, "\nPossible Oregon Scientific v2.1 message, but sync nibble wasn't found\n"); fprintf(stderr, "Raw Data: "); for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", bb[0][i]); fprintf(stderr,"\n\n");    
    } 
   } else {
//if (bb[0][3] != 0) int i; fprintf(stderr, "\nBadly formatted OS v2.1 message encountered."); for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", bb[0][i]); fprintf(stderr,"\n\n");}
   }
   return 0;
}

static int oregon_scientific_v3_parser(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
 
   // Check stream for possible Oregon Scientific v3 protocol data (skip part of first and last bytes to get past sync/startup bit errors)
   if ((((bb[0][0]&0xf) == 0x0f) && (bb[0][1] == 0xff) && ((bb[0][2]&0xc0) == 0xc0)) || 
       (((bb[0][0]&0xf) == 0x00) && (bb[0][1] == 0x00) && ((bb[0][2]&0xc0) == 0x00))) {
	  int i,j;
	  unsigned char msg[BITBUF_COLS] = {0};	  
	  unsigned int sync_test_val = (bb[0][2]<<24) | (bb[0][3]<<16) | (bb[0][4]<<8);
	  int dest_bit = 0;
	  int pattern_index;
	  // Could be extra/dropped bits in stream.  Look for sync byte at expected position +/- some bits in either direction
      for(pattern_index=0; pattern_index<16; pattern_index++) {
        unsigned int     mask = (unsigned int)(0xfff00000>>pattern_index);
        unsigned int  pattern = (unsigned int)(0xffa00000>>pattern_index);
        unsigned int pattern2 = (unsigned int)(0xff500000>>pattern_index);
		unsigned int pattern3 = (unsigned int)(0x00500000>>pattern_index);
//fprintf(stderr, "OS v3 Sync nibble search - test_val=%08x pattern=%08x  mask=%08x\n", sync_test_val, pattern, mask);
	    if (((sync_test_val & mask) == pattern)  ||
            ((sync_test_val & mask) == pattern2) ||		
            ((sync_test_val & mask) == pattern3)) {
		  //  Found sync byte - start working on decoding the stream data.
		  // pattern_index indicates  where sync nibble starts, so now we can find the start of the payload
	      int start_byte = 3 + (pattern_index>>3);
	      int start_bit = (pattern_index+4) & 0x07;
//fprintf(stderr, "Oregon Scientific v3 Sync test val %08x ok, starting decode at byte index %d bit %d\n", sync_test_val, start_byte, start_bit);
          j = start_bit;
	      for (i=start_byte;i<BITBUF_COLS;i++) {
	        while (j<8) {
			   unsigned char bit_val = ((bb[0][i] & (0x80 >> j)) >> (7-j));
				   
			   // copy every  bit from source stream to dest packet
			   msg[dest_bit>>3] |= (((bb[0][i] & (0x80 >> j)) >> (7-j)) << (7-(dest_bit & 0x07)));
				 
//fprintf(stderr,"i=%d j=%d dest_bit=%02x bb=%02x msg=%02x\n",i, j, dest_bit, bb[0][i], msg[dest_bit>>3]); 
			   if ((dest_bit & 0x07) == 0x07) {
				  // after assembling each dest byte, flip bits in each nibble to convert from lsb to msb bit ordering
				  int k = (dest_bit>>3);
                  unsigned char indata = msg[k];
	              // flip the 4 bits in the upper and lower nibbles
	              msg[k] = ((indata & 0x11) << 3) | ((indata & 0x22) << 1) |
	   	                   ((indata & 0x44) >> 1) | ((indata & 0x88) >> 3);
		         }
			   dest_bit++;
			   j++;
			}
			j=0;
	       }
		  break;
		  }
	    }
		
	if ((msg[0] == 0xf8) && (msg[1] == 0x24))	{
	   if (validate_os_checksum(msg, 15) == 0) {
	     if (os_msg_ok_callback != NULL) {
		os_msg_ok_callback(msg, 8, 0xf824);
		return 1;
	     }
	     int  channel = ((msg[2] >> 4)&0x0f);
	     float temp_c = get_os_temperature(msg, 0xf824);
		 int humidity = get_os_humidity(msg, 0xf824);
		 fprintf(stderr,"Weather Sensor THGR810  Channel %d Temp: %3.1f°C  %3.1f°F   Humidity: %d%%\n", channel, temp_c, ((temp_c*9)/5)+32, humidity);
	   }
	   return 1;
    } else if ((msg[0] != 0) && (msg[1]!= 0)) { //  sync nibble was found  and some data is present...
// THIS CODE IS TEMPORARY AND VERY KLUDGEY DUE TO INABILITY TO PARSE OWL CHECKSUM
// This code assumes the OWL sensor is connected to an oil burner circuit  (max 1000 watts) and validates the owl values based on that!
	unsigned int current = get_owl_current(msg);
	double total_current = get_owl_total_current(msg);
        if ((current > 0) && (total_current > 0) && (current < 1000) && (total_current < 10000)) {
		if (owl_msg_ok_callback != NULL) {
			owl_msg_ok_callback(msg, 13, (float) current, total_current);
			return 1;
		} else
//fprintf(stderr, "Power: %d (watts) Total: %7.4f (KWH)\n", current, total_current);
fprintf(stderr, " Energy Sensor OWLCM119 Channel %d Current: %d (watts) Total: %7.4f (KWH)\n", msg[0]>>4, current, total_current);
	} else if (owl_msg_error_callback != NULL)
		owl_msg_error_callback(msg, 13, (float) current, total_current);		
    } else if ((msg[0] != 0) && (msg[1]!= 0)) { //  sync nibble was found  and some data is present...
fprintf(stderr, "Message received from unrecognized Oregon Scientific v3 sensor.\n");
fprintf(stderr, "Message: "); for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", msg[i]); fprintf(stderr, "\n");
fprintf(stderr, "    Raw: "); for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", bb[0][i]); fprintf(stderr,"\n\n");       
    } else if (bb[0][3] != 0) {
//fprintf(stderr, "\nPossible Oregon Scientific v3 message, but sync nibble wasn't found\n"); fprintf(stderr, "Raw Data: "); for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", bb[0][i]); fprintf(stderr,"\n\n");   	
    }
   }	
   else { // Based on first couple of bytes, either corrupt message or something other than an Oregon Scientific v3 message
//if (bb[0][3] != 0) { fprintf(stderr, "\nUnrecognized Msg in v3: "); int i; for (i=0 ; i<BITBUF_COLS ; i++) fprintf(stderr, "%02x ", bb[0][i]); fprintf(stderr,"\n\n"); }
   } 
   return 0;
}

int oregon_scientific_decode(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
 int ret; 
 ret = oregon_scientific_v2_1_parser(bb);
 if (ret == 0)
   ret = oregon_scientific_v3_parser(bb);
 return ret;
}

int acurite_rain_gauge_decode(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]) {
    // This needs more validation to positively identify correct sensor type, but it basically works if message is really from acurite raingauge and it doesn't have any errors
    if ((bb[0][0] != 0) && (bb[0][1] != 0) && (bb[0][2]!=0) && (bb[0][3] == 0) && (bb[0][4] == 0)) {
	    float total_rain = ((bb[0][1]&0xf)<<8)+ bb[0][2];
		total_rain /= 2; // Sensor reports number of bucket tips.  Each bucket tip is .5mm
        fprintf(stderr, "AcuRite Rain Gauge Total Rain is %2.1fmm\n", total_rain);
		fprintf(stderr, "Raw Message: %02x %02x %02x %02x %02x\n",bb[0][0],bb[0][1],bb[0][2],bb[0][3],bb[0][4]);
        return 1;
    }
    return 0;
}

/*---------------------------------------------------------------------

EFERGY ENERGY SENSOR RTL-SDR DECODER via rtl_fm
Based on EfergyRPI_001.c and EfergyRPI_log.c
Copyright 2013 Nathaniel Elijah

Permission is hereby granted to use this Software for any purpose
including combining with commercial products, creating derivative
works, and redistribution of source or binary code, without
limitation or consideration. Any redistributed copies of this
Software must include the above Copyright Notice.

THIS SOFTWARE IS PROVIDED "AS IS". THE AUTHOR OF THIS CODE MAKES NO
WARRANTIES REGARDING THIS SOFTWARE, EXPRESS OR IMPLIED, AS TO ITS
SUITABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

Execute with rtl_fm using the following parameters:

rtl_433_fm -f 433.53e6 -s 200000 -r 96000 -A fast -p 56

// Changes by github user magellannh
// 08-17-2014  - merged code from rtl_433, rtl_fm, and EfergyRPI_log to support rtl-wx 
// 08-14-2014  - Changed logic to support different Efergy sensor types without changing settings
//                         Logic uses message size to determine if checksum or crc should be used.
//                         Also, changed code so instead of doing bit by bit processing when samples arrive, 
//                         a frame's worth of samples is saved after a valid preamble and then processed.
//                         One advantage of this approach is that the center is recalculated for each frame
//                         for more reliable decoding, especially if there's lots of 433Mhz noise,
//                         which for me was throwing off the old center calculation (noise from weather sensors).
//                         This is less efficient, but still uses < 7% of cpu on raspberry pi
//                          (plus another 25% for rtl_fm w/ -A fast option)
//                         
// 08-13-2014  - Added code to check the CRC-CCIT (Xmodem)  crc used by Elite 3.0 TPM
// 08/12/2014 - Some debugging and sample analysis code added 
//
// Bug Fix -	Changed frame bytearray to unsigned char and added cast on  byte used in pow() function
//	to explicitly make it signed char.  Default signed/unsigned for char is compiler dependent
//	and this resulted in various problems.
// 
// New Feature  - Added frame analysis feature that dumps debug information to help characterize the FSK
//	sample data received by rtl_fm.  The feature is invoked using a "-d" option followed by
//	an optional debug level (1..4).  The output is sent to stdout. 
//
// Usage Examples: (original examples above still work as before plus these new options)
//
//	rtl_fm -f 433.51e6 -s 200000 -r 96000 -A fast  | ./EfergyRPI_log -d 1
//		This mode shows the least information which is just the best guess at the decoded frame and a KW calculation
//		using bytes 4, 5, and 6.  The checksum is computed and displayed, but not validated.
//	rtl_fm -f 433.51e6 -s 200000 -r 96000 -A fast  | ./EfergyRPI_log -d 2
//		This  level shows average plus and minus sample values and centering which can help with finding the best frequency. 
//		Adjust frequency to get wave center close to  0 .  If center is too high, lower frequency, otherwise increase it.
//	rtl_fm -f 433.51e6 -s 200000 -r 96000 -A fast  | ./EfergyRPI_log -d 3
//		This mode outputs a summary with counts of consecutive positive or negative samples.  These consecutive pulse counts
//		are what the main code uses to decode 0 and 1 data bits.  See comments below for more details on expected pulse counts.
//	rtl_fm -f 433.51e6 -s 200000 -r 96000 -A fast  | ./EfergyRPI_log -d 4
//		This mode shows everything in modes 1..3 plus a raw dump of the sample data received from rtl_fm.
//
//	*Notice the "-A fast" option on  rtl_fm.  This cut Raspberry Pi cpu load from 50% to 25% and decode still worked fine.
//	Also, with an R820T USB dongle, leaving  rtl_fm gain in 'auto' mode  produced the best results.
//

//#define VOLTAGE			240	/*For non-TPM type sensors, set to line voltage */
#define VOLTAGE			1	/* For Efergy Elite 3.0 TPM,  set to 1 */

#define FRAMEBYTECOUNT				9  /* Attempt to decode up to this many bytes.   */
#define MINLOWBITS				3  /* Min number of positive samples for a logic 0 */
#define MINHIGHBITS				9  /* Min number of positive samples for a logic 1 */
#define MIN_POSITIVE_PREAMBLE_SAMPLES		40 /* Number of positive samples in  an efergy  preamble */
#define MIN_NEGATIVE_PREAMBLE_SAMPLES		40 /* Number of negative samples for a valid preamble  */
#define EXPECTED_BYTECOUNT_IF_CHECKSUM_USED	8
#define EXPECTED_BYTECOUNT_IF_CRC_USED 		9

int analysis_wavecenter=0;

#define LOGTYPE			1	// Allows changing line-endings - 0 is for Unix /n, 1 for Windows /r/n
#define SAMPLES_TO_FLUSH	10	// Number of samples taken before writing to file.
					// Setting this too low will cause excessive wear to flash due to updates to
					// filesystem! You have been warned! Set to 10 samples for 6 seconds = every min.					
int loggingok=0;	 // Global var indicating logging on or off
int samplecount=0; // Global var counter for samples taken since last flush
FILE *fp;	 // Global var file handle

// Instead of processing frames bit by bit as samples arrive from rtl_fm, all samples are stored once a preamble is detected until
// enough samples have been saved to cover the expected maximum frame size.  This maximum number of samples needed for a frame
// is an estimate with padding which will hopefully be enough to store a full frame with some extra.
// 
// It seems that with most Efergy formats, each data bit is encoded  using some combination of about 18-20 rtl_fm samples.
// zero bits are usually received as 10-13 negative samples followed by 4-7 positive samples, while 1 bits
// come in as 4-7 negative samples followed by 10-13 positive samples ( these #s may have wide tolerences)
// If the signal has excessive noise, it's theoretically possible to fill up this storage and still have more frame data coming in.
// The code handles this overflow by trunkating the frame, but when this happens, it usually means the data is  junk anyway.
// 
// To skip over noise frames, the code checks for a sequence with both a positive and negative preamble back to back (can be pos-neg or neg-pos)
//  From empirical testing, the preamble is usually about 180 negative samples followed by 45-50 positive samples.
// 
// At some frequencies the sign of the sample data becomes perfectly inverted.  When this happens,  frame data can still be decoded by keying off
// negative pulse sequences instead of positive pulse sequences.  This inversion condition can be detected by looking at the sign of the first samples
// received after the preamble.  If the first set of samples is negative, the data can get decoded from the positive sample pulses.  If the first set of
//samples after the preamble is  greater than 0 then the data probably can be found by decoding negative samples using the same rules that are usually
// used when decoding with the positive samples.  The analysis code automatically checks for this 'inverted' signal condition and decodes from either
// the positive or negative pulse stream depending on that check.

#define APPROX_SAMPLES_PER_BIT  19
#define FRAMEBITCOUNT           (FRAMEBYTECOUNT*8)  /* bits for entire frame (not including preamble) */
#define SAMPLE_STORE_SIZE       (FRAMEBITCOUNT*APPROX_SAMPLES_PER_BIT)

int sample_storage[SAMPLE_STORE_SIZE];			
int sample_store_index;

int decode_bytes_from_pulse_counts(int pulse_store[], int pulse_store_index, unsigned char bytes[]) {
	int i;
	int dbit=0;
	int bitpos=0;
	unsigned char bytedata=0;
	int bytecount=0;
	
	for (i=0;i<FRAMEBYTECOUNT;i++)
		bytes[i]=0;		
	for (i=0;i<pulse_store_index;i++) {
		if (pulse_store[i] > MINLOWBITS) {
			dbit++;
			bitpos++;	
			bytedata = bytedata << 1;
			if (pulse_store[i] > MINHIGHBITS)
				bytedata = bytedata | 0x1;
			if (bitpos > 7) {
				bytes[bytecount] = bytedata;
				bytedata = 0;
				bitpos = 0;
				bytecount++;
				if (bytecount == FRAMEBYTECOUNT) {
					return bytecount;
				}
			}
		}
	}
	return bytecount;
}

unsigned char compute_checksum(unsigned char bytes[], int bytecount) {
	// Calculate simple 1 byte checksum on message bytes
	unsigned char tbyte=0x00;
	int i;
	for(i=0;i<(bytecount-1);i++) {
	  tbyte += bytes[i];
	}
	return tbyte;
}

uint16_t compute_crc(unsigned char bytes[], int bytecount) {
	// Calculate  CRC-CCIT (Xmodem)  crc using 0x1021 polynomial
	uint16_t crc=0;
	int i;
	for (i=0;i<bytecount-2;i++) {
		crc = crc ^ ((uint16_t) bytes[i] << 8);
		int j;
		for (j=0; j<8; j++) {
			if (crc & 0x8000)
				crc = (crc << 1) ^ 0x1021;
			else
				crc <<= 1;
		}
	}
	return crc;
}

int calculate_wave_center(int *avg_positive_sample, int *avg_negative_sample) {
	int i;	
	int64_t avg_neg=0;
	int64_t avg_pos=0;
	int pos_count=0;
	int neg_count=0;
	for (i=0;i<sample_store_index;i++)
		if (sample_storage[i] >=0) {
			avg_pos += sample_storage[i];
			pos_count++;
		} else {
			avg_neg += sample_storage[i];
			neg_count++;
		}
	if (pos_count!=0) 
		avg_pos /= pos_count;
	if (neg_count!=0)
		avg_neg /= neg_count;
	*avg_positive_sample = avg_pos;
	*avg_negative_sample = avg_neg;
	int diff =(avg_neg + ((avg_pos-avg_neg)/2));
	return diff;
}

int generate_pulse_count_array(int display_pulse_details, int pulse_count_storage[]) {

	// When the signal is comes in inverted (maybe an image?) the data can be decoded by counting negative pulses rather than positive
	// pulses.  If the first sequence after the preamble is positive pulses, the data can usually be reliably decoded by parsing using the
	// negative pulse counts.  This flag decoder automatically detect this.
	int store_positive_pulses=(sample_storage[2] < analysis_wavecenter);

	if (display_pulse_details) printf("\nPulse stream for this frame (P-Consecutive samples > center, N-Consecutive samples < center)\n");

	int wrap_count=0;
	int pulse_count=0;
	int space_count=0;
	int pulse_store_index=0;
	int space_store_index=0;
	int display_pulse_info=1;
	int i;
	for(i=0;i<sample_store_index;i++) {
		int samplec = sample_storage[i] - analysis_wavecenter;
		if (samplec < 0) {
			if (pulse_count > 0) {
				if (store_positive_pulses)
				  pulse_count_storage[pulse_store_index++]=pulse_count;
				if (display_pulse_details) printf("%2dP ", pulse_count);
				wrap_count++;
			}
			pulse_count=0;
			space_count++;
		} else {
			if (space_count > 0) {
				if (store_positive_pulses==0)
				 pulse_count_storage[pulse_store_index++]=space_count;
				if (display_pulse_details) printf("%2dN ", space_count);
				wrap_count++;
			}
			space_count=0;
			pulse_count++;
		}
		if (wrap_count >= 16) {
			if (display_pulse_details) printf("\n");
			wrap_count=0;
		}
	}
	if (display_pulse_details) printf("\n\n");

	return pulse_store_index;
}

int display_frame_data(int debug_level, char *msg, unsigned char bytes[], int bytecount) {
	int message_successfully_decoded = 0;
 	time_t ltime; 
	char buffer[80];
	time( &ltime );
	struct tm *curtime = localtime( &ltime );
	strftime(buffer,80,"%x,%X", curtime); 
	
	// Some magic here to figure out whether the message has a 1 byte checksum or 2 byte crc
	char *data_ok_str = (char *) 0;
	unsigned char checksum=0;
	uint16_t crc = 0;
	if (bytecount == EXPECTED_BYTECOUNT_IF_CHECKSUM_USED) {
		checksum = compute_checksum(bytes, bytecount);
		if (checksum == bytes[bytecount-1])
			data_ok_str = "chksum ok";
	} else if (bytecount == EXPECTED_BYTECOUNT_IF_CRC_USED) {
		crc = compute_crc(bytes, bytecount);
		if (crc == ((bytes[bytecount-2]<<8) | bytes[bytecount-1]))
			data_ok_str = "crc ok";
	}

	float current_adc = (bytes[4] * 256) + bytes[5];
	float result  = (VOLTAGE*current_adc) / ((float) (32768) / (float) pow(2,(signed char) bytes[6]));
	if (debug_level > 0) {
		if (debug_level == 1)
			printf("%s  %s ", buffer, msg);
		else
			printf("%s ", msg);
			
		int i;
		for(i=0;i<bytecount;i++)
			printf("%02x ",bytes[i]);
	
		if (data_ok_str != (char *) 0)
			printf(data_ok_str);
		else {
			checksum = compute_checksum(bytes, bytecount);
			crc = compute_crc(bytes, bytecount);
			printf(" cksum: %02x crc16: %04x ",checksum, crc);
		}
		if (result < 100) {
			printf("  kW: %4.3f\n", result);
			message_successfully_decoded = 1;
		} else {
			printf("  kW: <out of range>\n");
			if (data_ok_str != (char *) 0)
			  printf("*For Efergy True Power Moniter (TPM), set VOLTAGE=1 before compiling\n");
		}		
        } else if ((data_ok_str != (char *) 0) && (result < 100)) {
		if (efergy_msg_ok_callback != NULL)
			efergy_msg_ok_callback(bytes, bytecount, result);
		else 
			printf("Efergy Energy Sensor %s   kW: %f\n",buffer,result);
		message_successfully_decoded = 1;
	} else {
		if (efergy_msg_error_callback != NULL)
			efergy_msg_error_callback(bytes, bytecount);
		else 
			printf("Efergy CRC error or value out of range.  Enable debug output with -a option\n");
	}
	return message_successfully_decoded;
}

int analyze_efergy_message(int debug_level) {
	int message_successfully_decoded = 0;
	
	// See how balanced/centered the sample data is.  Best case is  diff close to 0
	int avg_pos, avg_neg;
	int difference = calculate_wave_center(&avg_pos, &avg_neg);
		
	if (debug_level > 1) {
		time_t ltime; 
		char buffer[80];
		time( &ltime );
		struct tm *curtime = localtime( &ltime );
		strftime(buffer,80,"%x,%X", curtime); 		
		printf("\nAnalysis of rtl_fm sample data for frame received on %s\n", buffer);
		printf("     Number of Samples: %6d\n", sample_store_index);
		printf("    Avg. Sample Values: %6d (negative)   %6d (positive)\n", avg_neg, avg_pos);
		printf("           Wave Center: %6d (this frame) %6d (last frame)\n", difference, analysis_wavecenter);
	} 
	analysis_wavecenter = difference; // Use the calculated wave center from this sample to process next frame
	
	if (debug_level==4) { // Raw Sample Dump only in highest debug level
		int wrap_count=0;
		printf("\nShowing raw rtl_fm sample data received between start of frame and end of frame\n");
		int i;
		for(i=0;i<sample_store_index;i++) {
			printf("%6d ", sample_storage[i] - analysis_wavecenter);
			wrap_count++;
			if (wrap_count >= 16) {
				printf("\n");
				wrap_count=0;
			}
		}
		printf("\n\n");
	}

	int display_pulse_details = (debug_level >= 3?1:0);
	int pulse_count_storage[SAMPLE_STORE_SIZE];
	int pulse_store_index = generate_pulse_count_array(display_pulse_details, pulse_count_storage);
	unsigned char bytearray[FRAMEBYTECOUNT];
	int bytecount=decode_bytes_from_pulse_counts(pulse_count_storage, pulse_store_index, bytearray);
	char *frame_msg;
	if (sample_storage[2] < analysis_wavecenter)
		frame_msg = "Msg:";
	else
		frame_msg = "Msg (from negative pulses):";
	message_successfully_decoded = display_frame_data(debug_level, frame_msg, bytearray, bytecount);
	
	if (debug_level>1) printf("\n");
	return message_successfully_decoded;
}

// Look for a valid Efergy Preamble sequence which we'll define as
// a sequence of at least MIN_PEAMBLE_SIZE positive and negative or negative and positive pulses. eg 50N+50P or 50P+50N
int negative_preamble_count=0;
int positive_preamble_count=0;
int prvsamp=0;;
int preamble_found=0;
int  efergy_energy_sensor_decode(int16_t *buf, int len, int efergy_debug_level) 
{
	int message_successfully_decoded = 0;
	int i;
	for (i=0;i<len;i++) {	
		int cursamp = buf[i];
		if (preamble_found == 0) {
			// Check for preamble 
			if ((prvsamp >= analysis_wavecenter) && (cursamp >= analysis_wavecenter)) {
				positive_preamble_count++;
			} else if ((prvsamp < analysis_wavecenter) && (cursamp < analysis_wavecenter)) {
				negative_preamble_count++;				
			} else if ((prvsamp >= analysis_wavecenter) && (cursamp < analysis_wavecenter)) {
				if ((positive_preamble_count > MIN_POSITIVE_PREAMBLE_SAMPLES) &&
					(negative_preamble_count > MIN_NEGATIVE_PREAMBLE_SAMPLES)) {
					preamble_found = 1;
					positive_preamble_count=0;
					sample_store_index = 0;
				}
				negative_preamble_count=0;
			} else if ((prvsamp < analysis_wavecenter) && (cursamp >= analysis_wavecenter)) {
				if ((positive_preamble_count > MIN_POSITIVE_PREAMBLE_SAMPLES) &&
					(negative_preamble_count > MIN_NEGATIVE_PREAMBLE_SAMPLES)) {
					preamble_found = 1;
					negative_preamble_count=0;
					sample_store_index = 0;
				}
				positive_preamble_count=0;
			}
		} else { // preamble_found != 0	
			sample_storage[sample_store_index] = cursamp;
			if (sample_store_index < (SAMPLE_STORE_SIZE-1))
				sample_store_index++;
			else {
				message_successfully_decoded = analyze_efergy_message(efergy_debug_level);
				preamble_found=0;
			}
		}
		prvsamp = cursamp;
	} // for 
	return message_successfully_decoded;
}

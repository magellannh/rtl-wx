/*========================================================================
   rtl-wx.c

   This program gathers data from supported  wireless 433Mhz weather sensors by
   using a usb-rtl-sdr device such as the NooElec RTL device.
       
   This program can operate in any one of three modes depending on the command line
   used to invoke it.  In  "Server" or daemon mode, the program is intended to run in
   the background and doesn't use tty except for logging startup errors.  A normal 
   invocation for this mode would be something like "rtl-wx -s"

   A client mode of operation is available which can be used to connect to another invocation
   of this program that's running in server mode (using named pipes).  When running in client
   mode, the client tty is used to receive commands which are sent to the server over a pipe.  
   Also, when a client is connected, all server output is directed through a pipe to the
   client.  The client is provided primarily for debug/testing/status purposes.   The
   command line for invoking the program in client mode is simply "rtl-wx -c".  A variation on
   this mode called remote command mode is provided (rtl-wx -r <command char> ) where a
   single command is sent to the server and the output from the command is echoed to stdout.

   Finally, a standalone mode is also provided where interactive input and output is done through
   tty and stdout.  The program is fully functional in this mode and this can be useful for short-term
   testing.  An example command line for standalone mode is simply "rtl-wx"

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
   AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE, EVEN IF
   THE COPYRIGHT HOLDERS OR CONTRIBUTORS ARE AWARE OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "rtl-wx.h"

// Global collection of current weather station data
// This object is manipulated by several other modules...
WX_Data wxData;

time_t WX_programStartTime;

// Only used when OWL electricity sensor is connected to oil burner circuit
// and fuelBurnerOnWattageThreshold is > 0 in config file
long int WX_totalBurnerRunSeconds;

// Misc defines used only by this module
#define FALSE 0
#define TRUE 1

static void runServerStandaloneLoop(int receiveDesc, FILE *outputfd);
static void runClientLoop(int receiveDesc);
static void runRemoteCommand(int recvFromServer, char command);
static void outputProgramHelp(FILE *fd);
static void WX_milliSleep(int milliseconds);
static void WX_Init(void);

// filenames used for named pipes when running client mode and server mode.
#define WX_SERVER_SEND_PIPE "/tmp/WX_serverSend"
#define WX_CLIENT_SEND_PIPE "/tmp/WX_serverReceive"

// These file descriptors are used to send output to the right place.  outputfd is either stdout or a pipe that a client program
// may be listening on for testing and debug purposes.  logfd is the name of a logging file that logs program output when the
// program is run in server mode.  The DPRINTF() macro automatically sends output to these file descriptor by default.
FILE *outputfd=NULL;
FILE *logfd=NULL;

int rawxDataDumpMode = FALSE;
extern void WX_process_os_msg_error(unsigned char *msg, int length);
extern void WX_process_os_msg_ok(unsigned char *msg, int length, int sensor_id);
extern void WX_process_efergy_msg_error(unsigned char *msg, int length);
extern void WX_process_efergy_msg_ok(unsigned char *msg, int length, float kilowatts);
extern void WX_process_owl_msg_error(unsigned char *msg, int length, float watts, float total_kwh);
extern void WX_process_owl_msg_ok(unsigned char *msg, int length, float watts, float total_kwh);

//  rtl_433_fm message receiver/decoder  routines
extern void rtl_433fm_main(int argc, char **argv);
extern void rtl_decode_register_os_msg_ok_callback(void (*callback_function)(unsigned char *, int, int));
extern void rtl_decode_register_os_msg_error_callback(void (*callback_function)(unsigned char *, int));
extern void rtl_decode_register_efergy_msg_ok_callback(void (*callback_function)(unsigned char *, int, float));
extern void rtl_decode_register_efergy_msg_error_callback(void (*callback_function)(unsigned char *, int));
extern void rtl_decode_register_owl_msg_ok_callback(void (*callback_function)(unsigned char *, int, float, float));
extern void rtl_decode_register_owl_msg_error_callback(void (*callback_function)(unsigned char *, int, float, float));

// Need lock for multi-threaded rw access to energy sample history in wxData structure
pthread_rwlock_t energy_sample_array_rw_lock;

// Create a thread to start  the rtl_433_fm message receiver
pthread_t rtl_433fm_thread_struct;
void *rtl_433fm_thread(void *param) {
#ifdef ENABLE_EFERGY_SUPPORT
  rtl_433fm_main(0, NULL);
#else
  init_rtl_433_for_rtlwx_without_rtlfm();
#endif
  fprintf(stderr,"RTL 433 FM Thread exited unexpectedly\n");
  return NULL; 
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// MAIN() - Validate program input, configure i/o for user interaction, and invoke the correct main control loop
//                depending on whether program is in server mode (or standalone) or is in client mode.
//--------------------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
   struct termios  oldkey, newkey;   // place to store old and new keyboard settings
   char *workingDirName, *receiveFilename;
   int receiveDesc, outputDesc;
   typedef enum _opModeEnum { Client, Server, Standalone, RemoteCommand, Error} OpModeEnum;
   OpModeEnum opMode=Standalone;
    
   // Set default for working directory to run from.  For server mode, command line may override this.
   workingDirName = DEFAULT_WORKING_DIR;
   umask(0);

   // When program is invoked, operational mode can be specified as -s=server, -c=client or  omitted=standalone
   // optionally, command line can include the path to a working directory
   if (argc < 2)
   opMode = Server;
   else if (strncmp(argv[1], "-s",2) == 0) 
    opMode= Standalone;
   else if (strncmp(argv[1], "-r",2) == 0)
    opMode = RemoteCommand;
   else if (strncmp(argv[1], "-c",2) == 0)
    opMode = Client;
   else if (strncmp(argv[1], "-w",2) == 0) {
    opMode = Server;
    if (argc != 3)
      opMode=Error;
    else
      workingDirName = argv[2];
   } else 
     opMode=Error;

   if (opMode == Error) {
     fprintf(stderr, "\n");
     fprintf(stderr, "Usage: rtl-wx [-s -c -r] or rtl-wx -w <working dir name>\n\n");
     fprintf(stderr, "  rtl-wx    - Server mode (most typical - use web for control)\n");
     fprintf(stderr, "  rtl-wx -s - Standalone mode (terminal only, no web or  client support)\n");
     fprintf(stderr, "  rtl-wx -c - Client mode (Connects to running server with named pipe in /tmp)\n");
     fprintf(stderr, "  rtl-wx -r - Remote command mode (for cgi scripts in web interface)\n");
     fprintf(stderr, "  rtl-wx -w <working dir> - Server mode using <working dir> (default is ./%s)\n\n", DEFAULT_WORKING_DIR);
     exit(1); 
   }

   if (opMode == Server) {
    if (chdir(workingDirName) != 0) {
      fprintf(stderr,"RTL-Wx: Unable to set working directory to %s.  Exiting...\n\n", workingDirName);
      exit(1);
    }
    // Open a log file to store all server output (DPRINTF() output is echoed here)
    // This is only done for server mode
    if ((logfd = fopen(LOG_FILE_PATH, "a")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open logfile rtl-wx.log.  Exiting...\n\n");
      exit(1);
    }
    // Need to create the interprocess communications fifos in the filesystem 

    // First delete the fifos if they're already there in order to flush them out and start over
    remove(WX_CLIENT_SEND_PIPE);
    remove(WX_SERVER_SEND_PIPE);
     
    // Now create the two fifos
    if (mkfifo(WX_CLIENT_SEND_PIPE, 0666) != 0) {
        DPRINTF("RTL-Wx: Unable to create client send fifo:%s  Exiting...\n",
                                   WX_CLIENT_SEND_PIPE);
       exit(1);
    }
    if (mkfifo(WX_SERVER_SEND_PIPE, 0666) != 0) {
        DPRINTF("RTL-Wx: Unable to create server Send fifo:%s  Exiting...\n",
                                  WX_SERVER_SEND_PIPE);
       exit(1);
    }
    outputDesc = open(WX_SERVER_SEND_PIPE, O_RDWR | O_NDELAY | O_NOCTTY | O_NONBLOCK);  
    if ((outputfd = fopen(WX_SERVER_SEND_PIPE, "w")) == NULL) {
      DPRINTF("RTL-Wx: Unable to open server send pipe %s.  Exiting...\n\n",
                                  WX_SERVER_SEND_PIPE);
      exit(1);
    }
    receiveFilename = WX_CLIENT_SEND_PIPE;
   }
   else if ((opMode == Client) || (opMode == RemoteCommand)) {
     outputfd = stdout;
     receiveFilename = WX_SERVER_SEND_PIPE;
   }
   else { // Standalone mode
    if (chdir(workingDirName) != 0) {
      fprintf(stderr,"TRL-Wx: Unable to set working directory to %s.  Exiting...\n\n", workingDirName);
      exit(1);
    }
    outputfd = stdout;
    receiveFilename = "/dev/tty";
   }

   // Reconfigure the input pipe (or tty if standalone or client) to receive chars through  non-blocking reads char-by-char
   receiveDesc = open(receiveFilename, O_RDWR | O_NDELAY | O_NOCTTY | O_NONBLOCK);
   tcgetattr(receiveDesc, &oldkey);   // save current port settings
   newkey.c_cflag = B38400 | CRTSCTS | CS8 | CLOCAL | CREAD;
   newkey.c_iflag = IGNPAR;
   newkey.c_oflag = 0;
   newkey.c_lflag = 0;
   newkey.c_cc[VMIN] = 1;
   newkey.c_cc[VTIME] = 0;
   tcflush(receiveDesc, TCIFLUSH);
   tcsetattr(receiveDesc, TCSANOW, &newkey);
 
   if ((opMode == Server) || (opMode == Standalone)) {
    if (opMode == Server) {
       DPRINTF("\n");
       DPRINTF("Program started in SERVER Mode\n"); }
    else
       DPRINTF("Program started in STANDALONE Mode\n");
    runServerStandaloneLoop(receiveDesc, outputfd);
    close(outputDesc);
   }
   else if (opMode == RemoteCommand) {
    char cmd;
    if (argc < 3)
       cmd = ' ';
    else
       cmd = argv[2][0];
    runRemoteCommand(receiveDesc, cmd);
   }   
   else {
     DPRINTF("Program started in CLIENT mode\n");
     runClientLoop(receiveDesc);
   }
  tcsetattr(receiveDesc, TCSANOW, &oldkey);
  close(receiveDesc);

  fclose(outputfd);
  if (logfd != NULL)
     fclose(logfd);
  exit((int) 0);
} // end of main

static void init_sensor_lock_and_timeout_info() {
  wxData.idu.LockCode = -1;
  wxData.idu.LockCodeMismatchCount = 0;
  wxData.idu.noDataFor300Seconds = 0;
  wxData.idu.noDataBetweenSnapshots = 0;
  wxData.odu.LockCode = -1;
  wxData.odu.LockCodeMismatchCount = 0;
  wxData.odu.noDataFor300Seconds = 0;
  wxData.odu.noDataBetweenSnapshots = 0;
  wxData.rg.LockCode = -1;
  wxData.rg.LockCodeMismatchCount = 0;
  wxData.rg.noDataFor300Seconds = 0;
  wxData.rg.noDataBetweenSnapshots = 0;
  wxData.wg.LockCode = -1;
  wxData.wg.LockCodeMismatchCount = 0;
  wxData.wg.noDataFor300Seconds = 0;
  wxData.wg.noDataBetweenSnapshots = 0;
  wxData.energy.LockCode = -1;
  wxData.energy.LockCodeMismatchCount = 0;
  wxData.energy.noDataFor300Seconds = 0;
  wxData.energy.noDataBetweenSnapshots = 0;
  wxData.owl.LockCode = -1;
  wxData.owl.LockCodeMismatchCount = 0;
  wxData.owl.noDataFor300Seconds = 0;
  wxData.owl.noDataBetweenSnapshots = 0;
  int i;
  for(i=0;i<=MAX_SENSOR_CHANNEL_INDEX;i++) {
    wxData.ext[i].LockCode = -1;
    wxData.ext[i].LockCodeMismatchCount = 0;    
    wxData.ext[i].noDataFor300Seconds = 0;
    wxData.ext[i].noDataBetweenSnapshots = 0;
  } 
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// This function is called when the program is invoked in standalone (no - options) or in server mode (-s option). 
//                         THIS FUNCTION IS THE MAIN CONTROL LOOP FOR THE PROGRAM
//--------------------------------------------------------------------------------------------------------------------------------------------
void runServerStandaloneLoop(int receiveDesc, FILE *outputfd)
{
  int  status;
  int stop=FALSE;
  char Key;   
  int loopCnt=0;
  
  //  Init various data structures and modules such as WMR9x8 driver, scheduler, dataStore, etc
  WX_Init();

  if(pthread_create(&rtl_433fm_thread_struct, NULL, rtl_433fm_thread, NULL)) {
      fprintf(stderr, "Error creating rtl_433 receiver thread\n");
      exit(0);
  }  
  rtl_decode_register_os_msg_error_callback(WX_process_os_msg_error);
  rtl_decode_register_os_msg_ok_callback(WX_process_os_msg_ok);
  rtl_decode_register_efergy_msg_error_callback(WX_process_efergy_msg_error);
  rtl_decode_register_efergy_msg_ok_callback(WX_process_efergy_msg_ok); 
  rtl_decode_register_owl_msg_error_callback(WX_process_owl_msg_error);
  rtl_decode_register_owl_msg_ok_callback(WX_process_owl_msg_ok);   
  fprintf(outputfd, "Ready to gather weather sensor data \n");
  fprintf(outputfd,"\n   <<Hit 'h' for help, any key for status, ESC to quit>>\n\n");
  fflush(outputfd);

  // Ignore the broken pipe signal.  This just means the test client has shut down and isn't listening or sending to us anymore.
  // By staying running, we'll be there when/if the client starts again (and we'll be processing weather data!)
  signal(SIGPIPE, SIG_IGN);
    
  // flush out any chars in the receive pipe before processing starts. 
  status = 1;
  while (status == 1)  
    status = read(receiveDesc, &Key, 1);


  // The command processing loop here is used as a debug tool when the program is run in standalone mode, or the
  // commands may come from another instance of this program running in client mode or in remote control
  // mode (rtl-wx -c or rtl-wx -r) that sends commands to a server instance (rtl-wx&) over a named pipe.  Client mode is provided
  // for controlling a server instance and provides all of the controls of standalone mode.  Remote control mode is used to
  // connect to a server, send a single command to the server, retrieve the output of the command, then exit.  This supports
  // web browser based control of the server.
  while (stop == FALSE) {
    status = read(receiveDesc, &Key, 1);

    if (status == 1) {   // if a key was hit
       switch (Key) {
          case 0x1b:   /* Esc */
          case 'q':  /* q */
          case 0x03: /* ^C */
            DPRINTF("Shutting down in response to user command\n");
            stop = TRUE;
            break;
          case 'a':
                WX_DumpSchedulerInfo(outputfd);
                break;
          case 'c':
            DPRINTF("Executing user command to read configuration file: %s\n", CONFIG_FILE_PATH);
            WX_DoConfigFileRead();
            break;
          case 'd': 
            WX_DumpSensorInfo(outputfd);
            break;
          case 'e': 
            WX_DumpEnergyHistoryInfo(outputfd, "Efergy", &wxData.energy, ENERGY_HISTORY_SAMPLES_PER_MINUTE);
	    WX_DumpEnergyHistoryInfo(outputfd, "OWL", &wxData.owl, OWL_ENERGY_HISTORY_SAMPLES_PER_MINUTE); 
            break;
          case 'f': 
            DPRINTF("Executing user command to invoke tag file parser\n");
            WX_DoTagFileProcessing();
            break;
          case 'h':
            outputProgramHelp(outputfd);
            break;
          case 'i':
            WX_DumpConfigInfo(outputfd);
            break;
          case 'l':
            if (logfd != NULL)
              fclose(logfd);
            if ((logfd = fopen(LOG_FILE_PATH, "w")) == NULL) {
                fprintf(stderr, "RTL-Wx Error reopening log file %s\n",LOG_FILE_PATH);
                exit(1);
            }
            DPRINTF("Logfile cleared by user command\n");
            break;
          case 'm':
            WX_DumpMaxMinInfo(outputfd);
            break;
          case 'n':
            DPRINTF("Executing user command to reset historical max/min data\n");
            WX_InitHistoricalMaxMinData();
            break;
          case 'r':
            DPRINTF("Executing user command to reset sensor lock code and timout information\n");
            init_sensor_lock_and_timeout_info();
            break;
          case 's':
            DPRINTF("Executing user command to save data snapshot and rain snapshot\n");
            WX_DoDataSnapshotSave(WxConfig.dataSnapshotFrequency);
            WX_DoRainDataSnapshotSave();
            break;
          case 't':
            if (rawxDataDumpMode == TRUE) {
               DPRINTF("Executing user command to disable raw message data display mode.\n");
               rawxDataDumpMode = FALSE; }
            else {
               DPRINTF("Executing user command to enable raw message data display mode.  All bytes will be echoed\n");
               rawxDataDumpMode = TRUE; }
            break;
          case 'u': 
            DPRINTF("Executing user command to do FTP upload\n");   
            fflush(outputfd);
            //  dump the current data to a file and send that file via ftp to a server
            if (WX_DoFtpUpload() != 0) {
                DPRINTF("FTP upload completed successfully\n"); }
            else
                DPRINTF("FTP operation failed.\n");
            break;
          case 'w':
            DPRINTF("Executing user command to save webcam snapshot\n");
            WX_DoWebcamSnapshot();
            break;
          default:
            WX_DumpInfo(outputfd);
            break;
       }
    }   // end if key hit
   fflush(outputfd);
   // Now check to see if it's time to do tag file processing, FTP uploading or other actions...
   WX_DoScheduledActions();
   WX_milliSleep(100);
  } // while (stop == FALSE)
}

void WX_Init() {

  // Initialize all global data to 0
  memset ( &wxData, 0, sizeof(wxData));
  memset ( &WxConfig, 0, sizeof(WxConfig));
  
  time(&WX_programStartTime);
  
  WX_totalBurnerRunSeconds=0;
  
  pthread_rwlock_init(&energy_sample_array_rw_lock, NULL);
 
  // Only init this at startup since it is accessed asynchronously in callback routine
  WxConfig.sensorLockingEnabled = 0;
  init_sensor_lock_and_timeout_info();
  
  WX_processConfigSettingsFile(CONFIG_FILE_PATH , &WxConfig);
  
  WX_InitHistoricalWeatherData(WX_NUM_RECORDS_TO_STORE);
  WX_InitHistoricalRainData(WX_NUM_RAIN_RECORDS_TO_STORE);
  WX_InitActionScheduler(&wxData, &WxConfig);
}

static float get_oregon_scientific_temperature(unsigned char *message, unsigned int sensor_id) {
  float temp_c = 0;
  //if ((sensor_id == 0x1d20) || (sensor_id == 0x1d30) || (sensor_id == 0x5d60) || (sensor_id == 0xf824)) 
  {
    temp_c = (((message[5]>>4)*100)+((message[4]&0x0f)*10) + ((message[4]>>4)&0x0f)) /10.0F;
    if (message[5] & 0x0f)
       temp_c = -temp_c;
  }
  return temp_c;
}
static unsigned int get_oregon_scientific_humidity(unsigned char *message, unsigned int sensor_id) {
 int humidity = 0;
 //if ((sensor_id == 0x1d20) || (sensor_id == 0x1d30) || (sensor_id == 0x5d60) || (sensor_id == 0xf824)) 
 {
    humidity = ((message[6]&0x0f)*10)+(message[6]>>4);
 }
 return humidity;
}

// NOAA function to compute dew point from  celcius temperature and humidity percent 
static float compute_dew_point(float celsius, int humidity)
{
   // (1) Saturation Vapor Pressure = ESGG(T)
   double RATIO = 373.15 / (273.15 + celsius);
   double RHS = -7.90298 * (RATIO - 1);
   RHS += 5.02808 * log10(RATIO);
   RHS += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
   RHS += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
   RHS += log10(1013.246);

   // factor -3 is to adjust units - Vapor Pressure SVP * humidity
   double VP = pow(10, RHS - 3) * humidity;

   // (2) DEWPOINT = F(Vapor Pressure)
   double T = log(VP/0.61078);   // temp var
   return (241.88 * T) / (17.558 - T);
}

static int compute_sealevel_pressure_offset(int altitudeFt, float temp_c) {
  float altitudeMeters = altitudeFt/3.2808;
  float temp_k = temp_c + 273;
  if (temp_k == 0)
    return 0;
  else
    return (int) ((float) (altitudeMeters / (temp_k / 29.263)));
}
void WX_process_efergy_msg_error(unsigned char *msg, int length) {
  // record checksum errors and data decode errors on efergy messages
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM Efergy Msg: "); 
      int i; 
      for (i=0 ; i<20; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, " (Error Detected)\n");
   }
   wxData.BadPktCnt++;
}

void WX_process_efergy_msg_ok(unsigned char *msg, int length, float kilowatts) {
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM Efergy: "); 
      int i; 
      for (i=0 ; i<20; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, "  kW: %5.3f\n", kilowatts);
   }
   
   int sensor_lock_code = msg[2];
   if (wxData.energy.LockCode == -1)
       wxData.energy.LockCode = sensor_lock_code;
   else if (wxData.energy.LockCode != sensor_lock_code)
       wxData.energy.LockCodeMismatchCount++;
   if ((wxData.energy.LockCode == sensor_lock_code) || ( WxConfig.sensorLockingEnabled == 0)) {
       wxData.energy.LockCode = sensor_lock_code;
       wxData.energy.Watts = (int) (kilowatts*1000);
 
       wxData.currentTime.PktCnt++;
       wxData.energy.Timestamp = wxData.currentTime;
       struct tm *localTime = localtime(&wxData.energy.Timestamp.timet);
       int historyIdx=getEnergyHistoryIndex(localTime->tm_min, localTime->tm_sec, ENERGY_HISTORY_SAMPLES_PER_MINUTE);
       pthread_rwlock_wrlock(&energy_sample_array_rw_lock);
       wxData.energy.WattsHistory[historyIdx] = wxData.energy.Watts;
       pthread_rwlock_unlock(&energy_sample_array_rw_lock);
     }
}
void WX_process_owl_msg_error(unsigned char *msg, int length, float watts, float total_kwh) {
  // record  errors and data decode errors on owl messages
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM OWL Msg: "); 
      int i; 
      for (i=0 ; i<length; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, " (Error Detected)\n");
   }
   fprintf(outputfd, " OWLCM119 Error: Current: %5.0f (watts) Total:%7.3f (kW)\n", watts, total_kwh);

   wxData.BadPktCnt++;
}

void WX_process_owl_msg_ok(unsigned char *msg, int length, float watts, float total_kwh) {
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM OWL: "); 
      int i; 
      for (i=0 ; i<length; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, "  Watts: %4.0f   Total kWh: %7.4f\n", watts, total_kwh);
   }
   int sensor_lock_code = msg[2];
   if (wxData.owl.LockCode == -1)
       wxData.owl.LockCode = sensor_lock_code;
   else if (wxData.owl.LockCode != sensor_lock_code)
       wxData.owl.LockCodeMismatchCount++;
   if ((wxData.owl.LockCode == sensor_lock_code) || ( WxConfig.sensorLockingEnabled == 0)) {
       wxData.owl.LockCode = sensor_lock_code;
       wxData.owl.Watts = (int) watts;
 
       wxData.currentTime.PktCnt++;
       wxData.owl.Timestamp = wxData.currentTime;
       struct tm *localTime = localtime(&wxData.owl.Timestamp.timet);
       int historyIdx=getEnergyHistoryIndex(localTime->tm_min, localTime->tm_sec, OWL_ENERGY_HISTORY_SAMPLES_PER_MINUTE);
       pthread_rwlock_wrlock(&energy_sample_array_rw_lock);
       wxData.owl.WattsHistory[historyIdx] = wxData.owl.Watts;
       pthread_rwlock_unlock(&energy_sample_array_rw_lock);
     }
}
void WX_process_os_msg_error(unsigned char *msg, int length) {
  // record  v2.1 bit validation errors and checksum errors on Oregon Scientific v2.1 and v3 messages
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM OS Msg: "); 
      int i; 
      for (i=0 ; i<20; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, " (Error Detected)\n");
   }
  wxData.BadPktCnt++;
}

void WX_process_os_msg_ok(unsigned char *msg, int length, int sensor_id) {
   if (rawxDataDumpMode) { 
      fprintf(outputfd, "RTL-433FM OS Msg: "); 
      int i; 
      for (i=0 ; i<20; i++) 
         fprintf(outputfd, "%02x ", msg[i]); 
      fprintf(outputfd, "\n");
   }
   
   int sensor_rolling_code = (((msg[2]&0x0f)<<4) | (msg[3]>>4));
   if ((sensor_id == 0x1d20) || (sensor_id == 0xf824))    { 
     int  channel = ((msg[2] >> 4)&0x0f);
     if ((channel >= 4) && (sensor_id == 0x1d20))
           channel = 3;
     else if (channel > (MAX_SENSOR_CHANNEL_INDEX+1))
           channel = MAX_SENSOR_CHANNEL_INDEX+1;
     else if (channel < 1)
           channel = 1;
     channel--; // sensor data array is 0 based so sensor 1 -> arrray index 0
     
     float temp_c = get_oregon_scientific_temperature(msg, 0x1d20);
     int humidity = get_oregon_scientific_humidity(msg, 0x1d20);

     //fprintf(stderr, "Weather Sensor THGR122N Channel %d Temp: %3.1f°C  %3.1f°F   Humidity: %d%%\n",  channel, temp_c, ((temp_c*9)/5)+32, humidity);
     //fprintf(stderr, "Weather Sensor THGR810 Channel %d Temp: %3.1f°C  %3.1f°F   Humidity: %d%%\n",  channel, temp_c, ((temp_c*9)/5)+32, humidity);
     if (wxData.ext[channel].LockCode == -1)
       wxData.ext[channel].LockCode = sensor_rolling_code;
     else if (wxData.ext[channel].LockCode != sensor_rolling_code)
       wxData.ext[channel].LockCodeMismatchCount++;
     if ((wxData.ext[channel].LockCode == sensor_rolling_code) || ( WxConfig.sensorLockingEnabled == 0)) {
      wxData.ext[channel].LockCode = sensor_rolling_code;
      wxData.ext[channel].BatteryLow = (msg[3] & 0x40) ? TRUE : FALSE;
      wxData.ext[channel].Temp = temp_c;
      wxData.ext[channel].RelHum = humidity;
      wxData.ext[channel].Dewpoint = compute_dew_point(temp_c, humidity);
      wxData.currentTime.PktCnt++;
      wxData.ext[channel].Timestamp = wxData.currentTime;
      wxData.ext[channel].TempTimestamp = wxData.currentTime;
      wxData.ext[channel].RelHumTimestamp = wxData.currentTime;
      wxData.ext[channel].DewpointTimestamp = wxData.currentTime;
     }
   } else if (sensor_id == 0x1d30) {
     float temp_c = get_oregon_scientific_temperature(msg, 0x1d30);
     int humidity = get_oregon_scientific_humidity(msg, 0x1d30);
     //fprintf(stderr, "Weather Sensor THGR968  Outdoor   Temp: %3.1f°C  %3.1f°F   Humidity: %d%%\n",  temp_c, ((temp_c*9)/5)+32, humidity);
     if (wxData.odu.LockCode == -1)
       wxData.odu.LockCode = sensor_rolling_code;
     else if (wxData.odu.LockCode != sensor_rolling_code)
       wxData.odu.LockCodeMismatchCount++;
     if ((wxData.odu.LockCode == sensor_rolling_code) || ( WxConfig.sensorLockingEnabled == 0)) {
       wxData.odu.LockCode = sensor_rolling_code;
       wxData.odu.BatteryLow = (msg[3] & 0x04) ? TRUE : FALSE;
       wxData.odu.Temp = temp_c;
       wxData.odu.RelHum = humidity;
       wxData.odu.Dewpoint = compute_dew_point(temp_c, humidity);
       wxData.currentTime.PktCnt++;
       wxData.odu.Timestamp = wxData.currentTime;
       wxData.odu.TempTimestamp = wxData.currentTime;
       wxData.odu.RelHumTimestamp = wxData.currentTime;
       wxData.odu.DewpointTimestamp = wxData.currentTime;
     }
   } else if (sensor_id == 0x5d60) {
     float temp_c = get_oregon_scientific_temperature(msg, 0x5d60);
     int humidity = get_oregon_scientific_humidity(msg, 0x5d60);
     int pressure = ((msg[7] & 0x0f) | (msg[8] & 0xf0))+856;
     //fprintf(stderr,"Weather Sensor BHTR968  Indoor    Temp: %3.1f°C  %3.1f°F   Humidity: %d%%", temp_c, ((temp_c*9)/5)+32, humidity);  
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
     //fprintf(stderr, " (%s) Pressure: %dmbar (%s)\n", comfort_str, pressure , forecast_str);  
     if (wxData.idu.LockCode == -1)
       wxData.idu.LockCode = sensor_rolling_code;
     else if (wxData.idu.LockCode != sensor_rolling_code)
       wxData.idu.LockCodeMismatchCount++;
     if ((wxData.idu.LockCode == sensor_rolling_code) || ( WxConfig.sensorLockingEnabled == 0)) {
       wxData.idu.LockCode = sensor_rolling_code;
       wxData.idu.BatteryLow = (msg[3] & 0x04) ? TRUE : FALSE;
       wxData.idu.Temp = temp_c;
       wxData.idu.RelHum = humidity;
       wxData.idu.Dewpoint = compute_dew_point(temp_c, humidity);
       wxData.idu.Pressure = pressure;
       wxData.idu.ForecastStr = forecast_str;
       wxData.idu.SeaLevelOffset = compute_sealevel_pressure_offset(WxConfig.altitudeInFeet, temp_c);
       wxData.currentTime.PktCnt++;
       wxData.idu.Timestamp = wxData.currentTime;
       wxData.idu.TempTimestamp = wxData.currentTime;
       wxData.idu.RelHumTimestamp = wxData.currentTime;
       wxData.idu.DewpointTimestamp = wxData.currentTime;
       wxData.idu.PressureTimestamp = wxData.currentTime;
     }
   } else if (sensor_id == 0x2d10) {
     //fprintf(stderr, "Weather Sensor RGR968   Rain Gauge  Rain Rate: %2.0fmm/hr Total Rain %3.0fmm\n", rain_rate, total_rain);
     float rain_rate = (((msg[4] &0x0f)*100)+((msg[4]>>4)*10) + ((msg[5]>>4)&0x0f)) /10.0F;
     float total_rain = (((msg[7]&0xf)*10000)+((msg[7]>>4)*1000) + ((msg[6]&0xf)*100)+((msg[6]>>4)*10) + (msg[5]&0xf))/10.0F;
     if (wxData.rg.LockCode == -1)
       wxData.rg.LockCode = sensor_rolling_code;
     else if (wxData.rg.LockCode != sensor_rolling_code)
       wxData.rg.LockCodeMismatchCount++;
     if ((wxData.rg.LockCode == sensor_rolling_code) || ( WxConfig.sensorLockingEnabled == 0)) {
       wxData.rg.LockCode = sensor_rolling_code;
       wxData.rg.BatteryLow = (msg[3] & 0x04) ? TRUE : FALSE;
       wxData.rg.Rate = rain_rate;
       wxData.rg.Total = total_rain;
       wxData.currentTime.PktCnt++;
       wxData.rg.Timestamp = wxData.currentTime;
       wxData.rg.RateTimestamp = wxData.currentTime;
     }
   }
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// The client loop code is called when the program is invoked from the command line with the -c option.  In client mode, most
// of the functionality of the program is disabled and it basically becomes a dump terminal talking to another instance
// of itself (hopefully) which is running in server mode.  The reason for this magic is so the server can be left running
// without a tty for long periods.  A client can connect, do debug or check status, then disconnect, without taking the
//server down and losing the historical data that's getting collected.
//--------------------------------------------------------------------------------------------------------------------------------------------
void runClientLoop(int recvFromServer)
{
   struct termios oldkey, newkey;   // place to store old and new keyboard settings
   FILE *sendToServerfd;
   char Key;
   int tty;
   
   printf("\nAttempting to connect to Server...\n\n");

   if ((sendToServerfd = fopen(WX_CLIENT_SEND_PIPE, "w")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open pipe %s for sending commands to server.  Exiting...\n\n",
                                  WX_CLIENT_SEND_PIPE);
      exit(1);
   }

   printf("Suceeded! \n\n");
   fputc('x',sendToServerfd);
   fflush(sendToServerfd);

   // Reconfigure the local console input (tty) to receive chars from user through  non-blocking reads
    // Also, don't want to wait for newline char after user input.
   tty = open("/dev/tty", O_RDWR | O_NOCTTY | O_NONBLOCK);
   tcgetattr(tty, &oldkey);   // save current port settings  
   newkey.c_cflag = 0; //BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
   newkey.c_iflag = 0; //IGNPAR;
   newkey.c_oflag = 0;
   newkey.c_lflag = 0;   // ICANON;
   newkey.c_cc[VMIN] = 1;
   newkey.c_cc[VTIME] = 0;
   tcflush(tty, TCIFLUSH);
   tcsetattr(tty, TCSANOW, &newkey);   

   Key =0;
   while (Key != 'q') {
    char outChar;
    
    Key=0;
     read(tty, &Key, 1);
     if ((Key != 0) && (Key != 'q')) {   // if a key was hit send it to the server
       fputc(Key, sendToServerfd);
       fflush(sendToServerfd);
     }

    outChar = 0;
    read(recvFromServer, &outChar, 1);
    while (outChar != 0) {
       fputc(outChar, stdout);
       outChar = 0;
       read(recvFromServer, &outChar, 1);
    }

    WX_milliSleep(100);
   }

   tcsetattr(tty, TCSANOW, &oldkey);
   close(tty);
   fclose(sendToServerfd);
   printf("RTL-Wx Client exiting...\n");

}

//--------------------------------------------------------------------------------------------------------------------------------------------
// RunRemoteCommand connects to a server instance of the program, sends a single character command to the server,
// and waits for the server output which is sent to stdout.
//--------------------------------------------------------------------------------------------------------------------------------------------
void runRemoteCommand(int recvFromServer, char command)
{
   FILE *sendToServerfd;
   int i;
   char outChar=0;

   if ((sendToServerfd = fopen(WX_CLIENT_SEND_PIPE, "w")) == NULL) {
      fprintf(stderr,"RTL-Wx: Unable to open pipe %s for sending commands to server.  Exiting...\n\n",
                                  WX_CLIENT_SEND_PIPE);
      exit(1);
   }

  // Flush out any stuff left in the server send pipe    
  read(recvFromServer, &outChar, 1);
  while (outChar != 0) {
    outChar = 0;
    read(recvFromServer, &outChar, 1);
  }

   // Don't send a command that would kill server
   if ((command == 'q') || (command == 0x1b) || (command == 03))
      command = ' ';

   // Send command to server
   fputc(command,sendToServerfd);
   fflush(sendToServerfd);
 
   // Hang around for a while processing output.   Look for characters, output them, sleep a bit, then do over
   for(i=0;i<10;i++)  {
    char outChar=0;
    
    read(recvFromServer, &outChar, 1);
    while (outChar != 0) {
       fputc(outChar, stdout);
       fflush(stdout);
       outChar = 0;
       read(recvFromServer, &outChar, 1);
    }
    WX_milliSleep(100);
   }

   fclose(sendToServerfd);
}
void outputProgramHelp(FILE *fd)
{
   fprintf(fd,"\nRTL-Wx Weather Data Monitoring Software\n\n");
   fprintf(fd,"This program attempts to collect data from 433Mhz wireless weather sensors\n");
   fprintf(fd,"or an Oregon Scientific weather station connected through a serial port.\n\n");
   fprintf(fd,"Commands:\n");
   fprintf(fd,"             h  - help (this message)\n");
   fprintf(fd,"             a  - Display Action Scheduler Info\n");
   fprintf(fd,"             c  - Process configuration file\n");
   fprintf(fd,"             d  - Show debugging statistics\n");
   fprintf(fd,"             e  - Show energy data (kilowatts) for last 15 mins\n");
   fprintf(fd,"             f  - invoke file parser - to replace tags w/data\n");
   fprintf(fd,"             i  - Show config info\n");
   fprintf(fd,"             l  - clear log file (rtl-wx.log)\n");
   fprintf(fd,"             m  - show historical max/min data\n");
   fprintf(fd,"             n  - clear historical max/min data\n");
   fprintf(fd,"             r  - reset sensor lock codes and clear timeout counts\n");
   fprintf(fd,"             s  - Save data snapshot now\n");
   fprintf(fd,"             t  - Toggle raw sensor message display mode\n");
   fprintf(fd,"             u  - Initiate FTP upload\n");
   fprintf(fd,"             v  - (not implemented) Trim CSV files (older entries removed\n");
   fprintf(fd,"             w  - Save webcam snapshot\n");
   fprintf(fd,"             q  - quit client (server stays running)\n");
   fprintf(fd,"            ESC - Kill server (and quit client)\n");
   fprintf(fd,"            <Any other key shows current data>\n");
}

void WX_milliSleep(int milliseconds)
{
    struct timespec t;
    t.tv_sec=0;
    t.tv_nsec= 1000000* milliseconds;
    nanosleep(&t, NULL);
}

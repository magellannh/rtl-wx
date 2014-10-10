/*========================================================================
   rtl_433.h 
   
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
#ifndef __RTL_433FM_h
#define __RTL_433FM_h

// rtl-fm decode is suspended for this many seconds after successfully decoding an
// efergy message.  This is to reduce cpu load and keep it cooler based on the assumption
// that efergy messages are evenly spaced (on elite TPM, 10 seconds by default)
#define RTLFM_EFERGY_SLEEP_SECONDS    9

//#define DEFAULT_SAMPLE_RATE     24000 // rtl_fm default rate
#define DEFAULT_SAMPLE_RATE        250000
#define DEFAULT_FREQUENCY          433920000
#define DEFAULT_HOP_TIME           (60*10)
#define DEFAULT_HOP_EVENTS         2
#define DEFAULT_ASYNC_BUF_NUMBER   32
#define R433_DEFAULT_BUF_LENGTH    (16 * 16384)
#define DEFAULT_LEVEL_LIMIT        10000
#define DEFAULT_DECIMATION_LEVEL   0
#define MINIMAL_R433_BUF_LENGTH    512
//#define MAXIMAL_BUF_LENGTH      (256 * 16384)
#define MAXIMAL_R433_BUF_LENGTH    (16 * 16384)
#define FILTER_ORDER               1
#define MAX_PROTOCOLS              10
#define SIGNAL_GRABBER_BUFFER      (12 * R433_DEFAULT_BUF_LENGTH)
#define BITBUF_COLS                34
#define BITBUF_ROWS                5

/* Supported modulation types */
#define     OOK_PWM_D   	1   /* Pulses are of the same length, the distance varies */
#define     OOK_PWM_P   	2   /* The length of the pulses varies */
#define     OOK_MANCHESTER	3   /* Manchester code */

typedef struct {
    unsigned int    id;
    char            name[256];
    unsigned int    modulation;
    unsigned int    short_limit;
    unsigned int    long_limit;
    unsigned int    reset_limit;
    int     (*json_callback)(uint8_t bits_buffer[BITBUF_ROWS][BITBUF_COLS]) ;
} r_device;

struct protocol_state {
    int (*callback)(uint8_t bits_buffer[BITBUF_ROWS][BITBUF_COLS]);

    /* bits state */
    int bits_col_idx;
    int bits_row_idx;
    int bits_bit_col_idx;
    uint8_t bits_buffer[BITBUF_ROWS][BITBUF_COLS];
    int16_t bits_per_row[BITBUF_ROWS];
    int     bit_rows;
    unsigned int modulation;

    /* demod state */
    int pulse_length;
    int pulse_count;
    int pulse_distance;
    int sample_counter;
    int start_c;

    int packet_present;
    int pulse_start;
    int real_bits;
    int start_bit;
    /* pwm limits */
    int short_limit;
    int long_limit;
    int reset_limit;
};

struct dm_state {
    FILE *file;
    int save_data;
    int32_t level_limit;
    int32_t decimation_level;
    int16_t filter_buffer[MAXIMAL_R433_BUF_LENGTH+FILTER_ORDER];
    int16_t* f_buf;
    int analyze;
    int debug_mode;

    /* Signal grabber variables */
    int signal_grabber;
    int8_t* sg_buf;
    int sg_index;
    int sg_len;

    /* Protocol states */
    int r_dev_num;
    struct protocol_state *r_devs[MAX_PROTOCOLS];

};

extern int debug_output;
extern int events;
extern volatile int rtlsdr_do_exit;
extern struct dm_state* rtl_433_demod;
extern r_device oregon_scientific;
extern int rtl_433_a[];
extern int rtl_433_b[];

extern int rtl_433fm_main(int argc, char **argv);
extern void pwm_d_decode(struct dm_state *demod, struct protocol_state* p, int16_t *buf, uint32_t len);
extern void pwm_p_decode(struct dm_state *demod, struct protocol_state* p, int16_t *buf, uint32_t len);
extern void manchester_decode(struct dm_state *demod, struct protocol_state* p, int16_t *buf, uint32_t len);
extern void demod_print_bits_packet(struct protocol_state* p);
extern void demod_reset_bits_packet(struct protocol_state* p);
extern void demod_next_bits_packet(struct protocol_state* p);
extern void demod_add_bit(struct protocol_state* p, int bit);
extern uint16_t *envelope_detect(unsigned char *buf, uint32_t len, int decimate);
extern void low_pass_filter(uint16_t *x_buf, int16_t *y_buf, uint32_t len);
extern void calc_squares();
extern void register_protocol(struct dm_state *demod, r_device *t_dev, uint32_t samp_rate);

extern int oregon_scientific_decode(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]);
extern int acurite_rain_gauge_decode(uint8_t bb[BITBUF_ROWS][BITBUF_COLS]);
extern int efergy_energy_sensor_decode(int16_t *buf, int len, int efergy_debug_level);

#endif

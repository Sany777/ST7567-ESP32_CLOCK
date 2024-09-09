#ifndef DEVICE_SYSTEM_H
#define DEVICE_SYSTEM_H


#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "time.h"



enum BasicConst{
    NO_DATA = -1,
    WEEK_DAYS_NUM           = 7,
    MAX_STR_LEN             = 32,
    API_LEN                 = 32,
    FORBIDDED_NOTIF_HOUR    = 6*60,
    DESCRIPTION_SIZE        = 20,
    FORECAST_LIST_SIZE      = 5,
    NET_BUF_LEN             = 5000,
};

enum Bits{
    BIT_NOTIF_ENABLE            = (1<<0),
    BIT_FORECAST_OK             = (1<<1),
    BIT_SNTP_OK                 = (1<<2),
    BIT_STA_CONF_OK             = (1<<3),
    BIT_ENCODER_ROTATE          = (1<<4),
    BIT_IS_AP_CONNECTION        = (1<<5),
    BIT_IS_STA_CONNECTION       = (1<<6),
    BIT_IS_TIME                 = (1<<7),
    BIT_SERVER_RUN              = (1<<8),
    BIT_IS_AP_CLIENT            = (1<<9),
    BIT_WAIT_PROCCESS           = (1<<10),
    BIT_START_SERVER            = (1<<11),
    BIT_UPDATE_FORECAST_DATA    = (1<<12),
    BIT_IS_LOW_BAT              = (1<<13),
    BIT_BUT_PRESSED             = (1<<14),
    BIT_WAIT_BUT_INPUT          = (1<<15),
    BIT_NEW_DATA                = (1<<16),
    BIT_WAIT_PERIODIC_TASK      = (1<<17),
    BIT_WAIT_SIGNALE            = (1<<18),
    BIT_BUT_LONG_PRESSED        = (1<<19),
    BIT_ERR_SSID_NOT_FOUND      = (1<<20),
    BIT_NEW_MIN                 = (1<<21),
    BIT_CHECK_BAT               = (1<<22),
    STORED_FLAGS                = (BIT_NOTIF_ENABLE),
    BITS_DENIED_SLEEP           = (BIT_WAIT_PROCCESS|BIT_WAIT_BUT_INPUT|BIT_WAIT_PERIODIC_TASK|BIT_WAIT_SIGNALE),
    BITS_NEW_BUT_DATA           = (BIT_BUT_PRESSED|BIT_BUT_LONG_PRESSED|BIT_ENCODER_ROTATE)
};

typedef struct {
    char ssid[MAX_STR_LEN+1];
    char pwd[MAX_STR_LEN+1];
    char city_name[MAX_STR_LEN+1];
    char api_key[API_LEN+1];
    unsigned flags;
    unsigned loud;
    int time_offset;
    unsigned schema[WEEK_DAYS_NUM];
    unsigned *notification;
} settings_data_t;


typedef struct {
    int update_data_time;
    int pop_list[FORECAST_LIST_SIZE];
    int temp_list[FORECAST_LIST_SIZE];
    char desciption[FORECAST_LIST_SIZE][DESCRIPTION_SIZE+1];
} service_data_t;

// --------------------------------------- GPIO
void device_gpio_init(void);
int device_set_pin(int pin, unsigned state);
int get_encoder_val();
int get_encoder_val();
void reset_encoder_val();
int get_but_state();

#define PIN_LCD_BACKLIGHT_EN    GPIO_NUM_26
#define PIN_ENCODER_PIN_A       GPIO_NUM_12
#define PIN_ENCODER_PIN_B       GPIO_NUM_14
#define PIN_ENCODER_BUT         GPIO_NUM_33
#define PIN_DHT20_EN            GPIO_NUM_27
#define PIN_WAKEUP              PIN_ENCODER_BUT
#define PIN_SIG_OUT             GPIO_NUM_25 
#define I2C_MASTER_SCL_IO       GPIO_NUM_19       
#define I2C_MASTER_SDA_IO       GPIO_NUM_22        


// --------------------------------------- common
int device_get_offset();
void device_set_pwd(const char *str);
void device_set_ssid(const char *str);
void device_set_city(const char *str);
void device_set_key(const char *str);
int device_commit_changes();
unsigned device_get_state();
unsigned device_wait_bits_untile(unsigned bits, unsigned time_ms);
void device_set_notify_data(unsigned *schema, unsigned *notif_data);
bool is_signale(const struct tm *tm_info);
unsigned *device_get_schema();
unsigned * device_get_notif();
char *device_get_ssid();
char *device_get_pwd();
char *device_get_api_key();
char *device_get_city_name();
void device_set_offset(int time_offset);
void device_set_loud(int loud);
unsigned device_get_loud();
unsigned device_clear_state(unsigned bits);
void device_set_state_isr(unsigned bits);
void device_clear_state_isr(unsigned bits);
unsigned device_set_state(unsigned bits);
unsigned get_notif_num(unsigned *schema);
void device_init();

float device_get_volt();




#define device_wait_bits(bits) \
    device_wait_bits_untile(bits, 12000/portTICK_PERIOD_MS)
    
#define get_notif_size(schema) \
    (get_notif_num(schema)*sizeof(unsigned))


extern service_data_t service_data;

extern char network_buf[];






#ifdef __cplusplus
}
#endif



#endif
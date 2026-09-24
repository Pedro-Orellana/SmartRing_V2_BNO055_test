#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

//REGISTERS

//bma400
#define REG_CHIPID 0x00
#define REG_ACC_CONFIG0 0x19
#define REG_ACC_CONFIG1 0x1A
#define REG_ACC_CONFIG2 0x1B
#define REG_TAP_CONFIG 0x57
#define REG_TAP_CONFIG1 0x58
#define REG_INT_CONFIG1 0x20
#define REG_INT12_MAP 0x23
#define REG_INT_STAT1 0x0F

//bno055
#define BNO055_ID_ADDR 0x00
#define BNO055_ID_VAL 0xA0
#define BNO055_OPR_MODE_ADDR 0x3D
#define BNO055_OPERATION_MODE_ADDRESS 0x3D
#define BNO055_NDOF_OPERATION_MODE 0x0C
#define BNO055_ANGLE_DATA_START_REG 0x1A
#define BNO055_LINEAR_ACCEL_DATA_START_REG 0x28
#define BNO055_ACCEL_DATA_START_REG 0x08
#define BNO055_PITCH_ANGLE_START_REG 0x1E


//NODELABELS
#define RED_LED_NODELABEL       DT_NODELABEL(redled)
#define GREEN_LED_NODELABEL     DT_NODELABEL(greenled)
#define BLUE_LED_NODELABEL      DT_NODELABEL(blueled)
#define BMA400_INT_NODELABEL    DT_NODELABEL(bma400_int_pin)
#define BNO055_NODELABEL        DT_NODELABEL(bno055)
#define BMA400_NODELABEL        DT_NODELABEL(bma400)


//DEVICES
static const struct gpio_dt_spec redled = GPIO_DT_SPEC_GET(RED_LED_NODELABEL, gpios);
static const struct gpio_dt_spec greenled = GPIO_DT_SPEC_GET(GREEN_LED_NODELABEL, gpios);
static const struct gpio_dt_spec blueled = GPIO_DT_SPEC_GET(BLUE_LED_NODELABEL, gpios);
static const struct gpio_dt_spec bma400_int_pin = GPIO_DT_SPEC_GET(BMA400_INT_NODELABEL, gpios);

static const struct i2c_dt_spec bno055 = I2C_DT_SPEC_GET(BNO055_NODELABEL);
static const struct i2c_dt_spec bma400 = I2C_DT_SPEC_GET(BMA400_NODELABEL);

//BNO055 semaphore
K_SEM_DEFINE(bno055_sem, 0, 1);


//app state enum
enum APP_STATE {
        APP_STATE_IDLE,
        APP_STATE_BNO055_ACTIVATED,
        APP_STATE_BLE_SENDING_DATA
};

static volatile enum APP_STATE app_state = APP_STATE_IDLE;


//work struct
struct k_work unlatch_work;


//LED helper functions
void turn_led_off() {
        //turn all colors off
        gpio_pin_set_dt(&redled, 0);
        gpio_pin_set_dt(&greenled, 0);
        gpio_pin_set_dt(&blueled, 0);
}

void turn_led_on(char color) {
        switch (color)
        {
        case 'R':
                gpio_pin_set_dt(&redled, 1);
                gpio_pin_set_dt(&greenled, 0);
                gpio_pin_set_dt(&blueled, 0);
                break;
        
        case 'G':
                gpio_pin_set_dt(&redled, 0);
                gpio_pin_set_dt(&greenled, 1);
                gpio_pin_set_dt(&blueled, 0);
                break;
        
        case 'B':
                gpio_pin_set_dt(&redled, 0);
                gpio_pin_set_dt(&greenled, 0);
                gpio_pin_set_dt(&blueled, 1);
                break;

        default:
                break;
        }
}

//define timer and expiry function
void ble_timer_handler(struct k_timer *timer) {
        turn_led_off();
        app_state = APP_STATE_IDLE;
}

K_TIMER_DEFINE(ble_timer, ble_timer_handler, NULL);

void unlatch_interrupt()
{
        uint8_t addr = REG_INT_STAT1;
        uint8_t read_data;
        i2c_write_read_dt(&bma400, &addr, 1, &read_data, 1);

        // if (read_data == 0x08)
        // {
        //         printk("This really is a double tap!\n");
        // }
        // else
        // {
        //         printk("Read something else... value:%.2X", read_data);
        // }
}

void work_handler(struct k_work *work) {
        unlatch_interrupt();
        //turn green LED on and activate BNO055
        if(app_state == APP_STATE_IDLE) {
                k_sem_give(&bno055_sem);
                app_state = APP_STATE_BNO055_ACTIVATED;
                turn_led_on('G');
        
        //turn LED off and make app idle
        } else if (app_state == APP_STATE_BNO055_ACTIVATED) {
                app_state = APP_STATE_IDLE;
                turn_led_off();
        }
}



//interrupt callback struct
struct gpio_callback bma400_int_cb;


//bma400 interrupt function
//interrupt service routine
void bma400_interrupt_handler(
        const struct device *port,
        struct gpio_callback *cb,
        gpio_port_pins_t pins) {
      k_work_submit(&unlatch_work);
}



//setup function for BMA400
int configure_bma400()
{
        uint8_t data = 0;
        int ret = 0;

        // acc_config0
        data = 0x82;
        ret = i2c_burst_write_dt(&bma400, REG_ACC_CONFIG0, &data, 1);
        if (ret)
        {
                return ret;
        }

        // acc_config1
        data = 0xE9;
        ret = i2c_burst_write_dt(&bma400, REG_ACC_CONFIG1, &data, 1);
        if (ret)
        {
                return ret;
        }

        // acc_config2
        data = 0xE0;
        ret = i2c_burst_write_dt(&bma400, REG_ACC_CONFIG2, &data, 1);
        if (ret)
        {
                return ret;
        }

        // tap_config
        // data = 0x02 for less sensitivity
        data = 0x00;
        ret = i2c_burst_write_dt(&bma400, REG_TAP_CONFIG, &data, 1);
        if (ret)
        {
                return ret;
        }

        // tap_config1
        data = 0x0E;
        ret = i2c_burst_write_dt(&bma400, REG_TAP_CONFIG1, &data, 1);
        if (ret)
        {
                return ret;
        }

        // int_config1
        data = 0x88;
        ret = i2c_burst_write_dt(&bma400, REG_INT_CONFIG1, &data, 1);
        if (ret)
        {
                return ret;
        }

        // int12_map
        data = 0x04;
        ret = i2c_burst_write_dt(&bma400, REG_INT12_MAP, &data, 1);
        if (ret)
        {
                return ret;
        }

        return ret;
}


void configure_bno055()
{
        uint8_t bno055_op_mode_data[] = {BNO055_OPERATION_MODE_ADDRESS, BNO055_NDOF_OPERATION_MODE};
        i2c_write_dt(&bno055, bno055_op_mode_data, 2);
}


int configure_leds_interrupt() {

        int ret = 0;
        ret = gpio_pin_configure_dt(&redled, GPIO_OUTPUT_INACTIVE);
        if(ret){
                return ret;
        }

        ret = gpio_pin_configure_dt(&greenled, GPIO_OUTPUT_INACTIVE);
        if(ret) {
                return ret;
        }

        ret = gpio_pin_configure_dt(&blueled, GPIO_OUTPUT_INACTIVE);
        if(ret) {
                return ret;
        }

        ret = gpio_pin_configure_dt(&bma400_int_pin, GPIO_INPUT);
         if(ret) {
                return ret;
        }
        gpio_pin_interrupt_configure_dt(&bma400_int_pin, GPIO_INT_EDGE_TO_ACTIVE);

        return ret;
}


int main(void)  {
    
        int ret;

        //configure work
        k_work_init(&unlatch_work, work_handler);

        //configure sensors
        configure_bma400();
        configure_bno055();

        //configure leds and interrupt
       
        ret = configure_leds_interrupt();
        if(ret) {
                return ret;
        } 

        //register interrupt handler with the callback
        gpio_init_callback(&bma400_int_cb, bma400_interrupt_handler, BIT(bma400_int_pin.pin));
        gpio_add_callback_dt(&bma400_int_pin, &bma400_int_cb);


        return 0;
}



//BNO055 thread
void bno055_thread_handler() {
        while (true) {
                k_sem_take(&bno055_sem, K_FOREVER);

                while (app_state == APP_STATE_BNO055_ACTIVATED) {
                        //read sensor data
                        uint8_t angle_data [6];

                           //read angle data into angle_data buffer
                        i2c_burst_read_dt(&bno055, BNO055_ANGLE_DATA_START_REG, angle_data, 6);


                        //format the data so that it can be displayed on the screen

                        //angle data
                        int16_t heading_value = (angle_data[1] << 8) | angle_data[0];
                        int16_t roll_value = (angle_data[3] << 8)| angle_data[2];
                        int16_t pitch_value = (angle_data[5] << 8) | angle_data[4];

                        //getting raw values

                        //important value is pitch = 1280
                        //That is equal to pitch = 80 degrees

                        if(pitch_value >= 1280) {
                                //user has moved finger up, turn red LED on and start logging angle data for 1.5 seconds
                                turn_led_on('R');
                                app_state = APP_STATE_BLE_SENDING_DATA;
                                
                                //start timer, since data should only be sent for 1.5 seconds
                                k_timer_start(&ble_timer, K_MSEC(1500), K_NO_WAIT);
                                break;
                        }

                }


                while(app_state == APP_STATE_BLE_SENDING_DATA) {
                        //part where we send data through BLE
                        
                        //read sensor data
                        uint8_t angle_data [6];

                           //read angle data into angle_data buffer
                        i2c_burst_read_dt(&bno055, BNO055_ANGLE_DATA_START_REG, angle_data, 6);


                        //format the data so that it can be displayed on the screen

                        //angle data
                        int16_t heading_value = (angle_data[1] << 8) | angle_data[0];
                        int16_t roll_value = (angle_data[3] << 8)| angle_data[2];
                        int16_t pitch_value = (angle_data[5] << 8) | angle_data[4];
                }
        }
}

K_THREAD_DEFINE(bno055_thread, 1024, bno055_thread_handler, NULL, NULL, NULL, 7, 0, 0);

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


//work struct
struct k_work unlatch_work;

void unlatch_interrupt()
{
        uint8_t addr = REG_INT_STAT1;
        uint8_t read_data;
        i2c_write_read_dt(&bma400, &addr, 1, &read_data, 1);

        if (read_data == 0x08)
        {
                printk("This really is a double tap!\n");
        }
        else
        {
                printk("Read something else... value:%.2X", read_data);
        }
}

void work_handler(struct k_work *work) {
        unlatch_interrupt();
}



//interrupt callback struct
struct gpio_callback bma400_int_cb;


//bma400 interrupt function
//interrupt service routine
void bma400_interrupt_handler(
        const struct device *port,
        struct gpio_callback *cb,
        gpio_port_pins_t pins) {
      printk("Double tap has been registered!\n");
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
                printk("There was an error writing a register\n");
                return ret;
        }

        // acc_config1
        data = 0xE9;
        ret = i2c_burst_write_dt(&bma400, REG_ACC_CONFIG1, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
                return ret;
        }

        // acc_config2
        data = 0xE0;
        ret = i2c_burst_write_dt(&bma400, REG_ACC_CONFIG2, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
                return ret;
        }

        // tap_config
        // data = 0x02 for less sensitivity
        data = 0x00;
        ret = i2c_burst_write_dt(&bma400, REG_TAP_CONFIG, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
                return ret;
        }

        // tap_config1
        data = 0x0E;
        ret = i2c_burst_write_dt(&bma400, REG_TAP_CONFIG1, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
                return ret;
        }

        // int_config1
        data = 0x88;
        ret = i2c_burst_write_dt(&bma400, REG_INT_CONFIG1, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
                return ret;
        }

        // int12_map
        data = 0x04;
        ret = i2c_burst_write_dt(&bma400, REG_INT12_MAP, &data, 1);
        if (ret)
        {
                printk("There was an error writing a register\n");
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
                printk("Could not perform setup for red LED: %d", ret);
                return ret;
        }

        ret = gpio_pin_configure_dt(&greenled, GPIO_OUTPUT_INACTIVE);
        if(ret) {
                printk("Could not perform setup for green LED: %d", ret);
                return ret;
        }

        ret = gpio_pin_configure_dt(&blueled, GPIO_OUTPUT_INACTIVE);
        if(ret) {
                printk("Could not perform setup for blue LED: %d", ret);
                return ret;
        }

        ret = gpio_pin_configure_dt(&bma400_int_pin, GPIO_INPUT);
         if(ret) {
                printk("Could not set interrupt pin: %d", ret);
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
                printk("There was some error configuring LEDs: %d \n", ret);
                return ret;
        } else {
                printk("LEDs and interrupt pin have been successfully setup \n");
        }


        //register interrupt handler with the callback
        gpio_init_callback(&bma400_int_cb, bma400_interrupt_handler, BIT(bma400_int_pin.pin));
        gpio_add_callback_dt(&bma400_int_pin, &bma400_int_cb);


   


        return 0;
}

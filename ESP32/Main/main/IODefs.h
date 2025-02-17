/*
 * IODefs.h
 *
 *  Created on: 7 Jan 2019
 *      Author: xasin
 */

#ifndef MAIN_IODEFS_H_
#define MAIN_IODEFS_H_

#define PIN_I2C_SDA GPIO_NUM_16
#define PIN_I2C_SCL GPIO_NUM_17

#define PIN_IR_OUT	GPIO_NUM_33
#define PIN_IR_IN	GPIO_NUM_35

#define PIN_I2S_DATA GPIO_NUM_25
#define PIN_I2S_BLCK GPIO_NUM_32
#define PIN_I2S_LRCK GPIO_NUM_14

#define PIN_VBRT	GPIO_NUM_13

#define PIN_TRIGR	GPIO_NUM_22

#define PIN_BAT_MES 	GPIO_NUM_2
#define ADC_BAT_MES		ADC1_CHANNEL_5
#define PIN_BAT_CHGING	GPIO_NUM_4

#define PIN_WS2812_OUT	GPIO_NUM_27
#define WS2812_NUMBER 	4

#define PIN_BAT_GREEN	GPIO_NUM_34
// none of the battery monitoring is currently being used, I believe this is just a random pin.

#define PIN_BAT_RED		GPIO_NUM_18

#define PIN_CONN_IND	GPIO_NUM_21

#define PIN_CTRL_FWD	GPIO_NUM_2
#define PIN_CTRL_BACK	GPIO_NUM_0
#define PIN_CTRL_DOWN	GPIO_NUM_4
#endif /* MAIN_IODEFS_H_ */

#endif /* MAIN_IODEFS_H_ */
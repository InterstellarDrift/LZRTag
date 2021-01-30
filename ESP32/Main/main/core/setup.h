/*
 * setup.h
 *
 *  Created on: 7 Jan 2019
 *      Author: xasin
 */

#ifndef MAIN_SETUP_H_
#define MAIN_SETUP_H_

#include "xasin/BatteryManager.h"

#include <xasin/audio.h>
#include <xasin/neocontroller.h>

#include "xasin/mqtt/Handler.h"

#include "player.h"
#include "GunHandler.h"

#include "xasin/LSM6DS3.h"

namespace LZR {

enum CORE_WEAPON_STATUS {
	INITIALIZING,
	DISCHARGED,
	CHARGING,
	NOMINAL,
};

extern CORE_WEAPON_STATUS main_weapon_status;

extern Housekeeping::BatteryManager battery;

extern Xasin::Audio::TX	audioManager;
extern Xasin::NeoController::NeoController RGBController;
extern Xasin::I2C::LSM6DS3				gyro;

extern Xasin::MQTT::Handler mqtt;

extern LZR::Player	player;
extern Lasertag::GunHandler	gunHandler;

void setup();

uint8_t read_nav_switch();

}

#endif /* MAIN_SETUP_H_ */

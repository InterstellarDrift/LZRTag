/*
 * GunHandler.cpp
 *
 *  Created on: 18 Jan 2019
 *      Author: xasin
 */

#include "GunHandler.h"

#include "setup.h"
#include "IR.h"

#include "../weapons/wyre.h"
#include "../weapons/mylin.h"
#include "../weapons/zinger.h"

#include "../fx/sounds.h"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#include <cmath>
#include <sstream>
#include <iostream>

namespace Lasertag {

GunHandler::GunHandler(gpio_num_t trgPin, Xasin::Audio::TX &audio)
	:	mqttAmmo(0), lastMQTTPush(0),
		fireState(NO_GUN),
		currentGunID(0),
		shotTick(0), salveCounter(0), lastShotTick(0),
		lastTick(0),
		last_shot_sound(nullptr),
		gunHeat(0),
		triggerPin(trgPin), pressAlreadyTriggered(false),
		shot_performed(false),
		audio(audio) {

	gpio_set_direction(triggerPin, GPIO_MODE_INPUT);
	gpio_set_pull_mode(triggerPin, GPIO_PULLUP_PULLDOWN);
	gpio_pulldown_en(triggerPin);
	// gpio_pullup_en(triggerPin);

	esp_log_level_set(GUN_TAG, ESP_LOG_DEBUG);

	LZR::player.mqtt.subscribe_to(LZR::player.get_topic_base() + "/Stats/Ammo/#",
		[this](Xasin::MQTT::MQTT_Packet data) {
			if(0 >= currentGunID || currentGunID > 3)
				return;

			if(data.topic == "SetReserve")
				cGun().currentReserveAmmo = atoi(data.data.data());
			else if(data.topic == "SetClip")
				cGun().currentClipAmmo = atoi(data.data.data());
		});
}

GunSpecs &GunHandler::cGun() {
	if(0 >= currentGunID || currentGunID > 3)
		return LZR::Weapons::wyre;

	GunSpecs * gunSets[] = {
			&LZR::Weapons::wyre,
			&LZR::Weapons::mylin,
			&LZR::Weapons::zinger,
	};

	return *gunSets[currentGunID-1];
}

std::string bool_as_text(bool b)
{
    std::stringstream converter;
    converter << std::boolalpha << b;   // flag boolalpha calls converter.setf(std::ios_base::boolalpha)
    return converter.str();
}

bool GunHandler::triggerPressed() {
	ESP_LOGI("InterstellarDrift_Debugging", "TRIGGER CHECKED");
	bool isTriggered = gpio_get_level(triggerPin) == 0;
	int pinInt = gpio_get_level(triggerPin);
	std::string tmp = std::to_string(pinInt);
	char const *num_char = tmp.c_str();
	printf("Pin State: %s \n", num_char);
	// printf("PIN STATE" + int pinInt);
	printf("\n");
	// if(pinInt == 0) {
	// 	ESP_LOGI("InterstellarDrift_Debugging", "Pin Value 0");
	// } else {
	// 	ESP_LOGI("InterstellarDrift_Debugging", "Pin Value 1");

	// }
	
	// if(isTriggered)
	// 	ESP_LOGI("InterstellarDrift_Debugging", "TRIGGER PRESSED ??????");
	// else
	// 	ESP_LOGI("InterstellarDrift_Debugging", "TRIGGER IS NOT PRESSED");


	return isTriggered;
	
}

void GunHandler::handle_shot() {
	if(cGun().currentClipAmmo > 0)
		cGun().currentClipAmmo--;
	// We have to abort the salve if we have no ammo left
	// This also plays the empty click
	else if(cGun().currentClipAmmo == 0) {
		LZR::Sounds::play_audio("CLICK");
		pressAlreadyTriggered = true;

		set_fire_state(POST_SALVE_RELEASE);

		return;
	}

	gunHeat += cGun().perShotHeatup;

	set_fire_state(POST_SHOT_DELAY);
	lastShotTick = xTaskGetTickCount();

	shot_performed = true;

	add_sound(cGun().shotSounds);

	LZR::IR::send_signal();
	ESP_LOGD("InterstellarDrift Debugging", "IR FIRED SHOT");

}

void GunHandler::deny_beep() {
	if(triggerPressed() && !pressAlreadyTriggered) {
		pressAlreadyTriggered = true;
		LZR::Sounds::play_audio("DENY");
	}
}

void GunHandler::set_fire_state(FIRE_STATE newState) {
	if(newState == fireState)
		return;

	fireState = newState;
	shotTick = xTaskGetTickCount();
	switch(fireState) {
		default: shotTick = 0; break;

		case WEAPON_SWITCH_DELAY:
			shotTick += cGun().weaponSwitchDelay;
		break;
		case RELOAD_DELAY:
			shotTick += cGun().perReloadDelay;
		break;

		case POST_TRIGGER_RELEASE:
		case POST_TRIGGER_DELAY:
			shotTick += cGun().postTriggerTicks;
		break;
		case POST_SHOT_DELAY:
			shotTick += (cGun().perShotDelay*(95 + esp_random()%10))/100;
		break;
		case POST_SALVE_RELEASE:
		case POST_SALVE_DELAY:
			shotTick += cGun().postSalveDelay;
		break;
	}
}

void GunHandler::handle_reload_delay() {

	if(		(cGun().currentClipAmmo < 0)				// Detect infinite clip ammo and skip reload
		||	(cGun().currentClipAmmo >= cGun().clipSize) // We don't need to reload if our clip ammo is at or above maximum
		||	(cGun().currentReserveAmmo == 0)) {			// Equally, if we have nothing more to reload with, skip.

		set_fire_state(WAIT_ON_VALID);
		LZR::player.should_reload = false;
		return;
	}

	// Skip reloading if we aren't being forced to reload and the player
	// pressed the trigger.
	// Useful for shotgun-type weapons with piecewise reloading
	if(!LZR::player.should_reload && triggerPressed()) {
		set_fire_state(WAIT_ON_VALID);
		return;
	}
	deny_beep();

	if(xTaskGetTickCount() < shotTick)
		return;

	// Clear the reloading flag from the player
	LZR::player.should_reload = false;

	// Cap refill amount to how much we have left
	// This also handles infinite refill ammo (reserveAmmo < 0)
	auto refillAmount = cGun().perReloadRecharge;
	if(cGun().currentReserveAmmo >= 0)
		refillAmount = fmin(refillAmount, cGun().currentReserveAmmo);

	// Cap the refilled ammo amount to our clip size
	auto newAmmo = fmin(cGun().clipSize, cGun().currentClipAmmo + refillAmount);

	// Subtract the amount of ammo we are refilling from the reserve ammo
	// Also tracking infinite reserve ammo
	if(cGun().currentReserveAmmo >= 0)
		cGun().currentReserveAmmo -= newAmmo - cGun().currentClipAmmo;
	cGun().currentClipAmmo = newAmmo;

	// If our clip isn't full yet, continue reloading if we can.
	// Think of a shotgun that is loaded clip by clip
	if((cGun().currentClipAmmo < cGun().clipSize) && cGun().currentReserveAmmo != 0) {
		shotTick += cGun().perReloadDelay;
		ESP_LOGD(GUN_TAG, "Reloaded a little, continuing");
	}
	else {
		set_fire_state(WAIT_ON_VALID);
		ESP_LOGD(GUN_TAG, "Reload complete!");
		LZR::Sounds::play_audio("RELOAD FULL");
	}
}

void GunHandler::handle_wait_valid() {
	// Players may not shoot nor reload if they are
	// disabled!
	if(!LZR::player.can_shoot()) {
		deny_beep();
		return;
	}

	// First, check if a reload needs to be forced. This happens only
	// when we have no more ammo at all, but have some in reserve, or
	// if the player actively wants to reload
	if((cGun().currentClipAmmo == 0 && cGun().currentReserveAmmo != 0)
			||
		(LZR::player.should_reload)) {
		LZR::player.should_reload = true;

		set_fire_state(RELOAD_DELAY);
		return;
	}

	// Wait for the user to press the trigger
	if(!triggerPressed())
		ESP_LOGI("InterstellarDrift_Debugging", "TRIGGER PRESSED");
		return;

	// Play an empty clip sound if we don't have any more ammo and
	// reloading didn't work either.
	if(cGun().currentClipAmmo == 0) {
		if(!pressAlreadyTriggered)
			LZR::Sounds::play_audio("CLICK");
		pressAlreadyTriggered = true;

		return;
	}

	// Mark that this press was already handled - used later!
	pressAlreadyTriggered = true;

	// Otherwise, continue with our magic~
	if(cGun().postTriggerTicks != 0) {
		add_sound(cGun().chargeSounds);
	}

	set_fire_state(POST_TRIGGER_RELEASE);
}

void GunHandler::shot_tick() {
	shot_performed = false;

	auto playerGunID = LZR::player.get_gun_num();
	if(playerGunID != currentGunID) {
		currentGunID = playerGunID;

		if(playerGunID == 0)
			fireState = NO_GUN;
		else {
			fireState = WEAPON_SWITCH_DELAY;
			shotTick = xTaskGetTickCount() + cGun().weaponSwitchDelay;
			LZR::Sounds::play_audio("SWITCH WEAPON");
		}
	}

	FIRE_STATE oldState;
	do {
		oldState = fireState;


		switch(fireState) {
		case NO_GUN:
			break;

		case WEAPON_SWITCH_DELAY:
			deny_beep();

			if(xTaskGetTickCount() < shotTick)
				break;

			fireState = WAIT_ON_VALID;
		break;

		case RELOAD_DELAY:
			handle_reload_delay();
		break;

		case WAIT_ON_VALID:
			handle_wait_valid();
		break;
		case POST_TRIGGER_RELEASE:
			if(triggerPressed() && cGun().postTriggerRelease)
				break;

			fireState = POST_TRIGGER_DELAY;
		break;

		case POST_TRIGGER_DELAY:
			if(xTaskGetTickCount() >= shotTick) {
				salveCounter = cGun().shotsPerSalve;
				handle_shot();
				break;
			}
		break;

		case POST_SHOT_DELAY:
			if(xTaskGetTickCount() < shotTick)
				break;

			if(--salveCounter > 0) {
				handle_shot();
				break;
			}

			set_fire_state(POST_SALVE_RELEASE);
		break;

		case POST_SALVE_RELEASE:
			if(pressAlreadyTriggered && cGun().postSalveRelease)
				break;

			fireState = POST_SALVE_DELAY;
		break;

		case POST_SALVE_DELAY:
			if(xTaskGetTickCount() >= shotTick) {
				fireState = WAIT_ON_VALID;

				if(!triggerPressed() && !cGun().postSalveRelease && !cGun().postTriggerRelease) {
					if(gunHeat > 0.4)
						add_sound(cGun().cooldownSounds);
				}
			}
		break;
		}
	} while(oldState != fireState);

	// Clear the trigger handle flag on release
	// Is used to require re-pressing the trigger for some actions
	if(!triggerPressed()) {
		pressAlreadyTriggered = false;
		// ESP_LOGI("InterstellarDrift_Debugging", "TRIGGER WAS RELEASED");
	}
}

void GunHandler::fx_tick() {
	gunHeat *= cGun().perTickCooldown;

//	TickType_t cooldownSoundTick = lastShotTick + cGun().postShotCooldownTicks;
//	if(xTaskGetTickCount() >= cooldownSoundTick && lastTick < cooldownSoundTick)
//		Xasin::Audio::ByteCassette::play(audio, cGun().cooldownSounds);
}

bool GunHandler::was_shot_tick() {
	return shot_performed;
}
TickType_t GunHandler::timeSinceLastShot() {
	return xTaskGetTickCount() - lastShotTick;
}
uint8_t GunHandler::getGunHeat() {
	if(gunHeat > 1)
		return 255;
	if(gunHeat < 0)
		return 0;

	return gunHeat * (255-3) + 3;
}

void GunHandler::add_sound(const GunSoundCollection &sounds) {
	if(sounds.size() == 0)
		return;

	auto next_sound = audio.play(sounds, false);

	if(last_shot_sound != nullptr) {
		last_shot_sound->fade_out();
		last_shot_sound->release();
	}
	last_shot_sound = next_sound;
}

void GunHandler::tick() {
	shot_tick();
	fx_tick();

	if((cGun().currentClipAmmo != mqttAmmo) && (xTaskGetTickCount() > (lastMQTTPush+300))) {
		ESP_LOGD(GUN_TAG, "Weapon is: %d/%d (%d)", cGun().currentClipAmmo, cGun().clipSize, cGun().currentReserveAmmo);
		struct {
			int32_t currentAmmo;
			int32_t clipSize;
			int32_t reserveAmmo;
		} ammoData = {cGun().currentClipAmmo, cGun().clipSize, cGun().currentReserveAmmo};
		LZR::mqtt.publish_to(LZR::player.get_topic_base() +"/Stats/Ammo", &ammoData, sizeof(ammoData), 0);

		mqttAmmo = cGun().currentClipAmmo;
		lastMQTTPush = xTaskGetTickCount();
	}

	lastTick = xTaskGetTickCount();
}

} /* namespace Lasertag */

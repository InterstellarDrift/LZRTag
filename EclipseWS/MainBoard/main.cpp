/*
 * main.cpp
 *
 *  Created on: May 9, 2016
 *      Author: xasin
 */


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "Libcode/TIMER/Timer0.h"
#include "Localcode/Board/Board.h"

#include "Localcode/Connector.h"

#include "Localcode/Game/Player.h"

ISR(TIMER1_COMPA_vect) {
	Connector::update();
}

int main() {

	Connector::init();

	Game::Player::set_team(3);

	while(true) {
	}

	return 0;
}

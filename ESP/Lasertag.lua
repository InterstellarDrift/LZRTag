
dofile("UARTReceive.lc");

function setVestBrightness(c)
	uart.write(0, 200, c);
end

function setVestColor(c)
	uart.write(0, 101, c);
end
function overrideVest(duration, brightness)
	duration = duration/90;
	uart.write(0, 12, duration%255, duration/255, brightness);
end

function ping(sFreq, eFreq, duration)
	duration = duration or 20;
	sFreq = sFreq or 4000;
	eFreq = eFreq or sFreq;
	uart.write(0, 11, duration%255, duration/255, sFreq%255, sFreq/255, eFreq%255, eFreq/255);
end

function vibrate(duration)
	uart.write(0, 10, duration%255, duration/255);
end
function setVibratePattern(n)
	uart.write(0, 110, n);
end

function fireWeapon()
	if(player.id) then
		uart.write(0, 0, 0);
	end
end

homeQTT:publish(playerTopic .. "/Connection", "CONNECTING", 1, 1);
on_mqtt_sub_finish = function()
	homeQTT:publish(playerTopic .. "/Connection", "OK", 1, 1);
	systemIsSetUp = true;
	setVestColor(game.team);
	ping(5000, 1000, 50);
end

setVestBrightness(0);
setVestColor(5);

function fancyPling()
	ping(1000, 5000, 150);
	vibrate(50);
end
fancyPling();
tmr.create():alarm(200, tmr.ALARM_SINGLE, fancyPling);

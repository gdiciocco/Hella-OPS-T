/*
 	Sample verbose implementation of HELLA OPS+T (6PR 010 378-107) Oil Pressure and temperature sensor
	https://www.hella.com/municipal/assets/media_global/KI_OPS_T_HELLA_EN.pdf
	Watching pulse.total_time you can evaluate the skew between your uC and the sensor, 
	on a mega2560 with this code I've got an average error of 2.2%	
	Sensor uses PPM Pulse-Position modulation: https://en.wikipedia.org/wiki/Pulse-position_modulation
	
	Giacomo Di Ciocco - Nov 2020
*/
struct pulse {
	int index = 0; // Index of the pulse we are on, frame is composed by three pulses
	unsigned long on_time; // Time duration of the HIGH level 
	unsigned long off_time; // Time duration of the LOW level
	unsigned long total_time; // Time duration of the whole symbol
	unsigned long cur_event; // micros() time of current ISR call
	unsigned long last_event; // micros() time of last ISR call
	int value;
} pulse;

struct sensor {
	double temperature; // Celsius temperature representation of the relative (error corrected) pulse according to the datasheet
	double pressure; // Pressure representation of the relative (error corrected) pulse according to the datasheet
	double status; // Diagnosis pulse, containing its (error corrected) value
} sensor;

int inPin = 53;
byte current_level; // Keep track of the current level
byte got_sync = 0; // Keep track of synchronization on the first pulse

void setup() {
  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT0);
  Serial.begin(9600);
}

void loop() {
  print();
  delay(100);
}

ISR(PCINT0_vect) {
  pulse.cur_event = micros();
  // Last event was LOW and we have got a rising edge
  if(current_level == 0 && digitalRead(inPin) ) {
    pulse.off_time = pulse.cur_event - pulse.last_event;
	pulse.total_time = pulse.off_time + pulse.on_time;
    current_level = 1;
    pulse.last_event = pulse.cur_event;
	if (pulse.total_time <= 1024 && pulse.index == 0) {
        pulse.index++;
		got_sync = 1; 
		sensor.status = (1024.0/pulse.total_time)*pulse.on_time;
	} else if (pulse.index == 1) {
		sensor.temperature = ((4096.0/pulse.total_time)*pulse.on_time-128)/19.2-40;
		pulse.index++;
	} else if (pulse.index == 2) {
		sensor.pressure = (((4096.0/pulse.total_time)*pulse.on_time)-128)/384.0+0.5;
		pulse.index = 0;
	} 
  } else if(current_level == 1 && !digitalRead(inPin) ) { // Last event was HIGH and we have got a falling edge
    pulse.on_time = pulse.cur_event - pulse.last_event;
    current_level = 0;
    pulse.last_event = pulse.cur_event;
  }
}

void print() {
	if (got_sync) {
		Serial.print(sensor.temperature);
		Serial.print(" °C - ");
		Serial.print(sensor.pressure);
		Serial.print(" Bar - ");
		if (sensor.status >= 256-25 && sensor.status <= 256+25) {
			Serial.println(pulse.total_time);
		} else if (sensor.status >= 384-25 && sensor.status <= 384+25) {
			Serial.println("Pressure detection malfunction");
		} else if (sensor.status >= 512-25 && sensor.status <= 512+25) {
			Serial.println("Temperature detection malfunction");
		} else if (sensor.status >= 3640-25 && sensor.status <= 640+25) {
			Serial.println("Generic Hardware failure");
		}
	} else {
			Serial.println("PPM - Waiting for sync");
	}
}

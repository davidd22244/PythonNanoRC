 #include <Arduino.h>
 #include <stdlib.h>
 #include <string.h>
 #include <string.h>

/*
 * PythonNanoRC serial control protocol
 *
 * Transport: UTF-8/ASCII lines terminated by '\n' or '\r\n'.
 * Responses always start with OK, ERR, or DATA.
 * Example commands:
 *   PING
 *   PINMODE 13 OUTPUT
 *   DIGITALWRITE 13 1
 *   DIGITALREAD 13
 *   ANALOGREAD A0
 *   PWM 9 128
 *   WAIT 250
 *   SCRIPT BEGIN
 *   DIGITALWRITE 13 1
 *   WAIT 500
 *   DIGITALWRITE 13 0
 *   SCRIPT END
 *   SCRIPT RUN
 */

const unsigned long SERIAL_BAUD = 115200;
const size_t INPUT_BUFFER_SIZE = 96;
const size_t SCRIPT_LINE_SIZE = 96;
const size_t MAX_SCRIPT_LINES = 32;

char inputBuffer[INPUT_BUFFER_SIZE];
size_t inputLength = 0;
bool receivingScript = false;
bool scriptReady = false;
char scriptLines[MAX_SCRIPT_LINES][SCRIPT_LINE_SIZE];
size_t scriptLength = 0;

void sendError(const __FlashStringHelper *message) {
	Serial.print(F("ERR "));
	Serial.println(message);
}

void sendError(const char *message) {
	Serial.print(F("ERR "));
	Serial.println(message);
}

bool parseNumber(const char *text, long &value) {
	if (text == nullptr || *text == '\0') {
		return false;
	}

	char *end = nullptr;
	value = strtol(text, &end, 10);
	return *end == '\0';
}

bool parsePin(const char *text, int &pin) {
	if (text == nullptr || *text == '\0') {
		return false;
	}

	if (text[0] == 'A' || text[0] == 'a') {
		long analogPin;
		if (!parseNumber(text + 1, analogPin)) {
			return false;
		}
		pin = A0 + analogPin;
	} else {
		long digitalPin;
		if (!parseNumber(text, digitalPin)) {
			return false;
		}
		pin = digitalPin;
	}

	return pin >= 0 && pin <= 255;
}

bool parseState(const char *text, int &state) {
	long number;
	if (parseNumber(text, number) && (number == LOW || number == HIGH)) {
		state = number;
		return true;
	}

	if (text != nullptr && (strcasecmp(text, "ON") == 0 || strcasecmp(text, "HIGH") == 0)) {
		state = HIGH;
		return true;
	}
	if (text != nullptr && (strcasecmp(text, "OFF") == 0 || strcasecmp(text, "LOW") == 0)) {
		state = LOW;
		return true;
	}
	return false;
}

void printHelp() {
	Serial.println(F("DATA Commands: PING, INFO, MILLIS, HELP"));
	Serial.println(F("DATA PINMODE <pin> INPUT|OUTPUT|PULLUP"));
	Serial.println(F("DATA DIGITALWRITE <pin> 0|1|ON|OFF"));
	Serial.println(F("DATA DIGITALREAD <pin>, ANALOGREAD <pin>"));
	Serial.println(F("DATA PWM <pin> 0..255, TOGGLE <pin>"));
	Serial.println(F("DATA TONE <pin> <frequency> [duration], NOTONE <pin>"));
	Serial.println(F("DATA WAIT <milliseconds>"));
	Serial.println(F("DATA SCRIPT BEGIN, SCRIPT END, SCRIPT RUN, SCRIPT CLEAR"));
}

void executeCommand(char *line);

void handleScriptCommand(char *line) {
	if (strcasecmp(line, "SCRIPT END") == 0) {
		receivingScript = false;
		scriptReady = true;
		Serial.print(F("OK SCRIPT STORED "));
		Serial.println(scriptLength);
		return;
	}

	if (scriptLength >= MAX_SCRIPT_LINES) {
		sendError(F("SCRIPT FULL"));
		receivingScript = false;
		return;
	}

	strncpy(scriptLines[scriptLength], line, SCRIPT_LINE_SIZE - 1);
	scriptLines[scriptLength][SCRIPT_LINE_SIZE - 1] = '\0';
	scriptLength++;
}

void executeCommand(char *line) {
	char *command = strtok(line, " \t");
	if (command == nullptr) {
		return;
	}

	if (strcasecmp(command, "HELP") == 0) {
		printHelp();
		return;
	}
	if (strcasecmp(command, "PING") == 0) {
		Serial.println(F("OK PONG"));
		return;
	}
	if (strcasecmp(command, "INFO") == 0) {
		Serial.println(F("OK PythonNanoRC 1.0"));
		return;
	}
	if (strcasecmp(command, "MILLIS") == 0) {
		Serial.print(F("OK MILLIS "));
		Serial.println(millis());
		return;
	}
	if (strcasecmp(command, "SCRIPT") == 0) {
		char *action = strtok(nullptr, " \t");
		if (action == nullptr) {
			sendError(F("SCRIPT ACTION REQUIRED"));
		} else if (strcasecmp(action, "BEGIN") == 0) {
			receivingScript = true;
			scriptReady = false;
			scriptLength = 0;
			Serial.println(F("OK SCRIPT BEGIN"));
		} else if (strcasecmp(action, "CLEAR") == 0) {
			receivingScript = false;
			scriptReady = false;
			scriptLength = 0;
			Serial.println(F("OK SCRIPT CLEAR"));
		} else if (strcasecmp(action, "RUN") == 0) {
			if (!scriptReady) {
				sendError(F("NO SCRIPT"));
			} else {
				for (size_t index = 0; index < scriptLength; index++) {
					char commandLine[SCRIPT_LINE_SIZE];
					strncpy(commandLine, scriptLines[index], SCRIPT_LINE_SIZE);
					executeCommand(commandLine);
				}
				Serial.println(F("OK SCRIPT DONE"));
			}
		} else {
			sendError(F("UNKNOWN SCRIPT ACTION"));
		}
		return;
	}

	char *pinText = strtok(nullptr, " \t");
	int pin;
	if ((strcasecmp(command, "PINMODE") == 0 || strcasecmp(command, "DIGITALWRITE") == 0 ||
			 strcasecmp(command, "DIGITALREAD") == 0 || strcasecmp(command, "ANALOGREAD") == 0 ||
			 strcasecmp(command, "PWM") == 0 || strcasecmp(command, "TOGGLE") == 0 ||
			 strcasecmp(command, "TONE") == 0 || strcasecmp(command, "NOTONE") == 0) &&
			!parsePin(pinText, pin)) {
		sendError(F("INVALID PIN"));
		return;
	}

	if (strcasecmp(command, "PINMODE") == 0) {
		char *mode = strtok(nullptr, " \t");
		if (mode == nullptr) {
			sendError(F("MODE REQUIRED"));
		} else if (strcasecmp(mode, "OUTPUT") == 0) {
			pinMode(pin, OUTPUT);
			Serial.println(F("OK PINMODE OUTPUT"));
		} else if (strcasecmp(mode, "INPUT") == 0) {
			pinMode(pin, INPUT);
			Serial.println(F("OK PINMODE INPUT"));
		} else if (strcasecmp(mode, "PULLUP") == 0 || strcasecmp(mode, "INPUT_PULLUP") == 0) {
			pinMode(pin, INPUT_PULLUP);
			Serial.println(F("OK PINMODE PULLUP"));
		} else {
			sendError(F("INVALID MODE"));
		}
	} else if (strcasecmp(command, "DIGITALWRITE") == 0) {
		int state;
		if (!parseState(strtok(nullptr, " \t"), state)) {
			sendError(F("INVALID STATE"));
		} else {
			digitalWrite(pin, state);
			Serial.println(F("OK DIGITALWRITE"));
		}
	} else if (strcasecmp(command, "DIGITALREAD") == 0) {
		Serial.print(F("OK DIGITALREAD "));
		Serial.println(digitalRead(pin));
	} else if (strcasecmp(command, "ANALOGREAD") == 0) {
		Serial.print(F("OK ANALOGREAD "));
		Serial.println(analogRead(pin));
	} else if (strcasecmp(command, "PWM") == 0) {
		long value;
		if (!parseNumber(strtok(nullptr, " \t"), value) || value < 0 || value > 255) {
			sendError(F("PWM MUST BE 0..255"));
		} else {
			analogWrite(pin, value);
			Serial.println(F("OK PWM"));
		}
	} else if (strcasecmp(command, "TOGGLE") == 0) {
		digitalWrite(pin, !digitalRead(pin));
		Serial.println(F("OK TOGGLE"));
	} else if (strcasecmp(command, "WAIT") == 0) {
		long duration;
		if (!parseNumber(pinText, duration) || duration < 0) {
			sendError(F("INVALID MILLISECONDS"));
		} else {
			delay(duration);
			Serial.println(F("OK WAIT"));
		}
	} else if (strcasecmp(command, "TONE") == 0) {
		long frequency;
		char *frequencyText = strtok(nullptr, " \t");
		char *durationText = strtok(nullptr, " \t");
		if (!parseNumber(frequencyText, frequency) || frequency <= 0) {
			sendError(F("INVALID FREQUENCY"));
		} else if (durationText == nullptr) {
			tone(pin, frequency);
			Serial.println(F("OK TONE"));
		} else {
			long duration;
			if (!parseNumber(durationText, duration) || duration < 0) {
				sendError(F("INVALID DURATION"));
			} else {
				tone(pin, frequency, duration);
				Serial.println(F("OK TONE"));
			}
		}
	} else if (strcasecmp(command, "NOTONE") == 0) {
		noTone(pin);
		Serial.println(F("OK NOTONE"));
	} else {
		sendError(F("UNKNOWN COMMAND"));
	}
}

void processLine(char *line) {
	if (receivingScript) {
		handleScriptCommand(line);
	} else {
		executeCommand(line);
	}
}

void setup() {
	Serial.begin(SERIAL_BAUD);
	Serial.setTimeout(20);
	Serial.println(F("OK READY PythonNanoRC 1.0"));
}

void loop() {
	while (Serial.available() > 0) {
		char character = static_cast<char>(Serial.read());
		if (character == '\r') {
			continue;
		}
		if (character == '\n') {
			inputBuffer[inputLength] = '\0';
			processLine(inputBuffer);
			inputLength = 0;
		} else if (inputLength < INPUT_BUFFER_SIZE - 1) {
			inputBuffer[inputLength++] = character;
		} else {
			inputLength = 0;
			sendError(F("COMMAND TOO LONG"));
		}
	}
}

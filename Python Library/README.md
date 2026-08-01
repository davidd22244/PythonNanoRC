# PythonNanoRC

Python client library for the Arduino sketch in `Arduino Code/Code.ino`.
The Arduino communicates over USB serial at **115200 baud**.

## Install

From this directory, install the package with:

```powershell
pip install .
```

The dependency can also be installed directly:

```powershell
pip install pyserial
```

Upload `Arduino Code/Code.ino` to the board first. Replace `COM3` in the
examples with the port shown by Arduino IDE under **Tools > Port**.

## Basic Python Usage

The `NanoRC` context manager opens the serial connection and closes it when
the block ends:

```python
from pythonnanorc import NanoRC

with NanoRC("COM3") as board:
    print(board.ping())
    board.pin_mode(13, "OUTPUT")
    board.digital_write(13, True)
    print(board.digital_read(13))
    board.digital_write(13, False)
```

## All Serial Commands

Commands are sent as one line ending in `\n`. Command names are not
case-sensitive. The Arduino responds with `OK`, `ERR`, or `DATA`.

### PING

Checks that the Arduino is responding.

```text
PING
```

Response:

```text
OK PONG
```

Python:

```python
print(board.ping())  # PONG
```

### INFO

Returns the Arduino firmware name and version.

```text
INFO
```

Response:

```text
OK PythonNanoRC 1.0
```

Python:

```python
print(board.info())
```

### MILLIS

Returns the number of milliseconds since the Arduino started.

```text
MILLIS
```

Python:

```python
elapsed = board.millis()
print(elapsed)
```

### HELP

Returns a list of supported command descriptions from the Arduino.

```text
HELP
```

Python:

```python
for line in board.help():
    print(line)
```

### PINMODE

Sets a pin as an input, output, or input with its internal pull-up resistor.

```text
PINMODE <pin> INPUT
PINMODE <pin> OUTPUT
PINMODE <pin> PULLUP
```

`INPUT_PULLUP` can also be used instead of `PULLUP`. Pins can be written as
digital numbers such as `13` or analog names such as `A0`.

Python:

```python
board.pin_mode(13, "OUTPUT")
board.pin_mode("A0", "INPUT")
board.pin_mode(2, "PULLUP")
```

### DIGITALWRITE

Sets a digital pin HIGH or LOW. The state can be `0`, `1`, `ON`, `OFF`,
`HIGH`, or `LOW`.

```text
DIGITALWRITE <pin> <state>
```

Python:

```python
board.digital_write(13, True)
board.digital_write(13, False)
board.digital_write(13, "ON")
```

### DIGITALREAD

Reads a digital pin. The result is normally `0` or `1`.

```text
DIGITALREAD <pin>
```

Python:

```python
state = board.digital_read(13)
print(state)
```

### ANALOGREAD

Reads an analog pin. On many Arduino boards, the result is in the range
`0..1023`, but the exact range depends on the board.

```text
ANALOGREAD A0
```

Python:

```python
value = board.analog_read("A0")
print(value)
```

### PWM

Writes a PWM value between `0` and `255`. The selected pin must support PWM
on the specific Arduino board.

```text
PWM <pin> <value>
```

Python:

```python
board.pwm(9, 128)  # approximately 50% duty cycle
```

### TOGGLE

Changes a digital pin from LOW to HIGH or HIGH to LOW.

```text
TOGGLE <pin>
```

Python:

```python
board.toggle(13)
```

### TONE

Generates a tone on a pin. If `duration` is omitted, the tone continues until
`NOTONE` is sent.

```text
TONE <pin> <frequency>
TONE <pin> <frequency> <duration>
```

Frequency is in hertz and duration is in milliseconds.

Python:

```python
board.tone(8, 440, 500)
board.tone(8, 880)
```

### NOTONE

Stops a tone on a pin.

```text
NOTONE <pin>
```

Python:

```python
board.no_tone(8)
```

### WAIT

Pauses the Arduino for the specified number of milliseconds. This is a
blocking delay.

```text
WAIT <milliseconds>
```

Python:

```python
board.wait(500)
```

## Scripts

Scripts store several commands on the Arduino and can be run repeatedly.
The Arduino can store up to **32 lines**, with each line limited to **95
characters**.

### SCRIPT BEGIN

Starts a new script and clears the previously stored script.

```text
SCRIPT BEGIN
```

### SCRIPT END

Stops receiving script lines and stores the script.

```text
SCRIPT END
```

### SCRIPT RUN

Runs the stored script.

```text
SCRIPT RUN
```

### SCRIPT CLEAR

Deletes the stored script.

```text
SCRIPT CLEAR
```

The Python library provides the simpler `run()` method:

```python
with NanoRC("COM3") as board:
    board.run([
        "PINMODE 13 OUTPUT",
        "DIGITALWRITE 13 ON",
        "WAIT 500",
        "DIGITALWRITE 13 OFF",
    ])
```

For separate upload and execution, use:

```python
with NanoRC("COM3") as board:
    board.upload_script([
        "PINMODE 13 OUTPUT",
        "DIGITALWRITE 13 ON",
    ])
    responses = board.run_script()
```

## Raw Commands and Responses

Use `send()` when a new command has been added to the Arduino sketch but does
not yet have a convenience method in the Python library:

```python
with NanoRC("COM3") as board:
    response = board.send("PING")
    print(response.status)   # OK
    print(response.message)  # PONG
```

Arduino `ERR` responses raise `NanoRCProtocolError`. For example, an invalid
PWM value raises an exception instead of silently failing.

## Connection Options

The default baud rate is `115200` and the default response timeout is two
seconds. Both can be changed:

```python
board = NanoRC("COM3", baudrate=115200, timeout=5.0)
try:
    board.open()
    print(board.info())
finally:
    board.close()
```

If the connection fails, check that the Arduino is plugged in, the port name
is correct, and Arduino Serial Monitor is closed. Only one application can
usually use the serial port at a time.

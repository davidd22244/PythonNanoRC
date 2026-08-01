# PythonNanoRC

Python client library for the Arduino sketch in `Arduino Code/Code.ino`.

## Install

```text
pip install pyserial
```

From this directory, the package can be imported directly. For example:

```python
from pythonnanorc import NanoRC

with NanoRC("COM3") as board:
    board.pin_mode(13, "OUTPUT")
    board.digital_write(13, True)
    print(board.digital_read(13))
    board.digital_write(13, False)
```

## Scripts

```python
with NanoRC("COM3") as board:
    board.run([
        "PINMODE 13 OUTPUT",
        "DIGITALWRITE 13 ON",
        "WAIT 500",
        "DIGITALWRITE 13 OFF",
    ])
```

The library also exposes `board.send("PING")` for commands added to the Arduino sketch later. Install `pyserial` and replace `COM3` with the port shown by Arduino IDE or Device Manager.

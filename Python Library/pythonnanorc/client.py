"""Serial client for the PythonNanoRC Arduino command protocol."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional, Union

try:
    import serial
    from serial import Serial
    from serial.serialutil import SerialException
except ImportError:  # pragma: no cover - reported when a connection is attempted
    serial = None
    Serial = object

    class SerialException(Exception):
        """Fallback exception used when pyserial is not installed."""


Pin = Union[int, str]


class NanoRCError(Exception):
    """Base exception for PythonNanoRC errors."""


class NanoRCProtocolError(NanoRCError):
    """Raised when the Arduino returns an ERR response."""


@dataclass(frozen=True)
class ArduinoResponse:
    """One line returned by the Arduino."""

    status: str
    message: str

    @property
    def fields(self) -> list[str]:
        """Return the response message split into whitespace-separated fields."""
        return self.message.split()

    def integer(self) -> int:
        """Return the last response field as an integer."""
        try:
            return int(self.fields[-1])
        except (IndexError, ValueError) as error:
            raise NanoRCError(f"Response has no integer value: {self.message!r}") from error


class NanoRC:
    """Control an Arduino running the PythonNanoRC ``Arduino Code.ino`` sketch.

    The class can open its own pyserial connection, or receive an already-open
    serial-compatible object for testing and advanced integrations.
    """

    def __init__(
        self,
        port: Optional[str] = None,
        baudrate: int = 115200,
        timeout: float = 2.0,
        connection=None,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._connection = connection
        self._owns_connection = connection is None

    @property
    def is_open(self) -> bool:
        """Whether the underlying serial connection is open."""
        return bool(self._connection is not None and self._connection.is_open)

    def open(self) -> "NanoRC":
        """Open the configured serial port and consume the Arduino startup line."""
        if self._connection is not None and self.is_open:
            return self
        if serial is None:
            raise NanoRCError("pyserial is required; install it with: pip install pyserial")
        if not self.port:
            raise NanoRCError("A serial port is required, for example 'COM3'")

        try:
            self._connection = Serial(self.port, self.baudrate, timeout=self.timeout)
            self._owns_connection = True
            startup = self._read_response()
            if startup.status == "ERR":
                raise NanoRCProtocolError(startup.message)
        except SerialException as error:
            raise NanoRCError(f"Could not open {self.port}: {error}") from error
        return self

    def close(self) -> None:
        """Close the serial connection owned by this client."""
        if self._connection is not None and self._owns_connection:
            self._connection.close()
        self._connection = None

    def __enter__(self) -> "NanoRC":
        return self.open()

    def __exit__(self, exception_type, exception, traceback) -> None:
        self.close()

    def send(self, command: str, *, allow_data: bool = True) -> ArduinoResponse:
        """Send a raw command and return its response.

        Commands must not contain a newline because one line is one protocol
        command. Arduino ``ERR`` responses raise ``NanoRCProtocolError``.
        """
        if not command or "\n" in command or "\r" in command:
            raise ValueError("command must be a non-empty single line")
        self._require_connection()
        try:
            self._connection.write((command.strip() + "\n").encode("ascii"))
            response = self._read_response()
        except (UnicodeEncodeError, SerialException) as error:
            raise NanoRCError(f"Serial communication failed: {error}") from error
        if response.status == "ERR":
            raise NanoRCProtocolError(response.message)
        if not allow_data and response.status == "DATA":
            raise NanoRCError(f"Unexpected DATA response: {response.message}")
        return response

    def ping(self) -> str:
        return self.send("PING").message

    def info(self) -> str:
        return self.send("INFO").message

    def millis(self) -> int:
        return self.send("MILLIS").integer()

    def help(self) -> list[str]:
        """Return the Arduino help lines without raising on DATA responses."""
        self._require_connection()
        self._connection.write(b"HELP\n")
        lines: list[str] = []
        while True:
            response = self._read_response()
            if response.status != "DATA":
                if response.status == "ERR":
                    raise NanoRCProtocolError(response.message)
                break
            lines.append(response.message)
            if len(lines) == 8:
                break
        return lines

    def pin_mode(self, pin: Pin, mode: str) -> ArduinoResponse:
        mode = mode.upper()
        if mode not in {"INPUT", "OUTPUT", "PULLUP", "INPUT_PULLUP"}:
            raise ValueError("mode must be INPUT, OUTPUT, PULLUP, or INPUT_PULLUP")
        return self.send(f"PINMODE {self._pin(pin)} {mode}")

    def digital_write(self, pin: Pin, state: Union[bool, int, str]) -> ArduinoResponse:
        if isinstance(state, bool):
            value = "1" if state else "0"
        else:
            value = str(state).upper()
        if value not in {"0", "1", "ON", "OFF", "HIGH", "LOW"}:
            raise ValueError("state must be bool, 0, 1, ON, OFF, HIGH, or LOW")
        return self.send(f"DIGITALWRITE {self._pin(pin)} {value}")

    def digital_read(self, pin: Pin) -> int:
        return self.send(f"DIGITALREAD {self._pin(pin)}").integer()

    def analog_read(self, pin: Pin) -> int:
        return self.send(f"ANALOGREAD {self._pin(pin)}").integer()

    def pwm(self, pin: Pin, value: int) -> ArduinoResponse:
        if not 0 <= value <= 255:
            raise ValueError("PWM value must be between 0 and 255")
        return self.send(f"PWM {self._pin(pin)} {value}")

    def toggle(self, pin: Pin) -> ArduinoResponse:
        return self.send(f"TOGGLE {self._pin(pin)}")

    def tone(self, pin: Pin, frequency: int, duration: Optional[int] = None) -> ArduinoResponse:
        if frequency <= 0:
            raise ValueError("frequency must be positive")
        command = f"TONE {self._pin(pin)} {frequency}"
        if duration is not None:
            if duration < 0:
                raise ValueError("duration must not be negative")
            command += f" {duration}"
        return self.send(command)

    def no_tone(self, pin: Pin) -> ArduinoResponse:
        return self.send(f"NOTONE {self._pin(pin)}")

    def wait(self, milliseconds: int) -> ArduinoResponse:
        if milliseconds < 0:
            raise ValueError("milliseconds must not be negative")
        return self.send(f"WAIT {milliseconds}")

    def upload_script(self, commands: Iterable[str]) -> ArduinoResponse:
        """Upload commands and leave the script stored on the Arduino."""
        self.send("SCRIPT BEGIN")
        for command in commands:
            if not command.strip():
                continue
            if "\n" in command or "\r" in command:
                raise ValueError("script commands must be single lines")
            self._require_connection()
            self._connection.write((command.strip() + "\n").encode("ascii"))
        return self.send("SCRIPT END")

    def run_script(self) -> list[ArduinoResponse]:
        """Run the stored script and return its command responses."""
        self._require_connection()
        self._connection.write(b"SCRIPT RUN\n")
        responses: list[ArduinoResponse] = []
        while True:
            response = self._read_response()
            if response.status == "ERR":
                raise NanoRCProtocolError(response.message)
            responses.append(response)
            if response.message == "SCRIPT DONE":
                return responses

    def run(self, commands: Iterable[str]) -> list[ArduinoResponse]:
        """Upload and immediately run a script."""
        self.upload_script(commands)
        return self.run_script()

    def _require_connection(self) -> None:
        if not self.is_open:
            raise NanoRCError("Serial connection is not open; call open() first")

    def _read_response(self) -> ArduinoResponse:
        raw = self._connection.readline()
        if not raw:
            raise NanoRCError("Timed out waiting for Arduino response")
        line = raw.decode("ascii", errors="replace").strip()
        status, _, message = line.partition(" ")
        if status not in {"OK", "ERR", "DATA"}:
            raise NanoRCError(f"Invalid Arduino response: {line!r}")
        return ArduinoResponse(status, message)

    @staticmethod
    def _pin(pin: Pin) -> str:
        if isinstance(pin, int) and pin >= 0:
            return str(pin)
        if isinstance(pin, str) and pin.strip():
            return pin.strip().upper()
        raise ValueError("pin must be a non-negative integer or a name such as A0")

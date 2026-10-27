"""Parser and deterministic artifact generator for M485E Device Panels.

The runtime code intentionally uses only the Python standard library.  The GUI
and CLI both call the functions in this module so validation cannot diverge.
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import os
import re
import shutil
import struct
import tempfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence


GENERATOR_VERSION = "0.6.0"
MAX_ROWS_PER_FRAGMENT = 15
DISPLAY_CHUNK_SIZE = 15
MANIFEST_NAME = "generation-manifest.json"

DEVICE_HEADER = ("Index", "Port", "Slave_ID", "Device_Name", "CSV_DB_Name")
REGISTER_HEADER = (
    "Address",
    "SignalName",
    "Type",
    "Length",
    "Byte_Order",
    "Start_Bit",
    "Size",
    "Scale",
    "Offset",
    "Unit",
    "Init_Value",
    "Value_Table",
    "Min",
    "Max",
)
VALID_TYPES = {"Signed", "Unsigned", "IEEE Float", "IEEE Double"}
VALID_BYTE_ORDERS = {"BigEndian", "LittleEndian"}
MAX_FILE_BYTES = 4 * 1024 * 1024
MAX_ROW_BYTES = 4096
MAX_DEVICES = 256
MAX_SIGNALS = 4096

PANEL_VERSION = "9.0.137.0"
PANEL_SERIALIZER = (
    "Vector.CANalyzer.Panels.PanelSerializer, Vector.CANalyzer.Panels.Serializer, "
    f"Version={PANEL_VERSION}, Culture=neutral, PublicKeyToken=null"
)
PANEL_RUNTIME = (
    "Vector.CANalyzer.Panels.Runtime.Panel, Vector.CANalyzer.Panels.Common, "
    f"Version={PANEL_VERSION}, Culture=neutral, PublicKeyToken=null"
)
COMMON_CONTROLS = "Vector.CANalyzer.Panels.CommonControls"


class GeneratorError(ValueError):
    """An actionable input or generation error."""

    def __init__(
        self,
        message: str,
        *,
        path: Path | None = None,
        row: int | None = None,
        column: str | None = None,
    ) -> None:
        self.path = path
        self.row = row
        self.column = column
        details: list[str] = []
        if path is not None:
            details.append(f"file={path}")
        if row is not None:
            details.append(f"row={row}")
        if column is not None:
            details.append(f"column={column}")
        suffix = f" ({', '.join(details)})" if details else ""
        super().__init__(message + suffix)


@dataclass(frozen=True)
class Signal:
    mapping_index: int
    csv_row: int
    address: int
    name: str
    signal_type: str
    length: int
    byte_order: str
    start_bit: int
    size: int
    scale: float
    offset: float
    unit: str
    init_value: float
    value_table: str
    minimum: float | None
    maximum: float | None

    @property
    def word_count(self) -> int:
        return self.length // 2

    @property
    def frame_key(self) -> tuple[int, int, str]:
        return (self.address, self.length, self.byte_order)

    @property
    def raw_symbol(self) -> str:
        return f"S{self.mapping_index:03d}"


@dataclass(frozen=True)
class Device:
    csv_row: int
    index: int
    port: str
    slave_id: int
    name: str
    register_map_name: str
    register_map_path: Path
    register_map_sha256: str
    signals: tuple[Signal, ...]

    @property
    def namespace(self) -> str:
        return f"Index{self.index}"

    @property
    def capl_suffix(self) -> str:
        return f"D{self.index:03d}"


@dataclass(frozen=True)
class Fragment:
    device_index: int
    group_start: int
    group_end: int
    page: int
    signals: tuple[Signal, ...]

    @property
    def file_name(self) -> str:
        base = f"A{self.group_start}_A{self.group_end}"
        if self.page > 0:
            base += f"_P{self.page:02d}"
        return base + ".xvp"


@dataclass(frozen=True)
class Analysis:
    device_map_path: Path
    device_map_sha256: str
    devices: tuple[Device, ...]
    fragments: tuple[Fragment, ...] = field(default_factory=tuple)

    @property
    def signal_count(self) -> int:
        return sum(len(device.signals) for device in self.devices)

    def preview(self) -> list[dict[str, object]]:
        return [
            {
                "index": device.index,
                "deviceName": device.name,
                "csvDbName": device.register_map_name,
                "signalCount": len(device.signals),
                "xvpCount": sum(
                    1 for fragment in self.fragments if fragment.device_index == device.index
                ),
            }
            for device in self.devices
        ]


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def _read_csv(path: Path, expected_header: Sequence[str]) -> list[tuple[int, list[str]]]:
    try:
        size = path.stat().st_size
    except FileNotFoundError as exc:
        raise GeneratorError("CSV file does not exist", path=path) from exc
    if size > MAX_FILE_BYTES:
        raise GeneratorError(
            f"CSV exceeds the {MAX_FILE_BYTES}-byte file limit", path=path
        )
    raw = path.read_bytes()
    if b"\x00" in raw:
        raise GeneratorError("CSV contains a NUL byte", path=path)
    if b"\r" in raw.replace(b"\r\n", b""):
        raise GeneratorError("CSV contains a lone CR line ending", path=path)
    for row_number, raw_line in enumerate(raw.splitlines(), start=1):
        if len(raw_line) > MAX_ROW_BYTES:
            raise GeneratorError(
                f"CSV row exceeds the {MAX_ROW_BYTES}-byte row limit",
                path=path,
                row=row_number,
            )
    payload = raw[3:] if raw.startswith(b"\xef\xbb\xbf") else raw
    if b"\xef\xbb\xbf" in payload:
        raise GeneratorError(
            "CSV contains a UTF-8 BOM outside byte zero", path=path
        )
    try:
        text = payload.decode("utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        raise GeneratorError(
            f"CSV is not valid UTF-8 at byte {exc.start}", path=path
        ) from exc
    physical_lines = text.replace("\r\n", "\n").split("\n")
    while physical_lines and physical_lines[-1] == "":
        physical_lines.pop()
    parsed: list[list[str]] = []
    try:
        for physical_line in physical_lines:
            row = next(csv.reader([physical_line], strict=True))
            parsed.append(row)
    except csv.Error as exc:
        raise GeneratorError(
            f"CSV parsing failed: {exc}", path=path, row=len(parsed) + 1
        ) from exc
    if not parsed:
        raise GeneratorError("CSV is empty", path=path)
    header = tuple(parsed[0])
    if header != tuple(expected_header):
        raise GeneratorError(
            "CSV header name or order does not match the required contract: "
            + ",".join(expected_header),
            path=path,
            row=1,
        )
    rows: list[tuple[int, list[str]]] = []
    for row_number, row in enumerate(parsed[1:], start=2):
        if len(row) != len(expected_header):
            raise GeneratorError(
                f"Expected {len(expected_header)} columns, found {len(row)}",
                path=path,
                row=row_number,
            )
        rows.append((row_number, [cell.strip(" \t") for cell in row]))
    return rows


def _parse_int(
    value: str,
    *,
    path: Path,
    row: int,
    column: str,
    minimum: int,
    maximum: int,
) -> int:
    try:
        base = 16 if value.startswith(("0x", "0X")) else 10
        digits = value[2:] if base == 16 else value
        if re.fullmatch(r"[0-9A-Fa-f]+" if base == 16 else r"[0-9]+", digits) is None:
            raise ValueError
        parsed = int(digits, base)
    except ValueError as exc:
        raise GeneratorError(
            "Expected an unsigned decimal or 0x-prefixed hexadecimal integer",
            path=path,
            row=row,
            column=column,
        ) from exc
    if parsed < minimum or parsed > maximum:
        raise GeneratorError(
            f"Value must be in {minimum}..{maximum}", path=path, row=row, column=column
        )
    return parsed


def _parse_float(
    value: str,
    *,
    path: Path,
    row: int,
    column: str,
    required: bool = True,
) -> float | None:
    if not value and not required:
        return None
    if re.fullmatch(
        r"[+-]?(?:(?:[0-9]+(?:\.[0-9]*)?)|(?:\.[0-9]+))(?:[eE][+-]?[0-9]+)?",
        value,
    ) is None:
        raise GeneratorError("Expected a finite number", path=path, row=row, column=column)
    try:
        parsed = float(value)
    except ValueError as exc:
        raise GeneratorError("Expected a finite number", path=path, row=row, column=column) from exc
    if not math.isfinite(parsed):
        raise GeneratorError("Expected a finite number", path=path, row=row, column=column)
    return parsed


def _resolve_register_map(base: Path, value: str, *, path: Path, row: int) -> Path:
    if not value:
        raise GeneratorError("CSV_DB_Name is required", path=path, row=row, column="CSV_DB_Name")
    candidate = Path(value)
    if candidate.is_absolute() or candidate.drive:
        raise GeneratorError(
            "Absolute CSV_DB_Name paths are not allowed",
            path=path,
            row=row,
            column="CSV_DB_Name",
        )
    base_resolved = base.resolve()
    resolved = (base / candidate).resolve()
    try:
        resolved.relative_to(base_resolved)
    except ValueError as exc:
        raise GeneratorError(
            "CSV_DB_Name escapes the Device CSV directory",
            path=path,
            row=row,
            column="CSV_DB_Name",
        ) from exc
    if not resolved.is_file():
        raise GeneratorError(
            "Referenced Register Map does not exist",
            path=path,
            row=row,
            column="CSV_DB_Name",
        )
    return resolved


def _validate_init_encoding(
    signal_type: str,
    size: int,
    scale: float,
    offset: float,
    init_value: float,
    *,
    path: Path,
    row: int,
) -> None:
    """Match SignalCodec::Encode without changing the physical input value."""
    message = ""
    if signal_type == "IEEE Float":
        try:
            encoded = struct.unpack(">f", struct.pack(">f", init_value))[0]
        except OverflowError:
            encoded = math.inf
        if not math.isfinite(encoded):
            message = "Init_Value cannot be encoded as a finite IEEE Float"
    elif signal_type != "IEEE Double":
        candidate = (init_value - offset) / scale
        if not math.isfinite(candidate):
            message = "Init_Value produces a non-finite Raw candidate"
        else:
            # std::round uses halfway-away-from-zero. Splitting the fraction also
            # avoids adding 0.5 to a large, already integral binary64 value.
            fraction, rounded = math.modf(candidate)
            if abs(fraction) >= 0.5:
                rounded += math.copysign(1.0, candidate)
            if abs(rounded) > 9007199254740992:
                message = "Init_Value Raw exceeds the inclusive +/-2^53 precision range"
            else:
                minimum = 0 if signal_type == "Unsigned" else -(1 << (size - 1))
                maximum = (1 << (size if signal_type == "Unsigned" else size - 1)) - 1
                if rounded < minimum or rounded > maximum:
                    message = f"Init_Value Raw is outside the {signal_type} Size={size} range"
    if message:
        raise GeneratorError(message, path=path, row=row, column="Init_Value")


def _parse_register_map(path: Path) -> tuple[Signal, ...]:
    rows = _read_csv(path, REGISTER_HEADER)
    if len(rows) > MAX_SIGNALS:
        raise GeneratorError(
            f"Register Map exceeds the {MAX_SIGNALS}-signal limit", path=path
        )
    signals: list[Signal] = []
    names: set[str] = set()
    frame_ranges: dict[tuple[int, int], tuple[str, list[tuple[int, int, str, int]]]] = {}
    for mapping_index, (row_number, row) in enumerate(rows):
        values = dict(zip(REGISTER_HEADER, row, strict=True))
        for required in (
            "Address",
            "SignalName",
            "Type",
            "Length",
            "Byte_Order",
            "Start_Bit",
            "Size",
            "Scale",
            "Init_Value",
        ):
            if not values[required]:
                raise GeneratorError(
                    "Required value is empty", path=path, row=row_number, column=required
                )
        name = values["SignalName"]
        if len(name.encode("utf-8")) > 127:
            raise GeneratorError(
                "SignalName exceeds the 127-byte UTF-8 runtime limit",
                path=path,
                row=row_number,
                column="SignalName",
            )
        if len(values["Unit"].encode("utf-8")) > 31:
            raise GeneratorError(
                "Unit exceeds the 31-byte UTF-8 runtime limit",
                path=path,
                row=row_number,
                column="Unit",
            )
        if len(values["Value_Table"].encode("utf-8")) > 1023:
            raise GeneratorError(
                "Value_Table exceeds the 1023-byte UTF-8 runtime limit",
                path=path,
                row=row_number,
                column="Value_Table",
            )
        if name in names:
            raise GeneratorError(
                "Duplicate SignalName", path=path, row=row_number, column="SignalName"
            )
        names.add(name)
        address = _parse_int(
            values["Address"], path=path, row=row_number, column="Address", minimum=0, maximum=65535
        )
        length = _parse_int(
            values["Length"], path=path, row=row_number, column="Length", minimum=2, maximum=8
        )
        if length not in (2, 4, 6, 8):
            raise GeneratorError(
                "Length must be one of 2, 4, 6, or 8 bytes",
                path=path,
                row=row_number,
                column="Length",
            )
        if address + (length // 2) - 1 > 65535:
            raise GeneratorError(
                "Signal frame exceeds address 65535", path=path, row=row_number, column="Address"
            )
        signal_type = values["Type"]
        if signal_type not in VALID_TYPES:
            raise GeneratorError(
                "Type must be Signed, Unsigned, IEEE Float, or IEEE Double",
                path=path,
                row=row_number,
                column="Type",
            )
        byte_order = values["Byte_Order"]
        if byte_order not in VALID_BYTE_ORDERS:
            raise GeneratorError(
                "Byte_Order must be BigEndian or LittleEndian",
                path=path,
                row=row_number,
                column="Byte_Order",
            )
        start_bit = _parse_int(
            values["Start_Bit"], path=path, row=row_number, column="Start_Bit", minimum=0, maximum=63
        )
        size = _parse_int(
            values["Size"], path=path, row=row_number, column="Size", minimum=1, maximum=64
        )
        if start_bit + size > length * 8:
            raise GeneratorError(
                "Start_Bit + Size exceeds the signal frame",
                path=path,
                row=row_number,
                column="Size",
            )
        scale = _parse_float(values["Scale"], path=path, row=row_number, column="Scale")
        offset = _parse_float(
            values["Offset"] or "0", path=path, row=row_number, column="Offset"
        )
        init_value = _parse_float(
            values["Init_Value"], path=path, row=row_number, column="Init_Value"
        )
        minimum = _parse_float(
            values["Min"], path=path, row=row_number, column="Min", required=False
        )
        maximum = _parse_float(
            values["Max"], path=path, row=row_number, column="Max", required=False
        )
        assert scale is not None and offset is not None and init_value is not None
        if signal_type in ("IEEE Float", "IEEE Double"):
            expected_size = 32 if signal_type == "IEEE Float" else 64
            if size != expected_size or scale != 1 or offset != 0:
                raise GeneratorError(
                    f"{signal_type} requires Size={expected_size}, Scale=1, Offset=0",
                    path=path,
                    row=row_number,
                    column="Type",
                )
        elif scale == 0:
            raise GeneratorError(
                "Signed/Unsigned Scale must not be zero",
                path=path,
                row=row_number,
                column="Scale",
            )
        if minimum is not None and maximum is not None and minimum > maximum:
            raise GeneratorError(
                "Min must be less than or equal to Max", path=path, row=row_number, column="Min"
            )
        if minimum is not None and init_value < minimum:
            raise GeneratorError(
                "Init_Value is below Min", path=path, row=row_number, column="Init_Value"
            )
        if maximum is not None and init_value > maximum:
            raise GeneratorError(
                "Init_Value is above Max", path=path, row=row_number, column="Init_Value"
            )
        _validate_init_encoding(
            signal_type, size, scale, offset, init_value, path=path, row=row_number
        )

        range_key = (address, length)
        bit_range = (start_bit, start_bit + size - 1, name, row_number)
        if range_key in frame_ranges:
            prior_order, prior_bits = frame_ranges[range_key]
            if prior_order != byte_order:
                raise GeneratorError(
                    "Signals sharing a frame must use the same Byte_Order",
                    path=path,
                    row=row_number,
                    column="Byte_Order",
                )
            for other_start, other_end, other_name, _ in prior_bits:
                if start_bit <= other_end and other_start <= start_bit + size - 1:
                    raise GeneratorError(
                        f"Signal bit range overlaps {other_name}",
                        path=path,
                        row=row_number,
                        column="Start_Bit",
                    )
            prior_bits.append(bit_range)
        else:
            new_start = address
            new_end = address + length // 2 - 1
            for (other_address, other_length), (_, _) in frame_ranges.items():
                other_end = other_address + other_length // 2 - 1
                if new_start <= other_end and other_address <= new_end:
                    raise GeneratorError(
                        "Distinct signal frames overlap Register addresses",
                        path=path,
                        row=row_number,
                        column="Address",
                    )
            frame_ranges[range_key] = (byte_order, [bit_range])

        signals.append(
            Signal(
                mapping_index=mapping_index,
                csv_row=row_number,
                address=address,
                name=name,
                signal_type=signal_type,
                length=length,
                byte_order=byte_order,
                start_bit=start_bit,
                size=size,
                scale=scale,
                offset=offset,
                unit=values["Unit"],
                init_value=init_value,
                value_table=values["Value_Table"],
                minimum=minimum,
                maximum=maximum,
            )
        )
    if not signals:
        raise GeneratorError("Register Map contains no Signal rows", path=path)
    return tuple(signals)


def _build_fragments(devices: Sequence[Device]) -> tuple[Fragment, ...]:
    fragments: list[Fragment] = []
    for device in devices:
        minimum_address = min(signal.address for signal in device.signals)
        groups: dict[tuple[int, int], list[Signal]] = {}
        for signal in device.signals:
            group_start = minimum_address + ((signal.address - minimum_address) // 31) * 31
            key = (group_start, group_start + 30)
            groups.setdefault(key, []).append(signal)
        for (group_start, group_end) in sorted(groups):
            group_signals = groups[(group_start, group_end)]
            pages = math.ceil(len(group_signals) / MAX_ROWS_PER_FRAGMENT)
            for page_index in range(pages):
                start = page_index * MAX_ROWS_PER_FRAGMENT
                page_signals = tuple(group_signals[start : start + MAX_ROWS_PER_FRAGMENT])
                fragments.append(
                    Fragment(
                        device_index=device.index,
                        group_start=group_start,
                        group_end=group_end,
                        page=page_index + 1 if pages > 1 else 0,
                        signals=page_signals,
                    )
                )
    return tuple(fragments)


def analyze_device_map(device_map_path: os.PathLike[str] | str) -> Analysis:
    path = Path(device_map_path).resolve()
    rows = _read_csv(path, DEVICE_HEADER)
    if len(rows) > MAX_DEVICES:
        raise GeneratorError(f"Device CSV exceeds the {MAX_DEVICES}-device limit", path=path)
    devices: list[Device] = []
    indexes: set[int] = set()
    endpoints: set[tuple[str, int]] = set()
    ports: list[str] = []
    for row_number, row in rows:
        values = dict(zip(DEVICE_HEADER, row, strict=True))
        for required in ("Index", "Port", "Slave_ID", "CSV_DB_Name"):
            if not values[required]:
                raise GeneratorError(
                    "Required value is empty", path=path, row=row_number, column=required
                )
        index = _parse_int(
            values["Index"], path=path, row=row_number, column="Index", minimum=0, maximum=255
        )
        slave_id = _parse_int(
            values["Slave_ID"], path=path, row=row_number, column="Slave_ID", minimum=1, maximum=247
        )
        for column, maximum in (
            ("Port", 31),
            ("Device_Name", 127),
            ("CSV_DB_Name", 255),
        ):
            if len(values[column].encode("utf-8")) > maximum:
                raise GeneratorError(
                    f"Value exceeds the {maximum}-byte runtime limit",
                    path=path,
                    row=row_number,
                    column=column,
                )
        if index in indexes:
            raise GeneratorError("Duplicate Device Index", path=path, row=row_number, column="Index")
        indexes.add(index)
        endpoint = (values["Port"], slave_id)
        if endpoint in endpoints:
            raise GeneratorError(
                "Duplicate Port and Slave_ID combination",
                path=path,
                row=row_number,
                column="Slave_ID",
            )
        endpoints.add(endpoint)
        if values["Port"] not in ports:
            ports.append(values["Port"])
            if len(ports) > 4:
                raise GeneratorError(
                    "Device CSV contains more than four unique Ports",
                    path=path,
                    row=row_number,
                    column="Port",
                )
        register_map = _resolve_register_map(path.parent, values["CSV_DB_Name"], path=path, row=row_number)
        devices.append(
            Device(
                csv_row=row_number,
                index=index,
                port=values["Port"],
                slave_id=slave_id,
                name=values["Device_Name"],
                register_map_name=values["CSV_DB_Name"],
                register_map_path=register_map,
                register_map_sha256=_sha256(register_map),
                signals=_parse_register_map(register_map),
            )
        )
    if not devices:
        raise GeneratorError("Device CSV contains no Device rows", path=path)
    if sum(len(device.signals) for device in devices) > MAX_SIGNALS:
        raise GeneratorError(
            f"Instantiated Signal count exceeds the {MAX_SIGNALS}-signal runtime limit",
            path=path,
        )
    devices.sort(key=lambda item: item.index)
    fragments = _build_fragments(devices)
    return Analysis(path, _sha256(path), tuple(devices), fragments)


def extract_raw_hex(
    register_words: Sequence[int], byte_order: str, start_bit: int, size: int
) -> str:
    """Reference implementation of the generated CAPL bit/nibble algorithm."""

    if byte_order not in VALID_BYTE_ORDERS:
        raise ValueError("byte_order must be BigEndian or LittleEndian")
    if not register_words or len(register_words) > 4:
        raise ValueError("register_words must contain 1..4 words")
    if start_bit < 0 or size < 1 or size > 64 or start_bit + size > len(register_words) * 16:
        raise ValueError("start_bit and size exceed the logical frame")
    words: list[int] = []
    for word in register_words:
        if word < 0 or word > 0xFFFF:
            raise ValueError("register word is outside 0..65535")
        logical = word
        if byte_order == "LittleEndian":
            logical = ((word & 0x00FF) << 8) | ((word & 0xFF00) >> 8)
        words.append(logical)
    raw = 0
    for bit in range(size):
        frame_bit = start_bit + bit
        word_offset = len(words) - 1 - (frame_bit // 16)
        bit_offset = frame_bit % 16
        raw |= ((words[word_offset] >> bit_offset) & 1) << bit
    digits = (size + 3) // 4
    return f"0x{raw:0{digits}X}"


def _format_number(value: float) -> str:
    if value == 0:
        return "0"
    return format(value, ".15g")


class _PanelBuilder:
    """Small deterministic serializer for the validated Vector 9.x XVP subset."""

    def __init__(self, panel_name: str, width: int, height: int) -> None:
        self.root = ET.Element("Panel", {"Type": PANEL_SERIALIZER})
        self.panel = ET.SubElement(
            self.root,
            "Object",
            {
                "Type": PANEL_RUNTIME,
                "Name": "Panel",
                "Children": "Controls",
                "ControlName": panel_name,
            },
        )
        self._counter = 0
        self._tab_index = 0
        self._property(self.panel, "Name", "Panel")
        self._property(self.panel, "Size", f"{width}, {height}")

    @staticmethod
    def _property(parent: ET.Element, name: str, value: object) -> None:
        node = ET.SubElement(parent, "Property", {"Name": name})
        node.text = str(value)

    def _object(
        self,
        parent: ET.Element,
        kind: str,
        prefix: str,
        control_name: str,
        *,
        children: bool = True,
    ) -> ET.Element:
        self._counter += 1
        object_name = f"{prefix}_{self._counter:04d}"
        unique_control_name = f"{control_name}_{self._counter:04d}"
        attrs = {
            "Type": (
                f"Vector.CANalyzer.Panels.Design.{kind}, {COMMON_CONTROLS}, "
                f"Version={PANEL_VERSION}, Culture=neutral, PublicKeyToken=null"
            ),
            "Name": object_name,
            "ControlName": unique_control_name,
        }
        if children:
            attrs["Children"] = "Controls"
        node = ET.SubElement(parent, "Object", attrs)
        self._property(node, "Name", object_name)
        return node

    def _next_tab(self) -> int:
        self._tab_index += 1
        return self._tab_index

    def static_text(
        self,
        parent: ET.Element,
        text: str,
        x: int,
        y: int,
        width: int,
        height: int = 18,
        *,
        bold: bool = False,
    ) -> None:
        node = self._object(parent, "StaticTextControl", "StaticText", "Static Text")
        self._property(node, "Size", f"{width}, {height}")
        self._property(node, "Location", f"{x}, {y}")
        if bold:
            self._property(node, "Font", "Microsoft Sans Serif, 8.25pt, style=Bold")
        self._property(node, "Text", text)

    def textbox(
        self,
        parent: ET.Element,
        *,
        control_name: str,
        binding: str,
        x: int,
        y: int,
        width: int,
        display: str,
        read_only: bool,
    ) -> None:
        node = self._object(parent, "TextBoxControl", "TextBox", control_name)
        self._property(node, "Size", f"{width}, 21")
        self._property(node, "Location", f"{x}, {y}")
        self._property(node, "AlarmUpperTextColor", "WindowText")
        self._property(node, "AlarmLowerBkgColor", "Salmon")
        self._property(node, "ValueDisplay", display)
        self._property(node, "AlarmUpperBkgColor", "IndianRed")
        self._property(node, "AlarmLowerTextColor", "WindowText")
        self._property(node, "AlarmGeneralSettings", "1;0;0;0")
        self._property(node, "DisplayLabel", "Hide")
        self._property(node, "DescriptionSize", "14, 23")
        self._property(node, "SymbolConfiguration", binding)
        self._property(node, "TabIndex", self._next_tab())
        if read_only:
            self._property(node, "ReadOnlyControl", "True")

    def switch(
        self,
        parent: ET.Element,
        *,
        control_name: str,
        binding: str,
        x: int,
        y: int,
    ) -> None:
        node = self._object(
            parent, "SwitchControl", "Switch", control_name, children=False
        )
        self._property(node, "Size", "28, 30")
        self._property(node, "Location", f"{x}, {y}")
        self._property(node, "StateCnt", "2")
        self._property(node, "SwitchValues", "1;2;0;1")
        self._property(node, "SymbolConfiguration", binding)
        self._property(node, "TabIndex", self._next_tab())

    def group(
        self,
        *,
        title: str,
        x: int,
        y: int,
        width: int,
        height: int,
        control_name: str,
    ) -> ET.Element:
        node = self._object(
            self.panel, "GroupBoxControl", "GroupBox", control_name
        )
        self._property(node, "Size", f"{width}, {height}")
        self._property(node, "Location", f"{x}, {y}")
        self._property(node, "Text", title)
        self._property(node, "TabIndex", self._next_tab())
        return node

    def bytes(self) -> bytes:
        ET.indent(self.root, space="  ")
        body = ET.tostring(self.root, encoding="unicode", short_empty_elements=True)
        return ("<?xml version=\"1.0\"?>\r\n" + body + "\r\n").replace(
            "\n", "\r\n"
        ).replace("\r\r\n", "\r\n").encode("utf-8")


def _symbol(namespace: str, variable: str, symbol_type: int, array_index: int = -1) -> str:
    parts = namespace.split("::")
    while len(parts) < 3:
        parts.append("")
    return (
        f"4;16;{parts[0]};{parts[1]};{parts[2]};{variable};"
        f"{symbol_type};;;{array_index}"
    )


def _write_device_fragment(
    path: Path,
    device: Device,
    fragment: Fragment,
    *,
    include_operations: bool,
) -> None:
    operation_height = 126 if include_operations else 0
    height = 88 + len(fragment.signals) * 31 + operation_height
    builder = _PanelBuilder(
        f"M485E_DevicePanel_Index{device.index}_{fragment.file_name[:-4]}",
        930,
        height + 28,
    )
    title = f"Address A{fragment.group_start} ~ A{fragment.group_end}"
    group = builder.group(
        title=title,
        x=8,
        y=8,
        width=900,
        height=height,
        control_name=f"AddressGroup_Index{device.index}_{fragment.group_start}_{fragment.page:02d}",
    )
    builder.static_text(group, "Signal Name", 18, 26, 260, bold=True)
    builder.static_text(group, "Raw Hex", 300, 26, 180, bold=True)
    builder.static_text(group, "Physical Value (Dec)", 500, 26, 175, bold=True)
    builder.static_text(group, "Init Value", 705, 26, 155, bold=True)
    for row_index, signal in enumerate(fragment.signals):
        y = 52 + row_index * 31
        builder.static_text(group, signal.name, 18, y + 4, 260)
        builder.textbox(
            group,
            control_name=f"RawHex_Index{device.index}_{signal.raw_symbol}",
            binding=_symbol(
                f"M485E_DevicePanel::{device.namespace}::RawHex",
                signal.raw_symbol,
                4,
            ),
            x=300,
            y=y,
            width=180,
            display="String",
            read_only=True,
        )
        builder.textbox(
            group,
            control_name=f"PhysicalValue_Index{device.index}_{signal.mapping_index:03d}",
            binding=_symbol(
                f"M485E_DevicePanel::{device.namespace}",
                "PhysicalValue",
                2,
                signal.mapping_index,
            ),
            x=500,
            y=y,
            width=175,
            display="Double",
            read_only=False,
        )
        init_text = _format_number(signal.init_value)
        if signal.unit:
            init_text += f" {signal.unit}"
        builder.static_text(group, init_text, 705, y + 4, 155)

    if include_operations:
        base_y = 63 + len(fragment.signals) * 31
        controls = (
            ("Display Update", "DisplayUpdate"),
            ("Send Changed", "SendChanged"),
            ("Reset Default", "ResetDefault"),
        )
        for position, (label, variable) in enumerate(controls):
            x = 18 + position * 180
            builder.static_text(group, label, x, base_y + 9, 125)
            builder.switch(
                group,
                control_name=f"{variable}_Index{device.index}",
                binding=_symbol(
                    f"M485E_DevicePanel::{device.namespace}", variable, 1
                ),
                x=x + 130,
                y=base_y,
            )
        result_fields = (
            ("Result", "ResultCode", 1),
            ("Written", "WrittenCount", 1),
            ("Failed Signal", "FailedSignalIndex", 1),
        )
        for position, (label, variable, symbol_type) in enumerate(result_fields):
            x = 18 + position * 235
            builder.static_text(group, label, x, base_y + 48, 90)
            builder.textbox(
                group,
                control_name=f"{variable}_Index{device.index}",
                binding=_symbol(
                    f"M485E_DevicePanel::{device.namespace}", variable, symbol_type
                ),
                x=x + 92,
                y=base_y + 43,
                width=120,
                display="Decimal",
                read_only=True,
            )
        builder.static_text(group, "Snapshot", 720, base_y + 48, 72)
        builder.textbox(
            group,
            control_name=f"SnapshotValid_Index{device.index}",
            binding=_symbol(
                f"M485E_DevicePanel::{device.namespace}", "SnapshotValid", 1
            ),
            x=792,
            y=base_y + 43,
            width=70,
            display="Decimal",
            read_only=True,
        )
        builder.static_text(group, "Failed Signal Name", 18, base_y + 80, 125)
        builder.textbox(
            group,
            control_name=f"FailedSignalName_Index{device.index}",
            binding=_symbol(
                f"M485E_DevicePanel::{device.namespace}", "FailedSignalName", 4
            ),
            x=145,
            y=base_y + 74,
            width=715,
            display="String",
            read_only=True,
        )
    path.write_bytes(builder.bytes())


def _write_common_fragment(path: Path) -> None:
    builder = _PanelBuilder("M485E_DevicePanel_Common_Reset", 680, 170)
    group = builder.group(
        title="All Device Default Reset (non-atomic)",
        x=8,
        y=8,
        width=640,
        height=120,
        control_name="Common_ResetAllDefault_Group",
    )
    builder.static_text(group, "Reset All Default", 18, 32, 125)
    builder.switch(
        group,
        control_name="ResetAllDefault",
        binding=_symbol("M485E_DevicePanel::Common", "ResetAllDefault", 1),
        x=150,
        y=22,
    )
    fields = (
        ("Result", "ResultCode"),
        ("Completed", "CompletedDeviceCount"),
        ("Failed Index", "FailedDeviceIndex"),
    )
    for position, (label, variable) in enumerate(fields):
        x = 18 + position * 205
        builder.static_text(group, label, x, 79, 85)
        builder.textbox(
            group,
            control_name=f"Common_{variable}",
            binding=_symbol("M485E_DevicePanel::Common", variable, 1),
            x=x + 90,
            y=73,
            width=95,
            display="Decimal",
            read_only=True,
        )
    path.write_bytes(builder.bytes())


def _variable(
    parent: ET.Element,
    *,
    name: str,
    var_type: str,
    bitcount: int,
    comment: str,
    array_length: int | None = None,
) -> None:
    attrs = {
        "anlyzLocal": "2",
        "readOnly": "false",
        "valueSequence": "false",
        "unit": "",
        "name": name,
        "comment": comment,
        "bitcount": str(bitcount),
        "isSigned": "true" if var_type != "string" else "false",
        "encoding": "65001",
        "type": var_type,
    }
    if array_length is not None:
        attrs["arrayLength"] = str(array_length)
    ET.SubElement(parent, "variable", attrs)


def _write_system_variables(path: Path, devices: Sequence[Device]) -> None:
    root = ET.Element("systemvariables", {"version": "4"})
    empty = ET.SubElement(root, "namespace", {"name": "", "comment": ""})
    panel = ET.SubElement(
        empty,
        "namespace",
        {
            "name": "M485E_DevicePanel",
            "comment": "Generated M485E Device Signal Panel variables",
        },
    )
    common = ET.SubElement(
        panel, "namespace", {"name": "Common", "comment": "All-device reset state"}
    )
    _variable(
        common,
        name="ResetAllDefault",
        var_type="int",
        bitcount=32,
        comment="Momentary non-atomic reset command for all generated Device indexes",
    )
    _variable(common, name="ResultCode", var_type="int", bitcount=32, comment="Final all-device reset result")
    _variable(
        common,
        name="CompletedDeviceCount",
        var_type="int",
        bitcount=32,
        comment="Number of Devices fully reset and read back before completion",
    )
    _variable(
        common,
        name="FailedDeviceIndex",
        var_type="int",
        bitcount=32,
        comment="Actual failed Device Index, or -1 when no failure occurred",
    )
    for device in devices:
        namespace = ET.SubElement(
            panel,
            "namespace",
            {
                "name": device.namespace,
                "comment": f"Generated Device Index {device.index}: {device.name}",
            },
        )
        _variable(
            namespace,
            name="PhysicalValue",
            var_type="floatarray",
            bitcount=64,
            array_length=len(device.signals),
            comment="Editable physical values in Register Map signal-row order",
        )
        raw = ET.SubElement(
            namespace,
            "namespace",
            {"name": "RawHex", "comment": "Exact unsigned signal Raw Bit Pattern"},
        )
        for signal in device.signals:
            _variable(
                raw,
                name=signal.raw_symbol,
                var_type="string",
                bitcount=0,
                comment=f"{signal.name} raw bits; generated mapping index {signal.mapping_index}",
            )
        for name, comment in (
            ("DisplayUpdate", "Momentary command to read all Device signals in bounded chunks"),
            ("SendChanged", "Momentary command to write values changed from the valid baseline"),
            ("ResetDefault", "Momentary command to restore CSV Init values and read back"),
            ("SnapshotValid", "1 only after a complete successful Display Update/read-back"),
            ("ResultCode", "Result of the most recent Device Panel operation"),
            ("WrittenCount", "Signals successfully written by the most recent Send Changed"),
            ("FailedSignalIndex", "CSV signal-row mapping index, or -1"),
        ):
            _variable(namespace, name=name, var_type="int", bitcount=32, comment=comment)
        _variable(
            namespace,
            name="FailedSignalName",
            var_type="string",
            bitcount=0,
            comment="Exact failed CSV SignalName, or an empty string",
        )
    ET.indent(root, space="  ")
    text = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    path.write_bytes(("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n" + text + "\r\n").replace("\n", "\r\n").replace("\r\r\n", "\r\n").encode("utf-8"))


def _capl_bytes(lines: Iterable[str]) -> bytes:
    text = "\r\n".join(lines) + "\r\n"
    try:
        return text.encode("cp949", errors="strict")
    except UnicodeEncodeError as exc:
        raise GeneratorError(
            f"Generated CAPL contains text that cannot be encoded as CP949: {exc}"
        ) from exc


def _capl_float(value: float) -> str:
    text = format(value, ".17g")
    if "e" not in text.lower() and "." not in text:
        text += ".0"
    return text


def _signal_name_assignments(signal: Signal, target: str, indent: str) -> list[str]:
    encoded = signal.name.encode("utf-8")
    if len(encoded) > 127:
        raise GeneratorError(
            "SignalName exceeds the 127-byte UTF-8 runtime limit",
            path=None,
            row=signal.csv_row,
            column="SignalName",
        )
    lines = [f"{indent}{target}[0] = 0;"]
    for index, value in enumerate(encoded):
        lines.append(f"{indent}{target}[{index}] = {value};")
    lines.append(f"{indent}{target}[{len(encoded)}] = 0;")
    return lines


def _write_mapping_capl(path: Path, devices: Sequence[Device]) -> None:
    lines: list[str] = ["/*@!Encoding:949*/", "", "includes", "{"]
    for device in devices:
        lines.append(f'     #include "M485E_DevicePanel_{device.capl_suffix}.cin"')
    lines.extend(
        [
            "}",
            "",
            "// Generated file. Do not edit; regenerate from M485E_Device.csv.",
            "variables",
            "{",
            "     const long M485E_DEVICE_PANEL_CHUNK_SIZE = 15;",
            f"     const long M485E_DEVICE_PANEL_DEVICE_COUNT = {len(devices)};",
            "     const long M485E_DEVICE_PANEL_OPERATION_NONE = 0;",
            "     const long M485E_DEVICE_PANEL_OPERATION_DISPLAY = 1;",
            "     const long M485E_DEVICE_PANEL_OPERATION_SEND = 2;",
            "     const long M485E_DEVICE_PANEL_OPERATION_RESET = 3;",
            "     const long M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW = 0;",
            "     const long M485E_DEVICE_PANEL_DISPLAY_STAGE_READ = 1;",
            "     char gM485E_DevicePanelHexDigits[17] = \"0123456789ABCDEF\";",
            "     long gM485E_DevicePanelLastRefreshGeneration = -1;",
            "     long gM485E_DevicePanelCommonActive;",
            "     long gM485E_DevicePanelCommonCursor;",
            "     long gM485E_DevicePanelCommonCompleted;",
            "     msTimer tM485E_DevicePanelCommon;",
            "}",
            "",
            "void M485E_DevicePanel_FormatRawHex(word frameWords[], long wordCount,",
            "     long byteOrder, long startBit, long size, char outText[])",
            "{",
            "     long digitCount;",
            "     long nibbleIndex;",
            "     long bitInNibble;",
            "     long signalBit;",
            "     long frameBit;",
            "     long wordOffset;",
            "     long bitOffset;",
            "     long bitValue;",
            "     long nibbleValue;",
            "     long outputIndex;",
            "     word logicalWord;",
            "",
            "     digitCount = (size + 3) / 4;",
            "     outText[0] = '0';",
            "     outText[1] = 'x';",
            "     for (nibbleIndex = 0; nibbleIndex < digitCount; nibbleIndex++)",
            "     {",
            "          nibbleValue = 0;",
            "          for (bitInNibble = 0; bitInNibble < 4; bitInNibble++)",
            "          {",
            "               signalBit = nibbleIndex * 4 + bitInNibble;",
            "               if (signalBit < size)",
            "               {",
            "                    frameBit = startBit + signalBit;",
            "                    wordOffset = wordCount - 1 - (frameBit / 16);",
            "                    bitOffset = frameBit % 16;",
            "                    logicalWord = frameWords[wordOffset];",
            "                    if (byteOrder != 0)",
            "                         logicalWord = ((logicalWord & 0x00FF) << 8) |",
            "                              ((logicalWord & 0xFF00) >> 8);",
            "                    bitValue = (logicalWord >> bitOffset) & 1;",
            "                    nibbleValue = nibbleValue | (bitValue << bitInNibble);",
            "               }",
            "          }",
            "          outputIndex = 2 + digitCount - 1 - nibbleIndex;",
            "          outText[outputIndex] = gM485E_DevicePanelHexDigits[nibbleValue];",
            "     }",
            "     outText[2 + digitCount] = 0;",
            "}",
            "",
            "long M485E_DevicePanel_CommonIsActive()",
            "{",
            "     return gM485E_DevicePanelCommonActive;",
            "}",
            "",
            "long M485E_DevicePanel_AnyDeviceBusy()",
            "{",
        ]
    )
    if devices:
        condition = " ||\r\n          ".join(
            f"M485E_DevicePanel_{device.capl_suffix}_IsBusy() != 0" for device in devices
        )
        lines.append(f"     return {condition};")
    else:
        lines.append("     return 0;")
    lines.extend(
        [
            "}",
            "",
            "void M485E_DevicePanel_CommonFinish(long result, long failedIndex)",
            "{",
            "     gM485E_DevicePanelCommonActive = 0;",
            "     cancelTimer(tM485E_DevicePanelCommon);",
            "     @sysvar::M485E_DevicePanel::Common::ResultCode = result;",
            "     @sysvar::M485E_DevicePanel::Common::CompletedDeviceCount =",
            "          gM485E_DevicePanelCommonCompleted;",
            "     @sysvar::M485E_DevicePanel::Common::FailedDeviceIndex = failedIndex;",
            "}",
            "",
            "void M485E_DevicePanel_CommonDeviceCompleted(long deviceIndex, long result)",
            "{",
            "     if (gM485E_DevicePanelCommonActive == 0) return;",
            "     if (result != M485E_OK)",
            "     {",
            "          M485E_DevicePanel_CommonFinish(result, deviceIndex);",
            "          return;",
            "     }",
            "     gM485E_DevicePanelCommonCompleted++;",
            "     gM485E_DevicePanelCommonCursor++;",
            "     @sysvar::M485E_DevicePanel::Common::CompletedDeviceCount =",
            "          gM485E_DevicePanelCommonCompleted;",
            "     setTimer(tM485E_DevicePanelCommon, 1);",
            "}",
            "",
            "long M485E_DevicePanel_CommonBegin()",
            "{",
            "     if (gM485E_DevicePanelCommonActive != 0 ||",
            "          M485E_DevicePanel_AnyDeviceBusy() != 0)",
            "          return M485E_ERR_OPERATION_BUSY;",
            "     gM485E_DevicePanelCommonActive = 1;",
            "     gM485E_DevicePanelCommonCursor = 0;",
            "     gM485E_DevicePanelCommonCompleted = 0;",
            "     @sysvar::M485E_DevicePanel::Common::ResultCode = M485E_OK;",
            "     @sysvar::M485E_DevicePanel::Common::CompletedDeviceCount = 0;",
            "     @sysvar::M485E_DevicePanel::Common::FailedDeviceIndex = -1;",
            "     setTimer(tM485E_DevicePanelCommon, 1);",
            "     return M485E_OK;",
            "}",
            "",
            "void M485E_DevicePanel_InvalidateAll()",
            "{",
        ]
    )
    for device in devices:
        lines.append(f"     M485E_DevicePanel_{device.capl_suffix}_Invalidate();")
    lines.extend(
        [
            "}",
            "",
            "void M485E_DevicePanel_Poll(long lifecycleStatus[])",
            "{",
            "     long refreshGeneration;",
            "     refreshGeneration = lifecycleStatus[M485E_LS_LAST_COMPLETED_GENERATION];",
            "     if (lifecycleStatus[M485E_LS_LAST_COMPLETED_OPERATION] ==",
            "               M485E_OPERATION_REFRESH_CSV &&",
            "          lifecycleStatus[M485E_LS_LAST_RESULT] == M485E_OK &&",
            "          refreshGeneration != gM485E_DevicePanelLastRefreshGeneration)",
            "     {",
            "          M485E_DevicePanel_InvalidateAll();",
            "          gM485E_DevicePanelLastRefreshGeneration = refreshGeneration;",
            "     }",
            "}",
            "",
            "void M485E_DevicePanel_Start()",
            "{",
            "     gM485E_DevicePanelLastRefreshGeneration = -1;",
            "     gM485E_DevicePanelCommonActive = 0;",
            "     gM485E_DevicePanelCommonCursor = 0;",
            "     gM485E_DevicePanelCommonCompleted = 0;",
            "     cancelTimer(tM485E_DevicePanelCommon);",
            "     @sysvar::M485E_DevicePanel::Common::ResetAllDefault = 0;",
            "     @sysvar::M485E_DevicePanel::Common::ResultCode = M485E_OK;",
            "     @sysvar::M485E_DevicePanel::Common::CompletedDeviceCount = 0;",
            "     @sysvar::M485E_DevicePanel::Common::FailedDeviceIndex = -1;",
        ]
    )
    for device in devices:
        lines.append(f"     M485E_DevicePanel_{device.capl_suffix}_Start();")
    lines.extend(
        [
            "}",
            "",
            "void M485E_DevicePanel_Stop()",
            "{",
            "     gM485E_DevicePanelCommonActive = 0;",
            "     cancelTimer(tM485E_DevicePanelCommon);",
            "     @sysvar::M485E_DevicePanel::Common::ResetAllDefault = 0;",
        ]
    )
    for device in devices:
        lines.append(f"     M485E_DevicePanel_{device.capl_suffix}_Stop();")
    lines.extend(
        [
            "}",
            "",
            "on sysvar_update sysvar::M485E_DevicePanel::Common::ResetAllDefault",
            "{",
            "     long result;",
            "     if (@this == 0) return;",
            "     result = M485E_DevicePanel_CommonBegin();",
            "     if (result != M485E_OK)",
            "          M485E_DevicePanel_CommonFinish(result, -1);",
            "     @this = 0;",
            "}",
            "",
            "on timer tM485E_DevicePanelCommon",
            "{",
            "     long result;",
            "     if (gM485E_DevicePanelCommonActive == 0) return;",
            "     if (gM485E_DevicePanelCommonCursor >= M485E_DEVICE_PANEL_DEVICE_COUNT)",
            "     {",
            "          M485E_DevicePanel_CommonFinish(M485E_OK, -1);",
            "          return;",
            "     }",
            "     result = M485E_ERR_INVALID_ARGUMENT;",
            "     switch (gM485E_DevicePanelCommonCursor)",
            "     {",
        ]
    )
    for cursor, device in enumerate(devices):
        lines.extend(
            [
                f"          case {cursor}:",
                f"               result = M485E_DevicePanel_{device.capl_suffix}_BeginReset(1);",
                "               break;",
            ]
        )
    lines.extend(
        [
            "          default:",
            "               break;",
            "     }",
            "     if (result != M485E_OK)",
            "     {",
            "          switch (gM485E_DevicePanelCommonCursor)",
            "          {",
        ]
    )
    for cursor, device in enumerate(devices):
        lines.extend(
            [
                f"               case {cursor}:",
                f"                    M485E_DevicePanel_CommonFinish(result, {device.index});",
                "                    break;",
            ]
        )
    lines.extend(
        [
            "               default:",
            "                    M485E_DevicePanel_CommonFinish(result, -1);",
            "                    break;",
            "          }",
            "     }",
            "}",
        ]
    )
    path.write_bytes(_capl_bytes(lines))


def _write_device_capl(path: Path, device: Device) -> None:
    suffix = device.capl_suffix
    prefix = f"M485E_DevicePanel_{suffix}"
    namespace = f"M485E_DevicePanel::Index{device.index}"
    frames: list[tuple[int, int, str]] = []
    frame_indexes: dict[tuple[int, int, str], int] = {}
    for signal in device.signals:
        if signal.frame_key not in frame_indexes:
            frame_indexes[signal.frame_key] = len(frames)
            frames.append(signal.frame_key)

    lines: list[str] = [
        "/*@!Encoding:949*/",
        "",
        "// Generated file. Do not edit; regenerate from M485E_Device.csv.",
        "variables",
        "{",
        f"     const long {prefix.upper()}_INDEX = {device.index};",
        f"     const long {prefix.upper()}_SIGNAL_COUNT = {len(device.signals)};",
        f"     const long {prefix.upper()}_FRAME_COUNT = {len(frames)};",
        f"     struct {prefix}_FrameCache",
        "     {",
        "          word words[4];",
        "          long valid;",
        "     };",
        f"     struct {prefix}_FrameCache g{prefix[1:]}Frames[{len(frames)}];",
        f"     float g{prefix[1:]}Baseline[{len(device.signals)}];",
        f"     float g{prefix[1:]}Working[{len(device.signals)}];",
        f"     float g{prefix[1:]}PanelValues[{len(device.signals)}];",
        f"     long g{prefix[1:]}Meta[6];",
        f"     char g{prefix[1:]}SignalName[128];",
        f"     char g{prefix[1:]}RawText[20];",
        f"     long g{prefix[1:]}Operation;",
        f"     long g{prefix[1:]}Cursor;",
        f"     long g{prefix[1:]}DisplayStage;",
        f"     long g{prefix[1:]}SnapshotValid;",
        f"     long g{prefix[1:]}WrittenCount;",
        f"     long g{prefix[1:]}CommonRequest;",
        f"     msTimer t{prefix[1:]};",
        "}",
        "",
        f"long {prefix}_IsBusy()",
        "{",
        f"     return g{prefix[1:]}Operation != M485E_DEVICE_PANEL_OPERATION_NONE;",
        "}",
        "",
        f"void {prefix}_GetSignalName(long signalIndex, char outName[])",
        "{",
        "     outName[0] = 0;",
        "     switch (signalIndex)",
        "     {",
    ]
    for signal in device.signals:
        lines.append(f"          case {signal.mapping_index}:")
        lines.extend(_signal_name_assignments(signal, "outName", "               "))
        lines.extend(["               break;"])
    lines.extend(["          default:", "               break;", "     }", "}", ""])

    lines.extend(
        [
            f"long {prefix}_GetSignalMeta(long signalIndex, long outMeta[])",
            "{",
            "     switch (signalIndex)",
            "     {",
        ]
    )
    for signal in device.signals:
        byte_order = 0 if signal.byte_order == "BigEndian" else 1
        lines.extend(
            [
                f"          case {signal.mapping_index}:",
                f"               outMeta[0] = {frame_indexes[signal.frame_key]};",
                f"               outMeta[1] = {signal.address};",
                f"               outMeta[2] = {signal.word_count};",
                f"               outMeta[3] = {byte_order};",
                f"               outMeta[4] = {signal.start_bit};",
                f"               outMeta[5] = {signal.size};",
                "               return M485E_OK;",
            ]
        )
    lines.extend(
        [
            "          default:",
            "               return M485E_ERR_INVALID_ARGUMENT;",
            "     }",
            "}",
            "",
            f"void {prefix}_SetRawHex(long signalIndex, char text[])",
            "{",
            "     switch (signalIndex)",
            "     {",
        ]
    )
    for signal in device.signals:
        lines.extend(
            [
                f"          case {signal.mapping_index}:",
                f"               sysSetVariableString(sysvar::{namespace}::RawHex::{signal.raw_symbol}, text);",
                "               break;",
            ]
        )
    lines.extend(["          default:", "               break;", "     }", "}", ""])

    lines.extend(
        [
            f"void {prefix}_SetRawUnavailable(long signalIndex)",
            "{",
            f"     strncpy(g{prefix[1:]}RawText, \"N/A\", elcount(g{prefix[1:]}RawText));",
            f"     {prefix}_SetRawHex(signalIndex, g{prefix[1:]}RawText);",
            "}",
            "",
            f"void {prefix}_ClearFailure()",
            "{",
            f"     g{prefix[1:]}SignalName[0] = 0;",
            f"     @sysvar::{namespace}::FailedSignalIndex = -1;",
            f"     sysSetVariableString(sysvar::{namespace}::FailedSignalName, g{prefix[1:]}SignalName);",
            "}",
            "",
            f"void {prefix}_SetFailure(long result, long signalIndex)",
            "{",
            f"     @sysvar::{namespace}::ResultCode = result;",
            f"     @sysvar::{namespace}::FailedSignalIndex = signalIndex;",
            "     if (signalIndex >= 0)",
            f"          {prefix}_GetSignalName(signalIndex, g{prefix[1:]}SignalName);",
            "     else",
            f"          g{prefix[1:]}SignalName[0] = 0;",
            f"     sysSetVariableString(sysvar::{namespace}::FailedSignalName, g{prefix[1:]}SignalName);",
            "}",
            "",
            f"void {prefix}_ClearFrameCache()",
            "{",
            "     long frameIndex;",
            f"     for (frameIndex = 0; frameIndex < {prefix.upper()}_FRAME_COUNT; frameIndex++)",
            f"          g{prefix[1:]}Frames[frameIndex].valid = 0;",
            "}",
            "",
            f"void {prefix}_InvalidateSignalFrame(long signalIndex)",
            "{",
            f"     if ({prefix}_GetSignalMeta(signalIndex, g{prefix[1:]}Meta) == M485E_OK)",
            f"          g{prefix[1:]}Frames[g{prefix[1:]}Meta[0]].valid = 0;",
            "}",
            "",
            f"long {prefix}_ReadRaw(long signalIndex)",
            "{",
            "     long result;",
            "     long frameIndex;",
            f"     result = {prefix}_GetSignalMeta(signalIndex, g{prefix[1:]}Meta);",
            "     if (result != M485E_OK) return result;",
            f"     frameIndex = g{prefix[1:]}Meta[0];",
            f"     if (g{prefix[1:]}Frames[frameIndex].valid == 0)",
            "     {",
            f"          result = M485E_Core_ReadRegisters({device.index},",
            f"               g{prefix[1:]}Meta[1], g{prefix[1:]}Meta[2],",
            f"               g{prefix[1:]}Frames[frameIndex].words);",
            "          if (result != M485E_OK)",
            "          {",
            f"               {prefix}_SetRawUnavailable(signalIndex);",
            "               return result;",
            "          }",
            f"          g{prefix[1:]}Frames[frameIndex].valid = 1;",
            "     }",
            f"     M485E_DevicePanel_FormatRawHex(g{prefix[1:]}Frames[frameIndex].words,",
            f"          g{prefix[1:]}Meta[2], g{prefix[1:]}Meta[3],",
            f"          g{prefix[1:]}Meta[4], g{prefix[1:]}Meta[5], g{prefix[1:]}RawText);",
            f"     {prefix}_SetRawHex(signalIndex, g{prefix[1:]}RawText);",
            "     return M485E_OK;",
            "}",
            "",
            f"void {prefix}_FinishDisplay(long result, long failedSignalIndex)",
            "{",
            "     long signalIndex;",
            f"     if (result == M485E_OK && g{prefix[1:]}Cursor == {prefix.upper()}_SIGNAL_COUNT)",
            "     {",
            f"          for (signalIndex = 0; signalIndex < {prefix.upper()}_SIGNAL_COUNT; signalIndex++)",
            "          {",
            f"               g{prefix[1:]}Baseline[signalIndex] = g{prefix[1:]}Working[signalIndex];",
            f"               g{prefix[1:]}PanelValues[signalIndex] = g{prefix[1:]}Working[signalIndex];",
            "          }",
            f"          sysSetVariableFloatArray(sysvar::{namespace}::PhysicalValue,",
            f"               g{prefix[1:]}PanelValues, {prefix.upper()}_SIGNAL_COUNT);",
            f"          g{prefix[1:]}SnapshotValid = 1;",
            f"          @sysvar::{namespace}::SnapshotValid = 1;",
            f"          @sysvar::{namespace}::ResultCode = M485E_OK;",
            f"          {prefix}_ClearFailure();",
            "     }",
            "     else",
            "     {",
            f"          g{prefix[1:]}SnapshotValid = 0;",
            f"          @sysvar::{namespace}::SnapshotValid = 0;",
            f"          {prefix}_SetFailure(result, failedSignalIndex);",
            "     }",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            f"     if (g{prefix[1:]}CommonRequest != 0)",
            "     {",
            f"          g{prefix[1:]}CommonRequest = 0;",
            f"          M485E_DevicePanel_CommonDeviceCompleted({device.index}, result);",
            "     }",
            "}",
            "",
            f"void {prefix}_ProcessDisplay()",
            "{",
            "     long processed;",
            "     long result;",
            "     float physicalValue;",
            f"     if (g{prefix[1:]}DisplayStage == M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW)",
            "     {",
            f"          for (processed = 0; processed < M485E_DEVICE_PANEL_CHUNK_SIZE &&",
            f"               g{prefix[1:]}Cursor < {prefix.upper()}_SIGNAL_COUNT; processed++)",
            "          {",
            f"               {prefix}_SetRawUnavailable(g{prefix[1:]}Cursor);",
            f"               g{prefix[1:]}Cursor++;",
            "          }",
            f"          if (g{prefix[1:]}Cursor < {prefix.upper()}_SIGNAL_COUNT)",
            "          {",
            f"               setTimer(t{prefix[1:]}, 1);",
            "               return;",
            "          }",
            f"          g{prefix[1:]}DisplayStage = M485E_DEVICE_PANEL_DISPLAY_STAGE_READ;",
            f"          g{prefix[1:]}Cursor = 0;",
            f"          setTimer(t{prefix[1:]}, 1);",
            "          return;",
            "     }",
            f"     for (processed = 0; processed < M485E_DEVICE_PANEL_CHUNK_SIZE &&",
            f"          g{prefix[1:]}Cursor < {prefix.upper()}_SIGNAL_COUNT; processed++)",
            "     {",
            f"          result = {prefix}_ReadRaw(g{prefix[1:]}Cursor);",
            "          if (result != M485E_OK)",
            "          {",
            f"               {prefix}_FinishDisplay(result, g{prefix[1:]}Cursor);",
            "               return;",
            "          }",
            f"          {prefix}_GetSignalName(g{prefix[1:]}Cursor, g{prefix[1:]}SignalName);",
            "          physicalValue = 0.0;",
            f"          result = M485E_Core_ReadSignal({device.index}, g{prefix[1:]}SignalName, physicalValue);",
            "          if (result != M485E_OK)",
            "          {",
            f"               {prefix}_FinishDisplay(result, g{prefix[1:]}Cursor);",
            "               return;",
            "          }",
            f"          g{prefix[1:]}Working[g{prefix[1:]}Cursor] = physicalValue;",
            f"          g{prefix[1:]}Cursor++;",
            "     }",
            f"     if (g{prefix[1:]}Cursor >= {prefix.upper()}_SIGNAL_COUNT)",
            f"          {prefix}_FinishDisplay(M485E_OK, -1);",
            "     else",
            f"          setTimer(t{prefix[1:]}, 1);",
            "}",
            "",
            f"long {prefix}_BeginDisplay(long commonRequest)",
            "{",
            f"     if ({prefix}_IsBusy() != 0) return M485E_ERR_OPERATION_BUSY;",
            "     if (commonRequest == 0 && M485E_DevicePanel_CommonIsActive() != 0)",
            "          return M485E_ERR_OPERATION_BUSY;",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_DISPLAY;",
            f"     g{prefix[1:]}Cursor = 0;",
            f"     g{prefix[1:]}DisplayStage = M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW;",
            f"     g{prefix[1:]}CommonRequest = commonRequest;",
            f"     g{prefix[1:]}SnapshotValid = 0;",
            f"     @sysvar::{namespace}::SnapshotValid = 0;",
            f"     @sysvar::{namespace}::WrittenCount = 0;",
            f"     @sysvar::{namespace}::ResultCode = M485E_OK;",
            f"     {prefix}_ClearFailure();",
            f"     {prefix}_ClearFrameCache();",
            f"     setTimer(t{prefix[1:]}, 1);",
            "     return M485E_OK;",
            "}",
            "",
            f"void {prefix}_FinishSend(long result, long failedSignalIndex, long invalidateSnapshot)",
            "{",
            f"     sysSetVariableFloatArray(sysvar::{namespace}::PhysicalValue,",
            f"          g{prefix[1:]}PanelValues, {prefix.upper()}_SIGNAL_COUNT);",
            f"     @sysvar::{namespace}::WrittenCount = g{prefix[1:]}WrittenCount;",
            "     if (invalidateSnapshot != 0)",
            "     {",
            f"          g{prefix[1:]}SnapshotValid = 0;",
            f"          @sysvar::{namespace}::SnapshotValid = 0;",
            "     }",
            "     if (result == M485E_OK)",
            "     {",
            f"          @sysvar::{namespace}::ResultCode = M485E_OK;",
            f"          {prefix}_ClearFailure();",
            "     }",
            "     else",
            f"          {prefix}_SetFailure(result, failedSignalIndex);",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            "}",
            "",
            f"void {prefix}_ProcessSend()",
            "{",
            "     long processed;",
            "     long result;",
            "     float readBack;",
            f"     for (processed = 0; processed < M485E_DEVICE_PANEL_CHUNK_SIZE &&",
            f"          g{prefix[1:]}Cursor < {prefix.upper()}_SIGNAL_COUNT; processed++)",
            "     {",
            f"          if (g{prefix[1:]}PanelValues[g{prefix[1:]}Cursor] !=",
            f"               g{prefix[1:]}Baseline[g{prefix[1:]}Cursor])",
            "          {",
            f"               {prefix}_GetSignalName(g{prefix[1:]}Cursor, g{prefix[1:]}SignalName);",
            f"               result = M485E_Core_WriteSignal({device.index}, g{prefix[1:]}SignalName,",
            f"                    g{prefix[1:]}PanelValues[g{prefix[1:]}Cursor]);",
            "               if (result != M485E_OK)",
            "               {",
            f"                    {prefix}_FinishSend(result, g{prefix[1:]}Cursor, 0);",
            "                    return;",
            "               }",
            "               readBack = 0.0;",
            f"               result = M485E_Core_ReadSignal({device.index}, g{prefix[1:]}SignalName, readBack);",
            "               if (result != M485E_OK)",
            "               {",
            f"                    {prefix}_SetRawUnavailable(g{prefix[1:]}Cursor);",
            f"                    {prefix}_FinishSend(result, g{prefix[1:]}Cursor, 1);",
            "                    return;",
            "               }",
            f"               g{prefix[1:]}PanelValues[g{prefix[1:]}Cursor] = readBack;",
            f"               g{prefix[1:]}Baseline[g{prefix[1:]}Cursor] = readBack;",
            f"               {prefix}_InvalidateSignalFrame(g{prefix[1:]}Cursor);",
            f"               result = {prefix}_ReadRaw(g{prefix[1:]}Cursor);",
            "               if (result != M485E_OK)",
            "               {",
            f"                    {prefix}_FinishSend(result, g{prefix[1:]}Cursor, 1);",
            "                    return;",
            "               }",
            f"               g{prefix[1:]}WrittenCount++;",
            "          }",
            f"          g{prefix[1:]}Cursor++;",
            "     }",
            f"     if (g{prefix[1:]}Cursor >= {prefix.upper()}_SIGNAL_COUNT)",
            f"          {prefix}_FinishSend(M485E_OK, -1, 0);",
            "     else",
            f"          setTimer(t{prefix[1:]}, 1);",
            "}",
            "",
            f"long {prefix}_BeginSend()",
            "{",
            "     long arrayLength;",
            f"     if ({prefix}_IsBusy() != 0 || M485E_DevicePanel_CommonIsActive() != 0)",
            "          return M485E_ERR_OPERATION_BUSY;",
            f"     if (g{prefix[1:]}SnapshotValid == 0) return M485E_ERR_INVALID_STATE;",
            f"     arrayLength = SysGetVariableArrayLength(sysvar::{namespace}::PhysicalValue);",
            f"     if (arrayLength != {prefix.upper()}_SIGNAL_COUNT)",
            "          return M485E_ERR_INVALID_ARGUMENT;",
            f"     sysGetVariableFloatArray(sysvar::{namespace}::PhysicalValue,",
            f"          g{prefix[1:]}PanelValues, arrayLength);",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_SEND;",
            f"     g{prefix[1:]}Cursor = 0;",
            f"     g{prefix[1:]}WrittenCount = 0;",
            f"     @sysvar::{namespace}::WrittenCount = 0;",
            f"     @sysvar::{namespace}::ResultCode = M485E_OK;",
            f"     {prefix}_ClearFailure();",
            f"     setTimer(t{prefix[1:]}, 1);",
            "     return M485E_OK;",
            "}",
            "",
            f"long {prefix}_BeginReset(long commonRequest)",
            "{",
            "     long result;",
            f"     if ({prefix}_IsBusy() != 0) return M485E_ERR_OPERATION_BUSY;",
            "     if (commonRequest == 0 && M485E_DevicePanel_CommonIsActive() != 0)",
            "          return M485E_ERR_OPERATION_BUSY;",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_RESET;",
            f"     g{prefix[1:]}SnapshotValid = 0;",
            f"     @sysvar::{namespace}::SnapshotValid = 0;",
            f"     @sysvar::{namespace}::WrittenCount = 0;",
            f"     {prefix}_ClearFailure();",
            f"     result = M485E_Core_ResetDevice({device.index});",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            "     if (result != M485E_OK)",
            "     {",
            f"          {prefix}_SetFailure(result, -1);",
            "          return result;",
            "     }",
            f"     return {prefix}_BeginDisplay(commonRequest);",
            "}",
            "",
            f"void {prefix}_Invalidate()",
            "{",
            "     long notifyCommon;",
            f"     notifyCommon = g{prefix[1:]}CommonRequest;",
            f"     cancelTimer(t{prefix[1:]});",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            f"     g{prefix[1:]}CommonRequest = 0;",
            f"     g{prefix[1:]}DisplayStage = M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW;",
            f"     g{prefix[1:]}SnapshotValid = 0;",
            f"     @sysvar::{namespace}::SnapshotValid = 0;",
            f"     @sysvar::{namespace}::DisplayUpdate = 0;",
            f"     @sysvar::{namespace}::SendChanged = 0;",
            f"     @sysvar::{namespace}::ResetDefault = 0;",
            "     if (notifyCommon != 0)",
            f"          M485E_DevicePanel_CommonDeviceCompleted({device.index}, M485E_ERR_INVALID_STATE);",
            "}",
            "",
            f"void {prefix}_Start()",
            "{",
            "     long signalIndex;",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            f"     g{prefix[1:]}CommonRequest = 0;",
            f"     g{prefix[1:]}DisplayStage = M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW;",
            f"     g{prefix[1:]}SnapshotValid = 0;",
            f"     g{prefix[1:]}WrittenCount = 0;",
            f"     cancelTimer(t{prefix[1:]});",
            f"     @sysvar::{namespace}::DisplayUpdate = 0;",
            f"     @sysvar::{namespace}::SendChanged = 0;",
            f"     @sysvar::{namespace}::ResetDefault = 0;",
            f"     @sysvar::{namespace}::SnapshotValid = 0;",
            f"     @sysvar::{namespace}::ResultCode = M485E_OK;",
            f"     @sysvar::{namespace}::WrittenCount = 0;",
            f"     {prefix}_ClearFailure();",
            f"     for (signalIndex = 0; signalIndex < {prefix.upper()}_SIGNAL_COUNT; signalIndex++)",
            f"          {prefix}_SetRawUnavailable(signalIndex);",
            "}",
            "",
            f"void {prefix}_Stop()",
            "{",
            f"     cancelTimer(t{prefix[1:]});",
            f"     g{prefix[1:]}Operation = M485E_DEVICE_PANEL_OPERATION_NONE;",
            f"     g{prefix[1:]}CommonRequest = 0;",
            f"     g{prefix[1:]}DisplayStage = M485E_DEVICE_PANEL_DISPLAY_STAGE_CLEAR_RAW;",
            f"     g{prefix[1:]}SnapshotValid = 0;",
            f"     @sysvar::{namespace}::DisplayUpdate = 0;",
            f"     @sysvar::{namespace}::SendChanged = 0;",
            f"     @sysvar::{namespace}::ResetDefault = 0;",
            f"     @sysvar::{namespace}::SnapshotValid = 0;",
            "}",
            "",
            f"on sysvar_update sysvar::{namespace}::DisplayUpdate",
            "{",
            "     long result;",
            "     if (@this == 0) return;",
            f"     result = {prefix}_BeginDisplay(0);",
            "     if (result != M485E_OK)",
            f"          {prefix}_SetFailure(result, -1);",
            "     @this = 0;",
            "}",
            "",
            f"on sysvar_update sysvar::{namespace}::SendChanged",
            "{",
            "     long result;",
            "     if (@this == 0) return;",
            f"     result = {prefix}_BeginSend();",
            "     if (result != M485E_OK)",
            f"          {prefix}_SetFailure(result, -1);",
            "     @this = 0;",
            "}",
            "",
            f"on sysvar_update sysvar::{namespace}::ResetDefault",
            "{",
            "     long result;",
            "     if (@this == 0) return;",
            f"     result = {prefix}_BeginReset(0);",
            "     if (result != M485E_OK &&",
            f"          @sysvar::{namespace}::ResultCode == M485E_OK)",
            f"          {prefix}_SetFailure(result, -1);",
            "     @this = 0;",
            "}",
            "",
            f"on timer t{prefix[1:]}",
            "{",
            f"     if (g{prefix[1:]}Operation == M485E_DEVICE_PANEL_OPERATION_DISPLAY)",
            f"          {prefix}_ProcessDisplay();",
            f"     else if (g{prefix[1:]}Operation == M485E_DEVICE_PANEL_OPERATION_SEND)",
            f"          {prefix}_ProcessSend();",
            "}",
        ]
    )
    path.write_bytes(_capl_bytes(lines))


def _copy_guide(analysis: Analysis) -> str:
    device_rows = "\n".join(
        f"| {device.index} | {device.name or '(empty)'} | {device.register_map_name} | "
        f"{len(device.signals)} |"
        for device in analysis.devices
    )
    return f"""# M485E Device Panel Generator COPY GUIDE

Generator Version: {GENERATOR_VERSION}

Generated files are deterministic outputs. Do not edit generated XVP, CAPL, System Variable,
or manifest files directly; regenerate and replace them when the CSV schema or row order changes.

## Generated Device Summary

| Actual Device Index | Device Name | Register Map | Signal Count |
| ---: | --- | --- | ---: |
{device_rows}

## Company Configuration Application

1. Select the company `M485E_Device.csv` in `Panel_Generator.exe`.
2. Run **Analyze** and confirm Device Index, Device Name, CSV_DB_Name, Signal Count, and XVP Count.
3. Run **Generate**. Output is created beside `Panel_Generator.exe`, never under the current shell directory.
4. Add or replace `Output\\SystemVariables\\M485E_DevicePanel.generated.vsysvar` in the CANalyzer Configuration.
5. Replace the generated files under `Config\\CAPL\\M485E\\DevicePanel` with `Output\\CAPL\\M485E\\DevicePanel`.
6. Open each generated XVP in CANalyzer Panel Designer.
7. Select the generated GroupBox and controls and copy/paste them into the matching existing Device Tab.
8. Confirm every System Variable binding uses the actual `M485E_DevicePanel::IndexN` namespace.
9. Compile the Configuration in CANalyzer 9.x before Measurement.
10. Verify Display Update, edit, Send Changed, Device Reset, and non-atomic Reset All Default.
11. Regenerate whenever the Device CSV, Register Map schema, or Register Map row order changes.

`IndexN` is the actual Device Index, not the Device CSV row number. Devices that share one Register
Map still receive independent namespaces, CAPL state, baseline arrays, and XVP folders.

## Operating Contract

- Display Update is on-demand and processes at most {DISPLAY_CHUNK_SIZE} signals per timer chunk.
- Display Update overwrites unsent Panel edits and creates a new baseline only after every Signal succeeds.
- Send Changed requires `SnapshotValid=1`, compares against that baseline, and writes changed Signals in CSV order.
- Send Changed and Reset All are not all-or-nothing across multiple Signals or Devices. Earlier successful writes remain.
- Reset All processes actual Device Indexes in ascending order and stops at the first failure.
- Raw Hex is the exact unsigned Signal Raw Bit Pattern after Byte_Order normalization and Start_Bit/Size extraction.
- IEEE Float/Double physical values are supported. Signed/Unsigned integer physical values are exact only within ±2^53
  because the existing Signal API carries a double. Full 64-bit Raw display remains exact in Raw Hex.
- Raw Hex editing and full-range 64-bit integer writes are not supported in v{GENERATOR_VERSION}.
- Display Update is not a Device-wide atomic snapshot. An external Master can write between ReadRegisters and ReadSignal.
- The generated XVP is a copy/paste fragment, not a complete replacement for an existing company Panel or Tab layout.

## External Verification

The XML and CAPL source contracts are automatically validated. XVP import/copy-paste, String and GroupBox behavior,
CANalyzer 9.x/18 Measurement, real USB-485 communication, and real Board behavior remain External Verification Pending
until executed in the target environment.
"""


def _validate_xvp(path: Path) -> None:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as exc:
        raise GeneratorError(f"Generated XVP is not valid XML: {exc}", path=path) from exc
    if root.tag != "Panel" or PANEL_VERSION not in root.attrib.get("Type", ""):
        raise GeneratorError("Generated XVP has an unexpected Panel serializer", path=path)
    objects = root.findall(".//Object")
    names = [node.attrib.get("Name", "") for node in objects]
    control_names = [node.attrib.get("ControlName", "") for node in objects]
    if not any("GroupBoxControl" in node.attrib.get("Type", "") for node in objects):
        raise GeneratorError("Generated XVP does not contain a GroupBoxControl", path=path)
    if "" in names or len(names) != len(set(names)):
        raise GeneratorError("Generated XVP Object Name values are not unique", path=path)
    if "" in control_names or len(control_names) != len(set(control_names)):
        raise GeneratorError("Generated XVP Control Name values are not unique", path=path)
    tab_indexes = [
        property_node.text or ""
        for property_node in root.findall(".//Property[@Name='TabIndex']")
    ]
    if len(tab_indexes) != len(set(tab_indexes)):
        raise GeneratorError("Generated XVP TabIndex values are not unique", path=path)
    for node in objects:
        bindings = node.findall("./Property[@Name='SymbolConfiguration']")
        if "TextBoxControl" in node.attrib.get("Type", "") or "SwitchControl" in node.attrib.get("Type", ""):
            if len(bindings) != 1:
                raise GeneratorError(
                    "Every bound XVP Control must have exactly one SymbolConfiguration",
                    path=path,
                )
    xml_text = path.read_text(encoding="utf-8")
    if "ComboBox" in xml_text or "Value_Table" in xml_text:
        raise GeneratorError("Generated XVP must not contain Value Table or ComboBox controls", path=path)


def _validate_generated_capl(path: Path) -> None:
    raw = path.read_bytes()
    if b"\t" in raw:
        raise GeneratorError("Generated CAPL contains a tab", path=path)
    text = raw.decode("cp949", errors="strict")
    if "\r\n" not in text or "\n" in text.replace("\r\n", ""):
        raise GeneratorError("Generated CAPL must use CRLF", path=path)
    if "M485E_Dll_" in text:
        raise GeneratorError("Generated CAPL must not call DLL exports directly", path=path)
    allowed = {
        "M485E_Core_ReadRegisters",
        "M485E_Core_ReadSignal",
        "M485E_Core_WriteSignal",
        "M485E_Core_ResetDevice",
    }
    import re

    calls = set(re.findall(r"\b(M485E_Core_[A-Za-z0-9_]+)\s*\(", text))
    if not calls.issubset(allowed):
        raise GeneratorError(
            "Generated CAPL calls an unsupported Core wrapper: "
            + ", ".join(sorted(calls - allowed)),
            path=path,
        )


def validate_output(output_root: os.PathLike[str] | str) -> None:
    root = Path(output_root)
    manifest_path = root / MANIFEST_NAME
    if not manifest_path.is_file():
        raise GeneratorError("Generated Output manifest is missing", path=manifest_path)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise GeneratorError("Generated Output manifest is invalid", path=manifest_path) from exc
    if manifest.get("generatorVersion") != GENERATOR_VERSION:
        raise GeneratorError("Generated Output manifest version mismatch", path=manifest_path)
    for relative, expected_hash in manifest.get("outputs", {}).items():
        output_path = root / Path(relative)
        if not output_path.is_file() or _sha256(output_path) != expected_hash:
            raise GeneratorError("Generated Output hash mismatch", path=output_path)
    for xvp_path in sorted(root.rglob("*.xvp")):
        _validate_xvp(xvp_path)
    for capl_path in sorted(root.rglob("*.cin")):
        _validate_generated_capl(capl_path)
    try:
        system_root = ET.parse(
            root / "SystemVariables" / "M485E_DevicePanel.generated.vsysvar"
        ).getroot()
    except ET.ParseError as exc:
        raise GeneratorError("Generated System Variable DB is invalid XML") from exc
    if system_root.tag != "systemvariables":
        raise GeneratorError("Generated System Variable DB root is invalid")


def _manifest(analysis: Analysis, root: Path) -> dict[str, object]:
    outputs: dict[str, str] = {}
    for output_path in sorted(path for path in root.rglob("*") if path.is_file()):
        relative = output_path.relative_to(root).as_posix()
        if relative == MANIFEST_NAME:
            continue
        outputs[relative] = _sha256(output_path)
    return {
        "generatorVersion": GENERATOR_VERSION,
        "deviceMap": {
            "fileName": analysis.device_map_path.name,
            "sha256": analysis.device_map_sha256,
        },
        "devices": [
            {
                "index": device.index,
                "port": device.port,
                "slaveId": device.slave_id,
                "deviceName": device.name,
                "registerMap": device.register_map_name,
                "registerMapSha256": device.register_map_sha256,
                "signals": [
                    {
                        "mappingIndex": signal.mapping_index,
                        "symbol": signal.raw_symbol,
                        "csvRow": signal.csv_row,
                        "signalName": signal.name,
                        "address": signal.address,
                        "type": signal.signal_type,
                        "length": signal.length,
                        "byteOrder": signal.byte_order,
                        "startBit": signal.start_bit,
                        "size": signal.size,
                    }
                    for signal in device.signals
                ],
            }
            for device in analysis.devices
        ],
        "outputs": outputs,
    }


def _populate_output(analysis: Analysis, root: Path) -> None:
    capl_root = root / "CAPL" / "M485E" / "DevicePanel"
    system_root = root / "SystemVariables"
    common_root = root / "Common"
    capl_root.mkdir(parents=True)
    system_root.mkdir(parents=True)
    common_root.mkdir(parents=True)
    for device in analysis.devices:
        (root / f"Index{device.index}").mkdir(parents=True)
    first_fragment_by_device: set[int] = set()
    devices_by_index = {device.index: device for device in analysis.devices}
    for fragment in analysis.fragments:
        include_operations = fragment.device_index not in first_fragment_by_device
        first_fragment_by_device.add(fragment.device_index)
        _write_device_fragment(
            root / f"Index{fragment.device_index}" / fragment.file_name,
            devices_by_index[fragment.device_index],
            fragment,
            include_operations=include_operations,
        )
    _write_common_fragment(common_root / "ResetAllDefault.xvp")
    _write_mapping_capl(capl_root / "M485E_DevicePanel_Mapping.cin", analysis.devices)
    for device in analysis.devices:
        _write_device_capl(
            capl_root / f"M485E_DevicePanel_{device.capl_suffix}.cin", device
        )
    _write_system_variables(
        system_root / "M485E_DevicePanel.generated.vsysvar", analysis.devices
    )
    (root / "COPY_GUIDE.md").write_text(_copy_guide(analysis), encoding="utf-8", newline="\n")
    manifest = _manifest(analysis, root)
    (root / MANIFEST_NAME).write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    validate_output(root)


def generate_output(
    analysis_or_path: Analysis | os.PathLike[str] | str,
    executable_directory: os.PathLike[str] | str,
) -> Path:
    """Generate and atomically replace ``Output`` beside the executable."""

    analysis = (
        analysis_or_path
        if isinstance(analysis_or_path, Analysis)
        else analyze_device_map(analysis_or_path)
    )
    base = Path(executable_directory).resolve()
    base.mkdir(parents=True, exist_ok=True)
    output = base / "Output"
    if output.exists():
        if not output.is_dir():
            raise GeneratorError("Output exists but is not a directory", path=output)
        if not (output / MANIFEST_NAME).is_file():
            raise GeneratorError(
                "Existing Output has no Generator manifest and will not be overwritten",
                path=output,
            )

    temporary = Path(tempfile.mkdtemp(prefix=".Panel_Generator.tmp.", dir=base))
    backup = base / f".Panel_Generator.backup.{os.getpid()}"
    try:
        _populate_output(analysis, temporary)
        if output.exists():
            if backup.exists():
                raise GeneratorError("Generator backup path already exists", path=backup)
            os.replace(output, backup)
            try:
                os.replace(temporary, output)
            except BaseException:
                os.replace(backup, output)
                raise
            shutil.rmtree(backup)
        else:
            os.replace(temporary, output)
        validate_output(output)
        return output
    finally:
        if temporary.exists():
            shutil.rmtree(temporary, ignore_errors=True)

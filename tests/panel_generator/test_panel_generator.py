from __future__ import annotations

import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from panel_generator import core  # noqa: E402


DEVICE_HEADER = ",".join(core.DEVICE_HEADER)
REGISTER_HEADER = ",".join(core.REGISTER_HEADER)
FIXTURE = Path(__file__).parent / "fixtures" / "valid" / "M485E_Device.csv"


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def simple_signal(
    address: int,
    name: str,
    *,
    signal_type: str = "Unsigned",
    length: int = 2,
    byte_order: str = "BigEndian",
    start_bit: int = 0,
    size: int = 16,
    scale: str = "1",
    offset: str = "0",
    init: str = "0",
    value_table: str = "",
) -> str:
    return (
        f"{address},{name},{signal_type},{length},{byte_order},{start_bit},{size},"
        f"{scale},{offset},unit,{init},{value_table},,"
    )


def create_fixture(
    root: Path,
    signal_count: int,
    *,
    device_rows: list[str] | None = None,
    shared_map: bool = False,
) -> Path:
    map_name = "Synthetic.csv"
    signals = [simple_signal(10000 + index, f"Signal_{index:03d}") for index in range(signal_count)]
    write_text(root / map_name, REGISTER_HEADER + "\n" + "\n".join(signals) + "\n")
    if device_rows is None:
        device_rows = [f"0,COM8,1,Device 0,{map_name}"]
        if shared_map:
            device_rows.append(f"9,COM8,2,Device 9,{map_name}")
    write_text(root / "M485E_Device.csv", DEVICE_HEADER + "\n" + "\n".join(device_rows) + "\n")
    return root / "M485E_Device.csv"


def tree_hashes(root: Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(root.rglob("*"))
        if path.is_file()
    }


class CsvContractTests(unittest.TestCase):
    def test_valid_sparse_indexes_and_shared_map_are_independent(self) -> None:
        analysis = core.analyze_device_map(FIXTURE)
        self.assertEqual([device.index for device in analysis.devices], [0, 7])
        self.assertEqual([len(device.signals) for device in analysis.devices], [3, 3])
        self.assertIsNot(analysis.devices[0].signals, analysis.devices[1].signals)

    def test_exact_header_and_order_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            write_text(device, "Port,Index,Slave_ID,Device_Name,CSV_DB_Name\nCOM8,0,1,D,Synthetic.csv\n")
            with self.assertRaisesRegex(core.GeneratorError, "header name or order"):
                core.analyze_device_map(device)

    def test_register_header_and_order_are_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            write_text(root / "Synthetic.csv", "SignalName,Address\nBad,10000\n")
            with self.assertRaisesRegex(core.GeneratorError, "header name or order"):
                core.analyze_device_map(device)

    def test_missing_absolute_and_escape_register_maps_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for map_value, pattern in (
                ("missing.csv", "does not exist"),
                (str((root / "absolute.csv").resolve()), "Absolute"),
                ("../escape.csv", "escapes"),
            ):
                write_text(
                    root / "M485E_Device.csv",
                    DEVICE_HEADER + f"\n0,COM8,1,D,{map_value}\n",
                )
                with self.subTest(map_value=map_value):
                    with self.assertRaisesRegex(core.GeneratorError, pattern):
                        core.analyze_device_map(root / "M485E_Device.csv")

    def test_duplicate_device_index_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(
                root,
                1,
                device_rows=[
                    "3,COM8,1,A,Synthetic.csv",
                    "3,COM9,2,B,Synthetic.csv",
                ],
            )
            with self.assertRaisesRegex(core.GeneratorError, "Duplicate Device Index"):
                core.analyze_device_map(device)

    def test_duplicate_signal_name_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            write_text(
                root / "Synthetic.csv",
                REGISTER_HEADER
                + "\n"
                + simple_signal(10000, "Duplicate")
                + "\n"
                + simple_signal(10001, "Duplicate")
                + "\n",
            )
            with self.assertRaisesRegex(core.GeneratorError, "Duplicate SignalName"):
                core.analyze_device_map(device)

    def test_invalid_length_size_start_type_and_order_are_rejected(self) -> None:
        cases = (
            (simple_signal(10000, "Bad", length=3), "Length"),
            (simple_signal(10000, "Bad", start_bit=15, size=2), r"Start_Bit \+ Size"),
            (simple_signal(10000, "Bad", signal_type="Enum"), "Type must"),
            (simple_signal(10000, "Bad", byte_order="MiddleEndian"), "Byte_Order"),
        )
        for row, pattern in cases:
            with self.subTest(pattern=pattern), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                device = create_fixture(root, 1)
                write_text(root / "Synthetic.csv", REGISTER_HEADER + "\n" + row + "\n")
                with self.assertRaisesRegex(core.GeneratorError, pattern):
                    core.analyze_device_map(device)

    def test_signal_mapping_index_follows_csv_row_order(self) -> None:
        analysis = core.analyze_device_map(FIXTURE)
        self.assertEqual(
            [(signal.mapping_index, signal.raw_symbol) for signal in analysis.devices[0].signals],
            [(0, "S000"), (1, "S001"), (2, "S002")],
        )

    def test_core_compatible_hex_integers_are_accepted_and_displayed_decimal(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_text(
                root / "Synthetic.csv",
                REGISTER_HEADER
                + "\n"
                + simple_signal(10000, "HexSignal").replace("10000,", "0x2710,")
                + "\n",
            )
            write_text(
                root / "M485E_Device.csv",
                DEVICE_HEADER + "\n0x07,COM8,0x01,Hex Device,Synthetic.csv\n",
            )
            analysis = core.analyze_device_map(root / "M485E_Device.csv")
            self.assertEqual(analysis.devices[0].index, 7)
            self.assertEqual(analysis.devices[0].signals[0].address, 10000)
            self.assertEqual(analysis.fragments[0].file_name, "A10000_A10030.xvp")

    def test_core_incompatible_blank_row_late_bom_and_python_number_extension_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            original = (root / "Synthetic.csv").read_text(encoding="utf-8")
            write_text(root / "Synthetic.csv", original.replace("\n", "\n\n", 1))
            with self.assertRaisesRegex(core.GeneratorError, "Expected 14 columns"):
                core.analyze_device_map(device)

            (root / "Synthetic.csv").write_bytes(original.encode("utf-8") + b"\xef\xbb\xbf")
            with self.assertRaisesRegex(core.GeneratorError, "BOM outside byte zero"):
                core.analyze_device_map(device)

            write_text(
                root / "Synthetic.csv",
                REGISTER_HEADER + "\n" + simple_signal(10000, "Bad", scale="1_0") + "\n",
            )
            with self.assertRaisesRegex(core.GeneratorError, "finite number"):
                core.analyze_device_map(device)

    def test_device_string_limits_and_four_port_limit_match_runtime_loader(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(
                root,
                1,
                device_rows=[
                    f"{index},COM{index},1,D{index},Synthetic.csv" for index in range(5)
                ],
            )
            with self.assertRaisesRegex(core.GeneratorError, "more than four unique Ports"):
                core.analyze_device_map(device)

            write_text(
                device,
                DEVICE_HEADER + "\n0," + ("P" * 32) + ",1,D,Synthetic.csv\n",
            )
            with self.assertRaisesRegex(core.GeneratorError, "31-byte"):
                core.analyze_device_map(device)


class NumericValidationTests(unittest.TestCase):
    def test_integer_syntax_rejects_python_extensions_with_field_context(self) -> None:
        invalid = ("0_0", "+0", "-0", "０", "٠", "0x_0", "0x+0", "0X０", "0b0", "0x")
        for column in ("Index", "Address"):
            for value in invalid + (("100_00",) if column == "Address" else ()):
                with self.subTest(column=column, value=value), tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    device = create_fixture(root, 1)
                    target = device if column == "Index" else root / "Synthetic.csv"
                    text = target.read_text(encoding="utf-8")
                    lines = text.splitlines()
                    cells = lines[1].split(",")
                    cells[0] = value
                    write_text(target, lines[0] + "\n" + ",".join(cells) + "\n")
                    with self.assertRaises(core.GeneratorError) as caught:
                        core.analyze_device_map(device)
                    self.assertEqual(caught.exception.path, target)
                    self.assertEqual(caught.exception.row, 2)
                    self.assertEqual(caught.exception.column, column)

    def test_integer_syntax_accepts_ascii_decimal_hex_and_field_boundaries(self) -> None:
        for index, slave, address, length, start, size in (
            ("000", "001", "00000", "02", "00", "016"),
            ("0xFF", "0XF7", "0xFFFF", "0x2", "0x0", "0X10"),
            ("255", "247", "65532", "8", "63", "1"),
        ):
            with self.subTest(index=index, address=address), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                device = create_fixture(root, 1)
                write_text(device, DEVICE_HEADER + f"\n{index},COM8,{slave},D,Synthetic.csv\n")
                row = f"{address},S,Unsigned,{length},BigEndian,{start},{size},1,0,,0,,,"
                write_text(root / "Synthetic.csv", REGISTER_HEADER + "\n" + row + "\n")
                analysis = core.analyze_device_map(device)
                self.assertEqual(analysis.devices[0].index, 0 if index == "000" else 255)
                self.assertEqual(analysis.devices[0].signals[0].address, {"00000": 0, "0xFFFF": 65535, "65532": 65532}[address])

    def _check_init(self, *, accepted: bool, minimum: str = "", maximum: str = "", **parameters: object) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            cells = simple_signal(10000, "InitSignal", **parameters).split(",")
            cells[12:14] = [minimum, maximum]
            register_map = root / "Synthetic.csv"
            write_text(register_map, REGISTER_HEADER + "\n" + ",".join(cells) + "\n")
            if accepted:
                signal = core.analyze_device_map(device).devices[0].signals[0]
                self.assertEqual(signal.init_value, float(parameters["init"]))
            else:
                with self.assertRaises(core.GeneratorError) as caught:
                    core.analyze_device_map(device)
                self.assertEqual(caught.exception.path, register_map)
                self.assertEqual(caught.exception.row, 2)
                self.assertEqual(caught.exception.column, "Init_Value")

    def test_integer_init_raw_bounds_and_half_away_from_zero(self) -> None:
        # Hand-computed raw boundaries distinguish std::round from ties-to-even.
        for signal_type, size, value, accepted in (
            ("Unsigned", 16, "65535", True),
            ("Unsigned", 16, "70000", False),
            ("Unsigned", 16, "65535.49", True),
            ("Unsigned", 16, "65535.5", False),
            ("Unsigned", 16, "-0.49999999999999994", True),
            ("Unsigned", 16, "-0.5", False),
            ("Signed", 16, "-32768", True),
            ("Signed", 16, "32767", True),
            ("Signed", 16, "-32768.49", True),
            ("Signed", 16, "-32768.5", False),
            ("Signed", 16, "32767.5", False),
            ("Signed", 2, "1.5", False),
            ("Signed", 2, "-2.5", False),
            ("Signed", 1, "-0.5", True),
            ("Signed", 1, "0.5", False),
        ):
            with self.subTest(signal_type=signal_type, size=size, value=value):
                self._check_init(signal_type=signal_type, size=size, init=value, accepted=accepted)

    def test_integer_init_scale_offset_finiteness_and_precision(self) -> None:
        for scale, offset, value, accepted in (
            ("-0.1", "10", "22.5", True),  # raw -125
            ("-0.5", "10", "16394", True),  # raw -32768
            ("-0.5", "10", "16394.25", False),  # raw -32768.5
            ("-0.5", "10", "-16373.5", True),  # raw 32767
            ("-0.5", "10", "-16373.75", False),  # raw 32767.5
            ("1e-300", "0", "1e308", False),  # non-finite division
            ("1", "-1e308", "1e308", False),  # non-finite subtraction
        ):
            with self.subTest(scale=scale, offset=offset, value=value):
                self._check_init(signal_type="Signed", scale=scale, offset=offset, init=value, accepted=accepted)
        for signal_type, value, accepted in (
            ("Signed", "9007199254740991", True),
            ("Signed", "9007199254740992", True),
            ("Signed", "9007199254740994", False),
            ("Signed", "-9007199254740992", True),
            ("Signed", "-9007199254740994", False),
            ("Unsigned", "9007199254740992", True),
            ("Unsigned", "9007199254740994", False),
            ("Unsigned", "-1", False),
        ):
            with self.subTest(signal_type=signal_type, value=value):
                self._check_init(signal_type=signal_type, length=8, size=64, init=value, accepted=accepted)
        # At Size 53 the unsigned raw maximum is one below the API precision cap.
        self._check_init(length=8, size=53, init="9007199254740991", accepted=True)
        self._check_init(length=8, size=53, init="9007199254740992", accepted=False)

    def test_ieee_init_binary32_range_and_double_policy(self) -> None:
        for value, accepted in (
            ("25.5", True),
            ("1e100", False),
            ("-1e100", False),
            ("3.4028234663852886e38", True),  # FLT_MAX
            ("3.4028235e38", True),  # rounds to finite FLT_MAX
            ("3.4028235677973366e38", False),  # binary32 overflow midpoint
            ("-3.4028235e38", True),
            ("-3.4028235677973366e38", False),
            ("1.401298464324817e-45", True),  # smallest positive binary32 subnormal
            ("1e-100", True),  # binary32 zero is finite; Core allows it
        ):
            with self.subTest(value=value):
                self._check_init(signal_type="IEEE Float", length=4, size=32, init=value, accepted=accepted)
        for value in ("1e100", "-1e100", "1.7976931348623157e308"):
            self._check_init(signal_type="IEEE Double", length=8, size=64, init=value, accepted=True)

    def test_init_physical_min_max_still_apply_before_encoding(self) -> None:
        for value, accepted in (("-2", True), ("2", True), ("-2.01", False), ("2.01", False)):
            with self.subTest(value=value):
                self._check_init(signal_type="Signed", scale="-0.5", offset="10", init=value,
                                 minimum="-2", maximum="2", accepted=accepted)


class FragmentAndOutputTests(unittest.TestCase):
    def test_checked_in_sample_generated_artifacts_are_not_stale(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = core.generate_output(
                ROOT / "Config" / "Maps" / "M485E" / "M485E_Device.csv",
                Path(temporary),
            )
            expected_capl = output / "CAPL" / "M485E" / "DevicePanel"
            actual_capl = ROOT / "Config" / "CAPL" / "M485E" / "DevicePanel"
            self.assertEqual(tree_hashes(expected_capl), tree_hashes(actual_capl))
            self.assertEqual(
                (
                    output
                    / "SystemVariables"
                    / "M485E_DevicePanel.generated.vsysvar"
                ).read_bytes(),
                (
                    ROOT
                    / "Config"
                    / "System Variables"
                    / "M485E_DevicePanel.generated.vsysvar"
                ).read_bytes(),
            )

    def test_address_groups_use_decimal_31_register_windows_and_skip_empty_groups(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root, 1)
            rows = [
                simple_signal(10000, "First"),
                simple_signal(10030, "LastInFirst"),
                simple_signal(10062, "ThirdWindow"),
            ]
            write_text(root / "Synthetic.csv", REGISTER_HEADER + "\n" + "\n".join(rows) + "\n")
            analysis = core.analyze_device_map(device)
            self.assertEqual(
                [(fragment.group_start, fragment.group_end) for fragment in analysis.fragments],
                [(10000, 10030), (10062, 10092)],
            )

    def test_page_split_and_file_names_for_34_45_72_150_signals(self) -> None:
        expected_pages = {34: 4, 45: 4, 72: 7, 150: 14}
        for signal_count, page_count in expected_pages.items():
            with self.subTest(signal_count=signal_count), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                device = create_fixture(root, signal_count)
                analysis = core.analyze_device_map(device)
                self.assertEqual(len(analysis.fragments), page_count)
                self.assertTrue(all(len(fragment.signals) <= core.MAX_ROWS_PER_FRAGMENT for fragment in analysis.fragments))
                self.assertTrue(any("_P" in fragment.file_name for fragment in analysis.fragments))

    def test_generation_is_deterministic_and_shared_devices_have_independent_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root / "input", 4, shared_map=True)
            base = root / "bin"
            first = core.generate_output(device, base)
            first_hashes = tree_hashes(first)
            second = core.generate_output(device, base)
            self.assertEqual(first_hashes, tree_hashes(second))
            self.assertTrue((second / "Index0").is_dir())
            self.assertTrue((second / "Index9").is_dir())
            system_text = (second / "SystemVariables" / "M485E_DevicePanel.generated.vsysvar").read_text(encoding="utf-8")
            self.assertIn('name="Index0"', system_text)
            self.assertIn('name="Index9"', system_text)

    def test_manifestless_output_is_never_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root / "input", 1)
            output = root / "bin" / "Output"
            output.mkdir(parents=True)
            marker = output / "company-file.txt"
            marker.write_text("keep", encoding="utf-8")
            with self.assertRaisesRegex(core.GeneratorError, "no Generator manifest"):
                core.generate_output(device, root / "bin")
            self.assertEqual(marker.read_text(encoding="utf-8"), "keep")

    def test_failure_before_swap_preserves_existing_generated_output(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            device = create_fixture(root / "input", 1)
            base = root / "bin"
            output = core.generate_output(device, base)
            before = tree_hashes(output)
            with mock.patch.object(core, "_populate_output", side_effect=RuntimeError("synthetic failure")):
                with self.assertRaisesRegex(RuntimeError, "synthetic failure"):
                    core.generate_output(device, base)
            self.assertEqual(before, tree_hashes(output))

    def test_xvp_contract_bindings_read_only_editable_escape_and_no_value_table(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = core.generate_output(FIXTURE, Path(temporary))
            xvp = output / "Index0" / "A10000_A10030.xvp"
            root = ET.parse(xvp).getroot()
            xml_text = xvp.read_text(encoding="utf-8")
            self.assertIn("Status &amp; Mode &lt;A&gt;", xml_text)
            self.assertIn("한글 온도", xml_text)
            self.assertNotIn("ComboBox", xml_text)
            self.assertNotIn("Value_Table", xml_text)
            self.assertTrue(any("GroupBoxControl" in node.attrib.get("Type", "") for node in root.findall(".//Object")))
            bindings = [node.text or "" for node in root.findall(".//Property[@Name='SymbolConfiguration']")]
            for index in range(3):
                self.assertEqual(sum(f";PhysicalValue;2;;;{index}" in value for value in bindings), 1)
                self.assertEqual(sum(f";S{index:03d};4;;;-1" in value for value in bindings), 1)
            self.assertEqual(
                sum(";FailedSignalName;4;;;-1" in value for value in bindings), 1
            )
            for control in root.findall(".//Object"):
                binding = control.find("./Property[@Name='SymbolConfiguration']")
                if binding is None:
                    continue
                read_only = control.find("./Property[@Name='ReadOnlyControl']")
                if ";RawHex;S" in (binding.text or ""):
                    self.assertEqual(read_only.text if read_only is not None else None, "True")
                if ";PhysicalValue;2;" in (binding.text or ""):
                    self.assertIsNone(read_only)

    def test_common_reset_xvp_and_manifest_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = core.generate_output(FIXTURE, Path(temporary))
            common = output / "Common" / "ResetAllDefault.xvp"
            self.assertTrue(common.is_file())
            common_text = common.read_text(encoding="utf-8")
            for variable in (
                "ResetAllDefault",
                "ResultCode",
                "CompletedDeviceCount",
                "FailedDeviceIndex",
            ):
                self.assertEqual(common_text.count(f";{variable};1;;;-1"), 1)
            manifest = json.loads((output / core.MANIFEST_NAME).read_text(encoding="utf-8"))
            self.assertEqual(manifest["generatorVersion"], "0.6.0")
            self.assertEqual([device["index"] for device in manifest["devices"]], [0, 7])
            core.validate_output(output)


class RawHexTests(unittest.TestCase):
    def test_lengths_and_endianness(self) -> None:
        self.assertEqual(core.extract_raw_hex([0x1234], "BigEndian", 0, 16), "0x1234")
        self.assertEqual(core.extract_raw_hex([0x1234, 0x5678], "BigEndian", 0, 32), "0x12345678")
        self.assertEqual(core.extract_raw_hex([0x1234, 0x5678, 0x9ABC], "BigEndian", 0, 48), "0x123456789ABC")
        self.assertEqual(core.extract_raw_hex([0xFEDC, 0xBA98, 0x7654, 0x3210], "BigEndian", 0, 64), "0xFEDCBA9876543210")
        self.assertEqual(core.extract_raw_hex([0x3412, 0x7856], "LittleEndian", 0, 32), "0x12345678")

    def test_start_bit_crossing_nibble_width_and_leading_zero(self) -> None:
        self.assertEqual(core.extract_raw_hex([0x0001], "BigEndian", 0, 1), "0x1")
        self.assertEqual(core.extract_raw_hex([0x001F], "BigEndian", 0, 5), "0x1F")
        self.assertEqual(core.extract_raw_hex([0x0001, 0x8000], "BigEndian", 15, 2), "0x3")
        self.assertEqual(core.extract_raw_hex([0x00FF], "BigEndian", 0, 16), "0x00FF")

    def test_signed_temperature_and_ieee_patterns(self) -> None:
        self.assertEqual(core.extract_raw_hex([0xFF83], "BigEndian", 0, 16), "0xFF83")
        self.assertEqual(core.extract_raw_hex([0x00FF], "BigEndian", 0, 16), "0x00FF")
        float_bits = struct.unpack(">I", struct.pack(">f", 25.5))[0]
        self.assertEqual(float_bits, 0x41CC0000)
        self.assertEqual(core.extract_raw_hex([0x41CC, 0x0000], "BigEndian", 0, 32), "0x41CC0000")
        for value in (123.5, -123.5):
            bits = struct.unpack(">Q", struct.pack(">d", value))[0]
            words = [(bits >> shift) & 0xFFFF for shift in (48, 32, 16, 0)]
            self.assertEqual(core.extract_raw_hex(words, "BigEndian", 0, 64), f"0x{bits:016X}")

    def test_multiple_signals_share_one_frame(self) -> None:
        words = [0x1234, 0xABCD]
        self.assertEqual(core.extract_raw_hex(words, "BigEndian", 0, 8), "0xCD")
        self.assertEqual(core.extract_raw_hex(words, "BigEndian", 8, 8), "0xAB")


class ProbeFixtureTests(unittest.TestCase):
    def test_user_float_array_probe_serializer_and_symbol_types(self) -> None:
        probe = ROOT / "tests" / "capl" / "M485E_PanelFloatArrayProbe.xvp"
        root = ET.parse(probe).getroot()
        self.assertIn("Version=9.0.137.0", root.attrib["Type"])
        bindings = [node.text or "" for node in root.findall(".//Property[@Name='SymbolConfiguration']")]
        self.assertTrue(any(";SignalValue;2;;;0" in value for value in bindings))
        self.assertTrue(any(";ResultCode;1;;;-1" in value for value in bindings))
        sample_string = (
            "4;16;M485E_DevicePanel;Index0;RawHex;S000;4;;;-1"
        )
        self.assertEqual(sample_string.split(";")[6], "4")


if __name__ == "__main__":
    unittest.main(verbosity=2)

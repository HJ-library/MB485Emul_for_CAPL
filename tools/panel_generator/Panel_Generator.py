"""GUI and automation CLI for the M485E Device Signal Panel Generator."""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from panel_generator.core import (  # type: ignore[import-not-found]
        GENERATOR_VERSION,
        Analysis,
        GeneratorError,
        analyze_device_map,
        generate_output,
    )
else:
    from .core import (
        GENERATOR_VERSION,
        Analysis,
        GeneratorError,
        analyze_device_map,
        generate_output,
    )


def executable_directory() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parent


def _analysis_payload(analysis: Analysis) -> dict[str, object]:
    return {
        "generatorVersion": GENERATOR_VERSION,
        "deviceMap": str(analysis.device_map_path),
        "deviceMapSha256": analysis.device_map_sha256,
        "deviceCount": len(analysis.devices),
        "signalCount": analysis.signal_count,
        "devices": analysis.preview(),
    }


def _run_cli(args: argparse.Namespace) -> int:
    if args.version:
        print(GENERATOR_VERSION)
        return 0
    if args.tk_smoke:
        import tkinter as tk

        root = tk.Tk()
        root.withdraw()
        root.update_idletasks()
        root.destroy()
        print(f"tkinter {tk.TkVersion} PASS")
        return 0
    if args.analyze:
        analysis = analyze_device_map(args.analyze)
        payload = _analysis_payload(analysis)
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        return 0
    if args.generate:
        analysis = analyze_device_map(args.generate)
        output = generate_output(analysis, executable_directory())
        payload = _analysis_payload(analysis)
        payload["output"] = str(output)
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
        return 0
    return _run_gui()


def _run_gui() -> int:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    class Application:
        def __init__(self, root: tk.Tk) -> None:
            self.root = root
            self.root.title(f"M485E Device Panel Generator v{GENERATOR_VERSION}")
            self.root.minsize(860, 520)
            self.csv_path = tk.StringVar()
            self.status = tk.StringVar(value="Select M485E_Device.csv, then Analyze.")
            self.analysis: Analysis | None = None

            top = ttk.Frame(root, padding=12)
            top.pack(fill="both", expand=True)
            path_row = ttk.Frame(top)
            path_row.pack(fill="x")
            ttk.Label(path_row, text="M485E_Device.csv").pack(side="left")
            ttk.Entry(path_row, textvariable=self.csv_path).pack(
                side="left", fill="x", expand=True, padx=8
            )
            ttk.Button(path_row, text="Browse...", command=self.browse).pack(side="left")

            button_row = ttk.Frame(top)
            button_row.pack(fill="x", pady=(10, 8))
            ttk.Button(button_row, text="Analyze / Validate", command=self.analyze).pack(
                side="left"
            )
            self.generate_button = ttk.Button(
                button_row, text="Generate", command=self.generate, state="disabled"
            )
            self.generate_button.pack(side="left", padx=8)
            ttk.Button(button_row, text="Open Output", command=self.open_output).pack(
                side="left"
            )

            columns = ("index", "name", "map", "signals", "xvps")
            self.tree = ttk.Treeview(top, columns=columns, show="headings")
            headings = {
                "index": "Device Index",
                "name": "Device Name",
                "map": "CSV_DB_Name",
                "signals": "Signal Count",
                "xvps": "XVP Count",
            }
            widths = {"index": 95, "name": 190, "map": 285, "signals": 95, "xvps": 80}
            for column in columns:
                self.tree.heading(column, text=headings[column])
                self.tree.column(column, width=widths[column], anchor="center" if column in ("index", "signals", "xvps") else "w")
            self.tree.pack(fill="both", expand=True)
            ttk.Label(top, textvariable=self.status, relief="sunken", anchor="w").pack(
                fill="x", pady=(8, 0)
            )

        def browse(self) -> None:
            selected = filedialog.askopenfilename(
                title="Select M485E_Device.csv",
                filetypes=(("CSV files", "*.csv"), ("All files", "*.*")),
            )
            if selected:
                self.csv_path.set(selected)
                self.analysis = None
                self.generate_button.configure(state="disabled")

        def _show_error(self, exc: BaseException) -> None:
            self.status.set(str(exc))
            messagebox.showerror("M485E Panel Generator", str(exc))

        def analyze(self) -> None:
            try:
                analysis = analyze_device_map(self.csv_path.get())
            except (GeneratorError, OSError) as exc:
                self.analysis = None
                self.generate_button.configure(state="disabled")
                self._show_error(exc)
                return
            for item in self.tree.get_children():
                self.tree.delete(item)
            for row in analysis.preview():
                self.tree.insert(
                    "",
                    "end",
                    values=(
                        row["index"],
                        row["deviceName"],
                        row["csvDbName"],
                        row["signalCount"],
                        row["xvpCount"],
                    ),
                )
            self.analysis = analysis
            self.generate_button.configure(state="normal")
            self.status.set(
                f"Validated {len(analysis.devices)} Device(s), {analysis.signal_count} Signal(s)."
            )

        def generate(self) -> None:
            if self.analysis is None:
                self.analyze()
            if self.analysis is None:
                return
            try:
                output = generate_output(self.analysis, executable_directory())
            except (GeneratorError, OSError) as exc:
                self._show_error(exc)
                return
            self.status.set(f"Generated and validated: {output}")
            messagebox.showinfo("M485E Panel Generator", f"Generation completed.\n{output}")

        def open_output(self) -> None:
            output = executable_directory() / "Output"
            if not output.is_dir():
                self._show_error(GeneratorError("Output has not been generated", path=output))
                return
            os.startfile(output)  # type: ignore[attr-defined]

    root = tk.Tk()
    Application(root)
    root.mainloop()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Generate CANalyzer 9.x Device Signal Panel fragments from M485E_Device.csv."
    )
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--analyze", metavar="DEVICE_CSV", help="validate and print deterministic JSON preview")
    action.add_argument("--generate", metavar="DEVICE_CSV", help="generate Output beside this executable")
    action.add_argument("--version", action="store_true", help="print Generator version")
    action.add_argument("--tk-smoke", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    try:
        return _run_cli(args)
    except (GeneratorError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

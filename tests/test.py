#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["rich>=13"]
# ///
"""Run every wasm binary in tests/build against build/main.

A test passes iff `build/main -m <wasm>` exits with status 0. Each .wat
case is responsible for computing its own pass/fail decision and returning
0 (success) or non-zero (failure) as the wasm `_start`'s i32 return value.
"""

import subprocess
import sys
import time
from pathlib import Path

from rich.console import Console, Group, RenderableType
from rich.panel import Panel
from rich.progress import (
    BarColumn,
    MofNCompleteColumn,
    Progress,
    SpinnerColumn,
    TextColumn,
    TimeElapsedColumn,
)
from rich.rule import Rule
from rich.table import Table
from rich.text import Text

WASM_MAGIC = b"\0asm"


def is_wasm(path: Path) -> bool:
    if not path.is_file():
        return False
    try:
        with path.open("rb") as f:
            return f.read(4) == WASM_MAGIC
    except OSError:
        return False


def discover_tests(build_dir: Path) -> list[Path]:
    return sorted(p for p in build_dir.rglob("*") if is_wasm(p))


def run_test(main_bin: Path, wasm: Path) -> tuple[bool, float, str, str, str | None]:
    """Run one test and return (ok, elapsed, stdout, stderr, reason_if_failed)."""
    start = time.monotonic()
    proc = subprocess.run(
        [str(main_bin), "-m", str(wasm)],
        capture_output=True,
        text=True,
    )
    elapsed = time.monotonic() - start

    if proc.returncode != 0:
        return False, elapsed, proc.stdout, proc.stderr, f"exit code {proc.returncode}"

    return True, elapsed, proc.stdout, proc.stderr, None


def main() -> int:
    console = Console()

    repo_root = Path(__file__).resolve().parent.parent
    main_bin = repo_root / "build" / "main"
    build_dir = repo_root / "tests" / "build"

    if not main_bin.exists():
        console.print(f"[bold red]error:[/] {main_bin} not found — build it first")
        return 2
    if not build_dir.is_dir():
        console.print(f"[bold red]error:[/] {build_dir} not found")
        return 2

    tests = discover_tests(build_dir)
    if not tests:
        console.print(f"[bold red]error:[/] no wasm binaries found under {build_dir}")
        return 2

    failures: list[tuple[Path, str, str, str]] = []
    results: list[tuple[Path, bool, float]] = []

    progress = Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        MofNCompleteColumn(),
        TimeElapsedColumn(),
        console=console,
        transient=True,
    )

    console.rule(f"[bold]Running {len(tests)} test(s)")
    with progress:
        task = progress.add_task("running…", total=len(tests))
        for wasm in tests:
            name = wasm.relative_to(build_dir)
            progress.update(task, description=f"[cyan]{name}")
            ok, elapsed, out, err, reason = run_test(main_bin, wasm)
            results.append((name, ok, elapsed))
            mark = "[green]✓[/]" if ok else "[red]✗[/]"
            console.print(f"  {mark} {name} [dim]({elapsed:.2f}s)[/]")
            if not ok:
                failures.append((name, out, err, reason or "failed"))
            progress.advance(task)

    table = Table(show_header=False, box=None, pad_edge=False)
    table.add_column(justify="right")
    table.add_column()
    passed = sum(1 for _, ok, _ in results if ok)
    total_time = sum(t for _, _, t in results)
    table.add_row("[green]passed[/]", str(passed))
    table.add_row("[red]failed[/]" if failures else "[dim]failed[/]", str(len(failures)))
    table.add_row("total", str(len(tests)))
    table.add_row("time", f"{total_time:.2f}s")
    console.print(Panel(table, title="Summary", border_style="green" if not failures else "red"))

    if failures:
        console.rule("[bold red]Failures")
        for name, out, err, reason in failures:
            parts: list[RenderableType] = [
                Rule(f"[bold red]{name}[/] [dim]— {reason}[/]", style="red"),
            ]
            if out:
                parts.append(Panel(Text(out.rstrip()), title="stdout", border_style="yellow"))
            if err:
                parts.append(Panel(Text(err.rstrip()), title="stderr", border_style="red"))
            if not out and not err:
                parts.append(Text("(no output)", style="dim"))
            console.print(Group(*parts))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

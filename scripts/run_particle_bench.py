import csv
import subprocess
from pathlib import Path


def main():
    repo_root = Path(__file__).resolve().parent.parent
    exe_path = (
        repo_root / "out" / "build" / "Default" / "Debug" / "MastersLavaSimulation.exe"
    )
    if not exe_path.exists():
        raise FileNotFoundError(
            f"Executable not found: {exe_path}\nPlease build the project before running this script."
        )

    counts = [((8 << 10) * i) for i in range(1, 13)]  # 8192, 16384, ..., 98304
    output_csv = repo_root / "bench_results.csv"

    with output_csv.open("w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["particles", "sim_ms", "render_ms", "total_ms"])

        for count in counts:
            print(f"Running simulation with {count} particles...")
            proc = subprocess.run(
                [str(exe_path), str(count)], capture_output=True, text=True
            )

            if proc.stdout:
                print(proc.stdout)
            if proc.returncode != 0:
                print(proc.stderr, end="", flush=True)
                raise SystemExit(proc.returncode)

            sim_avg = None
            render_avg = None
            total_avg = None
            for line in proc.stdout.splitlines():
                if line.startswith("Simulation avg  : "):
                    sim_avg = float(line.split(": ")[1].split()[0])
                elif line.startswith("Render avg      : "):
                    render_avg = float(line.split(": ")[1].split()[0])
                elif line.startswith("Total avg       : "):
                    total_avg = float(line.split(": ")[1].split()[0])

            writer.writerow([count, sim_avg, render_avg, total_avg])
            print(
                f"Recorded {count} particles -> sim {sim_avg} ms, render {render_avg} ms, total {total_avg} ms\n"
            )

    print(f"Benchmark finished. Results written to: {output_csv}")


if __name__ == "__main__":
    main()

"""
MLOps experiment tracking for the Monte Carlo power-consumption forecast.

Every benchmark configuration (sequential / OpenMP / MPI / hybrid) is logged as one
MLflow run, so the whole scaling study becomes comparable, versioned and queryable
instead of living in a CSV nobody can filter.

Two modes
---------
1) Ingest an existing benchmark file (no recompiling needed):

       python mlops/track_experiments.py --from-csv results/performance_summary.csv

2) Run the benchmarks and log the new rows (needs the binaries to be built):

       python mlops/track_experiments.py --run --seqtime 675.139778

What is logged per run
----------------------
params    implementation, cores, nsims
metrics   sim_time, wall_time, speedup, efficiency, throughput
artifacts results/<version>_report.txt (when present)
tags      dataset, cpu_count, python version

The report itself is the "model card": the point of tracking is being able to answer
"which configuration gave the best speedup per core, and was the result still correct?"
"""
from __future__ import annotations

import argparse
import csv
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
RESULTS = PROJECT_ROOT / "results"
SUMMARY_CSV = RESULTS / "performance_summary.csv"
TRACKING_DIR = PROJECT_ROOT / "mlruns"
EXPERIMENT = "hpc-monte-carlo-forecast"

FIELDS = {
    "version": str,
    "label": str,
    "cores": int,
    "nsims": int,
    "sim_time": float,
    "wall_time": float,
    "speedup": float,
    "efficiency": float,
    "throughput": float,
}


def require_mlflow():
    try:
        import mlflow  # noqa: F401
    except ImportError:
        sys.exit(
            "mlflow is not installed.\n"
            "  pip install -r mlops/requirements.txt\n"
            "  (or: pip install mlflow)"
        )
    import mlflow

    mlflow.set_tracking_uri(TRACKING_DIR.as_uri())
    mlflow.set_experiment(EXPERIMENT)
    return mlflow


def read_rows(csv_path: Path) -> list[dict]:
    if not csv_path.exists():
        sys.exit(f"benchmark file not found: {csv_path}")

    rows = []
    with csv_path.open(newline="", encoding="utf-8") as fh:
        for raw in csv.DictReader(fh):
            row = {}
            for key, caster in FIELDS.items():
                value = raw.get(key)
                if value in (None, ""):
                    row[key] = None
                    continue
                try:
                    row[key] = caster(value)
                except ValueError:
                    row[key] = None
            rows.append(row)
    return rows


def log_rows(rows: list[dict], source: str) -> int:
    mlflow = require_mlflow()
    logged = 0

    for row in rows:
        if row["version"] is None or row["cores"] is None:
            continue

        with mlflow.start_run(run_name=f"{row['version']} · {row['cores']} cores"):
            mlflow.set_tags(
                {
                    "source": source,
                    "dataset": "UCI household electric power consumption",
                    "cpu_count": str(_cpu_count()),
                    "platform": platform.platform(),
                    "python": platform.python_version(),
                }
            )
            mlflow.log_params(
                {
                    "implementation": row["version"],
                    "label": row["label"],
                    "cores": row["cores"],
                    "nsims": row["nsims"],
                }
            )

            metrics = {
                "sim_time_s": row["sim_time"],
                "wall_time_s": row["wall_time"],
                "speedup": row["speedup"],
                "efficiency_pct": row["efficiency"],
                "throughput_sims_per_s": row["throughput"],
            }
            mlflow.log_metrics({k: v for k, v in metrics.items() if v is not None})

            report = RESULTS / f"{re.sub(r'_(2t|4t|7t|2r|4r|8r|2r2t|2r4t|4r2t)$', '', row['version'].replace('openmp_', 'openmp_').replace('mpi_', 'mpi_'))}_report.txt"
            candidates = [
                RESULTS / f"{row['version']}_report.txt",
                report,
                RESULTS / f"{row['version'].split('_')[0]}_report.txt",
            ]
            for candidate in candidates:
                if candidate.exists():
                    mlflow.log_artifact(str(candidate), artifact_path="reports")
                    break

            logged += 1
            print(f"  logged {row['version']:<14} cores={row['cores']:<3} speedup={row['speedup']}")

    # one run that carries the whole sweep as an artifact, for reproducibility
    with mlflow.start_run(run_name=f"benchmark-summary · {source}"):
        mlflow.set_tag("kind", "summary")
        if SUMMARY_CSV.exists():
            mlflow.log_artifact(str(SUMMARY_CSV), artifact_path="benchmark")
        for extra in ("Report.pdf", "Figure/Figure6_speedup.png", "Figure/Figure7_efficiency.png"):
            path = PROJECT_ROOT / extra
            if path.exists():
                mlflow.log_artifact(str(path), artifact_path="report")

    return logged


def _cpu_count() -> int:
    import os

    return os.cpu_count() or 0


def run_benchmarks(seqtime: float | None, nsims: int | None) -> None:
    """Execute the binaries; each run appends its own row to the summary CSV."""
    exe = ".exe" if platform.system() == "Windows" else ""
    mpi_launcher = "mpiexec" if platform.system() == "Windows" else "mpirun"

    def exists(name: str) -> bool:
        return (PROJECT_ROOT / "src" / f"{name}{exe}").exists()

    if not exists("sequential"):
        sys.exit("src/sequential%s not found - build the project first (see README)" % exe)

    seq_flag = f"--seqtime {seqtime}" if seqtime else ""
    nsim_flag = f"--nsims {nsims}" if nsims else ""

    plan = [
        ("sequential", ["sequential"]),
        ("openmp_2t", ["openmp", "--threads", "2"]),
        ("openmp_4t", ["openmp", "--threads", "4"]),
        ("openmp_7t", ["openmp", "--threads", "7"]),
        ("mpi_2r", [mpi_launcher, "-n", "2", "mpi_version"]),
        ("mpi_4r", [mpi_launcher, "-n", "4", "mpi_version"]),
        ("mpi_8r", [mpi_launcher, "-n", "8", "mpi_version"]),
        ("hybrid_2r2t", [mpi_launcher, "-n", "2", "hybrid", "--threads", "2"]),
        ("hybrid_4r2t", [mpi_launcher, "-n", "4", "hybrid", "--threads", "2"]),
    ]

    for version, cmd in plan:
        binary = cmd[-1] if cmd[0] == mpi_launcher else cmd[0]
        if not exists(binary):
            print(f"skip {version}: src/{binary}{exe} not built")
            continue
        full = [str(PROJECT_ROOT / "src" / f"{c}{exe}") if i == len(cmd) - 1 or (cmd[0] == mpi_launcher and i == 2) else c
                for i, c in enumerate(cmd)]
        if seq_flag:
            full += seq_flag.split()
        if nsim_flag:
            full += nsim_flag.split()
        print("run:", " ".join(full))
        subprocess.run(full, cwd=PROJECT_ROOT, check=False)


def print_best(rows: list[dict]) -> None:
    parsed = [r for r in rows if r["speedup"] is not None]
    if not parsed:
        return
    parallel = [r for r in parsed if (r["cores"] or 1) > 1] or parsed
    best_speed = max(parsed, key=lambda r: r["speedup"])
    best_eff = max(parallel, key=lambda r: r["efficiency"])
    best_thr = max(parsed, key=lambda r: r["throughput"])
    print("\nbest speedup   :", best_speed["label"], f"-> {best_speed['speedup']:.2f}x on {best_speed['cores']} cores")
    print("best efficiency:", best_eff["label"], f"-> {best_eff['efficiency']:.1f}% (best parallel config)")
    print("best throughput:", best_thr["label"], f"-> {best_thr['throughput']:.0f} sims/s")


def main() -> None:
    ap = argparse.ArgumentParser(description="Log Monte Carlo HPC benchmarks to MLflow")
    ap.add_argument("--from-csv", type=Path, default=None, help="ingest an existing benchmark CSV")
    ap.add_argument("--run", action="store_true", help="execute the binaries first, then log new rows")
    ap.add_argument("--seqtime", type=float, default=None, help="sequential baseline time T1")
    ap.add_argument("--nsims", type=int, default=None, help="override the simulation count")
    ap.add_argument("--clean", action="store_true", help="delete the previous mlruns/ store first")
    args = ap.parse_args()

    if args.clean and TRACKING_DIR.exists():
        shutil.rmtree(TRACKING_DIR)
        print("cleared mlruns/")

    if args.run:
        run_benchmarks(args.seqtime, args.nsims)

    csv_path = args.from_csv or SUMMARY_CSV
    rows = read_rows(csv_path)
    print(f"\nentry point : MLflow  (tracking uri: {TRACKING_DIR.as_uri()})")
    print(f"experiment  : {EXPERIMENT}")
    print(f"source      : {csv_path}")
    print(f"configurations: {len(rows)}\n")

    logged = log_rows(rows, csv_path.name)
    print_best(rows)

    print(f"\nlogged {logged} runs. Inspect them with:\n  mlflow ui --backend-store-uri \"{TRACKING_DIR.as_uri()}\" -p 5000")


if __name__ == "__main__":
    main()

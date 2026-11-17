# MLOps layer

Experiment tracking for the HPC benchmark study, built on **MLflow**.

The C code produces one row per configuration in `results/performance_summary.csv`
(implementation, cores, sim time, speedup, efficiency, throughput). This layer turns that
file - or a fresh benchmark run - into versioned, comparable MLflow runs.

## Why this exists

A scaling study is an experiment, not a build: parameters change (cores, thread counts,
simulation count), results have to be compared across machines, and the artefacts (reports,
speedup curves) need to stay attached to the run that produced them. A CSV loses that
context; MLflow keeps it.

## Install

```bash
pip install -r mlops/requirements.txt
# or simply
pip install mlflow
```

## Use

Ingest the benchmark file that already exists (nothing to recompile):

```bash
python mlops/track_experiments.py --from-csv results/performance_summary.csv
```

Run the benchmarks and log the new rows:

```bash
python mlops/track_experiments.py --run --seqtime 675.139778
```

Reset the store and log again:

```bash
python mlops/track_experiments.py --clean --from-csv results/performance_summary.csv
```

Inspect the runs:

```bash
mlflow ui --backend-store-uri "file:///C:/Users/<you>/Desktop/HPC/mlruns" -p 5000
```

Then open http://localhost:5000 - the runs are searchable by parameter and sortable by
metric, and every run carries the benchmark report it produced.

## What each run records

| Kind | Content |
|---|---|
| **params** | implementation, cores, simulation count |
| **metrics** | sim time, wall time, speedup, efficiency %, throughput |
| **tags** | dataset, machine CPU count, platform, Python version, source file |
| **artifacts** | `results/<version>_report.txt`, plus the full sweep CSV and the report PDF on the summary run |

## What it demonstrates

- **Experiment tracking** rather than spreadsheet archaeology: every configuration is a run
  with parameters, metrics and artefacts.
- **Reproducibility**: the machine's CPU count, platform and dataset are captured as tags, so
  two machines' results can be compared honestly.
- **Artifact lineage**: the report that a speedup number came from is attached to the run
  that produced the number.
- **Automation over manual work**: the script ingests results or drives the binaries, so the
  tracking is a pipeline step instead of a manual ritual.

## Limitations / next steps

- No model registry use: the "model" here is a simulation kernel, not a serialisable artifact.
  The natural extension is registering each compiled binary by hash as a versioned artefact.
- Tracking is local (`mlruns/`, ignored by git). A team setup would point
  `MLFLOW_TRACKING_URI` at a shared server and store artefacts in object storage.
- The `--run` mode depends on the binaries being built and on `mpiexec` being available;
  `--from-csv` has no such dependency, which is why it is the default path.

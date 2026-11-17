# Monte Carlo Power Consumption Forecast — HPC Project

Four versions, one shared kernel (`common.h`), all producing **bit-identical
statistical results** (this is the correctness verification for the report).

| File         | Parallelism model (per project requirement)                  |
|--------------|--------------------------------------------------------------|
| sequential.c | none — the T1 baseline                                       |
| openmp.c     | **OpenMP = TASK parallelism**: 1 task per column (7 tasks)   |
| mpi.c        | **MPI = DATA parallelism**: simulation blocks over ranks     |
| hybrid.c     | MPI data parallelism × OpenMP task parallelism combined      |

## Build

Linux / macOS / WSL:
```
gcc  -O2 -o sequential sequential.c -lm
gcc  -O2 -fopenmp -o openmp openmp.c -lm
mpicc -O2 -o mpi_version mpi.c -lm
mpicc -O2 -fopenmp -o hybrid hybrid.c -lm
```

Windows (MinGW + MS-MPI):
```
gcc -O2 -o sequential.exe sequential.c -lm
gcc -O2 -fopenmp -o openmp.exe openmp.c -lm
gcc -O2 -o mpi_version.exe mpi.c -lm -I"%MSMPI_INC%" -L"%MSMPI_LIB64%" -lmsmpi
gcc -O2 -fopenmp -o hybrid.exe hybrid.c -lm -I"%MSMPI_INC%" -L"%MSMPI_LIB64%" -lmsmpi
```
(On Windows use `mpiexec -n <ranks>` instead of `mpirun -np <ranks>`.)

## Data

Default path is relative: `data/household_power_consumption.txt`
(UCI "Individual household electric power consumption" dataset).
Override anywhere with `--data <path>` — no hardcoded D:/ paths, so the
project runs on any machine at the BYOD exam.

## Benchmark procedure (Performance Evaluation chapter)

1. Sequential baseline — note the printed Sim Time as T1:
   ```
   ./sequential
   ```
2. Pure OpenMP (task parallelism — saturates at 7, the number of columns):
   ```
   ./openmp --threads 1 --seqtime T1
   ./openmp --threads 2 --seqtime T1
   ./openmp --threads 4 --seqtime T1
   ./openmp --threads 7 --seqtime T1
   ```
3. Pure MPI (data parallelism — scales with any rank count):
   ```
   mpirun -np 1 ./mpi_version --seqtime T1
   mpirun -np 2 ./mpi_version --seqtime T1
   mpirun -np 4 ./mpi_version --seqtime T1
   mpirun -np 8 ./mpi_version --seqtime T1
   ```
4. Hybrid (keep ranks × threads ≤ physical cores):
   ```
   mpirun -np 2 ./hybrid --threads 2 --seqtime T1
   mpirun -np 2 ./hybrid --threads 4 --seqtime T1
   mpirun -np 4 ./hybrid --threads 2 --seqtime T1
   ```

Every run appends one row (cores, Tn, speedup Sn=T1/Tn, efficiency En=Sn/n,
throughput) to `results/performance_summary.csv` — plot speedup vs. number of
processing elements directly from it.

Tip: `-DNUM_SIMS=200000` at compile time scales the workload up for
better timing resolution on fast machines.

## Correctness verification (mention this in the report)

The seed of simulation `sim` for column `c` is a pure function of the global
indices: `seed = c*NUM_SIMS + sim + 1`. It does not depend on which rank or
thread executes the simulation, so sequential, OpenMP, MPI, and hybrid
produce identical means and confidence intervals, and the partial sums of
any block decomposition add up exactly to the full-range sums. (This also
gives each column a disjoint random stream — no cross-column correlation.)

## Mapping to the report template

- **Decomposition–Partitioning**: data decomposition of the simulation index
  space (MPI, block distribution) + task decomposition over the 7 columns
  (OpenMP tasks). Dependency graph: load/normalize → 7 parameter-estimation
  nodes on parallel paths → fan-out into simulation blocks → reduction →
  report.
- **Communication**: MPI_Bcast (one-to-all, 21+14 doubles) and MPI_Reduce
  (all-to-one, 2×7×30 doubles). Communication volume is constant and tiny
  versus O(NUM_SIMS × DAYS × STEPS) compute → explains near-linear speedup.
- **Aggregation**: per-day partial sums (sum_d, sum2_d) accumulated locally,
  sum-reduced across ranks; mean/CI computed only on fully reduced sums.
- **Mapping**: static block mapping of simulations to ranks (identical cost
  per simulation → near-perfect load balance at any rank count); untied task
  scheduling of columns to threads (saturates at 7 tasks — discuss the
  granularity limit and the 7 % threads load-imbalance steps, e.g. 4 threads
  → ceil(7/4)=2 rounds → upper bound 3.5×).

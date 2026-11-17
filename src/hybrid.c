#include "common.h"
#include <mpi.h>
#include <omp.h>

int main(int argc, char *argv[]) 
{
    int nthreads = 2;
    double seq_ref = 0.0;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    parse_common_args(argc, argv, &nthreads, &seq_ref);
    omp_set_num_threads(nthreads);


    int s0 = (int)((long long)NUM_SIMS * rank       / size);
    int s1 = (int)((long long)NUM_SIMS * (rank + 1) / size);

    if (rank == 0) 
    {
        printf("Hybrid | %d ranks x %d threads = %d workers | %d sims\n",
               size, nthreads, size * nthreads, NUM_SIMS);
        printf("MPI    DATA parallelism : ~%d sims/rank (block distribution)\n",
               NUM_SIMS / size);
        printf("OpenMP TASK parallelism : 7 columns x 2 dependent stages "
               "(simulate -> local reduce) with explicit depend clauses\n\n");
        fflush(stdout);
    }

    double params[NCOLS * 3], col_mean[NCOLS], col_std[NCOLS];

    if (rank == 0) 
    {
        int nrows = 0; double *data[NCOLS];

        load_csv(data, col_mean, col_std, &nrows);

        for (int c = 0; c < NCOLS; c++) 
        {
            estimate_params(data[c], nrows, &params[c*3+0], &params[c*3+1], &params[c*3+2]);
            free(data[c]);
        }
        printf("[Rank 0] Broadcasting parameters to all ranks...\n\n");
        fflush(stdout);
    }


    MPI_Bcast(params,   NCOLS * 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(col_mean, NCOLS,     MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(col_std,  NCOLS,     MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double wall = MPI_Wtime();

    double local_sum [NCOLS][DAYS] = {{0}};
    double local_sum2[NCOLS][DAYS] = {{0}};
    double col_times[NCOLS] = {0};

    double sums [NCOLS][DAYS];
    double sums2[NCOLS][DAYS];
    double t_start[NCOLS];

    double sim_start = MPI_Wtime();

    
    #pragma omp parallel num_threads(nthreads)
    {
        #pragma omp single
        {
            for (int c = 0; c < NCOLS; c++) 
            {

                #pragma omp task firstprivate(c) depend(out: sums[c])
                {
                    t_start[c] = omp_get_wtime();
                    for (int d = 0; d < DAYS; d++) { sums[c][d] = 0.0; sums2[c][d] = 0.0; }
                    run_column_chunk(c, s0, s1,params[c*3+0], params[c*3+1], params[c*3+2], sums[c], sums2[c]);
                }

            
                #pragma omp task firstprivate(c) depend(in: sums[c])
                {
                    for (int d = 0; d < DAYS; d++) 
                    {
                        local_sum [c][d] = sums[c][d];
                        local_sum2[c][d] = sums2[c][d];
                    }
                    col_times[c] = omp_get_wtime() - t_start[c];
                    printf("[Rank %d / Thread %d] %-25s done in %.2fs\n",rank, omp_get_thread_num(), COL_NAMES[c], col_times[c]);
                    fflush(stdout);
                }
            }

         
            #pragma omp taskwait
        }
    }


    double sim_time_local = MPI_Wtime() - sim_start;

    double glob_sum [NCOLS][DAYS], glob_sum2[NCOLS][DAYS];
    double max_col_times[NCOLS], sim_time;

    MPI_Reduce(local_sum,       glob_sum,       NCOLS*DAYS, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(local_sum2,      glob_sum2,      NCOLS*DAYS, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(col_times,       max_col_times,  NCOLS,      MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&sim_time_local, &sim_time,      1,          MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double wall_total = MPI_Wtime() - wall;

 
    if (rank == 0) {
        double all_mn[NCOLS][DAYS], all_lo[NCOLS][DAYS], all_hi[NCOLS][DAYS];
        for (int c = 0; c < NCOLS; c++)

            finalize_stats(glob_sum[c], glob_sum2[c], NUM_SIMS,all_mn[c], all_lo[c], all_hi[c]);

        char label[64]; sprintf(label, "Hybrid (%d ranks x %d threads)", size, nthreads);
        char fname[32]; sprintf(fname, "hybrid_%dr%dt", size, nthreads);
        save_report(fname, label, sim_time, wall_total, NUM_SIMS,size * nthreads, seq_ref,max_col_times, all_mn, all_lo, all_hi, col_mean, col_std);
    }

    MPI_Finalize();
    return 0;
}
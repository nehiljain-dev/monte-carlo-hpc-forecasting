#include "common.h"
#include <mpi.h>

int main(int argc, char *argv[]) 
{
    double seq_ref = 0.0;

    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    parse_common_args(argc, argv, NULL, &seq_ref);

   
    int s0 = (int)((long long)NUM_SIMS * rank       / size);
    int s1 = (int)((long long)NUM_SIMS * (rank + 1) / size);

    if (rank == 0) 
    {
        printf("MPI | %d ranks | %d sims | %d days x %d steps\n",
               size, NUM_SIMS, DAYS, STEPS_PER_DAY);
        printf("MPI DATA parallelism: each rank simulates a block of ~%d sims "
               "for ALL %d columns\n\n", NUM_SIMS / size, NCOLS);
        fflush(stdout);
    }

    printf("[Rank %d] simulations [%d, %d)\n", rank, s0, s1); fflush(stdout);

    double params[NCOLS * 3], col_mean[NCOLS], col_std[NCOLS];

    if (rank == 0) 
    {
        int nrows = 0; double *data[NCOLS];
        load_csv(data, col_mean, col_std, &nrows);
        for (int c = 0; c < NCOLS; c++) 
        {
            estimate_params(data[c], nrows, &params[c*3 + 0], &params[c*3 + 1], &params[c*3 + 2]);
            free(data[c]);
        }
        printf("[Rank 0] Broadcasting parameters...\n\n"); fflush(stdout);
    }


    MPI_Bcast(params,   NCOLS * 3, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(col_mean, NCOLS,     MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(col_std,  NCOLS,     MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double wall = MPI_Wtime();

    double local_sum [NCOLS][DAYS] = {{0}};
    double local_sum2[NCOLS][DAYS] = {{0}};
    double col_times[NCOLS] = {0};

    double sim_start = MPI_Wtime();


    for (int c = 0; c < NCOLS; c++) 
    {
        double t0 = MPI_Wtime();
        run_column_chunk(c, s0, s1, params[c*3 + 0], params[c*3 + 1], params[c*3 + 2],local_sum[c], local_sum2[c]);
        col_times[c] = MPI_Wtime() - t0;
    }

    double sim_time_local = MPI_Wtime() - sim_start;
    double glob_sum [NCOLS][DAYS], glob_sum2[NCOLS][DAYS];
    double max_col_times[NCOLS], sim_time;


    MPI_Reduce(local_sum,  glob_sum,  NCOLS * DAYS, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(local_sum2, glob_sum2, NCOLS * DAYS, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(col_times, max_col_times, NCOLS, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&sim_time_local, &sim_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    double wall_total = MPI_Wtime() - wall;
    if (rank == 0) 
    {

        double all_mn[NCOLS][DAYS], all_lo[NCOLS][DAYS], all_hi[NCOLS][DAYS];

        for (int c = 0; c < NCOLS; c++)

        finalize_stats(glob_sum[c], glob_sum2[c], NUM_SIMS,all_mn[c], all_lo[c], all_hi[c]);
        char label[48]; sprintf(label, "MPI (%d ranks)", size);
        char fname[32]; sprintf(fname, "mpi_%dr", size);
        save_report(fname, label, sim_time, wall_total, NUM_SIMS, size, seq_ref,max_col_times, all_mn, all_lo, all_hi, col_mean, col_std);
    }

    MPI_Finalize();
    return 0;
}
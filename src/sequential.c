#include "common.h"
#include <windows.h>

static double wall_time(void) 
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}

int main(int argc, char *argv[]) 
{   
  
    parse_common_args(argc, argv, NULL, NULL);

    int nrows = 0;
    double *data[NCOLS], col_mean[NCOLS], col_std[NCOLS];

    printf("Sequential | %d sims | %d days x %d steps\n\n", NUM_SIMS, DAYS, STEPS_PER_DAY);

    load_csv(data, col_mean, col_std, &nrows);
    double start[NCOLS], vol[NCOLS], drift[NCOLS];


    for (int c = 0; c < NCOLS; c++) 
    {
        estimate_params(data[c], nrows, &start[c], &vol[c], &drift[c]);
        free(data[c]);
    }

    double all_mn[NCOLS][DAYS], all_lo[NCOLS][DAYS], all_hi[NCOLS][DAYS];
    double col_times[NCOLS];

    double t_start = wall_time();

    for (int c = 0; c < NCOLS; c++) 
    {
        printf("Column %d/%d: %s ... ", c + 1, NCOLS, COL_NAMES[c]);
        fflush(stdout);

        double sum_d[DAYS] = {0}, sum2_d[DAYS] = {0};
        double t0 = wall_time();
        run_column_chunk(c, 0, NUM_SIMS, start[c], vol[c], drift[c], sum_d, sum2_d);
        col_times[c] = wall_time() - t0;

        finalize_stats(sum_d, sum2_d, NUM_SIMS, all_mn[c], all_lo[c], all_hi[c]);
        printf("%.1fs\n", col_times[c]);
    }

    double wall = wall_time() - t_start;
    printf("\nSim: %.2fs  Wall: %.2fs\n\n", wall, wall);

    save_report("sequential", "Sequential (1 core)", wall, wall,NUM_SIMS, 1, 0.0, col_times,all_mn, all_lo, all_hi, col_mean, col_std);

    return 0;
}
#include "common.h"
#include <omp.h>

int main(int argc, char *argv[]) 
{
    int nthreads = 4;
    double seq_ref = 0.0;
    parse_common_args(argc, argv, &nthreads, &seq_ref);
    omp_set_num_threads(nthreads);

    printf("OpenMP | %d threads | %d sims | %d days x %d steps\n", nthreads, NUM_SIMS, DAYS, STEPS_PER_DAY);
    printf("TASK parallelism: 7 columns x 3 dependent stages " "(estimate -> simulate -> finalize) with explicit depend clauses\n\n");

    int nrows = 0;
    double *data[NCOLS], col_mean[NCOLS], col_std[NCOLS];

    if (!load_csv(data, col_mean, col_std, &nrows)) return 1;

    double all_mn[NCOLS][DAYS], all_lo[NCOLS][DAYS], all_hi[NCOLS][DAYS],col_times[NCOLS];

    double params[NCOLS][3];          
    double sums  [NCOLS][DAYS];       
    double sums2 [NCOLS][DAYS];       
    double t_start[NCOLS];            

    double sim_start = omp_get_wtime();


    #pragma omp parallel num_threads(nthreads)
    {
        #pragma omp single
        {
            for (int c = 0; c < NCOLS; c++) 
            {
                #pragma omp task firstprivate(c) \
                        depend(out: params[c])
                {
                    t_start[c] = omp_get_wtime();
                    estimate_params(data[c], nrows, &params[c][0], &params[c][1], &params[c][2]);
                    free(data[c]);
                    data[c] = NULL;
                }

 
                #pragma omp task firstprivate(c) \
                        depend(in:  params[c])   \
                        depend(out: sums[c])
                {
                    for (int d = 0; d < DAYS; d++) { sums[c][d] = 0.0; sums2[c][d] = 0.0; }
                    run_column_chunk(c, 0, NUM_SIMS, params[c][0], params[c][1], params[c][2],sums[c], sums2[c]);
                }

                #pragma omp task firstprivate(c) \
                        depend(in: sums[c])
                {    
                    finalize_stats(sums[c], sums2[c], NUM_SIMS,all_mn[c], all_lo[c], all_hi[c]);
                    col_times[c] = omp_get_wtime() - t_start[c];
                    printf("[Thread %d] %-25s done in %.2fs\n",omp_get_thread_num(), COL_NAMES[c], col_times[c]);
                    fflush(stdout);
                }
            }


            #pragma omp taskwait
        }
    }

    double sim_time = omp_get_wtime() - sim_start;
    printf("\nTotal sim time: %.3fs\n\n", sim_time);

    char label[48]; sprintf(label, "OpenMP (%d threads)", nthreads);
    char fname[32];  sprintf(fname,  "openmp_%dt", nthreads);

    save_report(fname, label, sim_time, sim_time, NUM_SIMS, nthreads, seq_ref,col_times, all_mn, all_lo, all_hi, col_mean, col_std);
    return 0;
}
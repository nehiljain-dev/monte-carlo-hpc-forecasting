/* #include <stdio.h>
#include <omp.h>

int main() {
    //omp_set_num_threads(4);
    #pragma omp parallel num_threads(6)
    {
        int tid = omp_get_thread_num();
        printf("Hello from thread %d\n", tid);
    }
    printf("Total threads used: %d\n", omp_get_max_threads());
    return 0;
} */

/* #include <stdio.h>
#include <omp.h>
#define N 100

int main() {
    int A[N], B[N], C[N];
    for (int i = 0; i < N; i++) { A[i] = i; B[i] = i * 2; }

    #pragma omp parallel for shared(A, B, C)
    for (int i = 0; i < N; i++) {
        C[i] = A[i] + B[i];
    }

    // ✅ Sabhi threads se print karna
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();   // thread ka ID
        printf("Thread %d: C[50] = %d\n", tid, C[50]);
    }

    return 0;
} */

/* #include <stdio.h>
#include <omp.h>
#define N 10

int main() {
    int arr[N]; 
    long long prod = 1;
    for (int i = 0; i < N; i++) arr[i] = i + 1;

    #pragma omp parallel for reduction(*:prod)
    for (int i = 0; i < N; i++)
    {
        prod = prod * arr[i];
    }
    printf("product = %d\n",prod);
    return 0;
} */

/* 
#include <stdio.h>
#include <omp.h>
#define N 10

int main() {
    int arr[N];
    long long prod = 1;
    for (int i = 0; i < N; i++) arr[i] = i + 1;

    #pragma omp parallel for  reduction(*:prod) num_threads(4)
    for (int i = 0; i < N; i++) 
    {
        long long local = arr[i];   // is thread ka value
        int tid = omp_get_thread_num();
        printf("Thread %d → arr[%d] = %d, apna local_prod = %lld\n", tid, i, arr[i], local);

        prod = prod * arr[i];
    }

    printf("\nFinal product = %lld\n", prod);
    return 0;
} */


/* 
#include <stdio.h>
#include <omp.h>

int main() {
    int base = 100;

    #pragma omp parallel firstprivate(base) // Each thread gets base=100
    {
        base += omp_get_thread_num();
        printf("Thread %d: base = %d\n", omp_get_thread_num(), base);
    }
    printf("Outside region, base = %d (unchanged)\n", base);
    return 0;
} */


/* #include <stdio.h>
#include <omp.h>

int shared_var = 100;   // global — shared

int main() {
    int stack_var = 200;   // stack — shared

    #pragma omp parallel  num_threads(2)
    {
        int private_var = 300;   // har thread ki apni — private
        int tid = omp_get_thread_num();

        //printf("Thread %d:\n", tid);
        printf("  shared_var  address = %p = value = %d\n", &shared_var, shared_var);
        printf("  stack_var   address = %p = value = %d\n", &stack_var, stack_var);
        //printf("  private_var address = %p = value = %d\n\n", &private_var, private_var);
    }
    return 0;
} */



/* #include <stdio.h>
#include <omp.h>


int main() {

    #pragma omp parallel  num_threads(6)
    {
       #pragma omp single
    {
        printf("Only one thread prints this\n");
    }
        printf("Only one thread prints  rendommm this\n");
    }
    return 0;
}
 */


/* #include<stdio.h>
#include<omp.h>
#define n 10

int compute(int x)
{
    int z=x*x;
    return z;
} 
int main()

{
    #pragma omp parallel for ordered
    for (int i = 0; i < n; i++) 
    {
        int value = compute(i);

    #pragma omp ordered
    {
    printf("%d -> %d\n", i, value);
    }
}
    return 0;
}


 */



/*  #include <mpi.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int data[] = {10, 20, 30, 40, 50};
    MPI_Status status;

    if (rank == 0) {
        // Sender sends 5 integers
        MPI_Send(data, 5, MPI_INT, 1, 0, MPI_COMM_WORLD);
    } 
    else if (rank == 1) {
        int recv_buf[10]; // buffer larger than what we expect

        // Receiver doesn't know exactly how many elements are coming
        MPI_Recv(recv_buf, 10, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

        // Use MPI_Get_count to find out how many were actually received
        int count;
        MPI_Get_count(&status, MPI_INT, &count);

        printf("Received %d integers\n", count);
        for (int i = 0; i < count; i++)
            printf("  recv_buf[%d] = %d\n", i, recv_buf[i]);
    }

    MPI_Finalize();
    return 0;
} */



/* #include <mpi.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Same program, but each rank works on different data
    int data = rank * 10;  // rank 0 → 0, rank 1 → 10, rank 2 → 20

    printf("Rank %d working on data = %d\n", rank, data);

    MPI_Finalize();
    return 0;
} */


// #include <mpi.h>
// #include <stdio.h>
// #include <unistd.h>  // for sleep()

// int main(int argc, char* argv[]) {
//     MPI_Init(&argc, &argv);

//     int rank;
//     MPI_Comm_rank(MPI_COMM_WORLD, &rank);

//     if (rank == 0) {
//         int data = 42;

//         // Give rank 1 time to post MPI_Recv first
//         sleep(1);

//         // Now safe to call MPI_Rsend
//         MPI_Rsend(&data, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
//         printf("Rank 0: Sent = %d\n", data);
//     }
//     else if (rank == 1) {
//         int recv_data;

//         // Receiver posts MPI_Recv FIRST before rank 0 sends
//         MPI_Recv(&recv_data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//         printf("Rank 1: Received = %d\n", recv_data);
//     }

//     MPI_Finalize();
//     return 0;
// }




#include "common.h"
#include <omp.h>

int main(int argc, char *argv[])
{
    int nthreads = 4;
    double seq_ref = 0.0;
    parse_common_args(argc, argv, &nthreads, &seq_ref);
    omp_set_num_threads(nthreads);

    printf("OpenMP SPLIT SIMS | %d threads | %d sims | %d days x %d steps\n",
            nthreads, NUM_SIMS, DAYS, STEPS_PER_DAY);
    printf("TASK parallelism: 7 columns x (1 estimate + %d sim chunks + 1 finalize)\n\n",
            nthreads);

    int nrows = 0;
    double *data[NCOLS], col_mean[NCOLS], col_std[NCOLS];
    if (!load_csv(data, col_mean, col_std, &nrows)) return 1;

    double all_mn[NCOLS][DAYS], all_lo[NCOLS][DAYS];
    double all_hi[NCOLS][DAYS], col_times[NCOLS];

    double params [NCOLS][3];
    double t_start[NCOLS];

    // ── NEW: partial sums per column per thread chunk ──
    // [col][chunk][day]
    double partial_sums [NCOLS][4][DAYS];
    double partial_sums2[NCOLS][4][DAYS];

    // merged sums after chunks combine
    double merged_sums [NCOLS][DAYS];
    double merged_sums2[NCOLS][DAYS];

    // chunk size — split NUM_SIMS evenly
    int chunk = NUM_SIMS / nthreads;  // 50000 / 4 = 12500

    double sim_start = omp_get_wtime();

    #pragma omp parallel num_threads(nthreads)
    {
        #pragma omp single
        {
            for (int c = 0; c < NCOLS; c++)
            {
                // ── STAGE A: estimate params ──
                // same as before — one task per column
                #pragma omp task firstprivate(c) \
                        depend(out: params[c])
                {
                    t_start[c] = omp_get_wtime();
                    estimate_params(data[c], nrows,
                        &params[c][0],
                        &params[c][1],
                        &params[c][2]);
                    free(data[c]);
                    data[c] = NULL;
                    printf("[Thread %d] Stage A done: %s\n",
                            omp_get_thread_num(), COL_NAMES[c]);
                }

                // ── STAGE B: simulate — NOW SPLIT INTO nthreads TASKS! ──
                for (int t = 0; t < nthreads; t++)
                {
                    int sim_begin = t * chunk;
                    int sim_end   = (t == nthreads - 1)
                                    ? NUM_SIMS          // last chunk gets remainder
                                    : sim_begin + chunk;

                    #pragma omp task firstprivate(c, t, sim_begin, sim_end) \
                            depend(in:  params[c])             \
                            depend(out: partial_sums[c][t])
                    {
                        // initialize this chunk's partial sums
                        for (int d = 0; d < DAYS; d++) {
                            partial_sums [c][t][d] = 0.0;
                            partial_sums2[c][t][d] = 0.0;
                        }

                        // run only THIS chunk of sims
                        run_column_chunk(c,
                            sim_begin, sim_end,
                            params[c][0],
                            params[c][1],
                            params[c][2],
                            partial_sums [c][t],
                            partial_sums2[c][t]);

                        printf("[Thread %d] Stage B col=%d chunk=%d"
                               " sims[%d-%d] done\n",
                                omp_get_thread_num(),
                                c, t, sim_begin, sim_end);
                    }
                }

                // ── STAGE C: merge chunks then finalize ──
                // depends on ALL chunks of this column
                #pragma omp task firstprivate(c) \
                        depend(in:  partial_sums[c][0], \
                                    partial_sums[c][1], \
                                    partial_sums[c][2], \
                                    partial_sums[c][3]) \
                        depend(out: merged_sums[c])
                {
                    // initialize merged arrays
                    for (int d = 0; d < DAYS; d++) {
                        merged_sums [c][d] = 0.0;
                        merged_sums2[c][d] = 0.0;
                    }

                    // merge ALL chunk partial sums together
                    for (int t = 0; t < nthreads; t++) {
                        for (int d = 0; d < DAYS; d++) {
                            merged_sums [c][d] += partial_sums [c][t][d];
                            merged_sums2[c][d] += partial_sums2[c][t][d];
                        }
                    }

                    // finalize stats on merged sums
                    finalize_stats(
                        merged_sums [c],
                        merged_sums2[c],
                        NUM_SIMS,
                        all_mn[c],
                        all_lo[c],
                        all_hi[c]);

                    col_times[c] = omp_get_wtime() - t_start[c];
                    printf("[Thread %d] Stage C done: %-25s %.2fs\n",
                            omp_get_thread_num(),
                            COL_NAMES[c],
                            col_times[c]);
                    fflush(stdout);
                }
            }

            #pragma omp taskwait
        }
    }

    double sim_time = omp_get_wtime() - sim_start;
    printf("\nTotal sim time: %.3fs\n\n", sim_time);

    char label[48];
    sprintf(label, "OpenMP_SplitSims (%d threads)", nthreads);
    char fname[32];
    sprintf(fname, "openmp_splitsims_%dt", nthreads);

    save_report(fname, label, sim_time, sim_time,
                NUM_SIMS, nthreads, seq_ref,
                col_times, all_mn, all_lo, all_hi,
                col_mean, col_std);
    return 0;
}
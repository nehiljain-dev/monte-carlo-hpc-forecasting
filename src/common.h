#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEFAULT_DATA_FILE "C:/Users/Nehil/Desktop/HPC/data/household_power_consumption.txt"
#define DEFAULT_RESULTS   "C:/Users/Nehil/Desktop/HPC/results"

static const char *g_data_file      = DEFAULT_DATA_FILE;
static const char *g_results_folder = DEFAULT_RESULTS;

#define NUM_SIMS       50000
#define DAYS           30
#define STEPS_PER_DAY  1440
#define MAX_ROWS       2200000
#define NCOLS          7

static const char *COL_NAMES[NCOLS] = {
    "Global_active_power", "Global_reactive_power", "Voltage",
    "Global_intensity", "Sub_metering_1", "Sub_metering_2", "Sub_metering_3"};

static const char *COL_UNITS[NCOLS] = {"kW","kVAR","V","A","Wh","Wh","Wh"};

static void parse_common_args(int argc, char *argv[], int *nthreads, double *seq_ref) 
{
    for (int i = 1; i < argc; i++) 
    {
        if (nthreads  && !strcmp(argv[i], "--threads") && i+1 < argc) 
        {
            *nthreads = atoi(argv[++i]);
        } 
        else if (seq_ref && !strcmp(argv[i], "--seqtime") && i+1 < argc) 
        {
            *seq_ref  = atof(argv[++i]);
        }
    }
}

static unsigned int lcg_next(unsigned int seed) 
{
    return seed * 1664525u + 1013904223u;
}

static double gauss(unsigned int *seed) 
{
    *seed = lcg_next(*seed);
    double u1 = (*seed + 0.5) / 4294967296.0;
    *seed = lcg_next(*seed);
    double u2 = (*seed + 0.5) / 4294967296.0;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * 3.14159265358979 * u2);
}

static int load_csv(double *data[], double mean[], double std_arr[] ,int *total_rows) 
{
    FILE *f = fopen(g_data_file, "r");
    char line[2048];

    for (int i = 0; i < NCOLS; i++) 
    {
        data[i] = (double*) malloc(MAX_ROWS * sizeof(double));
    }

    int row = 0;
    while (row < MAX_ROWS && fgets(line, sizeof(line), f)) 
    {
        if (line[0] == '\n') continue;
        strtok(line, ";");
        strtok(NULL, ";");

        for (int c = 0; c < NCOLS; c++) 
        {
            char *tok = strtok(NULL, ";\r\n");
            data[c][row] = atof(tok);
        }
        row++;
    }

    fclose(f);
    *total_rows = row;

    for (int c = 0; c < NCOLS; c++) {
        double sum = 0;
        for (int r = 0; r < row; r++) 
        { 
            sum += data[c][r];
        }

        mean[c] = sum / row;
        double sq = 0;

        for (int r = 0; r < row; r++) 
        {
            double d = data[c][r] - mean[c]; 
            sq += d * d;
        }
        std_arr[c] = sqrt(sq / row);

        if (std_arr[c] == 0) std_arr[c] = 1.0;

        for (int r = 0; r < row; r++) 
        {
            data[c][r] = (data[c][r] - mean[c]) / std_arr[c];
        }
    }
    return 1;
}

static void estimate_params(const double *col, int nrows, double *start, double *vol, double *drift) 
{
    int si =  nrows - 365;
    double sum = 0, sum2 = 0; 
    int cnt = 0;

    for (int i = si; i < nrows; i++) 
    {
        sum  += col[i];
        sum2 += col[i] * col[i];
        cnt++;
    }
    double m = sum / cnt;
    *start = m;
    *vol   = sqrt(fmax(0.0, sum2 / cnt - m * m));
    *drift = (col[nrows-1] - col[si]) / (double)(nrows - si);
}

static void run_column_chunk(int c, int sim_begin, int sim_end,double start, double vol, double drift, double sum_d[DAYS], double sum2_d[DAYS])
{
    for (int sim = sim_begin; sim < sim_end; sim++)
    {
        unsigned int seed = (unsigned int)((long long)c * NUM_SIMS + sim + 1);
        double val = start;
        for (int day = 0; day < DAYS; day++)
        {
            double day_sum = 0.0;
            for (int step = 0; step < STEPS_PER_DAY; step++) 
            {
                double z    = gauss(&seed);
                double hour = step / 60.0;
                double m    = (hour - 8.0)  / 2.0;
                double e    = (hour - 18.0) / 2.0;
                double pattern = 1.0 + 0.3 * exp(-0.5 * m * m)+ 0.4 * exp(-0.5 * e * e);
                val += drift / STEPS_PER_DAY + z * vol * pattern * 0.01;
                day_sum += val;
            }
            double day_avg   = day_sum / STEPS_PER_DAY;
            sum_d[day]      += day_avg;
            sum2_d[day]     += day_avg * day_avg;
        }
    }
}

static void finalize_stats(const double sum_d[DAYS], const double sum2_d[DAYS],int nsims, double mn[DAYS], double lo[DAYS], double hi[DAYS]) 
{
    for (int d = 0; d < DAYS; d++) 
    {
        mn[d] = sum_d[d] / nsims;
        double var = sum2_d[d] / nsims - mn[d] * mn[d];
        if (var < 0) { var = 0; }
        double sd = sqrt(var);
        lo[d] = mn[d] - 1.96 * sd;
        hi[d] = mn[d] + 1.96 * sd;
    }
}

static void save_report(const char *fname, const char *label,double sim_t, double wall_t,int nsims,
                        int cores, double seq_t,double 
                        col_times[],double mn[NCOLS][DAYS],
                        double lo[NCOLS][DAYS],
                        double hi[NCOLS][DAYS],
                        double orig_mean[], double orig_std[]) 
{


    char path[512];
    sprintf(path, "%s/%s_report.txt", g_results_folder, fname);

    FILE *f = fopen(path, "w");

    if (!f) { fprintf(stderr, "Cannot write report: %s\n", path); return; }
    double speedup = (seq_t > 0) ? seq_t / sim_t : 1.0;
    double effic   = (speedup / cores) * 100.0;
    double tp  = (double)nsims * NCOLS / sim_t;

    double rm[NCOLS][DAYS], rl[NCOLS][DAYS], rh[NCOLS][DAYS];

    for (int c = 0; c < NCOLS; c++) 
    {
        for (int d = 0; d < DAYS; d++) 
        {
            rm[c][d] = mn[c][d] * orig_std[c] + orig_mean[c];
            rl[c][d] = lo[c][d] * orig_std[c] + orig_mean[c];
            rh[c][d] = hi[c][d] * orig_std[c] + orig_mean[c];
            
            if (c != 1) 
            {
                if (rm[c][d] < 0) rm[c][d] = 0;
                if (rl[c][d] < 0) rl[c][d] = 0;
                if (rh[c][d] < 0) rh[c][d] = 0;
            }
        }
    }

    fprintf(f, "===== HPC PROJECT REPORT =====\n");
    fprintf(f, "Version    : %s\n", label);
    fprintf(f, "Sims/col   : %d\n", nsims);
    fprintf(f, "Days       : %d\n\n", DAYS);
    fprintf(f, "Sim Time   : %.3f sec  (parallel computation only)\n", sim_t);
    fprintf(f, "Wall Time  : %.3f sec  (includes I/O and communication)\n", wall_t);
    fprintf(f, "Speedup    : %.3f  (Sn = T1 / Tn)\n", speedup);
    fprintf(f, "Efficiency : %.2f%%  (En = Sn / n)\n", effic);
    fprintf(f, "Throughput : %.1f sims/s\n\n", tp);
    fprintf(f, "%-25s %6s %10s %10s %10s %12s\n","Column", "Unit", "Day-1", "Day-15", "Day-30", "ColTime(s)");

    fprintf(f, "%s\n", "------------------------------------------------------------------------");

    for (int c = 0; c < NCOLS; c++) 
    {
        fprintf(f, "%-25s %6s %10.4f %10.4f %10.4f %12.4f\n",COL_NAMES[c], COL_UNITS[c], rm[c][0], rm[c][14], rm[c][29], col_times[c]);
        fprintf(f, "95%% CI day-30: [%.4f , %.4f]\n", rl[c][29], rh[c][29]);
    }
    fclose(f);

    char csv_path[512];

    sprintf(csv_path, "%s/performance_summary.csv", g_results_folder);

    FILE *csv = fopen(csv_path, "a+");

    if (csv) 
    {
        fseek(csv, 0, SEEK_END);
        if (ftell(csv) == 0)
            fprintf(csv,  "version,label,cores,nsims," "sim_time,wall_time,speedup,efficiency,throughput\n");
            fprintf(csv, "%s,\"%s\",%d,%d,%.6f,%.6f,%.6f,%.4f,%.2f\n",fname, label, cores, nsims, sim_t, wall_t, speedup, effic, tp);
            fclose(csv);
    }
}

#endif
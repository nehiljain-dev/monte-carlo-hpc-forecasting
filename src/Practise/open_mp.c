#include<stdio.h>
#include<omp.h>

int main()
{
    #pragma omp parallel num_threads(8)
{
    #pragma omp single
    {
        for (int c = 0; c < 8; c++) {
            #pragma omp task firstprivate(c)
            {
                printf("Task for column %d executed by thread %d\n",
                       c, omp_get_thread_num());
            }
        }
    }
}
    return 0;
}



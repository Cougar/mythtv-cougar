#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
  
#include "jitterometer.h"

Jitterometer::Jitterometer(char *nname, int ncycles)
{
  times = (unsigned *)malloc(ncycles * sizeof(unsigned));
  
  count = 0;
  starttime_valid = 0;
  num_cycles = ncycles;
  name = strdup(nname);
}

Jitterometer::~Jitterometer()
{
  free(times);
  free(name);
}

void Jitterometer::RecordCycleTime()
{
  RecordEndTime();
  RecordStartTime();
}

void Jitterometer::RecordEndTime()
{
  struct timeval timenow;

  gettimeofday(&timenow, NULL);

  if(starttime_valid)
    {
      times[count] =
        (timenow.tv_sec  - starttime.tv_sec ) * 1000000 +
        (timenow.tv_usec - starttime.tv_usec) ;

      //printf("recorded timediff '%d'\n", times[count]);

      count++;
    }

  if(count==num_cycles)
    {
      /* compute and display stuff, reset count to -1  */

      double mean = 0, sum_of_squared_deviations=0;
      double standard_deviation;
      double fps = 0, tottime = 0;
      int i;

      /* compute the mean */
      for(i=0; i<num_cycles; i++)
        {
          mean += times[i];
        }
      tottime = mean;
      mean /= num_cycles;

      fps = num_cycles / tottime * 1000000;
          
      /* compute the sum of the squares of each deviation from the mean */
      for(i=0; i<num_cycles;i++)
        {
          sum_of_squared_deviations += (mean - times[i]) * (mean - times[i]);
        }

      /* compute standard deviation */
      standard_deviation = sqrt(sum_of_squared_deviations / (num_cycles - 1));

      printf("'%s' mean = '%f', std. dev. = '%f', fps = '%f'\n", name, mean, standard_deviation, fps);

      count = 0;
    }
  starttime_valid = 0;
}

void Jitterometer::RecordStartTime()
{
  gettimeofday(&starttime, NULL);
  starttime_valid = 1;
}

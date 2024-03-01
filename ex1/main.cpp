

#include <cstdio>
#include "osm.h"
#include "iostream"

#define ITER 1000000



int main ()
{


  double x1 = osm_operation_time(ITER);
  double x2 = osm_function_time( ITER);
  double x3 = osm_syscall_time(ITER);


  printf("osm_operation_time = %lf\n",x1);
  printf("osm_function_time = %lf\n",x2);
  printf("osm_syscall_time = %lf\n", x3);




}

#ifndef _OSM_H
#define _OSM_H

#include <sys/time.h>
#include "osm.h"
#include <cmath>
#include "stdarg.h"
#include "stdlib.h"
#include "iostream"
#include "stdint.h"


/* calling a system call that does nothing */
#define OSM_NULLSYSCALL asm volatile( "int $0x80 " : : \
        "a" (0xffffffff) /* no such syscall */, "b" (0), "c" (0), "d" (0) /*:\
        "eax", "ebx", "ecx", "edx"*/)

#define FACTOR 7


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations){

  int result = 0;

  if (!iterations){
	  return -1;
	}

  struct timeval start_time, finish_time;


  if(gettimeofday(&start_time, NULL) == -1){
	  return -1;
	}

  for (size_t counter = 0; counter < iterations ; counter+=FACTOR)
	{
	  result +=1;
	  result +=1;
	  result +=1;
	  result +=1;
	  result +=1;
	  result +=1;
	  result +=1;
	}

  if(gettimeofday(&finish_time, NULL) == -1){
	  return -1;
	}

  return ((finish_time.tv_usec - start_time.tv_usec) * pow(10, 3))
		 / iterations;
}



void empty_function (){}

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations){

  unsigned int counter = 0;

  if (!iterations){
	  return -1;
	}

  struct timeval start_time, finish_time;

  if(gettimeofday(&start_time, NULL) == -1){
	  return -1;
	}

  while (counter < iterations){
	  empty_function();
	  empty_function();
	  empty_function();
	  empty_function();
	  empty_function();
	  empty_function();
	  empty_function();
	  counter += FACTOR;
	}

  if(gettimeofday(&finish_time, NULL) == -1){
	  return -1;
	}

  return ((finish_time.tv_usec - start_time.tv_usec) * pow(10, 3))
		 / iterations;

}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations){

  unsigned int counter = 0;

  if (!iterations){
	  return -1;
	}

  struct timeval start_time, finish_time;

  if(gettimeofday(&start_time, NULL) == -1){
	  return -1;
	}

  while (counter < iterations){
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  OSM_NULLSYSCALL;
	  counter += FACTOR;
	}

  if(gettimeofday(&finish_time, NULL) == -1){
	  return -1;
	}

  //printf("finish_time : w");

  return ((finish_time.tv_usec - start_time.tv_usec) * pow(10, 3))
		 / iterations;
}



#endif





















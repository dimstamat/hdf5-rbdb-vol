#include <stdio.h>
#include <time.h>
#include <stdarg.h> 

#include "H5DimLog.h"


static char* asctime_no_newline(const struct tm *timeptr)
{
  static const char wday_name[][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };
  static const char mon_name[][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  static char result[26];
  sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d",
    wday_name[timeptr->tm_wday],
    mon_name[timeptr->tm_mon],
    timeptr->tm_mday, timeptr->tm_hour,
    timeptr->tm_min, timeptr->tm_sec,
    1900 + timeptr->tm_year);
  return result;
}


static void H5DimLogWrite(const char* log_name, const char* type, const char* file, const char* func, const int line, const char* format, va_list args){
	FILE* fp;
	char* time_str;
	time_t rawtime;
        struct tm * timeinfo;
        char log_path [1024];
        memset(log_path, 0, 1024);
        strcpy(log_path, LOG_PATH);
        strcat(log_path, "/");
        strcat(log_path, log_name);
        
        fp = fopen(log_path, "a");
	if (fp == NULL){
		fprintf(stderr, "Error while opening file %s\n", log_path);
	}
  	time (&rawtime);
	timeinfo = localtime ( &rawtime );
	time_str =  asctime_no_newline (timeinfo);
        char format_all[1024];
        memset(format_all, 0, 1024);
        strcpy(format_all, "%s@%s:\t%s:\t%s: %d:\t");
        strcat(format_all, format);
        strcat(format_all, "\n");
        
        fprintf(fp, format_all, type, time_str, file, func, line, args);
	if (fclose(fp) != 0){
		fprintf(stderr, "Error while closing file %s\n", log_path);
	}
}

void H5DimLog_info(const char* log_name, const char* file, const char * func, const int line, const char* message){
        H5DimLogWrite(log_name, "INFO", file, func, line, "%s", message);
}

void H5DimLog_debug(const char* log_name, const char* file, const char * func, const int line, const char* message){
        H5DimLogWrite(log_name, "DEBUG", file, func, line, "%s", message);
}

void H5DimLog_info_format(const char* log_name, const char* file, const char * func, const int line, const char* format, ...){
        va_list args;
        va_start(args, format);
	H5DimLogWrite(log_name, "INFO", file, func, line, format, args);
        va_end(args);
}

void H5DimLog_debug_format(const char* log_name, const char* file, const char * func, const int line, const char* format, ...){
        va_list args;
        va_start(args, format);
	H5DimLogWrite(log_name, "DEBUG", file, func, line, format, args);
        va_end(args);
}


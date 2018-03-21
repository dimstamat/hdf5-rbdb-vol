#define LOG_LEVEL 0

/* 	Log levels	|
************************|
*	0: no output	|
*	1: info		|
*	2: debug	|
************************|			
*/

#define LOG_PATH "/home/dimos/Dropbox/phd/projects/hdf5/hdf5-vol"
#define LOG_NAME_DEFAULT "h5DimLog.log"

/*void H5DimLog_info(const char* file, const char * func, const int line, const char* message);

void H5DimLog_debug(const char* file, const char * func, const int line, const char* message);

void H5DimLog_info_format(const char* file, const char * func, const int line, const char* format, ...);

void H5DimLog_debug_format(const char* file, const char * func, const int line, const char* format, ...);

static void H5DimLogWrite(const char* type, const char* file, const char* func, const int line, const char* format, va_list args);

*/

#define H5DIMLOG_INFO(log_name, message)  H5DimLog_info(log_name, __FILE__, __func__, __LINE__, message);
#define H5DIMLOG_INFO_FORMAT(log_name, format, args...)  H5DimLog_info_format(log_name, __FILE__, __func__, __LINE__, format, args);
#define H5DIMLOG_DEBUG(log_name, message)  H5DimLog_debug(log_name, __FILE__, __func__, __LINE__, message);
#define H5DIMLOG_DEBUG_FORMAT(log_name, format, args...)  H5DimLog_debug_format(log_name, __FILE__, __func__, __LINE__, format, args);
#ifndef __TEST_FSPERF_FUNCS_H__
#define __TEST_FSPERF_FUNCS_H__

#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-result.h>
#include <utime.h>
#include <sys/time.h>

#define _(String) (String)
#define CR(result)       {int __r=(result); if (__r<0) return __r;}
#define CRU(result,file) {int __r=(result); if (__r<0) {gp_file_unref(file);return __r;}}
#define CL(result,list)  {int __r=(result); if (__r<0) {gp_list_free(list); return __r;}}

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); return (1);}}

struct timeval time_now(void);
float time_since (const struct timeval start);

int print_file_info (CameraFileInfo *info);
int wait_and_save_new_file (Camera *camera, GPContext *context, long waittime, CameraEventType *type, CameraFilePath	*path, int download);

#endif // __TEST_FSPERF_FUNCS_H__
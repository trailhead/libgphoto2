#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-result.h>
#include <utime.h>
#include <sys/time.h>

#include "test-fsperf-funcs.h"

struct timeval
time_now(void) {
	struct timeval curtime;
	gettimeofday (&curtime, NULL);
	return curtime;
}

float
time_since (const struct timeval start) {
	struct timeval curtime = time_now();
	return (((curtime.tv_sec - start.tv_sec)*1000)+((curtime.tv_usec - start.tv_usec)/1000))/1000.0f;
}

int
print_file_info (CameraFileInfo *info)
{
	if (info->file.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info->file.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:   '%s'\n"), info->file.type);
		if (info->file.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:        %lu byte(s)\n"), (unsigned long int)info->file.size);
		if (info->file.fields & GP_FILE_INFO_WIDTH)
			printf (_("  Width:       %i pixel(s)\n"), info->file.width);
		if (info->file.fields & GP_FILE_INFO_HEIGHT)
			printf (_("  Height:      %i pixel(s)\n"), info->file.height);
		if (info->file.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded:  %s\n"),
				(info->file.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
		if (info->file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf (_("  Permissions: "));
			if ((info->file.permissions & GP_FILE_PERM_READ) &&
			    (info->file.permissions & GP_FILE_PERM_DELETE))
				printf (_("read/delete"));
			else if (info->file.permissions & GP_FILE_PERM_READ)
				printf (_("read"));
			else if (info->file.permissions & GP_FILE_PERM_DELETE)
				printf (_("delete"));
			else
				printf (_("none"));
			putchar ('\n');
		}
		if (info->file.fields & GP_FILE_INFO_MTIME)
			printf (_("  Time:        %s"),
				asctime (localtime (&info->file.mtime)));
	}
	return (GP_OK);
}

int
wait_and_save_new_file (Camera *camera, GPContext *context, long waittime, CameraEventType *type,	CameraFilePath	*newpath, int download) {
	int 		result;
	CameraEventType	evtype;
	void		*data;
  CameraFilePath	*path;

	if (!type) type = &evtype;

  do {
    evtype = GP_EVENT_UNKNOWN;
    data = NULL;
    result = gp_camera_wait_for_event(camera, waittime, type, &data, context);
    if (result == GP_ERROR_NOT_SUPPORTED) {
      *type = GP_EVENT_TIMEOUT;
      printf("Returning not supported.\n");
      usleep(waittime*1000);
      return GP_OK;
    }
    if (result != GP_OK) {
      printf ("Returning error.\n");
      return result;
    }
    path = data;
    switch (*type) {
    case GP_EVENT_TIMEOUT:
      printf ("Event timeout.\n");
      break;
    case GP_EVENT_CAPTURE_COMPLETE:
      printf ("Event CAPTURE_COMPLETE during wait.\n");
      break;
    case GP_EVENT_FOLDER_ADDED:
      printf ("Event FOLDER_ADDED %s/%s during wait, ignoring.\n", path->folder, path->name);
      free (data);
      break;
    case GP_EVENT_FILE_CHANGED:
      printf ("Event FILE_CHANGED %s/%s during wait, ignoring.\n", path->folder, path->name);
      free (data);
      break;
    case GP_EVENT_FILE_ADDED:
      printf ("Event FILE_ADDED %s/%s during wait.\n", path->folder, path->name);
			memcpy (newpath, path, sizeof(CameraFilePath));
      //result = save_captured_file (path, download);
      result = GP_OK;
      free (data);
      /* result will fall through to final return */
      break;
    case GP_EVENT_UNKNOWN:
      printf ("Unknown event type %d during wait, ignoring.\n", *type);
      free (data);
      break;
    default:
        printf ("Unknown event type %d during wait, ignoring.\n", *type);
      break;
    }
  } while(*type != GP_EVENT_FILE_ADDED && *type != GP_EVENT_TIMEOUT);
  
	return result;
}
/* test-gphoto2.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <stdio.h>
#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif


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

int print_file_info (CameraFileInfo *info);
int wait_and_save_new_file (Camera *camera, GPContext *context, long waittime, CameraEventType *type, CameraFilePath	*path, int download);

static struct timeval
time_now() {
	struct timeval curtime;
	gettimeofday (&curtime, NULL);
	return curtime;
}

static float
time_since (const struct timeval start) {
	struct timeval curtime = time_now();
	return (((curtime.tv_sec - start.tv_sec)*1000)+((curtime.tv_usec - start.tv_usec)/1000))/1000.0f;
}

int
main (int argc, char ** argv)
{
	CameraText text;
	Camera *camera;
  CameraFile *camera_file;
	CameraAbilitiesList *al;
	CameraAbilities abilities;
	int m;
	GPContext *context;

#ifdef HAVE_MCHECK_H
	mtrace();
#endif

  // Disable output buffering for stdout
  setbuf(stdout, NULL);

  if (argc == 2 && !strcmp(argv[1], "-o")) {
    printf("Setting optimize to TRUE\n");
    gp_camera_set_filesys_optimize(TRUE);
    printf("Optimize set to %s\n", gp_camera_get_filesys_optimize() ? "TRUE" : "FALSE");
  } else {
    printf("Running in default mode without optimization\n");
  }

	/*
	 * You'll probably want to access your camera. You will first have
	 * to create a camera (that is, allocating the memory).
	 */
	printf ("Creating camera...\n");
	CHECK (gp_camera_new (&camera));
  context = gp_context_new();
	/*
	 * Before you initialize the camera, set the model so that
	 * gphoto2 knows which library to use.
	 */

	/*
	 * Now, initialize the camera (establish a connection).
	 */
	printf ("Initializing camera...\n");
	CHECK (gp_camera_init (camera, context));

  int result;
  CameraFilePath path;
  char *pathsep;
  const char *data;
  unsigned long filesize;

  struct timeval trig_start = time_now();

	printf("Triggering capture... ");

  result =  gp_camera_trigger_capture (camera, context);

  printf("%f\n", time_since(trig_start));
	if (result != GP_OK) {
		printf("Could not trigger capture.");
	} else {

    CameraEventType *type;
    struct timeval wait_start = time_now();
  	printf("Waiting for file added...\n");
    result = wait_and_save_new_file (camera, context, 5000, type, &path, 0);
    printf("Waited for %f\n", time_since(wait_start));

    if (result == GP_OK) {
      if (strcmp(path.folder, "/") == 0)
        pathsep = "";
      else
        pathsep = "/";

      printf("New file is in location %s%s%s on the camera\n",
        path.folder, pathsep, path.name);
      
	    gp_file_new (&camera_file);

      struct timeval info_start = time_now();
      printf("Getting GP_FILE_TYPE_NORMAL into local memory... ");
      result = gp_camera_file_get (camera, path.folder, path.name, GP_FILE_TYPE_NORMAL, camera_file, context);
      printf("%f\n", time_since(info_start));

      if (result != GP_OK) {
        printf("Error from gp_camera_file_get()\n");
      }

      CRU (gp_file_get_data_and_size (camera_file, &data, &filesize), camera_file);

      printf("File of %ld bytes retrieved into local memory.\n", filesize);

  		gp_file_unref (camera_file);

      struct timeval delete_start = time_now();
      printf("Delete file... ");
      result = gp_camera_file_delete(camera, path.folder, path.name, context);
      printf("%f\n", time_since(delete_start));

      if (result != GP_OK) {
        printf("Error from gp_camera_file_delete()\n");
      }

    }
  }

	gp_camera_exit(camera, context);

#ifdef HAVE_MCHECK_H
	muntrace();
#endif

	return (0);
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
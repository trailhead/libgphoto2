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

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-result.h>
#include <utime.h>
#include <sys/time.h>

#include "test-fsperf-funcs.h"

int
main (int argc, char ** argv)
{
	Camera *camera;
	GPContext *context;
  CameraFilePath path;
  CameraEventType *type = NULL;
  int result;
  char *pathsep;

  // Disable output buffering for stdout
  setbuf(stdout, NULL);

	/*
	 * You'll probably want to access your camera. You will first have
	 * to create a camera (that is, allocating the memory).
	 */
	printf ("Creating camera...\n");
	CHECK (gp_camera_new (&camera));
  context = gp_context_new();

	/*
	 * Now, initialize the camera (establish a connection).
	 */
	printf ("Initializing camera...\n");
	CHECK (gp_camera_init (camera, context));

  struct timeval trig_start = time_now();

	printf("Triggering capture... ");

  result =  gp_camera_trigger_capture (camera, context);

  printf("%f\n", time_since(trig_start));
	if (result != GP_OK) {
		printf("Could not trigger capture.");
	} else {

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
      
      struct timeval info_start = time_now();
      printf("Getting file info... ");
      CameraFileInfo info;
      CR (gp_camera_file_get_info (camera, path.folder, path.name, &info,
                context));
      printf("%f\n", time_since(info_start));
      print_file_info (&info);
      
      struct timeval delete_start = time_now();
      printf("Delete file... ");
      gp_camera_file_delete(camera, path.folder, path.name, context);
      printf("%f\n", time_since(delete_start));
    }
  }

	gp_camera_exit(camera, context);

	return (0);
}

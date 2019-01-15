#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-result.h>
#include <utime.h>
#include <sys/time.h>

#include "test-fsperf-funcs.h"

int
main (int argc, char ** argv)
{
	Camera *camera;
	GPContext *context;
  CameraFile *camera_file;
  CameraFilePath path;
  CameraEventType *type = NULL;
  int result;
  char *pathsep;
  const char *data;
  unsigned long filesize;

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

	return (0);
}
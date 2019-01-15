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
	CameraText text;
	Camera *camera;
	GPContext *context;
	CameraAbilitiesList *al;
	CameraAbilities abilities;
  CameraFile *camera_file;
  CameraFilePath path;
	int m;
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

	printf("Triggering capture preview... ");

  gp_file_new (&camera_file);

  result =  gp_camera_capture_preview(camera, camera_file, context);

  printf("%f\n", time_since(trig_start));
	if (result != GP_OK) {
		printf("Could not trigger capture preview.");
	}

  CRU (gp_file_get_data_and_size (camera_file, &data, &filesize), camera_file);

  printf("File of %ld bytes retrieved into local memory.\n", filesize);

  printf("Saving locally as preview.jpg.\n");
  result = gp_file_save(camera_file, "preview.jpg");

  gp_file_unref(camera_file);

	gp_camera_exit(camera, context);

	return (0);
}

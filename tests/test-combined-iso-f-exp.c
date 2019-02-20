#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <sys/time.h>

int
main (int argc, char ** argv)
{
	Camera *camera;
	GPContext *context;
  CameraFilePath path;
  int result;
  char *pathsep;

  // Disable output buffering for stdout
  setbuf(stdout, NULL);

	/*
	 * You'll probably want to access your camera. You will first have
	 * to create a camera (that is, allocating the memory).
	 */
	printf ("Creating camera...\n");
	gp_camera_new (&camera);
  context = gp_context_new();

	/*
	 * Now, initialize the camera (establish a connection).
	 */
	printf ("Initializing camera...\n");
	gp_camera_init (camera, context);

	

	gp_camera_exit(camera, context);

	return (0);
}
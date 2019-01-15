#include "config.h"

#include <stdio.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <sys/time.h>

#include "test-fsperf-funcs.h"

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
	CHECK (gp_camera_new (&camera));
  context = gp_context_new();

	/*
	 * Now, initialize the camera (establish a connection).
	 */
	printf ("Initializing camera...\n");
	CHECK (gp_camera_init (camera, context));

  struct timeval cap_start = time_now();

	printf("Capturing image... ");
  result = gp_camera_capture (camera, GP_CAPTURE_IMAGE, &path, context);
  printf("%f\n", time_since(cap_start));
  
  if (result != GP_OK) {
    printf("Could not capture image.\n");
  } else {
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

	gp_camera_exit(camera, context);

	return (0);
}
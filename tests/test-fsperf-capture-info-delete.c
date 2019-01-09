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


#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-file.h>
#include <sys/time.h>

#define _(String) (String)
#define CR(result)       {int __r=(result); if (__r<0) return __r;}
#define CRU(result,file) {int __r=(result); if (__r<0) {gp_file_unref(file);return __r;}}
#define CL(result,list)  {int __r=(result); if (__r<0) {gp_list_free(list); return __r;}}

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); return (1);}}

int print_file_info (CameraFileInfo *info);

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
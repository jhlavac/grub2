/* search.c - search devices based on a file or a filesystem label */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/device.h>
#include <grub/file.h>
#include <grub/env.h>
#include <grub/extcmd.h>

static const struct grub_arg_option options[] =
  {
    {"file",		'f', 0, "search devices by a file (default)", 0, 0},
    {"label",		'l', 0, "search devices by a filesystem label", 0, 0},
    {"fs-uuid",		'u', 0, "search devices by a filesystem UUID", 0, 0},
    {"set",		's', GRUB_ARG_OPTION_OPTIONAL, "set a variable to the first device found", "VAR", ARG_TYPE_STRING},
    {"no-floppy",	'n', 0, "do not probe any floppy drive", 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

enum options
  {
    SEARCH_FILE,
    SEARCH_LABEL,
    SEARCH_FS_UUID,
    SEARCH_SET,
    SEARCH_NO_FLOPPY,
 };

static void
search_fs (const char *key, const char *var, int no_floppy, int is_uuid)
{
  int count = 0;
  auto int iterate_device (const char *name);

  int iterate_device (const char *name)
    {
      grub_device_t dev;
      int abort = 0;

      /* Skip floppy drives when requested.  */
      if (no_floppy &&
	  name[0] == 'f' && name[1] == 'd' &&
	  name[2] >= '0' && name[2] <= '9')
	return 0;

      dev = grub_device_open (name);
      if (dev)
	{
	  grub_fs_t fs;

	  fs = grub_fs_probe (dev);

#define QUID(x)	(is_uuid ? (x)->uuid : (x)->label)

	  if (fs && QUID(fs))
	    {
	      char *quid;

	      (QUID(fs)) (dev, &quid);
	      if (grub_errno == GRUB_ERR_NONE && quid)
		{
		  if (grub_strcmp (quid, key) == 0)
		    {
		      /* Found!  */
		      count++;
		      if (var)
			{
			  grub_env_set (var, name);
			  abort = 1;
			}
		      else
			  grub_printf (" %s", name);
		    }

		  grub_free (quid);
		}
	    }

	  grub_device_close (dev);
	}

      grub_errno = GRUB_ERR_NONE;
      return abort;
    }

  grub_device_iterate (iterate_device);

  if (count == 0)
    grub_error (GRUB_ERR_FILE_NOT_FOUND, "no such device: %s", key);
}

static void
search_file (const char *key, const char *var, int no_floppy)
{
  int count = 0;
  char *buf = 0;
  auto int iterate_device (const char *name);

  int iterate_device (const char *name)
    {
      grub_size_t len;
      char *p;
      grub_file_t file;
      int abort = 0;

      /* Skip floppy drives when requested.  */
      if (no_floppy &&
	  name[0] == 'f' && name[1] == 'd' &&
	  name[2] >= '0' && name[2] <= '9')
	return 0;

      len = grub_strlen (name) + 2 + grub_strlen (key) + 1;
      p = grub_realloc (buf, len);
      if (! p)
	return 1;

      buf = p;
      grub_sprintf (buf, "(%s)%s", name, key);

      file = grub_file_open (buf);
      if (file)
	{
	  /* Found!  */
	  count++;
	  if (var)
	    {
	      grub_env_set (var, name);
	      abort = 1;
	    }
	  else
	    grub_printf (" %s", name);

	  grub_file_close (file);
	}

      grub_errno = GRUB_ERR_NONE;
      return abort;
    }

  grub_device_iterate (iterate_device);

  grub_free (buf);

  if (grub_errno == GRUB_ERR_NONE && count == 0)
    grub_error (GRUB_ERR_FILE_NOT_FOUND, "no such file: %s", key);
}

static grub_err_t
grub_cmd_search (grub_extcmd_t cmd, int argc, char **args)
{
  struct grub_arg_list *state = cmd->state;
  const char *var = 0;

  if (argc == 0)
    return grub_error (GRUB_ERR_INVALID_COMMAND, "no argument specified");

  if (state[SEARCH_SET].set)
    var = state[SEARCH_SET].arg ? state[SEARCH_SET].arg : "root";

  if (state[SEARCH_LABEL].set)
    search_fs (args[0], var, state[SEARCH_NO_FLOPPY].set, 0);
  else if (state[SEARCH_FS_UUID].set)
    search_fs (args[0], var, state[SEARCH_NO_FLOPPY].set, 1);
  else if (state[SEARCH_FILE].set)
    search_file (args[0], var, state[SEARCH_NO_FLOPPY].set);
  else
    return grub_error (GRUB_ERR_INVALID_COMMAND, "unspecified search type");

  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(search)
{
  cmd =
    grub_register_extcmd ("search", grub_cmd_search,
			  GRUB_COMMAND_FLAG_BOTH,
			  "search [-f|-l|-u|-s|-n] NAME",
			  "Search devices by file, filesystem label or filesystem UUID."
			  " If --set is specified, the first device found is"
			  " set to a variable. If no variable name is"
			  " specified, \"root\" is used.",
			  options);
}

GRUB_MOD_FINI(search)
{
  grub_unregister_extcmd (cmd);
}

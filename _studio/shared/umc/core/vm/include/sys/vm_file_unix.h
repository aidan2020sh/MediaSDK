/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/
/*
 * VM 64-bits buffered file operations library
 *    Linux and OSX definitions
 */
#ifndef VM_FILE_UNIX_H
#  define VM_FILE_UNIX_H
#  include <stdio.h>
#  include <stdarg.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <dirent.h>

# ifdef __APPLE__
#  define OSX
# endif
# define MAX_PATH 260

typedef FILE vm_file;
typedef DIR vm_dir;

# define vm_stderr  stderr
# define vm_stdout  stdout
# define vm_stdin    stdin
/*
 * file access functions
 */
# if defined(__ANDROID__) || defined(OSX) || defined(LINUX64)
/* native fopen is 64-bits on OSX */
#  define vm_file_fopen    fopen
# else
#  define vm_file_fopen    fopen64
# endif

# define vm_file_fclose     fclose
# define vm_file_feof         feof
# define vm_file_remove  remove

/*
 * binary file IO */
# define vm_file_fread    fread
# define vm_file_fwrite    fwrite

/*
 * character (string) file IO */
# define vm_file_fgets      fgets
# define vm_file_fputs      fputs
# define vm_file_fscanf     fscanf
# define vm_file_fprintf    fprintf
#if !defined(LINUX64)
# define vm_file_vfprintf   vfprintf
#endif /* #if !defined(LINUX64) */

/* temporary file support */
# define vm_file_tmpfile      tmpfile
# define vm_file_tmpnam       tmpnam
# define vm_file_tmpnam_r     tmpnam_r
# define vm_file_tempnam      tempnam

#endif //VM_FILE_UNIX_H


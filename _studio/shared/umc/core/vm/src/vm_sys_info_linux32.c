//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#if defined(LINUX32) || defined(__APPLE__)

#include "vm_sys_info.h"
#include <time.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/syslimits.h>
/* to access to sysctl function */
#include <sys/types.h>
#include <sys/sysctl.h>

/*
 * retrieve information about integer parameter
 * defined as CTL_... (ctl_class) . ENTRY (ctl_entry)
 * return value in res.
 *  status : 1  - OK
 *           0 - operation failed
 */
uint32_t osx_sysctl_entry_32u( int ctl_class, int ctl_entry, uint32_t *res ) {
  int dcb[2];
  size_t i;
   dcb[0] = ctl_class;  dcb[1] = ctl_entry;
   i = sizeof(res[0]);
  return (sysctl(dcb, 2, res, &i, NULL, 0) != -1) ? 1 : 0;
}

#else
#include <sys/sysinfo.h>

#endif /* __APPLE__ */

uint32_t vm_sys_info_get_cpu_num(void)
{
#if defined(__APPLE__)
    uint32_t cpu_num = 1;
    return (osx_sysctl_entry_32u(CTL_HW, HW_NCPU, &cpu_num)) ? cpu_num : 1;
#elif defined(ANDROID)
    return sysconf(_SC_NPROCESSORS_ONLN); /* on Android *_CONF will return number of _real_ processors */
#else
    return sysconf(_SC_NPROCESSORS_CONF); /* on Linux *_CONF will return number of _logical_ processors */
#endif
} /* uint32_t vm_sys_info_get_cpu_num(void) */

void vm_sys_info_get_cpu_name(vm_char *cpu_name)
{
#ifdef __APPLE__
  size_t pv;
  char s[128], k[128];
  pv = 128;
  if (sysctlbyname( "machdep.cpu.vendor", s, &pv, NULL, 0 ) == -1)
    vm_string_strcpy( s, (vm_char *)"Problem determine vendor" );
  pv = 128;
  if (sysctlbyname( "machdep.cpu.brand_string", k, &pv, NULL, 0 ) == -1)
    vm_string_strcpy( k, (vm_char *)"Problem determine brand string" );
  vm_string_sprintf( cpu_name, VM_STRING("%s %s"), k, s );
#else
    FILE *pFile = NULL;
    vm_char buf[_MAX_LEN];
    vm_char tmp_buf[_MAX_LEN] = { 0 };
    size_t len;

    /* check error(s) */
    if (NULL == cpu_name)
        return;

    pFile = fopen("/proc/cpuinfo", "r");
    if (!pFile)
        return;

    while (fgets(buf, _MAX_LEN, pFile))
    {
        if (!vm_string_strncmp(buf, VM_STRING("vendor_id"), 9))
        {
            vm_string_strncpy_s(tmp_buf, _MAX_LEN, (vm_char*)(buf + 12), vm_string_strnlen_s(buf, _MAX_LEN) - 13);
        }
        else if (!vm_string_strncmp(buf, VM_STRING("model name"), 10))
        {
            if ((len = vm_string_strnlen_s(buf, _MAX_LEN) - 14) > 8)
                vm_string_strncpy_s(cpu_name, _MAX_LEN, (vm_char *)(buf + 13), len);
            else
                vm_string_snprintf(cpu_name, PATH_MAX, VM_STRING("%s"), tmp_buf);
        }
    }
    fclose(pFile);
#endif
} /* void vm_sys_info_get_cpu_name(vm_char *cpu_name) */

void vm_sys_info_get_vga_card(vm_char *vga_card)
{
    /* check error(s) */
    if (NULL == vga_card)
        return;
} /* void vm_sys_info_get_vga_card(vm_char *vga_card) */

void vm_sys_info_get_os_name(vm_char *os_name)
{
    struct utsname buf;

    /* check error(s) */
    if (NULL == os_name)
        return;

    uname(&buf);
    vm_string_sprintf(os_name, VM_STRING("%s %s"), buf.sysname, buf.release);

} /* void vm_sys_info_get_os_name(vm_char *os_name) */

void vm_sys_info_get_computer_name(vm_char *computer_name)
{
    /* check error(s) */
    if (NULL == computer_name)
        return;

    gethostname(computer_name, 128);

} /* void vm_sys_info_get_computer_name(vm_char *computer_name) */

void vm_sys_info_get_program_name(vm_char *program_name)
{
    /* check error(s) */
    if (NULL == program_name)
        return;

#ifdef __APPLE__
    program_name = (vm_char *)getprogname();
#else
    vm_char path[PATH_MAX] = {0,};
    size_t i = 0;

    if(readlink("/proc/self/exe", path, sizeof(path)) == -1)
    {
        // Add error handling
    }
    i = vm_string_strrchr(path, (vm_char)('/')) - path + 1;
    vm_string_strncpy_s(program_name, PATH_MAX, path + i, vm_string_strnlen_s(path, PATH_MAX) - i);
#endif /* __APPLE__ */

} /* void vm_sys_info_get_program_name(vm_char *program_name) */

void vm_sys_info_get_program_path(vm_char *program_path)
{
    vm_char path[ PATH_MAX ] = {0,};
    size_t i = 0;

#ifndef __APPLE__
    /* check error(s) */
    if (NULL == program_path)
        return;

    if (readlink("/proc/self/exe", path, sizeof(path)) == -1)
    {
        // Add error handling
    }
    i = vm_string_strrchr(path, (vm_char)('/')) - path + 1;
    vm_string_strncpy_s(program_path, PATH_MAX, path, i-1);
    program_path[i - 1] = 0;
#else /* for __APPLE__ */
    if ( getcwd( path, PATH_MAX ) != NULL )
      vm_string_strcpy( program_path, path );
#endif /* __APPLE__ */

} /* void vm_sys_info_get_program_path(vm_char *program_path) */

uint32_t vm_sys_info_get_cpu_speed(void)
{
#ifdef __APPLE__
    uint32_t freq;
    return (osx_sysctl_entry_32u(CTL_HW, HW_CPU_FREQ, &freq)) ? (uint32_t)(freq/1000000) : 1000;
#else
    double ret = 0;
    FILE *pFile = NULL;
    vm_char buf[PATH_MAX];

    pFile = fopen("/proc/cpuinfo", "r" );
    if (!pFile)
        return 1000;

    while (fgets(buf, PATH_MAX, pFile))
    {
        if (!vm_string_strncmp(buf, VM_STRING("cpu MHz"), 7))
        {
            ret = vm_string_atol((vm_char *)(buf + 10));
            break;
        }
    }
    fclose(pFile);
    return ((uint32_t) ret);
#endif
} /* uint32_t vm_sys_info_get_cpu_speed(void) */

uint32_t vm_sys_info_get_mem_size(void)
{
#ifndef __APPLE__
    struct sysinfo info;
    sysinfo(&info);
    return (uint32_t)((double)info.totalram / (1024 * 1024) + 0.5);
#else
    uint32_t bts;
    return (osx_sysctl_entry_32u(CTL_HW, HW_PHYSMEM, &bts)) ? (uint32_t)(bts/(1024*1024+0.5)) : 1000;
#endif /* __APPLE__ */

} /* uint32_t vm_sys_info_get_mem_size(void) */

#else
# pragma warning( disable: 4206 )
#endif /* LINUX32 */

/*
 * Copyright (C) 2003, 2004 Stefan Leichter
 * Copyright (C) 2005, 2006 Detlef Riekenberg
 * Copyright (C) 2006 Dmitry Timoshkov
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <assert.h>

#define NONAMELESSSTRUCT
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wingdi.h"
#include "winnls.h"
#include "winuser.h"
#include "winreg.h"
#include "winspool.h"
#include "commdlg.h"
#include "wine/test.h"

#define MAGIC_DEAD  0xdeadbeef
#define DEFAULT_PRINTER_SIZE 1000

static CHAR does_not_exist_dll[]= "does_not_exist.dll";
static CHAR does_not_exist[]    = "does_not_exist";
static CHAR empty[]             = "";
static CHAR env_x86[]           = "Windows NT x86";
static CHAR env_win9x_case[]    = "windowS 4.0";
static CHAR illegal_name[]      = "illegal,name";
static CHAR invalid_env[]       = "invalid_env";
static CHAR LocalPortA[]        = "Local Port";
static CHAR portname_com1[]     = "COM1:";
static CHAR portname_file[]     = "FILE:";
static CHAR portname_lpt1[]     = "LPT1:";
static CHAR server_does_not_exist[] = "\\does_not_exist";
static CHAR version_dll[]       = "version.dll";
static CHAR winetest[]          = "winetest";
static CHAR xcv_localport[]     = ",XcvMonitor Local Port";

static WCHAR cmd_MonitorUIW[] = {'M','o','n','i','t','o','r','U','I',0};
static WCHAR cmd_PortIsValidW[] = {'P','o','r','t','I','s','V','a','l','i','d',0};
static WCHAR emptyW[] = {0};

static WCHAR portname_com1W[] = {'C','O','M','1',':',0};
static WCHAR portname_com2W[] = {'C','O','M','2',':',0};
static WCHAR portname_fileW[] = {'F','I','L','E',':',0};
static WCHAR portname_lpt1W[] = {'L','P','T','1',':',0};
static WCHAR portname_lpt2W[] = {'L','P','T','2',':',0};

static HANDLE  hwinspool;
static BOOL  (WINAPI * pGetDefaultPrinterA)(LPSTR, LPDWORD);
static BOOL  (WINAPI * pSetDefaultPrinterA)(LPCSTR);
static DWORD (WINAPI * pXcvDataW)(HANDLE, LPCWSTR, PBYTE, DWORD, PBYTE, DWORD, PDWORD, PDWORD);
static BOOL  (WINAPI * pAddPortExA)(LPSTR, DWORD, LPBYTE, LPSTR);


/* ################################ */

struct monitor_entry {
    LPSTR  env;
    CHAR  dllname[32];
};

static LPSTR default_printer = NULL;
static LPSTR local_server = NULL;
static LPSTR tempdirA = NULL;
static LPSTR tempfileA = NULL;
static LPWSTR tempdirW = NULL;
static LPWSTR tempfileW = NULL;

/* ################################ */
/* report common behavior only once */
static DWORD deactivated_spooler_reported = 0;
#define RETURN_ON_DEACTIVATED_SPOOLER(res) \
    if ((res == 0) && (GetLastError() == RPC_S_SERVER_UNAVAILABLE)) \
    { \
        if (!deactivated_spooler_reported) { \
            deactivated_spooler_reported++; \
            skip("The Service 'Spooler' is required for many test\n"); \
        } \
        return; \
    }

static DWORD access_denied_reported = 0;
#define RETURN_ON_ACCESS_DENIED(res) \
    if ((res == 0) && (GetLastError() == ERROR_ACCESS_DENIED)) \
    { \
        if (!access_denied_reported) { \
            access_denied_reported++; \
            skip("More Access-Rights are required for many test\n"); \
        } \
        return; \
    }

/* ################################ */

static void find_default_printer(VOID)
{
    static  char    buffer[DEFAULT_PRINTER_SIZE];
    DWORD   needed;
    DWORD   res;
    LPSTR   ptr;

    if ((default_printer == NULL) && (pGetDefaultPrinterA))
    {
        /* w2k and above */
        needed = sizeof(buffer);
        res = pGetDefaultPrinterA(buffer, &needed);
        if(res)  default_printer = buffer;
        trace("default_printer: '%s'\n", default_printer);
    }
    if (default_printer == NULL)
    {
        HKEY hwindows;
        DWORD   type;
        /* NT 3.x and above */
        if (RegOpenKeyEx(HKEY_CURRENT_USER, 
                        "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                        0, KEY_QUERY_VALUE, &hwindows) == NO_ERROR) {

            needed = sizeof(buffer);
            if (RegQueryValueEx(hwindows, "device", NULL, 
                                &type, (LPBYTE)buffer, &needed) == NO_ERROR) {

                ptr = strchr(buffer, ',');
                if (ptr) {
                    ptr[0] = '\0';
                    default_printer = buffer;
                }
            }
            RegCloseKey(hwindows);
        }
        trace("default_printer: '%s'\n", default_printer);
    }
    if (default_printer == NULL)
    {
        /* win9x */
        needed = sizeof(buffer);
        res = GetProfileStringA("windows", "device", "*", buffer, needed);
        if(res) {
            ptr = strchr(buffer, ',');
            if (ptr) {
                ptr[0] = '\0';
                default_printer = buffer;
            }
        }
        trace("default_printer: '%s'\n", default_printer);
    }
}


static struct monitor_entry * find_installed_monitor(void)
{
    MONITOR_INFO_2A mi2a; 
    static struct  monitor_entry * entry = NULL;
    DWORD   num_tests;
    DWORD   i = 0;

    static struct monitor_entry  monitor_table[] = {
        {env_win9x_case, "localspl.dll"},
        {env_x86,        "localspl.dll"},
        {env_win9x_case, "localmon.dll"},
        {env_x86,        "localmon.dll"},
        {env_win9x_case, "tcpmon.dll"},
        {env_x86,        "tcpmon.dll"},
        {env_win9x_case, "usbmon.dll"},
        {env_x86,        "usbmon.dll"},
        {env_win9x_case, "mspp32.dll"},
        {env_x86,        "win32spl.dll"},
        {env_x86,        "redmonnt.dll"},
        {env_x86,        "redmon35.dll"},
        {env_win9x_case, "redmon95.dll"},
        {env_x86,        "pdfcmnnt.dll"},
        {env_win9x_case, "pdfcmn95.dll"},
    };

    if (entry) return entry;

    num_tests = (sizeof(monitor_table)/sizeof(struct monitor_entry));

    /* cleanup */
    DeleteMonitorA(NULL, env_x86, winetest);
    DeleteMonitorA(NULL, env_win9x_case, winetest);

    /* find a usable monitor from the table */
    mi2a.pName = winetest;
    while ((entry == NULL) && (i < num_tests)) {
        entry = &monitor_table[i];
        i++;
        mi2a.pEnvironment = entry->env;
        mi2a.pDLLName = entry->dllname;

        if (AddMonitorA(NULL, 2, (LPBYTE) &mi2a)) {
            /* we got one */
            trace("using '%s', '%s'\n", entry->env, entry->dllname);
            DeleteMonitorA(NULL, entry->env, winetest);
        }
        else
        {
            entry = NULL;
        }
    }
    return entry;
}


/* ########################### */

static void find_local_server(VOID)
{
    static  char    buffer[MAX_PATH];
    DWORD   res;
    DWORD   size;

    size = sizeof(buffer) - 3 ;
    buffer[0] = '\\';
    buffer[1] = '\\';
    buffer[2] = '\0';

    SetLastError(0xdeadbeef);
    res = GetComputerNameA(&buffer[2], &size);
    trace("returned %d with %d and %d: '%s'\n", res, GetLastError(), size, buffer);

    ok( res != 0, "returned %d with %d and %d: '%s' (expected '!= 0')\n",
        res, GetLastError(), size, buffer);

    if (res) local_server = buffer;
}

/* ########################### */

static void find_tempfile(VOID)
{
    static CHAR buffer_dirA[MAX_PATH];
    static CHAR buffer_fileA[MAX_PATH];
    static WCHAR buffer_dirW[MAX_PATH];
    static WCHAR buffer_fileW[MAX_PATH];
    DWORD   res;
    int     resint;

    memset(buffer_dirA, 0, MAX_PATH - 1);
    buffer_dirA[MAX_PATH - 1] = '\0';
    SetLastError(0xdeadbeef);
    res = GetTempPathA(MAX_PATH, buffer_dirA);
    ok(res, "returned %u with %u and '%s' (expected '!= 0')\n", res, GetLastError(), buffer_dirA);
    if (res == 0) return;

    memset(buffer_fileA, 0, MAX_PATH - 1);
    buffer_fileA[MAX_PATH - 1] = '\0';
    SetLastError(0xdeadbeef);
    res = GetTempFileNameA(buffer_dirA, winetest, 0, buffer_fileA);
    ok(res, "returned %u with %u and '%s' (expected '!= 0')\n", res, GetLastError(), buffer_fileA);
    if (res == 0) return;

    SetLastError(0xdeadbeef);
    resint = MultiByteToWideChar(CP_ACP, 0, buffer_dirA, -1, buffer_dirW, MAX_PATH);
    ok(res, "returned %u with %u (expected '!= 0')\n", resint, GetLastError());
    if (resint == 0) return;

    SetLastError(0xdeadbeef);
    resint = MultiByteToWideChar(CP_ACP, 0, buffer_fileA, -1, buffer_fileW, MAX_PATH);
    ok(res, "returned %u with %u (expected '!= 0')\n", resint, GetLastError());
    if (resint == 0) return;

    tempdirA  = buffer_dirA;
    tempfileA = buffer_fileA;
    tempdirW  = buffer_dirW;
    tempfileW = buffer_fileW;
    trace("tempfile: '%s'\n", tempfileA);
}

/* ########################### */

static void test_AddMonitor(void)
{
    MONITOR_INFO_2A mi2a; 
    struct  monitor_entry * entry = NULL;
    DWORD   res;

    entry = find_installed_monitor();

    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 1, NULL);
    ok(!res && (GetLastError() == ERROR_INVALID_LEVEL), 
        "returned %d with %d (expected '0' with ERROR_INVALID_LEVEL)\n",
        res, GetLastError());

    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 3, NULL);
    ok(!res && (GetLastError() == ERROR_INVALID_LEVEL), 
        "returned %d with %d (expected '0' with ERROR_INVALID_LEVEL)\n",
        res, GetLastError());

    if (0)
    {
    /* This test crash with win9x on vmware (works with win9x on qemu 0.8.1) */
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, NULL);
    /* NT: unchanged,  9x: ERROR_PRIVILEGE_NOT_HELD */
    ok(!res &&
        ((GetLastError() == MAGIC_DEAD) ||
         (GetLastError() == ERROR_PRIVILEGE_NOT_HELD)), 
        "returned %d with %d (expected '0' with: MAGIC_DEAD or "
        "ERROR_PRIVILEGE_NOT_HELD)\n", res, GetLastError());
    }

    ZeroMemory(&mi2a, sizeof(MONITOR_INFO_2A));
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    RETURN_ON_ACCESS_DENIED(res)

    /* NT: ERROR_INVALID_PARAMETER,  9x: ERROR_INVALID_ENVIRONMENT */
    ok(!res && ((GetLastError() == ERROR_INVALID_PARAMETER) ||
                (GetLastError() == ERROR_INVALID_ENVIRONMENT)), 
        "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or "
        "ERROR_INVALID_ENVIRONMENT)\n", res, GetLastError());

    if (!entry) {
        skip("No usable Monitor found\n");
        return;
    }

    if (0)
    {
    /* The Test is deactivated, because when mi2a.pName is NULL, the subkey
       HKLM\System\CurrentControlSet\Control\Print\Monitors\C:\WINDOWS\SYSTEM
       or HKLM\System\CurrentControlSet\Control\Print\Monitors\?
       is created on win9x and we do not want to hit this bug here. */

    mi2a.pEnvironment = entry->env;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_INVALID_PARAMETER,  9x: ERROR_PRIVILEGE_NOT_HELD */
    }

    mi2a.pEnvironment = entry->env;
    mi2a.pName = empty;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_INVALID_PARAMETER,  9x: ERROR_PRIVILEGE_NOT_HELD */
    ok( !res &&
        ((GetLastError() == ERROR_INVALID_PARAMETER) ||
         (GetLastError() == ERROR_PRIVILEGE_NOT_HELD)), 
        "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or "
        "ERROR_PRIVILEGE_NOT_HELD)\n",
        res, GetLastError());

    mi2a.pName = winetest;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_INVALID_PARAMETER,  9x: ERROR_PRIVILEGE_NOT_HELD */
    ok( !res &&
        ((GetLastError() == ERROR_INVALID_PARAMETER) ||
         (GetLastError() == ERROR_PRIVILEGE_NOT_HELD)), 
        "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or "
        "ERROR_PRIVILEGE_NOT_HELD)\n",
        res, GetLastError());

    mi2a.pDLLName = empty;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "returned %d with %d (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());

    mi2a.pDLLName = does_not_exist_dll;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_MOD_NOT_FOUND,  9x: ERROR_INVALID_PARAMETER */
    ok( !res &&
        ((GetLastError() == ERROR_MOD_NOT_FOUND) ||
        (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with: ERROR_MOD_NOT_FOUND or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());

    mi2a.pDLLName = version_dll;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_PROC_NOT_FOUND,  9x: ERROR_INVALID_PARAMETER */
    ok( !res &&
        ((GetLastError() == ERROR_PROC_NOT_FOUND) ||
        (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with: ERROR_PROC_NOT_FOUND or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());
    if (res) DeleteMonitorA(NULL, entry->env, winetest);

   /* Test AddMonitor with real options */
    mi2a.pDLLName = entry->dllname;
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    /* add a monitor twice */
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    /* NT: ERROR_PRINT_MONITOR_ALREADY_INSTALLED (3006), 9x: ERROR_ALREADY_EXISTS (183) */
    ok( !res &&
        ((GetLastError() == ERROR_PRINT_MONITOR_ALREADY_INSTALLED) ||
        (GetLastError() == ERROR_ALREADY_EXISTS)), 
        "returned %d with %d (expected '0' with: "
        "ERROR_PRINT_MONITOR_ALREADY_INSTALLED or ERROR_ALREADY_EXISTS)\n",
        res, GetLastError());

    DeleteMonitorA(NULL, entry->env, winetest);
    SetLastError(MAGIC_DEAD);
    res = AddMonitorA(empty, 2, (LPBYTE) &mi2a);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    /* cleanup */
    DeleteMonitorA(NULL, entry->env, winetest);

}

/* ########################### */

static void test_AddPort(void)
{
    DWORD   res;

    SetLastError(0xdeadbeef);
    res = AddPortA(NULL, 0, NULL);
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    /* NT: RPC_X_NULL_REF_POINTER, 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == RPC_X_NULL_REF_POINTER) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());


    SetLastError(0xdeadbeef);
    res = AddPortA(NULL, 0, empty);
    /* Allowed only for (Printer-)Administrators */
    RETURN_ON_ACCESS_DENIED(res)

    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());


    SetLastError(0xdeadbeef);
    res = AddPortA(NULL, 0, does_not_exist);
    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());

}

/* ########################### */

static void test_AddPortEx(void)
{
    PORT_INFO_2A pi;
    DWORD   res;


    if (!pAddPortExA) {
        skip("AddPortEx not supported\n");
        return;
    }

    /* start test with a clean system */
    DeletePortA(NULL, 0, tempfileA);

    pi.pPortName = tempfileA;
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, LocalPortA);
    RETURN_ON_DEACTIVATED_SPOOLER(res)

    /* Allowed only for (Printer-)Administrators.
       W2K+XP: ERROR_INVALID_PARAMETER  */
    if (!res && (GetLastError() == ERROR_INVALID_PARAMETER)) {
        skip("ACCESS_DENIED (ERROR_INVALID_PARAMETER)\n");
        return;
    }
    ok( res, "got %u with %u (expected '!= 0')\n", res, GetLastError());

    /* Add a port, that already exist */
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    DeletePortA(NULL, 0, tempfileA);


    /* the Monitorname must match */
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, NULL);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    if (res) DeletePortA(NULL, 0, tempfileA);

    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, empty);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    if (res) DeletePortA(NULL, 0, tempfileA);

    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, does_not_exist);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    if (res) DeletePortA(NULL, 0, tempfileA);


    /* We need a Portname */
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, NULL, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());

    pi.pPortName = NULL;
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 1, (LPBYTE) &pi, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u with %u (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    if (res) DeletePortA(NULL, 0, tempfileA);


    /*  level 2 is documented as supported for Printmonitors,
        but that is not supported for "Local Port" (localspl.dll) and
        AddPortEx fails with ERROR_INVALID_LEVEL */

    pi.pPortName = tempfileA;
    pi.pMonitorName = LocalPortA;
    pi.pDescription = winetest;
    pi.fPortType = PORT_TYPE_WRITE;

    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 2, (LPBYTE) &pi, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_LEVEL),
        "got %u with %u (expected '0' with ERROR_INVALID_LEVEL)\n",
        res, GetLastError());
    if (res) DeletePortA(NULL, 0, tempfileA);


    /* invalid levels */
    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 0, (LPBYTE) &pi, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_LEVEL),
        "got %u with %u (expected '0' with ERROR_INVALID_LEVEL)\n",
        res, GetLastError());

    SetLastError(0xdeadbeef);
    res = pAddPortExA(NULL, 3, (LPBYTE) &pi, LocalPortA);
    ok( !res && (GetLastError() == ERROR_INVALID_LEVEL),
        "got %u with %u (expected '0' with ERROR_INVALID_LEVEL)\n",
        res, GetLastError());


    /* cleanup */
    DeletePortA(NULL, 0, tempfileA);

}

/* ########################### */

static void test_ConfigurePort(void)
{
    DWORD   res;


    SetLastError(0xdeadbeef);
    res = ConfigurePortA(NULL, 0, NULL);
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    /* NT: RPC_X_NULL_REF_POINTER, 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == RPC_X_NULL_REF_POINTER) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());

    SetLastError(0xdeadbeef);
    res = ConfigurePortA(NULL, 0, empty);
    /* Allowed only for (Printer-)Administrators */
    RETURN_ON_ACCESS_DENIED(res)

    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());


    SetLastError(0xdeadbeef);
    res = ConfigurePortA(NULL, 0, does_not_exist);
    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());


    /*  Testing-Results:
        - Case of Portnames is ignored 
        - Portname without ":" => NT: ERROR_NOT_SUPPORTED, 9x: Dialog comes up
        - Empty Servername (LPT1:) => NT: ERROR_NOT_SUPPORTED, 9x: Dialog comes up

        - Port not present =>  9x: ERROR_INVALID_PARAMETER, NT:ERROR_NOT_SUPPORTED
        - "FILE:" => 9x:Success, NT:ERROR_CANCELED
        - Cancel ("Local Port") => ERROR_CANCELED
        - Cancel ("Redirected Port") => Success
    */
    if (winetest_interactive > 0) {
        SetLastError(0xdeadbeef);
        res = ConfigurePortA(NULL, 0, portname_com1);
        trace("'%s' returned %d with %d\n", portname_com1, res, GetLastError());

        SetLastError(0xdeadbeef);
        res = ConfigurePortA(NULL, 0, portname_lpt1);
        trace("'%s' returned %d with %d\n", portname_lpt1, res, GetLastError());

        SetLastError(0xdeadbeef);
        res = ConfigurePortA(NULL, 0, portname_file);
        trace("'%s' returned %d with %d\n", portname_file, res, GetLastError());
    }
}

/* ########################### */

static void test_DeleteMonitor(void)
{
    MONITOR_INFO_2A         mi2a;
    struct monitor_entry  * entry = NULL;
    DWORD                   res;


    entry = find_installed_monitor();

    if (!entry) {
        skip("No usable Monitor found\n");
        return;
    }

    mi2a.pName = winetest;
    mi2a.pEnvironment = entry->env;
    mi2a.pDLLName = entry->dllname;

    /* Testing DeleteMonitor with real options */
    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);

    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, entry->env, winetest);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    /* Delete the Monitor twice */
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, entry->env, winetest);
    /* NT: ERROR_UNKNOWN_PRINT_MONITOR (3000), 9x: ERROR_INVALID_PARAMETER (87) */
    ok( !res &&
        ((GetLastError() == ERROR_UNKNOWN_PRINT_MONITOR) ||
        (GetLastError() == ERROR_INVALID_PARAMETER)), 
        "returned %d with %d (expected '0' with: ERROR_UNKNOWN_PRINT_MONITOR"
        " or ERROR_INVALID_PARAMETER)\n", res, GetLastError());

    /* the environment */
    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, NULL, winetest);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());

    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, empty, winetest);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());

    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, invalid_env, winetest);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());

    /* the monitor-name */
    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, entry->env, NULL);
    /* NT: ERROR_INVALID_PARAMETER (87),  9x: ERROR_INVALID_NAME (123)*/
    ok( !res &&
        ((GetLastError() == ERROR_INVALID_PARAMETER) ||
        (GetLastError() == ERROR_INVALID_NAME)),
        "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or "
        "ERROR_INVALID_NAME)\n", res, GetLastError());

    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(NULL, entry->env, empty);
    /* NT: ERROR_INVALID_PARAMETER (87),  9x: ERROR_INVALID_NAME (123)*/
    ok( !res && 
        ((GetLastError() == ERROR_INVALID_PARAMETER) ||
        (GetLastError() == ERROR_INVALID_NAME)),
        "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or "
        "ERROR_INVALID_NAME)\n", res, GetLastError());

    AddMonitorA(NULL, 2, (LPBYTE) &mi2a);
    SetLastError(MAGIC_DEAD);
    res = DeleteMonitorA(empty, entry->env, winetest);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());

    /* cleanup */
    DeleteMonitorA(NULL, entry->env, winetest);
}

/* ########################### */

static void test_DeletePort(void)
{
    DWORD   res;

    SetLastError(0xdeadbeef);
    res = DeletePortA(NULL, 0, NULL);
    RETURN_ON_DEACTIVATED_SPOOLER(res)

    SetLastError(0xdeadbeef);
    res = DeletePortA(NULL, 0, empty);
    /* Allowed only for (Printer-)Administrators */
    RETURN_ON_ACCESS_DENIED(res)

    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());


    SetLastError(0xdeadbeef);
    res = DeletePortA(NULL, 0, does_not_exist);
    /* XP: ERROR_NOT_SUPPORTED, NT351 and 9x: ERROR_INVALID_PARAMETER */
    ok( !res && ((GetLastError() == ERROR_NOT_SUPPORTED) || 
                 (GetLastError() == ERROR_INVALID_PARAMETER)),
        "returned %d with %d (expected '0' with ERROR_NOT_SUPPORTED or "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError());

}

/* ########################### */

static void test_EnumForms(LPSTR pName)
{
    DWORD   res;
    HANDLE  hprinter = 0;
    LPBYTE  buffer;
    DWORD   cbBuf;
    DWORD   pcbNeeded;
    DWORD   pcReturned;
    DWORD   level;
    UINT    i;
    const char *formtype;
    static const char * const formtypes[] = { "FORM_USER", "FORM_BUILTIN", "FORM_PRINTER", "FORM_flag_unknown" };
#define FORMTYPE_MAX 2
    PFORM_INFO_1A pFI_1a;
    PFORM_INFO_2A pFI_2a;

    res = OpenPrinter(pName, &hprinter, NULL);
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    if (!res || !hprinter)
    {
        /* Open the local Prinserver is not supported on win9x */
        if (pName) skip("Failed to open '%s' (not supported on win9x)\n", pName);
        return;
    }

    /* valid levels are 1 and 2 */
    for(level = 0; level < 4; level++) {
        cbBuf = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, NULL, 0, &cbBuf, &pcReturned);

        /* EnumForms is not implemented in win9x */
        if (!res && (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)) continue;

        /* EnumForms for the Server not implemented on all NT-Versions */
        if (!res && (GetLastError() == ERROR_INVALID_HANDLE) && !pName) continue;

        /* Level 2 for EnumForms is not supported on all systems */
        if (!res && (GetLastError() == ERROR_INVALID_LEVEL) && (level == 2)) continue;

        /* use only a short test, when we test with an invalid level */
        if(!level || (level > 2)) {
            ok( (!res && (GetLastError() == ERROR_INVALID_LEVEL)) ||
                (res && (pcReturned == 0)),
                "(%d) returned %d with %d and 0x%08x (expected '0' with "
                "ERROR_INVALID_LEVEL or '!=0' and 0x0)\n",
                level, res, GetLastError(), pcReturned);
            continue;
        }

        ok((!res) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());

        buffer = HeapAlloc(GetProcessHeap(), 0, cbBuf *2);
        if (buffer == NULL) continue;

        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, buffer, cbBuf, &pcbNeeded, &pcReturned);
        ok(res, "(%d) returned %d with %d (expected '!=0')\n",
                level, res, GetLastError());

        if (winetest_debug > 1) {
            trace("dumping %d forms level %d\n", pcReturned, level);
            pFI_1a = (PFORM_INFO_1A)buffer;
            pFI_2a = (PFORM_INFO_2A)buffer;
            for (i = 0; i < pcReturned; i++)
            {
                /* first part is same in FORM_INFO_1 and FORM_INFO_2 */
                formtype = (pFI_1a->Flags <= FORMTYPE_MAX) ? formtypes[pFI_1a->Flags] : formtypes[3];
                trace("%u (%s): %.03fmm x %.03fmm, %s\n", i, pFI_1a->pName,
                      (float)pFI_1a->Size.cx/1000, (float)pFI_1a->Size.cy/1000, formtype);

                if (level == 1) pFI_1a ++;
                else {
                    /* output additional FORM_INFO_2 fields */
                    trace("\tkeyword=%s strtype=%u muidll=%s resid=%u dispname=%s langid=%u\n",
                          pFI_2a->pKeyword, pFI_2a->StringType, pFI_2a->pMuiDll,
                          pFI_2a->dwResourceId, pFI_2a->pDisplayName, pFI_2a->wLangId);

                    /* offset pointer pFI_1a by 1*sizeof(FORM_INFO_2A) Bytes */
                    pFI_2a ++;
                    pFI_1a = (PFORM_INFO_1A)pFI_2a;
                }
            }
        }

        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, buffer, cbBuf+1, &pcbNeeded, &pcReturned);
        ok( res, "(%d) returned %d with %d (expected '!=0')\n",
            level, res, GetLastError());

        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, buffer, cbBuf-1, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());


        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, NULL, cbBuf, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INVALID_USER_BUFFER) ,
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INVALID_USER_BUFFER)\n", level, res, GetLastError());


        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, buffer, cbBuf, NULL, &pcReturned);
        ok( !res && (GetLastError() == RPC_X_NULL_REF_POINTER) ,
            "(%d) returned %d with %d (expected '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        SetLastError(0xdeadbeef);
        res = EnumFormsA(hprinter, level, buffer, cbBuf, &pcbNeeded, NULL);
        ok( !res && (GetLastError() == RPC_X_NULL_REF_POINTER) ,
            "(%d) returned %d with %d (expected '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        SetLastError(0xdeadbeef);
        res = EnumFormsA(0, level, buffer, cbBuf, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INVALID_HANDLE) ,
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INVALID_HANDLE)\n", level, res, GetLastError());

        HeapFree(GetProcessHeap(), 0, buffer);
    } /* for(level ... */

    ClosePrinter(hprinter);
}

/* ########################### */

static void test_EnumMonitors(void)
{
    DWORD   res;
    LPBYTE  buffer;
    DWORD   cbBuf;
    DWORD   pcbNeeded;
    DWORD   pcReturned;
    DWORD   level;

    /* valid levels are 1 and 2 */
    for(level = 0; level < 4; level++) {
        cbBuf = MAGIC_DEAD;
        pcReturned = MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = EnumMonitorsA(NULL, level, NULL, 0, &cbBuf, &pcReturned);

        RETURN_ON_DEACTIVATED_SPOOLER(res)

        /* not implemented yet in wine */
        if (!res && (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)) continue;


        /* use only a short test, when we test with an invalid level */
        if(!level || (level > 2)) {
            ok( (!res && (GetLastError() == ERROR_INVALID_LEVEL)) ||
                (res && (pcReturned == 0)),
                "(%d) returned %d with %d and 0x%08x (expected '0' with "
                "ERROR_INVALID_LEVEL or '!=0' and 0x0)\n",
                level, res, GetLastError(), pcReturned);
            continue;
        }        

        /* Level 2 is not supported on win9x */
        if (!res && (GetLastError() == ERROR_INVALID_LEVEL)) {
            skip("Level %d not supported\n", level);
            continue;
        }

        ok((!res) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());

        if (!cbBuf) {
            skip("no valid buffer size returned\n");
            continue;
        }

        buffer = HeapAlloc(GetProcessHeap(), 0, cbBuf *2);
        if (buffer == NULL) continue;

        SetLastError(MAGIC_DEAD);
        pcbNeeded = MAGIC_DEAD;
        res = EnumMonitorsA(NULL, level, buffer, cbBuf, &pcbNeeded, &pcReturned);
        ok(res, "(%d) returned %d with %d (expected '!=0')\n",
                level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n",
                level, pcbNeeded, cbBuf);
        /* We can validate the returned Data with the Registry here */


        SetLastError(MAGIC_DEAD);
        pcReturned = MAGIC_DEAD;
        pcbNeeded = MAGIC_DEAD;
        res = EnumMonitorsA(NULL, level, buffer, cbBuf+1, &pcbNeeded, &pcReturned);
        ok(res, "(%d) returned %d with %d (expected '!=0')\n", level,
                res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level,
                pcbNeeded, cbBuf);

        SetLastError(MAGIC_DEAD);
        pcbNeeded = MAGIC_DEAD;
        res = EnumMonitorsA(NULL, level, buffer, cbBuf-1, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());

        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level,
                pcbNeeded, cbBuf);

/*
      Do not add the next test:
      w2k+:  RPC_X_NULL_REF_POINTER 
      NT3.5: ERROR_INVALID_USER_BUFFER
      win9x: crash in winspool.drv

      res = EnumMonitorsA(NULL, level, NULL, cbBuf, &pcbNeeded, &pcReturned);
*/

        SetLastError(MAGIC_DEAD);
        pcbNeeded = MAGIC_DEAD;
        pcReturned = MAGIC_DEAD;
        res = EnumMonitorsA(NULL, level, buffer, cbBuf, NULL, &pcReturned);
        ok( res || (!res && (GetLastError() == RPC_X_NULL_REF_POINTER)) ,
            "(%d) returned %d with %d (expected '!=0' or '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        pcbNeeded = MAGIC_DEAD;
        pcReturned = MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = EnumMonitorsA(NULL, level, buffer, cbBuf, &pcbNeeded, NULL);
        ok( res || (!res && (GetLastError() == RPC_X_NULL_REF_POINTER)) ,
            "(%d) returned %d with %d (expected '!=0' or '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        HeapFree(GetProcessHeap(), 0, buffer);
    } /* for(level ... */
}

/* ########################### */

static void test_EnumPorts(void)
{
    DWORD   res;
    DWORD   level;
    LPBYTE  buffer;
    DWORD   cbBuf;
    DWORD   pcbNeeded;
    DWORD   pcReturned;

    /* valid levels are 1 and 2 */
    for(level = 0; level < 4; level++) {

        cbBuf = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPortsA(NULL, level, NULL, 0, &cbBuf, &pcReturned);
        RETURN_ON_DEACTIVATED_SPOOLER(res)

        /* use only a short test, when we test with an invalid level */
        if(!level || (level > 2)) {
            /* NT: ERROR_INVALID_LEVEL, 9x: success */
            ok( (!res && (GetLastError() == ERROR_INVALID_LEVEL)) ||
                (res && (pcReturned == 0)),
                "(%d) returned %d with %d and 0x%08x (expected '0' with "
                "ERROR_INVALID_LEVEL or '!=0' and 0x0)\n",
                level, res, GetLastError(), pcReturned);
            continue;
        }        

        
        /* Level 2 is not supported on NT 3.x */
        if (!res && (GetLastError() == ERROR_INVALID_LEVEL)) {
            skip("Level %d not supported\n", level);
            continue;
        }

        ok((!res) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());

        buffer = HeapAlloc(GetProcessHeap(), 0, cbBuf *2);
        if (buffer == NULL) continue;

        pcbNeeded = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPortsA(NULL, level, buffer, cbBuf, &pcbNeeded, &pcReturned);
        ok(res, "(%d) returned %d with %d (expected '!=0')\n", level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level, pcbNeeded, cbBuf);
        /* ToDo: Compare the returned Data with the Registry / "win.ini",[Ports] here */

        pcbNeeded = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPortsA(NULL, level, buffer, cbBuf+1, &pcbNeeded, &pcReturned);
        ok(res, "(%d) returned %d with %d (expected '!=0')\n", level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level, pcbNeeded, cbBuf);

        pcbNeeded = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPortsA(NULL, level, buffer, cbBuf-1, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%d) returned %d with %d (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER)\n", level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level, pcbNeeded, cbBuf);

        /*
          Do not add this test:
          res = EnumPortsA(NULL, level, NULL, cbBuf, &pcbNeeded, &pcReturned);
          w2k+:  RPC_X_NULL_REF_POINTER 
          NT3.5: ERROR_INVALID_USER_BUFFER
          win9x: crash in winspool.drv
         */

        SetLastError(0xdeadbeef);
        res = EnumPorts(NULL, level, buffer, cbBuf, NULL, &pcReturned);
        /* NT: RPC_X_NULL_REF_POINTER (1780),  9x: success */
        ok( (!res && (GetLastError() == RPC_X_NULL_REF_POINTER) ) ||
            ( res && (GetLastError() == ERROR_SUCCESS) ),
            "(%d) returned %d with %d (expected '0' with "
            "RPC_X_NULL_REF_POINTER or '!=0' with NO_ERROR)\n",
            level, res, GetLastError());


        SetLastError(0xdeadbeef);
        res = EnumPorts(NULL, level, buffer, cbBuf, &pcbNeeded, NULL);
        /* NT: RPC_X_NULL_REF_POINTER (1780),  9x: success */
        ok( (!res && (GetLastError() == RPC_X_NULL_REF_POINTER) ) ||
            ( res && (GetLastError() == ERROR_SUCCESS) ),
            "(%d) returned %d with %d (expected '0' with "
            "RPC_X_NULL_REF_POINTER or '!=0' with NO_ERROR)\n",
            level, res, GetLastError());

        HeapFree(GetProcessHeap(), 0, buffer);
    }
}

/* ########################### */

static void test_EnumPrinterDrivers(void)
{
    DWORD   res;
    LPBYTE  buffer;
    DWORD   cbBuf;
    DWORD   pcbNeeded;
    DWORD   pcReturned;
    DWORD   level;

    /* 1-3 for w95/w98/NT4; 1-3+6 for me; 1-6 for w2k/xp/2003; 1-6+8 for vista */
    for(level = 0; level < 10; level++) {
        cbBuf = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPrinterDriversA(NULL, NULL, level, NULL, 0, &cbBuf, &pcReturned);
        RETURN_ON_DEACTIVATED_SPOOLER(res)

        /* use only a short test, when we test with an invalid level */
        if(!level || (level == 7) || (level > 8)) {

            ok( (!res && (GetLastError() == ERROR_INVALID_LEVEL)) ||
                (res && (pcReturned == 0)),
                "(%d) got %u with %u and 0x%x "
                "(expected '0' with ERROR_INVALID_LEVEL or '!=0' and 0x0)\n",
                level, res, GetLastError(), pcReturned);
            continue;
        }

        /* some level are not supported in all windows versions */
        if (!res && (GetLastError() == ERROR_INVALID_LEVEL)) {
            skip("Level %d not supported\n", level);
            continue;
        }

        ok( ((!res) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) ||
            (res && (default_printer == NULL)),
            "(%u) got %u with %u for %s (expected '0' with "
            "ERROR_INSUFFICIENT_BUFFER or '!= 0' without a printer)\n",
            level, res, GetLastError(), default_printer);

        if (!cbBuf) {
            skip("no valid buffer size returned\n");
            continue;
        }

        buffer = HeapAlloc(GetProcessHeap(), 0, cbBuf + 4);
        if (buffer == NULL) continue;

        SetLastError(0xdeadbeef);
        pcbNeeded = 0xdeadbeef;
        res = EnumPrinterDriversA(NULL, NULL, level, buffer, cbBuf, &pcbNeeded, &pcReturned);
        ok(res, "(%u) got %u with %u (expected '!=0')\n", level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%d) returned %d (expected %d)\n", level, pcbNeeded, cbBuf);

        /* validate the returned Data here */
        if (level > 1) {
            LPDRIVER_INFO_2A di = (LPDRIVER_INFO_2A) buffer;

            ok( strrchr(di->pDriverPath, '\\') != NULL,
                "(%u) got %s for %s (expected a full path)\n",
                level, di->pDriverPath, di->pName);

        }

        SetLastError(0xdeadbeef);
        pcReturned = 0xdeadbeef;
        pcbNeeded = 0xdeadbeef;
        res = EnumPrinterDriversA(NULL, NULL, level, buffer, cbBuf+1, &pcbNeeded, &pcReturned);
        ok(res, "(%u) got %u with %u (expected '!=0')\n", level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%u) returned %u (expected %u)\n", level, pcbNeeded, cbBuf);

        SetLastError(0xdeadbeef);
        pcbNeeded = 0xdeadbeef;
        res = EnumPrinterDriversA(NULL, NULL, level, buffer, cbBuf-1, &pcbNeeded, &pcReturned);
        ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
            "(%u) got %u with %u (expected '0' with ERROR_INSUFFICIENT_BUFFER)\n",
            level, res, GetLastError());
        ok(pcbNeeded == cbBuf, "(%u) returned %u (expected %u)\n", level, pcbNeeded, cbBuf);

/*
      Do not add the next test:
      NT: ERROR_INVALID_USER_BUFFER
      win9x: crash or 100% CPU

      res = EnumPrinterDriversA(NULL, NULL, level, NULL, cbBuf, &pcbNeeded, &pcReturned);
*/

        SetLastError(0xdeadbeef);
        pcbNeeded = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        res = EnumPrinterDriversA(NULL, NULL, level, buffer, cbBuf, NULL, &pcReturned);
        ok( res || (!res && (GetLastError() == RPC_X_NULL_REF_POINTER)) ,
            "(%u) got %u with %u (expected '!=0' or '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        pcbNeeded = 0xdeadbeef;
        pcReturned = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = EnumPrinterDriversA(NULL, NULL, level, buffer, cbBuf, &pcbNeeded, NULL);
        ok( res || (!res && (GetLastError() == RPC_X_NULL_REF_POINTER)) ,
            "(%u) got %u with %u (expected '!=0' or '0' with "
            "RPC_X_NULL_REF_POINTER)\n", level, res, GetLastError());

        HeapFree(GetProcessHeap(), 0, buffer);
    } /* for(level ... */
}

/* ########################### */

static void test_GetDefaultPrinter(void)
{
    BOOL    retval;
    DWORD   exact = DEFAULT_PRINTER_SIZE;
    DWORD   size;
    char    buffer[DEFAULT_PRINTER_SIZE];

    if (!pGetDefaultPrinterA)  return;
	/* only supported on NT like OSes starting with win2k */

    SetLastError(ERROR_SUCCESS);
    retval = pGetDefaultPrinterA(buffer, &exact);
    if (!retval || !exact || !strlen(buffer) ||
	(ERROR_SUCCESS != GetLastError())) {
	if ((ERROR_FILE_NOT_FOUND == GetLastError()) ||
	    (ERROR_INVALID_NAME == GetLastError()))
	    trace("this test requires a default printer to be set\n");
	else {
		ok( 0, "function call GetDefaultPrinterA failed unexpected!\n"
		"function returned %s\n"
		"last error 0x%08x\n"
		"returned buffer size 0x%08x\n"
		"returned buffer content %s\n",
		retval ? "true" : "false", GetLastError(), exact, buffer);
	}
	return;
    }
    SetLastError(ERROR_SUCCESS);
    retval = pGetDefaultPrinterA(NULL, NULL); 
    ok( !retval, "function result wrong! False expected\n");
    ok( ERROR_INVALID_PARAMETER == GetLastError(),
	"Last error wrong! ERROR_INVALID_PARAMETER expected, got 0x%08x\n",
	GetLastError());

    SetLastError(ERROR_SUCCESS);
    retval = pGetDefaultPrinterA(buffer, NULL); 
    ok( !retval, "function result wrong! False expected\n");
    ok( ERROR_INVALID_PARAMETER == GetLastError(),
	"Last error wrong! ERROR_INVALID_PARAMETER expected, got 0x%08x\n",
	GetLastError());

    SetLastError(ERROR_SUCCESS);
    size = 0;
    retval = pGetDefaultPrinterA(NULL, &size); 
    ok( !retval, "function result wrong! False expected\n");
    ok( ERROR_INSUFFICIENT_BUFFER == GetLastError(),
	"Last error wrong! ERROR_INSUFFICIENT_BUFFER expected, got 0x%08x\n",
	GetLastError());
    ok( size == exact, "Parameter size wrong! %d expected got %d\n",
	exact, size);

    SetLastError(ERROR_SUCCESS);
    size = DEFAULT_PRINTER_SIZE;
    retval = pGetDefaultPrinterA(NULL, &size); 
    ok( !retval, "function result wrong! False expected\n");
    ok( ERROR_INSUFFICIENT_BUFFER == GetLastError(),
	"Last error wrong! ERROR_INSUFFICIENT_BUFFER expected, got 0x%08x\n",
	GetLastError());
    ok( size == exact, "Parameter size wrong! %d expected got %d\n",
	exact, size);

    size = 0;
    retval = pGetDefaultPrinterA(buffer, &size); 
    ok( !retval, "function result wrong! False expected\n");
    ok( ERROR_INSUFFICIENT_BUFFER == GetLastError(),
	"Last error wrong! ERROR_INSUFFICIENT_BUFFER expected, got 0x%08x\n",
	GetLastError());
    ok( size == exact, "Parameter size wrong! %d expected got %d\n",
	exact, size);

    size = exact;
    retval = pGetDefaultPrinterA(buffer, &size); 
    ok( retval, "function result wrong! True expected\n");
    ok( size == exact, "Parameter size wrong! %d expected got %d\n",
	exact, size);
}

static void test_GetPrinterDriverDirectory(void)
{
    LPBYTE      buffer = NULL;
    DWORD       cbBuf = 0, pcbNeeded = 0;
    BOOL        res;


    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA( NULL, NULL, 1, NULL, 0, &cbBuf);
    trace("first call returned 0x%04x, with %d: buffer size 0x%08x\n",
    res, GetLastError(), cbBuf);

    RETURN_ON_DEACTIVATED_SPOOLER(res)
    ok((res == 0) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
        "returned %d with lasterror=%d (expected '0' with "
        "ERROR_INSUFFICIENT_BUFFER)\n", res, GetLastError());

    if (!cbBuf) {
        skip("no valid buffer size returned\n");
        return;
    }

    buffer = HeapAlloc( GetProcessHeap(), 0, cbBuf*2);
    if (buffer == NULL)  return ;

    res = GetPrinterDriverDirectoryA(NULL, NULL, 1, buffer, cbBuf, &pcbNeeded);
    ok( res, "expected result != 0, got %d\n", res);
    ok( cbBuf == pcbNeeded, "pcbNeeded set to %d instead of %d\n",
                            pcbNeeded, cbBuf);

    res = GetPrinterDriverDirectoryA(NULL, NULL, 1, buffer, cbBuf*2, &pcbNeeded);
    ok( res, "expected result != 0, got %d\n", res);
    ok( cbBuf == pcbNeeded, "pcbNeeded set to %d instead of %d\n",
                            pcbNeeded, cbBuf);
 
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA( NULL, NULL, 1, buffer, cbBuf-1, &pcbNeeded);
    ok( !res , "expected result == 0, got %d\n", res);
    ok( cbBuf == pcbNeeded, "pcbNeeded set to %d instead of %d\n",
                            pcbNeeded, cbBuf);
    
    ok( ERROR_INSUFFICIENT_BUFFER == GetLastError(),
        "last error set to %d instead of ERROR_INSUFFICIENT_BUFFER\n",
        GetLastError());

/*
    Do not add the next test:
    XPsp2: crash in this app, when the spooler is not running 
    NT3.5: ERROR_INVALID_USER_BUFFER
    win9x: ERROR_INVALID_PARAMETER

    pcbNeeded = MAGIC_DEAD;
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA( NULL, NULL, 1, NULL, cbBuf, &pcbNeeded);
*/

    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA( NULL, NULL, 1, buffer, cbBuf, NULL);
    ok( (!res && RPC_X_NULL_REF_POINTER == GetLastError()) || res,
         "expected either result == 0 and "
         "last error == RPC_X_NULL_REF_POINTER or result != 0 "
         "got result %d and last error == %d\n", res, GetLastError());

    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA( NULL, NULL, 1, NULL, cbBuf, NULL);
    ok(res || (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %d (expected '!=0' or '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError());
 
 
    /* with a valid buffer, but level is too large */
    buffer[0] = '\0';
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(NULL, NULL, 2, buffer, cbBuf, &pcbNeeded);

    /* Level not checked in win9x and wine:*/
    if((res != FALSE) && buffer[0])
    {
        trace("Level '2' not checked '%s'\n", buffer);
    }
    else
    {
        ok( !res && (GetLastError() == ERROR_INVALID_LEVEL),
        "returned %d with lasterror=%d (expected '0' with "
        "ERROR_INVALID_LEVEL)\n", res, GetLastError());
    }

    /* printing environments are case insensitive */
    /* "Windows 4.0" is valid for win9x and NT */
    buffer[0] = '\0';
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(NULL, env_win9x_case, 1, 
                                        buffer, cbBuf*2, &pcbNeeded);

    if(!res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        cbBuf = pcbNeeded;
        buffer = HeapReAlloc(GetProcessHeap(), 0, buffer, cbBuf*2);
        if (buffer == NULL)  return ;

        SetLastError(MAGIC_DEAD);
        res = GetPrinterDriverDirectoryA(NULL, env_win9x_case, 1, 
                                        buffer, cbBuf*2, &pcbNeeded);
    }

    ok(res && buffer[0], "returned %d with "
        "lasterror=%d and len=%d (expected '1' with 'len > 0')\n", 
        res, GetLastError(), lstrlenA((char *)buffer));

    buffer[0] = '\0';
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(NULL, env_x86, 1, 
                                        buffer, cbBuf*2, &pcbNeeded);

    if(!res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        cbBuf = pcbNeeded;
        buffer = HeapReAlloc(GetProcessHeap(), 0, buffer, cbBuf*2);
        if (buffer == NULL)  return ;

        buffer[0] = '\0';
        SetLastError(MAGIC_DEAD);
        res = GetPrinterDriverDirectoryA(NULL, env_x86, 1, 
                                        buffer, cbBuf*2, &pcbNeeded);
    }

    /* "Windows NT x86" is invalid for win9x */
    ok( (res && buffer[0]) ||
        (!res && (GetLastError() == ERROR_INVALID_ENVIRONMENT)), 
        "returned %d with lasterror=%d and len=%d (expected '!= 0' with "
        "'len > 0' or '0' with ERROR_INVALID_ENVIRONMENT)\n",
        res, GetLastError(), lstrlenA((char *)buffer));

    /* A setup program (PDFCreator_0.8.0) use empty strings */
    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(empty, empty, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError() );

    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(NULL, empty, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError() );

    SetLastError(MAGIC_DEAD);
    res = GetPrinterDriverDirectoryA(empty, NULL, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError() );

    HeapFree( GetProcessHeap(), 0, buffer);
}

/* ##### */

static void test_GetPrintProcessorDirectory(void)
{
    LPBYTE      buffer = NULL;
    DWORD       cbBuf = 0;
    DWORD       pcbNeeded = 0;
    BOOL        res;


    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, NULL, 1, NULL, 0, &cbBuf);
    /* The deactivated Spooler is caught here on NT3.51 */
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
        "returned %d with %d (expected '0' with ERROR_INSUFFICIENT_BUFFER)\n",
        res, GetLastError());

    buffer = HeapAlloc(GetProcessHeap(), 0, cbBuf*2);
    if(buffer == NULL)  return;

    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, NULL, 1, buffer, cbBuf, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    SetLastError(0xdeadbeef);
    buffer[0] = '\0';
    res = GetPrintProcessorDirectoryA(NULL, NULL, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());
 
    /* Buffer to small */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA( NULL, NULL, 1, buffer, cbBuf-1, &pcbNeeded);
    ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER),
        "returned %d with %d (expected '0' with ERROR_INSUFFICIENT_BUFFER)\n",
        res, GetLastError());

    if (0)
    {
    /* XPsp2: the program will crash here, when the spooler is not running  */
    /*        GetPrinterDriverDirectory has the same bug */
    pcbNeeded = 0;
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA( NULL, NULL, 1, NULL, cbBuf, &pcbNeeded);
    }

    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA( NULL, NULL, 1, buffer, cbBuf, NULL);
    /* NT: RPC_X_NULL_REF_POINTER, 9x: res != 0  */
    ok( res || (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %d (expected '!= 0' or '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError());


    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA( NULL, NULL, 1, NULL, cbBuf, NULL);
    /* NT: RPC_X_NULL_REF_POINTER, 9x: res != 0  */
    ok( res || (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %d (expected '!= 0' or '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError());

 
    /* with a valid buffer, but level is invalid */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, NULL, 2, buffer, cbBuf, &pcbNeeded);
    if (res && buffer[0])
    {
        /* Level is ignored in win9x*/
        trace("invalid level (2) was ignored\n");
    }
    else
    {
        ok( !res && (GetLastError() == ERROR_INVALID_LEVEL),
            "returned %d with %d (expected '0' with ERROR_INVALID_LEVEL)\n",
            res, GetLastError());
    }

    /* Empty environment is the same as the default environment */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, empty, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    /* "Windows 4.0" is valid for win9x and NT */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, env_win9x_case, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());


    /* "Windows NT x86" is invalid for win9x */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, env_x86, 1, buffer, cbBuf*2, &pcbNeeded);
    ok( res || (GetLastError() == ERROR_INVALID_ENVIRONMENT), 
        "returned %d with %d (expected '!= 0' or '0' with "
        "ERROR_INVALID_ENVIRONMENT)\n", res, GetLastError());

    /* invalid on all Systems */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(NULL, invalid_env, 1, buffer, cbBuf*2, &pcbNeeded);
    ok( !res && (GetLastError() == ERROR_INVALID_ENVIRONMENT), 
        "returned %d with %d (expected '0' with ERROR_INVALID_ENVIRONMENT)\n",
        res, GetLastError());

    /* Empty servername is the same as the local computer */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(empty, NULL, 1, buffer, cbBuf*2, &pcbNeeded);
    ok(res, "returned %d with %d (expected '!= 0')\n", res, GetLastError());

    /* invalid on all Systems */
    buffer[0] = '\0';
    SetLastError(0xdeadbeef);
    res = GetPrintProcessorDirectoryA(server_does_not_exist, NULL, 1, buffer, cbBuf*2, &pcbNeeded);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER), 
        "returned %d with %d (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());

    HeapFree(GetProcessHeap(), 0, buffer);
}

/* ##### */

static void test_OpenPrinter(void)
{
    PRINTER_DEFAULTSA   defaults;
    HANDLE              hprinter;
    DWORD               res;

    SetLastError(MAGIC_DEAD);
    res = OpenPrinter(NULL, NULL, NULL);    
    /* The deactivated Spooler is caught here on NT3.51 */
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    ok(!res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "returned %d with %d (expected '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());


    /* Get Handle for the local Printserver (NT only)*/
    hprinter = (HANDLE) MAGIC_DEAD;
    SetLastError(MAGIC_DEAD);
    res = OpenPrinter(NULL, &hprinter, NULL);
    /* The deactivated Spooler is caught here on XPsp2 */
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    ok(res || (!res && GetLastError() == ERROR_INVALID_PARAMETER),
        "returned %d with %d (expected '!=0' or '0' with ERROR_INVALID_PARAMETER)\n",
        res, GetLastError());
    if(res) {
        ClosePrinter(hprinter);

        defaults.pDatatype=NULL;
        defaults.pDevMode=NULL;

        defaults.DesiredAccess=0;
        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(NULL, &hprinter, &defaults);
        ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());
        if (res) ClosePrinter(hprinter);

        defaults.DesiredAccess=-1;
        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(NULL, &hprinter, &defaults);
        todo_wine {
        ok(!res && GetLastError() == ERROR_ACCESS_DENIED,
            "returned %d with %d (expected '0' with ERROR_ACCESS_DENIED)\n", 
            res, GetLastError());
        }
        if (res) ClosePrinter(hprinter);

    }


    if (local_server != NULL) {
        hprinter = (HANDLE) 0xdeadbeef;
        SetLastError(0xdeadbeef);
        res = OpenPrinter(local_server, &hprinter, NULL);
        ok(res || (!res && GetLastError() == ERROR_INVALID_PARAMETER),
            "returned %d with %d (expected '!=0' or '0' with ERROR_INVALID_PARAMETER)\n",
            res, GetLastError());
        if(res) ClosePrinter(hprinter);
    }

    /* Invalid Printername */
    hprinter = (HANDLE) MAGIC_DEAD;
    SetLastError(MAGIC_DEAD);
    res = OpenPrinter(illegal_name, &hprinter, NULL);
    ok(!res && ((GetLastError() == ERROR_INVALID_PRINTER_NAME) || 
                (GetLastError() == ERROR_INVALID_PARAMETER) ),
       "returned %d with %d (expected '0' with: ERROR_INVALID_PARAMETER or"
       "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());
    if(res) ClosePrinter(hprinter);

    hprinter = (HANDLE) MAGIC_DEAD;
    SetLastError(MAGIC_DEAD);
    res = OpenPrinter(empty, &hprinter, NULL);
    /* NT: ERROR_INVALID_PRINTER_NAME,  9x: ERROR_INVALID_PARAMETER */
    ok( !res &&
        ((GetLastError() == ERROR_INVALID_PRINTER_NAME) || 
        (GetLastError() == ERROR_INVALID_PARAMETER) ),
        "returned %d with %d (expected '0' with: ERROR_INVALID_PRINTER_NAME"
        " or ERROR_INVALID_PARAMETER)\n", res, GetLastError());
    if(res) ClosePrinter(hprinter);


    /* Get Handle for the default Printer */
    if (default_printer)
    {
        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, &hprinter, NULL);
        if((!res) && (GetLastError() == RPC_S_SERVER_UNAVAILABLE))
        {
            trace("The Service 'Spooler' is required for '%s'\n", default_printer);
            return;
        }
        ok(res, "returned %d with %d (expected '!=0')\n", res, GetLastError());
        if(res) ClosePrinter(hprinter);

        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, NULL, NULL);
        /* NT: FALSE with ERROR_INVALID_PARAMETER, 9x: TRUE */
        ok(res || (GetLastError() == ERROR_INVALID_PARAMETER),
            "returned %d with %d (expected '!=0' or '0' with "
            "ERROR_INVALID_PARAMETER)\n", res, GetLastError());

        defaults.pDatatype=NULL;
        defaults.pDevMode=NULL;
        defaults.DesiredAccess=0;

        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, &hprinter, &defaults);
        ok(res || GetLastError() == ERROR_ACCESS_DENIED,
            "returned %d with %d (expected '!=0' or '0' with "
            "ERROR_ACCESS_DENIED)\n", res, GetLastError());
        if(res) ClosePrinter(hprinter);

        defaults.pDatatype = empty;

        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, &hprinter, &defaults);
        /* stop here, when a remote Printserver has no RPC-Service running */
        RETURN_ON_DEACTIVATED_SPOOLER(res)
        ok(res || ((GetLastError() == ERROR_INVALID_DATATYPE) ||
                   (GetLastError() == ERROR_ACCESS_DENIED)),
            "returned %d with %d (expected '!=0' or '0' with: "
            "ERROR_INVALID_DATATYPE or ERROR_ACCESS_DENIED)\n",
            res, GetLastError());
        if(res) ClosePrinter(hprinter);


        defaults.pDatatype=NULL;
        defaults.DesiredAccess=PRINTER_ACCESS_USE;

        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, &hprinter, &defaults);
        ok(res || GetLastError() == ERROR_ACCESS_DENIED,
            "returned %d with %d (expected '!=0' or '0' with "
            "ERROR_ACCESS_DENIED)\n", res, GetLastError());
        if(res) ClosePrinter(hprinter);


        defaults.DesiredAccess=PRINTER_ALL_ACCESS;
        hprinter = (HANDLE) MAGIC_DEAD;
        SetLastError(MAGIC_DEAD);
        res = OpenPrinter(default_printer, &hprinter, &defaults);
        ok(res || GetLastError() == ERROR_ACCESS_DENIED,
            "returned %d with %d (expected '!=0' or '0' with "
            "ERROR_ACCESS_DENIED)\n", res, GetLastError());
        if(res) ClosePrinter(hprinter);
    }

}


static void test_SetDefaultPrinter(void)
{
    DWORD   res;
    DWORD   size = DEFAULT_PRINTER_SIZE;
    CHAR    buffer[DEFAULT_PRINTER_SIZE];
    CHAR    org_value[DEFAULT_PRINTER_SIZE];


    if (!pSetDefaultPrinterA)  return;
	/* only supported on win2k and above */

    /* backup the original value */
    org_value[0] = '\0';
    SetLastError(MAGIC_DEAD);
    res = GetProfileStringA("windows", "device", NULL, org_value, size);

    /* first part: with the default Printer */
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA("no_printer_with_this_name");

    RETURN_ON_DEACTIVATED_SPOOLER(res)
    /* spooler is running or we have no spooler here*/

    /* Not implemented in wine */
    if (!res && (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)) {
        trace("SetDefaultPrinterA() not implemented yet.\n");
        return;
    }

    ok(!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME),
        "returned %d with %d (expected '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", org_value);
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA("");
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
        "returned %d with %d (expected '!=0' or '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", org_value);
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA(NULL);
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
        "returned %d with %d (expected '!=0' or '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", org_value);
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA(default_printer);
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
        "returned %d with %d (expected '!=0' or '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());


    /* second part: always without a default Printer */
    WriteProfileStringA("windows", "device", NULL);    
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA("no_printer_with_this_name");

    ok(!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME),
        "returned %d with %d (expected '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", NULL);    
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA("");
    /* we get ERROR_INVALID_PRINTER_NAME when no printer is installed */
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
         "returned %d with %d (expected '!=0' or '0' with "
         "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", NULL);    
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA(NULL);
    /* we get ERROR_INVALID_PRINTER_NAME when no printer is installed */
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
        "returned %d with %d (expected '!=0' or '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    WriteProfileStringA("windows", "device", NULL);    
    SetLastError(MAGIC_DEAD);
    res = pSetDefaultPrinterA(default_printer);
    ok(res || (!res && (GetLastError() == ERROR_INVALID_PRINTER_NAME)),
        "returned %d with %d (expected '!=0' or '0' with "
        "ERROR_INVALID_PRINTER_NAME)\n", res, GetLastError());

    /* restore the original value */
    res = pSetDefaultPrinterA(default_printer);          /* the nice way */
    WriteProfileStringA("windows", "device", org_value); /* the old way */

    buffer[0] = '\0';
    SetLastError(MAGIC_DEAD);
    res = GetProfileStringA("windows", "device", NULL, buffer, size);
    ok(!lstrcmpA(org_value, buffer), "'%s' (expected '%s')\n", buffer, org_value);

}

/* ########################### */

static void test_XcvDataW_MonitorUI(void)
{
    DWORD   res;
    HANDLE  hXcv;
    BYTE    buffer[MAX_PATH + 4];
    DWORD   needed;
    DWORD   status;
    DWORD   len;
    PRINTER_DEFAULTSA pd;

    /* api is not present before w2k */
    if (pXcvDataW == NULL) return;

    pd.pDatatype = NULL;
    pd.pDevMode  = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

    hXcv = NULL;
    SetLastError(0xdeadbeef);
    res = OpenPrinter(xcv_localport, &hXcv, &pd);
    RETURN_ON_DEACTIVATED_SPOOLER(res)
    RETURN_ON_ACCESS_DENIED(res)

    ok(res, "returned %d with %u and handle %p (expected '!= 0')\n", res, GetLastError(), hXcv);
    if (!res) return;

    /* ask for needed size */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, NULL, 0, &needed, &status);
    ok( res && (status == ERROR_INSUFFICIENT_BUFFER) && (needed <= MAX_PATH),
        "returned %d with %u and %u for status %u (expected '!= 0' and "
        "'<= MAX_PATH' for status ERROR_INSUFFICIENT_BUFFER)\n",
        res, GetLastError(), needed, status);

    if (needed > MAX_PATH) {
        ClosePrinter(hXcv);
        skip("buffer overflow (%u)\n", needed);
        return;
    }
    len = needed;       /* Size is in bytes */

    /* the command is required */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, emptyW, NULL, 0, NULL, 0, &needed, &status);
    ok( res && (status == ERROR_INVALID_PARAMETER),
        "returned %d with %u and %u for status %u (expected '!= 0' with "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, NULL, NULL, 0, buffer, MAX_PATH, &needed, &status);
    ok( !res && (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %u and %u for status %u (expected '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError(), needed, status);

    /* "PDWORD needed" is checked before RPC-Errors */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, buffer, len, NULL, &status);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "returned %d with %u and %u for status %u (expected '0' with "
        "ERROR_INVALID_PARAMETER)\n", res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, NULL, len, &needed, &status);
    ok( !res && (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %u and %u for status %u (expected '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, buffer, len, &needed, NULL);
    ok( !res && (GetLastError() == RPC_X_NULL_REF_POINTER),
        "returned %d with %u and %u for status %u (expected '0' with "
        "RPC_X_NULL_REF_POINTER)\n", res, GetLastError(), needed, status);

    /* off by one: larger  */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, buffer, len+1, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for status "
        "ERROR_SUCCESS)\n", res, GetLastError(), needed, status);

    /* off by one: smaller */
    /* the buffer is not modified for NT4, w2k, XP */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, buffer, len-1, &needed, &status);
    ok( res && (status == ERROR_INSUFFICIENT_BUFFER),
        "returned %d with %u and %u for status %u (expected '!= 0' for status "
        "ERROR_INSUFFICIENT_BUFFER)\n", res, GetLastError(), needed, status);


    /* Normal use. The DLL-Name without a Path is returned */
    memset(buffer, 0, len);
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_MonitorUIW, NULL, 0, buffer, len, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for status "
        "ERROR_SUCCESS)\n", res, GetLastError(), needed, status);

    ClosePrinter(hXcv);
}

/* ########################### */

static void test_XcvDataW_PortIsValid(void)
{
    DWORD   res;
    HANDLE  hXcv;
    DWORD   needed;
    DWORD   status;
    PRINTER_DEFAULTSA   pd;

    /* api is not present before w2k */
    if (pXcvDataW == NULL) return;

    pd.pDatatype = NULL;
    pd.pDevMode  = NULL;
    pd.DesiredAccess = SERVER_ACCESS_ADMINISTER;

    hXcv = NULL;
    SetLastError(0xdeadbeef);
    res = OpenPrinter(xcv_localport, &hXcv, &pd);

    RETURN_ON_DEACTIVATED_SPOOLER(res)
    RETURN_ON_ACCESS_DENIED(res)

    ok(res, "returned %d with %u and handle %p (expected '!= 0')\n", res, GetLastError(), hXcv);
    if (!res) return;


    /* "PDWORD needed" is always required */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_lpt1W, sizeof(portname_lpt1W), NULL, 0, NULL, &status);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "returned %d with %u and %u for status %u (expected '!= 0' with ERROR_INVALID_PARAMETER)\n",
         res, GetLastError(), needed, status);

    /* an empty name is not allowed */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) emptyW, sizeof(emptyW), NULL, 0, &needed, &status);
    ok( res && ((status == ERROR_FILE_NOT_FOUND) || (status == ERROR_PATH_NOT_FOUND)),
        "returned %d with %u and %u for status %u (expected '!= 0' for status: "
        "ERROR_FILE_NOT_FOUND or ERROR_PATH_NOT_FOUND)\n",
        res, GetLastError(), needed, status);

    /* a directory is not allowed */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) tempdirW, (lstrlenW(tempdirW) + 1) * sizeof(WCHAR), NULL, 0, &needed, &status);
    /* XP: ERROR_PATH_NOT_FOUND, w2k ERROR_ACCESS_DENIED */
    ok( res && ((status == ERROR_PATH_NOT_FOUND) || (status == ERROR_ACCESS_DENIED)),
        "returned %d with %u and %u for status %u (expected '!= 0' for status: "
        "ERROR_PATH_NOT_FOUND or ERROR_ACCESS_DENIED)\n",
        res, GetLastError(), needed, status);

    /* more valid well known Ports */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_lpt1W, sizeof(portname_lpt1W), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_lpt2W, sizeof(portname_lpt2W), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_com1W, sizeof(portname_com1W), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_com2W, sizeof(portname_com2W), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' for ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);

    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) portname_fileW, sizeof(portname_fileW), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' with  ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);


    /* a normal, writable file is allowed */
    needed = (DWORD) 0xdeadbeef;
    status = (DWORD) 0xdeadbeef;
    SetLastError(0xdeadbeef);
    res = pXcvDataW(hXcv, cmd_PortIsValidW, (PBYTE) tempfileW, (lstrlenW(tempfileW) + 1) * sizeof(WCHAR), NULL, 0, &needed, &status);
    ok( res && (status == ERROR_SUCCESS),
        "returned %d with %u and %u for status %u (expected '!= 0' with ERROR_SUCCESS)\n",
        res, GetLastError(), needed, status);

    ClosePrinter(hXcv);
}

/* ########################### */

static void test_GetPrinterDriver(void)
{
    HANDLE hprn;
    BOOL ret;
    BYTE *buf;
    INT level;
    DWORD needed, filled;

    if (!default_printer)
    {
        skip("There is no default printer installed\n");
        return;
    }

    hprn = 0;
    ret = OpenPrinter(default_printer, &hprn, NULL);
    if (!ret)
    {
        skip("Unable to open the default printer (%s)\n", default_printer);
        return;
    }
    ok(hprn != 0, "wrong hprn %p\n", hprn);

    for (level = -1; level <= 7; level++)
    {
        SetLastError(0xdeadbeef);
        needed = (DWORD)-1;
        ret = GetPrinterDriver(hprn, NULL, level, NULL, 0, &needed);
        ok(!ret, "level %d: GetPrinterDriver should fail\n", level);
        if (level >= 1 && level <= 6)
        {
            /* Not all levels are supported on all Windows-Versions */
            if(GetLastError() == ERROR_INVALID_LEVEL) continue;
            ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "wrong error %d\n", GetLastError());
            ok(needed > 0,"not expected needed buffer size %d\n", needed);
        }
        else
        {
            /* ERROR_OUTOFMEMORY found on win9x */
            ok( ((GetLastError() == ERROR_INVALID_LEVEL) ||
                 (GetLastError() == ERROR_OUTOFMEMORY)),
                "%d: returned %d with %d (expected '0' with: "
                "ERROR_INVALID_LEVEL or ERROR_OUTOFMEMORY)\n",
                level, ret, GetLastError());
            /* needed is modified in win9x. The modified Value depends on the
               default Printer. testing for "needed == (DWORD)-1" will fail */
            continue;
        }

        buf = HeapAlloc(GetProcessHeap(), 0, needed);

        SetLastError(0xdeadbeef);
        filled = -1;
        ret = GetPrinterDriver(hprn, NULL, level, buf, needed, &filled);
        ok(ret, "level %d: GetPrinterDriver error %d\n", level, GetLastError());
        ok(needed == filled, "needed %d != filled %d\n", needed, filled);

        if (level == 2)
        {
            DRIVER_INFO_2 *di_2 = (DRIVER_INFO_2 *)buf;
            DWORD calculated = sizeof(*di_2);

            /* MSDN is wrong: The Drivers on the win9x-CD's have cVersion=0x0400
               NT351: 1, NT4.0+w2k(Kernelmode): 2, w2k and above(Usermode): 3  */
            ok( (di_2->cVersion <= 3) ||
                (di_2->cVersion == 0x0400), "di_2->cVersion = %d\n", di_2->cVersion);
            ok(di_2->pName != NULL, "not expected NULL ptr\n");
            ok(di_2->pEnvironment != NULL, "not expected NULL ptr\n");
            ok(di_2->pDriverPath != NULL, "not expected NULL ptr\n");
            ok(di_2->pDataFile != NULL, "not expected NULL ptr\n");
            ok(di_2->pConfigFile != NULL, "not expected NULL ptr\n");

            trace("cVersion %d\n", di_2->cVersion);
            trace("pName %s\n", di_2->pName);
            calculated += strlen(di_2->pName) + 1;
            trace("pEnvironment %s\n", di_2->pEnvironment);
            calculated += strlen(di_2->pEnvironment) + 1;
            trace("pDriverPath %s\n", di_2->pDriverPath);
            calculated += strlen(di_2->pDriverPath) + 1;
            trace("pDataFile %s\n", di_2->pDataFile);
            calculated += strlen(di_2->pDataFile) + 1;
            trace("pConfigFile %s\n", di_2->pConfigFile);
            calculated += strlen(di_2->pConfigFile) + 1;

            /* XP allocates memory for both ANSI and unicode names */
            ok(filled >= calculated,"calculated %d != filled %d\n", calculated, filled);
        }

        HeapFree(GetProcessHeap(), 0, buf);
    }

    SetLastError(0xdeadbeef);
    ret = ClosePrinter(hprn);
    ok(ret, "ClosePrinter error %d\n", GetLastError());
}

static void test_DEVMODE(const DEVMODE *dm, LONG dmSize, LPCSTR exp_prn_name)
{
    /* On NT3.51, some fields in DEVMODE are empty/zero
      (dmDeviceName, dmSpecVersion, dmDriverVersion and dmDriverExtra)
       We skip the Tests on this Platform */
    if (dm->dmSpecVersion || dm->dmDriverVersion || dm->dmDriverExtra) {
    /* The 0-terminated Printername can be larger (MAX_PATH) than CCHDEVICENAME */
        ok(!strncmp(exp_prn_name, (LPCSTR)dm->dmDeviceName, CCHDEVICENAME -1),
            "expected '%s', got '%s'\n", exp_prn_name, dm->dmDeviceName);
        ok(dm->dmSize + dm->dmDriverExtra == dmSize,
            "%u != %d\n", dm->dmSize + dm->dmDriverExtra, dmSize);
    }
    trace("dmFields %08x\n", dm->dmFields);
}

static void test_DocumentProperties(void)
{
    HANDLE hprn;
    LONG dm_size, ret;
    DEVMODE *dm;

    if (!default_printer)
    {
        skip("There is no default printer installed\n");
        return;
    }

    hprn = 0;
    ret = OpenPrinter(default_printer, &hprn, NULL);
    if (!ret)
    {
        skip("Unable to open the default printer (%s)\n", default_printer);
        return;
    }
    ok(hprn != 0, "wrong hprn %p\n", hprn);

    dm_size = DocumentProperties(0, hprn, NULL, NULL, NULL, 0);
    trace("DEVMODE required size %d\n", dm_size);
    ok(dm_size >= sizeof(DEVMODE), "unexpected DocumentProperties ret value %d\n", dm_size);

    dm = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dm_size);

    ret = DocumentProperties(0, hprn, NULL, dm, dm, DM_OUT_BUFFER);
    ok(ret == IDOK, "DocumentProperties ret value %d != expected IDOK\n", ret);

    test_DEVMODE(dm, dm_size, default_printer);

    HeapFree(GetProcessHeap(), 0, dm);

    SetLastError(0xdeadbeef);
    ret = ClosePrinter(hprn);
    ok(ret, "ClosePrinter error %d\n", GetLastError());
}

static void test_EnumPrinters(void)
{
    DWORD neededA, neededW, num;
    DWORD ret;

    SetLastError(0xdeadbeef);
    neededA = -1;
    ret = EnumPrintersA(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &neededA, &num);
    RETURN_ON_DEACTIVATED_SPOOLER(ret)
    if (!ret)
    {
        /* We have 1 or more printers */
        ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "gle %d\n", GetLastError());
        ok(neededA > 0, "Expected neededA to show the number of needed bytes\n");
    }
    else
    {
        /* We don't have any printers defined */
        ok(GetLastError() == S_OK, "gle %d\n", GetLastError());
        ok(neededA == 0, "Expected neededA to be zero\n");
    }
    ok(num == 0, "num %d\n", num);

    SetLastError(0xdeadbeef);
    neededW = -1;
    ret = EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &neededW, &num);
    /* EnumPrintersW is not supported on all platforms */
    if (!ret && (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED))
    {
        skip("EnumPrintersW is not implemented\n");
        return;
    }

    if (!ret)
    {
        /* We have 1 or more printers */
        ok(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "gle %d\n", GetLastError());
        ok(neededW > 0, "Expected neededW to show the number of needed bytes\n");
    }
    else
    {
        /* We don't have any printers defined */
        ok(GetLastError() == S_OK, "gle %d\n", GetLastError());
        ok(neededW == 0, "Expected neededW to be zero\n");
    }
    ok(num == 0, "num %d\n", num);

    /* Outlook2003 relies on the buffer size returned by EnumPrintersA being big enough
       to hold the buffer returned by EnumPrintersW */
    ok(neededA == neededW, "neededA %d neededW %d\n", neededA, neededW);
}

static void test_DeviceCapabilities(void)
{
    HANDLE hComdlg32;
    BOOL (WINAPI *pPrintDlgA)(PRINTDLGA *);
    PRINTDLGA prn_dlg;
    DEVMODE *dm;
    DEVNAMES *dn;
    const char *driver, *device, *port;
    WORD *papers;
    POINT *paper_size;
    POINTS ext;
    struct
    {
        char name[64];
    } *paper_name;
    INT n_papers, n_paper_size, n_paper_names, n_copies, ret;
    DWORD fields;

    hComdlg32 = LoadLibrary("comdlg32.dll");
    assert(hComdlg32);
    pPrintDlgA = (void *)GetProcAddress(hComdlg32, "PrintDlgA");
    assert(pPrintDlgA);

    memset(&prn_dlg, 0, sizeof(prn_dlg));
    prn_dlg.lStructSize = sizeof(prn_dlg);
    prn_dlg.Flags = PD_RETURNDEFAULT;
    ret = pPrintDlgA(&prn_dlg);
    FreeLibrary(hComdlg32);
    if (!ret)
    {
        skip("PrintDlg returned no default printer\n");
        return;
    }
    ok(prn_dlg.hDevMode != 0, "PrintDlg returned hDevMode == NULL\n");
    ok(prn_dlg.hDevNames != 0, "PrintDlg returned hDevNames == NULL\n");

    dm = GlobalLock(prn_dlg.hDevMode);
    ok(dm != NULL, "GlobalLock(prn_dlg.hDevMode) failed\n");
    trace("dmDeviceName \"%s\"\n", dm->dmDeviceName);

    dn = GlobalLock(prn_dlg.hDevNames);
    ok(dn != NULL, "GlobalLock(prn_dlg.hDevNames) failed\n");
    ok(dn->wDriverOffset, "expected not 0 wDriverOffset\n");
    ok(dn->wDeviceOffset, "expected not 0 wDeviceOffset\n");
    ok(dn->wOutputOffset, "expected not 0 wOutputOffset\n");
    ok(dn->wDefault == DN_DEFAULTPRN, "expected DN_DEFAULTPRN got %x\n", dn->wDefault);
    driver = (const char *)dn + dn->wDriverOffset;
    device = (const char *)dn + dn->wDeviceOffset;
    port = (const char *)dn + dn->wOutputOffset;
    trace("driver \"%s\" device \"%s\" port \"%s\"\n", driver, device, port);

    test_DEVMODE(dm, dm->dmSize + dm->dmDriverExtra, device);

    n_papers = DeviceCapabilities(device, port, DC_PAPERS, NULL, NULL);
    ok(n_papers > 0, "DeviceCapabilities DC_PAPERS failed\n");
    papers = HeapAlloc(GetProcessHeap(), 0, sizeof(*papers) * n_papers);
    ret = DeviceCapabilities(device, port, DC_PAPERS, (LPSTR)papers, NULL);
    ok(ret == n_papers, "expected %d, got %d\n", n_papers, ret);
#if VERBOSE
    for (ret = 0; ret < n_papers; ret++)
        trace("papers[%d] = %d\n", ret, papers[ret]);
#endif
    HeapFree(GetProcessHeap(), 0, papers);

    n_paper_size = DeviceCapabilities(device, port, DC_PAPERSIZE, NULL, NULL);
    ok(n_paper_size > 0, "DeviceCapabilities DC_PAPERSIZE failed\n");
    ok(n_paper_size == n_papers, "n_paper_size %d != n_papers %d\n", n_paper_size, n_papers);
    paper_size = HeapAlloc(GetProcessHeap(), 0, sizeof(*paper_size) * n_paper_size);
    ret = DeviceCapabilities(device, port, DC_PAPERSIZE, (LPSTR)paper_size, NULL);
    ok(ret == n_paper_size, "expected %d, got %d\n", n_paper_size, ret);
#if VERBOSE
    for (ret = 0; ret < n_paper_size; ret++)
        trace("paper_size[%d] = %d x %d\n", ret, paper_size[ret].x, paper_size[ret].y);
#endif
    HeapFree(GetProcessHeap(), 0, paper_size);

    n_paper_names = DeviceCapabilities(device, port, DC_PAPERNAMES, NULL, NULL);
    ok(n_paper_names > 0, "DeviceCapabilities DC_PAPERNAMES failed\n");
    ok(n_paper_names == n_papers, "n_paper_names %d != n_papers %d\n", n_paper_names, n_papers);
    paper_name = HeapAlloc(GetProcessHeap(), 0, sizeof(*paper_name) * n_paper_names);
    ret = DeviceCapabilities(device, port, DC_PAPERNAMES, (LPSTR)paper_name, NULL);
    ok(ret == n_paper_names, "expected %d, got %d\n", n_paper_names, ret);
#if VERBOSE
    for (ret = 0; ret < n_paper_names; ret++)
        trace("paper_name[%u] = %s\n", ret, paper_name[ret].name);
#endif
    HeapFree(GetProcessHeap(), 0, paper_name);

    n_copies = DeviceCapabilities(device, port, DC_COPIES, NULL, dm);
    ok(n_copies > 0, "DeviceCapabilities DC_COPIES failed\n");
    trace("n_copies = %d\n", n_copies);

    /* these capabilities not available on all printer drivers */
    if (0)
    {
        ret = DeviceCapabilities(device, port, DC_MAXEXTENT, NULL, NULL);
        ok(ret != -1, "DeviceCapabilities DC_MAXEXTENT failed\n");
        ext = MAKEPOINTS(ret);
        trace("max ext = %d x %d\n", ext.x, ext.y);

        ret = DeviceCapabilities(device, port, DC_MINEXTENT, NULL, NULL);
        ok(ret != -1, "DeviceCapabilities DC_MINEXTENT failed\n");
        ext = MAKEPOINTS(ret);
        trace("min ext = %d x %d\n", ext.x, ext.y);
    }

    fields = DeviceCapabilities(device, port, DC_FIELDS, NULL, NULL);
    ok(fields != (DWORD)-1, "DeviceCapabilities DC_FIELDS failed\n");
    ok(fields == dm->dmFields, "fields %x != dm->dmFields %x\n", fields, dm->dmFields);

    GlobalUnlock(prn_dlg.hDevMode);
    GlobalFree(prn_dlg.hDevMode);
    GlobalUnlock(prn_dlg.hDevNames);
    GlobalFree(prn_dlg.hDevNames);
}

START_TEST(info)
{
    hwinspool = GetModuleHandleA("winspool.drv");
    pGetDefaultPrinterA = (void *) GetProcAddress(hwinspool, "GetDefaultPrinterA");
    pSetDefaultPrinterA = (void *) GetProcAddress(hwinspool, "SetDefaultPrinterA");
    pXcvDataW = (void *) GetProcAddress(hwinspool, "XcvDataW");
    pAddPortExA = (void *) GetProcAddress(hwinspool, "AddPortExA");

    find_default_printer();
    find_local_server();
    find_tempfile();

    test_AddMonitor();
    test_AddPort();
    test_AddPortEx();
    test_ConfigurePort();
    test_DeleteMonitor();
    test_DeletePort();
    test_DeviceCapabilities();
    test_DocumentProperties();
    test_EnumForms(NULL);
    if (default_printer) test_EnumForms(default_printer);
    test_EnumMonitors();
    test_EnumPorts();
    test_EnumPrinterDrivers();
    test_EnumPrinters();
    test_GetDefaultPrinter();
    test_GetPrinterDriverDirectory();
    test_GetPrintProcessorDirectory();
    test_OpenPrinter();
    test_GetPrinterDriver();
    test_SetDefaultPrinter();
    test_XcvDataW_MonitorUI();
    test_XcvDataW_PortIsValid();

    /* Cleanup our temporary file */
    DeleteFileA(tempfileA);
}

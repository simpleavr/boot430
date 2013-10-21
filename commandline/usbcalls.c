/* Name: usbcalls.c
 * Project: usbcalls library
 * Author: Christian Starkjohann
 * Creation Date: 2006-02-02
 * Tabsize: 4
 * Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id$
 */

/* This file includes the appropriate implementation based on platform
 * specific defines.
 */

#if defined(WIN32)
#   include "usb-windows.c"
void uSleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#else
/* e.g. defined(__APPLE__) */
#   include "usb-libusb.c"
#endif

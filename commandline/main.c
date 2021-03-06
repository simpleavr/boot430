/*
 boot430loader

 chris chung 2013 January
 www.simpleavr.com

 . supports msp430g2553 only

 borrow heavily / derived from ..
 * Project: AVR bootloader HID
 * Author: Christian Starkjohann
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id$
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "usbcalls.h"

#define IDENT_VENDOR_NUM        0x16c0
#define IDENT_VENDOR_STRING     "simpleavr"
#define IDENT_PRODUCT_NUM       1503
#define IDENT_PRODUCT_STRING    "boot430"

//________________________________________________________________________________
char *usbErrorMessage(int errCode)
{
  static char buffer[80];

    switch(errCode){
        case USB_ERROR_ACCESS:      return "Access to device denied";
        case USB_ERROR_NOTFOUND:    return "The specified device was not found";
        case USB_ERROR_BUSY:        return "The device is used by another application";
        case USB_ERROR_IO:          return "Communication error with device";
        default:
            sprintf(buffer, "Unknown USB error %d", errCode);
            return buffer;
    }
    return NULL;    /* not reached */
}

//________________________________________________________________________________
int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf("\nusage: %s <intel-hexfile>\n\n", argv[0]);
  }//if

  usbDevice_t *dev = NULL;
  int err=0;
  //_______________________ try device
  if ((err = usbOpenDevice(&dev,
                           IDENT_VENDOR_NUM,
                           IDENT_VENDOR_STRING,
                           IDENT_PRODUCT_NUM,
                           IDENT_PRODUCT_STRING, 1)) != 0)
  {
    fprintf(stderr, "Error opening HIDBoot device: %s\n", usbErrorMessage(err));
  }//if

  char reportBuf[32] = { 0, 0, 0, 0, 0, 0, 0, 0, };
  union
  {
    char val[32];
    struct
    {
      unsigned char dummy;
      unsigned char report_id;
      unsigned char version;
      unsigned char release;
      unsigned short end_flash;
      unsigned short device;
    } info;
  } report;
  int len = sizeof(reportBuf);
  if (!err && (err = usbGetReport(dev, USB_HID_REPORT_TYPE_FEATURE, 1, report.val+1, &len)) != 0)
  {
    fprintf(stderr, "Error reading page size: %s\n", usbErrorMessage(err));
  }//if

  printf("\ndevice (%04x), version (%d.%d), application end at (0x%04x)\n\n",
         report.info.device, report.info.version, report.info.release, report.info.end_flash);

  int flash_start = 0xe000;   // default is 8k flash

  if (report.info.device >= 0x2500)
    flash_start = 0xc000; // device is 16k


  /* When using usbSetReport() with USB_HID_REPORT_TYPE_FEATURE, you will
     have to send a buffer with the following constraints :
      - the first byte must be the report id
      - the report id must be one of those declared by the hid descriptor (here, 1 or 2 -- see HID_Descriptor in bbusb_protocol.c)
      - the number of data bytes must be equal to the report count of chosen report id (here, 6 for report id 1, and 4 for report id 2 -- see HID_Descriptor in bbusb_protocol.c)
      - the data bytes must follow the report id
      - the total length of the buffer must be the data bytes + 1 for the report id

     For instance, we want to send 6 bytes 0xBA of data using report id 1. The hid descriptor
     tells us that it is possible (report id 1 exists and report count == 6 bytes), and the
     whole buffer will be as follows : 0x01 0xBA 0xBA 0xBA 0xBA 0xBA 0xBA . We will call :
     char buffer[7] = { 0x01, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA, 0xBA };
     usbSetReport(dev, USB_HID_REPORT_TYPE_FEATURE, buffer, 7);

    On the MCU device side, we will receive exactly this usb packet in DataPacketBuffer :
      // 0a 4b 01 ba ba ba ba ba ba c1 50
      // LL D1 RI d0 d1 d2 d3 d4 d5 C1 C2
    with
      LL : length (without the LL byte itself). Here we sent 6 bytes + 1 report id byte + 2 checksum bytes + DATA1 = 0a.
      D1 : DATA1 (0x4b) since using endpoint 1, else it would be DATA0 (0xc3) for endpoint 0.
      RI : report id
      d0..d5 : user-supplied data
      C1..C2 : CRC16 checksum
  */


  //_______________________ try open hex file
  if (!err && argc >= 2)
  {
    FILE *fp = fopen(argv[1], "r");
    if (fp)
    {
      int  addrStart=0, addrEnd=0;
      unsigned char data[16*1024];    // 16k flash space
      char line[80];
      union
      {
        unsigned char val[36];
        struct
        {
          unsigned char len;
          unsigned char addrH;
          unsigned char addrL;
          unsigned char type;
          unsigned char data[32];
        } set;
      } raw;

      memset(data, 0xff, sizeof(data));
      //___________________ take in file
      while (fgets(line, 80, fp))
      {
        //printf("read: %s", line);
        //:10E000005542200135D0085A8245040231400003B0
        char sav, *cp = line+1;
        int i=0;
        while (*cp>='0') {
          sav = *(cp+2);
          *(cp+2) = '\0';
          raw.val[i++] = (unsigned char) strtol(cp, NULL, 16);
          cp += 2;
          *cp = sav;
        }//while
        //_______________ load our big buffer
        if (!raw.set.type)
        {
          int offset = raw.set.addrH;
          offset *= 256;
          offset -= flash_start;
          if (offset < 0)
          {
            printf("\nfirmware out of range for device 0x%04x < 0x%04x\n\n", offset + flash_start, flash_start);
            addrStart = addrEnd = 0;
            break;
          }//if
          offset += raw.set.addrL;
          if (raw.set.addrH < 0xff)
          {
            if (offset <= addrStart)   addrStart = offset;
            if ((offset+raw.set.len) >  addrEnd) addrEnd = offset + raw.set.len;
          }//if
          cp = (char *) data + offset;
          printf("offset..%04x (%d)\n", offset, raw.set.len);
          for (i=0;i<raw.set.len;i++)
            *cp++ = raw.set.data[i];
        }//if

      }//while
      fclose(fp);

      addrStart += flash_start;
      addrEnd   += flash_start;

      printf("flash_start = 0x%04x\n", flash_start);
      printf("range.. 0x%04x -> 0x%04x (0x%04x)\n", addrStart, addrEnd, addrEnd-addrStart);
      #define TK 0x0a

      //___________________ now do flash
      if (addrEnd > addrStart)
      {
        char instr = 0x10;
        int  bcnt=0;
        while (addrStart < addrEnd)
        {
          char buf[16];
          int report_size = 0;
          int i=0;

          if (instr == 0x10) // command to send address
          {
            buf[i++] = 0x01; // flash write request (report id 1)
            buf[i++] = addrStart>>8;
            buf[i++] = addrStart&0xff;
            buf[i++] = addrEnd>>8;
            buf[i++] = addrEnd&0xff;
            report_size = 7;    // adjust length (report size + 1)
          }//if
          else
          {
            buf[0] = 0x02; // data bytes (report id 2)
            for (i=1;i<5;i++)
            {
              buf[i] = data[(addrStart++)-flash_start];
              printf(" %02x", (unsigned char) buf[i]);
              bcnt++;
            }//for
            report_size = 5; // adjust length (report size + 1)
          }//else

          printf(" (%03x)", bcnt);
          int relax = 0;
          if ((bcnt/32) || (instr == 0x10) || (addrStart>=addrEnd))
          {
            relax = 1;
            bcnt = bcnt % 32;
            printf("* %04x/%04x", addrStart, addrEnd);
            if(instr == 0x10)
              printf("      Erasing flash block...");
            else
              printf("      Writing data to flash memory...");
          }//if
          instr = TK;

          printf("\n");
          if (!err && (err = usbSetReport(dev, USB_HID_REPORT_TYPE_FEATURE, buf, report_size)) != 0)
          {
            //if (!relax) {
              printf("\n");
              fprintf(stderr, "Error uploading data block: %s\n", usbErrorMessage(err));
              break;
            //}//if
          }//if
          //uSleep(2500);
          uSleep(500);

          //~ boost::this_thread::sleep(boost::posix_time::microseconds(500*1000));
          if(buf[0] == 0x01 || relax) // flash erase request or receive buffer full,
            uSleep(500*1000);         // sleep a bit to allow the device to go into usb idle mode.
                                      // This sleep is IMPORTANT : it allows the device to switch
                                      // to usb idle mode where it erases/writes to flash.

          if (buf[0] == 0x40) break; // ???

          if (addrStart>=addrEnd)
          {
            if (addrStart<0xff80)
            {
              // done flashing code segment, now do interrupts
              //_____________________ we want the interrupt vector table relocated
              addrStart = 0xffa0;
              addrEnd = 0xffc0;
              memcpy(data+0xffa0-flash_start, data+0xffe0-flash_start, 0x20);
              printf("\ninterrupt vector\n");
              instr = 0x10;
              bcnt = 0;
            }//if
            else
            {
              instr = 0x40;
              //addrStart -= 8;
            }//else
            uSleep(100*1000);
          }//if
        }//while

      }//if

    }//if
    else {
      printf("cannot open file %s!\n", argv[1]);
    }//else
  }//if
  if (dev != NULL) usbCloseDevice(dev);

  return 0;
}

/* ------------------------------------------------------------------------- */



/*
 *  dvdzone
 *
 *	This program demonstrates PLScsi usage.
 *
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plscsi.h"

// Some fixes for windows
#define SENSE_LENGTH             0xE			/* offsetof SK ASC ASCQ < xE */

#define MAX_SECONDS              300

#if (_WIN32 || __MSDOS__)

#define NULL_FD fopen("NUL", "w")

#else

#define NULL_FD fopen("/dev/null", "w")

#endif



/* ------------------------------------------------------------------------
To avoid any cross platform problem, printf as "%ld" "%lu" or %ld" have
  been enforced, with operand being casted to [unsigned] long
   ------------------------------------------------------------------------ */

int allDevices;
int setRegion;
char name[1024] = {0};



int ProcessDevice(Scsi *scsi, char const *name);
int isDVD(Scsi *scsi);
char *regionString(unsigned char m);
int getInquiry(Scsi *scsi, char *vendor, char *model, char *revision);
int waitForReady(Scsi *scsi, int withMedium);
int getRPC(Scsi *scsi, char *buffer);
int getMovieMask(Scsi *scsi, char *movieMask, char *copyright);
void unpad(char *s);



/* ------------------------------------------------------------------------ */
void unpad(char *s)
{
size_t l;

l = strlen(s);
while (l--)
  {
  if (s[l-1] == ' ') s[l-1] = 0;
  else break;
  }
}



/* ------------------------------------------------------------------------ */
char *regionString(unsigned char m)
{
static char result[16];
int i;
int r;

m &= 0xff;

if (m == 0xff) return "none";

m = ~m;
i = 0;

for (r = 1; r <= 8; r++)
  {
  if (m & 1)
    {
    result[i++] = '0' + r;
    m &= ~1;
    if (!m) break;
    result[i++] = '+';
    }
  m = m >> 1;
  }

result[i] = 0;

return result;
}



/* ------------------------------------------------------------------------ */
int isDVD(Scsi *scsi)
{
char cdb[10];
unsigned char buffer[255];
int size;
INT result;

size = (sizeof(buffer) > 255) ? 255 : sizeof(buffer);

// Use MODE_SENSE with page 2A (MMC2 Capabilities and Mechanical Status Page)
// sg devices on Linux doesn't seem to handle get configuration (but scd devices work)
cdb[0] = 0x5A;
cdb[1] = 0x00;
cdb[2] = 0x2A;
cdb[3] = 0x00;
cdb[4] = 0x00;
cdb[5] = 0x00;
cdb[6] = 0x00;
cdb[7] = 0x00;
cdb[8] = size;
cdb[9] = 0x00;

result = scsiSay(scsi, cdb, 10, (char *) buffer, size, X1_DATA_IN);
if (result < 0) return 0;

if (buffer[8] != 0x2A) return 0;	// Security check: code page should be returned

// bit 3 of response byte 2 (+8) indicates DVD-ROM read capability
if (buffer[10] & 0x08)
	return 1;

return 0;
}



/* ------------------------------------------------------------------------ */
int getInquiry(Scsi *scsi, char *vendor, char *model, char *revision)
{
char cdb[6];
char buffer[96];
int size;
INT result;

size = (sizeof(buffer) > 255) ? 255 : sizeof(buffer);
cdb[0] = 0x12;
cdb[1] = 0x00;
cdb[2] = 0x00;
cdb[3] = 0x00;
cdb[4] = size;
cdb[5] = 0x00;

result = scsiSay(scsi, cdb, 6, buffer, size, X1_DATA_IN);
if (result < 0) return 0;
if ((size - result) < 36) return 0;

memmove(vendor, buffer+8, 8); vendor[8] = 0;
memmove(model, buffer+8+8, 16); model[16] = 0;
memmove(revision, buffer+8+8+16, 4); revision[4] = 0;
unpad(vendor);
unpad(model);
unpad(revision);

return 1;
}



/* ------------------------------------------------------------------------ */
int waitForReady(Scsi *scsi, int withMedium)
{
char cdb[6];
char sns[0x20];
int retries;
INT result;

cdb[0] = 0x00;
cdb[1] = 0x00;
cdb[2] = 0x00;
cdb[3] = 0x00;
cdb[4] = 0x00;
cdb[5] = 0x00;

result = scsiSay(scsi, cdb, 6, NULL, 8, X0_DATA_NOT);

retries = 0;
do
  {
  if (result)
    {
    if (scsiGetSense(scsi, sns, sizeof(sns), 0) > 2)
      {
      if ((sns[2] & 0xf) == 0x2)
        {
        if ( (sns[12] == 0x04) && (sns[13] == 0x01) )
          retries++; // becoming ready
        else if ( !withMedium && (sns[12] == 0x3a) )
          return 1; // no medium
        else
          return 0; // not ready
        }
      if ((sns[2] & 0xf) == 0x6)
        retries++; // unit attention
      result = scsiSay(scsi, cdb, 6, NULL, 8, X0_DATA_NOT);
      }
    }
  } while (retries--);

return 1;
}


/* ------------------------------------------------------------------------ */
int getRPC(Scsi *scsi, char *buffer)
{
char cdb[12];
INT result;

cdb[0]  = (char) 0xA4;
cdb[1]  = 0x00;
cdb[2]  = 0x00;
cdb[3]  = 0x00;
cdb[4]  = 0x00;
cdb[5]  = 0x00;
cdb[6]  = 0x00;
cdb[7]  = 0x00;
cdb[8]  = 0x00;
cdb[9]  = 0x08;
cdb[10] = 0x08;
cdb[11] = 0x00;

// This is needed for DVD Region Killer on windows
buffer[6] = 0;

result = scsiSay(scsi, cdb, 12, buffer, 8, X1_DATA_IN);

if (result) return 0;

return 1;
}



/* ------------------------------------------------------------------------ */
int getMovieMask(Scsi *scsi, char *movieMask, char *copyright)
{
char cdb[12];
char buffer[8];
INT result;

cdb[0]  = (char) 0xAD;
cdb[1]  = 0x00;
cdb[2]  = 0x00;
cdb[3]  = 0x00;
cdb[4]  = 0x00;
cdb[5]  = 0x00;
cdb[6]  = 0x00;
cdb[7]  = 0x01;
cdb[8]  = 0x00;
cdb[9]  = 0x08;
cdb[10] = 0x00;
cdb[11] = 0x00;

result = scsiSay(scsi, cdb, 12, buffer, 8, X1_DATA_IN);
if (result) return 0;

*copyright = buffer[4];
*movieMask = buffer[5];

return 1;
}



/* ------------------------------------------------------------------------ */
int ProcessDevice(Scsi *scsi, char const *name)
{
int ok;
char vendor[8+1];
char model[16+1];
char revision[4+1];
char buffer[8];
char driveMask;
char movieMask;
char copyright;
unsigned char changesLeft;
unsigned char resetsLeft;
int flags;
static char *flagText[4] = {"Region not set", "Region set", "Last chance", "Locked"};


if (scsiOpen(scsi, name))
  {
  if (allDevices) return 0; // quietly return if scanning for all devices.
  fprintf(stderr, "Can't open device named \"%s\".\n", name);
  return 1;
  }

// Windows queries fail without those
scsiLimitSense(scsi, SENSE_LENGTH);

scsiLimitSeconds(scsi, MAX_SECONDS, 0);


if (isDVD(scsi))
  {
  ok = getInquiry(scsi, vendor, model, revision);
  if (ok)
    {
    printf("\n");
#ifdef __APPLE__
// just because PLScsi names have different meaning on Mac.
    printf("     Device : '%8s%16s%4s'\n", vendor, model, revision);
#else
    printf("     Device : %s\n", name+((name[5]==':')?4:0));
#endif
    printf("     Vendor : %s\n", vendor);
    printf("      Model : %s\n", model);
    printf("   Revision : %s\n", revision);
    if (waitForReady(scsi, 0))
      {
      if (getRPC(scsi, buffer) && buffer[6])
        {
        printf("     Status : RPC-%lu (region locked)\n", ((unsigned long) buffer[6])+1);
        if (buffer[6] == 1)
          {
          driveMask = buffer[5];
          changesLeft = buffer[4] & 0x7;
          resetsLeft = (buffer[4] >> 3) & 0x7;
          flags = (buffer[4] >> 6) & 0x3;
          printf("     Region : %s\n", regionString(driveMask));
          printf("    Changes : %lu region change%s left\n", (unsigned long) changesLeft,
                                (changesLeft > 1) ? "s" : "");
          printf("              %lu vendor reset%s left (not available to user)\n", (unsigned long) resetsLeft,
                                (resetsLeft > 1) ? "s" : "");
          printf("              state is '%s'\n", flagText[flags]);
          }
        else
          {
          printf("              Unknown RPC scheme.\n");
          printf("              (%02lx %02lx %02lx %02lx)\n",
                                ((unsigned long) buffer[4]) & 0xff,
                                ((unsigned long) buffer[5]) & 0xff,
                                ((unsigned long) buffer[6]) & 0xff,
                                ((unsigned long) buffer[7]) & 0xff );
          }
        }
      else
        {
        printf("     Status : RPC-1 (region free)\n");
        driveMask = 0;
        }
      if (getMovieMask(scsi, &movieMask, &copyright))
        {
        char *str;
          
        str = regionString(movieMask);
        printf("      Movie : region%s %s%s\n", str[1] ? "s" : "", str, (copyright == 0x01) ? ", CSS encrypted" : "");
        if ( (copyright == 0x01) && ((driveMask | movieMask) & 0xff) == 0xff)
          {
          if ((driveMask & 0xff) == 0xff)
            {
            printf("Region Lock : ACTIVE, no region has been set to this DVD drive.\n");
            }
          else
            {
            printf("Region Lock : ACTIVE, drive and movie regions mismatch.\n");
            printf("              Please visit <http://www.rpc1.org/> for information on how to\n");
            printf("                make your drive region free (a.k.a. RPC-1).\n");
            }
          }
        }
      }
    else
      { // not ready
      printf("      Error : drive not ready, can't get its region lock status\n");
      printf("              Please retry\n");
      }
    }
  }
else
  {
  if (!allDevices)
    {
    ok = getInquiry(scsi, vendor, model, revision);
    if (ok)
      {
      printf("\n");
      printf("     Vendor : %s\n", vendor);
      printf("      Model : %s\n", model);
      printf("   Revision : %s\n", revision);
      printf("              This isn't a DVD drive\n\n");
      }
    }
  }


scsiClose(scsi);
return 0;
}



/* ------------------------------------------------------------------------ */
void shiftArgs(int *argc, char *(*argv[]))
{
int i;

if ( (*argc) <= 1) return;
for (i = 2; i < (*argc); i++)
  {
  (*argv)[i-1] = (*argv)[i];
  }
*argc = (*argc) -1;
}



/* ------------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
Scsi *scsi;
int i;
char *devicename;
char sptxname[] = "\\\\.\\D:";


printf("DVD Zone v0.3\n");

scsi = newScsi();
if (!scsi)
  {
  fprintf(stderr, "Internal error: newScsi() returned NULL.\n");
  return 1;
  }

/* not yet
setRegion = 0;
if (!strcmp(argv[1], "-s"))
  {
  setRegion = 1;
  shiftArgs(&argc, &argv);
  }
*/

allDevices = (argc == 1); // if user gives no device name, just scan all devices

if (allDevices)
  {
  memset(name, 0, sizeof(name));

  // Need to disable stderr on device detection for windows
  scsiSetErr(scsi, NULL_FD);

  for ( ; ; )
    {
    if (scsiReadName(scsi, name, sizeof(name)) < 0) break;
    if (ProcessDevice(scsi, name)) break;
    }
  scsiSetErr(scsi, stderr);
  }
else
  {
  for (i = 1; i < argc; i++)
    {
    // Simplify SPTX device names
    if (argv[i][1] == ':')
      {
      sptxname[4] = argv[i][0];
      devicename = sptxname;
      }
    else
      devicename = argv[i];

    if (ProcessDevice(scsi, devicename))
      {
      fprintf(stderr, "Error processing device named \"%s\".\n", argv[i]);
      }
    }
  }
}

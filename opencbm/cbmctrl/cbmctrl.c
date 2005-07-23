/*
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *  Copyright 1999-2004 Michael Klein <michael.klein@puffin.lb.shuttle.de>
 *  Modifications for cbm4win Copyright 2001-2004 Spiro Trikaliotis
*/

#ifdef SAVE_RCSID
static char *rcsid =
    "@(#) $Id: cbmctrl.c,v 1.7.2.1 2005-07-23 10:25:38 strik Exp $";
#endif

#include "opencbm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
typedef int (*mainfunc)(CBM_FILE fd, char *argv[]);

#include "arch.h"

static const unsigned char prog_tdchange[] = {
#include "tdchange.inc"
};


/*
 * Simple wrapper for reset
 */
static int do_reset(CBM_FILE fd, char *argv[])
{
    return cbm_reset(fd);
}

/*
 * Simple wrapper for listen
 */
static int do_listen(CBM_FILE fd, char *argv[])
{
    return cbm_listen(fd, arch_atoc(argv[0]), arch_atoc(argv[1]));
}

/*
 * Simple wrapper for talk
 */
static int do_talk(CBM_FILE fd, char *argv[])
{
    return cbm_talk(fd, arch_atoc(argv[0]), arch_atoc(argv[1]));
}

/*
 * Simple wrapper for unlisten
 */
static int do_unlisten(CBM_FILE fd, char *argv[])
{
    return cbm_unlisten(fd);
}

/*
 * Simple wrapper for untalk
 */
static int do_untalk(CBM_FILE fd, char *argv[])
{
    return cbm_untalk(fd);
}

/*
 * Simple wrapper for open
 */
static int do_open(CBM_FILE fd, char *argv[])
{
    return cbm_open(fd, arch_atoc(argv[0]), arch_atoc(argv[1]), argv[2], strlen(argv[2]));
}

/*
 * Simple wrapper for close
 */
static int do_close(CBM_FILE fd, char *argv[])
{
    return cbm_close(fd, arch_atoc(argv[0]), arch_atoc(argv[1]));
}

/*
 * display device status w/ PetSCII conversion
 */
static int do_status(CBM_FILE fd, char *argv[])
{
    unsigned char buf[40];
    unsigned char unit;
    int rv;

    unit = arch_atoc(argv[0]);

    rv = cbm_device_status(fd, unit, buf, sizeof(buf));
    fprintf(stderr, "%s", cbm_petscii2ascii(buf));

    return (rv == 99) ? 1 : 0;
}

/*
 * send device command
 */
static int do_command(CBM_FILE fd, char *argv[])
{
    int  rv;

    rv = cbm_listen(fd, arch_atoc(argv[0]), 15);
    if(rv == 0)
    {
        cbm_raw_write(fd, argv[1], strlen(argv[1]));
        rv = cbm_unlisten(fd);
    }
    return rv;
}

/*
 * display directory
 */
static int do_dir(CBM_FILE fd, char *argv[])
{
    unsigned char c, buf[40];
    int rv;
    unsigned char unit;

    unit = arch_atoc(argv[0]);
    rv = cbm_open(fd, unit, 0, "$", strlen("$"));
    if(rv == 0)
    {
        if(cbm_device_status(fd, unit, buf, sizeof(buf)) == 0)
        {
            cbm_talk(fd, unit, 0);
            if(cbm_raw_read(fd, buf, 2) == 2)
            {
                while(cbm_raw_read(fd, buf, 2) == 2)
                {
                    if(cbm_raw_read(fd, buf, 2) == 2)
                    {
                        printf("%u ", buf[0] | (buf[1] << 8));
                        while((cbm_raw_read(fd, &c, 1) == 1) && c)
                        {
                            putchar(cbm_petscii2ascii_c(c));
                        }
                        putchar('\n');
                    }
                }
                cbm_device_status(fd, unit, buf, sizeof(buf));
                fprintf(stderr, "%s", cbm_petscii2ascii(buf));
            }
            cbm_untalk(fd);
        }
        else
        {
            fprintf(stderr, "%s", cbm_petscii2ascii(buf));
        }
        cbm_close(fd, unit, 0);
    }
    return rv;
}

/*
 * read device memory, dump to stdout or a file
 */
static int do_download(CBM_FILE fd, char *argv[])
{
    unsigned char unit;
    unsigned short c;
    int addr, count, i, rv = 0;
    char *tail, buf[32], cmd[7];
    FILE *f;

    unit = arch_atoc(argv[0]);

    addr = strtol(argv[1], &tail, 0);
    if(addr < 0 || addr > 0xffff || *tail)
    {
        arch_error(0, 0, "invalid address: %s", argv[1]);
        return 1;
    }

    count = strtol(argv[2], &tail, 0);
    if((count + addr) > 0x10000 || *tail)
    {
        arch_error(0, arch_get_errno(), "invalid byte count %s", argv[2]);
        return 1;
    }

    if(argv[3] && strcmp(argv[3],"-") != 0)
    {
        /* a filename (other than simply "-") was given, open that file */

        f = fopen(argv[3], "wb");
    }
    else
    {
        /* no filename was given, open stdout in binary mode */

        f = arch_fdopen(arch_fileno(stdout), "wb");

        /* set binary mode for output stream */

        arch_setbinmode(arch_fileno(stdout));
    }

    if(!f)
    {
        arch_error(0, arch_get_errno(), "could not open output file: %s",
              (argv[3] && strcmp(argv[3], "-") != 0) ? argv[3] : "stdout");
        return 1;
    }

    for(i = 0; (rv == 0) && (i < count); i+=32)
    {
        c = count - i;
        if(c > 32) 
        {
            c = 32;
        }
        sprintf(cmd, "M-R%c%c%c", addr%256, addr/256, c);
        cbm_listen(fd, unit, 15);
        rv = cbm_raw_write(fd, cmd, 6) == 6 ? 0 : 1;
        cbm_unlisten(fd);
        if(rv == 0)
        {
            addr += c;
            cbm_talk(fd, unit, 15);
            rv = cbm_raw_read(fd, buf, c) == c ? 0 : 1;
            cbm_untalk(fd);
            if(rv == 0)
            {
                fwrite(buf, 1, c, f);
            }
        }
    }
    fclose(f);
    return rv;
}

/*
 * load binary data from file into device memory
 */
static int do_upload(CBM_FILE fd, char *argv[])
{
    unsigned char unit;
    int addr;
    size_t size;
    char *tail, *fn;
    unsigned char addr_buf[2];
    static unsigned char buf[65537];
    FILE *f;

    unit = arch_atoc(argv[0]);

    addr = strtoul(argv[1], &tail, 0);
    if(addr < -1 || addr > 0xffff || *tail)
    {
        arch_error(0, 0, "invalid address: %s", argv[1]);
        return 1;
    }

    if(!argv[2] || strcmp(argv[2], "-") == 0 || strcmp(argv[2], "") == 0)
    {
        fn = "(stdin)";
        f = stdin;

        // set binary mode for input stream

        arch_setbinmode(arch_fileno(stdin));
    }
    else
    {
        off_t filesize;

        fn = argv[2];
        f = fopen(argv[2], "rb");
        if(f == NULL)
        {
            arch_error(0, arch_get_errno(), "could not open %s", fn);
            return 1;
        }
        if(arch_filesize(argv[2], &filesize))
        {
            arch_error(0, arch_get_errno(), "could not stat %s", fn);
            return 1;
        }
    }

    if(addr == -1)
    {
        /* read address from file */
        if(fread(addr_buf, 2, 1, f) != 1)
        {
            arch_error(0, arch_get_errno(), "could not read %s", fn);
            if(f != stdin) fclose(f);
            return 1;
        }

        /* don't assume a particular endianess, although the cbm4linux
         * package is only i386 for now  */
        addr = addr_buf[0] | (addr_buf[1] << 8);
    }

    size = fread(buf, 1, sizeof(buf), f);
    if(ferror(f))
    {
        arch_error(0, 0, "could not read %s", fn);
        if(f != stdin) fclose(f);
        return 1;
    }
    else if(size == 0 && feof(f))
    {
        arch_error(0, 0, "no data: %s", fn);
        if(f != stdin) fclose(f);
        return 1;
    }

    if(addr + size > 0x10000)
    {
        arch_error(0, 0, "program too big: %s", fn);
        if (f != stdin) fclose(f);
        return 1;
    }

    if(f != stdin) fclose(f);

    return (cbm_upload(fd, unit, addr, buf, size) == (int)size) ? 0 : 1;
}

/*
 * identify connected devices
 */
static int do_detect(CBM_FILE fd, char *argv[])
{
    unsigned int num_devices;
    unsigned char device;
    const char *type_str;

    num_devices = 0;

    for( device = 8; device < 16; device++ )
    {
        enum cbm_device_type_e device_type;
        if( cbm_identify( fd, device, &device_type, &type_str ) == 0 )
        {
            enum cbm_cable_type_e cable_type;
            const char *cable_str = "(cannot determine cable type)";
 
            num_devices++;

            if ( cbm_identify_xp1541( fd, device, &device_type, &cable_type ) == 0 )
            {
                switch (cable_type)
                {
                case cbm_ct_none:
                    cable_str = "";
                    break;

                case cbm_ct_xp1541:
                    cable_str = "(XP1541)";
                    break;

                case cbm_ct_unknown:
                default:
                    break;
                }
            }
            printf( "%2d: %s %s\n", device, type_str, cable_str );
        }
    }
    arch_set_errno(0);
    return num_devices > 0 ? 0 : 1;
}

/*
 * wait until user changes the disk
 */
static int do_change(CBM_FILE fd, char *argv[])
{
    unsigned char unit;
    int rv;

    unit = arch_atoc(argv[0]);

    do
    {
        rv = cbm_upload(fd, unit, 0x500, prog_tdchange, sizeof(prog_tdchange));
    
        if (rv != sizeof(prog_tdchange))
        {
            rv = 1;
            break;
        }

        cbm_exec_command(fd, unit, "U3:", 0);
        cbm_iec_release(fd, IEC_ATN | IEC_DATA | IEC_CLOCK | IEC_RESET);

        /*
         * Now, wait for the drive routine to signal its starting
         */
        cbm_iec_wait(fd, IEC_DATA, 1);

        /*
         * Now, wait until CLOCK is high, too, which tells us that
         * a new disk has been successfully read.
         */
        cbm_iec_wait(fd, IEC_CLOCK, 1);

        /*
         * Signal: We recognized this
         */
        cbm_iec_set(fd, IEC_ATN);

        /*
         * Wait for routine ending.
         */
        cbm_iec_wait(fd, IEC_CLOCK, 0);

        /*
         * Release ATN again
         */
        cbm_iec_release(fd, IEC_ATN);

    } while (0);

    return rv;
}

struct prog
{
    char    *name;
    mainfunc prog;
    int      req_args_min;
    int      req_args_max;
    char    *arglist;
};

static struct prog prog_table[] =
{
    {"listen"  , do_listen  , 2, 2, "<device> <secadr>"               },
    {"talk"    , do_talk    , 2, 2, "<device> <secadr>"               },
    {"unlisten", do_unlisten, 0, 0, ""                                },
    {"untalk"  , do_untalk  , 0, 0, ""                                },
    {"open"    , do_open    , 3, 3, "<device> <secadr> <filename>"    },
    {"close"   , do_close   , 2, 2, "<device> <secadr>"               },
    {"status"  , do_status  , 1, 1, "<device>"                        },
    {"command" , do_command , 2, 2, "<device> <cmdstr>"               },
    {"dir"     , do_dir     , 1, 1, "<device>"                        },
    {"download", do_download, 3, 4, "<device> <adr> <count> [<file>]" },
    {"upload"  , do_upload  , 2, 3, "<device> <adr> [<file>]"         },
    {"reset"   , do_reset   , 0, 0, ""                                },
    {"detect"  , do_detect  , 0, 0, ""                                },
    {"change"  , do_change  , 1, 1, "<device>"                        },
    {NULL,NULL}
};

static struct prog *find_main(char *name)
{
    int i;

    for(i=0; prog_table[i].name; i++)
    {
        if(strcmp(name, prog_table[i].name) == 0)
        {
            return &prog_table[i];
        }
    }
    return NULL;
}

int ARCH_MAINDECL main(int argc, char *argv[])
{
    struct prog *p;
    int i;

    p = argc < 2 ? NULL : find_main(argv[1]);
    if(p)
    {
        if((p->req_args_min <= argc-2) && (p->req_args_max >= argc-2))
        {
            CBM_FILE fd;
            int rv;

            rv = cbm_driver_open(&fd, 0);

            if(rv == 0)
            {
                rv = p->prog(fd, &argv[2]) != 0;
                if(rv && arch_get_errno())
                {
                    arch_error(0, arch_get_errno(), "%s", argv[1]);
                }
                cbm_driver_close(fd);
            }
            else
            {
                if(arch_get_errno())
                {
                    arch_error(0, arch_get_errno(), "%s", cbm_get_driver_name(0));
                }
                rv = 1;
            }
            return rv;
        }
        else
        {
            arch_error(0, arch_get_errno(), "wrong number of arguments:\n\n  %s %s %s\n",
                        argv[0], argv[1], p->arglist);
        }
    }
    else
    {
        printf("invalid command, available ones are:\n\n");
        for(i=0; prog_table[i].prog; i++)
        {
            printf("  %s %s\n", prog_table[i].name, prog_table[i].arglist);
        }
    }
    return 2;
}

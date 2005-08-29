/*
 *  Copyright (C) 2005 Tomasz Kojm <tkojm@clamav.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "cltypes.h"
#include "elf.h"
#include "clamav.h"

#define DETECT_BROKEN		    (options & CL_SCAN_BLOCKBROKEN)

static short need_conversion = 0;

static inline uint16_t EC16(uint16_t v)
{
    if(!need_conversion)
	return v;
    else
	return ((v >> 8) + (v << 8));
}

static inline uint32_t EC32(uint32_t v)
{
    if(!need_conversion)
	return v;
    else
	return ((v >> 24) | ((v & 0x00FF0000) >> 8) | ((v & 0x0000FF00) << 8) | (v << 24));
}

int cli_scanelf(int desc, const char **virname, long int *scanned, const struct cl_node *root, const struct cl_limits *limits, unsigned int options, unsigned int arec, unsigned int mrec)
{
	struct elf_file_hdr32 file_hdr;
	struct elf_section_hdr32 *section_hdr;
	uint16_t shnum, shentsize;
	uint32_t entry, shoff, image;
	int i;

    cli_dbgmsg("in cli_elfheader\n");

    if(read(desc, &file_hdr, sizeof(file_hdr)) != sizeof(file_hdr)) {
	/* Not an ELF file? */
	cli_dbgmsg("ELF: Can't read file header\n");
	return CL_CLEAN;
    }

    if(memcmp(file_hdr.e_ident, "\x7f\x45\x4c\x46", 4)) {
	cli_dbgmsg("ELF: Not an ELF file\n");
	return CL_CLEAN;
    }

    if(file_hdr.e_ident[4] != 1) {
	cli_dbgmsg("ELF: 64-bit binaries not supported\n");
	return CL_CLEAN;
    }

    if(file_hdr.e_ident[5] == 1) {
	image =  0x8048000;
#if WORDS_BIGENDIAN == 0
	cli_dbgmsg("ELF: File is little-endian - conversion not required\n");
#else
	cli_dbgmsg("ELF: File is little-endian - data conversion enabled\n");
	need_conversion = 1;
#endif
    } else {
	image =  0x10000;
#if WORDS_BIGENDIAN == 0
	cli_dbgmsg("ELF: File is big-endian - data conversion enabled\n");
	need_conversion = 1;
#else
	cli_dbgmsg("ELF: File is big-endian - conversion not required\n");
#endif
    }

    switch(EC16(file_hdr.e_type)) {
	case 0x0: /* ET_NONE */
	    cli_dbgmsg("ELF: File type: None\n");
	    break;
	case 0x1: /* ET_REL */
	    cli_dbgmsg("ELF: File type: Relocatable\n");
	    break;
	case 0x2: /* ET_EXEC */
	    cli_dbgmsg("ELF: File type: Executable\n");
	    break;
	case 0x3: /* ET_DYN */
	    cli_dbgmsg("ELF: File type: Core\n");
	    break;
	case 0x4: /* ET_CORE */
	    cli_dbgmsg("ELF: File type: Core\n");
	    break;
	default:
	    cli_dbgmsg("ELF: File type: Unknown (%d)\n", EC16(file_hdr.e_type));
    }

    switch(EC16(file_hdr.e_machine)) {
	/* Due to a huge list, we only include the most popular machines here */
	case 0x0: /* EM_NONE */
	    cli_dbgmsg("ELF: Machine type: None\n");
	    break;
	case 0x2: /* EM_SPARC */
	    cli_dbgmsg("ELF: Machine type: SPARC\n");
	    break;
	case 0x3: /* EM_386 */
	    cli_dbgmsg("ELF: Machine type: Intel 80386\n");
	    break;
	case 0x4: /* EM_68K */
	    cli_dbgmsg("ELF: Machine type: Motorola 68000\n");
	    break;
	case 0x8: /* EM_MIPS */
	    cli_dbgmsg("ELF: Machine type: MIPS RS3000\n");
	    break;
	case 0x15: /* EM_PARISC */
	    cli_dbgmsg("ELF: Machine type: HPPA\n");
	    break;
	case 0x20: /* EM_PPC */
	    cli_dbgmsg("ELF: Machine type: PowerPC\n");
	    break;
	case 0x21: /* EM_PPC64 */
	    cli_dbgmsg("ELF: Machine type: PowerPC 64-bit\n");
	    break;
	case 0x22: /* EM_S390 */
	    cli_dbgmsg("ELF: Machine type: IBM S390\n");
	    break;
	case 0x40: /* EM_ARM */
	    cli_dbgmsg("ELF: Machine type: ARM\n");
	    break;
	case 0x41: /* EM_FAKE_ALPHA */
	    cli_dbgmsg("ELF: Machine type: Digital Alpha\n");
	    break;
	case 0x43: /* EM_SPARCV9 */
	    cli_dbgmsg("ELF: Machine type: SPARC v9 64-bit\n");
	    break;
	case 0x50: /* EM_IA_64 */
	    cli_dbgmsg("ELF: Machine type: IA64\n");
	    break;
	default:
	    cli_dbgmsg("ELF: Machine type: Unknown (%d)\n", EC16(file_hdr.e_machine));
    }

    entry = EC32(file_hdr.e_entry);
    cli_dbgmsg("ELF: Entry point address: 0x%.8x\n", entry);
    cli_dbgmsg("ELF: Entry point offset: 0x%.8x (%d)\n", entry - image, entry - image);

    shnum = EC16(file_hdr.e_shnum);
    cli_dbgmsg("ELF: Number of sections: %d\n", shnum);
    if(shnum > 256) {
	cli_dbgmsg("ELF: Suspicious number of sections\n");
        if(DETECT_BROKEN) {
	    if(virname)
            *virname = "Broken.Executable";
            return CL_VIRUS;
        }
	return CL_EFORMAT;
    }

    shentsize = EC16(file_hdr.e_shentsize);
    if(shentsize != sizeof(struct elf_section_hdr32)) {
	cli_errmsg("ELF: shentsize != sizeof(struct elf_section_hdr32)\n");
        if(DETECT_BROKEN) {
	    if(virname)
            *virname = "Broken.Executable";
            return CL_VIRUS;
        }
	return CL_EFORMAT;
    }

    shoff = EC32(file_hdr.e_shoff);
    cli_dbgmsg("ELF: Section header table offset: %d\n", shoff);
    if(lseek(desc, shoff, SEEK_SET) != shoff) {
	/* Possibly broken end of file */
        if(DETECT_BROKEN) {
	    if(virname)
            *virname = "Broken.Executable";
            return CL_VIRUS;
        }
	return CL_CLEAN;
    }

    section_hdr = (struct elf_section_hdr32 *) cli_calloc(shnum, shentsize);
    if(!section_hdr) {
	cli_errmsg("ELF: Can't allocate memory for section headers\n");
	return CL_EMEM;
    }

    cli_dbgmsg("------------------------------------\n");

    for(i = 0; i < shnum; i++) {

	if(read(desc, &section_hdr[i], sizeof(struct elf_section_hdr32)) != sizeof(struct elf_section_hdr32)) {
            cli_dbgmsg("ELF: Can't read section header\n");
            cli_dbgmsg("ELF: Possibly broken ELF file\n");
            free(section_hdr);
            if(DETECT_BROKEN) {
                if(virname)
                    *virname = "Broken.Executable";
                return CL_VIRUS;
            }
            return CL_CLEAN;
        }

	cli_dbgmsg("ELF: Section %d\n", i);
	cli_dbgmsg("ELF: Section offset: %d\n", EC32(section_hdr[i].sh_offset));

	switch(EC32(section_hdr[i].sh_type)) {
	    case 0x6: /* SHT_DYNAMIC */
		cli_dbgmsg("ELF: Section type: Dynamic linking information\n");
		break;
	    case 0xb: /* SHT_DYNSYM */
		cli_dbgmsg("ELF: Section type: Symbols for dynamic linking\n");
		break;
	    case 0xf: /* SHT_FINI_ARRAY */
		cli_dbgmsg("ELF: Section type: Array of pointers to termination functions\n");
		break;
	    case 0x5: /* SHT_HASH */
		cli_dbgmsg("ELF: Section type: Symbol hash table\n");
		break;
	    case 0xe: /* SHT_INIT_ARRAY */
		cli_dbgmsg("ELF: Section type: Array of pointers to initialization functions\n");
		break;
	    case 0x8: /* SHT_NOBITS */
		cli_dbgmsg("ELF: Section type: Empty section (NOBITS)\n");
		break;
	    case 0x7: /* SHT_NOTE */
		cli_dbgmsg("ELF: Section type: Note section\n");
		break;
	    case 0x0: /* SHT_NULL */
		cli_dbgmsg("ELF: Section type: Null (no associated section)\n");
		break;
	    case 0x10: /* SHT_PREINIT_ARRAY */
		cli_dbgmsg("ELF: Section type: Array of pointers to preinit functions\n");
		break;
	    case 0x1: /* SHT_PROGBITS */
		cli_dbgmsg("ELF: Section type: Program information\n");
		break;
	    case 0x9: /* SHT_REL */
		cli_dbgmsg("ELF: Section type: Relocation entries w/o explicit addends\n");
		break;
	    case 0x4: /* SHT_RELA */
		cli_dbgmsg("ELF: Section type: Relocation entries with explicit addends\n");
		break;
	    case 0x3: /* SHT_STRTAB */
		cli_dbgmsg("ELF: Section type: String table\n");
		break;
	    case 0x2: /* SHT_SYMTAB */
		cli_dbgmsg("ELF: Section type: Symbol table\n");
		break;
	    case 0x6ffffffd: /* SHT_GNU_verdef */
		cli_dbgmsg("ELF: Section type: Provided symbol versions\n");
		break;
	    case 0x6ffffffe: /* SHT_GNU_verneed */
		cli_dbgmsg("ELF: Section type: Required symbol versions\n");
		break;
	    case 0x6fffffff: /* SHT_GNU_versym */
		cli_dbgmsg("ELF: Section type: Symbol Version Table\n");
		break;
	    default :
		cli_dbgmsg("ELF: Section type: Unknown\n");
	}

	if(EC32(section_hdr[i].sh_flags) & 0x1) /* SHF_WRITE */
	    cli_dbgmsg("ELF: Section contains writable data\n");

	if(EC32(section_hdr[i].sh_flags) & 0x2) /* SHF_ALLOC */
	    cli_dbgmsg("ELF: Section occupies memory\n");

	if(EC32(section_hdr[i].sh_flags) & 0x4) /* SHF_EXECINSTR */
	    cli_dbgmsg("ELF: Section contains executable code\n");

	cli_dbgmsg("------------------------------------\n");
    }

    free(section_hdr);
    return CL_CLEAN;
}

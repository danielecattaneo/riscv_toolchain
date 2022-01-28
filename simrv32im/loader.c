#include <stdio.h>
#include <inttypes.h>
#include "loader.h"
#include "debugger.h"


t_ldrError ldrLoadBinary(const char *path, t_memAddress baseAddr, t_memAddress entry)
{
   size_t size;
   uint8_t *buf;
   FILE *fp;

   dbgPrintf("Loading raw binary file \"%s\" at address %"PRIu32"\n", path);

   fp = fopen(path, "rb");
   if (fp == NULL)
      return LDR_FILE_ERROR;
   
   if (fseek(fp, 0, SEEK_END) < 0)
      return LDR_FILE_ERROR;
   size = ftello(fp);
   if (size < 0) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }
   if (fseek(fp, 0, SEEK_SET) < 0) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }
   
   if (memMapArea(baseAddr, size, &buf) != MEM_NO_ERROR) {
      fclose(fp);
      return LDR_MEMORY_ERROR;
   }
   if (fread(buf, size, 1, fp) < 1) {
      fclose(fp);
      return LDR_FILE_ERROR;
   }

   cpuReset(entry);

   fclose(fp);
   return LDR_NO_ERROR;
}


#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

#define EI_MAG0      0     /* File identification */
#define EI_MAG1      1     /* File identification */
#define EI_MAG2      2     /* File identification */
#define EI_MAG3      3     /* File identification */
#define EI_CLASS     4     /* File class */
#define EI_DATA      5     /* Data encoding */
#define EI_VERSION   6     /* File version */
#define EI_PAD       7     /* Start of padding bytes */

#define ELFCLASSNONE 0     /* Invalid class  */
#define ELFCLASS32   1     /* 32-bit objects */

#define ELFDATANONE  0     /* Invalid data encoding */
#define ELFDATA2LSB  1     /* Little-endian */

#define ET_NONE      0     /* No file type */
#define ET_EXEC      2     /* Executable file */

#define EM_NONE      0     /* No machine */
#define EM_RISCV     0xF3  /* RISC-V */

typedef struct __attribute__ ((packed)) Elf32_Ehdr {
  unsigned char e_ident[EI_NIDENT];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off  e_phoff;
  Elf32_Off  e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
} Elf32_Ehdr;

#define PT_NULL      0     /* Ignored segment */
#define PT_LOAD      1     /* Loadable segment */
#define PT_NOTE      4     /* Target-dependent auxiliary information */

typedef struct __attribute__ ((packed)) Elf32_Phdr {
  Elf32_Word  p_type;
  Elf32_Off   p_offset;
  Elf32_Addr  p_vaddr;
  Elf32_Addr  p_paddr;
  Elf32_Word  p_filesz;
  Elf32_Word  p_memsz;
  Elf32_Word  p_flags;
  Elf32_Word  p_align;
} Elf32_Phdr;

t_ldrError ldrLoadELF(const char *path)
{
   t_ldrError res = LDR_NO_ERROR;
   size_t size;
   FILE *fp = NULL;
   Elf32_Ehdr header;
   Elf32_Phdr segment;
   off_t phi, phnum, phoff, phentsize, readsz;
   uint8_t *buf;

   dbgPrintf("Loading ELF file \"%s\"\n", path);

   fp = fopen(path, "rb");
   if (fp == NULL)
      return LDR_FILE_ERROR;

   if (fread(&header, sizeof(Elf32_Ehdr), 1, fp) < 1)
      goto read_error;
   if (header.e_ident[EI_MAG0] != 0x7f || header.e_ident[EI_MAG1] != 'E' ||
         header.e_ident[EI_MAG2] != 'L' || header.e_ident[EI_MAG3] != 'F' ||
         header.e_ident[EI_CLASS] != ELFCLASS32 ||
         header.e_ident[EI_DATA] != ELFDATA2LSB ||
         header.e_ident[EI_VERSION] != 1)
      goto invalid_file;
   if (header.e_type != ET_EXEC || header.e_version != 1)
      goto invalid_file;
   if (header.e_machine != EM_RISCV)
      goto invalid_arch;

   phnum = header.e_phnum;
   phoff = header.e_phoff;
   phentsize = header.e_phentsize;
   for (phi=0; phi<phnum; phi++) {
      fseeko(fp, phoff + phi*phentsize, SEEK_SET);
      if (fread(&segment, sizeof(Elf32_Phdr), 1, fp) < 1)
         goto read_error;
      
      if (segment.p_type == PT_NULL || segment.p_type == PT_NOTE)
         continue;
      if (segment.p_type != PT_LOAD)
         goto invalid_file;

      dbgPrintf("Loaded section at 0x%08"PRIx32" (size=0x%08"PRIx32") to 0x%08"PRIx32" (size=0x%08"PRIx32")\n", 
            segment.p_offset, segment.p_filesz, segment.p_vaddr, segment.p_memsz);
      if (memMapArea(segment.p_vaddr, segment.p_memsz, &buf) != MEM_NO_ERROR)
         goto mem_error;
      fseeko(fp, (off_t)segment.p_offset, SEEK_SET);
      readsz = MIN(segment.p_memsz, segment.p_filesz);
      if (fread(buf, readsz, 1, fp) < 1)
         goto read_error;
   }

   dbgPrintf("Setting the entry point to 0x%"PRIx32"\n", header.e_entry); 
   cpuReset(header.e_entry);

   goto cleanup;
mem_error:
   res = LDR_MEMORY_ERROR;
   goto cleanup;
read_error:
   res = LDR_FILE_ERROR;
   goto cleanup;
invalid_file:
   res = LDR_INVALID_FORMAT;
   goto cleanup;
invalid_arch:
   res = LDR_INVALID_ARCH;
cleanup:
   fclose(fp);
   return res;
}


t_ldrFileType ldrDetectExecType(const char *path)
{
   t_ldrFileType res;
   FILE *fp = NULL;
   char buf[4];

   fp = fopen(path, "rb");
   if (fp == NULL)
      return LDR_FORMAT_DETECT_ERROR;

   if (fread(buf, 4, 1, fp) < 1)
      goto file_error;

   if (buf[EI_MAG0] == 0x7f && buf[EI_MAG1] == 'E' &&
         buf[EI_MAG2] == 'L' && buf[EI_MAG3] == 'F')
      res = LDR_FORMAT_ELF;
   else
      res = LDR_FORMAT_BINARY;

   goto cleanup;
file_error:
   res = LDR_FORMAT_DETECT_ERROR;
cleanup:
   fclose(fp);
   return res;
}


#include <stdint.h>
#include <stdio.h>
#include "output.h"


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

typedef struct __attribute__((packed)) Elf32_Ehdr {
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

#define PF_R       0x4
#define PF_W       0x2
#define PF_X       0x1

typedef struct __attribute__((packed)) Elf32_Phdr {
  Elf32_Word  p_type;
  Elf32_Off   p_offset;
  Elf32_Addr  p_vaddr;
  Elf32_Addr  p_paddr;
  Elf32_Word  p_filesz;
  Elf32_Word  p_memsz;
  Elf32_Word  p_flags;
  Elf32_Word  p_align;
} Elf32_Phdr;

#define NUM_SECTIONS 2

typedef struct __attribute__((packed)) t_outputELFHead {
   Elf32_Ehdr e;
   Elf32_Phdr p[NUM_SECTIONS];
} t_outputELFHead;


Elf32_Phdr outputSecToELFPHdr(t_objSection *sec, Elf32_Addr *curWriteAddr)
{
   Elf32_Phdr phdr = { 0 };
   uint32_t secSize = objSecGetSize(sec);

   phdr.p_type = PT_LOAD;
   phdr.p_vaddr = objSecGetStart(sec);
   phdr.p_offset = *curWriteAddr;
   *curWriteAddr += secSize;
   phdr.p_filesz = secSize;
   phdr.p_memsz = secSize;
   phdr.p_flags = PF_R | PF_W | PF_X;
   phdr.p_align = 0;

   return phdr;
}

t_outError outputSecContentToFile(FILE *fp, off_t whence, t_objSection *sec)
{
   t_objSecItem *itm;
   uint8_t tmp;
   int i;

   if (fseeko(fp, whence, SEEK_SET) < 0)
      return OUT_FILE_ERROR;
   
   itm = objSecGetItemList(sec);
   for (; itm != NULL; itm = itm->next) {
      if (itm->class == OBJ_SEC_ITM_CLASS_VOID)
         continue;
      if (itm->class != OBJ_SEC_ITM_CLASS_DATA)
         return OUT_INVALID_BINARY;

      if (itm->body.data.initialized) {
         if (fwrite(itm->body.data.data, itm->body.data.dataSize, 1, fp) < 1)
            return OUT_FILE_ERROR;
      } else {
         tmp = 0;
         for (i=0; i<itm->body.data.dataSize; i++)
            if (fwrite(&tmp, sizeof(uint8_t), 1, fp) < 1)
               return OUT_FILE_ERROR;
      }
   }
   return OUT_NO_ERROR;
}

t_outError outputToELF(t_object *obj, const char *fname)
{
   FILE *fp;
   t_outError res = OUT_NO_ERROR;
   t_objLabel *l_entry;
   t_outputELFHead head = { 0 };
   Elf32_Addr dataAddr = sizeof(t_outputELFHead);

   head.e.e_ident[EI_MAG0] = 0x7F;
   head.e.e_ident[EI_MAG1] = 'E';
   head.e.e_ident[EI_MAG2] = 'L';
   head.e.e_ident[EI_MAG3] = 'F';
   head.e.e_ident[EI_CLASS] = ELFCLASS32;
   head.e.e_ident[EI_DATA] = ELFDATA2LSB;
   head.e.e_ident[EI_VERSION] = 1;
   head.e.e_type = ET_EXEC;
   head.e.e_machine = EM_RISCV;
   head.e.e_version = 1;
   head.e.e_phoff = ((intptr_t)head.p - (intptr_t)&head.e);
   head.e.e_shoff = 0;
   head.e.e_flags = 0;
   head.e.e_ehsize = sizeof(Elf32_Ehdr);
   head.e.e_phentsize = sizeof(Elf32_Phdr);
   head.e.e_phnum = NUM_SECTIONS;
   head.e.e_shentsize = 0;
   head.e.e_shnum = 0;
   head.e.e_shstrndx = 0;

   l_entry = objGetLabel(obj, "_start");
   if (!l_entry) {
      fprintf(stderr, "warning: _start symbol not found, entry will be start of .text section\n");
      head.e.e_entry = objSecGetStart(objGetSection(obj, OBJ_SECTION_TEXT));
   } else {
      head.e.e_entry = objLabelGetPointer(l_entry);
   }

   head.p[0] = outputSecToELFPHdr(objGetSection(obj, OBJ_SECTION_TEXT), &dataAddr);
   head.p[1] = outputSecToELFPHdr(objGetSection(obj, OBJ_SECTION_DATA), &dataAddr);

   fp = fopen(fname, "w");
   if (fp == NULL)
      return OUT_FILE_ERROR;
   if (fwrite(&head, sizeof(t_outputELFHead), 1, fp) < 1) {
      res = OUT_FILE_ERROR;
      goto exit;
   }
   res = outputSecContentToFile(fp, head.p[0].p_offset, objGetSection(obj, OBJ_SECTION_TEXT));
   if (res != OUT_NO_ERROR)
      goto exit;
   res = outputSecContentToFile(fp, head.p[1].p_offset, objGetSection(obj, OBJ_SECTION_DATA));
   if (res != OUT_NO_ERROR)
      goto exit;

exit:
   fclose(fp);
   return res;
}

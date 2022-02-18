#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "output.h"


typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT     16

#define EI_MAG0        0   /* File identification */
#define EI_MAG1        1   /* File identification */
#define EI_MAG2        2   /* File identification */
#define EI_MAG3        3   /* File identification */
#define EI_CLASS       4   /* File class */
#define EI_DATA        5   /* Data encoding */
#define EI_VERSION     6   /* File version */
#define EI_PAD         7   /* Start of padding bytes */
   
#define ELFCLASSNONE   0   /* Invalid class  */
#define ELFCLASS32     1   /* 32-bit objects */
   
#define ELFDATANONE    0   /* Invalid data encoding */
#define ELFDATA2LSB    1   /* Little-endian */
   
#define ET_NONE        0   /* No file type */
#define ET_EXEC        2   /* Executable file */
   
#define EM_NONE        0   /* No machine */
#define EM_RISCV    0xF3   /* RISC-V */

typedef struct __attribute__((packed)) Elf32_Ehdr {
   unsigned char e_ident[EI_NIDENT];
   Elf32_Half e_type;
   Elf32_Half e_machine;
   Elf32_Word e_version;
   Elf32_Addr e_entry;
   Elf32_Off e_phoff;
   Elf32_Off e_shoff;
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

#define PF_R       0x4     /* Program segment Read flag */
#define PF_W       0x2     /* Program segment Write flag */
#define PF_X       0x1     /* Program segment eXecute flag */

typedef struct __attribute__((packed)) Elf32_Phdr {
   Elf32_Word p_type;
   Elf32_Off p_offset;
   Elf32_Addr p_vaddr;
   Elf32_Addr p_paddr;
   Elf32_Word p_filesz;
   Elf32_Word p_memsz;
   Elf32_Word p_flags;
   Elf32_Word p_align;
} Elf32_Phdr;

#define SHN_UNDEF     0          /* undefined section ID */

#define SHT_NULL      0          /* null section */
#define SHT_PROGBITS  1          /* section loaded with the program */
#define SHT_STRTAB    3          /* string table */

#define SHF_WRITE     (1 << 0)   /* section writable flag */
#define SHF_ALLOC     (1 << 1)   /* section initialized flag */
#define SHF_EXECINSTR (1 << 2)   /* section executable flag */
#define SHF_STRINGS   (1 << 5)   /* section string table flag */

typedef struct __attribute__((packed)) Elf32_Shdr {
   Elf32_Word sh_name;
   Elf32_Word sh_type;
   Elf32_Word sh_flags;
   Elf32_Addr sh_addr;
   Elf32_Off sh_offset;
   Elf32_Word sh_size;
   Elf32_Word sh_link;
   Elf32_Word sh_info;
   Elf32_Word sh_addralign;
   Elf32_Word sh_entsize;
} Elf32_Shdr;


typedef struct t_outStrTbl {
   char *buf;
   size_t bufSz;
   off_t tail;
} t_outStrTbl;

t_outError outStrTblInit(t_outStrTbl *tbl)
{
   tbl->bufSz = 64;
   tbl->tail = 0;
   tbl->buf = calloc(tbl->bufSz, sizeof(char));
   if (!tbl->buf)
      return OUT_MEMORY_ERROR;
   tbl->buf[tbl->tail++] = '\0';
   return OUT_NO_ERROR;
}

void outStrTblDeinit(t_outStrTbl *tbl)
{
   free(tbl->buf);
}

t_outError outStrTblAddString(t_outStrTbl *tbl, char *str, Elf32_Word *outIdx)
{
   size_t strSz, newBufSz;
   char *newBuf;

   strSz = strlen(str) + 1;
   if (tbl->bufSz - tbl->tail < strSz) {
      newBufSz = tbl->bufSz * 2 + strSz;
      newBuf = realloc(tbl->buf, tbl->bufSz);
      if (!newBuf)
         return OUT_MEMORY_ERROR;
      tbl->buf = newBuf;
      tbl->bufSz = newBufSz;
   }
   strcpy(tbl->buf + tbl->tail, str);
   if (outIdx)
      *outIdx = tbl->tail;
   tbl->tail += strSz;
   return OUT_NO_ERROR;
}

Elf32_Shdr outputStrTabToELFSHdr(t_outStrTbl *tbl, Elf32_Addr fileOffset, Elf32_Word name)
{
   Elf32_Shdr shdr = { 0 };

   shdr.sh_name = name;
   shdr.sh_type = SHT_STRTAB;
   shdr.sh_flags = SHF_STRINGS;
   shdr.sh_addr = 0;
   shdr.sh_offset = fileOffset;
   shdr.sh_size = tbl->tail;
   shdr.sh_link = SHN_UNDEF;
   shdr.sh_info = 0;
   shdr.sh_addralign = 0;
   shdr.sh_entsize = 0;

   return shdr;
}

t_outError outputStrTabContentToFile(FILE *fp, off_t whence, t_outStrTbl *tbl)
{
   t_objSecItem *itm;
   uint8_t tmp;
   int i;

   if (fseeko(fp, whence, SEEK_SET) < 0)
      return OUT_FILE_ERROR;
   if (fwrite(tbl->buf, tbl->tail, 1, fp) < 1)
      return OUT_FILE_ERROR;
   return OUT_NO_ERROR;
}


Elf32_Phdr outputSecToELFPHdr(t_objSection *sec, Elf32_Addr fileOffset, Elf32_Word flags)
{
   Elf32_Phdr phdr = { 0 };
   uint32_t secSize = objSecGetSize(sec);

   phdr.p_type = PT_LOAD;
   phdr.p_vaddr = objSecGetStart(sec);
   phdr.p_offset = fileOffset;
   phdr.p_filesz = secSize;
   phdr.p_memsz = secSize;
   phdr.p_flags = flags;
   phdr.p_align = 0;

   return phdr;
}

Elf32_Shdr outputSecToELFSHdr(t_objSection *sec, Elf32_Addr fileOffset, Elf32_Word name, Elf32_Word flags)
{
   Elf32_Shdr shdr = { 0 };

   shdr.sh_name = name;
   shdr.sh_type = SHT_PROGBITS;
   shdr.sh_flags = flags;
   shdr.sh_addr = objSecGetStart(sec);
   shdr.sh_offset = fileOffset;
   shdr.sh_size = objSecGetSize(sec);
   shdr.sh_link = SHN_UNDEF;
   shdr.sh_info = 0;
   shdr.sh_addralign = 0;
   shdr.sh_entsize = 0;

   return shdr;
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


enum {
   PRG_ID_TEXT = 0,
   PRG_ID_DATA,
   PRG_NUM
};

enum {
   SEC_ID_NULL = SHN_UNDEF,
   SEC_ID_TEXT,
   SEC_ID_DATA,
   SEC_ID_SYMTAB,
   SEC_NUM
};

typedef struct __attribute__((packed)) t_outputELFHead {
   Elf32_Ehdr e;
   Elf32_Phdr p[PRG_NUM];
   Elf32_Shdr s[SEC_NUM];
} t_outputELFHead;

t_outError outputToELF(t_object *obj, const char *fname)
{
   FILE *fp = NULL;
   t_outError res = OUT_NO_ERROR;
   t_objLabel *l_entry;
   t_objSection *text, *data;
   t_outStrTbl strTbl;
   t_outputELFHead head = { 0 };
   Elf32_Word textSecName, dataSecName, strtabSecName;
   Elf32_Addr textAddr, dataAddr, strtabAddr;

   res = outStrTblInit(&strTbl);
   if (res != OUT_NO_ERROR)
      return res;

   text = objGetSection(obj, OBJ_SECTION_TEXT);
   data = objGetSection(obj, OBJ_SECTION_DATA);

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
   head.e.e_shoff = ((intptr_t)head.s - (intptr_t)&head.e);
   head.e.e_flags = 0;
   head.e.e_ehsize = sizeof(Elf32_Ehdr);
   head.e.e_phentsize = sizeof(Elf32_Phdr);
   head.e.e_phnum = PRG_NUM;
   head.e.e_shentsize = sizeof(Elf32_Shdr);
   head.e.e_shnum = SEC_NUM;
   head.e.e_shstrndx = SEC_ID_SYMTAB;

   l_entry = objGetLabel(obj, "_start");
   if (!l_entry) {
      fprintf(stderr, "warning: _start symbol not found, entry will be start of .text section\n");
      head.e.e_entry = objSecGetStart(text);
   } else {
      head.e.e_entry = objLabelGetPointer(l_entry);
   }

   textAddr = sizeof(t_outputELFHead);
   dataAddr = textAddr + objSecGetSize(text);
   strtabAddr = dataAddr + objSecGetSize(data);

   res = outStrTblAddString(&strTbl, ".text", &textSecName);
   if (res != OUT_NO_ERROR)
      goto exit;
   res = outStrTblAddString(&strTbl, ".data", &dataSecName);
   if (res != OUT_NO_ERROR)
      goto exit;
   res = outStrTblAddString(&strTbl, ".strtab", &strtabSecName);
   if (res != OUT_NO_ERROR)
      goto exit;

   head.p[PRG_ID_TEXT] = outputSecToELFPHdr(text, textAddr, PF_R+PF_X);
   head.p[PRG_ID_DATA] = outputSecToELFPHdr(data, dataAddr, PF_R+PF_W);

   head.s[SEC_ID_TEXT] = outputSecToELFSHdr(text, textAddr, textSecName, SHF_ALLOC+SHF_EXECINSTR);
   head.s[SEC_ID_DATA] = outputSecToELFSHdr(data, dataAddr, dataSecName, SHF_ALLOC+SHF_WRITE);
   head.s[SEC_ID_SYMTAB] = outputStrTabToELFSHdr(&strTbl, strtabAddr, strtabSecName);

   fp = fopen(fname, "w");
   if (fp == NULL) {
      res = OUT_FILE_ERROR;
      goto exit;
   }
   if (fwrite(&head, sizeof(t_outputELFHead), 1, fp) < 1) {
      res = OUT_FILE_ERROR;
      goto exit;
   }
   res = outputSecContentToFile(fp, textAddr, text);
   if (res != OUT_NO_ERROR)
      goto exit;
   res = outputSecContentToFile(fp, dataAddr, data);
   if (res != OUT_NO_ERROR)
      goto exit;
   res = outputStrTabContentToFile(fp, strtabAddr, &strTbl);
   if (res != OUT_NO_ERROR)
      goto exit;

exit:
   outStrTblDeinit(&strTbl);
   if (fp)
      fclose(fp);
   return res;
}

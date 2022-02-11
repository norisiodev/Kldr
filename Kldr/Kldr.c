/*
 * The application to load the linux kernel and initrd.
 * 
 * Copyright (c) 2022 norisio.dev
 * 
 * SPDX short identifier: MIT
 * 
 */

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/UnicodeCollation.h>
#include <Library/UefiDevicePathLib/UefiDevicePathLib.h>

void EFIAPI boot_lin64(UINT64, UINT64);
void EFIAPI _sgdt(UINT64);
void EFIAPI _lgdt(UINT64);
UINT64 EFIAPI getcs(void);
UINT64 EFIAPI getds(void);
UINT64 EFIAPI getss(void);
UINT64 EFIAPI chg_csds(UINT64 cs, UINT64 ds);

#pragma pack(push, 1)

#define E820_MAX_ENTRIES_ZEROPAGE 128

typedef struct {
   UINT16   limit;
   UINT64   addr;
} GDTR;

typedef struct {
   UINT16   lim_0_15;
   UINT16   bas_0_15;
   UINT8    bas_16_23;
   UINT8    access;
   UINT8    gran;
   UINT8    bas_24_31;
} DESC;

typedef struct {
   UINT8    setup_sects;
   UINT16   root_flags;
   UINT32   syssize;
   UINT16   ram_size;
   UINT16   vid_mode;
   UINT16   root_dev;
   UINT16   boot_flag;
   UINT16   jump;
   UINT32   header;
   UINT16   version;
   UINT32   realmode_swtch;
   UINT16   start_sys_seg;
   UINT16   kernel_version;
   UINT8    type_of_loader;
   UINT8    loadflags;
   UINT16   setup_move_size;
   UINT32   code32_start;
   UINT32   ramdisk_image;
   UINT32   ramdisk_size;
   UINT32   bootsect_kludge;
   UINT16   heap_end_ptr;
   UINT8    ext_loader_ver;
   UINT8    ext_loader_type;
   UINT32   cmd_line_ptr;
   UINT32   initrd_addr_max;
   UINT32   kernel_alignment;
   UINT8    relocatable_kernel;
   UINT8    min_alignment;
   UINT16   xloadflags;
   UINT32   cmdline_size;
   UINT32   hardware_subarch;
   UINT64   hardware_subarch_data;
   UINT32   payload_offset;
   UINT32   payload_length;
   UINT64   setup_data;
   UINT64   pref_address;
   UINT32   init_size;
   UINT32   handover_offset;
   UINT32   kernel_info_offset;
} setup_header;

typedef struct {
   UINT8    orig_x;
   UINT8    orig_y;
   UINT16   ext_mem_k;
   UINT16   orig_video_page;
   UINT8    orig_video_mode;
   UINT8    orig_video_cols;
   UINT8    flags;
   UINT8    unused2;
   UINT16   orig_video_ega_bx;
   UINT16   unused3;
   UINT8    orig_video_lines;
   UINT8    orig_video_isVGA;
   UINT16   orig_video_points;

   /* VESA */
   UINT16   lfb_width;
   UINT16   lfb_height;
   UINT16   lfb_depth;
   UINT32   lfb_base;
   UINT32   lfb_size;
   UINT16   cl_magic, cl_offset;
   UINT16   lfb_linelength;
   UINT8    red_size;
   UINT8    red_pos;
   UINT8    green_size;
   UINT8    green_pos;
   UINT8    blue_size;
   UINT8    blue_pos;
   UINT8    rsvd_size;
   UINT8    rsvd_pos;
   UINT16   vesapm_seg;
   UINT16   vesapm_off;
   UINT16   pages;
   UINT16   vesa_attributes;
   UINT32   capabilities;
   UINT32   ext_lfb_base;
   UINT8    rsvd[2];
} screen_info;

typedef struct {
   UINT32   efi_loader_signature;
   UINT32   efi_systab;
   UINT32   efi_memdesc_size;
   UINT32   efi_memdesc_version;
   UINT32   efi_memmap;
   UINT32   efi_memmap_size;
   UINT32   efi_systab_hi;
   UINT32   efi_memmap_hi;
} efi_info;

typedef struct {
   UINT64   addr;
   UINT64   size;
   UINT32   type;
} e820_entry;

typedef struct {
   screen_info screen_info;

   UINT8       _rsvd1[24];

   UINT64      tboot_addr;

   UINT8       _rsvd2[16];

   UINT64      acpi_rsdp_addr;

   UINT8       _rsvd3[72];

   UINT32      ext_ramdisk_image;
   UINT32      ext_ramdisk_size;
   UINT32      ext_cmd_line_ptr;

   UINT8       _rsvd4[244];

   efi_info    efi_info;

   UINT8       _rsvd5[4];

   UINT32      scratch;
   UINT8       e820_entries;

   UINT8       _rsvd6[2];

   UINT8       kbd_status;
   UINT8       secure_boot;

   UINT8       _rsvd7[2];

   UINT8       sentinel;            // 0xff

   UINT8       _rsvd8[1];

   setup_header hdr;                // setup header
   UINT8       _rsvd9[0x290 - 0x1f1 - sizeof(setup_header)];

   UINT32      _rsvd10[16];

   e820_entry  e820_table[E820_MAX_ENTRIES_ZEROPAGE];

   UINT8       _rsvd11[816];
} boot_params;

#pragma pack(pop)

#define AddressRangeMemory             1
#define AddressRangeReserved           2
#define AddressRangeACPI               3
#define AddressRangeNVS                4
#define AddressRangeUnusable           5
#define AddressRangeDisabled           6
#define AddressRangePersistentMemory   7

UINT32 type_efi_to_acpi(UINT32 type)
{
   UINT32 cnv[] = {
      AddressRangeReserved,         // 0 EfiReservedMemoryType
      AddressRangeMemory,           // 1 EfiLoaderCode
      AddressRangeMemory,           // 2 EfiLoaderData
      AddressRangeMemory,           // 3 EfiBootServicesCode
      AddressRangeMemory,           // 4 EfiBootServicesData
      AddressRangeReserved,         // 5 EfiRuntimeServiceCode
      AddressRangeReserved,         // 6 EfiRuntimeServicesData
      AddressRangeMemory,           // 7 EfiConventionalMemory
      AddressRangeReserved,         // 8 EfiUnusableMemory
      AddressRangeACPI,             // 9 EfiACPIReclaimMemory
      AddressRangeNVS,              //10 EfiACPIMemoryNVS
      AddressRangeReserved,         //11 EfiMemoryMappedIO
      AddressRangeReserved,         //12 EfiMemoryMappedIOPortSpace
      AddressRangeReserved,         //13 EfiPalCode
      AddressRangePersistentMemory, //14 EfiPersistentMemory
      AddressRangeReserved          //15 Reserved
   };

   if (type <= 15) {
      return cnv[type];
   } else {
      return AddressRangeReserved;
   }
}

VOID cnv_efi_to_acpi(e820_entry* acpi, const EFI_MEMORY_DESCRIPTOR* efi)
{
   acpi->addr = efi->PhysicalStart;
   acpi->size = efi->NumberOfPages * 4096;
   acpi->type = type_efi_to_acpi(efi->Type);
}

CHAR16 getchar(VOID)
{
   EFI_INPUT_KEY key;

   EFI_STATUS  Status;

   do {
      Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
   } while (Status == EFI_NOT_READY);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   return key.UnicodeChar;
}

VOID* malloc_pool(UINTN size)
{
   VOID* ptr;

   EFI_STATUS  Status;

   Status = gBS->AllocatePool(EfiLoaderData, size, &ptr);
   if (EFI_ERROR(Status)) {
      return 0; // EFI_OUT_OF_RESOURCES;
   }

   return ptr;
}

VOID free_pool(VOID* ptr)
{
   gBS->FreePool(ptr);
}

VOID* malloc_pages(UINT64 size)
{
   EFI_PHYSICAL_ADDRESS addr;

   EFI_STATUS  Status;

   Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, (size + 4095) / 4096, &addr);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   return (VOID*)addr;
}

VOID free_pages(VOID* ptr, UINT64 size)
{
   gBS->FreePages((EFI_PHYSICAL_ADDRESS)ptr, (size + 4095) / 4096);
}

VOID* malloc_pages_at(UINT64 size, EFI_PHYSICAL_ADDRESS addr)
{
   EFI_STATUS  Status;

   Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, (size + 4095) / 4096, &addr);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   return (VOID*)addr;
}


EFI_STATUS get_file_size(EFI_FILE_PROTOCOL* file, UINT64* size)
{
   EFI_GUID       finfo_guid = EFI_FILE_INFO_ID;
   EFI_FILE_INFO* finfo;
   UINTN          finfo_size;

   EFI_STATUS  Status;

   if (!size) {
      return EFI_INVALID_PARAMETER;
   }

   finfo = 0;
   finfo_size = 0;
   Status = file->GetInfo(file, &finfo_guid, &finfo_size, finfo);
   if (Status != EFI_BUFFER_TOO_SMALL) {
      return Status;
   }

   finfo = malloc_pool(finfo_size);
   if (!finfo) {
      return EFI_OUT_OF_RESOURCES;
   }

   Status = file->GetInfo(file, &finfo_guid, &finfo_size, finfo);
   if (EFI_ERROR(Status)) {
      free_pool(finfo);
      return Status;
   }

   *size = finfo->FileSize;

   free_pool(finfo);

   return EFI_SUCCESS;
}

EFI_STATUS chk_linux(setup_header* header)
{
   if (header->boot_flag != 0xaa55) {
      return EFI_NOT_FOUND;
   }

   if (header->header != 0x53726448UL) { // 'HdrS'
      return EFI_NOT_FOUND;
   }

   //Print(L"boot protocol ver.%d.%d\r\n", header->version >> 8, header->version & 0xff);

   if (header->version < 0x020a) { // old boot protocol is not supported
      return EFI_UNSUPPORTED;
   }

   if (!(header->loadflags & 0x01)) { // not LOADED_HIGH
      return EFI_UNSUPPORTED;
   }

   if (!(header->xloadflags & 0x01)) { // not XFL_KERNEL_64
      return EFI_UNSUPPORTED;
   }

   if (header->setup_sects == 0) {
      header->setup_sects = 4;
   }

   return EFI_SUCCESS;
}

VOID initE820(boot_params* params, UINT8* MemoryMap, UINTN MapSize, UINTN DescSize)
{
   UINTN       Count = MapSize / DescSize;
   e820_entry  e820[E820_MAX_ENTRIES_ZEROPAGE];
   UINT8       e_count;
   UINT8*      tmp;

   tmp = MemoryMap;
   e_count = 0;
   for (UINTN i = 0; i < Count; ++i) {
      e820_entry  entry;
      cnv_efi_to_acpi(&entry, (EFI_MEMORY_DESCRIPTOR*)tmp);

      if (e_count == 0) {
         CopyMem(&e820[e_count], &entry, sizeof(e820_entry));
         ++e_count;

      } else {
         if (entry.type == e820[e_count - 1].type
          && entry.addr == e820[e_count - 1].addr + e820[e_count - 1].size) {
            e820[e_count - 1].size += entry.size;

         } else {
            CopyMem(&e820[e_count], &entry, sizeof(e820_entry));
            ++e_count;

            if (e_count == E820_MAX_ENTRIES_ZEROPAGE) {
               break;
            }
         }
      }

      tmp += DescSize;
   }

   for (int i = 0; i < e_count; ++i) {
      params->e820_table[i].addr = e820[i].addr;
      params->e820_table[i].size = e820[i].size;
      params->e820_table[i].type = e820[i].type;
   }
   params->e820_entries = e_count;
}

UINT64 FindRSDP(VOID)
{
   for (UINTN i = 0; i < gST->NumberOfTableEntries; ++i) {
      if (CompareGuid(&(gST->ConfigurationTable[i].VendorGuid), &gEfiAcpi20TableGuid)) {
         return (UINT64)gST->ConfigurationTable[i].VendorTable;
      }
   }

   return 0;
}

UINT8 count_bits(UINT32 c)
{
   UINT8 bits = 0;

   while (c) {
      if (c & 0x01) {
         ++bits;
      }
      c >>= 1;
   }

   return bits;
}

UINT8 start_bit(UINT32 c)
{
   UINT8 n = 0;

   if (!c) {
      return 0;
   }
   while (c) {
      if (c & 0x01) {
         return n;
      }
      c >>= 1;
      ++n;
   }

   return n;
}

VOID dump_gdt(GDTR* gdtr)
{
   DESC* desc = (DESC*)gdtr->addr;
   UINTN i;

   Print(L"cs = %x, ds = %x, ss = %x\r\n", getcs(), getds(), getss());

   for (i = 0; i < (gdtr->limit + 1) / 8; ++i) {
      Print(L"%x lim_0_15:%x bas_0_15:%x bas_16_23:%x access:%x gran:%x bas_24_31:%x\r\n",
            i * 8,
            desc[i].lim_0_15,
            desc[i].bas_0_15,
            desc[i].bas_16_23,
            desc[i].access,
            desc[i].gran,
            desc[i].bas_24_31
         );
   }
}

VOID modify_gdt(GDTR* gdtr, UINTN new_cs, UINTN new_ds)
{
   DESC* desc = (DESC*)gdtr->addr;

   desc[new_cs / 8].lim_0_15 = 0xffff;
   desc[new_cs / 8].bas_0_15 = 0x0000;
   desc[new_cs / 8].bas_16_23 = 0x00;
   desc[new_cs / 8].access = 0x9a;
   desc[new_cs / 8].gran = 0xaf;
   desc[new_cs / 8].bas_24_31 = 0x00;

   desc[new_ds / 8].lim_0_15 = 0xffff;
   desc[new_ds / 8].bas_0_15 = 0x0000;
   desc[new_ds / 8].bas_16_23 = 0x00;
   desc[new_ds / 8].access = 0x92;
   desc[new_ds / 8].gran = 0xcf;
   desc[new_ds / 8].bas_24_31 = 0x00;
}

EFI_STATUS setup_desc(GDTR* gdtr, UINT64* cs, UINT64* ds)
{
   DESC* new_desc;
   UINTN new_desc_size;

   _sgdt((UINT64)gdtr);

   //Print(L"sgdt.limit = %x, ", gdtr->limit);
   //Print(L"sgdt.addr  = %lx\r\n", gdtr->addr);

   //dump_gdt(gdtr);

   new_desc_size = gdtr->limit + 1 + sizeof(DESC) * 2;
   if (new_desc_size > 0x10000) {
      return EFI_OUT_OF_RESOURCES;
   }
   *cs = gdtr->limit + 1;
   *ds = *cs + 0x08;

   new_desc = malloc_pool(new_desc_size);
   if (!new_desc) {
      return EFI_OUT_OF_RESOURCES;
   }

   CopyMem((VOID*)new_desc, (VOID*)gdtr->addr, gdtr->limit + 1);

   gdtr->limit = (UINT16)(new_desc_size - 1);
   gdtr->addr = (UINT64)new_desc;

   modify_gdt(gdtr, *cs, *ds);

   //dump_gdt(gdtr);

   return EFI_SUCCESS;
}

VOID release_desc(GDTR* gdtr)
{
   free_pool((VOID*)gdtr->addr);
}

boot_params* init_zeropage(VOID)
{
   boot_params*   params;

   params = malloc_pool(sizeof(boot_params));
   if (!params) {
      return 0;
   }
   SetMem((VOID*)params, sizeof(boot_params), 0);

   return params;
}

VOID release_zeropage(boot_params* params)
{
   UINT64 ptr;

   // free kernel pages
   //
   if (params->hdr.pref_address) {
      free_pages((VOID*)params->hdr.pref_address, params->hdr.init_size);
   }

   // free command line pool
   //
   ptr = (UINT64)params->ext_cmd_line_ptr << 32;
   ptr += params->hdr.cmd_line_ptr;
   if (ptr) {
      free_pool((VOID*)ptr);
   }

   // free initrd pages
   //
   ptr = (UINT64)params->ext_ramdisk_image << 32;
   ptr += params->hdr.ramdisk_image;
   if (ptr) {
      UINT64 size;

      size = (UINT64)params->ext_ramdisk_size << 32;
      size += params->hdr.ramdisk_size;

      free_pages((VOID*)ptr, size);
   }

   // free efi memory map pool
   //
   ptr = (UINT64)params->efi_info.efi_memmap_hi << 32;
   ptr += params->efi_info.efi_memmap;
   if (ptr) {
      free_pool((VOID*)ptr);
   }

   // free params pool
   //
   free_pool(params);
}

EFI_STATUS load_kernel_header(boot_params* params, EFI_FILE_PROTOCOL* file)
{
   UINT8 header_size;
   UINTN size;

   EFI_STATUS  Status;

   // get setup header size
   //
   Status = file->SetPosition(file, 0x201);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   size = 1;
   Status = file->Read(file, &size, &header_size);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   size = (UINTN)header_size + 0x202;

   // read setup header
   //
   Status = file->SetPosition(file, 0x1f1);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = file->Read(file, &size, &(params->hdr));
   if (EFI_ERROR(Status)) {
      SetMem(&(params->hdr), size, 0); // clear header
      return Status;
   }

   // check header
   //
   Status = chk_linux(&(params->hdr));
   if (EFI_ERROR(Status)) {
      SetMem(&(params->hdr), size, 0); // clear header
      return Status;
   }

   params->hdr.type_of_loader = 0xff;
   params->hdr.vid_mode = 0xffff;

   if (params->hdr.kernel_version) {
      Status = file->SetPosition(file, 512 + params->hdr.kernel_version);

      if (!EFI_ERROR(Status)) {
         char kver[512];

         size = 512;
         Status = file->Read(file, &size, kver);

         if (!EFI_ERROR(Status)) {
            kver[511] = 0;

            Print(L"linux %a\r\n", kver);
         }
      }
   }

   params->sentinel = 0xff;

   return EFI_SUCCESS;
}


EFI_STATUS load_linux32(EFI_FILE_PROTOCOL* file, setup_header* header)
{
   UINTN size;
   VOID* prot;

   EFI_STATUS  Status;

   size = header->syssize * 16;
   //Print(L"protected code size = %d\r\n", size);

   if (header->relocatable_kernel) {
      prot = malloc_pages(header->init_size);
      if (!prot) {
         header->pref_address = 0;
         return EFI_OUT_OF_RESOURCES;
      }
      header->pref_address = (UINT64)prot;

   } else {
      prot = malloc_pages_at(header->init_size, header->pref_address);

      if (!prot) {
         Print(L"cannot allocate kernel memory at %X\r\n", header->pref_address);
         header->pref_address = 0;
         return EFI_OUT_OF_RESOURCES;
      }
   }

   //Print(L"Load at %lx (%d, %d)\r\n", (UINT64) prot, header->syssize * 61, header->init_size);

   Status = file->SetPosition(file, (1 + header->setup_sects) * 512);
   if (EFI_ERROR(Status)) {
      header->pref_address = 0;
      return Status;
   }

   Status = file->Read(file, &size, prot);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   //Print(L"load %d bytes\r\n", size);

   return EFI_SUCCESS;
}

EFI_STATUS load_kernel(EFI_FILE_PROTOCOL* root, CHAR16* bzImage, boot_params* params)
{
   EFI_FILE_PROTOCOL*   file;

   EFI_STATUS  Status;

   Status = root->Open(root, &file, bzImage, EFI_FILE_MODE_READ, 0);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = load_kernel_header(params, file);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = load_linux32(file, &params->hdr);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = file->Close(file);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return EFI_SUCCESS;
}

EFI_STATUS init_cmdline(EFI_FILE_PROTOCOL* root, CHAR16* config, boot_params* params)
{
   CHAR8*   cmdline;
   UINT64   size;
   UINTN    read;

   EFI_FILE_PROTOCOL*   file;

   EFI_STATUS  Status;

   Status = root->Open(root, &file, config, EFI_FILE_MODE_READ, 0);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = get_file_size(file, &size);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   cmdline = malloc_pool(size + 1);
   if (EFI_ERROR(Status)) {
      return EFI_OUT_OF_RESOURCES;
   }

   params->hdr.cmd_line_ptr = (UINT64)cmdline & 0xffffffff;
   params->ext_cmd_line_ptr = (UINT64)cmdline >> 32;

   read = size;
   Status = file->Read(file, &read, cmdline);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   cmdline[read] = 0;

   Status = file->Close(file);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return EFI_SUCCESS;
}


EFI_STATUS load_initrd(EFI_FILE_PROTOCOL* root, CHAR16* initrd, boot_params* params)
{
   VOID*    load_addr;
   UINT64   size;
   //UINT64 initrd_max_addr;

   EFI_FILE_PROTOCOL*   file;

   EFI_STATUS  Status;

   Status = root->Open(root, &file, initrd, EFI_FILE_MODE_READ, 0);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   //initrd_max_addr = params->hdr.initrd_addr_max;
   //Print(L"initrd_addr_max = %lx\r\n", initrd_max_addr);

   Status = get_file_size(file, &size);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   load_addr = malloc_pages(size);
   if (!load_addr) {
      return EFI_OUT_OF_RESOURCES;
   }

   Print(L"initrd load address = %lx\r\n", (UINT64)load_addr);
   Print(L"initrd size = %ld\r\n", size);

   params->hdr.ramdisk_image = (UINT64)load_addr & 0xffffffff;
   params->ext_ramdisk_image = (UINT64)load_addr >> 32;

   params->hdr.ramdisk_size = size & 0xffffffff;
   params->ext_ramdisk_size = size >> 32;

   Status = file->Read(file, &size, load_addr);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = file->Close(file);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return EFI_SUCCESS;
}

EFI_STATUS setup_graphics(boot_params* params, UINT32* orig)
{
   EFI_GRAPHICS_OUTPUT_PROTOCOL* gout;

   EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*  ginfo;
   UINTN                                  ginfo_size;

   EFI_STATUS  Status;

   Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gout);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   *orig = gout->Mode->Mode;

   ginfo_size = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
   ginfo = malloc_pool(ginfo_size);
   if (!ginfo) {
      return EFI_OUT_OF_RESOURCES;
   }

   Status = gout->QueryMode(gout, gout->Mode->MaxMode - 1, &ginfo_size, &ginfo);
   if (EFI_ERROR(Status)) {
      free_pool(ginfo);
      return Status;
   }

   params->screen_info.lfb_width  = (UINT16)ginfo->HorizontalResolution;
   params->screen_info.lfb_height = (UINT16)ginfo->VerticalResolution;

   switch (ginfo->PixelFormat) {
      case PixelRedGreenBlueReserved8BitPerColor:
         Print(L"PixelRedGreenBlueReserved8BitPerColor\r\n");

         params->screen_info.lfb_depth  = 32;

         params->screen_info.red_size   = 8;
         params->screen_info.green_size = 8;
         params->screen_info.blue_size  = 8;
         params->screen_info.rsvd_size  = 8;

         params->screen_info.red_pos    = 0;
         params->screen_info.green_pos  = 8;
         params->screen_info.blue_pos   = 16;
         params->screen_info.rsvd_pos   = 24;
         break;

      case PixelBlueGreenRedReserved8BitPerColor:
         Print(L"PixelBlueGreenRedReserved8BitPerColor\r\n");

         params->screen_info.lfb_depth  = 32;

         params->screen_info.red_size   = 8;
         params->screen_info.green_size = 8;
         params->screen_info.blue_size  = 8;
         params->screen_info.rsvd_size  = 8;

         params->screen_info.blue_pos   = 0;
         params->screen_info.green_pos  = 8;
         params->screen_info.red_pos    = 16;
         params->screen_info.rsvd_pos   = 24;
         break;

      case PixelBitMask:
         Print(L"PixelBitMask\r\n");

         params->screen_info.red_size   = count_bits(ginfo->PixelInformation.RedMask);
         params->screen_info.green_size = count_bits(ginfo->PixelInformation.GreenMask);
         params->screen_info.blue_size  = count_bits(ginfo->PixelInformation.BlueMask);
         params->screen_info.rsvd_size  = count_bits(ginfo->PixelInformation.ReservedMask);

         params->screen_info.lfb_depth  = params->screen_info.red_size
            + params->screen_info.green_size
            + params->screen_info.blue_size
            + params->screen_info.rsvd_size;

         params->screen_info.red_pos   = start_bit(ginfo->PixelInformation.RedMask);
         params->screen_info.green_pos = start_bit(ginfo->PixelInformation.GreenMask);
         params->screen_info.blue_pos  = start_bit(ginfo->PixelInformation.BlueMask);
         params->screen_info.rsvd_pos  = start_bit(ginfo->PixelInformation.ReservedMask);
         break;

      default:
         break;
   }

   params->screen_info.orig_video_isVGA = 0x70; // VIDEO_TYPE_EFI

   params->screen_info.lfb_base = gout->Mode->FrameBufferBase & 0xffffffff;
   params->screen_info.ext_lfb_base = gout->Mode->FrameBufferBase >> 32;

   params->screen_info.lfb_size = (UINT32)gout->Mode->FrameBufferSize;

   params->screen_info.lfb_linelength = (UINT16)ginfo->PixelsPerScanLine
      * ((params->screen_info.lfb_depth + 7) / 8);

   params->screen_info.capabilities = 0x02; // VIDEO_CAPABILITY_64BIT_BASE

   free_pool(ginfo);

   Status = gout->SetMode(gout, gout->Mode->MaxMode - 1);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return EFI_SUCCESS;
}

EFI_STATUS restore_graphics(UINT32 orig)
{
   EFI_GRAPHICS_OUTPUT_PROTOCOL* gout;

   EFI_STATUS  Status;

   Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&gout);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = gout->SetMode(gout, orig);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return EFI_SUCCESS;
}

EFI_STATUS init_memory_map(UINTN* Key, boot_params* params)
{
   VOID*    MemoryMap;
   UINTN    MapSize;
   UINTN    DescSize;
   UINT32   DescVer;

   EFI_STATUS  Status;

   params->acpi_rsdp_addr = FindRSDP();
   //Print(L"RSDP = %x\r\n", params->acpi_rsdp_addr);
   if (!params->acpi_rsdp_addr) {
      return EFI_NOT_FOUND;
   }

   MapSize = 4096;
   while (1) {
      UINTN sz = MapSize;
      MemoryMap = malloc_pool(MapSize);
      if (!MemoryMap) {
         return EFI_OUT_OF_RESOURCES;
      }
      Status = gBS->GetMemoryMap(
            &MapSize,
            (EFI_MEMORY_DESCRIPTOR*)MemoryMap,
            Key,
            &DescSize,
            &DescVer);

      if (!EFI_ERROR(Status)) {
         break;
      }

      free_pool(MemoryMap);

      if (Status != EFI_BUFFER_TOO_SMALL) {
         return Status;
      }

      MapSize = sz + 4096;
   }

   params->efi_info.efi_memmap = (UINT64)MemoryMap & 0xffffffff;
   params->efi_info.efi_memmap_hi = ((UINT64)MemoryMap >> 32);

   initE820(params, MemoryMap, MapSize, DescSize);

   // "EL64";
   params->efi_info.efi_loader_signature = 'E' + ('L' << 8) + ('6' << 16) + ('4' << 24);
   params->efi_info.efi_memdesc_size = (UINT32)DescSize;
   params->efi_info.efi_memdesc_version = DescVer;
   params->efi_info.efi_memmap_size = (UINT32)MapSize;

   params->efi_info.efi_systab = (UINT64)gST & 0xffffffff;
   params->efi_info.efi_systab_hi = ((UINT64)gST >> 32);

   return EFI_SUCCESS;
}

EFI_STATUS boot_linux(VOID)
{
   EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
   EFI_FILE_PROTOCOL*               root;
   EFI_DEVICE_PATH_PROTOCOL*        root_devpath;

   EFI_GUID       loaded_dp_guid = EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

   EFI_HANDLE     root_handle;

   UINT32         disp_mode;

   boot_params*   params;

   GDTR     gdtr;
   UINT64   tmp_cs;
   UINT64   tmp_ds;

   UINTN    Key;

   EFI_STATUS  Status;

   Status = gBS->HandleProtocol(gImageHandle, &loaded_dp_guid, (VOID**)&root_devpath);
   if (EFI_ERROR(Status)) {
      Print(L"loaded devicepath failed:%r\r\n", Status);
      return Status;
   }

   Status = gBS->LocateDevicePath(&gEfiSimpleFileSystemProtocolGuid, &root_devpath, &root_handle);
   if (EFI_ERROR(Status)) {
      Print(L"locate devicepath failed:%r\r\n", Status);
      return Status;
   }

   Status = gBS->HandleProtocol(root_handle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&fs);
   if (EFI_ERROR(Status)) {
      Print(L"simple file system not found:%r\r\n", Status);
      return Status;
   }

   Status = fs->OpenVolume(fs, &root);
   if (EFI_ERROR(Status)) {
      Print(L"Open Volume failed:%r\r\n", Status);
      return Status;
   }

   params = init_zeropage();
   if (!params) {
      Status = EFI_OUT_OF_RESOURCES;
      Print(L"init zeropage failed:%r\r\n", Status);
      return Status;
   }

   Status = load_kernel(root, L"bzimage", params);
   if (EFI_ERROR(Status)) {
      release_zeropage(params);
      Print(L"bzimage load failed:%r\r\n", Status);
      return Status;
   }

   Status = init_cmdline(root, L"config.txt", params);
   if (EFI_ERROR(Status)) {
      release_zeropage(params);
      Print(L"config.txt load failed:%r\r\n", Status);
      return Status;
   }

   Status = load_initrd(root, L"initrd", params);
   if (EFI_ERROR(Status)) {
      release_zeropage(params);
      Print(L"initrd load failed%r\r\n", Status);
      return Status;
   }

   Status = setup_desc(&gdtr, &tmp_cs, &tmp_ds);
   if (EFI_ERROR(Status)) {
      release_zeropage(params);
      Print(L"setup desc failed%r\r\n", Status);
      return Status;
   }
   //getchar();

   Status = setup_graphics(params, &disp_mode);
   if (EFI_ERROR(Status)) {
      release_desc(&gdtr);
      release_zeropage(params);
      Print(L"setup graphics failed:%r\r\n", Status);
      return Status;
   }

   Status = init_memory_map(&Key, params);
   if (EFI_ERROR(Status)) {
      release_desc(&gdtr);
      release_zeropage(params);
      restore_graphics(disp_mode);
      Print(L"init memory map failed:%r\r\n", Status);
      return Status;
   }

   Status = gBS->ExitBootServices(gImageHandle, Key);
   if (EFI_ERROR(Status)) {
      release_desc(&gdtr);
      release_zeropage(params);
      restore_graphics(disp_mode);
      Print(L"ExitBootServices failed:%d\r\n", Status);
      return Status;
   }

   _lgdt((UINT64)&gdtr);
   chg_csds(tmp_cs, tmp_ds);
   modify_gdt(&gdtr, 0x10, 0x18);

   boot_lin64((UINT64)(params->hdr.pref_address + 0x200), (UINT64)params);

   return EFI_SUCCESS;
}

VOID* get_var(CHAR16* name, UINTN* size)
{
   VOID*    ptr;
   UINT32   attr;

   EFI_STATUS  Status;

   ptr = 0;
   *size = 0;

   Status = gRT->GetVariable(name, &gEfiGlobalVariableGuid, &attr, size, ptr);
   if (Status != EFI_BUFFER_TOO_SMALL) {
      return 0;
   }

   ptr = malloc_pool(*size);
   if (!ptr) {
      return 0;
   }

   Status = gRT->GetVariable(name, &gEfiGlobalVariableGuid, &attr, size, ptr);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   return ptr;
}

EFI_STATUS set_var(CHAR16* name, VOID* ptr, UINTN size)
{
   EFI_STATUS  Status;

   Status = gRT->SetVariable(
         name,
         &gEfiGlobalVariableGuid,
         EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
         size,
         ptr);

   if (EFI_ERROR(Status)) {
      Print(L"ERROR : %r\r\n", Status);
   }

   return EFI_SUCCESS;
}

UINT16* get_bootorder(UINTN* boot_order_size)
{
   UINT16* order;

   order = get_var(L"BootOrder", boot_order_size);
   if (!order) {
      return 0;
   }

   *boot_order_size /= sizeof(UINT16);

   return order;
}

EFI_STATUS set_bootorder(UINT16* boot_order, UINTN boot_order_size)
{
   EFI_STATUS  Status;

   Status = set_var(L"BootOrder", boot_order, boot_order_size * sizeof(UINT16));

   return Status;
}

EFI_STATUS dump_bootoption(UINTN bootorder)
{
   CHAR16   varname[16];
   UINTN    size;

   EFI_LOAD_OPTION*  elo;

   UnicodeSPrint(varname, sizeof(varname), L"Boot%04X", bootorder);
   Print(L"%s : ", varname);

   elo = get_var(varname, &size);
   if (elo) {
      Print(L"[%s]\r\n", (CHAR16*)((UINT8*)elo + sizeof(EFI_LOAD_OPTION)));

      free_pool(elo);
   }

   return EFI_SUCCESS;
}

EFI_STATUS get_var_word(CHAR16* name, UINT16* var)
{
   UINT32   attr;
   UINTN    size = sizeof(UINT16);

   EFI_STATUS  Status;

   Status = gRT->GetVariable(name, &gEfiGlobalVariableGuid, &attr, &size, var);
   if (EFI_ERROR(Status)) {
      Print(L"ERROR : %r\r\n", Status);
   }

   if (size != sizeof(UINT16)) {
      return EFI_NOT_FOUND;
   }

   return EFI_SUCCESS;
}

INTN find_order(UINT16* order, INTN count, UINTN n)
{
   for (INTN i = 0; i < count; ++i) {
      if (order[i] == n) {
         return i;
      }
   }

   return -1;
}

VOID move_top(UINT16* order, INTN count, UINT16 n)
{
   INTN index;

   index = find_order(order, count, n);
   if (index == -1) {
      return;
   }

   for (INTN i = index - 1; 0 <= i; --i) {
      order[i + 1] = order[i];
   }

   order[0] = n;

   return;
}

EFI_STATUS get_param(UINTN* boot, UINTN* install, UINTN* chg_order)
{
   EFI_LOADED_IMAGE_PROTOCOL*       params;
   EFI_UNICODE_COLLATION_PROTOCOL*  uc;

   EFI_GUID uc_guid = EFI_UNICODE_COLLATION_PROTOCOL2_GUID;
   EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

   EFI_STATUS  Status;

   Status = gBS->LocateProtocol(&uc_guid, NULL, (VOID**)&uc);

   Status = gBS->OpenProtocol(
         gImageHandle,
         &li_guid,
         (VOID**)&params,
         gImageHandle,
         NULL,
         EFI_OPEN_PROTOCOL_GET_PROTOCOL);

   if (EFI_ERROR(Status)) {
      return Status;
   }

   // default behavior
   *boot = 1;
   *install = 0;
   *chg_order = 1;

   if (params->LoadOptionsSize) {
      CHAR16*  str;
      CHAR16*  p;
      CHAR16*  end;

      str = malloc_pool(params->LoadOptionsSize / sizeof(CHAR16) + 1);
      if (!str) {
         return EFI_OUT_OF_RESOURCES;
      }

      CopyMem(str, params->LoadOptions, params->LoadOptionsSize);
      str[params->LoadOptionsSize / sizeof(CHAR16)] = 0;

      p = str;
      end = p + params->LoadOptionsSize / sizeof(CHAR16);
      while (p < end) {
         CHAR16* n;
         while ((*p == L' ') || (*p == L'\t')) {
            ++p;
         }
         if (!*p) {
            break;
         }
         n = p;
         while (*n) {
            if ((*n == L' ') || (*p == L'\t')) {
               *n = 0;
               break;
            }
            ++n;
         }
         //Print(L"[%s]\r\n", p);
         if (uc->StriColl(uc, p, L"kldr.efi") == 0) {
            *boot = 0;
            *chg_order = 0;
         } else if (uc->StriColl(uc, p, L"install") == 0) {
            *install = 1;
         } else if (uc->StriColl(uc, p, L"boot") == 0) {
            *boot = 1;
         }
         p = n + 1;
      }
      free_pool(str);
   }

   Status = gBS->CloseProtocol(gImageHandle, &li_guid, gImageHandle, NULL);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return Status;
}

EFI_STATUS assign_order(UINT16* norder)
{
   for (INTN order = 0; order < 0x10000; ++order) {
      CHAR16   varname[16];
      UINTN    size;
      UINT8*   var;

      UnicodeSPrint(varname, sizeof(varname), L"Boot%04X", order);

      var = get_var(varname, &size);
      if (!var) {
         *norder = (UINT16)order;
         return EFI_SUCCESS;
      }

      free_pool(var);
   }

   return EFI_OUT_OF_RESOURCES;
}

EFI_DEVICE_PATH_PROTOCOL* dp_next(EFI_DEVICE_PATH_PROTOCOL* path)
{
   UINTN len;

   if (path->Type == 0x7f) {
      return 0;
   }

   len = (UINTN)path->Length[0] + (UINTN)path->Length[1] * 16;

   return (EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)path + len);
}

VOID dump_dp(EFI_DEVICE_PATH_PROTOCOL* dpath)
{
   EFI_DEVICE_PATH_TO_TEXT_PROTOCOL*   dp;
   EFI_GUID dp_guid = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;

   EFI_STATUS  Status;

   Status = gBS->LocateProtocol(&dp_guid, NULL, (VOID**)&dp);
   if (EFI_ERROR(Status)) {
      return;
   }

   while (1) {
      CHAR16*  str = dp->ConvertDeviceNodeToText(dpath, FALSE, FALSE);
      if (str) {
         Print(L"%s", str);
         gBS->FreePool(str);
      }
      dpath = dp_next(dpath);

      if (dpath->Type == 0x7f && dpath->SubType == 0xff) {
         break;
      } else {
         Print(L"/");
      }
   }

   Print(L"\r\n");
}

EFI_DEVICE_PATH_PROTOCOL* get_media(EFI_DEVICE_PATH_PROTOCOL* path)
{
   while (path->Type != 0x04) {
      path = dp_next(path);
      if (!path) {
         return 0;
      }
   }

   return path;
}

VOID* get_media_dp(UINTN* size)
{
   EFI_GUID ld_guid = EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;
   EFI_GUID du_guid = EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;

   EFI_DEVICE_PATH_PROTOCOL            *devpath;
   EFI_DEVICE_PATH_PROTOCOL            *mpath;
   EFI_DEVICE_PATH_UTILITIES_PROTOCOL  *du;

   EFI_STATUS  Status;

   Status = gBS->LocateProtocol(&du_guid, NULL, (VOID**)&du);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   Status = gBS->OpenProtocol(
         gImageHandle,
         &ld_guid,
         (VOID**)&devpath,
         gImageHandle,
         NULL,
         EFI_OPEN_PROTOCOL_GET_PROTOCOL);

   if (EFI_ERROR(Status)) {
      return 0;
   }

   mpath = get_media(devpath);
   if (mpath == 0) {
      mpath = devpath;
   }

   mpath = du->DuplicateDevicePath(mpath);
   *size = du->GetDevicePathSize(mpath);

   Status = gBS->CloseProtocol(gImageHandle, &ld_guid, gImageHandle, NULL);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   return mpath;
}

EFI_STATUS add_boot_order(UINT16 order)
{
   UINT16*  cur_order;
   UINT16*  new_order;
   UINTN    boot_order_count;

   EFI_STATUS  Status;

   cur_order = get_bootorder(&boot_order_count);
   if (!cur_order) {
      return EFI_OUT_OF_RESOURCES;
   }

   new_order = malloc_pool((boot_order_count + 1) * sizeof(UINT16));
   if (!new_order) {
      free_pool(cur_order);
      return EFI_OUT_OF_RESOURCES;
   }

   CopyMem(new_order, cur_order, boot_order_count * sizeof(UINT16));

   free_pool(cur_order);

   new_order[boot_order_count] = order;
   ++boot_order_count;

   move_top(new_order, boot_order_count, order);
   Status = set_bootorder(new_order, boot_order_count);
   if (EFI_ERROR(Status)) {
      free_pool(new_order);
      return Status;
   }

   Print(L"BootOrder\r\n");
   for (UINTN i = 0; i < boot_order_count; ++i) {
      dump_bootoption(new_order[i]);
   }
   Print(L"\r\n");

   free_pool(new_order);

   return EFI_SUCCESS;
}

EFI_STATUS get_max_size(UINT64* size)
{
   UINT64 max_storage_size;
   UINT64 remain_storage_size;
   UINT64 max_var_size;

   EFI_STATUS  Status;

   Status = gRT->QueryVariableInfo(
         EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
         &max_storage_size,
         &remain_storage_size,
         &max_var_size);

   if (EFI_ERROR(Status)) {
      return Status;
   }

   *size = max_var_size;

   return Status;
}

EFI_STATUS install_boot_order(const CHAR16* desc, VOID* opt_data, UINTN opt_size)
{
   UINT16   order;
   CHAR16   varname[16];

   EFI_LOAD_OPTION   *elo;
   UINT8*   var;
   UINTN    desc_size;
   UINTN    dp_size;
   VOID*    dp;
   UINT64   var_max;
   UINTN    size;

   EFI_STATUS  Status;

   Status = get_max_size(&var_max);
   if (EFI_ERROR(Status)) {
      return 0;
   }

   Status = assign_order(&order);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   UnicodeSPrint(varname, sizeof(varname), L"Boot%04X", order);

   desc_size = StrSize(desc);
   dp = get_media_dp(&dp_size);
   if (!dp) {
      return EFI_OUT_OF_RESOURCES;
   }
   
   if (dp_size > 0x10000) {
      free_pool(dp);
      return EFI_OUT_OF_RESOURCES;
   }

   size = sizeof(EFI_LOAD_OPTION) + desc_size + dp_size + opt_size;

   if (size > var_max) {
      free_pool(dp);
      return EFI_INVALID_PARAMETER;
   }

   var = malloc_pool(size);
   if (!var) {
      return EFI_OUT_OF_RESOURCES;
   }

   // EFI_LOAD_OPTION
   elo = (EFI_LOAD_OPTION*)var;
   elo->Attributes = LOAD_OPTION_ACTIVE;
   elo->FilePathListLength = (UINT16)dp_size;

   // Desc
   CopyMem(var + sizeof(EFI_LOAD_OPTION), desc, desc_size);

   // Device Path
   CopyMem(var + sizeof(EFI_LOAD_OPTION) + desc_size, dp, dp_size);

   // Option Data
   if (opt_data) {
      CopyMem(var + sizeof(EFI_LOAD_OPTION) + desc_size + dp_size, opt_data, opt_size);
   }
   free_pool(dp);

   Status = set_var(varname, var, size);
   if (EFI_ERROR(Status)) {
      free_pool(var);
      return Status;
   }

   free_pool(var);

   Status = add_boot_order(order);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   return Status;
}

EFI_STATUS EFIAPI UefiMain(
      IN EFI_HANDLE ImageHandle,
      IN EFI_SYSTEM_TABLE* SystemTable)
{
   UINTN boot = 0;
   UINTN install = 0;
   UINTN chg_order = 0;

   EFI_STATUS  Status;

   //gImageHandle = ImageHandle;
   //gST = SystemTable;
   //gBS = gST->BootServices;
   //gRT = gST->RuntimeServices;

   Status = get_param(&boot, &install, &chg_order);
   if (EFI_ERROR(Status)) {
      Print(L"%r\r\n", Status);
      return Status;
   }

   if (install) {
      Print(L"install\r\n");
      Status = install_boot_order(L"Kldr - linux kernel loader", NULL, 0);
      if (EFI_ERROR(Status)) {
         Print(L"install error:%r\r\n", Status);
         return Status;
      }
   }

   if (boot) {

      if (chg_order) {
         UINT16*  boot_order;
         UINTN    boot_order_count;

         boot_order = get_bootorder(&boot_order_count);

         if (boot_order) {
            UINT16 boot_current;

            Status = get_var_word(L"BootCurrent", &boot_current);
            if (!EFI_ERROR(Status)) {
               if (boot_order[0] != boot_current) {
                  move_top(boot_order, boot_order_count, boot_current);
                  set_bootorder(boot_order, boot_order_count);
               }
            }
            free_pool(boot_order);
         }
      }

      Status = boot_linux();
      if (EFI_ERROR(Status)) {
         Print(L"linux boot failed\r\n");
         return Status;
      }
   }
   return EFI_SUCCESS;
}

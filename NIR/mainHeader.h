#pragma once
#include <Uefi.h>
#include <Library/PrintLib.h>
#include <Guid/SmBios.h>
#include <IndustryStandard/SmBios.h>
#include <VmmSimpleFileSystem.h>
#include <Pi/PiFirmwareVolume.h>
#include <Pi/PiFirmwareFile.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

extern EFI_SYSTEM_TABLE* gST;
extern EFI_BOOT_SERVICES* gBS;

#define DEBUG_BUFFER_SIZE     0x200      
#define CLEAR_EFI_STATUS(st)  (st & 0xFF)    	
#define SHA256_SIZE_BYTES     32      
#define GUID_SIZE             16      
#define VOLUME_GUID           "78E58C8C3D8A1C4F9935896185C32DD3"      
#define NVRAM_GUID            "A3B9F5CE6D477F499FDCE98143E0422C"      
#define PEI_MODULE            EFI_FV_FILETYPE_PEIM      
#define DXE_DRIVER            EFI_FV_FILETYPE_DRIVER      
#define PEI_CORE              EFI_FV_FILETYPE_PEI_CORE      
#define VOLUME_IMAGE          EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE      

typedef struct {
    UINT8 hash[SHA256_SIZE_BYTES];
    VOID* filePosition;
    UINTN size;
    VOID* baseAddress;
} HASH;

typedef struct {
    UINT8 AnchorString[5];
    UINT8 EntryPointStructureChecksum;
    UINT8 EntryPointLength;
    UINT8 SmbiosMajorVersion;
    UINT8 SmbiosMinorVersion;
    UINT8 SmbiosDocrev;
    UINT8 EntryPointRevision;
    UINT8 Reserved;
    UINT32 StructureTableMaxSize;
    VOID* StructureTableAddress;
} SMBIOS_ENTRY_POINT_STRUCTURE;

void sha256_hash(
    CONST UINT8* data,
    UINTN len,
    CONST UINT8* hash
);
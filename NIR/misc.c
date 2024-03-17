#include "mainHeader.h"

VOID PrintF(CONST CHAR16* fmt, ...) {
	VA_LIST Marker;
	CHAR16 Buffer16[DEBUG_BUFFER_SIZE];

	VA_START(Marker, fmt);
	UnicodeVSPrint(Buffer16, sizeof(Buffer16), fmt, Marker);
	VA_END(Marker);

	gST->ConOut->OutputString(gST->ConOut, Buffer16);
}

EFI_STATUS WriteFile(EFI_FILE_PROTOCOL* file, VOID* message, UINTN bytesToWrite) {
	EFI_STATUS Status;
	UINTN bytesWriten = bytesToWrite;

	Status = file->Write(file, &bytesWriten, message);

	return EFI_SUCCESS;
}

UINTN GetSizeBios() {
	EFI_GUID gEfiSmbiosTableGuid = SMBIOS3_TABLE_GUID;
	SMBIOS_ENTRY_POINT_STRUCTURE* EntryPointStructSmbios;
	EFI_CONFIGURATION_TABLE* confTable = gST->ConfigurationTable;
	UINTN entryCount = gST->NumberOfTableEntries;
	UINT8 biosSize = NULL;

	for (UINTN i = 0; i < entryCount; ++i) {
		if (CompareGuid(&(confTable[i].VendorGuid), &gEfiSmbiosTableGuid)) {
			EntryPointStructSmbios = (SMBIOS_ENTRY_POINT_STRUCTURE*)(confTable[i].VendorTable);
			biosSize = ((SMBIOS_TABLE_TYPE0*)(EntryPointStructSmbios->StructureTableAddress))->BiosSize;
			UINTN size = ((UINT8)biosSize + 1) * 64 * 1024;
			return size;
		}
	}
	return 0;
}

EFI_STATUS HexStringToBytes(const CHAR8* hexString, UINT8* bytes) {
	UINTN len = AsciiStrLen(hexString);
	UINTN byteLen = len / 2;

	for (UINTN i = 0; i < byteLen; i++)
	{
		CHAR8 byteString[3] = {hexString[2 * i], hexString[2 * i + 1], '\0' };
		bytes[i] = (UINT8)AsciiStrHexToUint64(byteString);
	}

	return EFI_SUCCESS;
}
#include "mainHeader.h"
#include "../tmp/HashBase.h"

EFI_SYSTEM_TABLE* gST = NULL;
EFI_BOOT_SERVICES* gBS = NULL;

EFI_STATUS parsVolume(
	EFI_FILE_PROTOCOL* RomFile
);

BOOLEAN parsFile(
	UINT64 position, 
	UINT64 basePosition, 
	EFI_FILE_PROTOCOL* RomFile, 
	UINT64 VolumeSize
);

EFI_STATUS HashAddress(
	HASH hash
);

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
	gST = SystemTable;
	gBS = SystemTable->BootServices;
	CHAR16* Path = L"uefi.bin";
	EFI_STATUS Status;
	EFI_FILE_PROTOCOL* RomFile = NULL;
	EFI_FILE_PROTOCOL* root = NULL;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem = NULL;
	EFI_PHYSICAL_ADDRESS MemoryAddress;

	UINTN BiosSize = GetSizeBios();
	PrintF(L"Size: %u\n", BiosSize);

	Status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&fileSystem);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to locate file system protocol. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}

	Status = fileSystem->OpenVolume(fileSystem, &root);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to open file system volume. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}

	Status = root->Open(root, &RomFile, Path, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, NULL);
	if (EFI_ERROR(Status)) {
		root->Close(root);
		PrintF(L"Failed to open or create the file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}

	Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, EFI_SIZE_TO_PAGES(BiosSize), &MemoryAddress);
	if (EFI_ERROR(Status)) {
		RomFile->Close(RomFile);
		root->Close(root);
		PrintF(L"Allocate memory failed. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}
	gBS->SetMem((VOID*)MemoryAddress, BiosSize, 0);

	VOID* StartAddress = (VOID*)(MAX_UINT32 - BiosSize + 1);
	PrintF(L"Start address: %x\n", StartAddress);

	gBS->CopyMem((VOID*)MemoryAddress, (VOID*)StartAddress, BiosSize);

	Status = WriteFile(RomFile, (VOID*)MemoryAddress, BiosSize);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to save hash to file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePages(MemoryAddress, EFI_SIZE_TO_PAGES(BiosSize));
		RomFile->Close(RomFile);
		root->Close(root);
		return Status;
	}
	RomFile->Close(RomFile);

	Status = root->Open(root, &RomFile, Path, EFI_FILE_MODE_READ, NULL);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to open or create the file1. Status: %d\n", CLEAR_EFI_STATUS(Status));
		root->Close(root);
		return Status;
	}

	Status = parsVolume(RomFile);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to parsing volume. Status: %d\n", CLEAR_EFI_STATUS(Status));
		root->Close(root);
		return Status;
	}

	gBS->FreePages(MemoryAddress, EFI_SIZE_TO_PAGES(BiosSize));
	RomFile->Close(RomFile);
	root->Close(root);

	return EFI_SUCCESS;
}

EFI_STATUS parsVolume(EFI_FILE_PROTOCOL* RomFile) {
	HASH hash;
	EFI_STATUS Status;
	UINT8 targetGuid[GUID_SIZE];
	UINT8 buffer[GUID_SIZE];
	UINTN BufferLenght = GUID_SIZE;
	UINTN matches = 0;
	BOOLEAN flag = FALSE;

	HexStringToBytes(VOLUME_GUID, targetGuid);

	while (!EFI_ERROR(RomFile->Read(RomFile, &BufferLenght, buffer)) && BufferLenght) {
		if (memcmp(buffer, targetGuid, GUID_SIZE)) continue;

		matches++;
		UINT64 position = 0;
		Status = RomFile->GetPosition(RomFile, &position);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to get file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return Status;
		}

		position -= GUID_SIZE;

		PrintF(L"\nVolume at address %p contains the following information:", position - GUID_SIZE);

		UINT64 base_position = position - GUID_SIZE;
		hash.filePosition = (VOID*)base_position;
		UINT64 VolumeSize;
		BufferLenght = sizeof(UINT64);
		Status = RomFile->Read(RomFile, &BufferLenght, &VolumeSize);
		if (EFI_ERROR(Status)) {
			continue;
		}

		PrintF(L"\nVolumeSize: %xh\n", VolumeSize);

		Status = RomFile->SetPosition(RomFile, base_position);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return Status;
		}

		BufferLenght = sizeof(EFI_FIRMWARE_VOLUME_HEADER);
		position = base_position + BufferLenght + 8;

		Status = RomFile->SetPosition(RomFile, position);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return Status;
		}

		flag = parsFile(position, base_position, RomFile, VolumeSize);
		if (flag) {
			hash.size = VolumeSize;

			Status = gBS->AllocatePool(EfiLoaderData, hash.size, (VOID**)&hash.baseAddress);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to allocate memory for EFI_FILE_INFO. Status: %d\n", CLEAR_EFI_STATUS(Status));
				return Status;
			}

			BufferLenght = hash.size;
			Status = RomFile->Read(RomFile, &BufferLenght, hash.baseAddress);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to read file. Status: %d\n", CLEAR_EFI_STATUS(Status));
				return Status;
			}

			HashAddress(hash);
			gBS->FreePool(hash.baseAddress);
		}

		Status = RomFile->SetPosition(RomFile, base_position + VolumeSize);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return Status;
		}
		BufferLenght = GUID_SIZE;
	}

	if (matches == 0) {
		PrintF(L"\nNo matches found.\n");
		return EFI_SUCCESS;
	}
	else {
		PrintF(L"\nTotal matches found.\n");
		return EFI_SUCCESS;
	}
}

BOOLEAN parsFile(UINT64 position, UINT64 basePosition, EFI_FILE_PROTOCOL* RomFile, UINT64 VolumeSize) {
	BOOLEAN flag = FALSE;
	EFI_STATUS Status;
	EFI_FFS_FILE_HEADER fileHead;
	UINT8 targetGuid[GUID_SIZE];
	UINTN BufferLenght = sizeof(EFI_FFS_FILE_HEADER);
	UINTN VolumeLenght = sizeof(EFI_FIRMWARE_VOLUME_HEADER);

	while (!EFI_ERROR(RomFile->Read(RomFile, &BufferLenght, &fileHead)) && BufferLenght) {
		UINTN fileSize = *((UINT32*)fileHead.Size) & 0x00FFFFFF;

		if (position >= VolumeSize + basePosition) {
			break;
		}

		HexStringToBytes(NVRAM_GUID, targetGuid);

		if (((fileHead.Type == PEI_MODULE) || (fileHead.Type == PEI_CORE)) && !fileSize == 0) {
			flag = TRUE;
			position = position + fileSize - BufferLenght;
			UINTN i = position % 8;
			if (i != 0) {
				position = position + (8 - i);
			}
			Status = RomFile->SetPosition(RomFile, position);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
				return FALSE;
			}
			break;
		}
		else if (((fileHead.Type == VOLUME_IMAGE) || (fileHead.Type == DXE_DRIVER)) && !fileSize == 0) {
			EFI_COMMON_SECTION_HEADER Section;
			UINTN SectionBuffer = sizeof(EFI_COMMON_SECTION_HEADER);
			flag = TRUE;

			Status = RomFile->GetPosition(RomFile, &position);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to get file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
				return FALSE;
			}

			UINTN i = position % 8;
			if (i != 0) {
				position = position + (8 - i);
			}

			Status = RomFile->SetPosition(RomFile, position);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
				return FALSE;
			}

			Status = RomFile->SetPosition(RomFile, position);
			if (EFI_ERROR(Status)) {
				PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
				return FALSE;
			}

			RomFile->Read(RomFile, &SectionBuffer, &Section);

			break;
		}
		else if (!memcmp((VOID*)&fileHead.Name, targetGuid, GUID_SIZE) && !fileSize == 0) {
			flag = FALSE;
			break;
		}

		Status = RomFile->GetPosition(RomFile, &position);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to get file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return FALSE;
		}

		position = position + fileSize - BufferLenght;
		UINTN i = position % 8;
		if (i != 0) {
			position = position + (8 - i);
		}

		Status = RomFile->SetPosition(RomFile, position);
		if (EFI_ERROR(Status)) {
			PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
			return FALSE;
		}
		flag = FALSE;
	}
	return flag;
}

EFI_STATUS HashAddress(HASH hash) {
	EFI_STATUS Status;
	EFI_FILE_PROTOCOL* file = NULL;
	CHAR16* FilePath = L"hash.bin";
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fileSystem = NULL;
	EFI_FILE_INFO* FileSizeInfo = NULL;
	UINT64 filePosition;
	UINTN FileSizeInfoSize;
	UINT64 FileSize;
	EFI_FILE_PROTOCOL* root = NULL;

	Status = gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&fileSystem);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to locate file system protocol. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}

	Status = fileSystem->OpenVolume(fileSystem, &root);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to open file system volume. Status: %d\n", CLEAR_EFI_STATUS(Status));
		return Status;
	}

	Status = root->Open(root, &file, FilePath, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, NULL);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to open or create the file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		root->Close(root);
		return Status;
	}

	Status = file->GetInfo(file, &gEfiFileInfoGuid, &FileSizeInfoSize, NULL);
	if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL) {
		PrintF(L"Failed to get the buffer size for EFI_FILE_INFO. Status: %d\n", CLEAR_EFI_STATUS(Status));
		root->Close(root);
		return Status;
	}

	Status = gBS->AllocatePool(EfiLoaderData, FileSizeInfoSize, (VOID**)&FileSizeInfo);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to allocate memory for EFI_FILE_INFO. Status: %d\n", CLEAR_EFI_STATUS(Status));
		root->Close(root);
		return Status;
	}

	Status = file->GetInfo(file, &gEfiFileInfoGuid, &FileSizeInfoSize, FileSizeInfo);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to get information about the file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		root->Close(root);
		return Status;
	}

	FileSize = FileSizeInfo->FileSize;

	Status = file->SetPosition(file, FileSize);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to set file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		return Status;
	}

	Status = file->GetPosition(file, &filePosition);
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to get file position. Status: %r\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		return Status;
	}

	sha256_hash(hash.baseAddress, hash.size, (HASH*)&(hash.hash));

	PrintF(L"Hash: ");
	for (UINTN j = 0; j < SHA256_SIZE_BYTES; j++) {
		PrintF(L"%02x", hash.hash[j]);
	}
	PrintF(L"\n");

	UINTN numRows = sizeof(HashBase) / sizeof(HashBase[0]);
	UINT8* baseBytes = (UINT8*)hash.filePosition;

	for (UINTN i = 0; i < numRows; ++i) {
		if (HashBase[i].filePosition != baseBytes) {
			continue;
		}

		BOOLEAN hashFlag = FALSE;
		for (UINTN k = 0; k < SHA256_SIZE_BYTES; k++) {
			if (HashBase[i].hash[k] != hash.hash[k]) {
				hashFlag = TRUE;
				continue;
			}
		}

		if (hashFlag) {
			PrintF(L"Base address found but hash doesn't match!\n");
			PrintF(L"Hash from file: ");
			for (int k = 0; k < SHA256_SIZE_BYTES; k++) {
				PrintF(L"%x", HashBase[i].hash[k]);
			}
			PrintF(L"\n");
			continue;
		}

		PrintF(L"\nBase address found and hash matches!\n");
		PrintF(L"Hash from file: ");
		for (int k = 0; k < SHA256_SIZE_BYTES; k++) {
			PrintF(L"%x", HashBase[i].hash[k]);
		}
		PrintF(L"\n");

		gBS->FreePool((VOID**)&FileSizeInfo);
		root->Close(root);
		file->Close(file);
		return EFI_SUCCESS;
	}

	PrintF(L"Base address not found, data will be written to file!\n");

	Status = WriteFile(file, &hash.hash, sizeof(hash.hash));
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to save hash to file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		root->Close(root);
		file->Close(file);
		return Status;
	}

	Status = WriteFile(file, &hash.filePosition, sizeof(hash.filePosition));
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to save hash to file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		root->Close(root);
		file->Close(file);
		return Status;
	}

	Status = WriteFile(file, &hash.size, sizeof(hash.size));
	if (EFI_ERROR(Status)) {
		PrintF(L"Failed to save hash to file. Status: %d\n", CLEAR_EFI_STATUS(Status));
		gBS->FreePool((VOID**)&FileSizeInfo);
		root->Close(root);
		file->Close(file);
		return Status;
	}

	PrintF(L"File saved successfully.\n");

	gBS->FreePool((VOID**)&FileSizeInfo);
	root->Close(root);
	file->Close(file);
	return EFI_SUCCESS;
}




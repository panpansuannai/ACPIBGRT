/*
 *  Copyright (c) 2020 panpansuannai <panpansuannai@outlook.com>
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
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>

#define NEXTBOOT        L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi"
#define BMPPATH         L"\\EFI\\ACPIBGRT\\bg.bmp"

/** Bitmap file header */
typedef struct {
	UINT8 magic_BM[2];
	UINT32 file_size;
	UINT8 unused[4];
	UINT32 pixel_data_offset;
	UINT32 dib_header_size;
	UINT32 width;
	UINT32 height;
	UINT16 planes;
	UINT16 bpp;
} __attribute__((packed)) BMP;

/** Root System Description Pointer **/
typedef struct {
	CHAR8 signature[8];
	UINT8 checksum;
	CHAR8 oem_id[6];
	UINT8 revision;
	UINT32 rsdt_address;
	UINT32 length;
	UINT64 xsdt_address;
	UINT8 extended_checksum;
	UINT8 reserved[3];
}__attribute__((packed)) ACPI_RSDP;

/** System Description Table Header **/
typedef struct {
	CHAR8 signature[4];
	UINT32 length;
	UINT8 revision;
	UINT8 checksum;
	CHAR8 oem_id[6];
	CHAR8 oem_table_id[8];
	UINT32 oem_revision;
	UINT32 asl_compiler_id;
	UINT32 asl_compiler_revision;
} __attribute__((packed))ACPI_SDT_HEADER;

/** ACPI BGRT **/
typedef struct {
	ACPI_SDT_HEADER header;
	UINT16 version;
	UINT8 status;
	UINT8 image_type;
	UINT64 image_address;
	UINT32 image_offset_x;
	UINT32 image_offset_y;
} __attribute__((packed)) ACPI_BGRT;

void checkSdtSum(ACPI_SDT_HEADER* sdt)
{
    UINT32 length = sdt->length;
    char c = 0;
    char* t = (char*)sdt;
    for(UINT32 i =0;i<length;++i){
        c=c^t[i];
    }
    for(UINT8 i =0;i<8;++i){
        sdt->checksum^(c>>i);
    }
}
int Memcmp(void* aptr,void* bptr,UINTN size)
{
    char* a = (char*)aptr, *b = (char*)bptr;
    for(UINTN i =0;i<size;++i){
        if(*a>*b)return 1;
        else if(*a<*b)return -1;
        a++;
        b++;
    }
    return 0;
}
int Guidcmp(EFI_GUID* a , EFI_GUID* b)
{
    if(a->Data1==b->Data1&&
            a->Data2==b->Data2&&
            a->Data3==b->Data3&&
            *(UINT32*)a->Data4==*(UINT32*)b->Data4){
        return 0;
    }
    return 1;
}
//Copy memory from byte to byte
void Memcpy(void* from, void* to,UINTN cnt)
{
    char* f=from, *t = to;
    for(UINTN i =0;i<cnt;++i){
        *f++ = *t++;
    }
}
ACPI_SDT_HEADER* CreateXsdt(ACPI_SDT_HEADER* oldXsdt,UINTN cnt)
{
    UINTN newCnt = oldXsdt->length + cnt*sizeof(UINT64);
    ACPI_SDT_HEADER* newXsdt;
    uefi_call_wrapper(gBS->AllocatePool,3,EfiLoaderData,&newCnt,(void**)newXsdt);
    Memcpy(oldXsdt,newXsdt,oldXsdt->length);
    newXsdt->length += cnt*sizeof(UINT64);
    return newXsdt;
}
EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_FILE* LoadedFile;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
    uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,3,
            ImageHandle,&gEfiLoadedImageProtocolGuid,(void**)&LoadedImage);

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,3,
            LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(void**)&FileSystem);

    if(Directory == NULL){
        uefi_call_wrapper(FileSystem->OpenVolume,2,FileSystem,&Directory);
    }
    EFI_STATUS s = uefi_call_wrapper(Directory->Open,5,
            Directory,&LoadedFile,Path,EFI_FILE_MODE_READ,EFI_FILE_READ_ONLY);
    if(s!=EFI_SUCCESS){
            return NULL;
    }
    return LoadedFile;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle,EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(ImageHandle,SystemTable);

    ACPI_RSDP* rsdp;
    //Load the Root System Description Pointer
    {
        EFI_GUID AcpiGuid = ACPI_TABLE_GUID;
        EFI_GUID Acpi20Guid = ACPI_20_TABLE_GUID;
        for(UINTN i=0;i<SystemTable->NumberOfTableEntries;++i){
            UINT64* Guid = &(SystemTable->ConfigurationTable[i].VendorGuid);
            if(Guidcmp(Guid,&AcpiGuid)&&Guidcmp(Guid,&Acpi20Guid)){
                continue;
            }
            rsdp = SystemTable->ConfigurationTable[i].VendorTable;
        }
    }
    if(Memcmp(rsdp->signature,"RSD PTR ",8)){
        Print(L"rsdp invalid\n");
        return EFI_SUCCESS;
    }
    else{
        Print(L"Load rsdp,revision:%d\n",rsdp->revision);
    }
    //Get the Extended System Description Table
    ACPI_SDT_HEADER* xsdt = (ACPI_SDT_HEADER*)(UINTN) rsdp->xsdt_address;
    if(!xsdt){
        Print(L"Can't get xsdt\n");
        return EFI_SUCCESS;
    }
    if(Memcmp(xsdt->signature,"XSDT",4)){
        Print(L"xsdt invalid\n");
    }
    else {
        Print(L"xsdt valid\n");
    }
    //Get the Boot Graphics Resource Table
    ACPI_BGRT* bgrt = NULL;
    {
        UINTN* entries = (UINTN*)&xsdt[1];
        UINTN entry_cnt = (UINTN)(xsdt->length-sizeof(ACPI_SDT_HEADER)) / sizeof(ACPI_SDT_HEADER*);
        UINT64* addr = (UINT64*)entries;
        for(UINTN i =0; i<entry_cnt;++i){
            ACPI_SDT_HEADER* hdr = (ACPI_SDT_HEADER*)entries[i];
            if(Memcmp(hdr->signature,"BGRT",4)!=0){
                //Isn't the BGRT
                continue;
            }
            //Found BGRT
            bgrt = (ACPI_BGRT*) hdr; 
        }
        if(!bgrt){
            Print(L"Can't found BGRT.Adding BGRT\n");
            //Allocate memory for BGRT
            UINTN size = sizeof(ACPI_BGRT);
            uefi_call_wrapper(gBS->AllocatePool,3,EfiLoaderData,&size,(void**)bgrt);
            //Create a new xsdt with one more entry
            xsdt = CreateXsdt(xsdt,1);
            entries = (UINTN*)&xsdt[1];
            entries[entry_cnt] = bgrt;
            rsdp->xsdt_address = xsdt;
            //Re-calculate the sum of xsdt
            checkSdtSum(xsdt);
        }
    }
    BMP* bmp = NULL;
    //Load the BMP. Assume that it is a valid BMP file.
    {
        EFI_FILE* BMPFile = LoadFile(NULL,BMPPATH,ImageHandle,SystemTable);
        if(!BMPFile){
            Print(L"Can't found BMP file\n");
            return EFI_SUCCESS;
        }
        else {
            Print(L"Found BMP.\n");
        }
        BMP temp;
        UINTN size = sizeof(temp);
        Print(L"Read%d.\n",size);
        uefi_call_wrapper(BMPFile->Read,3,BMPFile,&size,&temp);
        //Get actually the size of BMP file
        size = temp.file_size;

        //Allocate Memory
        Print(L"Allocate Pool.\n");
        uefi_call_wrapper(gBS->AllocatePool,3,EfiLoaderData,&size,(void**)&bmp);

        //Read the whole content into memory
        uefi_call_wrapper(BMPFile->SetPosition,2,BMPFile,0);
        uefi_call_wrapper(BMPFile->Read,3,BMPFile,&size,bmp);
    }
    //Configure the BGRT
    {
        //BGRT signature is "BGRT"
        UINT32* val;
        val = (UINT32*)bgrt->header.signature;
        val = 'B'<<24 + 'G'<<16 + 'R'<<8 + 'T';
        bgrt->header.length = sizeof(ACPI_BGRT);
        bgrt->header.revision = 1;

        /** Don't need **/
        //bgrt->header.oem_id = xsdt->oem_id;
        //bgrt->header.oem_table_id = xsdt->oem_table_id;
        //bgrt->header.oem_revision = xsdt->oem_revision;
        //bgrt->header.asl_compiler_id = xsdt->asl_compiler_id;
        //bgrt->header.asl_compiler_revision = xsdt->asl_compiler_revision;
        
        bgrt->version = 1;
        bgrt->status = 0;
        bgrt->image_type = 0;
        bgrt->image_address = (UINT64)bmp;
        //Assume that there are 1920 pixels per line.
        bgrt->image_offset_x = (1920 - bmp->width) / 2;
        bgrt->image_offset_y = 300;
        Print(L"BGRT check sum.\n");
        checkSdtSum(bgrt);
    }
    //Go to the OP UEFI
    {
        EFI_LOADED_IMAGE* image;
        uefi_call_wrapper(gBS->HandleProtocol,3,ImageHandle,&LoadedImageProtocol,(void**)&image);
        CHAR16* next_boot_path = NEXTBOOT;

        EFI_HANDLE next_boot_handle = 0;
        EFI_DEVICE_PATH* boot_dp = FileDevicePath(image->DeviceHandle,next_boot_path);

        uefi_call_wrapper(gBS->LoadImage,6,0,ImageHandle,boot_dp,0,0,&next_boot_handle); 
        EFI_STATUS s = uefi_call_wrapper(gBS->StartImage,3,next_boot_handle,NULL,NULL);
        if(s==EFI_INVALID_PARAMETER){
            Print(L"Start image error: invalid parameter\n");
        }
        else if(s==EFI_SECURITY_VIOLATION){
            Print(L"Start image error: secrity violation\n");
        }
    }
    return EFI_SUCCESS;
}

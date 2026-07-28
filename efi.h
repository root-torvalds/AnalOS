#ifndef EFI_H
#define EFI_H

#define EFIAPI __attribute__((ms_abi))

typedef unsigned long long UINTN;
typedef unsigned int UINT32;
typedef unsigned short CHAR16;
typedef unsigned char UINT8;
typedef void* EFI_HANDLE;

typedef struct {
    UINT32 Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} EFI_GUID;

typedef struct {
    UINT8 Blue;
    UINT8 Green;
    UINT8 Red;
    UINT8 Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    int PixelFormat;
    int PixelInformation;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    unsigned long long FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    long long (EFIAPI *QueryMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber,
        UINTN *SizeOfInfo,
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );
    long long (EFIAPI *SetMode)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        UINT32 ModeNumber
    );
    long long (EFIAPI *Blt)(
        struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
        EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
        UINTN SourceX, UINTN SourceY,
        UINTN DestinationX, UINTN DestinationY,
        UINTN Width, UINTN Height,
        UINTN Delta
    );
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    unsigned long long Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void* RaiseTPL; void* RestoreTPL;
    long long (EFIAPI *AllocatePages)(int Type, int MemoryType, UINTN Pages, unsigned long long *Memory);
    void* FreePages;
    long long (EFIAPI *GetMemoryMap)(UINTN *MemoryMapSize, void *MemoryMap, UINTN *MapKey, UINTN *DescriptorSize, UINT32 *DescriptorVersion);
    void* AllocatePool; void* FreePool;
    void* CalculateCrc32;
    void* CreateEvent; void* SetTimer; void* WaitForEvent; void* SignalEvent; void* CloseEvent; void* CheckEvent;
    void* InstallProtocolInterface; void* ReinstallProtocolInterface; void* UninstallProtocolInterface; void* HandleProtocol;
    void* VoidReserved; void* RegisterProtocolNotify;
    void* LocateHandle; void* LocateDevicePath; void* InstallConfigurationTable;
    void* LoadImage; void* StartImage; void* Exit; void* UnloadImage;
    long long (EFIAPI *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void* GetNextMonotonicCount; void* Stall; void* SetWatchdogTimer;
    void* ConnectController; void* DisconnectController;
    void* OpenProtocol; void* CloseProtocol; void* ProtocolPerHandle;
    void* LocateHandleBuffer;
    long long (EFIAPI *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    void* ConsoleInHandle; void* ConIn;
    void* ConsoleOutHandle; void* ConOut;
    void* StandardErrorHandle; void* StdErr;
    void* RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

typedef struct {
    void* FrameBufferBase;
    unsigned long long FrameBufferSize;
    unsigned int HorizontalResolution;
    unsigned int VerticalResolution;
    unsigned int PixelsPerScanLine;
    void* VirtualFrameBuffer; // <-- НАШЕ НОВОЕ ПОЛЕ
} BootInfo;

EFIAPI EFI_GRAPHICS_OUTPUT_PROTOCOL* init_gop(EFI_SYSTEM_TABLE *SystemTable);
EFIAPI void fill_screen(UINT8 r, UINT8 g, UINT8 b, UINT8 a);
EFIAPI void draw_square(UINT32 size, UINT8 r, UINT8 g, UINT8 b, UINT8 a);
EFIAPI void draw_rect(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT8 r, UINT8 g, UINT8 b, UINT8 alpha);
EFIAPI void draw_ui_element(UINT32 w, UINT32 h, int anchor, UINT8 r, UINT8 g, UINT8 b, UINT8 a);
EFIAPI void swap_buffers(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop);
EFIAPI void draw_pixel(UINT32 x, UINT32 y, UINT8 r, UINT8 g, UINT8 b, UINT8 alpha);
EFIAPI void draw_alpha_test_bar(UINT32 start_x, UINT32 start_y, UINT32 height);
EFIAPI void draw_circle(UINT32 center_x, UINT32 center_y, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a);
EFIAPI void draw_taskbar(UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a);

#endif

#include <windows.h>
#include <stdint.h>

// define uintx_t types to be referred as uintx
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;


// Defined macros to be identified as static
// Casey named these macros according to what they are doing in the code more clearly, 
// this way we can have specific keywords for differrent types of static purposes, since they can be different as you can read in the description of each one of them below
//
// To be used with functions only and means that the function is local only to the source file it is in   
#define internal static
// means the variable is locally peristant, like the Operation that contains WHITHNESS/BLACKNESS. 
// We want that to keep the last value assigned until if else/logic changes it  
#define local_persist static
// simply means that the variable is global 
#define global_variable static

//TODO: This is a global for now
// holds a bool that confirms if window is running or not 
global_variable bool Running;

// needs to be global so it can be passed to StretchDIBits() in Win32UpdateWindow()
// NOTE: Remember that static declarations initalizes to 0 if no value is defined
// Because of this, and since this is a struct reference, we can actually delete/comment all the struct members that are set to 0 in the Win32ResizeDIBSectio() 
//since setting this struct reference to 0 sets all the uinitlialized data members to 0  
global_variable BITMAPINFO BitmapInfo;

// needs to be global so it can be passed to StretchDIBits() in Win32UpdateWindow()
global_variable void *BitmapMemory;

// more globals for the 2 functions below 
global_variable int BitmapWidth;
global_variable int BitmapHeight;

global_variable int BytesPerPixel = 4;


// Global Variable TIP: If you want to see all the lines in the code that are using a specific global variable, 
// you can add a random character to the variable to make the previous ones undeclared, e.g.
// global_variable int BitmapHeight#
// Then compile the code an you will see the errors with the exact lines of the referenced variables that now became undeclared :) 


// Function that paints a Blue and Green pixel gradient pattern to the backbuffer 
internal void RenderWeirdGradient(int XOffset, int YOffset)
{
    /* 
                        WIDTH ->                                                    until 
                        0                                                  Width*BytesPerPixel
        BitmapMemory-> Row 0: BB GG RR xx BB GG RR xx BB GG RR xx ...........................
        BM + Pitch     Row 1: BB GG RR xx BB GG RR xx BB GG RR xx ...........................
    
        BitmapMemory Ptr  + Pitch takes us to the next row 
        Pitch is the number of bytes used by each row of the image in memory, including padding 
     */


    int Width = BitmapWidth;
    int Height = BitmapHeight;

    int Pitch = Width*BytesPerPixel;
    //  Start row at the offset 0 of the BitmapMemory
    uint8 *Row = (uint8 *)BitmapMemory;
    for(int Y = 0; Y < BitmapHeight; ++Y)
    {
        // Assign a copy of the start of the row casted to 32 bits to a Pixel ptr (because each pixel has 32 bits (RGB + padding))
        uint32 *Pixel = (uint32 *)Row;
        // so we can start filling in rows of pixels. Go thorugh each pixel of the row and write it 
        for(int X = 0; X < BitmapWidth; ++X)
        {
            /*  Structure of a Pixel 
                
                        pixels num    0    1    2    3
                    Pixel in memory: 0x00 0x00 0x00 0x00 
                RGB bytes of Pixel    BB   GG   RR   xx

                Remember pixel data is stored in LE 

             */
            
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);

            // Since the Pixel ptr is 32 bit (uint32 *Pixel) we move to the next pixel (32 bits forward) the post-increment
            // This is way we casted the Row ptr to uint32 before the copy up there  (uint32 *Pixel = (uint32 *)Row)
            *Pixel++ = ((Green << 8) | Blue);           

            /*  Description of the BIT Wise operation code above 
            
                Memory:             BB GG RR xx
                Register:           xx RR GG BB

                Pixel (32 bits)

                We are ORing (combining) Blue bits which are at the bottom bits (beacuse of the LEndianness) with the
                Green bits which sit 8 bits up. 
             */
        }

        // add Pitch to move to the next row 
        Row += Pitch;
    }

}




internal void Win32ResizeDIBSection(int Width, int Height)
{
    // Create the backbuffer  


    // TODO: Bulletproof this
    // Maybe don't free first, free after, then free first if that fails 

    // free the memory before we start allocating with VirtualAlloc()
    if(BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }


    BitmapWidth = Width;
    BitmapHeight = Height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight;    // bitmap set to negative for top down DIB, in other words, ask for the rows to go sequentially from the top down (more info in the MSDN doc of bmiHeader)  
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;           // bits for RGB (8 bits * 3) with additionl 8 bits padding to keep it 32 bit (4 byte) alligned 
    BitmapInfo.bmiHeader.biCompression = BI_RGB;
    // Commenting the rest of these out as they are already set (could have deleted them but keeping them here so I can understand what happened) 
    // to 0 through the static global BitmapInfo declaration :)  
    //
    // BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    // BitmapInfo.bmiHeader.biSizeImage = 0;
    // BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    // BitmapInfo.bmiHeader.biClrUsed = 0;
    // BitmapInfo.bmiHeader.biClrImportant = 0;

    
    int BitmapMemorySize = (BitmapWidth*BitmapHeight)*BytesPerPixel;
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);


    // TODO: Probably clear this to black 
   

}

internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;


    // Function that will do a rectangle to rectangle copy (also known as 'blitting')
    StretchDIBits(DeviceContext,   
                //   X, Y, Width, Height,  // destination rectangle
                //   X, Y, Width, Height,  // source rectangle
                  0, 0, BitmapWidth, BitmapHeight,  // source 
                  0, 0, WindowWidth, WindowHeight,  // destination 
                  BitmapMemory,
                  &BitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
}



// Note: WindowProc function identifier and arguments renamed for clarity 
LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window, 
                   UINT Message,
                   WPARAM WParam,
                   LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
            RECT ClientRect;
            // fill in the RECT struct with the windoww client area data (coordinates of the rectangle on the screen we can draw into (hence client))
            GetClientRect(Window, &ClientRect); 
            LONG Width = ClientRect.right - ClientRect.left;
            LONG Height = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(Width, Height);

            OutputDebugStringA("WM_SIZE\n");
        } break;

        case WM_CLOSE:
        {
            // TODO: Handle this as a handler to the user?
            Running = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO: Handle this as an error - recreate window? 
            Running = false;
        } break;

        case WM_PAINT:
        {   
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);

            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top; 
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            
            RECT ClientRect;
            GetClientRect(Window, &ClientRect); 

            Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);

            EndPaint(Window, &Paint); 
        } break;

        default: 
        {
            // OutputDebugStringA("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;

    }

    return Result;
}

// Note: Function arguments renamed for clarity 
int CALLBACK WinMain(HINSTANCE Instance, 
                     HINSTANCE PrevInstance, 
                     LPSTR CommandLine,
                     int ShowCode)
{   
    WNDCLASSA  WindowClass {};
    
    // TODO: Check if HREDRAW/VREDRAW/OWNDC still matter
    // Note update: Checked and seems like they are not needed. commenting them for the time being 
    // WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW; 
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "MochoWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window =  
            CreateWindowExA(
                0,
                WindowClass.lpszClassName,
                "Mocho",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance, 
                0);
        if(Window)
        {
            int XOffset = 0;
            int YOffset = 0;
            Running = true;
            while(Running)
            {
                MSG Message {};
                // we have our Running variable checking if the program is running but for safety and to make sure to catch any other quit message that the program decides to send,
                // we can do this condition: 
                if(Message.message == WM_QUIT)
                {
                    Running = false;
                }

                // We switched the GetMessage() for PeekMessage() so we can keep repainting our pixels even when there are no messages to be dispatched 
                // This is the main different between these functions, GetMessage() pauses and waits (blocking our program) when there are no incoming messages and PeekMessage keeps running regardless 
                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) // last arg PM_REMOVE deals with the incoming message and flushes it for dispatch   
                {
                    TranslateMessage(&Message);
                    // DispatchMessage set to Ansi
                    DispatchMessageA(&Message);
                }


                // Start paitning and animating the window from here!
                RenderWeirdGradient(XOffset, YOffset);
                
                HDC DeviceContext = GetDC(Window); 
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                int WindowWidth = ClientRect.right - ClientRect.left;
                int WindowHeight = ClientRect.bottom - ClientRect.top;
                
                Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
                
                ReleaseDC(Window, DeviceContext);

                ++XOffset;
            }
        }
        else
        {
            // TODO: Logging
        }
    }
    else
    {
        // TODO: Logging
    }


    return (0);
}



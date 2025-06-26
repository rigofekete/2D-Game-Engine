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

// Day 5 (around 22:00): Instead of having global variables for the bitmap buffer, we implement a struct with these data memebrs
// This will give us not only the encapsulation we need but we will also be able to create as many individual buffers structs as we want
// since the members are all placed in a struct. 
// This is also a good example of data types that need to be passed bundled together, since they are all interdependent. We are not goingto use any of the data members separately so
// they can all be passed bundled. HOWEVER them Dimension struct (win32_window_dimension) right below this can be an example of how you can make a set of members but they can also be used separately, depending of what the dev wants    
struct win32_onscreen_buffer
{
    // Pixels are always 32-bits wide, LE Memory Order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    // Pitch is now part of this struct 
    int Pitch;
    // Day 5 (min 2:02:42) BytesPerPixel changed bck to be a local only variable in Win32ResizeDIBSection() because it's value will be aways the same (32 bits, 4 bytes - 3 for RGB plus + 1 byte padding)
    // int BytesPerPixel;
};

// Setting the struct construct as global for now so it can be passed to the several function calls in the code 
global_variable win32_onscreen_buffer GlobalBackbuffer {};

//TODO: This is a global for now
// holds a bool that confirms if window is running or not 
global_variable bool GlobalRunning;


// Day 5(min 38): Instead of repeating code everytime we need to get the window Rect data
// we can simply use the win32_window_dimension struct and the Win32GetWindowDimension() 
//
// NOTE: This is also a neat way to return more than 1 value from functions! 
struct win32_window_dimension
{
    int Width;
    int Height;
};


// Helper function that deals with the RECT construction  
win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    // fill in the RECT struct with the windoww client area data (coordinates of the rectangle on the screen we can draw into (hence client))
    GetClientRect(Window, &ClientRect); 
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result; 
}


// Global Variable TIP: If you want to see all the lines in the code that are using a specific global variable, 
// you can add a random character to the variable to make the previous ones undeclared, e.g.
// global_variable int BitmapHeight#
// Then compile the code an you will see the errors with the exact lines of the referenced variables that now became undeclared :) 

 // Function that paints a Blue and Green pixel gradient pattern to the backbuffer 
 // adding the win32_onscreen_buffer as arg 
internal void RenderWeirdGradient(win32_onscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
    /* 
                        WIDTH ->                                                    until 
                        0                                                  Width*BytesPerPixel
        BitmapMemory-> Row 0: BB GG RR xx BB GG RR xx BB GG RR xx ...........................
        BM + Pitch     Row 1: BB GG RR xx BB GG RR xx BB GG RR xx ...........................
    
        BitmapMemory Ptr  + Pitch takes us to the next row 
        Pitch is the number of bytes used by each row of the image in memory, including padding 
     */

    // TODO: Let's see what the optimizer does if we pass the buffer struct as value 
    
    //  Start row at the offset 0 of the BitmapMemory
    uint8 *Row = (uint8 *)Buffer.Memory;
    for(int Y = 0; Y < Buffer.Height; ++Y)
    {
        // Assign a copy of the start of the row casted to 32 bits to a Pixel ptr (beacuse each pixel has 32 bits (RGB + padding))
        uint32 *Pixel = (uint32 *)Row;
        // so we can start filling in rows of pixels. Go thorugh each pixel of the row and write it 
        for(int X = 0; X < Buffer.Width; ++X)
        {
            /*  Structure of a Pixel 
                
                        pixels num    0    1    2    3
                    Pixel in memory: 0x00 0x00 0x00 0x00 
                RGB bytes of Pixel    BB   GG   RR   xx

                Remember pixel data is stored in LE BUT the padding sits at the end as per Microsoft LE implementation (otherwise it would logically by the first byte in LE) 

             */
            
            uint8 Blue  = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);


            // NOTE: This line here is what paints each pixel, with the ORed bits for the Green and Blue RGB channels 
            // We need to shift Green left 8 times beacuse of the LE order in memory (BB GG RR xx), Blue is in the low order, so to OR it with Green we need to move Green left 8 bits (1 byte) 
            // These operation produce the final pixel value to be painted in each iteration (we don't touch Red here because we only want to paint pixels with Green and Blue) 
            *Pixel++ = ((Green << 8) | Blue);           
            // Also, since the Pixel ptr is 32 bit (uint32 *Pixel) we move to the next pixel (32 bits forward) the post-increment
            // This is way we casted the Row ptr to uint32 before the copy up there  (uint32 *Pixel = (uint32 *)Row)

            /*  Description of the BIT Wise operation code above 
            
                Memory:             BB GG RR xx
                Register:           xx RR GG BB


                // Little Intel quirk: when storing in memory as LE, intel decided to keep the padding bytes to the right
                Pixel (32 bits - RR GG BB xx)

                We are ORing (combining) Blue bits which are at the bottom bits (beacuse of the LEndianness) with the
                Green bits which sit 8 bits up (that is way we need to shift Green left, 8x). 

             */
        }
    
        // add Pitch to move to the next row 
        Row += Buffer.Pitch;

        // Full review of this function in the Q&A of the Day 5, around min 1:15:00 
    }

}



// Create the backbuffer (bitmap) 
// Here we need to pass a pointer to Buffer because we need to set the struct members with data that needs to be accessed after the scope of this function call   
internal void Win32ResizeDIBSection(win32_onscreen_buffer *Buffer, int Width, int Height)
{


    // TODO: Bulletproof this
    // Maybe don't free first, free after, then free first if that fails 

    // free the memory before we start allocating with VirtualAlloc()
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }


    Buffer->Width = Width;
    Buffer->Height = Height;
    // Day 5 (2:02:17) 
    
    // Pixels are always 32-bits wide, LE Memory Order BB GG RR XX
    int BytesPerPixel = 4; 

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;    // bitmap set to negative for top down DIB, in other words, ask for the rows to go sequentially from the top down (first byte starts at top left pixel - more info in the MSDN doc of bmiHeader)  
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;           // bits for RGB (8 bits * 3) with additionl 8 bits padding to keep it 32 bit (4 byte) alligned 
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    // Commenting the rest of these out as they are already set (could have deleted them but keeping them here so I can understand what happened) 
    // to 0 through the static global BitmapInfo declaration :)  
    //
    // BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    // BitmapInfo.bmiHeader.biSizeImage = 0;
    // BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    // BitmapInfo.bmiHeader.biClrUsed = 0;
    // BitmapInfo.bmiHeader.biClrImportant = 0;

    
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width*BytesPerPixel;

    // TODO: Probably clear this to black 

}


// internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
// In Day 5 (around 9:00), Casey explains why it is more efficient to pass the Rect as value since it is a small structure.
// If Rect was eventually considerably big structure then it would be more efficient to pass a pointer to void having to make a whole copy of the structure just for this function call  
//
// Day 5 (around 27:00) function name changed to Win32DisplayBufferInWindow
// internal void Win32CopyBufferToWindow(HDC DeviceContext, RECT ClientRect, 
//                                       win32_onscreen_buffer Buffer,
//                                       int X, int Y, int Width, int Height)

// Note how we replaced the ClientRect call with our Dimension members 
// Day 5 (min 2:00:00) We got rid of the args for the dirty rectangle since we are not going to update them or deal with them directly
// We can do this because we are blitting the whole screen at 30 fps 
internal void Win32DisplayBufferInWindow(HDC DeviceContext, 
                                        int WindowWidth, int WindowHeight, 
                                        win32_onscreen_buffer Buffer)
{

    // TODO: Aspect ratio correction 
    // TODO: Play with stretch modes

    // Function that will do a rectangle to rectangle copy (also known as 'blitting')
    StretchDIBits(DeviceContext,   
                //   X, Y, Width, Height,  // destination rectangle
                //   X, Y, Width, Height,  // source rectangle
                  0, 0, WindowWidth, WindowHeight,  // destination 
                  0, 0, Buffer.Width, Buffer.Height,  // source 
                  Buffer.Memory,
                  &Buffer.Info,
                  DIB_RGB_COLORS, SRCCOPY); // SRCCOPY is a flag tells the compiler not do any operations wuth the bits, only copy them. 
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
            // Day 5 (min 43): The Dimension and Win32ResizeDIBSection() will be moved to be called in the actual start of WinMain instead of here
            // win32_window_dimension Dimension = Win32GetWindowDimension(Window);
           
            // Day 5(min 38): Instead of repeating this chunk of code everytime we need to get the window rect data
            // we can simply use the win32_window_dimension struct and the Win32GetWindowDimension() look at how much code repetition we can get rid off..
            //
            // RECT ClientRect;
            // // fill in the RECT struct with the windoww client area data (coordinates of the rectangle on the screen we can draw into (hence client))
            // GetClientRect(Window, &ClientRect); 
            // LONG Width = ClientRect.right - ClientRect.left;
            // LONG Height = ClientRect.bottom - ClientRect.top;

            // Win32ResizeDIBSection(&GlobalBackbuffer, Dimension.Width, Dimension.Height);

        } break;

        case WM_CLOSE:
        {
            // TODO: Handle this as a handler to the user?
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO: Handle this as an error - recreate window? 
            GlobalRunning = false;
        } break;

        case WM_PAINT:
        {   
            PAINTSTRUCT Paint;
            // In WM_PAINT messages we need to Begin and End Paint with the respective functions
            // In this case we return the Begin value handle to be our DC here
            HDC DeviceContext = BeginPaint(Window, &Paint);

            // Day 5 (min 2:00:30) We got rid of the dirty rectangle coordinates since we are not going to update them or deal with them directly
            // We can do this because we are blitting the whole screen at 30 fps 
            // int X = Paint.rcPaint.left;
            // int Y = Paint.rcPaint.top; 
            // int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            // int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            

            // Day 5 (39 min) Here we also replace the default RECT calls by our dimension custom functions 
            // RECT ClientRect;
            // GetClientRect(Window, &ClientRect); 
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            
            // Note how we replace the use of ClientRect with the dimension coordinates 
            // Win32DisplayBufferInWindow(DeviceContext, ClientRect, GlobalBackbuffer, X, Y, Width, Height);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, 
                                    GlobalBackbuffer);

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
    
    // Day 5 (min 43): Resize Bitmap from here instead of getting called in the VM_SIZE of the winproc
    // Setting specific window size values 
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    // CS_HREDRAW|CS_VREDRAW flags that repaints the whole window after resizing (vertical and horizontal) ad not just a portion of it 
    WindowClass.style = CS_HREDRAW|CS_VREDRAW; 
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    // WindowClass.hIcon;
    // ClassName is only necessary so we can pass it as arg in CreateWindowExA
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
            // Get a DC handle of the window to issue draw calls  
            // All drawing stuff needs to go through  DeviceContext handle of the window 
            // 
            // Dat 5 (1:25:52)
            // Since we specified CS_OWNDC, we can just get on device context and use it forever 
            //because we are not sharing it with anyone
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            GlobalRunning = true;
            while(GlobalRunning)
            {
                MSG Message {};
                // we have our GlobalRunning variable checking if the program is running but for safety and to make sure to catch any other quit message that the program decides to send,
                // we can do this condition: 
                if(Message.message == WM_QUIT)
                {
                    GlobalRunning = false;
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
                RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);
                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);

                // Day 5: Another place where the default RECT method is replaced by our Dimension struct and function  
                // RECT ClientRect; 
                // GetClientRect(Window, &ClientRect);
                // int WindowWidth = ClientRect.right - ClientRect.left;
                // int WindowHeight = ClientRect.bottom - ClientRect.top;
                
                // Day5 (min 42): Why aren't we passing the whole Dimension (win32_window_dimension) struct as arg to the function? 
                // Because of flexibility, this way the user is no forced to pass the whole struct, and can this way pass already assigned different Widths and Heigths depending of the intention.
                // This is why we see the Dimension.Width and Height pairs being passed twice in this example, this happens because we had no need to pass a different pair here, but we will need to do so in other calls to this function    
                Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackbuffer);
                
                // Day 5 (min 1:25:52)
                // Since we set the WindowClass style has CS_OWNDC and we are already getting the DC at the beggining of this loop, we don't need to release any more because the window will always own it's own DC
                // // release the DC handle since we made the painting and blitting we needed to 
                // ReleaseDC(Window, DeviceContext);


                // Incrementing X/Y offsets to create the animating effect 
                ++XOffset; 
                // YOffset += 2;
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



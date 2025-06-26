#include <windows.h>
#include <stdint.h>
// Day 6 - Including the library to control the controller events 
#include <xinput.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define internal static
#define local_persist static
#define global_variable static

struct win32_onscreen_buffer
{
    // Pixels are always 32-bits wide, LE Memory Order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

global_variable win32_onscreen_buffer GlobalBackbuffer {};

//TODO: This is a global for now
global_variable bool GlobalRunning;

struct win32_window_dimension
{
    int Width;
    int Height;
};

// Day 6 (min 19:03) - Trick to call windows functions direclty to avoid program crashing or not loading for lack of required dll's for the library linkage, in this case the xinput.h functions 
// Basically getting the actual code definitions for the functions we want and place them in our main file, so we don't need to use any import library (#include) specifications (we still need to import the xinput h lib because we are renaming one of its functions as a macro, remember). For more info see the video at the timestamp 19:03

// Since we only want 2 functions from the xinput library (XInputSetState and XInputGetState) we are going to do this trick righ there. As said in the timestamped video, these 2 functions require a few specs and dll's that might not be available on all systems. This is another reason for setting up the functions ourselves here


// This typdef creates an alias 'x_input_get_state', representing a function pointer type (typedef on function declarations does not return alias like other simple declarations, typedefs on function signatures automatically become function pointer types!!!) for a function that returns DWORD, uses WINAPI calling convention, and takes (DWORD dwUserIndexm XINPUT_STATE *pState) as parameters
// typedef DWORD WINAPI  x_input_get_state(DWORD dwUserIndex, XINPUT_STATE* pState);
// typedef DWORD WINAPI x_input_set_state(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

// NOTE: To make things simpler and more readable, we can replace the typedef above with the following logic:

// we can just make another macro definition to rename the whole function signature as a X_INPUT_GET_STATE(name) marco, passing a custom defined function identifier (name) through it, so it can expand it to the actual function signature with that passed in name   
// Note that if we eventually want to change the function parameters or return value we can do it here and it will change it everywere else where this is referred to (with the custom defined marcos and pointers) 
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dUserIndex, XINPUT_STATE *pState)

// Then we can make a type def for the function (X_INPUT_SET_STATE(name)) which will create a function pointer type (typedef on function declaration do not return alias like other simple declarations, typedefs on function signature automatically become function pointer types!!!), which will get the name we wish to set to the identifier as an arg of that defined macro (X_INPUT_GET_STATE)

typedef X_INPUT_GET_STATE(x_input_get_state);  
// Remember this expands to the actual function signature we defined a macro for, in the code above this one 

// Function override Stubs (stub is a placeholder that does not return anyhting and it's only purpose is to make the code compile without any errors.) 
// With this in place, we can then implement the function definition override as we wish at a later time BUT the actual purpose of this stub is to serve as a placeholder safety default function that does nothing, in case the call to the actual windows function from xinput library (called through internal void Win32LoadXInput(void) function from WinMain) does not load for whatever reason (e.g. the dll necessary is not present in the machine where this game will be played)
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
// we can then set that type to be a pointer to that initial stub function, so we can make our custom implementation (overriding the original function from xinput library). This is basically a dynamic function implementation (evaluated at runtime), similar to what we do with virtual funtions overrides. We can later either chose to call this dynamically implemented custom version of the function or call the actual original function from xinput! 
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

// here we define a macro to rename XInputGetState to XInputGetState_ so that any call to the former in the code will actually reference the pointer we made to the defined type x_input_get_state 
#define XInputGetState XInputGetState_
// this way we can call the XInputGetState for the controller code later in the code without having to worry about the specifications and requirements needed for the original xinput function, since we are dynamically setting/defining (overriding) the function ourselves

// Same as above but grouped for the XInputSetState function version 
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}

global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputSetState XInputSetState_

// This will be called from WinMain to get the actual windows xinput lib function and pass it to our defined function pointer type (x_input_set_state)
internal void Win32LoadXInput(void)
{
    // Get lib module handle from xinput1_3.dll (we chose this 1_3 version since it is older and will run on more machines since it should be present on them) 
    HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
    if(XInputLibrary)
    {   
        // Then we get the process address of the xinput lib functions we want from that dll cast to our defined function pointer type and pass it to the marco we made for it (XInputGetState = XInputGetState_ - Rememeber?)
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        
        // safeguard in case there is no address return from the above getprocaddress (not really necessary, see lesson 6 min 1:18:00, but we can keep it here as it is always good pratcice)
        if(!XInputGetState) XInputGetState = XInputGetStateStub;
        
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) XInputSetState = XInputSetStateStub;
    }
}


internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect); 
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result; 
}

internal void RenderWeirdGradient(win32_onscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    // TODO: Let's see what the optimizer does if we pass the buffer struct as value 

    uint8 *Row = (uint8 *)Buffer->Memory;
    for(int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < Buffer->Width; ++X)
        {
            uint8 Blue  = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);           
        }
    
        Row += Buffer->Pitch;
    }
}

internal void Win32ResizeDIBSection(win32_onscreen_buffer *Buffer, int Width, int Height)
{


    // TODO: Bulletproof this
    // Maybe don't free first, free after, then free first if that fails 

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    int BytesPerPixel = 4; 

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;    
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;           
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width*BytesPerPixel;

    // TODO: Probably clear this to black 

}

internal void Win32DisplayBufferInWindow(win32_onscreen_buffer *Buffer,
                                         HDC DeviceContext, 
                                         int WindowWidth, int WindowHeight)
{

    // TODO: Aspect ratio correction 
    // TODO: Play with stretch modes

    StretchDIBits(DeviceContext,   
                  0, 0, WindowWidth, WindowHeight,  // destination 
                  0, 0, Buffer->Width, Buffer->Height,  // source 
                  Buffer->Memory,
                  &Buffer->Info,
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




        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            // First we set up this case to decrypt the messages above into a reliable bool that can then be executed with its corresponding key state and key code. The WasDown, IsDown bool check below for example, serves to properly exectute the if conditions with the key codes at the most appropriate timing where the keys were pressed/released 

            uint32 VKCode = WParam;

            // Day 6 - around min 51
            // here we check if the previous key state for WM_KEYDOWN was in fact down (pressed down before the message was sent) or not
            // for this, we and(bitwise &) the LParam, which holds this information (bit 30 in this case) in its bitfield with the a shift left 30 on value 1, so we can get/set the 30th bit (bit 30 of WM_KEYDOWN LParam has the info of the previous key state) and return a bool by comparing this bitwise& with != 0 (if result is not 0 it means bit 30 in LParam is 1 and key was indeed pressed befire themessage was sent which will give us the bool 1 (true))
            // For more info about the LParam bitfield for WM_KEYDOWN check the MSDN documention 
            bool WasDown = ((LParam & (1 << 30)) != 0);

            //  Here we do the same but for WM_KEYDOWN, checking the bit 31 (checks current state, when message is sent). If the value is 0, it means the key is currently down
            bool IsDown = ((LParam & (1 << 31)) == 0);
            
            if(WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                }
                else if(VKCode == 'A')
                {
                }
                else if(VKCode == 'S')
                {
                }
                else if(VKCode == 'D')
                {
                }
                else if(VKCode == 'Q')
                {
                }
                else if(VKCode == 'E')
                {
                }
                else if(VKCode == VK_UP)
                {
                }
                else if(VKCode == VK_LEFT)
                {
                }
                else if(VKCode == VK_DOWN)
                {
                }
                else if(VKCode == VK_RIGHT)
                {
                }
                else if(VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if(IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if(WasDown)
                    {
                        OutputDebugStringA("WasDown ");
                    }
                    OutputDebugStringA("\n");
                }
                else if(VKCode == VK_SPACE)
                {
                }
            }
        } break;


    
        case WM_PAINT:
        {   
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
                                       Dimension.Width, Dimension.Height);

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

    Win32LoadXInput();

    WNDCLASSA  WindowClass {};
    
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW; 
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
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            GlobalRunning = true;
            while(GlobalRunning)
            {
                MSG Message {};
                if(Message.message == WM_QUIT)
                {
                    GlobalRunning = false;
                }

                while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) 
                {
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                // TODO: Should we poll this more frequently
                for(DWORD ControllerIndex = 0; 
                    ControllerIndex < XUSER_MAX_COUNT; 
                    ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    // if return from this func is ERROR_SUCCESS....
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // .. then this controller is plugged in 
                        // TODO: See if ControllerState.dwPacketNumber increments too rapidly
                        
                        // This is a nice practice to shorten the pointer access to reach the member values we need. By snapping a pointer to XINPUT_GAMEPAD with the address of the struct it belongs to, and exact location, we can then afterwards simply use this new pointer Pad to access the wButtons for state checks
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RighShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        // thumb sticks     
                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;

                        // change coordinate offsets with Left Thumb Stick 
                        XOffset += StickX >> 12;
                        YOffset += StickY >> 12;

                    }
                    else
                    {
                        // ...the controller is not available 
                    }

                

                }


                //  // Test controller vibration 
                //  XINPUT_VIBRATION Vibration;
                //  Vibration.wLeftMotorSpeed = 60000;
                //  Vibration.wRightMotorSpeed = 60000;
                //  XInputSetState(0, &Vibration);



                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
                                           Dimension.Width, Dimension.Height);
                
                // Increasing this with the controller Sticks in the controller code block 
                // ++XOffset; 
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



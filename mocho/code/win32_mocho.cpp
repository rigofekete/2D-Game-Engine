#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
// TODO: Implement sine ourselves
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32 bool32;

typedef float real32;
typedef double real64;

struct win32_onscreen_buffer
{
    // Pixels are always 32-bits wide, LE Memory Order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};


//TODO: These are globals for now
global_variable win32_onscreen_buffer GlobalBackbuffer {};
global_variable bool32 GlobalRunning;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

struct win32_window_dimension
{
    int Width;
    int Height;
};

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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

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


// Set grouped for XInputGetState dynamic function version 
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);  

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;

#define XInputGetState XInputGetState_



// Same as above but grouped for the XInputSetState dynamic function version 
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputSetState XInputSetState_




#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)

typedef DIRECT_SOUND_CREATE(direct_sound_create);



// This will be called from WinMain to get the actual windows xinput lib function and pass it to our defined function pointer type (x_input_set_state)
internal void Win32LoadXInput(void)
{ 
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

    if(!XInputLibrary) 
    {
      //TODO: Diagnostic
      XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
    }

    if(!XInputLibrary) 
    {
      //TODO: Diagnostic
      XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if(XInputLibrary)
    {   
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) XInputGetState = XInputGetStateStub;
        
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) XInputSetState = XInputSetStateStub;
	//TODO: Diagnostic
    }
    else
    {
      //TODO: Diagnostic
    }
}


internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE: Load the library 
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
        // NOTE: Get a DirectSound object - cooperative
	direct_sound_create* DirectSoundCreate = (direct_sound_create*)
	  GetProcAddress(DSoundLibrary, "DirectSoundCreate");

	// TODO: Double-check that this works on XP - DirectSound8 or 7??
	LPDIRECTSOUND DirectSound;
	if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
	{
	  WAVEFORMATEX WaveFormat = {};
	  WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	  WaveFormat.nChannels = 2;
	  WaveFormat.nSamplesPerSec = SamplesPerSecond;
	  WaveFormat.wBitsPerSample = 16;
	  WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
	  WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
	  WaveFormat.cbSize = 0;

	  if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
	  {
	    // initialize struct members to zero 
	    DSBUFFERDESC BufferDescription = {};
	    BufferDescription.dwSize = sizeof(BufferDescription);
	    BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

	    // NOTE: Create a primary buffer
	    // IMPORTANT: This primary buffer will only be used to actually set the format to the sound card/chip sample rate. The actual implemention will be done in the secondary buffer. This is done to make sure the calls to the main buffer (secondary buffer) are done with the correct format in place without the need of any resampling additional tasks. We can simply consider this as a handle to the soundcard mode/format (to use the SetFormat function) and not an actual buffer.
	    // TODO: DSBCAPS_GLOBALFOCUS?
	    LPDIRECTSOUNDBUFFER PrimaryBuffer;
	    if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
	    {
	      HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat); 
	      if(SUCCEEDED(Error))
	      {
		    // NOTE: We have finaly set the format!
		    OutputDebugStringA("Primary buffer format was set.\n");
	      }
	      else
	      {
		//TODO: Diagnostic
	      }
	    }
	    else
	    {
	      //TODO: Diagnostic
	    }

	  }
	  else
	  {
	    //TODO: Diagnostic
	  }

	  // NOTE: Create a secondary buffer
	  // TODO: DSBCAPS_GETCURRENTPOSITION2
	  DSBUFFERDESC BufferDescription = {};
	  BufferDescription.dwSize = sizeof(BufferDescription);
	  BufferDescription.dwFlags = 0;
	  BufferDescription.dwBufferBytes = BufferSize;
	  BufferDescription.lpwfxFormat = &WaveFormat;
	  HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0); 
	  if(SUCCEEDED(Error))
	  {
	    OutputDebugStringA("Secondary buffer created succesfully.\n");
	  }
	  
	}
	else
	{
	  //TODO: Diagnostic
	}
    }
    else
    {
      //TODO: Diagnostic
    }

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
            uint32 VKCode = WParam; 
            bool32 WasDown = ((LParam & (1 << 30)) != 0);
            bool32 IsDown = ((LParam & (1 << 31)) == 0);
            
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

	    // we defined a new type bool32 so we dont need to rely on 1 or 0 as possible results and only care about the bitwise operation having a value or not. 
	    // This way we dont need to do this:
	    //
	    // bool AltKeyWasDown = ((LParam & (1 << 29)) != 0);
	    bool32 AltKeyWasDown = (LParam & (1 << 29));
	    if((VKCode == VK_F4) && AltKeyWasDown)
	    {
	      GlobalRunning = false;
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
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;

    }

    return Result;
}


struct win32_sound_output
{
  // Sound test
  int SamplesPerSecond = 48000;
  // hz(waves) per second
  int ToneHz = 256;
  int16 ToneVolume = 3000;
  uint32 RunningSampleIndex = 0;
  int WavePeriod = SamplesPerSecond/ToneHz;
  // size of the left-right wave pair 
  int BytesPerSample = sizeof(int16)*2;
  // Convert samples per second to total bytes needed in the buffer (each sample/measure takes 4 bytes)
  int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;
  real32 tSine;
  int LatencySampleCount;
};


void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{

    // Visual example of how the sound buffer will work 
    // int16  int16  int16 int16 ... 
    // [LEFT  RIGHT] LEFT  RIGHT LEFT RIGHT LEFT ...
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock,
			      BytesToWrite,
			      &Region1, &Region1Size,
			      &Region2, &Region2Size,
			      0)));
    {

	// TODO: assert that Region1Size/Region2Size is valid
	DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
	int16 *SampleOut = (int16 *)Region1;
	// TODO: collapse these 2 loops
	for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
	{
	    // TODO: Draw this in the diagram for to understand what is going on. Make some comments here if needed.
	    real32 SineValue = sinf(SoundOutput->tSine);
	    // convert amplitude of the sine wave value to audio sample format
	    int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
	    *SampleOut++ = SampleValue;
	    *SampleOut++ = SampleValue;

	    SoundOutput->tSine += 2.0f*Pi32*1.0f/(real32)SoundOutput->WavePeriod;
	    ++SoundOutput->RunningSampleIndex;
	}

	DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
	SampleOut = (int16 *)Region2;
	for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
	{
	    real32 SineValue = sinf(SoundOutput->tSine);
	    int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
	    *SampleOut++ = SampleValue;
	    *SampleOut++ = SampleValue;

	    SoundOutput->tSine += 2.0f*Pi32*1.0f/(real32)SoundOutput->WavePeriod;
	    ++SoundOutput->RunningSampleIndex;
	}

	// It is very important to unlock the byte location previously locked in order to properly play the sound
	GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
    
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

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0,
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

	    // Graphics test 
            int XOffset = 0;
            int YOffset = 0;
	  
	    // Sound test 
	    win32_sound_output SoundOutput = {};



	    // TODO: Maybe make this sixty seconds? 
	    SoundOutput.SamplesPerSecond = 48000;
	    // hz per second
	    SoundOutput.ToneHz = 256;
	    SoundOutput.ToneVolume = 3000;
	    // SoundOutput.RunningSampleIndex = 0;
	    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
	    // size of the left-right wave pair 
	    SoundOutput.BytesPerSample = sizeof(int16)*2;
	    SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
	    // set latency buffer of 1/15 of a second (around 66 ms)
	    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond  / 15;
	    Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
	    Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample);
	    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

	    LARGE_INTEGER LastCounter;
	    QueryPerformanceCounter(&LastCounter);
	    uint64 LastCycleCount = __rdtsc();
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
		  if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
		  {
		    // .. then this controller is plugged in 
		    // TODO: See if ControllerState.dwPacketNumber increments too rapidly

		    // This is a nice practice to shorten the pointer access to reach the member values we need.
		    // By snapping a pointer to XINPUT_GAMEPAD with the address of the struct it belongs to, 
		    // and exact location, we can then afterwards simply use this new pointer Pad to access the wButtons
		    // for state checks
		    XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
		    bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
		    bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		    bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		    bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
		    bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
		    bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
		    bool32 LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
		    bool32 RighShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
		    bool32 AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
		    bool32 BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
		    bool32 XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
		    bool32 YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

		    // thumb sticks     
		    int16 StickX = Pad->sThumbLX;
		    int16 StickY = Pad->sThumbLY;

		    // TODO: We will do deadzone handling later using:
		    // #define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
		    // #define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689

		    // change coordinate offsets with Left Thumb Stick 
		    XOffset += StickX / 4096;
		    YOffset += StickY / 4096;


		    SoundOutput.ToneHz = 512 + (int)(256.0f*((real32)StickY / 30000.0f));
		    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;

                    }
                    else
                    {
                        // ..the controller is not available 
                    }
                }


                 //  // Test controller vibration 
                 //  XINPUT_VIBRATION Vibration;
                 //  Vibration.wLeftMotorSpeed = 60000;
                 //  Vibration.wRightMotorSpeed = 60000;
                 //  XInputSetState(0, &Vibration);

                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
		
		DWORD PlayCursor;
		DWORD WriteCursor; 
		if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
		{
		    DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
					SoundOutput.SecondaryBufferSize);

		    // we define this to have 1/15 of a second additional buffer to always write ahead of the play cursor and reduce 
		    // the latency needed to process the sound since we are doing it ahead of time
		    // Of course we need to map all of this to the actual buffer in case it wraps around the end hence the use of
		    // tht mod operator again
		    DWORD TargetCursor = ((PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) %
					  SoundOutput.SecondaryBufferSize);
		    DWORD BytesToWrite;
		    // TODO: Change this to using a lower latency offset from the playcursor 
		    // when we actually start having sound effects.
		    if(ByteToLock > TargetCursor)
		    {
		      BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
		      BytesToWrite += TargetCursor;
		    }
		    else
		    {
		      BytesToWrite = TargetCursor - ByteToLock;
		    }

		    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
		}

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, 
                                           Dimension.Width, Dimension.Height);
                
                // Increasing this with the controller Sticks in the controller code block 
                // ++XOffset; 

		uint64 EndCycleCount = __rdtsc();

		LARGE_INTEGER EndCounter;
		QueryPerformanceCounter(&EndCounter);

		// TODO: Display the value here
		uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
		int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
		// CounterElapsed is total number of cycles the cpu did after each frame/loop
		// and PerfCountFrequency is the counts per second
		real64 MSPerFrame = ((1000.0f*(real64)CounterElapsed) / (real64)PerfCountFrequency);
		real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
		// these are getting casted to int32 just because of how the print formatted string function wsprintf historically expects int32 types for integers 
		real64 MCPF = (real64)(CyclesElapsed / (1000.0f * 1000.0f));

		char Buffer[256];
		sprintf(Buffer, "%.02fms/f, %.02ff/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
		OutputDebugStringA(Buffer);

		LastCounter = EndCounter;
		LastCycleCount = EndCycleCount;
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




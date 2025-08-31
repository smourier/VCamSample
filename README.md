# WinCamHTTP
This solution contains a basic Media Foundation Virtual Camera application that streams from an MJPEG HTTP source. It works only on Windows 11 thanks to the [MFCreateVirtualCamera](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera) API.

There are two projects in the solution:

* **WinCamHTTPSource**: the Media Source that reads from an MPJEG stream.
* **WinCamHTTPSetup**: the main application that creates and manages the virtual camera.

## Build and Run

To use the virtual camera:

* Build in debug or release.
* **WinCamHTTPSource**: the Media Source that reads from an MJPEG stream.
* **WinCamHTTPSetup**: the main application that creates and manages the virtual camera.
  - Enter an `MJPEG URL`.
  - Pick a `Resolution` (default `1920x1080`).
  - Optionally adjust the `Camera Name`.
  - Click `Start` to create and start the virtual camera for the current session.
  - Click `Stop` to remove it.
* Open the Windows Camera app, a web browser using the ImageCapture API, or any application that supports virtual cameras.

You should now see the virtual camera output in supported applications.
* Go to the build output and register the media source (a COM object) with a command similar to this: `regsvr32 WinCamHTTPSource.dll` (you must run this as Administrator. Registration happens under `HKLM` so the system Frame Server can load it).
* Run the `WinCamHTTPSetup` app (it requires Administrator rights to write configuration under `HKLM`).

## Configuration (HKLM)

The app and the media source share configuration via the system registry so that the Frame Server (running as a system service) and the UI (running as the user) agree on the same values. Keys are stored under:

`HKEY_LOCAL_MACHINE\SOFTWARE\WinCamHTTP`

Values used today:

- `URL` (`REG_SZ`): MJPEG endpoint, e.g., `http://host:port/path`.
- `Width` (`REG_DWORD`): desired frame width (e.g., `1920`).
- `Height` (`REG_DWORD`): desired frame height (e.g., `1080`).
- `FriendlyName` (`REG_SZ`): camera display name chosen in the UI.

Notes:

- The `WinCamHTTPSetup` app writes these values when you click `Start`. Because they live under `HKLM`, running the app as Administrator is required.
- The `WinCamHTTPSource` DLL (loaded by the Windows Frame Server) reads these values on startup to configure the media source and MJPEG ingest.
* The media source uses `Direct2D` and `DirectWrite` to create images. It will then create Media Foundation samples from these. To create MF samples, it can use:
  * The GPU, if a Direct3D manager has been provided by the environment. This is the case of the Windows 11 camera app.
  * The CPU, if no Direct3D environment has been provided. In this case, the media source uses a WIC bitmap as a render target and it then copies the bits over to an MF sample. The ImageCapture API code embedded in Chrome or Edge, Teams, etc., is an example of such a D3D-less environment.
-- The `WinCamHTTPSetup` app writes these values when you click `Start`. Because they live under `HKLM`, running the app as Administrator is required.
-- The `WinCamHTTPSource` DLL (loaded by the Windows Frame Server) reads these values on startup to configure the media source and MJPEG ingest.
* The media source provides RGB32 and NV12 formats as most setups prefer the NV12 format. Samples are initially created as RGB32 (Direct2D) and converted to NV12. To convert the samples, the media source uses two ways:
  * The GPU, if a Direct3D manager has been provided, using Media Foundation's [Video Processor MFT](https://learn.microsoft.com/en-us/windows/win32/medfound/video-processor-mft).
  * The CPU, if no Direct3D environment has been provided. In this case, the RGB to NV12 conversion is done in the code (so on the CPU).
  * If you want to force RGB32 mode, you can change the code in `MediaStream::Initialize` and set the media types array size to 1 (check comments in the code).

* The code currently has an issue where the virtual camera screen is shown in the preview window of apps such as Microsoft Teams, but it's not rendered to the communicating party. If you know why this happens, feel free to contribute!

## Troubleshooting "Access Denied" on IMFVirtualCamera::Start method
If you get access denied here, it's probably the same issue as described in https://github.com/smourier/VCamSample/issues/1.

Here is a summary:

* The COM object that serves as a Virtual Camera Source (here `WinCamHTTPSource.dll`) must be accessible by the two Windows 11 services **Frame Server** & **Frame Server Monitor** (running as `svchost.exe`).
* These two services usually run as *Local Service* & *Local System* credentials respectively.
* If you compile or build in a directory under your compiling user's root, for example something like `C:\Users\<your login>\source\repos\WinCamHTTP\x64\Debug\` or somewhere restricted in some way, **it won't work** since these two services will need to access that.

=> So the solution is just to either copy the output directory once built (or downloaded) somewhere where everyone has access and register `WinCamHTTPSource.dll` from there, or copy/checkout the whole repo where everyone has access and build and register there.

## Tracing

The code output lots of interesting traces. It's quite important in this virtual camera environment because there's not just your process that's involved but at least 4: the VCamSample app, the Windows Frame Server, the Windows camera monitor, and the reader app (camera, etc.). They all load the media source COM object in-process.

So to read these ETW traces, use WpfTraceSpy you can download here https://github.com/smourier/TraceSpy. Configure an ETW Provider with the GUID set to `964d4572-adb9-4f3a-8170-fcbecec27467`.

If you see no frames:

- Ensure the MJPEG URL is reachable and actually serves multipart JPEGs.
- Confirm the app has written the configuration under `HKLM\SOFTWARE\VCamSample` (requires Administrator).
- Check the ETW trace output for media type negotiation and sample delivery messages.
- Verify `VCamSampleSource.dll` is registered from a directory accessible by the Frame Server services (see section above).

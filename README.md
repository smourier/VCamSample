# VCamSample
This solution contains a Media Foundation Virtual Camera Sample. It works only on Windows 11 thanks to the [MFCreateVirtualCamera](https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera) API.

There are two projects in the solution:

* VCamSampleSource: the Media Source that provides RGB32 and NV12 VCamSampleSource
* VCamSample: the "driver" application that does very little but calls MFCreateVirtualCamera.



To test the project:

* Build in debug or release
* Go to the build output and register the media source with a command similar to this: "regsvr32 VCamSampleSource.dll" (you must run this as administrator, it' not possible to register a Virtual Camera media source in HKCU)
* Run the VCamSample app.
* Run for example the Windows Camera app

You should now see something like this (changing in real time):





## Notes

* The media source uses Direct2D and DirectWrite to create samples. For that, it requires a Direct3D manager to be provided by the environment. If the Direct3D manager is not provided (through a call IMediaSource:SetD3DManager) then samples are provided but their buffer are left untouched (empty).
* The media source provided RGB32 and NV12. Samples are initially created as RGB32 (DirectX/Direct2D) and converted to NV12 using Media Foundation's [Video Processor MFT](https://learn.microsoft.com/en-us/windows/win32/medfound/video-processor-mft). The way it's used (GPU) therefore also depends on a D3D environment.
* It means the VCamSampleSource will work but output nothing visible in environments that don't call IMediaSource:SetD3DManager. Such environments are WebCam handlers embedded in Chrome or Edge, etc.
* It's relatively easy to modify the sample to provide your own buffer in the case where D3D is absent and support these environements. The code to change is located in MediaStream::RequestSample method.



## Tracing

The code output lots of interesting traces. It's quite important in the virtual camera environment because there's not just your process that's involved but at least 4: the VCamSample app, the Windows Frame Server, the Windows camera monitor, and the reader. They all load the media source in-process.

It doesn't use OutputDebugString because it's 100% old, crappy, truncating text, slow, etc.  Instead it uses Event Tracing for Windows ("ETW") in "string-only" mode (the mode where it's very simple and you don't have register traces and complicated readers).

So to read these ETW traces, use WpfTraceSpy https://github.com/smourier/TraceSpy . Configure an ETW Provider with the GUID set to 964d4572-adb9-4f3a-8170-fcbecec27467

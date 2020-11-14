# Squall's D3D12 Graphic Demo
Custom demo of D3D12 Rendering. <br>
It's not the best but it's how I try to maximize the advantage of D3D12. <br>
I borrow the Unity's asset system to get all resources as ID3D12Resource interface, and customize my renderer in native DLL. <br>
I hope one day I can work outside of my country and be participated in 3A game team :). <br>

# Requirement to run the demo
1. Unity Editor 2018.4.17f1+ <br><br>
2. Win10 for running D3D12. With version 1909 + 2004 SDK. <br>
(Maybe you can try a lower version with 2004 SDK installed, I just enumerate my developing environment here.) <br><br>
3. Developer Mode Enabled. Measuring GPU time needs it. <br>
Here is the document that shows you how to enable developer mode: <br>
https://docs.microsoft.com/en-us/windows/apps/get-started/enable-your-device-for-development <br><br>
4. NVIDIA RTX Graphic Cards <br>
Yes, I implement the DXR ray tracing shader and don't consider fallback API for GTX series. <br>
(Since GTX series are impossible to get real time performance.) <br>
You can try AMD RX cards also, but I'm not sure since I don't have one. <br><br>

Missing one of above requirement the demo would crash. <br><br>

# Features
**Multithread Rendering** <br>
![alt text](https://i.imgur.com/yFHJejE.jpg) <br>
![alt text](https://i.imgur.com/xOmMTow.jpg) <br>
This is the key design of D3D12, Microsoft wants us to submit work on different threads. <br>
As it shown in image, I use AMD R5 5600X (6C12T) to run the test. I set 12 threads to rendering. <br>
Now the total draw calls in this shot is 377, which means each thread renders about 31 draw calls. <br>
You can specify the number of threads in my system, max up to min(16, MaxLogicialCpuCore). <br>

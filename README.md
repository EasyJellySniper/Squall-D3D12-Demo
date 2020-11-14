# Squall's D3D12 Graphic Demo
Custom demo of D3D12 Rendering. <br>
It's not the best but it's how I try to maximize the advantage of D3D12. <br>
I borrow the Unity's asset system to get all resources as ID3D12Resource interface, and customize my renderer in native DLL. <br>
I also implement some material management(PSO) and shader parser. <br>
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
**Multithread Forward Rendering** <br>
![alt text](https://i.imgur.com/yFHJejE.jpg) <br>
![alt text](https://i.imgur.com/xOmMTow.jpg) <br>
This is the key design of D3D12, Microsoft wants us to submit work on different threads. <br>
As it shown in image, I use AMD R5 5600X (6C12T) to run the test. I set 12 threads to rendering. <br>
Now the total draw calls in this shot is 377, which means each thread renders about 31 draw calls. <br>
You can specify the number of threads in my system, max up to min(16, MaxLogicialCpuCore). <br><br>

![alt text](https://i.imgur.com/eWkUayG.png) <br>
My rendering pipeline, the work with a star mark means I use multithread on it. <br><br>

**Instance Based Rendering** <br>
In demo scene, there are 2XXX draw calls at the beginning. <br>
I collect the objects that has the same mesh/material, and implement the instance based rendering. <br><br>

**SM5.1 Style Texture Management** <br>
D3D12 introduce the concept of Descriptor Heap. (https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps) <br>
I collect all ShaderResourceView into a big texture descriptor heap. <br>
My texture sampling is gonna like this: <br>
```
// need /enable_unbounded_descriptor_tables when compiling
Texture2D _SqTexTable[] : register(t0);
SamplerState _SqSamplerTable[] : register(s0);
#define SQ_SAMPLE_TEXTURE(x,y,z) _SqTexTable[x].Sample(_SqSamplerTable[y], z)
float4 color = SQ_SAMPLE_TEXTURE(_DiffuseTexIndex, _LinearSamplerIndex);
```
Shader model 5.1 provides dynamic indexing and we can have more flexible way for texture binding. <br><br>

using System;
using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// sq camera
/// </summary>
[RequireComponent(typeof(Camera))]
public class SqCamera : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern bool AddCamera(CameraData _cameraData);

    [DllImport("SquallGraphics")]
    static extern void RemoveCamera(int _instanceID);

    [DllImport("SquallGraphics")]
    static extern void SetViewProjMatrix(int _instanceID, Matrix4x4 _view, Matrix4x4 _proj, Matrix4x4 _projCulling, Vector3 _position);


    [DllImport("SquallGraphics")]
    static extern void SetViewPortScissorRect(int _instanceID, ViewPort _viewPort, RawRect _rawRect);

    [DllImport("SquallGraphics")]
    static extern void SetDebugDepth(int _instance, IntPtr _debugDepth);

    [DllImport("SquallGraphics")]
    static extern void SetRenderMode(int _instance, int _renderMode);

    [DllImport("SquallGraphics")]
    static extern void GetDebugDepth(int _instance, IntPtr _debugDepth);

    /// <summary>
    /// msaa factor
    /// </summary>
    public enum MsaaFactor
    {
        None = 1,
        Msaa2x = 2,
        Msaa4x = 4,
        Msaa8x = 8
    }

    /// <summary>
    /// render mode
    /// </summary>
    public enum RenderMode
    {
        WireFrame = 1,
        Depth,
        ForwardPass
    }

    /// <summary>
    /// camera data will be sent to native plugin
    /// </summary>
    struct CameraData
    {
        /// <summary>
        /// clear color
        /// </summary>
        public float[] clearColor;

        /// <summary>
        /// camera order
        /// </summary>
        public float camOrder;

        /// <summary>
        /// rendering path
        /// </summary>
        public int renderingPath;

        /// <summary>
        /// allow msaa
        /// </summary>
        public int allowMSAA;

        /// <summary>
        /// instance id
        /// </summary>
        public int instanceID;

        /// <summary>
        /// max to 8 render targets
        /// </summary>
        public IntPtr[] renderTarget;

        /// <summary>
        /// depth target
        /// </summary>
        public IntPtr depthTarget;
    }

    struct ViewPort
    {
        /// <summary>
        /// view port data
        /// </summary>
        public float TopLeftX;
        public float TopLeftY;
        public float Width;
        public float Height;
        public float MinDepth;
        public float MaxDepth;
    }

    struct RawRect
    {
        /// <summary>
        /// raw rect data
        /// </summary>
        public int left;
        public int top;
        public int right;
        public int bottom;
    }

    /// <summary>
    /// instance
    /// </summary>
    public static SqCamera instance = null;

    /// <summary>
    /// render mode
    /// </summary>
    public RenderMode renderMode = RenderMode.WireFrame;

    /// <summary>
    /// msaa sample
    /// </summary>
    public MsaaFactor msaaSample = MsaaFactor.None;

    /// <summary>
    /// render target
    /// </summary>
    RenderTexture renderTarget;

    /// <summary>
    /// debug depth
    /// </summary>
    RenderTexture debugDepth;

    Camera attachedCam;
    CameraData camData;
    MsaaFactor lastMsaaSample;

    void Start ()
    {
        // one instance only
        if (instance != null)
        {
            enabled = false;
            return;
        }

        instance = this;
        attachedCam = GetComponent<Camera>();
        attachedCam.renderingPath = RenderingPath.Forward;  // currently only fw is implement
        attachedCam.cullingMask = 0;    // draw nothing
        attachedCam.allowHDR = true;    // force hdr mode

        CreateRenderTarget();
        CreateCameraData();
        GetDebugDepth(attachedCam.GetInstanceID(), debugDepth.GetNativeDepthBufferPtr());
        lastMsaaSample = msaaSample;
    }

    void OnDestroy()
    {
        if (renderTarget)
        {
            renderTarget.Release();
            DestroyImmediate(renderTarget);
        }
        instance = null;
    }

    void Update()
    {
        int instanceID = attachedCam.GetInstanceID();
        SetRenderMode(instanceID, (int)renderMode);

        // check aa change
        if (lastMsaaSample != msaaSample)
        {
            RemoveCamera(instanceID);
            OnDestroy();
            Start();
            SqGraphicManager.instance.resetingFrame = true;
        }

        // update VP matrix
        SetViewProjMatrix(instanceID,
            attachedCam.worldToCameraMatrix,
            GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, true),
            GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, false),
            transform.position);

        Rect viewRect = attachedCam.pixelRect;
        ViewPort vp;
        vp.TopLeftX = viewRect.xMin;
        vp.TopLeftY = viewRect.yMin;
        vp.Width = viewRect.width;
        vp.Height = viewRect.height;
        vp.MinDepth = 0f;
        vp.MaxDepth = 1f;

        RawRect rr;
        rr.left = 0;
        rr.top = 0;
        rr.right = (int)viewRect.width;
        rr.bottom = (int)viewRect.height;

        SetViewPortScissorRect(instanceID, vp, rr);
        lastMsaaSample = msaaSample;
    }

    void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        if (renderMode == RenderMode.Depth)
        {
            Graphics.Blit(debugDepth, destination);
        }
        else
        {
            Graphics.Blit(renderTarget, destination);
        }
    }

    void CreateRenderTarget()
    {
        renderTarget = new RenderTexture(attachedCam.pixelWidth, attachedCam.pixelHeight, 32, RenderTextureFormat.DefaultHDR);
        renderTarget.name = name + " Target";
        renderTarget.antiAliasing = 1;
        renderTarget.bindTextureMS = false;

        // actually create so that we have native resources
        renderTarget.Create();

        debugDepth = new RenderTexture(attachedCam.pixelWidth, attachedCam.pixelHeight, 32, RenderTextureFormat.Depth);
        debugDepth.name = "Debug Depth";
        debugDepth.Create();
    }

    void CreateCameraData()
    {
        // create camera data
        camData.instanceID = attachedCam.GetInstanceID();

        camData.clearColor = new float[4];
        camData.clearColor[0] = attachedCam.backgroundColor.linear.r;
        camData.clearColor[1] = attachedCam.backgroundColor.linear.g;
        camData.clearColor[2] = attachedCam.backgroundColor.linear.b;
        camData.clearColor[3] = attachedCam.backgroundColor.linear.a;

        camData.camOrder = attachedCam.depth;
        camData.renderingPath = (int)attachedCam.renderingPath;
        camData.allowMSAA = (int)msaaSample;

        camData.renderTarget = new IntPtr[8];
        camData.renderTarget[0] = renderTarget.GetNativeTexturePtr();
        camData.depthTarget = renderTarget.GetNativeDepthBufferPtr();

        // add camera to native plugin
        if (!AddCamera(camData))
        {
            Debug.LogError("[Error] SqCamera: AddCamera() Failed.");
            enabled = false;
            OnDestroy();
        }
    }
}

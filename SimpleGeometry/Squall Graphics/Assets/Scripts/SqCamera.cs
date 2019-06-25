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
    static extern void SetViewProjMatrix(int _instanceID, Matrix4x4 _viewProj);


    [DllImport("SquallGraphics")]
    static extern void SetViewPortScissorRect(int _instanceID, ViewPort _viewPort, RawRect _rawRect);

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
        public long left;
        public long top;
        public long right;
        public long bottom;
    }

    /// <summary>
    /// msaa sample
    /// </summary>
    public MsaaFactor msaaSample = MsaaFactor.None;

    /// <summary>
    /// render target
    /// </summary>
    public RenderTexture renderTarget;

    Camera attachedCam;
    CameraData camData;
    MsaaFactor lastMsaaSample;

    void Start ()
    {
        attachedCam = GetComponent<Camera>();
        attachedCam.renderingPath = RenderingPath.Forward;  // currently only fw is implement
        attachedCam.cullingMask = 0;    // draw nothing
        attachedCam.allowHDR = true;    // force hdr mode

        CreateRenderTarget();
        CreateCameraData();
        lastMsaaSample = msaaSample;
    }

    void OnDestroy()
    {
        if (renderTarget)
        {
            renderTarget.Release();
            DestroyImmediate(renderTarget);
        }
    }

    void Update()
    {
        int instanceID = attachedCam.GetInstanceID();

        // check aa change
        if (lastMsaaSample != msaaSample)
        {
            RemoveCamera(instanceID);
            OnDestroy();
            Start();
        }

        // update VP matrix
        if (transform.hasChanged)
        {
            Matrix4x4 viewProj = attachedCam.worldToCameraMatrix * GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, true);
            SetViewProjMatrix(instanceID, viewProj);
            transform.hasChanged = false;

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
            rr.right = (long)viewRect.width;
            rr.bottom = (long)viewRect.height;

            SetViewPortScissorRect(instanceID, vp, rr);
        }

        lastMsaaSample = msaaSample;
    }

    void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        Graphics.Blit(renderTarget, destination);
    }

    void CreateRenderTarget()
    {
        renderTarget = new RenderTexture(Screen.width, Screen.height, 24, RenderTextureFormat.DefaultHDR);
        renderTarget.name = name + " Target";
        renderTarget.antiAliasing = 1;
        renderTarget.bindTextureMS = false;

        // actually create so that we have native resources
        renderTarget.Create();
    }

    void CreateCameraData()
    {
        // create camera data
        camData.instanceID = attachedCam.GetInstanceID();

        camData.clearColor = new float[4];
        camData.clearColor[0] = attachedCam.backgroundColor.r;
        camData.clearColor[1] = attachedCam.backgroundColor.g;
        camData.clearColor[2] = attachedCam.backgroundColor.b;
        camData.clearColor[3] = attachedCam.backgroundColor.a;

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

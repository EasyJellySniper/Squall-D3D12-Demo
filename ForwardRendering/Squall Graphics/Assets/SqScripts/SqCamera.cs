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
    static extern void SetViewProjMatrix(int _instanceID, Matrix4x4 _view, Matrix4x4 _proj, Matrix4x4 _projCulling, Matrix4x4 _invView, Matrix4x4 _invProj, Vector3 _position, float _far, float _near);


    [DllImport("SquallGraphics")]
    static extern void SetViewPortScissorRect(int _instanceID, ViewPort _viewPort, RawRect _rawRect);

    [DllImport("SquallGraphics")]
    static extern void SetRenderMode(int _instance, int _renderMode);

    [DllImport("SquallGraphics")]
    static extern int GetNativeFrameIndex();

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

    RenderTexture renderTarget;
    RenderTexture transparentDepth;
    RenderTexture normalTarget;
    RenderTexture result;

    Camera attachedCam;
    CameraData camData;
    MsaaFactor lastMsaaSample;

    void Start ()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.Instance == null)
        {
            enabled = false;
            return;
        }

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
        lastMsaaSample = msaaSample;
    }

    void OnDestroy()
    {
        SqUtility.SafeDestroyRT(ref renderTarget);
        SqUtility.SafeDestroyRT(ref transparentDepth);
        SqUtility.SafeDestroyRT(ref normalTarget);
        SqUtility.SafeDestroyRT(ref result);
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
            SqGraphicManager.Instance.resetingFrame = true;
        }

        // update VP matrix
        Matrix4x4 projRendering = GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, true);
        Matrix4x4 projCulling = GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, false);
        Matrix4x4 invView = attachedCam.worldToCameraMatrix.inverse;    // didn't be the same as CameraToWorldMatrix, this is bullshit lol

        SetViewProjMatrix(instanceID,
            attachedCam.worldToCameraMatrix,
            projRendering,
            projCulling,
            invView,
            projRendering.inverse,
            transform.position, attachedCam.farClipPlane, attachedCam.nearClipPlane);

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
            Graphics.Blit(transparentDepth, destination);
        }
        else
        {
            Graphics.Blit(result, destination);
        }
    }

    void CreateRenderTarget()
    {
        // for receive result
        renderTarget = SqUtility.CreateRT(attachedCam.pixelWidth, attachedCam.pixelHeight, 32, RenderTextureFormat.DefaultHDR, "Color Target");
        transparentDepth = SqUtility.CreateRT(attachedCam.pixelWidth, attachedCam.pixelHeight, 32, RenderTextureFormat.Depth, "Transparent Depth");
        normalTarget = SqUtility.CreateRT(attachedCam.pixelWidth, attachedCam.pixelHeight, 0, RenderTextureFormat.DefaultHDR, "Normal Target");
        result = SqUtility.CreateRT(attachedCam.pixelWidth, attachedCam.pixelHeight, 0, RenderTextureFormat.DefaultHDR, "Result");
    }

    void CreateCameraData()
    {
        // create camera data
        camData.instanceID = attachedCam.GetInstanceID();

        // always use black clear
        camData.clearColor = new float[4];
        camData.clearColor[0] = 0;
        camData.clearColor[1] = 0;
        camData.clearColor[2] = 0;
        camData.clearColor[3] = 0;

        camData.camOrder = attachedCam.depth;
        camData.renderingPath = (int)attachedCam.renderingPath;
        camData.allowMSAA = (int)msaaSample;

        camData.renderTarget = new IntPtr[8];
        camData.renderTarget[0] = renderTarget.GetNativeTexturePtr();
        camData.renderTarget[1] = transparentDepth.GetNativeDepthBufferPtr();
        camData.renderTarget[2] = normalTarget.GetNativeTexturePtr();
        camData.renderTarget[3] = result.GetNativeTexturePtr();
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

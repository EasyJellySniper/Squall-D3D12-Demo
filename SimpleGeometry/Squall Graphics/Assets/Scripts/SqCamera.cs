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
        // check aa change
        if (lastMsaaSample != msaaSample)
        {
            RemoveCamera(attachedCam.GetInstanceID());
            OnDestroy();
            Start();
        }

        // update VP matrix
        if (transform.hasChanged)
        {
            Matrix4x4 viewProj = attachedCam.worldToCameraMatrix * GL.GetGPUProjectionMatrix(attachedCam.projectionMatrix, true);
            SetViewProjMatrix(attachedCam.GetInstanceID(), viewProj);
            transform.hasChanged = false;
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

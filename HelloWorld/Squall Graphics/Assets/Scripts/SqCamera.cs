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
        public float clearColorR;
        public float clearColorG;
        public float clearColorB;
        public float clearColorA;

        /// <summary>
        /// camera order
        /// </summary>
        public float camOrder;

        /// <summary>
        /// rendering path
        /// </summary>
        public int renderingPath;

        /// <summary>
        /// allow hdr
        /// </summary>
        public int allowHDR;

        /// <summary>
        /// allow msaa
        /// </summary>
        public int allowMSAA;

        /// <summary>
        /// max to 8 render targets
        /// </summary>
        public IntPtr renderTarget0;
        public IntPtr renderTarget1;
        public IntPtr renderTarget2;
        public IntPtr renderTarget3;
        public IntPtr renderTarget4;
        public IntPtr renderTarget5;
        public IntPtr renderTarget6;
        public IntPtr renderTarget7;

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

    void Start ()
    {
        attachedCam = GetComponent<Camera>();
        attachedCam.renderingPath = RenderingPath.Forward;  // currently only fw is implement
        attachedCam.cullingMask = 0;    // draw nothing

        CreateRenderTarget();
        CreateCameraData();
    }

    void OnDestroy()
    {
        if (renderTarget)
        {
            renderTarget.Release();
            DestroyImmediate(renderTarget);
        }
    }

    void OnRenderImage(RenderTexture source, RenderTexture destination)
    {
        Graphics.Blit(renderTarget, destination);
    }

    void CreateRenderTarget()
    {
        renderTarget = new RenderTexture(Screen.width, Screen.height, 24, (attachedCam.allowHDR) ? RenderTextureFormat.DefaultHDR : RenderTextureFormat.Default);
        renderTarget.name = name + " Target";
        renderTarget.antiAliasing = (int)msaaSample;
        renderTarget.bindTextureMS = (msaaSample != MsaaFactor.None);

        // actually create so that we have native resources
        renderTarget.Create();
    }

    void CreateCameraData()
    {
        // create camera data
        camData.clearColorR = attachedCam.backgroundColor.r;
        camData.clearColorG = attachedCam.backgroundColor.g;
        camData.clearColorB = attachedCam.backgroundColor.b;
        camData.clearColorA = attachedCam.backgroundColor.a;

        camData.camOrder = attachedCam.depth;
        camData.renderingPath = (int)attachedCam.renderingPath;
        camData.allowHDR = (attachedCam.allowHDR) ? 1 : 0;
        camData.allowMSAA = (int)msaaSample;

        camData.renderTarget0 = renderTarget.GetNativeTexturePtr();
        camData.depthTarget = renderTarget.GetNativeDepthBufferPtr();

        // add camera to native plugin
        if (!AddCamera(camData))
        {
            Debug.LogError("[Error] SqCamera: AddCamera() Failed.");
        }
    }
}

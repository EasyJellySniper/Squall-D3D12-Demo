using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// for init sq graphic after collecting
/// </summary>
public class SqPostGraphicManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitRayTracingInstance();

    [DllImport("SquallGraphics")]
    static extern void InitInstanceRendering();

    void Start()
    {
        // unload unused assets
        Resources.UnloadUnusedAssets();

        // init instance rendering
        InitInstanceRendering();

        // make sure the execution order of this script is after SqMeshFilter & SqMeshRenderer
        InitRayTracingInstance();
    }
}

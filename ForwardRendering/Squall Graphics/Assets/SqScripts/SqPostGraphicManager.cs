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

    [DllImport("SquallGraphics")]
    static extern void UpdateRayTracingRange(float _range);

    /// <summary>
    /// ray tracing range
    /// </summary>
    public float rayTracingRange = 100;

    void Start()
    {
        // unload unused assets
        Resources.UnloadUnusedAssets();

        // init instance rendering
        InitInstanceRendering();

        // make sure the execution order of this script is after SqMeshFilter & SqMeshRenderer
        InitRayTracingInstance();
    }

    void Update()
    {
        UpdateRayTracingRange(rayTracingRange);
    }
}

using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// for init native ray tracing
/// </summary>
public class SqRayTracingManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitRayTracingInstance();

    void Start()
    {
        // unload unused assets
        Resources.UnloadUnusedAssets();

        // make sure the execution order of this script is after SqMeshFilter & SqMeshRenderer
        InitRayTracingInstance();
    }
}

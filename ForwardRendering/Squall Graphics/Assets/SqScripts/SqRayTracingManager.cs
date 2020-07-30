using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// for init native ray tracing
/// </summary>
public class SqRayTracingManager : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern void InitRayTracingInterface();

    [DllImport("SquallGraphics")]
    static extern void InitRayTracingInstance();

    void Start()
    {
        // make sure the execution order of this script is after SqMeshFilter & SqMeshRenderer
        InitRayTracingInterface();
        InitRayTracingInstance();
    }
}

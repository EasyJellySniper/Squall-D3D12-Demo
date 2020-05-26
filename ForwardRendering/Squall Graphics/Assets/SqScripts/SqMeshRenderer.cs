using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// sq mesh renderer
/// </summary>
[RequireComponent(typeof(SqMeshFilter))]
[RequireComponent(typeof(MeshRenderer))]
public class SqMeshRenderer : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern int AddRenderer(int _instanceID, int _meshInstanceID, int _numOfMaterial, int[] _renderQueue);


    [DllImport("SquallGraphics")]
    static extern bool UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);

    [DllImport("SquallGraphics")]
    static extern void SetWorldMatrix(int _instanceID, Matrix4x4 _world);

    MeshRenderer rendererCache;
    int rendererNativeID = -1;

    void Start ()
    {
        rendererCache = GetComponent<MeshRenderer>();
        int[] queue = new int[rendererCache.sharedMaterials.Length];
        for (int i = 0; i < queue.Length; i++)
        {
            queue[i] = rendererCache.sharedMaterials[i].renderQueue;
        }

        rendererNativeID = AddRenderer(GetInstanceID(), GetComponent<SqMeshFilter>().MainMesh.GetInstanceID(), queue.Length, queue);

        Bounds b = rendererCache.bounds;
        UpdateRendererBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);
    }

    void Update()
    {
        if (transform.hasChanged)
        {
            Bounds b = rendererCache.bounds;
            UpdateRendererBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);
            SetWorldMatrix(rendererNativeID, rendererCache.localToWorldMatrix);
            transform.hasChanged = false;
        }
    }
}

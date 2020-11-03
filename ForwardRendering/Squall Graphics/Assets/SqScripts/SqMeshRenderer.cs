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
    static extern int AddNativeRenderer(int _instanceID, int _meshInstanceID, bool _isDynamic);

    [DllImport("SquallGraphics")]
    static extern bool UpdateLocalBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);

    [DllImport("SquallGraphics")]
    static extern void SetWorldMatrix(int _instanceID, Matrix4x4 _world);

    [DllImport("SquallGraphics")]
    static extern void UpdateNativeMaterialProp(int _instanceID, uint _byteSize, MaterialConstant _mc);

    [DllImport("SquallGraphics")]
    static extern void SetNativeRendererActive(int _id, bool _active);

    MeshRenderer rendererCache;
    int rendererNativeID = -1;

    void Start ()
    {
        // return if sqgraphic not init
        if (SqGraphicManager.Instance == null)
        {
            enabled = false;
            return;
        }

        if (!GetComponent<MeshRenderer>().enabled)
        {
            enabled = false;
            return;
        }

        InitRenderer();
        ExtractMaterialData();
        UpdateNativeTransform();
    }

    void OnEnable()
    {
        SetNativeRendererActive(rendererNativeID, true);
    }

    void OnDisable()
    {
        SetNativeRendererActive(rendererNativeID, false);
    }

    void Update()
    {
        if (gameObject.isStatic)
        {
            return;
        }

        if (transform.hasChanged)
        {
            UpdateNativeTransform();
        }
    }

    void InitRenderer()
    {
        rendererCache = GetComponent<MeshRenderer>();
        rendererNativeID = AddNativeRenderer(GetInstanceID(), GetComponent<SqMeshFilter>().MainMesh.GetInstanceID(), !gameObject.isStatic);

        Bounds b = GetComponent<MeshFilter>().sharedMesh.bounds;
        UpdateLocalBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);

        // destroy unity mesh after use it (I create a copy on native side for ray tracing accleration structures)
        Destroy(GetComponent<MeshFilter>());
    }

    void UpdateNativeTransform()
    {
        SetWorldMatrix(rendererNativeID, rendererCache.localToWorldMatrix);
        transform.hasChanged = false;
    }

    void ExtractMaterialData()
    {
        Material[] mats = rendererCache.sharedMaterials;
        for (int i = 0; i < mats.Length; i++)
        {
            SqMaterial.Instance.ExtractMaterialData(mats[i], rendererNativeID);
        }
    }
}

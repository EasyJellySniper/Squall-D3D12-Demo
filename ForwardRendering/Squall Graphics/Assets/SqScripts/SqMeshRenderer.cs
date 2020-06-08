using System;
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
    static extern int AddNativeRenderer(int _instanceID, int _meshInstanceID);

    [DllImport("SquallGraphics")]
    static extern int AddNativeMaterial(int _nRendererId, int _matInstanceId, int _queue, int _cullMode);

    [DllImport("SquallGraphics")]
    static extern bool UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);

    [DllImport("SquallGraphics")]
    static extern void SetWorldMatrix(int _instanceID, Matrix4x4 _world);

    [DllImport("SquallGraphics")]
    static extern void AddNativeMaterialProp(int _instanceID, int _matIndex, uint _byteSize, MaterialConstant _mc);

    MeshRenderer rendererCache;
    int rendererNativeID = -1;

    void Start ()
    {
        InitRenderer();
        ExtractMaterialData();
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

    void InitRenderer()
    {
        rendererCache = GetComponent<MeshRenderer>();
        rendererNativeID = AddNativeRenderer(GetInstanceID(), GetComponent<SqMeshFilter>().MainMesh.GetInstanceID());

        Bounds b = rendererCache.bounds;
        UpdateRendererBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);
    }

    void ExtractMaterialData()
    {
        Material[] mats = rendererCache.sharedMaterials;
        for (int i = 0; i < mats.Length; i++)
        {
            int cullMode = (mats[i].HasProperty("_CullMode")) ? (int)mats[i].GetFloat("_CullMode") : 0;
            AddNativeMaterial(rendererNativeID, mats[i].GetInstanceID(), mats[i].renderQueue, cullMode);
        }

        for (int i = 0; i < mats.Length; i++)
        {
            MaterialConstant mc = SqMaterial.Instance.GetMaterialConstant(mats[i]);
            AddNativeMaterialProp(rendererNativeID, i, (uint)SqMaterial.Instance.matConstantSize, mc);
        }
    }
}

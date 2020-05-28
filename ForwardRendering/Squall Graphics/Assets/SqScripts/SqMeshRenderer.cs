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
    /// <summary>
    /// material constant
    /// </summary>
    struct MaterialConstant
    {
        public Vector4 _MainTex_ST;
        public float _CutOff;
        public uint _TexIndex;
        public uint _SamplerIndex;
        public float _Padding;
    };

    [DllImport("SquallGraphics")]
    static extern int AddNativeRenderer(int _instanceID, int _meshInstanceID);

    [DllImport("SquallGraphics")]
    static extern int AddNativeMaterial(int _instanceID, int _queue);

    [DllImport("SquallGraphics")]
    static extern bool UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);

    [DllImport("SquallGraphics")]
    static extern void SetWorldMatrix(int _instanceID, Matrix4x4 _world);

    //[DllImport("SquallGraphics")]
    //static extern int AddNativeTexture(int _texID, IntPtr _texture);

    //[DllImport("SquallGraphics")]
    //static extern int AddNativeSampler(int _texID, FilterMode _filterMode, TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel);

    //[DllImport("SquallGraphics")]
    //static extern void SetMaterialConstant(int _instanceID, int _matID, MaterialConstant _mc);

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
            AddNativeMaterial(rendererNativeID, mats[i].renderQueue);
        }

        //for (int i = 0; i < mats.Length; i++)
        //{
        //    MaterialConstant mc = new MaterialConstant();

        //    Texture2D albedo = mats[i].mainTexture as Texture2D;
        //    if (albedo)
        //    {
        //        mc._TexIndex = (uint)AddNativeTexture(albedo.GetInstanceID(), albedo.GetNativeTexturePtr());
        //        mc._SamplerIndex = (uint)AddNativeSampler(albedo.GetInstanceID(), albedo.filterMode, albedo.wrapModeU, albedo.wrapModeV, albedo.wrapModeW, albedo.anisoLevel);
        //    }

        //    mc._MainTex_ST = new Vector4(mats[i].mainTextureScale.x, mats[i].mainTextureScale.y, mats[i].mainTextureOffset.x, mats[i].mainTextureOffset.y);
        //    mc._CutOff = mats[i].GetFloat("_Cutoff");

        //    SetMaterialConstant(rendererNativeID, i, mc);
        //}
    }
}

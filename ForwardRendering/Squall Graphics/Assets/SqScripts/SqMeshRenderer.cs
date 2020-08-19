﻿using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

/// <summary>
/// sq mesh renderer
/// </summary>
[RequireComponent(typeof(SqMeshFilter))]
[RequireComponent(typeof(MeshRenderer))]
public class SqMeshRenderer : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern int AddNativeRenderer(int _instanceID, int _meshInstanceID);

    [DllImport("SquallGraphics", CharSet = CharSet.Ansi)]
    static extern int AddNativeMaterial(int _nRendererId, int _matInstanceId, int _queue, int _cullMode, int _srcBlend, int _dstBlend, string _nativeShader, int _numMacro, string []_macro);

    [DllImport("SquallGraphics")]
    static extern bool UpdateRendererBound(int _instanceID, float _x, float _y, float _z, float _ex, float _ey, float _ez);

    [DllImport("SquallGraphics")]
    static extern void SetWorldMatrix(int _instanceID, Matrix4x4 _world);

    [DllImport("SquallGraphics")]
    static extern void AddNativeMaterialProp(int _instanceID, uint _byteSize, MaterialConstant _mc);

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
        if (transform.hasChanged)
        {
            UpdateNativeTransform();
        }
    }

    void InitRenderer()
    {
        rendererCache = GetComponent<MeshRenderer>();
        rendererNativeID = AddNativeRenderer(GetInstanceID(), GetComponent<SqMeshFilter>().MainMesh.GetInstanceID());

        Bounds b = rendererCache.bounds;
        UpdateRendererBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);
    }

    void UpdateNativeTransform()
    {
        Bounds b = rendererCache.bounds;
        UpdateRendererBound(rendererNativeID, b.center.x, b.center.y, b.center.z, b.extents.x, b.extents.y, b.extents.z);
        SetWorldMatrix(rendererNativeID, rendererCache.localToWorldMatrix);
        transform.hasChanged = false;
    }

    void ExtractMaterialData()
    {
        Material[] mats = rendererCache.sharedMaterials;
        for (int i = 0; i < mats.Length; i++)
        {
            int cullMode = (mats[i].HasProperty("_CullMode")) ? (int)mats[i].GetFloat("_CullMode") : 2;
            int srcBlend = (mats[i].HasProperty("_SrcBlend")) ? (int)mats[i].GetFloat("_SrcBlend") : 1;
            int dstBlend = (mats[i].HasProperty("_DstBlend")) ? (int)mats[i].GetFloat("_DstBlend") : 0;
            bool isCutOff = (mats[i].renderQueue <= (int)RenderQueue.GeometryLast) && (mats[i].renderQueue >= 2226);
            bool isShadowCutoff = (mats[i].renderQueue >= 2226);
            bool isTransparent = (mats[i].renderQueue > (int)RenderQueue.GeometryLast);

            List<string> macro = new List<string>();

            if (isCutOff)
            {
                macro.Add("_CUTOFF_ON");
            }

            if (isShadowCutoff)
            {
                macro.Add("_SHADOW_CUTOFF_ON");
            }

            if(isTransparent)
            {
                macro.Add("_TRANSPARENT_ON");
            }

            SqMaterial.Instance.AddTexKeyword(mats[i], ref macro, "_SpecGlossMap", "_SPEC_GLOSS_MAP");
            SqMaterial.Instance.AddTexKeyword(mats[i], ref macro, "_BumpMap", "_NORMAL_MAP");

            bool shouldEmissionBeEnabled = (mats[i].globalIlluminationFlags & MaterialGlobalIlluminationFlags.EmissiveIsBlack) == 0;
            if (shouldEmissionBeEnabled)
            {
                macro.Add("_EMISSION");
            }

            if (mats[i].GetTexture("_DetailAlbedoMap"))
            {
                macro.Add("_DETAIL_MAP");
            }

            if (mats[i].GetTexture("_DetailNormalMap"))
            {
                macro.Add("_DETAIL_NORMAL_MAP");
            }

            AddNativeMaterial(rendererNativeID, mats[i].GetInstanceID(), mats[i].renderQueue, cullMode, srcBlend, dstBlend, "ForwardPass.hlsl", macro.Count, macro.ToArray());
            macro.Clear();
        }

        for (int i = 0; i < mats.Length; i++)
        {
            MaterialConstant mc = SqMaterial.Instance.GetMaterialConstant(mats[i]);
            AddNativeMaterialProp(mats[i].GetInstanceID(), (uint)SqMaterial.Instance.matConstantSize, mc);
        }
    }
}
